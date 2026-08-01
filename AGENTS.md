# Notes for agents working in this repository

Read this before changing anything. `CLAUDE.md` is a symlink to this file.

## What this is

An exhaustive study of the kids' game Genius Square: a 6x6 grid, 7 pegs, and 9
polyomino pieces whose areas sum to exactly `36 - 7 = 29`. The dice that ship
with the game are deliberately **out of scope** -- they exist to guarantee
solvable boards, and we study all `C(36,7) = 8,347,680` peg placements instead.

Everything is C11. No Python, no external dependencies beyond `zlib` and
`pthreads`. Speed is a primary requirement, not an afterthought.

## Layout

```
src/gs_core.[ch]      board, pieces, 625 placements, D4 group, lex ranking
src/gs_search.[ch]    single-board bitmask DFS: count, enumerate, freedom, geometry
src/gs_dlx.[ch]       Knuth Algorithm X / dancing links (cross-check engine)
src/gs_io.[ch]        canonical orbit index, .gsc and .u32 file formats
src/gs_countall.c     the global enumeration -- counts for every board
src/gs_solve.c        tool: everything about one board
src/gs_stats.c        tool: histogram, hardest boards, trap classification
src/gs_analyze.c      tool: heat maps, spectra, blocking patterns
src/gs_selftest.c     structural checks + throughput probe
docs/                 PLAN, ALGORITHMS, FORMATS, HARDNESS + generated results
docs/tutorial/        pedagogical deep dive (read this to *learn* the design;
                      keep it consistent if you change what it describes)
data/counts.gsc       the computed count table (committed, ~2 MB)
web/                  browser solver published to GitHub Pages (see below)
```

`web/` is the only JavaScript in the project. `web/gs.js` is a hand port of
`gs_core.c` + `gs_search.c` -- same cell numbering, same piece list, same
placement order, same DFS -- so it enumerates solutions in the same order as
`bin/gs_solve --all`. `web/app.js` is DOM only; `web/verify.mjs` is the
cross-check. **If you change the pieces, the placement table or the search,
re-run `make web-verify`**; nothing else will catch the port drifting.

## Build and run

```sh
make            # everything into bin/
make test       # gs_selftest -- run this after ANY change to core or search
make counts     # the long enumeration (~12 min on 4 cores); writes data/counts.gsc
make report     # regenerates docs/RESULTS.md and docs/ANALYSIS_*.md
make web-verify # checks web/gs.js against the C engine (the only target
                # that needs node; ~25 s)
```

The page in `web/` is static: no build step, no dependencies. To look at it
locally, serve the directory over HTTP (ES modules will not load over
`file://`) and open `web/index.html`.

## Rules that are easy to break by accident

**Do not change the cell numbering, the piece list, or the ranking functions
without regenerating `data/counts.gsc`.** Every stored count is indexed by
`gs_rank`. A change to indexing silently invalidates the data file; nothing
will crash, the numbers will just be wrong.

**`gs_rank` is lexicographic, not colexicographic.** This is load-bearing for
performance, not a stylistic choice -- see ALGORITHMS.md. Colex would turn the
inner loop's counter increments into cache misses.

**Canonical form is lexicographically smallest, so `gs_rank(gs_canon(b)) <=
gs_rank(b)` always.** `gs_canon` compares masks with `lex_less`, not with `<`
on the integer. The two orderings are different and the search-time mirror cut
depends on this one.

**The mirror-symmetry cut in `gs_countall` is subtle.** It prunes branches
whose hole set is lexicographically greater than its left-right mirror image,
decided one row at a time. If you touch it, `make test` checks the row-wise
verdict against a direct whole-board comparison on 300,000 random boards, and
`gs_countall --nosym` disables it. Both matter.

**The browser port is a second copy of the search.** `web/gs.js` duplicates the
piece list, the placement order and the DFS. Changing either side without the
other makes the published page silently disagree with the tools; `make
web-verify` is the check.

**Solution counts must be D4-invariant.** This is the single most useful
invariant in the codebase. Any bug in placement generation, ranking, or the
symmetry cut breaks it. `gs_selftest` checks it directly; `gs_countall` checks
the finished table.

## Verifying a change

`gs_countall` self-verifies at the end of a run: the sum of counts must equal
the leaf count, the mirror fold must reproduce the total, thousands of random
boards are re-solved from scratch with the single-board engine, and the table
must be constant on D4 orbits. If you have changed the search, run the full
thing rather than trusting a benchmark.

For a quick check on one board, the two independent engines should always
agree:

```sh
bin/gs_solve --both A1 C1 E1 B4 D4 F4 C6
```

## Performance notes

* The hot loop is `rec()` in `gs_countall.c`. It reads `T.pmask[]` (masks only,
  8 bytes per placement) rather than the 16-byte `gs_placement` records.
* The connectivity prune is guarded by `gs_dense_h[rem][holes]`, which skips
  the flood fill whenever it provably cannot fire. Removing that guard roughly
  halves throughput.
* Worker threads each own a private 33 MB counter array; there are no atomics
  in the search. Memory is `33 MB x threads`.
* Current numbers on 4 cores: ~11-19 Mleaf/s, ~1.2e10 leaves, ~12 minutes.

## Style

Plain C11, 4-space indent, `gs_` prefix on everything public. Comments explain
*why* (especially the non-obvious index and symmetry choices); the code is
expected to be readable without them for the *what*.

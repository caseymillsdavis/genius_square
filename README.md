# genius_square

An exhaustive study of the kids' game **Genius Square**: solution counts for
every possible board, tools to solve and analyse individual boards, and a
structural study of what makes a board hard or impossible.

The game: roll 7 dice, place 7 pegs on a 6x6 grid, then fill the remaining 29
cells with 9 pieces (1x1, 2x1, 3x1, L-tromino, L/S/I/T-tetromino, 2x2). The
piece areas sum to exactly 29, so the pieces tile whatever the pegs leave. The
dice exist only to guarantee solvable boards, so this project ignores them and
studies **all** `C(36,7) = 8,347,680` peg placements.

Everything is C11 with no dependencies beyond zlib and pthreads. See
[AGENTS.md](AGENTS.md) before making changes.

**Solve a board in the browser:
[caseymillsdavis.github.io/genius_square](https://caseymillsdavis.github.io/genius_square/)** --
place the seven pegs, get the exact solution count, browse the solutions, and
see where the board falls in the distribution of all 8.3M boards. It runs
entirely client-side; nothing is uploaded.

## Headline numbers

| | |
|---|---|
| boards | 8,347,680 (1,044,690 up to symmetry) |
| total packings of the 9 pieces into the 6x6 grid | **11,387,941,312** |
| solvable boards | 8,175,240 (97.93%) |
| unsolvable boards | 172,440 (2.07%) |
| median / mean solutions per board | 719 / 1364 |
| most solutions | 100,593 (pegs `A1..F1 A2`) |
| boards with a unique solution | 800 (100 orbits) -- all of them pin every piece |
| fewest solutions for a *trap-free* board | 2 (e.g. `C1 B2 E2 C4 F4 B5 E5`) |
| smallest fatal peg pattern | 6 cells; 42 such patterns up to symmetry |

Full tables: [docs/RESULTS.md](docs/RESULTS.md). Hardness study:
[docs/HARDNESS.md](docs/HARDNESS.md) (design) and
[docs/ANALYSIS_UNSOLVABLE.md](docs/ANALYSIS_UNSOLVABLE.md) /
[docs/ANALYSIS_HARD.md](docs/ANALYSIS_HARD.md) (numbers).

## Quick start

```sh
make            # builds bin/
make test       # self-checks + throughput probe

# everything about one board (pegs as column-letter row-digit):
bin/gs_solve A1 C1 E1 B4 D4 F4 C6          # count + one solution
bin/gs_solve --all A1 C1 E1 B4 D4 F4 C6    # every solution
bin/gs_solve --freedom C1 B2 E2 C4 F4 B5 E5  # per-piece freedom / traps
bin/gs_solve --both A1 C1 E1 B4 D4 F4 C6   # bitmask DFS vs dancing links

# recompute the full count table (~10 min on 4 cores; self-verifying):
make counts

# regenerate the reports from data/counts.gsc:
make report
```

`data/counts.gsc` (committed, 1.5 MB) holds the solution count of every board:
counts for the 1,044,690 D4-canonical boards, byte-plane split and deflated;
the other boards follow by symmetry. Format and lookup recipe:
[docs/FORMATS.md](docs/FORMATS.md).

## How it works

Solving 8.3M boards one at a time would take hours. Instead `gs_countall`
enumerates every *packing* of the nine pieces into the empty grid once --
each packing leaves exactly 7 cells uncovered and is therefore a solution of
exactly one board -- and buckets the 11.4 billion packings by their hole set.
Shared prefixes, cache-friendly lexicographic board indices, a
connected-component feasibility prune, a mirror-symmetry cut, and a
lock-free thread pool bring the whole computation to ~10 minutes on 4 cores.
Results are verified five independent ways, including re-solving thousands of
random boards with a from-scratch dancing-links engine.

Details: [docs/ALGORITHMS.md](docs/ALGORITHMS.md).

## Learning the internals

[docs/tutorial/](docs/tutorial/README.md) is a four-part walkthrough that
*teaches* the machinery from first principles -- bit tricks and ranking,
the search engines and the one-pass global enumeration, the group theory,
and the mathematics of the hardness analysis -- with worked examples and
exercises. The other docs are terse references by comparison.

## Repository map

```
src/            the library and the five tools (see AGENTS.md)
web/            the browser solver published to GitHub Pages
docs/tutorial/  four-part deep dive: representations, search, symmetry, analysis
docs/PLAN.md    goals and status
docs/ALGORITHMS.md   the counting scheme, symmetry, pruning, verification
docs/FORMATS.md      cell numbering, board ranks, file formats
docs/HARDNESS.md     design notes for the unsolvability study
docs/RESULTS.md      generated: histogram + hardest boards
docs/ANALYSIS_*.md   generated: heat maps, spectra, blocking patterns
data/counts.gsc      solution count of every board (compressed)
```

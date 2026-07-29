# Plan

Status of the five goals, in the order they were asked for.

## 1. Solution counts for every board -- DONE

Compute `solutions(B)` for all `C(36,7) = 8,347,680` peg placements.

* **Approach.** One global enumeration of every packing of the nine pieces into
  the empty 6x6 grid, bucketed by the 7 cells it leaves empty. Because the
  piece areas sum to exactly 29, every packing is a solution of exactly one
  board, so a single pass produces the whole count vector and all prefix work is
  shared. Cost is proportional to the answer (~1.2e10 packings) rather than to
  8.3 million independent searches.
* **Parallel.** Root expanded breadth-first into ~200k subtree tasks, claimed
  dynamically by worker threads with private counter arrays and no atomics in
  the search.
* **Symmetry.** Counts are D4-equivariant. Used for storage (1,044,690 orbits
  instead of 8,347,680 boards), for validation, and as an in-search cut on the
  left-right mirror -- the only element of D4 decidable from a row-major prefix.
  Measured saving ~1.6x. See ALGORITHMS.md for why the other seven elements
  cannot be cut cheaply and what a full 8x would cost.
* **Storage.** `data/counts.gsc`: canonical orbits only, byte-plane split, zlib
  deflated. 33.4 MB flat -> 4.2 MB canonical -> ~2 MB on disk. See FORMATS.md.
* **Output.** `docs/RESULTS.md`, including the requested histogram of boards by
  number of solutions.

## 2. All solutions for a given board -- DONE

`bin/gs_solve` prints the board, the structure of the empty region, and any or
all solutions as ASCII grids. Two independent engines: the bitmask DFS and a
from-scratch Knuth DLX (Algorithm X with dancing links). `--both` runs them
against each other.

## 3. Hard boards, excluding trapped pieces -- DONE

The low end of the distribution is dominated by boards that **pin** a piece: the
pegs force some piece into one and only one position (classically an isolated
empty cell, which nails the 1x1). Those are not interesting as puzzles.

A board is classified as *trapped* when some piece occupies the same cells in
every solution -- computed exactly by enumerating the full solution set, which
is cheap for the boards that matter. `docs/RESULTS.md` reports the hardest
boards three ways: overall, trap-free, and "1x1 not pinned", plus how much of
the low end traps account for at each threshold.

Note that a uniquely-solvable board is trapped by definition, so trap-free
boards necessarily have several solutions -- the smallest achievable count for a
trap-free board is itself a reported result.

## 4. Study of the unsolvable boards -- DONE

See `docs/HARDNESS.md` for the design discussion and `docs/ANALYSIS_UNSOLVABLE.md`
for the numbers. Briefly:

* The proposed heat map is the **degree-1** projection of the unsolvability
  indicator; the proposed spectral study is a change of basis on the
  **degree-2** information. Both are computed, and both are given error bars by
  fitting the indicator exactly in the Johnson scheme `J(36,7)` and reporting
  the explained variance at each degree.
* The spectral part is done in the basis that diagonalises the excess
  co-occurrence matrix (the principled version of the idea) as well as in a DCT
  basis (the familiar presentation).
* Beyond statistics: **minimal blocking patterns** -- sets of cells such that
  *every* board containing them is unsolvable -- computed exactly by a top-down
  sweep of the subset lattice. These produce checkable statements rather than
  correlations. Coverage tells us which unsolvable boards fail for a local,
  nameable reason and which fail only globally.

`gs_analyze --hard T` runs the same machinery on near-miss boards.

## 5. Documentation -- DONE

`README.md` (orientation), `AGENTS.md` = `CLAUDE.md` (agent-facing rules and
traps), `docs/ALGORITHMS.md` (how and why), `docs/FORMATS.md` (indexing and
file layout), `docs/HARDNESS.md` (the analysis design), plus the generated
`docs/RESULTS.md` and `docs/ANALYSIS_*.md`.

## Not done / possible next steps

* A symmetry-adapted cell ordering that makes all eight D4 elements cuttable.
  Worth up to 8x on paper; costs the row-major frontier that the connectivity
  prune depends on. Would need measuring, not guessing.
* Solution counts conditioned on the actual dice, for comparison with the
  as-shipped game -- explicitly out of scope here, but the data supports it.
* An `mmap`-backed reader so tools can query counts without the 33 MB expansion.

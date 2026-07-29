# The Genius Square project, explained from first principles

This tutorial teaches the machinery behind this repository: the data
structures, the algorithms, and the mathematics of the analysis. It assumes
you are comfortable in C and with big-O reasoning, and nothing else -- the
group theory and the algebraic combinatorics are built up on the spot.

The reference docs ([ALGORITHMS.md](../ALGORITHMS.md),
[FORMATS.md](../FORMATS.md), [HARDNESS.md](../HARDNESS.md)) state *what* the
code does, tersely, for someone maintaining it. These four parts instead
derive everything, with worked examples small enough to check by hand:

| part | contents |
|---|---|
| [1. Boards, bits, and ranks](01-boards-bits-and-ranks.md) | the 36-bit board; neighborhood masks; generating the 625 placements; the anchor index; the combinatorial number system; why *lexicographic* ranks are a performance decision |
| [2. Search](02-search.md) | the single-board solver and its correctness argument; connectivity pruning with a bitmask subset-sum DP; dancing links from scratch; the one-pass global enumeration of all 8.3M boards; parallel decomposition |
| [3. Symmetry](03-symmetry.md) | group actions; Burnside's lemma worked out to 1,044,690 orbits; the orbit-size distribution derived by hand; canonical forms and their ordering subtleties; the in-search mirror cut; why the count table compresses 22x |
| [4. The analysis](04-analysis.md) | unsolvability as a boolean function on 7-subsets; why the hypercube Fourier transform is the wrong tool and the Johnson scheme is the right one; exact least squares via closed-form Gram matrices; R-squared ladders; eigenmodes and the DCT spectrum; minimal blocking patterns |

Suggested reading order is 1 → 2 → 3 → 4; part 4 only depends on part 3
through the notion of a D4 orbit.

## The one-paragraph summary of the whole project

A Genius Square board is a choice of 7 blocker cells out of 36; the nine
pieces have total area 29 = 36 − 7, so a solution is an exact tiling of
everything the pegs leave behind. We computed the number of solutions of
every one of the C(36,7) = 8,347,680 boards -- not by solving 8.3 million
puzzles, but by enumerating all 11,387,941,312 *packings* of the nine pieces
into the empty grid exactly once, and crediting each packing to the single
board whose pegs sit on the 7 cells it leaves uncovered. Symmetry cuts the
work roughly in half during the search, cuts storage eightfold afterward, and
supplies free correctness checks throughout. With the full count table in
hand, we studied *why* boards are hard: hardest-board tables that filter out
boards which merely trap a piece, an exact variance decomposition of the
unsolvability indicator by interaction order (the Johnson scheme), spectral
views of the second-order structure, and an exact computation of the minimal
peg patterns that are fatal no matter where the remaining pegs go.

## Where the code lives

| file | role |
|---|---|
| `src/gs_core.[ch]` | board representation, pieces, placements, ranking, D4 |
| `src/gs_search.[ch]` | single-board DFS: count / enumerate / per-piece freedom |
| `src/gs_dlx.[ch]` | dancing links (the independent cross-check engine) |
| `src/gs_countall.c` | the global enumeration over all boards |
| `src/gs_io.[ch]` | canonical orbit index, compressed file formats |
| `src/gs_stats.c` | histogram, hardest boards, trap classification |
| `src/gs_analyze.c` | heat maps, Johnson-scheme fits, spectra, blocking patterns |
| `src/gs_selftest.c` | the invariants that keep all of the above honest |

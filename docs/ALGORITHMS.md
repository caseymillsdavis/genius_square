# Algorithms

## The problem

The board is a 6x6 grid. Seven cells are blocked by pegs. The remaining 29
must be tiled exactly by nine pieces, each used once:

| id | name | area | shape |
|---|---|---:|---|
| 0 | `mono1`   | 1 | `1x1` |
| 1 | `domino2` | 2 | `2x1` |
| 2 | `line3`   | 3 | `3x1` |
| 3 | `ell3`    | 3 | L tromino |
| 4 | `ell4`    | 4 | L tetromino |
| 5 | `ess4`    | 4 | S/Z tetromino |
| 6 | `line4`   | 4 | `4x1` |
| 7 | `square4` | 4 | `2x2` |
| 8 | `tee4`    | 4 | T tetromino |

The areas sum to `1+2+3+3+4+4+4+4+4 = 29 = 36 - 7`, which is what makes the
game work: **the pieces exactly fill whatever the pegs leave behind.**

Pieces are physical tiles and may be flipped as well as rotated, so each is a
*free* polyomino and the set of nine is closed under the eight symmetries of
the square. Enumerating every rotation/reflection of every piece at every
translation gives **625 placements**:

```
mono1 36, domino2 60, line3 48, ell3 100, ell4 160,
ess4 80, line4 36, square4 25, tee4 80        (total 625)
```

We ignore the dice entirely. The object of study is all
`C(36,7) = 8,347,680` ways to place the pegs.

## Counting every board at once

The naive plan -- solve 8.3 million puzzles independently -- throws away the
fact that the puzzles overlap enormously. The better formulation:

> Every packing of the nine pieces into the empty 6x6 grid covers exactly 29
> cells and therefore leaves exactly 7 empty. So every packing *is* a solution
> of exactly one board: the board whose pegs sit on the 7 holes.

Hence

```
    sum over all boards B of  solutions(B)  =  total number of packings
```

and one enumeration of all packings, bucketed by hole set, produces the entire
count vector. The work is proportional to the *answer* rather than to the
number of puzzles, and all prefix work is shared between boards.

Measured on this repo: about 1.2e10 packings exist, so the single global
enumeration replaces 8.3 million searches with one.

### The search

Cells are decided in increasing index order. At the lowest still-undecided
cell the search either

* declares that cell a **hole** (if fewer than 7 holes have been committed), or
* covers it with some **unused piece**.

Because a placement's *anchor* is defined as its lowest-numbered cell, only
placements anchored at the current cell can cover it. That makes the branching
exhaustive and non-redundant simultaneously: every packing is generated exactly
once, and no canonical-form test is needed at the leaves.

State is four machine words: the decided-cell mask, the hole mask, the 9-bit
set of used pieces, and a partial board index.

### Pruning

After every decision the still-empty region is split into connected components
(bit-parallel flood fill on the 36-bit mask). Each component must be tiled
exactly by some sub-multiset of the remaining pieces plus at most the remaining
hole budget, so its size must lie in

```
    gs_sumset_h[remaining pieces][holes left]
```

a precomputed 512 x 8 table of achievable-sum bitmasks. This is what kills
isolated pockets: a lone cell once the monomino is gone, a 2-cell pocket once
the monomino and domino are gone, and so on.

The flood fill is not cheap, so it is guarded by `gs_dense_h[rem][hl]`, which
records whether *every* size up to the total remaining is achievable. Early in
the search all nine pieces are available and nothing can fail, so the check is
skipped outright; it only starts running once the piece supply is thin enough
to matter. This is worth roughly a factor of two on its own.

### Board indices are lexicographic on purpose

The counter array has 8,347,680 entries and receives ~1.2e10 increments. If
board indices were *colex* ranks (the usual choice, and the easier code) the
index would be dominated by the *largest* peg, which is exactly the part that
varies fastest deep in the search -- every increment would be a cache miss in a
33 MB array.

With *lexicographic* ranks, fixing the first `j` holes confines the index to a
contiguous run of `C(35-h_j, 7-j)` entries. Since the search commits holes in
increasing cell order, a DFS subtree writes into one shrinking window. The rank
is maintained incrementally on the way down:

```
    rank += C(35 - prev_hole, 8-i) - C(36 - this_hole, 8-i)
```

for the `i`-th hole. At a leaf the remaining cells are appended in one short
loop.

## Symmetry

The eight symmetries of the square act on boards, and since the piece set is
closed under those symmetries, `solutions(g.B) = solutions(B)` for every
`g` in D4. Burnside's lemma gives the number of orbits:

```
    (1/8) * sum over g of #{7-subsets fixed by g}  =  1,044,690
```

so only 1,044,690 of the 8,347,680 boards are genuinely distinct. This is used
three ways.

**Storage.** Only canonical representatives are stored (see FORMATS.md), an
eight-fold reduction before compression.

**Validation.** The finished table must be constant on orbits; `gs_countall`
spot-checks this, and `gs_selftest` independently verifies that solution counts
are D4-invariant on random boards.

**Search.** Turning symmetry into a *search* cut is harder than it looks,
because the cut has to be decidable from a prefix of the decisions and the
search scans cells in row-major order. Take the canonical member of an orbit to
be the lexicographically smallest hole set (holes as early as possible). Then a
symmetry `g` can only be tested once every cell in `g^-1({0..c})` has been
decided:

| symmetry | first cell it needs | usable early? |
|---|---|---|
| left-right mirror | 5 | **yes** -- maps each row onto itself |
| rot270 | 5, then 11, 17 | partly |
| rot90, up-down mirror, rot180, both diagonals | 30 or 35 | no |

Only the left-right mirror maps every row onto itself, so as soon as a row is
fully decided its 6-bit hole pattern can be compared with its own bit reversal;
the first asymmetric row decides. Branches whose hole set is lexicographically
*greater* than its mirror image are pruned. Every D4-canonical board survives
(its mirror lies in its own orbit), so the canonical counts come out intact.

The measured saving is about **1.6x**, not 2x, because the subtree in which cell
A1 is itself a hole is always mirror-canonical and is never cut -- and that is a
large subtree (a fraction 7/36 of all packings have a hole at A1).

Getting the full 8x would require ordering the cells by D4 orbit so that all
eight symmetries become testable at the same depth. That destroys the row-major
frontier that makes the rest of the search fast -- the empty region stops being
compact, and the connectivity prune, which depends on pockets forming behind a
tight frontier, weakens badly. The trade was judged not worth it; the
symmetry-reduced storage and validation keep the group-theoretic benefit where
it is cheap. `--nosym` disables the cut for cross-checking.

## Parallelism

The root is expanded breadth-first into ~200,000 independent subtree tasks
(cheap: a few milliseconds), which worker threads then claim with an atomic
counter. Dynamic scheduling matters because subtree sizes vary by orders of
magnitude.

Each worker owns a private 33 MB counter array and they are summed at the end;
this avoids atomics in the innermost loop entirely. Memory is `33 MB x threads`,
which is the only real scaling limit.

## Verification

Correctness of a number nobody has computed before rests on independent paths
agreeing:

1. **Two engines.** `gs_solve --both` runs the bitmask DFS and a from-scratch
   Knuth DLX (Algorithm X with dancing links, S-heuristic) on the same board
   and compares. The DLX matrix has 45 columns (36 cells + 9 "use this piece
   once" constraints); the peg columns are covered before the search starts,
   which simultaneously satisfies them and deletes every overlapping placement.
2. **Internal identity.** The sum of the produced counts must equal the number
   of leaves the search visited.
3. **Mirror-fold identity.** Folding the mirror-canonical half back up must
   reproduce the total packing count obtained from the D4 expansion.
4. **Independent re-solve.** After the global run, thousands of random boards
   are re-solved from scratch with the single-board engine and compared against
   the table.
5. **Equivariance.** Counts must be constant on D4 orbits -- a strong check,
   since the enumeration has no notion of symmetry beyond the mirror cut.

`gs_selftest` additionally checks the placement table (625 placements, correct
areas and anchors), that D4 is closed and acts faithfully, that the piece set is
closed under D4, rank/unrank round trips over the whole index space, and that
the row-wise mirror test agrees with a direct comparison on 300,000 random
boards.

## Why not DLX for the bulk pass?

Dancing links is the natural choice for one hard exact-cover instance, and it is
implemented here and used as the cross-check. For this workload it is about 2x
slower than the bitmask DFS (measured: 3.1 ms vs 1.4 ms on a 426-solution
board), because the state is four registers rather than a pointer-chasing
linked structure, and because the "lowest undecided cell" branching rule is
already near-optimal on a grid this small -- the column-minimum heuristic that
justifies DLX's overhead buys little here. The bulk pass also needs to share
prefixes across boards, which the bitmask formulation expresses directly.

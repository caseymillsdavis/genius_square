# Part 2: Search

Two engines live in this repo. The bitmask depth-first search does all the
heavy lifting; Knuth's dancing links exists to check it. This part builds
both from scratch, proves the correctness of the branching rule, explains the
pruning mathematics, and then derives the project's central algorithmic move:
counting the solutions of all 8,347,680 boards in a single enumeration.

## 2.1 The single-board solver

Given a peg mask, count (or enumerate) the ways to tile the other 29 cells
with the nine pieces, each used exactly once. The recursion
(`rec_count` in `src/gs_search.c`) is four lines of idea:

```
state: filled (bitmask of decided cells), used (9-bit set of placed pieces)

if used == ALL:            one solution found
c = lowest empty cell
for each unused piece q:
    for each placement of q anchored at c that fits:
        recurse(filled | placement, used | q)
```

### Why "lowest empty cell + anchored placements" is exactly right

Two properties are needed: every tiling is generated (**completeness**), and
each is generated once (**uniqueness**).

*Completeness.* Consider any tiling of the empty region. Look at the lowest
empty cell `c`. Some piece covers `c`; consider that piece's placement mask
`m`. Every cell of `m` below `c` would be an empty cell lower than `c` --
contradiction -- so `m`'s lowest cell is exactly `c`; i.e. `m` is *anchored*
at `c` and therefore appears in the bucket `lo[c][q]..hi[c][q]` the loop
scans. Recursing preserves the invariant.

*Uniqueness.* At each node the search branches on which placement covers the
*specific* cell `c`. In a given tiling exactly one placement covers `c`, so a
tiling survives into exactly one branch. Distinct leaves therefore hold
distinct tilings, and no canonical-form deduplication is ever needed.

This is the standard polyomino-packer's rule, and it has a second virtue:
the frontier stays *compact*. Cells are decided roughly in reading order, so
the boundary between decided and undecided territory is a short ragged line,
which keeps the connectivity structure of the empty region simple -- and that
is what the pruner feeds on.

### Pruning: components x sub-multiset sums

After a placement, the still-empty region may have been pinched into pieces.
Each connected component must be tiled *exactly* by some sub-multiset of the
remaining pieces -- pieces cannot straddle a gap. A cheap necessary condition
is arithmetic: the component's *size* must be a sum of some sub-multiset of
the remaining piece areas.

Piece areas are `{1,2,3,3,4,4,4,4,4}`. Which sums are achievable by subsets
of the remaining pieces? That is a subset-sum problem over a 9-element
multiset, so all 512 piece-sets can be precomputed, each in one word, by the
classic bitmask DP (`gs_init_search`):

```c
uint32_t reach = 1;                     /* bit j set <=> sum j achievable */
for each piece p in set: reach |= reach << size[p];
```

`gs_sumset[rem]` then answers "is size s feasible" in one AND. Example: if
the 1x1 is already used, an isolated empty cell (component of size 1) kills
the branch instantly -- bit 1 is only reachable via the monomino. If the 1x1
and 2x1 are both gone, any component of size 1, 2 *or 5* is fatal (5 = 1+4 =
2+3 needs a small piece). This prune fires constantly in the endgame and is
the difference between milliseconds and seconds per board.

The component sizes come from a register-resident flood fill: take the lowest
bit of the empty region, `gs_grow` it (part 1) to a fixed point, popcount,
erase, repeat. Each component costs about its diameter in iterations, all on
one word -- no queue, no visited array.

### Two refinements

**Hole budgets** (needed by the global enumeration below): when the search is
allowed to leave up to `h` cells untiled, the feasible sizes are "achievable
sum + at most h". Precomputed as `gs_sumset_h[rem][h]`, an OR of shifted
copies of `gs_sumset[rem]`.

**Skipping the check when it cannot fire.** Early in the search all nine
pieces remain, and then *every* component size up to 29 is achievable -- the
flood fill would be pure overhead. `gs_dense_h[rem][h]` precomputes "is the
achievable-size set gap-free up to the total remaining area?"; if so the
pruner returns true without touching the board. Since the search always has
all its unused pieces near the root -- where the node counts are largest --
this guard roughly *doubled* throughput, one bit-test replacing a flood fill
at the majority of nodes.

### What it costs

Measured (this machine, one core): ~3 ms and a few thousand nodes for a
typical board, ~1.4 ms for a sparse 426-solution board. Note for later:
extrapolated over all boards, one-at-a-time solving would cost about
**7.1 core-hours**.

## 2.2 Dancing links, the referee

An independent implementation of the same count is worth a lot: two engines
sharing no code and no representation, agreeing on every board, make a deep
bug in either vastly less likely. The natural second engine is Knuth's
**Algorithm X with dancing links (DLX)** -- the classical method for exactly
this problem class.

### Exact cover in one paragraph

An exact-cover instance is a 0/1 matrix; a solution is a set of rows whose
1s hit every column exactly once. Tiling is exact cover: make one column per
constraint --

* 36 *cell* columns ("this cell is covered exactly once"),
* 9 *piece* columns ("this piece is used exactly once"),

-- and one row per legal placement (625 of them), with 1s in its piece column
and its 1-4 cell columns. A set of rows covering all 45 columns exactly once
is precisely a tiling that uses each piece once. Pegs are handled with a
trick worth remembering: before searching, **cover the 7 peg columns** -- the
standard DLX cover operation simultaneously marks those constraints satisfied
and deletes every row that would overlap a peg. No special cases in the
search itself.

### Algorithm X

```
if no columns remain: record solution
pick the column c with the fewest remaining rows     (the "S heuristic")
if c has zero rows: dead end
for each row r in c:
    add r to the partial solution
    cover every column that r has a 1 in
    recurse
    uncover them in reverse order
```

Branching on the *most constrained* column is what makes it strong: a column
with one candidate is forced and costs nothing to process; a column with zero
kills the branch immediately.

### The dancing part

The whole trick is the data structure that makes cover/uncover O(ones
removed) with zero memory allocation. Every 1 of the matrix is a node in a
torus of doubly-linked lists: `L/R` link the 1s of a row in a ring, `U/D`
link the 1s of a column in a ring through a per-column header that carries a
live size counter (`src/gs_dlx.c` stores the links as parallel `int` arrays,
which is friendlier to the cache than pointers).

Covering column `c` = unlink `c` from the header ring, then for each row in
`c`, unlink that row's *other* nodes from their columns:

```c
d->U[d->D[j]] = d->U[j];      /* splice node j out vertically */
d->D[d->U[j]] = d->D[j];
d->size[col[j]]--;
```

The magic is that a spliced-out node *keeps its own pointers*. Uncovering
walks the same nodes in exactly reverse order and re-executes the two stores
mirrored -- `U[D[j]] = j; D[U[j]] = j` -- and the torus is restored bit-for-
bit. Undo is not a log or a copy; it is the algebraic inverse of the splice.
That reversibility discipline (LIFO order, exact mirror) is the entire
correctness burden of DLX, and it is why the implementation is 120 lines.

### Why DLX is the referee and not the engine

Measured head-to-head on a 426-solution board: bitmask DFS 1.4 ms, DLX 3.1 ms
(`gs_solve --both` reproduces this and asserts the counts match). Three
structural reasons:

1. The bitmask state is two registers; DLX state is a pointer web with
   megabyte-scale working sets and cache-hostile traversal.
2. On a 6x6 grid the "lowest empty cell" rule is already a good branching
   heuristic; DLX's smarter column choice cannot win back its constant
   factor.
3. The killer app of the bitmask design is the *global* enumeration below,
   which needs to share work across boards -- a per-instance matrix structure
   has no way to express that.

DLX absolutely would win on, say, a 12x12 grid with 20 pieces, where
constraint propagation dominates. Tool for the job.

## 2.3 The global enumeration: all boards at once

Here is the observation the whole project pivots on. The nine pieces have
total area 29 = 36 − 7. So **any** packing of the nine pieces into the empty
6x6 grid leaves exactly 7 cells bare -- and is therefore a solution of
exactly one board: the one whose pegs sit on those 7 cells. Conversely every
solution of every board arises this way. The map

```
packings of the 9 pieces into the 6x6 grid
        <-->  pairs (board B, solution of B)
```

is a bijection. Consequently

```
sum over all 8,347,680 boards of solutions(B)  =  #packings
```

and *one* enumeration of packings, incrementing `counts[rank(holes)]` at each
leaf, computes the entire count vector simultaneously. There turn out to be
11,387,941,312 packings, so this identity also tells you the average board
has 1,364 solutions.

### Holes as a tenth piece

Implementation-wise (`src/gs_countall.c`), the trick costs one extra branch in
the solver: at the lowest undecided cell, besides "cover it with an unused
piece," the search may now declare it a **hole**, up to 7 times. The
completeness/uniqueness argument of section 2.1 extends verbatim -- in a
finished packing the lowest undecided cell is either bare (hole branch) or
covered by an anchored placement (piece branch), and exactly one branch
applies to any given packing. Feasibility pruning uses the hole-budget tables
from section 2.1.

Because holes are committed in increasing cell order, the lexicographic rank
of the hole set is maintained *incrementally* -- one closed-form term per hole
(part 1) -- and at the leaf the final rank is already in hand: the increment
is `counts[rp]++`, no ranking pass, and thanks to lex ordering, sibling
leaves hit neighboring addresses.

### The accounting

Why is this so much better than solving boards one at a time?

* Per-board solving pays the *search overhead* (the shallow, high-branching
  part of the tree, plus setup) 8.3 million times. The global tree pays it
  once: shallow prefixes are shared by construction, since a prefix does not
  yet know which board it belongs to.
* The work is proportional to the *answer* (11.4e9 leaves at 6.55 nodes per
  leaf, measured) rather than to #boards x average-search-cost.

Measured end to end: **~39 core-minutes** (591 s wall on 4 cores, with the
symmetry cut of part 3) versus ~7.1 core-hours extrapolated for one-at-a-time
-- an ~11x algorithmic improvement before any parallelism, and it
parallelizes embarrassingly.

## 2.4 Parallelism without atomics

The natural parallel unit is a subtree. `gs_countall` expands the root
**breadth-first** into ~200,000 frontier states (a few milliseconds -- each
state is 4 words), then worker threads repeatedly claim the next unprocessed
state and run the depth-first search on it.

Two design points carry the performance:

* **Dynamic claiming.** Subtree sizes span orders of magnitude (the first
  few percent of tasks contain a large fraction of all leaves -- deciding the
  A1 region pins down a lot). Static partitioning would leave threads idle;
  an index behind a mutex costs nanoseconds per task that runs for
  milliseconds.
* **Private counter arrays.** Each worker owns a full 33.4 MB `uint32`
  array and increments it with plain stores; arrays are summed once at the
  end. The alternative -- atomic increments into one shared array -- would
  put a lock prefix and cross-core cache-line traffic in the innermost loop
  of the entire computation, on essentially random high addresses. Memory is
  33 MB x threads, a fine trade at 4-16 threads.

The merged result is then checked, not trusted -- see part 3 for the
symmetry-based validation and `gs_countall`'s five-way verification suite
(sum-vs-leaves identity, mirror-fold identity, random re-solves against the
single-board engine, D4 equivariance, and the DLX cross-check via
`gs_solve --both`).

---

**Check your understanding**

1. In the completeness proof, where exactly is the fact used that already-
   decided cells are *below* the current lowest empty cell?
2. A component of size 6 remains and the unused pieces are {3x1, 2x2, T}.
   Feasible? (Sums: {0,3,4,7,8,11} -- no 6. Dead branch.)
3. Why does covering a peg's column in DLX automatically delete every
   placement overlapping that peg?
4. The mirror cut of part 3 halves the number of leaves. Why does the
   private-counter design mean it does *not* halve memory traffic at merge
   time?

Next: [Part 3 -- Symmetry](03-symmetry.md), where group theory removes a
factor of 8 from storage, 2 from search, and buys the strongest correctness
checks in the project.

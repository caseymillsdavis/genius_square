# Part 1: Boards, bits, and ranks

Everything in this project stands on three representation decisions: boards
are 36-bit integers, piece placements are precomputed bitmasks bucketed by
their lowest cell, and boards are numbered by a *lexicographic* combinatorial
ranking. None of the three is exotic, but each was chosen for a specific
reason that pays off later, and this part derives all of them.

## 1.1 The board is one machine word

Cells are numbered row-major:

```
     A  B  C  D  E  F
 1   0  1  2  3  4  5
 2   6  7  8  9 10 11
 3  12 13 14 15 16 17
 4  18 19 20 21 22 23
 5  24 25 26 27 28 29
 6  30 31 32 33 34 35
```

A set of cells is a `uint64_t` with bit `i` set iff cell `i` is in the set
(`gs_mask`). A *board* is such a mask with exactly 7 bits set (the pegs).
The all-cells mask is `GS_FULL = 0xFFFFFFFFF` (nine F's = 36 ones).

Why this beats a `char grid[6][6]`:

* **Set algebra is single instructions.** Overlap testing is `a & b`,
  placing a piece is `a | b`, the empty region is `~filled & GS_FULL`.
* **Iteration is `ctz`.** `__builtin_ctzll(m)` finds the lowest element;
  `m &= m - 1` deletes it. The pair costs ~2 cycles.
* **State fits in registers.** The whole search state of the solver is a
  couple of words, so recursion is cheap and there is nothing to undo on
  backtrack -- passing `filled | m` down the stack *is* the undo mechanism.

### Neighborhoods without falling off the edge

Moving one cell right is `x << 1` -- except that bit 5 (`F1`) would slide
into bit 6 (`A2`), which is a different row. The fix is a *guard mask*
applied before the shift: `NOT_C5` is the set of cells whose column is not F,
`NOT_C0` the set whose column is not A. Then the 4-neighborhood expansion of
a set `x`, clipped to a region, is branch-free:

```c
/* src/gs_core.h */
static inline gs_mask gs_grow(gs_mask x, gs_mask region)
{
    const gs_mask NOT_C5 = 0x7DF7DF7DFULL; /* cells whose col != 5 */
    const gs_mask NOT_C0 = 0xFBEFBEFBEULL; /* cells whose col != 0 */
    gs_mask r = x | (x << GS_N) | (x >> GS_N)      /* down, up   */
                  | ((x & NOT_C5) << 1)            /* right      */
                  | ((x & NOT_C0) >> 1);           /* left       */
    return r & region;
}
```

Vertical shifts (`<< 6`, `>> 6`) need no guard: bits pushed past position 35
or below 0 are annihilated by the final `& region`. This one inline function
is the entire geometry engine; flood fill (part 2) is just `gs_grow` iterated
to a fixed point.

## 1.2 Pieces and the placement table

The nine pieces are *free* polyominoes -- physical tiles you may rotate and
flip. Each is stored as a canonical list of (row, col) offsets, e.g. the
S/Z tetromino is `{0,0},{0,1},{1,1},{1,2}`.

Placements are generated in `gs_build_placements` (`src/gs_core.c`) by brute
force, once, at startup:

1. **Orientations.** Apply all 8 symmetries of the square to the offset list
   (each is one of `(r,c) -> (±r,±c) / (±c,±r)`), then *normalize*: translate
   so the minimum row and column are 0, and sort the cells row-major.
   Normalized duplicates are discarded. A fully asymmetric piece (the L
   tetromino) keeps 8 orientations; the 2x2 square keeps 1; the S/Z keeps 4
   (its mirror images are related by rotation two at a time).
2. **Translations.** Slide each orientation to every position where its
   bounding box fits in the 6x6 grid, and record the resulting bitmask.

The totals are worth memorizing because they recur in every cost estimate:

| piece | orientations | placements |
|---|---:|---:|
| 1x1 | 1 | 36 |
| 2x1 | 2 | 60 |
| 3x1 | 2 | 48 |
| L-tromino | 4 | 100 |
| L-tetromino | 8 | 160 |
| S/Z | 4 | 80 |
| 4x1 | 2 | 36 |
| 2x2 | 1 | 25 |
| T | 4 | 80 |
| **total** | | **625** |

(A quick sanity check you can do in your head: a `h x w` bounding box has
`(7-h)(7-w)` positions; the L-tromino's 4 orientations each occupy a 2x2 box,
so 4 * 25 = 100.)

### The anchor index

Define a placement's **anchor** as its lowest-numbered cell
(`__builtin_ctzll(mask)`). The table is sorted by `(anchor, piece)` and two
small index arrays give, for every `(cell, piece)` pair, the half-open range
of placements with that anchor:

```c
/* src/gs_core.h */
gs_placement p[GS_MAXPLACE];   /* sorted by (anchor, piece)        */
gs_mask      pmask[GS_MAXPLACE]; /* masks only, for the hot loop   */
int lo[GS_CELLS][GS_NPIECES];  /* p[lo[c][q] .. hi[c][q]) have     */
int hi[GS_CELLS][GS_NPIECES];  /*   anchor c and piece q           */
```

This is the data structure that makes the solver's branching rule ("cover the
lowest empty cell") a *table lookup*: all candidate placements for that cell
sit in one contiguous run. The parallel `pmask[]` array exists purely for
cache reasons -- the inner loop only needs the 8-byte mask, not the 16-byte
record, so streaming `pmask` halves the bytes touched.

Why anchors work as a complete enumeration rule is a search-correctness
argument, deferred to part 2.

## 1.3 Numbering the boards: the combinatorial number system

We need a bijection between the C(36,7) = 8,347,680 boards and the integers
`0 .. 8347679`, so that "the count of board B" can be an array subscript.
Both directions must be fast (`rank` runs in the innermost loop of the global
enumeration).

The classical tool is the **combinatorial number system**: for any fixed k,
the k-subsets of {0..n-1} in some canonical order are in bijection with
`0 .. C(n,k)-1` via sums of binomial coefficients. There are two natural
canonical orders, and the difference between them turned into a real
performance decision, so we do both carefully.

### Colex, the textbook version

Order subsets by comparing their *largest* elements first ("colexicographic").
The rank of `{c1 < c2 < ... < ck}` is simply

```
rank_colex = C(c1,1) + C(c2,2) + ... + C(ck,k)
```

Reason: the number of k-subsets whose largest element is *less than* `ck` is
C(ck, k), so that term counts all subsets that finish earlier; recurse on the
remaining k-1 elements below `ck`. It is beautiful and branch-light, and the
project uses it -- but only for the small internal tables of the analysis
(part 4), not for boards.

### Lex, what boards actually use

Order subsets like words: compare *smallest* elements first, so
`{0,1,2,3,4,5,6}` is rank 0 and `{29,...,35}` is last. To rank
`h1 < h2 < ... < h7`, count the subsets that come strictly earlier. A subset
comes earlier iff at some position `i` it agrees with ours on `h1..h(i-1)`
and its i-th element `x` is smaller than `hi`. For each such `x`
(`h(i-1) < x < hi`), the remaining `7-i` elements range freely above `x`,
giving `C(36-1-x, 7-i)` subsets. Summing over `x` telescopes by the
hockey-stick identity into closed form:

```
rank_lex = sum over i of [ C(35 - h(i-1), 8-i) - C(36 - hi, 8-i) ]     (h0 = -1)
```

which is exactly the expression in `gs_rank` (`src/gs_core.c`), evaluated
with one precomputed table `gs_binom[][]` and one `ctz` per element. The
crucial *incremental* property: the term for peg `i` depends only on `hi` and
its predecessor -- so a search that discovers the pegs in increasing order can
maintain the partial rank as it goes, adding one term per committed peg. The
global enumeration in part 2 does precisely this.

For general (n, k) the formula reads

```
rank_lex = sum over i=1..k of [ C(n-1-h(i-1), k+1-i) - C(n-hi, k+1-i) ],  h0 = -1
```

(the board case is n = 36, k = 7, matching `gs_binom[35 - prev][8 - i] -
gs_binom[36 - c][8 - i]` in the code).

#### Worked example (small enough to check by hand)

Take n = 5, k = 2, so C(5,2) = 10 subsets and each term is
`C(4 - prev, 3-i) - C(5 - hi, 3-i)`:

| subset | i=1 term | i=2 term | rank |
|---|---|---|---:|
| {0,1} | C(5,2)−C(5,2) = 0 | C(4,1)−C(4,1) = 0 | 0 |
| {0,2} | 0 | C(4,1)−C(3,1) = 1 | 1 |
| {0,3} | 0 | C(4,1)−C(2,1) = 2 | 2 |
| {0,4} | 0 | C(4,1)−C(1,1) = 3 | 3 |
| {1,2} | C(5,2)−C(4,2) = 4 | C(3,1)−C(3,1) = 0 | 4 |
| {1,3} | 4 | C(3,1)−C(2,1) = 1 | 5 |
| {1,4} | 4 | C(3,1)−C(1,1) = 2 | 6 |
| {2,3} | C(5,2)−C(3,2) = 7 | C(2,1)−C(2,1) = 0 | 7 |
| {2,4} | 7 | C(2,1)−C(1,1) = 1 | 8 |
| {3,4} | C(5,2)−C(2,2) = 9 | C(1,1)−C(1,1) = 0 | 9 |

Note how the first column only moves when the *smallest* element moves --
that is the windowing property in miniature. (And note that hand arithmetic
here is easy to fumble, which is why `gs_selftest` round-trips
`rank(unrank(r)) == r` across the whole 8,347,680-board space rather than
trusting anyone's algebra.)

Unranking (`gs_unrank`) inverts greedily: choose the smallest `h1` such that
the block of subsets starting with something `<= h1` covers the target rank,
subtract, recurse. Seven short scans of the binomial table.

### Why lex and not colex: cache locality is a ranking property

The global enumeration performs ~11.4 billion increments of
`counts[rank(holes)]` on a 33.4 MB array. The access *pattern* is inherited
from the ranking function:

* Under **lex**, fixing the first `j` (lowest) pegs confines the rank to a
  contiguous interval of length `C(35 - hj, 7-j)`. The search decides cells
  in increasing order, so an entire DFS subtree writes into one window that
  *shrinks geometrically with depth* -- by the time the last pegs are being
  placed, the window is a few hundred bytes and lives in L1. Sibling leaves
  touch adjacent addresses.
* Under **colex**, the rank is dominated by the term `C(c7, 7)` of the
  *largest* peg -- exactly the coordinate that varies fastest deep in the
  search. Consecutive leaves would scatter increments across the entire
  33 MB array, and essentially every one would be a cache miss.

Same information content, ~identical instruction count, an order-of-magnitude
difference in memory behavior. This is the single most consequential "boring"
decision in the project, which is why `AGENTS.md` warns against changing it:
every stored data file is indexed by this function.

## 1.4 Parsing, printing, and the human-facing convention

Cell names follow the physical game: column letter A-F, row digit 1-6, so
cell 0 is `A1`, cell 5 is `F1`, cell 35 is `F6` (`gs_parse_cell`,
`gs_cell_name`). Boards render as ASCII grids with `#` for pegs and one
letter per piece (`gs_render`), which is how `gs_solve` prints solutions:

```
   A B C D E F
1  # 1 # 2 # 3
2  J J T 2 I 3
3  J T T T I 3
4  J # S # I #
5  O O S S I L
6  O O # S L L
```

One convention to internalize: **row-major from the top-left, and every
data structure in the repo agrees with it** -- the bitmasks, the ranking,
the symmetry permutations of part 3, and the row-by-row mirror test that
part 3 builds on top of them.

---

**Check your understanding**

1. Why does the vertical shift in `gs_grow` need no guard mask while the
   horizontal one does?
2. The 4x1 piece has 36 placements and so does the 1x1. Coincidence?
   (Compute `(7-h)(7-w)` for both orientations of the 4x1.)
3. If you fixed the three lowest pegs of a board at cells 0, 1, 2, how long
   is the interval of lex ranks the board can still fall in? (Answer:
   C(33,4) = 40,920 -- the windowing that makes the counter writes local.)

Next: [Part 2 -- Search](02-search.md), where these representations get used
11 billion times.

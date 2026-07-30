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

### First, the bridge: a board *is* a subset

The literature on this speaks of subsets and their elements, so pin down what
those are here before any formulas appear. One board, three equivalent
descriptions:

| view | pegs at A1, C1, B2, D2, B3, A4, B5 |
|---|---|
| grid | seven pegs sitting on seven squares |
| 36-bit mask | bits 0, 2, 7, 9, 13, 18, 25 are 1, the other 29 are 0 |
| **subset** | **{0, 2, 7, 9, 13, 18, 25} contained in {0,...,35}** |

The **subset is the set of cell indices that hold a peg**, and its
**elements are those cell indices** -- ordinary integers in `0..35`. A
weight-7 mask and a 7-element subset carry identical information (bit `i` is
set exactly when `i` is in the subset); set language is used below only
because phrases like "compare their largest elements" are clumsy to say in
bit terms.

So whenever the text says *order subsets by comparing their largest (or
smallest) elements*, read: **sort the seven peg cell indices ascending and
compare those seven numbers.** For the board above the sorted list is

```
h1=0, h2=2, h3=7, h4=9, h5=13, h6=18, h7=25
```

and `h1..h7` is the notation used throughout.

### Why an ordering is what we want

The numbering scheme is: **pick a total order on the boards, then let
`rank(B)` be the number of boards that come strictly before B.**

A *total order* is a rule for comparing any two boards that lines all of them
up in a single queue -- no ties, and an answer for every pair. Formally,
writing `a <= b` for "a comes first", the rule must be reflexive (`a <= a`),
antisymmetric (`a <= b` and `b <= a` only when `a = b`), transitive, and --
the interesting one -- **total**: for every pair, either `a <= b` or
`b <= a`. The first three properties alone make a *partial* order, where
some pairs may simply be incomparable.

An example of the difference, using our own objects: order boards by
*containment* (`A <= B` iff A is a subset of B). Reflexive, antisymmetric and
transitive, but hopeless here -- no 7-cell board contains a different 7-cell
board, so containment declines to compare any two of them. A rule that
shrugs at some pairs yields a branching structure, not a queue, and you
cannot number a branching structure by position.

Lex and colex both answer every time: two distinct boards have different
sorted peg lists, so scanning those lists (from the front for lex, from the
back for colex) must reach a position where they differ, and the smaller cell
number there decides. That is exactly why there are two *valid* schemes to
choose between rather than one.

Given such an order, `rank(B) = #{A : A < B}` is automatically a bijection
onto `0 .. C(36,7)-1`:

* if `A < B` then everything before A is before B too, plus A itself, so
  `rank(A) < rank(B)` -- distinct boards get distinct ranks;
* every rank counts a sub-collection of the other boards, so it lies in
  `0 .. N-1`;
* an injective map between finite sets of the same size is onto, so every
  index is used exactly once.

Totality is what powers the first bullet -- with incomparable pairs, two
boards could tie and collide. (The everyday analogue: a total order is what
a comparison sort needs. `strcmp` totally orders strings, which is why a
dictionary lists every word in one sequence and "the 4,182nd word" means
something.)

All the binomial coefficients below are just closed forms for "how many come
before", computed without building any list.

The classical tool for that is the **combinatorial number system**: for fixed
k, the k-subsets of `{0..n-1}` in a canonical order correspond to
`0 .. C(n,k)-1` via sums of binomial coefficients. There are two natural
orders -- because there are two natural ends of the sorted list to start
comparing from -- and choosing between them turned out to be a real
performance decision, so we do both carefully.

### Colex, the textbook version

Compare the sorted peg lists starting from the *largest* peg cell
("colexicographic"). The rank of a board with pegs `c1 < c2 < ... < ck` is

```
rank_colex = C(c1,1) + C(c2,2) + ... + C(ck,k)
```

Reason: the number of boards whose largest peg cell is *less than* `ck` is
C(ck, k) -- choose all k pegs from the cells `0..ck-1` -- and every one of
them sorts earlier, so that term counts them all at once; then recurse on the
remaining k-1 pegs below `ck`. It is beautiful and branch-light, and the
project does use it -- but only for the small internal tables of the analysis
(part 4), never for boards. The next subsection says why.

### Lex, what boards actually use

Compare the sorted peg lists starting from the *smallest* peg cell, exactly
as a dictionary compares words letter by letter. So the board with pegs
`{0,1,2,3,4,5,6}` (the top row plus A2) is rank 0, and `{29,...,35}` is last.

To rank `h1 < h2 < ... < h7`, count the boards that come strictly earlier.
Another board comes earlier exactly when, at the first position `i` where the
two sorted lists differ, its i-th peg is smaller than `hi`. So walk the
positions and count:

* boards agreeing with ours on `h1..h(i-1)` whose i-th peg is some
  `x` with `h(i-1) < x < hi`;
* for each such `x` the remaining `7-i` pegs may sit anywhere among the
  `36-1-x` cells above `x`, giving `C(36-1-x, 7-i)` boards.

Concretely for our running example `{0, 2, 7, 9, 13, 18, 25}`:

| position | boards counted here | how many |
|---|---|---|
| i=1 | lowest peg `x < 0` | 0 -- nothing beats cell 0 |
| i=2 | `h1=0`, second peg `x = 1`, other 5 pegs free above | C(34,5) = 278,256 |
| i=3 | `h1=0, h2=2`, third peg `x` in {3,4,5,6}, other 4 free | C(32,4)+C(31,4)+C(30,4)+C(29,4) = 118,581 |
| ... | ... | ... |
| | **total** | **400,675 = `gs_rank`** |

(That total is worth a look in `docs/RESULTS.md`: rank 400,675 is one of the
100 orbits of boards with a *unique* solution.)

Summing over `x` within each position telescopes by the hockey-stick identity
(`sum_{j=a..b} C(j,r) = C(b+1,r+1) - C(a,r+1)`) into a two-term closed form
per position:

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
  contiguous interval of length `C(35 - hj, 7-j)` -- visible in the worked
  table above, where the `i=1` column stays put while later columns move.
  The search decides cells in increasing order, so it discovers pegs
  smallest-first, and an entire DFS subtree writes into one window that
  *shrinks geometrically with depth*: knowing `h1=0` leaves C(35,6) = 1.6M
  possible ranks, knowing `h1,h2,h3` leaves C(28,4) = 20,475, and by the last
  pegs the window is a few hundred bytes living in L1. Sibling leaves touch
  adjacent addresses.
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

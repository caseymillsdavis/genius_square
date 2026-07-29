# Part 3: Symmetry

The square has eight symmetries, and they show up in this project four
separate times: as a *validator* (counts must respect them), as a *search
cut* (skip work whose answer is forced by work already done), as a
*compressor* (store one board per orbit), and as pure *mathematics* (orbit
counting). This part builds the group theory from zero, because every ounce
of it gets used.

## 3.1 Crash course: actions, orbits, stabilizers

The symmetry group of the square is **D4**: four rotations (0, 90, 180,
270 degrees) and four reflections (horizontal, vertical, two diagonals).
Eight elements. Each element permutes the 36 cells -- e.g. rot90 maps
(r, c) to (c, 5-r) -- so each is stored simply as `uint8_t perm[36]`
(`gs_perm` in `src/gs_core.c`), and applying one to a board is a loop over
set bits.

Vocabulary, with the only three facts we need:

* The **orbit** of a board B is the set { gB : g in D4 } -- B together with
  its rotations and reflections. Orbits partition the set of all boards.
* The **stabilizer** of B is { g : gB = B }, the symmetries B itself has.
  It is a subgroup, and the **orbit-stabilizer theorem** says
  |orbit| x |stabilizer| = |D4| = 8; so orbits here have size 1, 2, 4 or 8.
* **Burnside's lemma**: the number of orbits equals the *average number of
  fixed points* of the group elements,
  `#orbits = (1/|G|) * sum over g of #Fix(g)`.
  Proof in one line: count pairs (g, B) with gB = B both ways --
  `sum_g #Fix(g) = sum_B |Stab(B)| = sum over orbits O of |O| * (8/|O|)
  = 8 * #orbits`.

## 3.2 Burnside, fully worked

To count distinct-up-to-symmetry boards we need, for each of the 8
symmetries, the number of 7-cell subsets it fixes. A subset is fixed by g
iff it is a **union of cycles** of g's permutation of the cells. So write
down each element's cycle structure on the 36 cells:

| g | cycle structure | fixed 7-subsets |
|---|---|---:|
| identity | 36 fixed cells | C(36,7) = 8,347,680 |
| rot90, rot270 | nine 4-cycles | 7 is not a sum of 4s: **0** |
| rot180 | eighteen 2-cycles | 7 is odd: **0** |
| mirror-h, mirror-v | eighteen 2-cycles (no cell on the axis -- the grid is even) | **0** |
| transpose | 6 fixed (the diagonal) + fifteen 2-cycles | see below |
| anti-transpose | 6 fixed + fifteen 2-cycles | same |

For the transpose: choose j 2-cycles and 7-2j diagonal cells,

```
sum over j of C(15,j) * C(6, 7-2j)
  = C(15,1)C(6,5) + C(15,2)C(6,3) + C(15,3)C(6,1)
  = 15*6 + 105*20 + 455*6
  = 90 + 2100 + 2730 = 4,920.
```

Burnside then gives

```
#orbits = (8,347,680 + 0+0+0+0+0 + 4,920 + 4,920) / 8 = 1,044,690.
```

(`gs_selftest` recomputes this number generically -- cycle decomposition plus
a knapsack DP over cycle lengths -- so the code and the hand calculation
check each other.)

### The orbit-size distribution, for free

Which orbit sizes actually occur? A stabilizer is a subgroup all of whose
elements fix the board -- but the table shows only the identity and the two
diagonal reflections fix *any* board. A subgroup containing both diagonals
also contains their product, rot180 (fixes nothing), so it cannot occur.
Possible stabilizers: {e} (orbit size 8), or {e, one diagonal} (orbit size
4). Orbits of size 1 and 2 are impossible.

Counting the size-4 orbits: each contains exactly two transpose-fixed
members (B and rot180 B; the other two members are fixed by the *anti*-
diagonal instead, since conjugating by a rotation swaps the two diagonal
axes). With 4,920 transpose-fixed boards total,

```
#size-4 orbits = 4,920 / 2 = 2,460
#size-8 orbits = (8,347,680 - 4*2,460) / 8 = 1,042,230
check:            2,460 + 1,042,230 = 1,044,690  ✓
```

This is why every orbit column in `docs/RESULTS.md` reads 4 or 8, never 1
or 2 -- a fact the statistics code did not assume but the data confirms.

## 3.3 Counts are constant on orbits -- and why that's not automatic

The claim `solutions(gB) = solutions(B)` looks obvious but rests on a
property of the *piece set*: applying g to every placement of a solution of
B yields a solution of gB **only if each transformed placement is still a
legal placement** -- i.e. the piece set must be closed under D4. Ours is,
because the pieces are free polyominoes (flipping is allowed). If the game
had, say, only the S-tetromino and not its Z mirror, counts would *not* be
mirror-invariant.

`gs_selftest` verifies closure directly (every symmetry image of each of the
625 placements is again a placement of the same piece), and then verifies the
consequence on 200 random boards by brute force. This equivariance is the
single most valuable invariant in the project: it is sensitive to bugs in
placement generation, in the permutations, in ranking, and in the global
enumeration, and it is checked again on the finished count table.

## 3.4 Canonical forms, and an ordering subtlety

To store one number per orbit we need a deterministic representative: the
**canonical form** `gs_canon(B) = the lex-least of the 8 images`, where
"lex" compares boards as *sorted cell lists* -- the board containing the
lowest differing cell is smaller. This matches the lex *rank* order of
part 1, giving the tidy property

```
rank(canon(B)) = min over the orbit of rank  <=  rank(B).
```

The subtlety flagged in `AGENTS.md`: sorted-list-lex is **not** integer `<`
on the bitmasks. Integer comparison is decided by the *highest* differing
bit, list-lex by the *lowest* differing cell. For equal popcount they order
subsets differently, and the mirror cut below depends on the list-lex
version (it is the one decidable from low cells first). `lex_less()` in
`src/gs_core.c` implements it with two bit tricks:
`d = a ^ b; a & (d & -d)` -- isolate the lowest differing cell, ask who has
it.

The canonical boards are *derived, never stored*: `gs_canon_list()` walks all
8.3M subsets in lex order (the odometer loop) and keeps those equal to their
canonical form -- 1,044,690 of them, reproduced identically by every tool
that needs the index.

## 3.5 The mirror cut: symmetry inside the search

Storage-level symmetry is easy; *search-level* symmetry is where it gets
interesting. We would like the global enumeration to skip subtrees whose
hole sets are non-canonical (their counts follow from canonical ones). The
obstacle: the search decides cells in increasing order, so a symmetry test
must be **decidable from a prefix** -- from the low cells only -- or it is
useless as a cut.

Ask, for each g in D4: to compare S with g(S) on the first few cells, which
cells of S must be known? The comparison needs g^-1 of the low cells:

| g | g maps row 1 to | testable... |
|---|---|---|
| mirror-v (left-right) | row 1 | after each completed row ✓ |
| rot90 / rot270 | a column | only near the end |
| rot180, mirror-h | row 6 | only at the very end |
| diagonals | column A / F | only near the end |

Exactly one element besides the identity maps each row to itself: the
left-right mirror. So the cut implemented in `gs_countall.c` is: **keep a
hole set only if it is list-lex <= its own left-right mirror**, decided
incrementally -- whenever a row completes, compare its 6-bit hole pattern
against its bit-reversal (`rev6[]` table); the first asymmetric row settles
the comparison for good.

Correctness: a D4-canonical set is lex-least in its whole orbit, in
particular <= its mirror image, so *every canonical board survives the cut*.
After the run, the full table is rebuilt from the canonical entries alone
(`gs_expand_canon`), so the extra survivors are merely ignored. `gs_selftest`
hammers the equivalence of the row-wise test and a whole-board comparison on
300,000 random boards, because this is the easiest place in the project to
write a subtle bug.

Two numerical facts about the cut, one exact and one measured:

* **Leaves are halved exactly.** A mirror-symmetric 7-subset would need a
  fixed cell (none exist) or even size -- impossible. So boards pair up
  perfectly and the retained leaf count is exactly half:
  5,693,970,656 = 11,387,941,312 / 2. The run verifies this "fold identity"
  (each retained count used once for itself and once for its mirror).
* **Time shrinks by ~1.6x, not 2x.** Pruning can only trigger when a
  completed row is asymmetric *in the mirror-larger direction*; subtrees
  that decide symmetric or mirror-smaller rows first are never cut. In
  particular the whole "hole at A1" subtree -- which contains a board's
  lowest cell in column A -- is always lex-smaller than its mirror
  immediately, is never cut, and is enormous. Measured nodes/leaf rose from
  6.07 to 6.55, the classic signature of pruning that removes leaves faster
  than nodes.

**Why not use all eight symmetries** and aim for 8x? To make rot90 testable
early you would have to decide cells in an order that interleaves rows and
columns (orbit by orbit). That destroys the compact row-major frontier, and
with it the effectiveness of the connectivity prune (pockets form behind a
ragged frontier long before their infeasibility is noticed). The judgment
call -- documented rather than hidden -- was that a guaranteed 1.6x with
50 lines and a strong test beats a speculative 8x that risks the pruning
that delivers orders of magnitude. `--nosym` turns the cut off, which is
both the fallback and the cross-check.

## 3.6 Symmetry as compression

The stored artifact `data/counts.gsc` applies the orbit reduction and then a
representation trick:

1. **Canonical-only**: 1,044,690 uint32 counts instead of 8,347,680 --
   4.18 MB instead of 33.4 MB. Expansion is exact: enumerate each canonical
   board's orbit, write its count at each member's rank.
2. **Byte-plane split + deflate**: instead of compressing the uint32 stream
   (where each count's bytes interleave), store all byte-0s, then all
   byte-1s, etc., and deflate each plane separately. The planes have wildly
   different entropy: the max count is 100,593 < 2^17, so plane 3 is all
   zeros and plane 2 is zero except for literally two orbits; plane 1 is
   small-valued (the median count is 719, high byte 2); plane 0 is
   near-random and incompressible. Mixing them wastes the compressor's
   model on the noisy byte; separating them lets three planes vanish.

Net: 1.43 bytes per orbit, 1.49 MB on disk -- a 22x reduction from the flat
array, of which a factor 8 is group theory and the rest is knowing where the
entropy lives. (The same columnar idea appears in database formats and
image codecs; it is worth having in your toolbox.)

---

**Check your understanding**

1. Why does an even-sided grid make all four axis mirrors fixed-point-free
   on cells, and why does that plus |pegs| = 7 forbid mirror-symmetric
   boards?
2. The hardest-boards tables show orbit sizes 4 and 8 only. Derive from the
   Burnside table which boards have orbit size 4. (Answer: exactly the
   9,840 diagonal-symmetric boards; each size-4 orbit contains four of
   them -- two fixed by each diagonal -- giving 2,460 orbits.)
3. Suppose the game's piece set had the S-tetromino but not its mirror.
   Which verification in `gs_selftest` would fail first?
4. Plane 2 of the count file has exactly two nonzero bytes. Which boards do
   they belong to? (Hint: part 2's histogram -- counts >= 65,536.)

Next: [Part 4 -- The analysis](04-analysis.md): what the completed table
says about *why* boards are hard, and the mathematics for saying it
precisely.

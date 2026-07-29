# Data formats and indexing

## Cells

`cell = row * 6 + col`, row 0 at the top, column 0 at the left. Algebraic
names follow the physical game: column letter `A`..`F`, row digit `1`..`6`,
so `A1` is cell 0, `F1` is cell 5, `A6` is cell 30, `F6` is cell 35.

```
     A  B  C  D  E  F
 1   0  1  2  3  4  5
 2   6  7  8  9 10 11
 3  12 13 14 15 16 17
 4  18 19 20 21 22 23
 5  24 25 26 27 28 29
 6  30 31 32 33 34 35
```

A board is a 36-bit mask with exactly 7 bits set (the pegs).

## Board index

Boards are indexed by the **lexicographic rank** of the sorted 7-tuple of peg
cells, in `0 .. 8347679`. Rank 0 is `{0,1,2,3,4,5,6}`; the last rank is
`{29,..,35}`.

```
    rank = sum over i=1..7 of [ C(36 - h_{i-1} - 1, 8-i) - C(36 - h_i, 8-i) ]
```

with `h_0 = -1` and `h_1 < .. < h_7` the peg cells. `gs_rank` / `gs_unrank`
implement this and are round-trip tested over the entire index space.

Lexicographic rather than colexicographic is a deliberate choice; see
ALGORITHMS.md ("Board indices are lexicographic on purpose").

## Canonical boards

D4 (the 8 symmetries of the square) acts on boards. The canonical
representative of an orbit is the member whose sorted cell list is
lexicographically smallest, so a canonical board always has the smallest rank
in its orbit. There are **1,044,690** canonical boards. Orbits have size 1, 2,
4 or 8.

The ascending list of canonical ranks is *derived*, never stored:
`gs_canon_list()` regenerates it in about a second by walking all 7-subsets in
lex order and keeping the ones equal to their own canonical form.

## `.gsc` -- canonical count table (the primary artifact)

```
offset  type        meaning
0       u32         magic 0x43515347  ("GSQC")
4       u32         version = 1
8       u32         nboards = 8347680
12      u32         ncanon  = 1044690
16      u32         nplanes = 4
20      u32         reserved = 0
24      ...         four deflated byte planes, each:
                        u64 compressed length
                        u64 raw length (= ncanon)
                        compressed bytes (zlib)
```

Counts for the canonical boards, in ascending canonical-rank order, are split
into four byte planes -- all the low bytes, then all the second bytes, and so
on -- and each plane is deflated separately. Byte-plane splitting is what makes
this compress: the top two bytes of the counts are almost entirely zero and
collapse to nothing, while the low byte is near-random and is stored roughly
verbatim.

Size progression: 33.4 MB flat over all boards, 4.2 MB canonical-only, ~2 MB
after byte-plane deflate.

Reading back is `gs_read_canon()`; `gs_expand_canon()` reinflates the full
8,347,680-entry array by applying the orbit of each canonical board.

## `.u32` -- flat table (optional, for tools that want mmap-style access)

```
offset  type        meaning
0       u32         magic 0x46515347  ("GSQF")
4       u32         version = 1
8       u32         nboards = 8347680
12      u32         reserved
16      u32[8347680]  counts, indexed directly by gs_rank
```

33.4 MB. Produced by `gs_countall --full FILE`. Not committed to the
repository; regenerate it if you want it.

## Looking a board up

```c
gs_init_all();
uint32_t n;
uint32_t *canon = gs_read_canon("data/counts.gsc", &n);
uint32_t *list  = gs_canon_list(&n);
uint32_t *full  = gs_expand_canon(canon, list, n);   /* 33 MB */

gs_mask b = gs_parse_board("A1 C1 E1 B4 D4 F4 C6");
printf("%u solutions\n", full[gs_rank(b)]);
```

To avoid the 33 MB expansion, canonicalize and binary-search instead:

```c
gs_mask c = gs_canon(b);
uint32_t r = gs_rank(c);
/* binary search r in list[0..n) -> slot; count = canon[slot] */
```

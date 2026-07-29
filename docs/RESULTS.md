# Genius Square: solution-count statistics

All 8347680 = C(36,7) placements of the 7 pegs, grouped into 1044690 orbits
under the symmetry group D4 of the square.

| quantity | value |
|---|---|
| boards (7-peg placements) | 8347680 |
| distinct boards up to symmetry | 1044690 |
| total packings of the 9 pieces into the 6x6 grid | 11387941312 |
| mean solutions per board | 1364.20 |
| unsolvable boards | 172440 (2.0657%) |
| unsolvable boards up to symmetry | 21757 |
| solvable boards | 8175240 (97.9343%) |
| most solutions | 100593 (board `A1 B1 C1 D1 E1 F1 A2`) |
| median solutions | 719 |

## Exact counts at the low end

| solutions | orbits | boards | cumulative boards |
|---:|---:|---:|---:|
| 0 | 21757 | 172440 | 172440 |
| 1 | 100 | 800 | 173240 |
| 2 | 167 | 1324 | 174564 |
| 3 | 166 | 1328 | 175892 |
| 4 | 260 | 2056 | 177948 |
| 5 | 234 | 1872 | 179820 |
| 6 | 279 | 2208 | 182028 |
| 7 | 285 | 2280 | 184308 |
| 8 | 339 | 2668 | 186976 |
| 9 | 350 | 2800 | 189776 |
| 10 | 367 | 2908 | 192684 |
| 11 | 372 | 2976 | 195660 |
| 12 | 432 | 3432 | 199092 |
| 13 | 405 | 3240 | 202332 |
| 14 | 496 | 3940 | 206272 |
| 15 | 459 | 3672 | 209944 |
| 16 | 475 | 3764 | 213708 |
| 17 | 465 | 3720 | 217428 |
| 18 | 551 | 4376 | 221804 |
| 19 | 533 | 4264 | 226068 |
| 20 | 545 | 4348 | 230416 |

## Histogram of boards by number of solutions

| solutions | orbits | boards | % of boards | cumulative % |
|---|---:|---:|---:|---:|
| 0 (unsolvable) | 21757 | 172440 | 2.06572 | 2.06572 |
| 1 | 100 | 800 | 0.00958 | 2.07531 |
| 2 - 3 | 333 | 2652 | 0.03177 | 2.10708 |
| 4 - 7 | 1058 | 8416 | 0.10082 | 2.20789 |
| 8 - 15 | 3220 | 25636 | 0.30710 | 2.51500 |
| 16 - 31 | 9524 | 75992 | 0.91034 | 3.42533 |
| 32 - 63 | 26381 | 210584 | 2.52266 | 5.94800 |
| 64 - 127 | 64127 | 512308 | 6.13713 | 12.08513 |
| 128 - 255 | 115311 | 921320 | 11.03684 | 23.12197 |
| 256 - 511 | 174558 | 1395264 | 16.71439 | 39.83636 |
| 512 - 1023 | 222614 | 1779672 | 21.31936 | 61.15572 |
| 1024 - 2047 | 207563 | 1659352 | 19.87800 | 81.03372 |
| 2048 - 4095 | 129606 | 1035956 | 12.41011 | 93.44383 |
| 4096 - 8191 | 52404 | 418664 | 5.01533 | 98.45916 |
| 8192 - 16383 | 13713 | 109376 | 1.31026 | 99.76942 |
| 16384 - 32767 | 2242 | 17864 | 0.21400 | 99.98342 |
| 32768 - 65535 | 177 | 1368 | 0.01639 | 99.99981 |
| 65536 - 100593 | 2 | 16 | 0.00019 | 100.00000 |

## Hardest boards

A board is called **trapped** when some piece has the *same* placement in
every solution -- the pegs pin it. The classic case is an empty cell with
all four neighbours blocked, which nails the 1x1. Such boards dominate the
low end of the distribution but are not interesting as puzzles, so they are
listed separately from the trap-free ones.

Note that a board with a unique solution is trapped by definition (every
piece is pinned), so trap-free boards necessarily have several solutions.

### Fewest solutions overall (any board)

| pegs | solutions | orbit | rank | pinned pieces |
|---|---:|---:|---:|---|
| `A1 C1 B2 D2 B3 A4 B5` | 1 | 8 | 400675 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `A1 C1 B2 D2 F3 A4 D6` | 1 | 8 | 401407 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `A1 C1 F2 C4 D5 C6 E6` | 1 | 8 | 471662 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `A1 C1 D3 F3 E4 A5 B6` | 1 | 8 | 496682 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `A1 D1 E2 D3 B4 A5 B6` | 1 | 8 | 658373 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `A1 D1 E2 B4 E4 A5 B6` | 1 | 8 | 661672 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `A1 E2 D3 F3 E4 B5 D5` | 1 | 8 | 1427156 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `A1 E2 D3 F3 E4 D5 D6` | 1 | 8 | 1427179 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `A1 E2 D3 C4 E4 D5 C6` | 1 | 8 | 1428873 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `B1 D1 A2 E2 F3 B4 B6` | 1 | 8 | 1929526 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `B1 D1 A2 E2 B4 C5 B6` | 1 | 8 | 1929866 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `B1 D1 A2 E3 F4 E5 B6` | 1 | 8 | 1938995 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `B1 D1 B2 C2 E2 A3 B4` | 1 | 8 | 1943472 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `B1 D1 B2 E2 A3 C3 B4` | 1 | 8 | 1948944 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `B1 D1 B2 E2 A3 D3 E4` | 1 | 8 | 1948967 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `B1 D1 B2 A3 C3 B4 C6` | 1 | 8 | 1953283 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `B1 D1 C2 E2 A3 A5 E5` | 1 | 8 | 1966658 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `B1 D1 C2 E2 A3 E5 D6` | 1 | 8 | 1966697 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `B1 D1 C2 E2 B3 B4 A5` | 1 | 8 | 1966820 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |
| `B1 D1 C2 E2 B3 B5 E5` | 1 | 8 | 1966899 | mono1 domino2 line3 ell3 ell4 ess4 line4 square4 tee4 |

### Fewest solutions among trap-free boards

No piece is confined to a single placement in these.

| pegs | solutions | orbit | rank | pinned pieces |
|---|---:|---:|---:|---|
| `C1 B2 E2 C4 F4 B5 E5` | 2 | 4 | 3644124 | none |
| `B2 C2 E2 D3 C4 B5 E5` | 2 | 8 | 6809147 | none |
| `A1 E2 D3 E4 B5 D5 C6` | 4 | 8 | 1429689 | none |
| `B1 E1 C2 A3 D3 F4 E5` | 4 | 8 | 2140984 | none |
| `B1 C2 E2 B3 D3 B5 E5` | 4 | 8 | 2610416 | none |
| `B1 C3 E3 A4 D4 F4 B5` | 4 | 8 | 2899351 | none |
| `C1 D1 B2 E2 C3 C4 E5` | 4 | 8 | 3057067 | none |
| `C1 B2 D2 E3 D4 B5 E5` | 4 | 8 | 3628819 | none |
| `C1 B2 E2 C3 F3 B5 C6` | 4 | 8 | 3638912 | none |
| `B1 E1 C2 A3 F3 E5 C6` | 5 | 8 | 2141357 | none |
| `B1 C2 E2 D3 C4 B5 E5` | 5 | 8 | 2613571 | none |
| `C1 B2 E2 C3 A4 F4 B5` | 5 | 8 | 3639020 | none |
| `A1 D1 C3 F3 A4 C6 F6` | 6 | 4 | 692875 | none |
| `A1 E1 F3 D4 A5 E5 C6` | 6 | 4 | 877120 | none |
| `B1 D1 F2 A3 E3 F5 D6` | 6 | 8 | 2009526 | none |
| `B1 D1 F2 A3 B5 F5 D6` | 6 | 8 | 2010375 | none |
| `B1 E1 C2 A3 B5 E5 D6` | 6 | 8 | 2142054 | none |
| `B1 E2 C3 F3 E4 B5 D6` | 6 | 8 | 2767221 | none |
| `C1 D1 B2 E2 B5 E5 C6` | 6 | 8 | 3058365 | none |
| `C1 B2 D2 F3 A4 C4 B5` | 6 | 8 | 3629256 | none |

### Fewest solutions with the 1x1 free

Boards where the monomino is *not* pinned, but other pieces may be.

| pegs | solutions | orbit | rank | pinned pieces |
|---|---:|---:|---:|---|
| `B1 D1 E2 A3 D4 B5 E5` | 2 | 8 | 1999470 | domino2 tee4 |
| `C1 B2 E2 C4 F4 B5 E5` | 2 | 4 | 3644124 | none |
| `C1 D2 A3 C3 C4 D5 C6` | 2 | 8 | 3805281 | domino2 ell3 ell4 ess4 line4 square4 |
| `B2 C2 E2 D3 C4 B5 E5` | 2 | 8 | 6809147 | none |
| `B1 D1 E2 A3 C3 B5 C6` | 3 | 8 | 1998508 | domino2 ess4 square4 tee4 |
| `B1 D1 E2 D3 C4 E4 B6` | 3 | 8 | 2003359 | domino2 line4 square4 |
| `C1 B2 E2 C3 D3 F3 B4` | 3 | 8 | 3638467 | domino2 ell3 ell4 ess4 |
| `C1 B2 E2 C3 F3 A4 B5` | 3 | 8 | 3638814 | ell3 tee4 |
| `C1 B2 E2 D3 C4 B5 E5` | 3 | 8 | 3640409 | domino2 line3 ell3 line4 square4 tee4 |
| `C1 D2 B3 F3 A4 C4 D6` | 3 | 8 | 3815220 | line3 ell4 ess4 tee4 |
| `A1 E2 D3 E4 B5 D5 C6` | 4 | 8 | 1429689 | none |
| `B1 D1 E2 D3 E4 A5 F5` | 4 | 8 | 2003549 | domino2 ell3 line4 |
| `B1 E1 C2 A3 D3 F4 E5` | 4 | 8 | 2140984 | none |
| `B1 C2 E2 B3 D3 B5 E5` | 4 | 8 | 2610416 | none |
| `B1 C3 E3 A4 D4 F4 B5` | 4 | 8 | 2899351 | none |
| `C1 D1 B2 E2 C3 C4 E5` | 4 | 8 | 3057067 | none |
| `C1 B2 D2 E3 D4 B5 E5` | 4 | 8 | 3628819 | none |
| `C1 B2 E2 C3 F3 B5 C6` | 4 | 8 | 3638912 | none |
| `C1 B2 E2 D3 A4 F4 E5` | 4 | 8 | 3640163 | square4 |
| `B1 D1 B2 E2 A3 C3 E4` | 5 | 8 | 1948947 | domino2 |

### How much of the low end is explained by traps

| max solutions | boards (orbits) | trapped | trap-free | trap-free % |
|---:|---:|---:|---:|---:|
| 1 | 100 | 100 | 0 | 0.00 |
| 4 | 693 | 684 | 9 | 1.30 |
| 16 | 5186 | 4862 | 324 | 6.25 |
| 64 | 41531 | 34134 | 7397 | 17.81 |
| 256 | 220851 | 134188 | 86663 | 39.24 |

The smallest number of solutions attained by a trap-free board is **2**.

# What makes a Genius Square board hard?

Class studied: **unsolvable boards**.

| quantity | value |
|---|---|
| boards in class | 172440 |
| base rate | 2.065724% |

## Degree 1: where the pegs sit

Enrichment = P(cell is a peg | class) / P(cell is a peg). 1.00 means the class says nothing about that cell.

**Peg enrichment**

|  | A | B | C | D | E | F |
|---|---|---|---|---|---|---|
| **1** | 0.791 | 1.419 | 0.952 | 0.952 | 1.419 | 0.791 |
| **2** | 1.419 | 0.904 | 0.922 | 0.922 | 0.904 | 1.419 |
| **3** | 0.952 | 0.922 | 0.720 | 0.720 | 0.922 | 0.952 |
| **4** | 0.952 | 0.922 | 0.720 | 0.720 | 0.922 | 0.952 |
| **5** | 1.419 | 0.904 | 0.922 | 0.922 | 0.904 | 1.419 |
| **6** | 0.791 | 1.419 | 0.952 | 0.952 | 1.419 | 0.791 |

Largest deviation from D4 symmetry in the heat map: 0.00e+00 (it must be 0 up to rounding, since the class is symmetry-closed).

## How much of hardness is low order?

Exact least-squares fit of the class indicator in the Johnson-scheme
subspaces, over all 8347680 boards. R^2 is the fraction of variance
explained relative to the constant model.

| model | parameters | R^2 |
|---|---:|---:|
| degree 0 (base rate) | 1 | 0.0000 |
| degree <= 1 (per-cell weights) | 36 | 0.0099 |
| degree <= 2 (per-pair weights) | 630 | 0.2224 |
| degree <= 3 (per-triple weights) | 7140 | 0.3466 |

Energy in level 1 alone: 0.0099; in level 2 alone: 0.2126; in level 3 alone: 0.1242; unexplained by degree <= 3: 0.6534.

## Degree 2: co-occurrence beyond the heat map

M[i][j] = P(pegs at i and j | class) minus what the degree-1 heat map
alone would predict. Positive means the two cells conspire.

| rank | cells | excess |
|---:|---|---:|
| +1 | B1 A2 | +0.11208 |
| +2 | E1 F2 | +0.11208 |
| +3 | A5 B6 | +0.11208 |
| +4 | F5 E6 | +0.11208 |
| +5 | A1 B2 | +0.04684 |
| +6 | B5 A6 | +0.04684 |
| +7 | E5 F6 | +0.04684 |
| +8 | F1 E2 | +0.04684 |
| +9 | B4 A5 | +0.04471 |
| +10 | E4 F5 | +0.04471 |
| -5 | F1 F2 | -0.03250 |
| -4 | E1 F1 | -0.03250 |
| -3 | A1 B1 | -0.03250 |
| -2 | F5 F6 | -0.03250 |
| -1 | E6 F6 | -0.03250 |

## Spectral view of the second-order structure

Eigenvectors of M are the peg-placement *modes* whose presence most
distinguishes the class. A mode is a weighting of the 36 cells; boards
whose peg pattern correlates strongly with a high-eigenvalue mode are
over-represented in the class.

| mode | eigenvalue |
|---:|---:|
| 1 | +0.553115 |
| 2 | +0.158171 |
| 3 | +0.158171 |
| 4 | +0.125243 |
| 5 | -0.114732 |
| 6 | -0.113804 |
| 7 | -0.113760 |
| 8 | -0.113760 |

**Mode 1** (eigenvalue +0.553115)

|  | A | B | C | D | E | F |
|---|---|---|---|---|---|---|
| **1** | +0.86 | -1.00 | +0.88 | -0.88 | +1.00 | -0.86 |
| **2** | -1.00 | +0.84 | -0.85 | +0.85 | -0.84 | +1.00 |
| **3** | +0.88 | -0.85 | +0.70 | -0.70 | +0.85 | -0.88 |
| **4** | -0.88 | +0.85 | -0.70 | +0.70 | -0.85 | +0.88 |
| **5** | +1.00 | -0.84 | +0.85 | -0.85 | +0.84 | -1.00 |
| **6** | -0.86 | +1.00 | -0.88 | +0.88 | -1.00 | +0.86 |

**Mode 2** (eigenvalue +0.158171)

|  | A | B | C | D | E | F |
|---|---|---|---|---|---|---|
| **1** | +0.12 | -0.27 | -0.22 | +0.17 | -0.99 | +0.39 |
| **2** | -0.31 | +0.09 | -0.09 | -0.51 | +0.30 | -1.00 |
| **3** | +0.37 | -0.23 | +0.01 | +0.04 | -0.55 | +0.34 |
| **4** | -0.34 | +0.55 | -0.04 | -0.01 | +0.23 | -0.37 |
| **5** | +1.00 | -0.30 | +0.51 | +0.09 | -0.09 | +0.31 |
| **6** | -0.39 | +0.99 | -0.17 | +0.22 | +0.27 | -0.12 |

**Mode 3** (eigenvalue +0.158171)

|  | A | B | C | D | E | F |
|---|---|---|---|---|---|---|
| **1** | +0.39 | -1.00 | +0.34 | -0.37 | +0.31 | -0.12 |
| **2** | -0.99 | +0.30 | -0.55 | +0.23 | -0.09 | +0.27 |
| **3** | +0.17 | -0.51 | +0.04 | -0.01 | +0.09 | +0.22 |
| **4** | -0.22 | -0.09 | +0.01 | -0.04 | +0.51 | -0.17 |
| **5** | -0.27 | +0.09 | -0.23 | +0.55 | -0.30 | +0.99 |
| **6** | +0.12 | -0.31 | +0.37 | -0.34 | +1.00 | -0.39 |

**Mode 4** (eigenvalue +0.125243)

|  | A | B | C | D | E | F |
|---|---|---|---|---|---|---|
| **1** | -0.68 | +1.00 | -0.30 | -0.30 | +1.00 | -0.68 |
| **2** | +1.00 | -0.56 | +0.18 | +0.18 | -0.56 | +1.00 |
| **3** | -0.30 | +0.18 | -0.15 | -0.15 | +0.18 | -0.30 |
| **4** | -0.30 | +0.18 | -0.15 | -0.15 | +0.18 | -0.30 |
| **5** | +1.00 | -0.56 | +0.18 | +0.18 | -0.56 | +1.00 |
| **6** | -0.68 | +1.00 | -0.30 | -0.30 | +1.00 | -0.68 |

## Spatial-frequency spectrum

The same second-order information read in a 2-D DCT-II basis: the mean
power at spatial frequency (u,v) for boards in the class, divided by the
mean power over all boards. Row/column 0 is the DC term.

|  | u=0 | u=1 | u=2 | u=3 | u=4 | u=5 |
|---|---|---|---|---|---|---|
| **v=0** | 1.000 | 1.532 | 0.902 | 0.648 | 0.700 | 0.667 |
| **v=1** | 1.532 | 1.272 | 0.721 | 0.657 | 0.717 | 0.871 |
| **v=2** | 0.902 | 0.721 | 0.690 | 0.696 | 0.920 | 0.907 |
| **v=3** | 0.648 | 0.657 | 0.696 | 0.830 | 1.044 | 1.391 |
| **v=4** | 0.700 | 0.717 | 0.920 | 1.044 | 1.575 | 1.448 |
| **v=5** | 0.667 | 0.871 | 0.907 | 1.391 | 1.448 | 2.992 |

## Minimal blocking patterns

A set of cells is **blocking** if *every* board whose 7 pegs include
those cells is unsolvable -- the pattern is fatal regardless of where
the other pegs go. Blocking is upward closed, so the minimal blocking
sets are the real culprits, and they are computed exactly rather than
inferred statistically.

| pattern size | blocking sets | of which minimal | minimal, up to symmetry |
|---:|---:|---:|---:|
| 1 | 0 | 0 | 0 |
| 2 | 0 | 0 | 0 |
| 3 | 0 | 0 | 0 |
| 4 | 0 | 0 | 0 |
| 5 | 0 | 0 | 0 |
| 6 | 316 | 316 | 42 |

### Smallest fatal patterns (size 6)

`#` marks a peg of the pattern; the remaining 1 pegs may be anywhere.

```
   A B C D E F
1  # . # . # .
2  . # . # . #
3  . . . . . .
4  . . . . . .
5  . . . . . .
6  . . . . . .
```

```
   A B C D E F
1  # . # . # .
2  . # . # . .
3  # . . . . .
4  . . . . . .
5  . . . . . .
6  . . . . . .
```

```
   A B C D E F
1  # . # . # .
2  . # . # . .
3  . . # . . .
4  . . . . . .
5  . . . . . .
6  . . . . . .
```

```
   A B C D E F
1  # . # . # .
2  . # . . . #
3  # . . . . .
4  . . . . . .
5  . . . . . .
6  . . . . . .
```

```
   A B C D E F
1  # . # . # .
2  . . . # . #
3  . . . . # .
4  . . . . . .
5  . . . . . .
6  . . . . . .
```

```
   A B C D E F
1  # . # . . .
2  . # . # . .
3  # . # . . .
4  . . . . . .
5  . . . . . .
6  . . . . . .
```

### Coverage of the unsolvable boards

| pattern size <= k | unsolvable boards containing one | % of class |
|---:|---:|---:|
| 1 | 0 | 0.00 |
| 2 | 0 | 0.00 |
| 3 | 0 | 0.00 |
| 4 | 0 | 0.00 |
| 5 | 0 | 0.00 |
| 6 | 6332 | 3.67 |

A board covered at size k is unsolvable *for a reason that fits in k cells*. The residue -- unsolvable boards covered by nothing smaller than all 7 pegs -- are the genuinely global failures.


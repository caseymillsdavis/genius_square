# What makes a Genius Square board hard?

Class studied: **boards with few solutions**.

(1 to 100 solutions inclusive)

| quantity | value |
|---|---|
| boards in class | 616392 |
| base rate | 7.383992% |

## Degree 1: where the pegs sit

Enrichment = P(cell is a peg | class) / P(cell is a peg). 1.00 means the class says nothing about that cell.

**Peg enrichment**

|  | A | B | C | D | E | F |
|---|---|---|---|---|---|---|
| **1** | 0.398 | 1.151 | 1.017 | 1.017 | 1.151 | 0.398 |
| **2** | 1.151 | 1.366 | 0.918 | 0.918 | 1.366 | 1.151 |
| **3** | 1.017 | 0.918 | 1.064 | 1.064 | 0.918 | 1.017 |
| **4** | 1.017 | 0.918 | 1.064 | 1.064 | 0.918 | 1.017 |
| **5** | 1.151 | 1.366 | 0.918 | 0.918 | 1.366 | 1.151 |
| **6** | 0.398 | 1.151 | 1.017 | 1.017 | 1.151 | 0.398 |

Largest deviation from D4 symmetry in the heat map: 0.00e+00 (it must be 0 up to rounding, since the class is symmetry-closed).

## How much of hardness is low order?

Exact least-squares fit of the class indicator in the Johnson-scheme
subspaces, over all 8347680 boards. R^2 is the fraction of variance
explained relative to the constant model.

| model | parameters | R^2 |
|---|---:|---:|
| degree 0 (base rate) | 1 | 0.0000 |
| degree <= 1 (per-cell weights) | 36 | 0.0419 |
| degree <= 2 (per-pair weights) | 630 | 0.2448 |
| degree <= 3 (per-triple weights) | 7140 | 0.3646 |

Energy in level 1 alone: 0.0419; in level 2 alone: 0.2029; in level 3 alone: 0.1198; unexplained by degree <= 3: 0.6354.

## Degree 2: co-occurrence beyond the heat map

M[i][j] = P(pegs at i and j | class) minus what the degree-1 heat map
alone would predict. Positive means the two cells conspire.

| rank | cells | excess |
|---:|---|---:|
| +1 | B1 A2 | +0.04533 |
| +2 | E1 F2 | +0.04533 |
| +3 | A5 B6 | +0.04533 |
| +4 | F5 E6 | +0.04533 |
| +5 | B2 A3 | +0.03047 |
| +6 | E2 F3 | +0.03047 |
| +7 | A4 B5 | +0.03047 |
| +8 | F4 E5 | +0.03047 |
| +9 | D1 E2 | +0.03047 |
| +10 | B5 C6 | +0.03047 |
| -5 | A4 A5 | -0.02693 |
| -4 | B1 C1 | -0.02693 |
| -3 | D6 E6 | -0.02693 |
| -2 | B6 C6 | -0.02693 |
| -1 | F2 F3 | -0.02693 |

## Spectral view of the second-order structure

Eigenvectors of M are the peg-placement *modes* whose presence most
distinguishes the class. A mode is a weighting of the 36 cells; boards
whose peg pattern correlates strongly with a high-eigenvalue mode are
over-represented in the class.

| mode | eigenvalue |
|---:|---:|
| 1 | +0.147648 |
| 2 | +0.147648 |
| 3 | +0.105516 |
| 4 | +0.100898 |
| 5 | +0.088485 |
| 6 | -0.071908 |
| 7 | -0.071908 |
| 8 | -0.070816 |

**Mode 1** (eigenvalue +0.147648)

|  | A | B | C | D | E | F |
|---|---|---|---|---|---|---|
| **1** | +0.33 | -0.78 | +0.88 | -0.93 | +0.88 | -0.40 |
| **2** | -0.56 | +0.84 | -0.88 | +0.91 | -1.00 | +0.70 |
| **3** | +0.18 | -0.09 | +0.35 | -0.41 | +0.24 | -0.33 |
| **4** | +0.33 | -0.24 | +0.41 | -0.35 | +0.09 | -0.18 |
| **5** | -0.70 | +1.00 | -0.91 | +0.88 | -0.84 | +0.56 |
| **6** | +0.40 | -0.88 | +0.93 | -0.88 | +0.78 | -0.33 |

**Mode 2** (eigenvalue +0.147648)

|  | A | B | C | D | E | F |
|---|---|---|---|---|---|---|
| **1** | -0.40 | +0.70 | -0.33 | -0.18 | +0.56 | -0.33 |
| **2** | +0.88 | -1.00 | +0.24 | +0.09 | -0.84 | +0.78 |
| **3** | -0.93 | +0.91 | -0.41 | -0.35 | +0.88 | -0.88 |
| **4** | +0.88 | -0.88 | +0.35 | +0.41 | -0.91 | +0.93 |
| **5** | -0.78 | +0.84 | -0.09 | -0.24 | +1.00 | -0.88 |
| **6** | +0.33 | -0.56 | +0.18 | +0.33 | -0.70 | +0.40 |

**Mode 3** (eigenvalue +0.105516)

|  | A | B | C | D | E | F |
|---|---|---|---|---|---|---|
| **1** | +0.45 | -0.83 | +0.39 | +0.39 | -0.83 | +0.45 |
| **2** | -0.83 | +1.00 | -0.33 | -0.33 | +1.00 | -0.83 |
| **3** | +0.39 | -0.33 | +0.12 | +0.12 | -0.33 | +0.39 |
| **4** | +0.39 | -0.33 | +0.12 | +0.12 | -0.33 | +0.39 |
| **5** | -0.83 | +1.00 | -0.33 | -0.33 | +1.00 | -0.83 |
| **6** | +0.45 | -0.83 | +0.39 | +0.39 | -0.83 | +0.45 |

**Mode 4** (eigenvalue +0.100898)

|  | A | B | C | D | E | F |
|---|---|---|---|---|---|---|
| **1** | -0.00 | +0.44 | -1.00 | +1.00 | -0.44 | +0.00 |
| **2** | -0.44 | +0.00 | +0.68 | -0.68 | +0.00 | +0.44 |
| **3** | +1.00 | -0.68 | +0.00 | -0.00 | +0.68 | -1.00 |
| **4** | -1.00 | +0.68 | -0.00 | +0.00 | -0.68 | +1.00 |
| **5** | +0.44 | +0.00 | -0.68 | +0.68 | +0.00 | -0.44 |
| **6** | +0.00 | -0.44 | +1.00 | -1.00 | +0.44 | -0.00 |

## Spatial-frequency spectrum

The same second-order information read in a 2-D DCT-II basis: the mean
power at spatial frequency (u,v) for boards in the class, divided by the
mean power over all boards. Row/column 0 is the DC term.

|  | u=0 | u=1 | u=2 | u=3 | u=4 | u=5 |
|---|---|---|---|---|---|---|
| **v=0** | 1.000 | 0.978 | 0.848 | 0.735 | 0.830 | 0.784 |
| **v=1** | 0.978 | 0.854 | 0.759 | 0.680 | 0.758 | 0.862 |
| **v=2** | 0.848 | 0.759 | 0.813 | 0.708 | 0.991 | 1.152 |
| **v=3** | 0.735 | 0.680 | 0.708 | 0.887 | 1.241 | 1.485 |
| **v=4** | 0.830 | 0.758 | 0.991 | 1.241 | 1.691 | 1.823 |
| **v=5** | 0.784 | 0.862 | 1.152 | 1.485 | 1.823 | 1.488 |


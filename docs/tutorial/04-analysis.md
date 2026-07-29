# Part 4: The analysis -- what makes a board hard, said precisely

With `counts[B]` known for every board, "study the unsolvable boards" stops
being a computation problem and becomes a *language* problem: what kind of
statement about 172,440 bad boards is actually informative? This part builds
the three languages we used -- variance decomposition by interaction order,
spectral summaries, and exact blocking patterns -- and shows why they are
three views of a coherent whole.

Throughout, fix the **class** of interest, e.g. the unsolvable boards, and
let `f` be its indicator: `f(B) = 1` if B is in the class. `f` is a function
on the C(36,7) = 8,347,680 seven-element subsets of the 36 cells. The class
size is `U` (172,440), the universe `N` (8,347,680), the base rate
`p = U/N` (2.07%).

## 4.1 Two intuitive proposals, formalized

**The heat map.** "Where do pegs sit on unsolvable boards?" is the vector

```
h(i) = P(cell i is a peg | B in class) / P(cell i is a peg)
```

with `P(cell i is a peg) = 7/36` uniformly. Estimating nothing -- we have
the full population -- this is computed exactly from the **first moments**
`m1[i] = #{B in class : i in B}` in one sweep.

**The spectral idea.** Treat each board as a 6x6 binary image `b`, expand it
in some orthonormal basis (a 2-D DCT, say), and average the squared
coefficients over the class. Because `E[F_u^2] = phi_u^T E[b b^T] phi_u`,
that average is a *linear read-out of the second moments*
`m2[i][j] = #{B in class : i,j both in B}` -- nothing more. Any fixed-basis
"power spectrum" sees exactly the pair statistics, presented differently.

So both proposals are projections of `f` onto low-order statistics. That
raises the real question: *how much of `f` do low-order statistics
determine?* To answer it we need the right notion of "degree" for functions
on fixed-size subsets.

## 4.2 Why not the Boolean Fourier transform

For functions on the hypercube {0,1}^36 the standard tool is the
Walsh-Fourier expansion in parity characters, with "degree" = size of the
largest character. But our `f` lives on the **slice** of the hypercube where
exactly 7 coordinates are 1. On the slice the parities are no longer
orthogonal (e.g. the sum of all 36 coordinates is the constant 7), and naive
hypercube degrees misbehave. The correct decomposition for the slice is
classical, and goes by the name of the **Johnson scheme** J(36,7).

## 4.3 The Johnson scheme in working terms

Let `X` = the space of real functions on 7-subsets (dimension 8,347,680).
For each k, define the subspace spanned by the *inclusion indicators* of
k-cell patterns:

```
W_k = span{ phi_S : S a k-subset },   phi_S(B) = 1 if S ⊆ B else 0.
```

`W_0 ⊂ W_1 ⊂ ... ⊂ W_7 = X` (a k-pattern indicator is a scaled sum of
(k+1)-pattern indicators: summing `phi_T` over the (k+1)-supersets T of S
inside B counts `7-k` copies of `phi_S`). Under the symmetric group S36
acting on cells, X splits into orthogonal irreducible pieces

```
X = V_0 ⊕ V_1 ⊕ ... ⊕ V_7,      W_k = V_0 ⊕ ... ⊕ V_k,
dim V_k = C(36,k) - C(36,k-1).
```

`V_k` is the "pure degree k" part -- what k-cell patterns can express that
(k-1)-cell patterns cannot. Dimensions here: 1, 35, 594, 6510, ... so
`dim W_1 = 36`, `dim W_2 = 630`, `dim W_3 = 7140` -- exactly C(36,k),
because the inclusion indicators are linearly independent (Gottlieb's
theorem: the k-subset x 7-subset inclusion matrix has full rank C(36,k)
whenever k <= min(7, 29)).

**A miniature you can verify by hand.** Take J(4,2): six 2-subsets of
{1,2,3,4}. Dimensions: V_0 = 1 (constants), V_1 = C(4,1)-C(4,0) = 3,
V_2 = C(4,2)-C(4,1) = 2, total 6. A function in W_1 is a sum of vertex
weights: f({i,j}) = a_i + a_j -- "degree 1" in the honest sense. The
2-dimensional V_2 is spanned by contrasts of perfect matchings, e.g.
g = phi_{12} + phi_{34} - phi_{13} - phi_{24}: try writing g as vertex
weights and you will find the 4 equations force a contradiction -- it is
irreducibly pairwise. Our question "is unsolvability a per-cell story?" is
precisely "how much of f lies in W_1?" scaled up.

## 4.4 Exact least squares with a closed-form Gram matrix

Project `f` orthogonally onto `W_k` and report explained variance. Because
we possess `f` on the *entire* population, this is linear algebra, not
statistics -- no sampling error, no regularization, no train/test split.

Write the projection as `sum over k-subsets S of alpha_S phi_S`. The normal
equations are `G alpha = m` where

```
G[S,T] = <phi_S, phi_T> = #{B : S ∪ T ⊆ B} = C(36-u, 7-u),  u = |S ∪ T|
m[S]   = <phi_S, f>     = #{B in class : S ⊆ B}             (the k-th moments)
```

The Gram matrix needs no enumeration -- a board contains S ∪ T iff its other
`7-u` pegs sit anywhere in the remaining `36-u` cells. For k = 3 the values
are, by |S ∩ T| = 6-u:

| shared cells | u | G entry |
|---:|---:|---:|
| 3 (S = T) | 3 | C(33,4) = 40,920 |
| 2 | 4 | C(32,3) = 4,960 |
| 1 | 5 | C(31,2) = 465 |
| 0 | 6 | C(30,1) = 30 |

Moments come from one sweep: for each class board, bump all C(7,k) of its
k-subsets (35 triples per board -- 6M increments for the whole degree-3
pass). Solve by Cholesky: G is positive definite because the phi_S are
independent (Gottlieb again). Sizes: 36x36, 630x630, 7140x7140; the last is
0.4 GB and ~1.2e11 flops, 95 s single-core -- the largest *exact* linear
algebra in the project.

The explained variance drops out of inner products alone. With
`||f||^2 = U` (f is 0/1), mean `U/N`, and `<f, proj f> = alpha^T m`
(since `<proj f, proj f> = alpha^T G alpha = alpha^T m`):

```
R^2(k) = (alpha^T m - U^2/N) / (U - U^2/N)
```

Two structural checks cost nothing and catch real bugs: `R^2` must be
non-decreasing in k (nested subspaces), and every projection must be
D4-invariant because `f` is (the report prints the largest deviation of the
heat map from D4 symmetry -- it must be 0.00e+00, and is). Symmetry also
deflates the honest parameter count: D4-invariant functions in W_1 have 6
free parameters (cell orbits), in W_2 93 (pair orbits, another Burnside
exercise), not 36 and 630.

### What the ladder says

| model | dim | R^2, unsolvable | R^2, 1..100 solutions |
|---|---:|---:|---:|
| degree <= 1 | 36 | 0.0099 | 0.0419 |
| degree <= 2 | 630 | 0.2224 | 0.2448 |
| degree <= 3 | 7140 | 0.3466 | 0.3646 |

Read it as a spectrum of energies per level: 1.0%, 21.3%, 12.4%, and **65%
at degree >= 4**. The punchline: the heat map -- the thing one naturally
plots first -- carries one percent of the signal. Pair effects are the
largest identifiable component; beyond pairs the increments *shrink*
(level 3 < level 2), yet nearly two-thirds of unsolvability is irreducibly
high-order. Genius Square boards do not fail for per-cell reasons; they fail
through configurations.

## 4.5 The second-order structure, spectrally

Since level 2 is the biggest nameable component, look at it properly. Form
the **excess co-occurrence matrix**

```
M[i][j] = P(i,j both pegs | class) - P(i|class) P(j|class) * (p2 / p1^2)
```

where `p1 = 7/36`, `p2 = (7*6)/(36*35)`. The correction factor
`p2/p1^2 = 0.882` is easy to forget and poisons everything if you do:
placing 7 pegs *without replacement* makes any two cells negatively
correlated already in the full population (a peg here is a peg not there),
and we want M to be zero for a structureless class, not to rediscover
sampling-without-replacement. M is the deviation of the class's pair
distribution from what its own heat map predicts.

Now the "spectral study" has a principled form: **diagonalize M**. Its
eigenvectors are cell-weightings ("modes") and eigenvalues say how strongly
the class's peg patterns align with each mode beyond first-order prediction.
A 36x36 symmetric eigenproblem is solved to machine precision by Jacobi
rotations (repeatedly zeroing the largest off-diagonal entry with a 2x2
rotation -- 40 lines, no library).

Results for the unsolvable class, and their geometric reading:

* **Top pairs** (+0.112, four of them by symmetry): {B1, A2} and its
  images -- the two cells diagonally flanking a corner. Pegs there seal the
  corner cell into a 1-cell pocket, forcing the monomino; a second such
  event anywhere is fatal. The strongest *negative* pairs ({A1, B1},
  {F1, F2}, {E6, F6}, ... at -0.033) are a corner plus its edge neighbor:
  occupying the corner itself defuses the corner-sealing mechanism, and
  adjacent pegs waste blocking power on each other.
* **Mode 1** (eigenvalue +0.553, 3.5x the runner-up) is a full-board
  **checkerboard**. A checkerboard of pegs is maximally isolating -- each
  peg cuts connectivity in the empty complement. The next modes are the
  same idea localized to corners/edges.
* The **DCT power ratios** (class power / population power at each 2-D
  spatial frequency) tell the same story in a familiar basis: the (5,5)
  highest-frequency bin -- the checkerboard bin -- is enhanced 2.99x, the
  low-frequency (1,0)/(0,1) bins 1.53x, mid-frequencies suppressed. Note
  the (0,0) entry is exactly 1.000: every board has exactly 7 pegs, so DC
  carries no class information -- a built-in sanity check that the
  machinery is honest.

Why the eigen-view rather than *only* the DCT: the DCT basis is fixed in
advance, so class structure smears across bins (a corner-sealing motif is
not a pure frequency); the eigenbasis is the one the data itself
diagonalizes. The DCT's virtue is familiarity, and that its bins have names
("high frequency"). Use both, trust the eigenmodes.

## 4.6 Blocking patterns: from correlation to mechanism

Everything above is statistics *about* the class. The sharpest results in
the study are not statistical at all.

Call a cell-set `S` **blocking** if *every* board whose pegs include S is
unsolvable -- S is fatal no matter where the remaining `7-|S|` pegs go.
Blocking sets are **upward closed** (more pegs never help), so the
informative objects are the *minimal* ones: remove any cell and some
completion becomes solvable. This is a monotone Boolean function on the
subset lattice, and with the full count table it is computable **exactly**,
top-down:

```
A_7(S) = [ counts[S] == 0 ]                        (S a 7-set: look it up)
A_k(S) = AND over all cells c not in S of A_{k+1}(S + c)
```

Induction gives: `A_k(S) = 1` iff every 7-superset of S is unsolvable,
which is the definition of blocking. The lattice is small -- levels have
C(36,6) = 1.9M, C(36,5) = 377k, ... entries, one byte each, and each entry
ANDs over at most 30 children: ~60M array lookups, seconds. Minimality of a
blocking S is then `A(S) && no (|S|-1)-subset of S is blocking`; counting
up to symmetry reuses `gs_canon` from part 3.

Findings (`docs/ANALYSIS_UNSOLVABLE.md`):

* **No blocking set of size <= 5 exists.** Any 5 pegs whatsoever leave a
  completable position -- a fact about the game with real content, and
  invisible to any statistical summary.
* **316 blocking 6-sets (42 up to symmetry), all minimal.** The rendered
  examples are two-row checkerboard-like combs, e.g. pegs A1 C1 E1 B2 D2 F2:
  each of B1, D1, F1 becomes an isolated cell, three pockets demanding three
  monominoes when the game has one. Note how this *derives* the checkerboard
  eigenmode of 4.5 rather than merely echoing it.
* **Coverage: only 3.67%** of unsolvable boards contain any blocking
  6-pattern. The other 96%: every 6-subset of their pegs has some solvable
  completion -- the board fails only through the joint action of all seven
  pegs. This is the mechanistic twin of the Fourier fact "65% of the
  variance is degree >= 4," reached by a completely different route, and it
  is the study's bottom line:

> The heat map is a summary. The pair effects are the strongest nameable
> mechanism. But most unsolvable boards are seven-peg conspiracies with no
> compact local cause.

## 4.7 Judging *hard* (not impossible) boards: the trap subtlety

The same machinery runs on the near-miss class (`--hard T`), but ranking
"hardest solvable boards" raises one game-specific trap. The raw
fewest-solutions list is dominated by boards where the pegs **pin** a piece
-- some piece occupies the same cells in *every* solution (the archetype: an
isolated empty cell nails the 1x1). Those are easy for a human (the pinned
piece is free information) and were excluded on request.

The clean formalization: enumerate the board's full solution set
(`gs_solve_freedom`) and count, per piece, its distinct placements across
all solutions. A piece with exactly one is pinned; a board with any pinned
piece is *trapped*. Two pleasant exact facts fell out: every
unique-solution board is trivially trapped (all nine pieces pinned -- 800
such boards), and the minimum solution count among trap-free boards is
**2**, achieved by `C1 B2 E2 C4 F4 B5 E5` -- whose two solutions share not a
single placement: every one of the nine pieces moves. As a puzzle, that
board is arguably the game's hardest fair position.

## 4.8 Where the mathematics could go next

* **Degree 4 head-on** is a C(36,4) = 58,905-dimensional Gram system --
  27.8 GB dense, out of reach as written. The structured route: G_k =
  W W^T for the inclusion matrix W, and multiplying by W or W^T is a
  *lattice zeta transform* computable level-by-level without materializing
  G. Conjugate gradients with that matvec would fit in memory; the
  eigenvalues of G_k on each V_i are even known in closed form, giving a
  preconditioner.
* **Exact level energies without nesting**: the projections onto each V_k
  can be written via the scheme's primitive idempotents; the R^2
  differences we report are exactly those energies, but the idempotent
  route would compute any single level in isolation.
* **Explaining the 65%**: the residual is concentrated (by 4.6) on boards
  with no small fatal pattern. A dictionary of *conditional* blocking
  patterns -- "S is fatal given a peg count in a region" -- is the natural
  refinement, and the lattice sweep generalizes.

---

**Check your understanding**

1. Why is `sum_i x_i = 7` fatal to using hypercube parities as an
   orthogonal basis on the slice?
2. Derive `G[S,T] = C(36-u, 7-u)` in one sentence.
3. The DC bin of the DCT table must equal 1.000 for *any* class. Why does
   the same argument not force the (1,0) bin to 1?
4. Show that "S blocking and |S| = 6" implies every superset board is
   counted in the coverage row for k = 6 -- and why coverage can still be
   only 3.67%.
5. (Harder) In J(4,2), verify dim V_2 = 2 by exhibiting two independent
   matching-contrasts and showing any function orthogonal to W_1 is a
   combination of them.

*Previous parts:* [1. Boards, bits, and ranks](01-boards-bits-and-ranks.md) ·
[2. Search](02-search.md) · [3. Symmetry](03-symmetry.md)

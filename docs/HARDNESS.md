# What makes a board hard? Notes on studying the unsolvable boards

This is the design document for the stretch goal: given the solution count of
every one of the 8,347,680 boards, can we say something structural about *why*
some peg configurations are fatal?

The numerical output lives in `ANALYSIS_UNSOLVABLE.md` (and
`ANALYSIS_HARD.md` for the near-miss boards). This file is about the framing,
including the parts I think are dead ends and the parts I think are the real
prize.

---

## 1. The two proposals, and what they actually are

The question came with two suggestions. Both are good instincts, and it is
worth noting up front that **they are the same object viewed in two bases.**

> *"Sum across all of the boards where peg placements yield some non-zero value
> and generate a heat map of where pegs tend to be on unsolvable boards."*

Write `f(B) = 1` if board `B` is unsolvable. The heat map is

```
    h(i) = P(cell i holds a peg | f(B) = 1)
```

which is, up to normalisation, the projection of `f` onto the span of the
single-cell indicator functions. In the language of section 2 this is exactly
the **degree-1 part** of `f`. Nothing is lost by saying it that way, and a lot
is gained: it tells you immediately what the *next* thing to compute is
(degree 2, degree 3), and it tells you how to measure whether the heat map is
any good (how much of the variance of `f` it explains).

> *"Do some sort of spectral study -- compute some kind of Fourier transform on
> the board to get its spectra and summarize these spectra across unsolvable
> boards."*

Take a board's peg pattern as a 6x6 binary image `b`, transform it with any
fixed linear basis (2-D DCT, Walsh-Hadamard, whatever) to get coefficients
`F_u = <b, phi_u>`, and average the power over the unsolvable class:

```
    E[ |F_u|^2 | unsolvable ] = sum over i,j of phi_u(i) phi_u(j) E[ b_i b_j | unsolvable ]
```

The right-hand side depends on the class **only through the matrix of pair
correlations** `E[b_i b_j | unsolvable]`. So the averaged spectrum is a linear
functional of the degree-2 statistics -- it cannot see anything a
pair-correlation analysis cannot, and it can see less, because averaging power
throws away the cross terms.

That is not a reason to skip it; it is a reason to do it *properly*. If you are
going to look at the second-order structure in some basis, use the basis that
diagonalises it rather than a basis chosen in advance. That means
eigen-decomposing the excess pair-correlation matrix -- a PCA on the peg
indicator, restricted to the class. The DCT then becomes a sanity check and a
familiar way to present the result, not the analysis itself.

Both are implemented in `gs_analyze`.

---

## 2. The right harmonic analysis: the Johnson scheme

A board is a 7-element subset of a 36-element set. Functions on such subsets do
not live on a hypercube, so the Boolean Fourier-Walsh transform is not the
natural tool -- the boards all have exactly 7 pegs, so the usual `{-1,+1}^36`
characters are not orthogonal on the slice we care about.

The correct object is the **Johnson scheme `J(36,7)`**. The space of real
functions on 7-subsets decomposes orthogonally under the symmetric group as

```
    R^{C(36,7)}  =  V_0 + V_1 + V_2 + ... + V_7,
    dim V_k = C(36,k) - C(36,k-1)
```

`V_k` is the "pure degree k" part. Equivalently, define

```
    W_k = span{ 1[S subset of B] : |S| = k }   so   W_k = V_0 + ... + V_k
```

`W_k` is precisely the set of functions expressible as a sum of weights
attached to `k`-cell patterns. So:

* `W_0` = the base rate.
* `W_1` = the heat map. "Unsolvability is a sum of per-cell effects."
* `W_2` = per-pair effects. "Certain pairs of cells conspire."
* the residual `f - proj_{W_k} f` = what is irreducibly higher-order.

**This gives the heat map an honest error bar.** Fitting `f` in `W_1` and
reporting `R^2` answers "how much of unsolvability is a per-cell story?" -- and
if the answer is small, the heat map is pretty but not explanatory.

### Computing it exactly

No sampling is needed. The Gram matrix of the spanning set has a closed form:

```
    <1[S subset .], 1[S' subset .]>  =  #{B : B contains S union S'}
                                     =  C(36 - u, 7 - u),   u = |S union S'|
```

and the right-hand side of the normal equations is the moment

```
    m_k(S) = #{unsolvable boards B : B contains S}
```

obtained in a single pass over the class (for each board, bump all `C(7,k)` of
its `k`-subsets). The systems are tiny -- 36x36 for degree 1, 630x630 for
degree 2 -- and solved by Cholesky. Degree 3 is 7140x7140, still feasible
(~400 MB, a few minutes) if it ever looks worth it.

`R^2` is then `(a^T m - U^2/N) / (U - U^2/N)` where `U` is the class size and
`N = 8347680`.

### A free consequence of symmetry

`f` is D4-invariant, so its projections are too. The degree-1 coefficients must
therefore be constant on the six D4-orbits of cells, and the degree-2
coefficients constant on the orbits of pairs. This is a strong, free
correctness check -- `gs_analyze` reports the largest deviation of the heat map
from D4 symmetry, which must be zero up to rounding. It also means the honest
parameter counts are 6 and (number of pair orbits), not 36 and 630.

---

## 3. What I think is the more interesting question

Variance decompositions tell you *how much* structure there is at each order.
They do not tell you *what the structure is*. For that I think the better tool
is combinatorial rather than statistical.

### Minimal blocking patterns

Call a set `S` of cells **blocking** if every board whose pegs include `S` is
unsolvable -- the pattern is fatal no matter where the other pegs land.
Blocking is upward closed, so the objects of interest are the **minimal**
blocking sets.

These are computable exactly, top down, with no search at all:

```
    A_7(B) = [B is unsolvable]
    A_k(S) = AND over cells c not in S of A_{k+1}(S + {c})
```

The level sizes are `C(36,6) = 1.9M`, `C(36,5) = 377k`, `C(36,4) = 59k`, ... so
the whole lattice is a few seconds of work and a couple of megabytes.

This is what I would actually want from the analysis, because it produces
*statements*, not correlations: "these particular 4-cell configurations kill the
board outright". Rendered as little grids they are directly checkable by eye,
and they are the honest answer to "what makes a board hard".

The complementary number is **coverage**: what fraction of unsolvable boards
contain a blocking pattern of size <= k. Boards that are covered fail for a
local, nameable reason. Boards covered by nothing smaller than all seven pegs
fail *globally* -- no proper sub-pattern is fatal, the seven pegs only kill the
board in concert. Those are the genuinely interesting ones and I would expect
them to be where any remaining higher-order Fourier energy is concentrated.

Both are implemented.

### Structural invariants worth correlating against

Cheap, search-free properties of the empty region that plausibly explain
hardness, all computable in microseconds:

* **Connected components.** The region must split into pieces whose sizes are
  sub-multiset sums of `{1,2,3,3,4,4,4,4,4}`. A component of size 5 with only
  4-cell pieces left is instantly dead. This is already the search's main
  pruning rule; as a *statistic* it identifies boards that are unsolvable for
  purely arithmetic reasons.
* **Isolated cells.** An empty cell with four blocked neighbours forces the
  monomino. Two such cells is an immediate contradiction -- and that alone
  should account for a visible slice of the unsolvable set.
* **Boundary structure.** Pegs on the edge remove fewer degrees of freedom than
  pegs in the interior; the heat map should show this and it is a good
  smell test for the whole pipeline.

### Ideas I considered and did not pursue

* **Colouring / parity arguments.** The classical tool for polyomino packing
  impossibility. They are weak here precisely because the piece set contains a
  monomino and a domino, which defeat most colourings. Worth a paragraph, not a
  program.
* **Transfer matrix over column profiles.** An exact alternative to enumeration
  (sweep column by column, state = boundary profile plus which pieces are used).
  It would give the total packing count much faster than brute force, but it
  does *not* naturally give per-board counts, which is what we actually want.
* **Learned models.** A decision tree or logistic regression over the
  structural features above would predict unsolvability well and explain
  nothing that the blocking patterns do not explain better and exactly.

---

## 4. Summary of the plan

| question | method | status |
|---|---|---|
| Where do pegs sit on unsolvable boards? | degree-1 marginals, D4-checked | implemented |
| Is the heat map an *explanation*? | `R^2` of the `W_1` least-squares fit | implemented |
| What pairs conspire? | degree-2 fit, excess co-occurrence matrix | implemented |
| The "spectral study" | eigenmodes of that matrix; DCT power ratios | implemented |
| Which configurations are actually fatal? | minimal blocking sets, exact | implemented |
| Which boards fail for no local reason? | coverage residue | implemented |
| Pure degree-3 energy | 7140x7140 Cholesky | not done; feasible |

The headline claim I would want to defend at the end: *the heat map is a
summary, the blocking patterns are the explanation, and the interesting boards
are the ones the blocking patterns miss.*

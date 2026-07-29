/* gs_analyze -- what makes a board hard?
 *
 * Treat the property of interest (unsolvable, or "fewer than T solutions") as
 * a boolean function f on the C(36,7) boards and ask how much of it is
 * explained by low-order structure:
 *
 *   degree 0   a constant                       (the base rate)
 *   degree 1   a sum of per-cell weights        (the heat map)
 *   degree 2   plus per-pair weights            (co-occurrence)
 *
 * This is the harmonic analysis of the Johnson scheme J(36,7): the space of
 * functions on 7-subsets splits into orthogonal levels V_0..V_7, and W_k =
 * V_0+..+V_k is exactly the span of the indicators {B : B contains S} over
 * k-subsets S.  Least-squares fitting f in W_k and reporting the explained
 * variance therefore measures precisely "how much of hardness is a k-cell
 * effect".  The Gram matrix of that basis is
 *
 *      <1_{S subset .}, 1_{S' subset .}> = C(36-u, 7-u),  u = |S union S'|,
 *
 * so the normal equations are tiny (36x36 and 630x630) and exact.
 *
 * The second-order part is then re-expressed spectrally: the eigenvectors of
 * the excess co-occurrence matrix are the "modes" of peg placement that
 * distinguish hard boards, and the same matrix read in a DCT basis gives the
 * spatial-frequency spectrum.  See docs/HARDNESS.md.
 */
#include "gs_core.h"
#include "gs_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define NPAIR 630                        /* C(36,2) */
#define NTRIP 7140                       /* C(36,3) */

static int    pair_id[GS_CELLS][GS_CELLS];
static int    pair_i[NPAIR], pair_j[NPAIR];
static gs_mask tri_mask[NTRIP];          /* triple, colex order */

/* <1_S, 1_S'> = number of boards containing S union S' */
static const double gram_u[8] = {
    8347680.0, 1623160.0, 278256.0, 40920.0, 4960.0, 465.0, 30.0, 1.0
};

/* colex rank of a triple a < b < c: C(a,1) + C(b,2) + C(c,3) */
static inline int trip_rank(int a, int b, int c)
{
    return (int)(gs_binom[a][1] + gs_binom[b][2] + gs_binom[c][3]);
}

/* ------------------------------------------------------------------ */
/* dense symmetric solve (Cholesky) and Jacobi eigenvalues             */
/* ------------------------------------------------------------------ */
static int cholesky_solve(double *A, double *b, int n)
{
    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            double s = A[i * n + j];
            for (int k = 0; k < j; k++) s -= A[i * n + k] * A[j * n + k];
            if (i == j) {
                if (s <= 0) return -1;
                A[i * n + i] = sqrt(s);
            } else
                A[i * n + j] = s / A[j * n + j];
        }
    }
    for (int i = 0; i < n; i++) {                 /* forward */
        double s = b[i];
        for (int k = 0; k < i; k++) s -= A[i * n + k] * b[k];
        b[i] = s / A[i * n + i];
    }
    for (int i = n - 1; i >= 0; i--) {            /* back */
        double s = b[i];
        for (int k = i + 1; k < n; k++) s -= A[k * n + i] * b[k];
        b[i] = s / A[i * n + i];
    }
    return 0;
}

/* Jacobi eigen-decomposition of a symmetric n x n matrix (n small). */
static void jacobi(double *a, int n, double *eval, double *evec)
{
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) evec[i * n + j] = (i == j);
    for (int sweep = 0; sweep < 100; sweep++) {
        double off = 0;
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++) off += a[i * n + j] * a[i * n + j];
        if (off < 1e-24) break;
        for (int p = 0; p < n; p++)
            for (int q = p + 1; q < n; q++) {
                double apq = a[p * n + q];
                if (fabs(apq) < 1e-18) continue;
                double theta = (a[q * n + q] - a[p * n + p]) / (2 * apq);
                double t = (theta >= 0 ? 1.0 : -1.0)
                         / (fabs(theta) + sqrt(theta * theta + 1));
                double c = 1 / sqrt(t * t + 1), s = t * c;
                for (int k = 0; k < n; k++) {
                    double akp = a[k * n + p], akq = a[k * n + q];
                    a[k * n + p] = c * akp - s * akq;
                    a[k * n + q] = s * akp + c * akq;
                }
                for (int k = 0; k < n; k++) {
                    double apk = a[p * n + k], aqk = a[q * n + k];
                    a[p * n + k] = c * apk - s * aqk;
                    a[q * n + k] = s * apk + c * aqk;
                }
                for (int k = 0; k < n; k++) {
                    double vkp = evec[k * n + p], vkq = evec[k * n + q];
                    evec[k * n + p] = c * vkp - s * vkq;
                    evec[k * n + q] = s * vkp + c * vkq;
                }
            }
    }
    for (int i = 0; i < n; i++) eval[i] = a[i * n + i];
}

/* ------------------------------------------------------------------ */
/* blocking patterns                                                    */
/* ------------------------------------------------------------------ */
/* A set S of cells is *blocking* when every board whose pegs include S is
 * unsolvable -- the pattern kills the puzzle no matter where the remaining
 * pegs land.  Blocking is monotone (a superset of a blocking set blocks), so
 * the informative objects are the minimal ones.  These are computed exactly,
 * top down:  A_k(S) = AND over cells c not in S of A_{k+1}(S + c), seeded by
 * A_7 = "this board is unsolvable".  Unlike a heat map this is a *mechanistic*
 * explanation: it names the actual configurations that do the damage. */

static uint32_t colex_rank(const int *c, int k)
{
    uint32_t r = 0;
    for (int i = 0; i < k; i++) r += gs_binom[c[i]][i + 1];
    return r;
}

static uint32_t nsub[GS_CELLS + 1];      /* C(36,k) */

static uint8_t *blocking_level(const uint8_t *up, int k, const uint32_t *counts)
{
    /* up == NULL and k == 6 means "derive from the board table directly" */
    uint8_t *A = calloc(nsub[k], 1);
    if (!A) return NULL;
    int c[GS_CELLS], ext[GS_CELLS];
    for (int i = 0; i < k; i++) c[i] = i;
    for (;;) {
        int all = 1;
        for (int x = 0; x < GS_CELLS && all; x++) {
            int in = 0;
            for (int i = 0; i < k; i++) if (c[i] == x) { in = 1; break; }
            if (in) continue;
            int m = 0, placed = 0;
            for (int i = 0; i < k; i++) {
                if (!placed && c[i] > x) { ext[m++] = x; placed = 1; }
                ext[m++] = c[i];
            }
            if (!placed) ext[m++] = x;
            if (up) all = up[colex_rank(ext, k + 1)];
            else {
                gs_mask msk = 0;
                for (int i = 0; i < m; i++) msk |= (gs_mask)1 << ext[i];
                all = (counts[gs_rank(msk)] == 0);
            }
        }
        A[colex_rank(c, k)] = (uint8_t)all;

        int i = k - 1;
        while (i >= 0 && c[i] == GS_CELLS - k + i) i--;
        if (i < 0) break;
        c[i]++;
        for (int j = i + 1; j < k; j++) c[j] = c[j - 1] + 1;
    }
    return A;
}

static void grid_double(FILE *f, const char *title, const double *v, const char *fmt)
{
    fprintf(f, "%s\n\n", title);
    fprintf(f, "|  | A | B | C | D | E | F |\n|---|---|---|---|---|---|---|\n");
    for (int r = 0; r < GS_N; r++) {
        fprintf(f, "| **%d** |", r + 1);
        for (int c = 0; c < GS_N; c++) { fprintf(f, " "); fprintf(f, fmt, v[r * GS_N + c]); fprintf(f, " |"); }
        fprintf(f, "\n");
    }
    fprintf(f, "\n");
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    const char *cpath = "data/counts.gsc";
    const char *opath = NULL;
    long hard_T = 0;                  /* 0 => study unsolvable boards */
    int  deg3 = 0;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "-c") && i + 1 < argc) cpath = argv[++i];
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) opath = argv[++i];
        else if (!strcmp(argv[i], "--hard") && i + 1 < argc) hard_T = atol(argv[++i]);
        else if (!strcmp(argv[i], "--deg3")) deg3 = 1;
        else { fprintf(stderr,
                 "usage: %s [-c counts.gsc] [-o report.md] [--hard T] [--deg3]\n"
                 "  default studies unsolvable boards; --hard T studies boards\n"
                 "  with 1..T solutions instead; --deg3 adds the degree-3\n"
                 "  Johnson projection (7140x7140 Cholesky, ~0.5 GB, minutes)\n",
                 argv[0]); return 1; }
    }

    gs_init_all();
    for (int i = 0, k = 0; i < GS_CELLS; i++)
        for (int j = i + 1; j < GS_CELLS; j++, k++) {
            pair_id[i][j] = pair_id[j][i] = k;
            pair_i[k] = i; pair_j[k] = j;
        }
    /* triples in colex order: rank runs 0..NTRIP-1 exactly in this loop order */
    for (int c = 2, k = 0; c < GS_CELLS; c++)
        for (int b = 1; b < c; b++)
            for (int a = 0; a < b; a++, k++)
                tri_mask[k] = ((gs_mask)1 << a) | ((gs_mask)1 << b)
                            | ((gs_mask)1 << c);

    uint32_t n = 0;
    uint32_t *canon = gs_read_canon(cpath, &n);
    if (!canon) return 1;
    uint32_t *list = gs_canon_list(&n);
    uint32_t *counts = gs_expand_canon(canon, list, n);
    if (!counts) return 1;
    free(canon); free(list);

    FILE *out = opath ? fopen(opath, "w") : stdout;
    if (!out) { perror(opath); return 1; }

    /* ---- first, second (and optionally third) moments of the indicator ---- */
    double  U = 0;
    double *m1 = calloc(GS_CELLS, sizeof *m1);
    double *m2 = calloc(NPAIR, sizeof *m2);
    double *m3 = deg3 ? calloc(NTRIP, sizeof *m3) : NULL;
    int c7[GS_PEGS];
    for (int i = 0; i < GS_PEGS; i++) c7[i] = i;
    for (uint32_t rank = 0; rank < GS_NBOARDS; rank++) {
        uint32_t v = counts[rank];
        int in = hard_T ? (v > 0 && (long)v <= hard_T) : (v == 0);
        if (in) {
            U += 1.0;
            for (int i = 0; i < GS_PEGS; i++) {
                m1[c7[i]] += 1.0;
                for (int j = i + 1; j < GS_PEGS; j++) m2[pair_id[c7[i]][c7[j]]] += 1.0;
            }
            if (m3)
                for (int i = 0; i < GS_PEGS; i++)
                    for (int j = i + 1; j < GS_PEGS; j++)
                        for (int k = j + 1; k < GS_PEGS; k++)
                            m3[trip_rank(c7[i], c7[j], c7[k])] += 1.0;
        }
        int i = GS_PEGS - 1;
        while (i >= 0 && c7[i] == GS_CELLS - GS_PEGS + i) i--;
        if (i < 0) break;
        c7[i]++;
        for (int j = i + 1; j < GS_PEGS; j++) c7[j] = c7[j - 1] + 1;
    }

    const double N  = (double)GS_NBOARDS;
    const double p1 = 7.0 / 36.0;                     /* P(cell is a peg)      */
    const double p2 = 7.0 * 6.0 / (36.0 * 35.0);      /* P(both cells are pegs) */

    fprintf(out, "# What makes a Genius Square board hard?\n\n");
    fprintf(out, "Class studied: **%s**.\n\n",
            hard_T ? "boards with few solutions" : "unsolvable boards");
    if (hard_T) fprintf(out, "(1 to %ld solutions inclusive)\n\n", hard_T);
    fprintf(out, "| quantity | value |\n|---|---|\n");
    fprintf(out, "| boards in class | %.0f |\n", U);
    fprintf(out, "| base rate | %.6f%% |\n", 100.0 * U / N);
    if (U == 0) { fprintf(out, "\nClass is empty; nothing to analyse.\n"); return 0; }

    /* ---- degree-1 heat map ---- */
    double heat[GS_CELLS];
    for (int i = 0; i < GS_CELLS; i++) heat[i] = (m1[i] / U) / p1;
    fprintf(out, "\n## Degree 1: where the pegs sit\n\n");
    fprintf(out,
      "Enrichment = P(cell is a peg | class) / P(cell is a peg). "
      "1.00 means the class says nothing about that cell.\n\n");
    grid_double(out, "**Peg enrichment**", heat, "%.3f");

    /* D4 invariance of the heat map is a free correctness check */
    {
        double worst = 0;
        for (int g = 1; g < GS_NSYM; g++)
            for (int i = 0; i < GS_CELLS; i++) {
                double d = fabs(heat[i] - heat[gs_perm[g][i]]);
                if (d > worst) worst = d;
            }
        fprintf(out, "Largest deviation from D4 symmetry in the heat map: %.2e "
                     "(it must be 0 up to rounding, since the class is "
                     "symmetry-closed).\n\n", worst);
    }

    /* ---- least squares in W_1 and W_2 ---- */
    double ssq_total = U - U * U / N;      /* ||f - mean||^2 */
    double r2_1 = 0, r2_2 = 0;
    {
        double *G = malloc((size_t)GS_CELLS * GS_CELLS * sizeof *G);
        double *b = malloc((size_t)GS_CELLS * sizeof *b);
        for (int i = 0; i < GS_CELLS; i++) {
            b[i] = m1[i];
            for (int j = 0; j < GS_CELLS; j++)
                G[i * GS_CELLS + j] = gram_u[(i == j) ? 1 : 2];
        }
        double *rhs = malloc((size_t)GS_CELLS * sizeof *rhs);
        memcpy(rhs, b, (size_t)GS_CELLS * sizeof *rhs);
        if (cholesky_solve(G, b, GS_CELLS) == 0) {
            double dot = 0;
            for (int i = 0; i < GS_CELLS; i++) dot += b[i] * rhs[i];
            r2_1 = (dot - U * U / N) / ssq_total;
        }
        free(G); free(b); free(rhs);
    }
    {
        double *G = malloc((size_t)NPAIR * NPAIR * sizeof *G);
        double *b = malloc((size_t)NPAIR * sizeof *b);
        for (int s = 0; s < NPAIR; s++) {
            b[s] = m2[s];
            for (int t = 0; t < NPAIR; t++) {
                int u = 4;
                int a1 = pair_i[s], a2 = pair_j[s], b1 = pair_i[t], b2 = pair_j[t];
                int common = (a1 == b1) + (a1 == b2) + (a2 == b1) + (a2 == b2);
                u = 4 - common;
                G[(size_t)s * NPAIR + t] = gram_u[u];
            }
        }
        double *rhs = malloc((size_t)NPAIR * sizeof *rhs);
        memcpy(rhs, b, (size_t)NPAIR * sizeof *rhs);
        if (cholesky_solve(G, b, NPAIR) == 0) {
            double dot = 0;
            for (int s = 0; s < NPAIR; s++) dot += b[s] * rhs[s];
            r2_2 = (dot - U * U / N) / ssq_total;
        }
        free(G); free(b); free(rhs);
    }
    /* Degree 3: the span of the C(36,3) = 7140 triple indicators.  Same
     * normal equations, just bigger: the Gram matrix is 7140^2 doubles
     * (~0.4 GB) and the Cholesky is n^3/3 ~ 1.2e11 flops.  The inclusion
     * matrix W_{3,7} has full rank C(36,3) (3 <= min(7, 29)), so the Gram
     * matrix is positive definite and the row-form Cholesky is safe. */
    double r2_3 = -1;
    if (m3) {
        double t0 = (double)clock() / CLOCKS_PER_SEC;
        double *G = malloc((size_t)NTRIP * NTRIP * sizeof *G);
        double *b = malloc((size_t)NTRIP * sizeof *b);
        double *rhs = malloc((size_t)NTRIP * sizeof *rhs);
        if (!G || !b || !rhs) { fprintf(stderr, "deg3: out of memory\n"); return 1; }
        for (int s = 0; s < NTRIP; s++) {
            b[s] = rhs[s] = m3[s];
            for (int t = 0; t < NTRIP; t++) {
                int u = 6 - __builtin_popcountll(tri_mask[s] & tri_mask[t]);
                G[(size_t)s * NTRIP + t] = gram_u[u];
            }
        }
        fprintf(stderr, "  deg3: Gram built, factoring 7140x7140...\n");
        if (cholesky_solve(G, b, NTRIP) == 0) {
            double dot = 0;
            for (int s = 0; s < NTRIP; s++) dot += b[s] * rhs[s];
            r2_3 = (dot - U * U / N) / ssq_total;
        } else
            fprintf(stderr, "  deg3: Cholesky failed (Gram not PD?)\n");
        fprintf(stderr, "  deg3 done in %.1f s cpu\n",
                (double)clock() / CLOCKS_PER_SEC - t0);
        free(G); free(b); free(rhs);
    }

    fprintf(out, "## How much of hardness is low order?\n\n");
    fprintf(out,
      "Exact least-squares fit of the class indicator in the Johnson-scheme\n"
      "subspaces, over all %.0f boards. R^2 is the fraction of variance\n"
      "explained relative to the constant model.\n\n", N);
    fprintf(out, "| model | parameters | R^2 |\n|---|---:|---:|\n");
    fprintf(out, "| degree 0 (base rate) | 1 | 0.0000 |\n");
    fprintf(out, "| degree <= 1 (per-cell weights) | 36 | %.4f |\n", r2_1);
    fprintf(out, "| degree <= 2 (per-pair weights) | 630 | %.4f |\n", r2_2);
    if (r2_3 >= 0)
        fprintf(out, "| degree <= 3 (per-triple weights) | 7140 | %.4f |\n", r2_3);
    fprintf(out, "\nEnergy in level 1 alone: %.4f; in level 2 alone: %.4f",
            r2_1, r2_2 - r2_1);
    if (r2_3 >= 0)
        fprintf(out, "; in level 3 alone: %.4f; unexplained by degree <= 3: "
                     "%.4f.\n\n", r2_3 - r2_2, 1.0 - r2_3);
    else
        fprintf(out, "; unexplained by degree <= 2: %.4f.\n\n", 1.0 - r2_2);

    /* ---- second-order excess co-occurrence, spectrally ---- */
    double *M = calloc((size_t)GS_CELLS * GS_CELLS, sizeof *M);
    for (int s = 0; s < NPAIR; s++) {
        int i = pair_i[s], j = pair_j[s];
        double obs = m2[s] / U;
        double ind = (m1[i] / U) * (m1[j] / U) * (p2 / (p1 * p1));
        double e = obs - ind;
        M[i * GS_CELLS + j] = M[j * GS_CELLS + i] = e;
    }
    for (int i = 0; i < GS_CELLS; i++) M[i * GS_CELLS + i] = 0.0;

    fprintf(out, "## Degree 2: co-occurrence beyond the heat map\n\n");
    fprintf(out,
      "M[i][j] = P(pegs at i and j | class) minus what the degree-1 heat map\n"
      "alone would predict. Positive means the two cells conspire.\n\n");
    {
        /* strongest positive and negative pairs */
        int idx[NPAIR];
        for (int s = 0; s < NPAIR; s++) idx[s] = s;
        for (int a = 0; a < NPAIR; a++)          /* partial selection sort */
            for (int b2 = a + 1; b2 < NPAIR; b2++) {
                int x = idx[a], y = idx[b2];
                double va = M[pair_i[x] * GS_CELLS + pair_j[x]];
                double vb = M[pair_i[y] * GS_CELLS + pair_j[y]];
                if (vb > va) { idx[a] = y; idx[b2] = x; }
            }
        fprintf(out, "| rank | cells | excess |\n|---:|---|---:|\n");
        for (int a = 0; a < 10; a++) {
            char n1[4], n2[4];
            gs_cell_name(pair_i[idx[a]], n1); gs_cell_name(pair_j[idx[a]], n2);
            fprintf(out, "| +%d | %s %s | %+.5f |\n", a + 1, n1, n2,
                    M[pair_i[idx[a]] * GS_CELLS + pair_j[idx[a]]]);
        }
        for (int a = NPAIR - 5; a < NPAIR; a++) {
            char n1[4], n2[4];
            gs_cell_name(pair_i[idx[a]], n1); gs_cell_name(pair_j[idx[a]], n2);
            fprintf(out, "| -%d | %s %s | %+.5f |\n", NPAIR - a, n1, n2,
                    M[pair_i[idx[a]] * GS_CELLS + pair_j[idx[a]]]);
        }
        fprintf(out, "\n");
    }

    fprintf(out, "## Spectral view of the second-order structure\n\n");
    fprintf(out,
      "Eigenvectors of M are the peg-placement *modes* whose presence most\n"
      "distinguishes the class. A mode is a weighting of the 36 cells; boards\n"
      "whose peg pattern correlates strongly with a high-eigenvalue mode are\n"
      "over-represented in the class.\n\n");
    {
        double *A = malloc((size_t)GS_CELLS * GS_CELLS * sizeof *A);
        double *ev = malloc((size_t)GS_CELLS * sizeof *ev);
        double *V = malloc((size_t)GS_CELLS * GS_CELLS * sizeof *V);
        memcpy(A, M, (size_t)GS_CELLS * GS_CELLS * sizeof *A);
        jacobi(A, GS_CELLS, ev, V);
        int order[GS_CELLS];
        for (int i = 0; i < GS_CELLS; i++) order[i] = i;
        for (int a = 0; a < GS_CELLS; a++)
            for (int b2 = a + 1; b2 < GS_CELLS; b2++)
                if (fabs(ev[order[b2]]) > fabs(ev[order[a]])) {
                    int t = order[a]; order[a] = order[b2]; order[b2] = t;
                }
        fprintf(out, "| mode | eigenvalue |\n|---:|---:|\n");
        for (int a = 0; a < 8; a++)
            fprintf(out, "| %d | %+.6f |\n", a + 1, ev[order[a]]);
        fprintf(out, "\n");
        for (int a = 0; a < 4; a++) {
            double v[GS_CELLS];
            double mx = 0;
            for (int i = 0; i < GS_CELLS; i++) {
                v[i] = V[i * GS_CELLS + order[a]];
                if (fabs(v[i]) > mx) mx = fabs(v[i]);
            }
            /* fix the sign so the largest entry is positive, then scale */
            double sgn = 1;
            for (int i = 0; i < GS_CELLS; i++) if (fabs(v[i]) == mx) { sgn = v[i] > 0 ? 1 : -1; break; }
            for (int i = 0; i < GS_CELLS; i++) v[i] = v[i] * sgn / mx;
            char title[128];
            snprintf(title, sizeof title, "**Mode %d** (eigenvalue %+.6f)",
                     a + 1, ev[order[a]]);
            grid_double(out, title, v, "%+.2f");
        }
        free(A); free(ev); free(V);
    }

    /* ---- DCT spectrum ---- */
    fprintf(out, "## Spatial-frequency spectrum\n\n");
    fprintf(out,
      "The same second-order information read in a 2-D DCT-II basis: the mean\n"
      "power at spatial frequency (u,v) for boards in the class, divided by the\n"
      "mean power over all boards. Row/column 0 is the DC term.\n\n");
    {
        double phi[GS_N][GS_N];
        for (int u = 0; u < GS_N; u++)
            for (int x = 0; x < GS_N; x++)
                phi[u][x] = ((u == 0) ? sqrt(1.0 / GS_N) : sqrt(2.0 / GS_N))
                          * cos(M_PI * (2 * x + 1) * u / (2.0 * GS_N));

        /* E[b_i b_j | class]: diagonal is E[b_i | class] since b is 0/1 */
        double Ecl[GS_CELLS][GS_CELLS], Eal[GS_CELLS][GS_CELLS];
        for (int i = 0; i < GS_CELLS; i++)
            for (int j = 0; j < GS_CELLS; j++) {
                if (i == j) { Ecl[i][j] = m1[i] / U;              Eal[i][j] = p1; }
                else        { Ecl[i][j] = m2[pair_id[i][j]] / U;  Eal[i][j] = p2; }
            }
        double spec[GS_CELLS];
        for (int u = 0; u < GS_N; u++)
            for (int v = 0; v < GS_N; v++) {
                double pc = 0, pa = 0;
                for (int i = 0; i < GS_CELLS; i++) {
                    double wi = phi[u][i % GS_N] * phi[v][i / GS_N];
                    for (int j = 0; j < GS_CELLS; j++) {
                        double wj = phi[u][j % GS_N] * phi[v][j / GS_N];
                        pc += wi * wj * Ecl[i][j];
                        pa += wi * wj * Eal[i][j];
                    }
                }
                spec[v * GS_N + u] = (pa > 1e-12) ? pc / pa : 0.0;
            }
        fprintf(out, "|  | u=0 | u=1 | u=2 | u=3 | u=4 | u=5 |\n"
                     "|---|---|---|---|---|---|---|\n");
        for (int v = 0; v < GS_N; v++) {
            fprintf(out, "| **v=%d** |", v);
            for (int u = 0; u < GS_N; u++) fprintf(out, " %.3f |", spec[v * GS_N + u]);
            fprintf(out, "\n");
        }
        fprintf(out, "\n");
    }

    /* ---- minimal blocking patterns ---- */
    if (!hard_T) {
        for (int k = 0; k <= GS_CELLS; k++) {
            double v = 1;
            for (int i = 0; i < k; i++) v = v * (GS_CELLS - i) / (i + 1);
            nsub[k] = (uint32_t)(v + 0.5);
        }
        fprintf(out, "## Minimal blocking patterns\n\n");
        fprintf(out,
          "A set of cells is **blocking** if *every* board whose 7 pegs include\n"
          "those cells is unsolvable -- the pattern is fatal regardless of where\n"
          "the other pegs go. Blocking is upward closed, so the minimal blocking\n"
          "sets are the real culprits, and they are computed exactly rather than\n"
          "inferred statistically.\n\n");

        uint8_t *lvl[GS_PEGS + 1];
        memset(lvl, 0, sizeof lvl);
        lvl[6] = blocking_level(NULL, 6, counts);
        for (int k = 5; k >= 1; k--) lvl[k] = blocking_level(lvl[k + 1], k, counts);

        fprintf(out, "| pattern size | blocking sets | of which minimal | "
                     "minimal, up to symmetry |\n|---:|---:|---:|---:|\n");
        int smallest = 0;
        for (int k = 1; k <= 6; k++) {
            if (!lvl[k]) continue;
            uint64_t nb = 0, nmin = 0;
            uint32_t norb = 0;
            int c[GS_CELLS], sub[GS_CELLS];
            for (int i = 0; i < k; i++) c[i] = i;
            for (;;) {
                if (lvl[k][colex_rank(c, k)]) {
                    nb++;
                    int minimal = 1;
                    if (k > 1) {
                        for (int drop = 0; drop < k && minimal; drop++) {
                            int m = 0;
                            for (int i = 0; i < k; i++) if (i != drop) sub[m++] = c[i];
                            if (lvl[k - 1][colex_rank(sub, k - 1)]) minimal = 0;
                        }
                    }
                    if (minimal) {
                        nmin++;
                        if (!smallest) smallest = k;
                        gs_mask msk = 0;
                        for (int i = 0; i < k; i++) msk |= (gs_mask)1 << c[i];
                        if (gs_canon(msk) == msk) norb++;
                    }
                }
                int i = k - 1;
                while (i >= 0 && c[i] == GS_CELLS - k + i) i--;
                if (i < 0) break;
                c[i]++;
                for (int j = i + 1; j < k; j++) c[j] = c[j - 1] + 1;
            }
            fprintf(out, "| %d | %llu | %llu | %u |\n", k,
                    (unsigned long long)nb, (unsigned long long)nmin, norb);
        }
        fprintf(out, "\n");

        /* show a few of the smallest minimal patterns */
        if (smallest) {
            fprintf(out, "### Smallest fatal patterns (size %d)\n\n", smallest);
            fprintf(out, "`#` marks a peg of the pattern; the remaining %d pegs "
                         "may be anywhere.\n\n", GS_PEGS - smallest);
            int c[GS_CELLS], shown = 0;
            for (int i = 0; i < smallest; i++) c[i] = i;
            for (;;) {
                if (lvl[smallest][colex_rank(c, smallest)] && shown < 6) {
                    gs_mask msk = 0;
                    for (int i = 0; i < smallest; i++) msk |= (gs_mask)1 << c[i];
                    if (gs_canon(msk) == msk) {
                        char buf[512];
                        gs_render(msk, NULL, buf, sizeof buf);
                        fprintf(out, "```\n%s```\n\n", buf);
                        shown++;
                    }
                }
                int i = smallest - 1;
                while (i >= 0 && c[i] == GS_CELLS - smallest + i) i--;
                if (i < 0) break;
                c[i]++;
                for (int j = i + 1; j < smallest; j++) c[j] = c[j - 1] + 1;
            }
        }

        /* coverage: how many unsolvable boards contain a blocking pattern */
        fprintf(out, "### Coverage of the unsolvable boards\n\n");
        fprintf(out, "| pattern size <= k | unsolvable boards containing one | %% of class |\n"
                     "|---:|---:|---:|\n");
        for (int K = 1; K <= 6; K++) {
            uint64_t cov = 0;
            int cb[GS_PEGS], sub[GS_PEGS];
            for (int i = 0; i < GS_PEGS; i++) cb[i] = i;
            for (uint32_t rank = 0; rank < GS_NBOARDS; rank++) {
                if (counts[rank] == 0) {
                    int hit = 0;
                    for (int k = 1; k <= K && !hit; k++) {
                        if (!lvl[k]) continue;
                        /* iterate k-subsets of the 7 pegs */
                        int idx[GS_PEGS];
                        for (int i = 0; i < k; i++) idx[i] = i;
                        for (;;) {
                            for (int i = 0; i < k; i++) sub[i] = cb[idx[i]];
                            if (lvl[k][colex_rank(sub, k)]) { hit = 1; break; }
                            int i = k - 1;
                            while (i >= 0 && idx[i] == GS_PEGS - k + i) i--;
                            if (i < 0) break;
                            idx[i]++;
                            for (int j = i + 1; j < k; j++) idx[j] = idx[j - 1] + 1;
                        }
                    }
                    cov += hit;
                }
                int i = GS_PEGS - 1;
                while (i >= 0 && cb[i] == GS_CELLS - GS_PEGS + i) i--;
                if (i < 0) break;
                cb[i]++;
                for (int j = i + 1; j < GS_PEGS; j++) cb[j] = cb[j - 1] + 1;
            }
            fprintf(out, "| %d | %llu | %.2f |\n", K,
                    (unsigned long long)cov, 100.0 * (double)cov / U);
        }
        fprintf(out, "\nA board covered at size k is unsolvable *for a reason that "
                     "fits in k cells*. The residue -- unsolvable boards covered by "
                     "nothing smaller than all 7 pegs -- are the genuinely global "
                     "failures.\n\n");
        for (int k = 1; k <= 6; k++) free(lvl[k]);
    }

    if (opath) { fclose(out); fprintf(stderr, "  wrote %s\n", opath); }
    free(m1); free(m2); free(m3); free(M); free(counts);
    return 0;
}

/* gs_selftest -- structural checks plus a throughput probe. */
#include "gs_core.h"
#include "gs_search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + 1e-9 * ts.tv_nsec;
}

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { \
    printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(int argc, char **argv)
{
    gs_init_all();
    gs_init_search();
    static gs_ptable t;
    gs_build_placements(&t);

    printf("== placements ==\n");
    int per[GS_NPIECES] = {0};
    for (int i = 0; i < t.n; i++) per[t.p[i].piece]++;
    for (int p = 0; p < GS_NPIECES; p++)
        printf("  %-8s area=%d  placements=%d\n",
               gs_piece_name[p], gs_piece_size[p], per[p]);
    printf("  total placements = %d\n", t.n);

    int area = 0;
    for (int p = 0; p < GS_NPIECES; p++) area += gs_piece_size[p];
    CHECK(area == GS_AREA, "piece areas sum to %d, expected %d", area, GS_AREA);
    CHECK(t.n == 625, "expected 625 placements, got %d", t.n);

    /* index consistency */
    for (int c = 0; c < GS_CELLS; c++)
        for (int q = 0; q < GS_NPIECES; q++)
            for (int i = t.lo[c][q]; i < t.hi[c][q]; i++) {
                CHECK(t.p[i].anchor == c, "anchor mismatch");
                CHECK(t.p[i].piece == q, "piece mismatch");
                CHECK(__builtin_ctzll(t.p[i].mask) == c, "lowest bit != anchor");
                CHECK(__builtin_popcountll(t.p[i].mask) == gs_piece_size[q],
                      "placement popcount mismatch");
            }

    printf("== symmetry ==\n");
    /* D4 must be closed and act faithfully by permutations */
    for (int g = 0; g < GS_NSYM; g++) {
        int seen[GS_CELLS] = {0};
        for (int c = 0; c < GS_CELLS; c++) seen[gs_perm[g][c]]++;
        for (int c = 0; c < GS_CELLS; c++) CHECK(seen[c] == 1, "perm %d not bijective", g);
    }
    for (int g = 0; g < GS_NSYM; g++)
        for (int h = 0; h < GS_NSYM; h++) {
            uint8_t comp[GS_CELLS];
            for (int c = 0; c < GS_CELLS; c++) comp[c] = gs_perm[g][gs_perm[h][c]];
            int found = 0;
            for (int k = 0; k < GS_NSYM; k++)
                if (!memcmp(comp, gs_perm[k], GS_CELLS)) { found = 1; break; }
            CHECK(found, "D4 not closed under composition (%d o %d)", g, h);
        }
    /* the piece set must be closed under D4: every image of a placement is
     * again a placement of the same piece */
    for (int i = 0; i < t.n; i++)
        for (int g = 0; g < GS_NSYM; g++) {
            gs_mask m = gs_apply(g, t.p[i].mask);
            int found = 0;
            for (int j = t.cell_lo[__builtin_ctzll(m)];
                 j < t.cell_hi[__builtin_ctzll(m)]; j++)
                if (t.p[j].mask == m && t.p[j].piece == t.p[i].piece) { found = 1; break; }
            CHECK(found, "placement %d not closed under symmetry %d", i, g);
        }
    printf("  D4 closed; piece set closed under D4 (solution counts are "
           "D4-equivariant)\n");

    /* Burnside: number of D4 orbits on 7-subsets */
    {
        uint64_t fixsum = 0;
        for (int g = 0; g < GS_NSYM; g++) {
            /* cycles of g on cells */
            int cyc[GS_CELLS], ncyc = 0, mark[GS_CELLS] = {0};
            for (int c = 0; c < GS_CELLS; c++) {
                if (mark[c]) continue;
                int len = 0, x = c;
                do { mark[x] = 1; x = gs_perm[g][x]; len++; } while (x != c);
                cyc[ncyc++] = len;
            }
            /* count subsets of size 7 that are unions of cycles */
            uint64_t dp[GS_PEGS + 1] = {1};
            for (int i = 0; i < ncyc; i++)
                for (int k = GS_PEGS; k >= cyc[i]; k--) dp[k] += dp[k - cyc[i]];
            fixsum += dp[GS_PEGS];
        }
        printf("  orbits of D4 on 7-subsets (Burnside) = %llu\n",
               (unsigned long long)(fixsum / GS_NSYM));
    }

    printf("== mirror canonicality (the search-time symmetry cut) ==\n");
    {
        /* gs_countall prunes a branch when the hole set is lexicographically
         * greater than its left-right mirror, decided one row at a time.  That
         * row-wise test must agree with a direct comparison of the whole sets,
         * and every D4-canonical board must survive it. */
        uint8_t rev6[64];
        for (int b = 0; b < 64; b++) {
            int r = 0;
            for (int k = 0; k < 6; k++) if (b & (1 << k)) r |= 1 << (5 - k);
            rev6[b] = (uint8_t)r;
        }
        for (int trial = 0; trial < 300000; trial++) {
            gs_mask h = gs_unrank((uint32_t)(((uint64_t)rand() * 32768 + rand()) % GS_NBOARDS));
            /* row-wise verdict, exactly as the search computes it */
            int rowwise = 0;
            for (int r = 0; r < GS_N; r++) {
                unsigned b = (unsigned)((h >> (GS_N * r)) & 63u), rb = rev6[b];
                unsigned d = b ^ rb;
                if (!d) continue;
                rowwise = ((b >> __builtin_ctz(d)) & 1u) ? 1 : -1;
                break;
            }
            /* direct verdict on the full 36-cell sets */
            gs_mask mv = gs_apply(4, h);
            gs_mask d  = h ^ mv;
            int direct = !d ? 0 : ((h & (d & (~d + 1))) ? 1 : -1);
            CHECK(rowwise == direct, "row-wise mirror test disagrees (%d vs %d)",
                  rowwise, direct);
            /* the canonical member of an orbit is never mirror-rejected */
            gs_mask cn = gs_canon(h);
            gs_mask cmv = gs_apply(4, cn), cd = cn ^ cmv;
            CHECK(!cd || (cn & (cd & (~cd + 1))),
                  "D4-canonical board would be pruned by the mirror cut");
            CHECK(gs_rank(cn) <= gs_rank(h), "canonical rank exceeds member rank");
            CHECK(gs_canon(mv) == cn, "canon not constant on orbits");
        }
        printf("  row-wise mirror test == direct test; canonical boards survive\n");
    }

    printf("== ranking ==\n");
    srand(12345);
    for (int trial = 0; trial < 200000; trial++) {
        uint32_t r = (uint32_t)(((uint64_t)rand() * 32768 + rand()) % GS_NBOARDS);
        gs_mask m = gs_unrank(r);
        CHECK(__builtin_popcountll(m) == GS_PEGS, "unrank popcount");
        CHECK(gs_rank(m) == r, "rank/unrank round trip at %u", r);
    }
    CHECK(gs_rank(gs_unrank(0)) == 0, "rank 0");
    CHECK(gs_rank(gs_unrank(GS_NBOARDS - 1)) == GS_NBOARDS - 1, "last rank");
    printf("  rank/unrank round trip ok\n");

    printf("== solver ==\n");
    /* An empty-corner reference board and a couple of sanity cases. */
    {
        gs_mask b = gs_parse_board("A1 B1 C1 D1 E1 F1 A2");
        printf("  board A1..F1,A2 -> %llu solutions\n",
               (unsigned long long)gs_solve_count(&t, b));
        gs_mask b2 = gs_parse_board("A1 C1 E1 B4 D4 F4 C6");
        printf("  board A1,C1,E1,B4,D4,F4,C6 -> %llu solutions\n",
               (unsigned long long)gs_solve_count(&t, b2));
        /* equivariance: a board and its 8 images must agree */
        for (int trial = 0; trial < 200; trial++) {
            gs_mask m = gs_unrank((uint32_t)(((uint64_t)rand() * 32768 + rand()) % GS_NBOARDS));
            uint64_t c0 = gs_solve_count(&t, m);
            for (int g = 1; g < GS_NSYM; g++)
                CHECK(gs_solve_count(&t, gs_apply(g, m)) == c0,
                      "count not invariant under symmetry %d", g);
        }
        printf("  solution counts verified D4-invariant on 200 random boards\n");
        /* enumeration count must match the counter */
        for (int trial = 0; trial < 100; trial++) {
            gs_mask m = gs_unrank((uint32_t)(((uint64_t)rand() * 32768 + rand()) % GS_NBOARDS));
            CHECK(gs_solve_enum(&t, m, NULL, NULL) == gs_solve_count(&t, m),
                  "enum != count");
        }
        printf("  gs_solve_enum agrees with gs_solve_count\n");
    }

    printf("== throughput ==\n");
    {
        int nsamp = (argc > 1) ? atoi(argv[1]) : 20000;
        uint64_t tot = 0;
        double t0 = now();
        for (int i = 0; i < nsamp; i++) {
            uint32_t r = (uint32_t)((uint64_t)i * 8347679u / (uint64_t)nsamp);
            tot += gs_solve_count(&t, gs_unrank(r));
        }
        double dt = now() - t0;
        printf("  %d boards in %.3f s  =>  %.1f us/board, mean solutions %.1f\n",
               nsamp, dt, 1e6 * dt / nsamp, (double)tot / nsamp);
        printf("  extrapolated single-core time for all %u boards: %.1f s\n",
               GS_NBOARDS, dt / nsamp * GS_NBOARDS);
        printf("  extrapolated total packings (sum over all boards): %.3g\n",
               (double)tot / nsamp * GS_NBOARDS);
    }

    printf(fails ? "\n%d CHECK(s) FAILED\n" : "\nall checks passed\n", fails);
    return fails ? 1 : 0;
}

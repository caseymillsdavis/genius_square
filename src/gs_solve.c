/* gs_solve -- everything about one board.
 *
 *   gs_solve A1 B3 C2 D5 E1 F6 B6          # count + one solution
 *   gs_solve --all A1 B3 C2 D5 E1 F6 B6    # print every solution
 *   gs_solve --freedom  ...                # per-piece freedom / trap report
 *   gs_solve --dlx ...                     # use dancing links instead
 */
#include "gs_core.h"
#include "gs_search.h"
#include "gs_dlx.h"

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

static gs_ptable T;
static gs_mask   PEGS;
static long      limit = 1;
static long      shown = 0;

static void print_solution(const int idx[GS_NPIECES], uint64_t n)
{
    uint8_t cp[GS_CELLS];
    memset(cp, 0xFF, sizeof cp);
    for (int d = 0; d < GS_NPIECES; d++) {
        gs_mask m = T.p[idx[d]].mask;
        while (m) { int c = __builtin_ctzll(m); m &= m - 1; cp[c] = T.p[idx[d]].piece; }
    }
    char buf[512];
    gs_render(PEGS, cp, buf, sizeof buf);
    printf("solution %llu:\n%s\n", (unsigned long long)n, buf);
}

static int cb_print(const int idx[GS_NPIECES], void *user)
{
    uint64_t *n = user;
    (*n)++;
    if (limit < 0 || shown < limit) { print_solution(idx, *n); shown++; }
    return (limit >= 0 && shown >= limit) ? 1 : 0;
}

static int cb_count_only(const int idx[GS_NPIECES], void *user)
{
    (void)idx; (void)user;
    return 0;
}

static void usage(const char *p)
{
    fprintf(stderr,
      "usage: %s [options] <7 cells>\n"
      "  cells are A1..F6 (column letter, row digit) or raw indices 0..35\n"
      "  --count        only print the number of solutions\n"
      "  --all          print every solution\n"
      "  -n N           print at most N solutions (default 1)\n"
      "  --freedom      per-piece placement freedom and trap report\n"
      "  --dlx          solve with dancing links (cross-check / timing)\n"
      "  --both         run both engines and compare\n", p);
}

int main(int argc, char **argv)
{
    gs_init_all();
    gs_init_search();
    gs_build_placements(&T);

    int mode_count = 0, mode_freedom = 0, use_dlx = 0, both = 0;
    char cells[512] = {0};
    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--count"))   mode_count = 1;
        else if (!strcmp(argv[i], "--all"))     limit = -1;
        else if (!strcmp(argv[i], "--freedom")) mode_freedom = 1;
        else if (!strcmp(argv[i], "--dlx"))     use_dlx = 1;
        else if (!strcmp(argv[i], "--both"))    both = 1;
        else if (!strcmp(argv[i], "-n") && i + 1 < argc) limit = atol(argv[++i]);
        else if (argv[i][0] == '-')             { usage(argv[0]); return 1; }
        else { strncat(cells, argv[i], sizeof cells - strlen(cells) - 2);
               strncat(cells, " ",     sizeof cells - strlen(cells) - 2); }
    }
    PEGS = gs_parse_board(cells);
    if (!PEGS) { fprintf(stderr, "need exactly 7 distinct cells\n"); usage(argv[0]); return 1; }

    if (!mode_count) {
        char buf[512];
        gs_render(PEGS, NULL, buf, sizeof buf);
        printf("board (rank %u, canonical rank %u):\n%s\n",
               gs_rank(PEGS), gs_rank(gs_canon(PEGS)), buf);
        gs_geom g;
        gs_geometry(PEGS, &g);
        printf("empty region: %d component(s), sizes", g.ncomp);
        for (int i = 0; i < g.ncomp; i++) printf(" %d", g.comp_size[i]);
        printf("%s\n\n", g.n_singletons ? "   (has isolated cell(s))" : "");
    }

    if (both) {
        double t0 = now();
        uint64_t a = gs_solve_count(&T, PEGS);
        double t1 = now();
        uint64_t b = gs_dlx_solve(&T, PEGS, cb_count_only, NULL);
        double t2 = now();
        printf("bitmask DFS : %10llu solutions  %8.3f ms\n",
               (unsigned long long)a, 1e3 * (t1 - t0));
        printf("dancing links %10llu solutions  %8.3f ms\n",
               (unsigned long long)b, 1e3 * (t2 - t1));
        printf(a == b ? "engines agree\n" : "*** ENGINES DISAGREE ***\n");
        return a == b ? 0 : 2;
    }

    if (mode_count) {
        uint64_t n = use_dlx ? gs_dlx_solve(&T, PEGS, cb_count_only, NULL)
                             : gs_solve_count(&T, PEGS);
        printf("%llu\n", (unsigned long long)n);
        return 0;
    }

    if (mode_freedom) {
        gs_freedom f;
        uint64_t n = gs_solve_freedom(&T, PEGS, &f);
        printf("solutions: %llu\n\n", (unsigned long long)n);
        if (!n) { printf("board is UNSOLVABLE\n"); return 0; }
        printf("%-8s %10s  %s\n", "piece", "placements", "status");
        int ntrap = 0;
        for (int p = 0; p < GS_NPIECES; p++) {
            const char *st = "";
            if (f.nplaces[p] == 1) { st = "PINNED (only one possible position)"; ntrap++; }
            printf("%-8s %10u  %s\n", gs_piece_name[p], f.nplaces[p], st);
        }
        printf("\n%d of %d pieces are pinned; board is %s\n",
               ntrap, GS_NPIECES, ntrap ? "TRAPPED" : "trap-free");
        for (int p = 0; p < GS_NPIECES; p++)
            if (f.nplaces[p] == 1) {
                uint8_t cp[GS_CELLS];
                memset(cp, 0xFF, sizeof cp);
                gs_mask m = f.first[p];
                while (m) { int c = __builtin_ctzll(m); m &= m - 1; cp[c] = (uint8_t)p; }
                char buf[512];
                gs_render(PEGS, cp, buf, sizeof buf);
                printf("\npinned %s:\n%s", gs_piece_name[p], buf);
            }
        return 0;
    }

    uint64_t n = 0;
    if (use_dlx) gs_dlx_solve(&T, PEGS, (gs_dlx_cb)cb_print, &n);
    else         gs_solve_enum(&T, PEGS, cb_print, &n);

    if (n == 0) printf("no solutions -- this board is UNSOLVABLE\n");
    else if (limit >= 0) {
        uint64_t total = use_dlx ? gs_dlx_solve(&T, PEGS, cb_count_only, NULL)
                                 : gs_solve_count(&T, PEGS);
        printf("%llu solution(s) total (%llu shown)\n",
               (unsigned long long)total, (unsigned long long)shown);
    } else
        printf("%llu solution(s)\n", (unsigned long long)n);
    return 0;
}

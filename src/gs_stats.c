/* gs_stats -- summary statistics over the full count table.
 *
 *   - histogram of boards by number of solutions (exact for small counts,
 *     dyadic bins above)
 *   - the hardest boards, with and without the "trapped piece" boards that
 *     otherwise dominate the low end
 *
 * Every count is reported both per D4 orbit and per board; the two differ
 * because orbits have size 1, 2, 4 or 8.
 */
#include "gs_core.h"
#include "gs_search.h"
#include "gs_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gs_ptable T;

/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t rank;
    uint32_t count;
    uint8_t  orbit;
    unsigned pinned;      /* 9-bit set of pieces with a unique placement */
    int      npinned;
    int      min_comp;
    int      n_singletons;
} board_info;

static int cmp_info(const void *a, const void *b)
{
    const board_info *x = a, *y = b;
    if (x->count != y->count) return (x->count < y->count) ? -1 : 1;
    return (x->rank < y->rank) ? -1 : (x->rank > y->rank);
}

static void board_line(FILE *f, const board_info *bi)
{
    gs_mask m = gs_unrank(bi->rank);
    char cells[64] = {0};
    gs_mask t = m;
    while (t) {
        char nm[4];
        gs_cell_name(__builtin_ctzll(t), nm);
        t &= t - 1;
        strncat(cells, nm, sizeof cells - strlen(cells) - 2);
        if (t) strncat(cells, " ", sizeof cells - strlen(cells) - 2);
    }
    fprintf(f, "| `%s` | %u | %u | %u |", cells, bi->count, bi->orbit, bi->rank);
    if (bi->npinned == 0) fprintf(f, " none |\n");
    else {
        fprintf(f, " ");
        for (int p = 0; p < GS_NPIECES; p++)
            if (bi->pinned & (1u << p)) fprintf(f, "%s ", gs_piece_name[p]);
        fprintf(f, "|\n");
    }
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    const char *cpath = "data/counts.gsc";
    const char *opath = NULL;
    uint32_t hard_threshold = 400;   /* analyse traps below this many solutions */
    int topk = 25;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "-c") && i + 1 < argc) cpath = argv[++i];
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) opath = argv[++i];
        else if (!strcmp(argv[i], "--threshold") && i + 1 < argc)
            hard_threshold = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--top") && i + 1 < argc) topk = atoi(argv[++i]);
        else {
            fprintf(stderr, "usage: %s [-c counts.gsc] [-o report.md] "
                            "[--threshold N] [--top K]\n", argv[0]);
            return 1;
        }
    }

    gs_init_all();
    gs_init_search();
    gs_build_placements(&T);

    uint32_t n = 0;
    uint32_t *canon = gs_read_canon(cpath, &n);
    if (!canon) return 1;
    uint32_t *list = gs_canon_list(&n);
    uint8_t  *orb  = gs_canon_orbit_sizes(list, n);
    if (!list || !orb) return 1;

    FILE *out = opath ? fopen(opath, "w") : stdout;
    if (!out) { perror(opath); return 1; }

    /* ---------------- global summary ---------------- */
    uint64_t nboards = 0, packings = 0, unsolv_boards = 0, unsolv_orbits = 0;
    uint32_t maxc = 0, maxr = 0;
    for (uint32_t i = 0; i < n; i++) {
        nboards  += orb[i];
        packings += (uint64_t)canon[i] * orb[i];
        if (!canon[i]) { unsolv_boards += orb[i]; unsolv_orbits++; }
        if (canon[i] > maxc) { maxc = canon[i]; maxr = list[i]; }
    }

    fprintf(out, "# Genius Square: solution-count statistics\n\n");
    fprintf(out, "All %llu = C(36,7) placements of the 7 pegs, grouped into %u orbits\n"
                 "under the symmetry group D4 of the square.\n\n",
            (unsigned long long)nboards, n);
    fprintf(out, "| quantity | value |\n|---|---|\n");
    fprintf(out, "| boards (7-peg placements) | %llu |\n", (unsigned long long)nboards);
    fprintf(out, "| distinct boards up to symmetry | %u |\n", n);
    fprintf(out, "| total packings of the 9 pieces into the 6x6 grid | %llu |\n",
            (unsigned long long)packings);
    fprintf(out, "| mean solutions per board | %.2f |\n",
            (double)packings / (double)nboards);
    fprintf(out, "| unsolvable boards | %llu (%.4f%%) |\n",
            (unsigned long long)unsolv_boards,
            100.0 * (double)unsolv_boards / (double)nboards);
    fprintf(out, "| unsolvable boards up to symmetry | %llu |\n",
            (unsigned long long)unsolv_orbits);
    fprintf(out, "| solvable boards | %llu (%.4f%%) |\n",
            (unsigned long long)(nboards - unsolv_boards),
            100.0 * (double)(nboards - unsolv_boards) / (double)nboards);
    {
        gs_mask m = gs_unrank(maxr);
        char cells[64] = {0};
        while (m) {
            char nm[4]; gs_cell_name(__builtin_ctzll(m), nm); m &= m - 1;
            strncat(cells, nm, sizeof cells - strlen(cells) - 2);
            if (m) strncat(cells, " ", sizeof cells - strlen(cells) - 2);
        }
        fprintf(out, "| most solutions | %u (board `%s`) |\n", maxc, cells);
    }

    /* median over boards */
    {
        uint64_t *hist = calloc(maxc + 2, sizeof *hist);
        for (uint32_t i = 0; i < n; i++) hist[canon[i]] += orb[i];
        uint64_t acc = 0;
        for (uint32_t v = 0; v <= maxc; v++) {
            acc += hist[v];
            if (acc * 2 >= nboards) { fprintf(out, "| median solutions | %u |\n", v); break; }
        }
        free(hist);
    }
    fprintf(out, "\n");

    /* ---------------- exact low-end table ---------------- */
    fprintf(out, "## Exact counts at the low end\n\n");
    fprintf(out, "| solutions | orbits | boards | cumulative boards |\n|---:|---:|---:|---:|\n");
    {
        uint64_t cum = 0;
        for (uint32_t v = 0; v <= 20; v++) {
            uint64_t ob = 0, bb = 0;
            for (uint32_t i = 0; i < n; i++)
                if (canon[i] == v) { ob++; bb += orb[i]; }
            cum += bb;
            fprintf(out, "| %u | %llu | %llu | %llu |\n", v,
                    (unsigned long long)ob, (unsigned long long)bb,
                    (unsigned long long)cum);
        }
    }
    fprintf(out, "\n");

    /* ---------------- dyadic histogram ---------------- */
    fprintf(out, "## Histogram of boards by number of solutions\n\n");
    fprintf(out, "| solutions | orbits | boards | %% of boards | cumulative %% |\n"
                 "|---|---:|---:|---:|---:|\n");
    {
        uint64_t cum = 0;
        /* bin 0 is exactly zero, then [1,1], [2,3], [4,7], ... */
        uint64_t ob = 0, bb = 0;
        for (uint32_t i = 0; i < n; i++) if (!canon[i]) { ob++; bb += orb[i]; }
        cum += bb;
        fprintf(out, "| 0 (unsolvable) | %llu | %llu | %.5f | %.5f |\n",
                (unsigned long long)ob, (unsigned long long)bb,
                100.0 * bb / nboards, 100.0 * cum / nboards);
        for (uint32_t lo = 1; lo <= maxc; lo *= 2) {
            uint32_t hi = (lo > maxc / 2) ? maxc : lo * 2 - 1;
            ob = bb = 0;
            for (uint32_t i = 0; i < n; i++)
                if (canon[i] >= lo && canon[i] <= hi) { ob++; bb += orb[i]; }
            cum += bb;
            if (lo == hi) fprintf(out, "| %u | ", lo);
            else          fprintf(out, "| %u - %u | ", lo, hi);
            fprintf(out, "%llu | %llu | %.5f | %.5f |\n",
                    (unsigned long long)ob, (unsigned long long)bb,
                    100.0 * bb / nboards, 100.0 * cum / nboards);
        }
    }
    fprintf(out, "\n");

    /* ---------------- hard boards ---------------- */
    fprintf(out, "## Hardest boards\n\n");
    fprintf(out,
      "A board is called **trapped** when some piece has the *same* placement in\n"
      "every solution -- the pegs pin it. The classic case is an empty cell with\n"
      "all four neighbours blocked, which nails the 1x1. Such boards dominate the\n"
      "low end of the distribution but are not interesting as puzzles, so they are\n"
      "listed separately from the trap-free ones.\n\n"
      "Note that a board with a unique solution is trapped by definition (every\n"
      "piece is pinned), so trap-free boards necessarily have several solutions.\n\n");

    /* collect candidates below the threshold and run the freedom analysis */
    size_t ncand = 0;
    for (uint32_t i = 0; i < n; i++)
        if (canon[i] > 0 && canon[i] <= hard_threshold) ncand++;
    board_info *cand = malloc(ncand * sizeof *cand);
    size_t k = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (canon[i] == 0 || canon[i] > hard_threshold) continue;
        board_info *bi = &cand[k++];
        bi->rank  = list[i];
        bi->count = canon[i];
        bi->orbit = orb[i];
        gs_mask m = gs_unrank(list[i]);
        gs_freedom f;
        gs_solve_freedom(&T, m, &f);
        bi->pinned = 0; bi->npinned = 0;
        for (int p = 0; p < GS_NPIECES; p++)
            if (f.nplaces[p] == 1) { bi->pinned |= 1u << p; bi->npinned++; }
        gs_geom g;
        gs_geometry(m, &g);
        bi->min_comp     = g.min_comp;
        bi->n_singletons = g.n_singletons;
    }
    qsort(cand, ncand, sizeof *cand, cmp_info);
    fprintf(stderr, "  analysed %zu boards with 1..%u solutions\n", ncand, hard_threshold);

    fprintf(out, "### Fewest solutions overall (any board)\n\n");
    fprintf(out, "| pegs | solutions | orbit | rank | pinned pieces |\n|---|---:|---:|---:|---|\n");
    for (int i = 0; i < topk && (size_t)i < ncand; i++) board_line(out, &cand[i]);

    fprintf(out, "\n### Fewest solutions among trap-free boards\n\n");
    fprintf(out, "No piece is confined to a single placement in these.\n\n");
    fprintf(out, "| pegs | solutions | orbit | rank | pinned pieces |\n|---|---:|---:|---:|---|\n");
    {
        int shown = 0;
        for (size_t i = 0; i < ncand && shown < topk; i++)
            if (cand[i].npinned == 0) { board_line(out, &cand[i]); shown++; }
        if (!shown) fprintf(out, "| (none below the threshold) | | | | |\n");
    }

    fprintf(out, "\n### Fewest solutions with the 1x1 free\n\n");
    fprintf(out, "Boards where the monomino is *not* pinned, but other pieces may be.\n\n");
    fprintf(out, "| pegs | solutions | orbit | rank | pinned pieces |\n|---|---:|---:|---:|---|\n");
    {
        int shown = 0;
        for (size_t i = 0; i < ncand && shown < topk; i++)
            if (!(cand[i].pinned & (1u << GS_P_MONO))) { board_line(out, &cand[i]); shown++; }
        if (!shown) fprintf(out, "| (none below the threshold) | | | | |\n");
    }

    /* how much of the low end is trapped? */
    fprintf(out, "\n### How much of the low end is explained by traps\n\n");
    fprintf(out, "| max solutions | boards (orbits) | trapped | trap-free | trap-free %% |\n"
                 "|---:|---:|---:|---:|---:|\n");
    for (uint32_t lim = 1; lim <= hard_threshold; lim *= 4) {
        size_t tot = 0, trapped = 0;
        for (size_t i = 0; i < ncand; i++)
            if (cand[i].count <= lim) { tot++; if (cand[i].npinned) trapped++; }
        fprintf(out, "| %u | %zu | %zu | %zu | %.2f |\n",
                lim, tot, trapped, tot - trapped,
                tot ? 100.0 * (double)(tot - trapped) / (double)tot : 0.0);
    }

    /* smallest trap-free count */
    {
        uint32_t best = 0;
        for (size_t i = 0; i < ncand; i++)
            if (cand[i].npinned == 0) { best = cand[i].count; break; }
        if (best)
            fprintf(out, "\nThe smallest number of solutions attained by a trap-free "
                         "board is **%u**.\n", best);
    }

    if (opath) { fclose(out); fprintf(stderr, "  wrote %s\n", opath); }
    free(cand); free(canon); free(list); free(orb);
    return 0;
}

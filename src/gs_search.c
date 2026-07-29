#include "gs_search.h"
#include <string.h>
#include <stdlib.h>

uint32_t gs_sumset[512];
uint32_t gs_sumset_h[512][GS_PEGS + 1];
uint8_t  gs_dense_h[512][GS_PEGS + 1];

void gs_init_search(void)
{
    static int done = 0;
    if (done) return;
    done = 1;
    for (unsigned s = 0; s < 512; s++) {
        uint32_t reach = 1u;                       /* sum 0 achievable */
        int area = 0;
        for (int p = 0; p < GS_NPIECES; p++)
            if (s & (1u << p)) { reach |= reach << gs_piece_size[p];
                                 area += gs_piece_size[p]; }
        gs_sumset[s] = reach;
        uint32_t acc = reach;
        for (int h = 0; h <= GS_PEGS; h++) {
            if (h) acc |= (reach << h);
            gs_sumset_h[s][h] = acc;
            int total = area + h;                  /* cells still to place */
            uint32_t need = (total >= 31) ? 0xFFFFFFFEu
                                          : (uint32_t)(((1u << (total + 1)) - 1u) & ~1u);
            gs_dense_h[s][h] = ((acc & need) == need);
        }
    }
}

/* ------------------------------------------------------------------ */
/* pruning                                                             */
/* ------------------------------------------------------------------ */
/* Every connected component of the empty region must be exactly tiled by
 * a sub-multiset of the remaining pieces.  A necessary condition is that
 * its size is an achievable sub-multiset sum.  Cheap and very effective:
 * it kills isolated cells once the monomino is gone, size-2 pockets once
 * both the monomino and the domino are gone, and so on. */
static inline int feasible(gs_mask empty, unsigned rem)
{
    if (gs_dense_h[rem][0]) return 1;
    uint32_t ok = gs_sumset[rem];
    while (empty) {
        gs_mask comp = empty & (~empty + 1), prev;
        do { prev = comp; comp = gs_grow(comp, empty); } while (comp != prev);
        int sz = __builtin_popcountll(comp);
        if (!((ok >> sz) & 1u)) return 0;
        empty ^= comp;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* counting                                                            */
/* ------------------------------------------------------------------ */
static uint64_t rec_count(const gs_ptable *t, gs_mask filled, unsigned used)
{
    if (used == GS_ALL_PIECES) return 1;
    gs_mask empty = ~filled & GS_FULL;
    unsigned rem = GS_ALL_PIECES & ~used;
    if (!feasible(empty, rem)) return 0;

    int c = __builtin_ctzll(empty);
    uint64_t n = 0;
    unsigned r = rem;
    while (r) {
        int q = __builtin_ctz(r);
        r &= r - 1;
        const int hi = t->hi[c][q];
        for (int i = t->lo[c][q]; i < hi; i++) {
            gs_mask m = t->p[i].mask;
            if (m & filled) continue;
            n += rec_count(t, filled | m, used | (1u << q));
        }
    }
    return n;
}

uint64_t gs_solve_count(const gs_ptable *t, gs_mask pegs)
{
    return rec_count(t, pegs, 0);
}

/* ------------------------------------------------------------------ */
/* enumeration                                                         */
/* ------------------------------------------------------------------ */
typedef struct {
    const gs_ptable *t;
    gs_sol_cb cb;
    void *user;
    int stack[GS_NPIECES];
    uint64_t nsol;
    int stop;
} enum_ctx;

static void rec_enum(enum_ctx *e, gs_mask filled, unsigned used, int depth)
{
    if (e->stop) return;
    if (used == GS_ALL_PIECES) {
        e->nsol++;
        if (e->cb && e->cb(e->stack, e->user)) e->stop = 1;
        return;
    }
    gs_mask empty = ~filled & GS_FULL;
    unsigned rem = GS_ALL_PIECES & ~used;
    if (!feasible(empty, rem)) return;

    int c = __builtin_ctzll(empty);
    unsigned r = rem;
    while (r) {
        int q = __builtin_ctz(r);
        r &= r - 1;
        const int hi = e->t->hi[c][q];
        for (int i = e->t->lo[c][q]; i < hi; i++) {
            gs_mask m = e->t->p[i].mask;
            if (m & filled) continue;
            e->stack[depth] = i;
            rec_enum(e, filled | m, used | (1u << q), depth + 1);
            if (e->stop) return;
        }
    }
}

uint64_t gs_solve_enum(const gs_ptable *t, gs_mask pegs, gs_sol_cb cb, void *user)
{
    enum_ctx e;
    memset(&e, 0, sizeof e);
    e.t = t; e.cb = cb; e.user = user;
    rec_enum(&e, pegs, 0, 0);
    return e.nsol;
}

/* ------------------------------------------------------------------ */
/* freedom analysis                                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    const gs_ptable *t;
    gs_freedom *f;
    uint64_t seen[(GS_MAXPLACE + 63) / 64];
} free_ctx;

static int freedom_cb(const int idx[GS_NPIECES], void *user)
{
    free_ctx *fc = (free_ctx *)user;
    gs_freedom *f = fc->f;
    for (int d = 0; d < GS_NPIECES; d++) {
        int i = idx[d];
        int p = fc->t->p[i].piece;
        gs_mask m = fc->t->p[i].mask;
        if (!((fc->seen[i >> 6] >> (i & 63)) & 1u)) {
            fc->seen[i >> 6] |= (uint64_t)1 << (i & 63);
            if (f->nplaces[p]++ == 0) f->first[p] = m;
            f->reach[p] |= m;
        }
        f->always[p] = (f->nsol == 0) ? m : (f->always[p] & m);
    }
    f->nsol++;
    return 0;
}

uint64_t gs_solve_freedom(const gs_ptable *t, gs_mask pegs, gs_freedom *f)
{
    free_ctx fc;
    memset(&fc, 0, sizeof fc);
    memset(f, 0, sizeof *f);
    fc.t = t; fc.f = f;
    gs_solve_enum(t, pegs, freedom_cb, &fc);
    return f->nsol;
}

/* ------------------------------------------------------------------ */
/* geometry                                                            */
/* ------------------------------------------------------------------ */
static int cmp_desc(const void *a, const void *b)
{
    return *(const int *)b - *(const int *)a;
}

void gs_geometry(gs_mask pegs, gs_geom *g)
{
    memset(g, 0, sizeof *g);
    g->min_comp = GS_AREA;
    gs_mask empty = ~pegs & GS_FULL;
    while (empty) {
        gs_mask comp = empty & (~empty + 1), prev;
        do { prev = comp; comp = gs_grow(comp, empty); } while (comp != prev);
        int sz = __builtin_popcountll(comp);
        g->comp_size[g->ncomp++] = sz;
        if (sz < g->min_comp) g->min_comp = sz;
        if (sz == 1) { g->n_singletons++; g->isolated |= comp; }
        empty ^= comp;
    }
    qsort(g->comp_size, (size_t)g->ncomp, sizeof(int), cmp_desc);
}

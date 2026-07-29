#include "gs_dlx.h"
#include <stdlib.h>
#include <string.h>

#define NCOL   (GS_CELLS + GS_NPIECES)          /* 45 */
#define ROOT   NCOL
#define MAXNODE (NCOL + 1 + GS_MAXPLACE * 5 + 8)

typedef struct {
    int L[MAXNODE], R[MAXNODE], U[MAXNODE], D[MAXNODE];
    int col[MAXNODE], row[MAXNODE], size[NCOL];
    int nnode;
    const gs_ptable *t;
    gs_dlx_cb cb;
    void *user;
    int sol[GS_NPIECES];
    uint64_t nsol;
    int stop;
} dlx_t;

static void add_node(dlx_t *d, int c, int r, int *first)
{
    int n = d->nnode++;
    d->col[n] = c; d->row[n] = r;
    /* link into column c above the header */
    d->U[n] = d->U[c]; d->D[n] = c;
    d->D[d->U[c]] = n; d->U[c] = n;
    d->size[c]++;
    /* link into the row */
    if (*first < 0) { *first = n; d->L[n] = d->R[n] = n; }
    else {
        d->L[n] = d->L[*first]; d->R[n] = *first;
        d->R[d->L[*first]] = n; d->L[*first] = n;
    }
}

static void build(dlx_t *d, const gs_ptable *t)
{
    d->nnode = NCOL + 1;
    for (int c = 0; c < NCOL; c++) {
        d->L[c] = (c == 0) ? ROOT : c - 1;
        d->R[c] = (c == NCOL - 1) ? ROOT : c + 1;
        d->U[c] = d->D[c] = c;
        d->size[c] = 0;
        d->col[c] = c; d->row[c] = -1;
    }
    d->L[ROOT] = NCOL - 1; d->R[ROOT] = 0;
    d->U[ROOT] = d->D[ROOT] = ROOT;
    d->col[ROOT] = ROOT; d->row[ROOT] = -1;

    for (int r = 0; r < t->n; r++) {
        int first = -1;
        add_node(d, GS_CELLS + t->p[r].piece, r, &first);
        gs_mask m = t->p[r].mask;
        while (m) {
            int c = __builtin_ctzll(m);
            m &= m - 1;
            add_node(d, c, r, &first);
        }
    }
    d->t = t;
}

static void cover(dlx_t *d, int c)
{
    d->L[d->R[c]] = d->L[c];
    d->R[d->L[c]] = d->R[c];
    for (int i = d->D[c]; i != c; i = d->D[i])
        for (int j = d->R[i]; j != i; j = d->R[j]) {
            d->U[d->D[j]] = d->U[j];
            d->D[d->U[j]] = d->D[j];
            d->size[d->col[j]]--;
        }
}

static void uncover(dlx_t *d, int c)
{
    for (int i = d->U[c]; i != c; i = d->U[i])
        for (int j = d->L[i]; j != i; j = d->L[j]) {
            d->size[d->col[j]]++;
            d->U[d->D[j]] = j;
            d->D[d->U[j]] = j;
        }
    d->L[d->R[c]] = c;
    d->R[d->L[c]] = c;
}

static void search(dlx_t *d, int k)
{
    if (d->stop) return;
    if (d->R[ROOT] == ROOT) {
        d->nsol++;
        if (d->cb && d->cb(d->sol, d->user)) d->stop = 1;
        return;
    }
    /* Knuth's S heuristic: branch on the column with fewest remaining rows */
    int best = -1, bs = 1 << 30;
    for (int c = d->R[ROOT]; c != ROOT; c = d->R[c])
        if (d->size[c] < bs) { bs = d->size[c]; best = c; if (bs <= 1) break; }
    if (bs == 0) return;

    cover(d, best);
    for (int i = d->D[best]; i != best; i = d->D[i]) {
        d->sol[k] = d->row[i];
        for (int j = d->R[i]; j != i; j = d->R[j]) cover(d, d->col[j]);
        search(d, k + 1);
        for (int j = d->L[i]; j != i; j = d->L[j]) uncover(d, d->col[j]);
        if (d->stop) break;
    }
    uncover(d, best);
}

uint64_t gs_dlx_solve(const gs_ptable *t, gs_mask pegs, gs_dlx_cb cb, void *user)
{
    dlx_t *d = calloc(1, sizeof *d);
    if (!d) return 0;
    build(d, t);
    d->cb = cb; d->user = user;
    gs_mask m = pegs;
    while (m) { int c = __builtin_ctzll(m); m &= m - 1; cover(d, c); }
    search(d, 0);
    uint64_t n = d->nsol;
    free(d);
    return n;
}

/* gs_search.h -- single-board search: count / enumerate all packings that
 * exactly tile the 29 non-peg cells with the 9 pieces.
 */
#ifndef GS_SEARCH_H
#define GS_SEARCH_H

#include "gs_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sub-multiset-sum feasibility tables.
 *   gs_sumset[s]      : bit j set  <=>  some sub-multiset of the pieces in the
 *                       9-bit set s has total area j   (j <= 29)
 *   gs_sumset_h[s][h] : same, allowing up to h extra unit "hole" cells.
 * Built by gs_init_search(); used by the pruners.                          */
extern uint32_t gs_sumset[512];
extern uint32_t gs_sumset_h[512][GS_PEGS + 1];

/* gs_dense_h[s][h] is 1 when every size from 1 up to the total number of
 * cells still to be placed is achievable.  In that case no component size can
 * ever fail the test, so the (relatively expensive) flood fill can be skipped
 * outright.  Early in the search this is almost always true, which is what
 * makes the prune affordable. */
extern uint8_t gs_dense_h[512][GS_PEGS + 1];

void gs_init_search(void);

/* Number of distinct solutions of the board defined by `pegs`.          */
uint64_t gs_solve_count(const gs_ptable *t, gs_mask pegs);

/* Enumerate every solution.  `cb` receives the 9 placement indices (into
 * t->p[]) in the order they were laid down.  Return non-zero from cb to
 * stop early.  Returns the number of solutions visited.                 */
typedef int (*gs_sol_cb)(const int place_idx[GS_NPIECES], void *user);
uint64_t gs_solve_enum(const gs_ptable *t, gs_mask pegs, gs_sol_cb cb, void *user);

/* ------------------------------------------------------------------ */
/* structural analysis of a board                                       */
/* ------------------------------------------------------------------ */

/* Per-piece freedom across the whole solution set. */
typedef struct {
    uint32_t nplaces[GS_NPIECES];   /* # distinct placements piece p takes  */
    gs_mask  first[GS_NPIECES];     /* one such placement (if nplaces >= 1) */
    gs_mask  reach[GS_NPIECES];     /* union of cells piece p ever occupies */
    gs_mask  always[GS_NPIECES];    /* cells piece p occupies in every soln  */
    uint64_t nsol;
} gs_freedom;

/* Enumerates the full solution set; only sensible for small counts. */
uint64_t gs_solve_freedom(const gs_ptable *t, gs_mask pegs, gs_freedom *f);

/* Cheap, search-free structure of the empty region. */
typedef struct {
    int      ncomp;                 /* connected components of empty region */
    int      min_comp;              /* smallest component size              */
    int      n_singletons;          /* components of size 1 (pin the mono)  */
    gs_mask  isolated;              /* the size-1 components themselves     */
    int      comp_size[GS_AREA];    /* sizes, descending                    */
} gs_geom;
void gs_geometry(gs_mask pegs, gs_geom *g);

#ifdef __cplusplus
}
#endif
#endif

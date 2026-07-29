/* gs_dlx.h -- Knuth's Algorithm X with dancing links, for a single board.
 *
 * Exact cover formulation:
 *   columns = 36 cell constraints + 9 "use each piece exactly once"
 *             constraints                                  (45 columns)
 *   rows    = the 625 legal placements; row r has a 1 in its piece column
 *             and in each cell it occupies
 * The 7 peg columns are covered before the search starts, which simultaneously
 * satisfies them and deletes every placement that would overlap a peg.
 */
#ifndef GS_DLX_H
#define GS_DLX_H

#include "gs_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*gs_dlx_cb)(const int rows[GS_NPIECES], void *user);

/* Returns the number of solutions.  cb may be NULL.  The row indices handed
 * to cb index gs_ptable::p[].  Return non-zero from cb to stop early. */
uint64_t gs_dlx_solve(const gs_ptable *t, gs_mask pegs, gs_dlx_cb cb, void *user);

#ifdef __cplusplus
}
#endif
#endif

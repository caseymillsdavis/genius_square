/* gs_io.h -- canonical-orbit indexing and the on-disk count formats. */
#ifndef GS_IO_H
#define GS_IO_H

#include "gs_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* canonical orbit index                                               */
/* ------------------------------------------------------------------ */
/* Ascending list of the lexicographic ranks of the D4-canonical boards.
 * Deterministic, so it never needs to be stored: 1,044,690 entries.
 * Caller frees. */
uint32_t *gs_canon_list(uint32_t *n_out);

/* rank -> canonical slot, for all GS_NBOARDS ranks (33 MB).  Caller frees. */
uint32_t *gs_canon_slot(const uint32_t *list, uint32_t n);

/* Orbit size (1..8) of each canonical board, parallel to `list`. */
uint8_t  *gs_canon_orbit_sizes(const uint32_t *list, uint32_t n);

/* ------------------------------------------------------------------ */
/* file formats                                                        */
/* ------------------------------------------------------------------ */
/* "GSQF": flat little-endian uint32[GS_NBOARDS] indexed by gs_rank.    */
int       gs_write_full(const char *path, const uint32_t *counts);
uint32_t *gs_read_full (const char *path);

/* "GSQC": D4-canonical counts only, byte-plane split then deflated.
 * Roughly 8x smaller than the flat array before compression and about
 * another 2.5x after, because the high bytes of the counts are nearly
 * constant. */
int       gs_write_canon(const char *path, const uint32_t *counts_full);
uint32_t *gs_read_canon (const char *path, uint32_t *n_out);

/* Reinflate a canonical table into the full GS_NBOARDS array. */
uint32_t *gs_expand_canon(const uint32_t *canon, const uint32_t *list, uint32_t n);

/* Spot-check that counts[gs_rank(g.B)] == counts[gs_rank(B)] for random B
 * and every g in D4.  Returns 0 on success. */
int gs_check_equivariance(const uint32_t *counts_full, uint32_t nsamples);

#ifdef __cplusplus
}
#endif
#endif

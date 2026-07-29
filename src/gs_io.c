#include "gs_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* ------------------------------------------------------------------ */
/* canonical orbit index                                               */
/* ------------------------------------------------------------------ */
uint32_t *gs_canon_list(uint32_t *n_out)
{
    gs_init_all();
    uint32_t cap = 1100000, n = 0;
    uint32_t *list = malloc(cap * sizeof *list);
    if (!list) return NULL;

    /* walk the 7-subsets in lexicographic order; the loop index *is* the rank */
    int c[GS_PEGS];
    for (int i = 0; i < GS_PEGS; i++) c[i] = i;
    for (uint32_t rank = 0; rank < GS_NBOARDS; rank++) {
        gs_mask m = 0;
        for (int i = 0; i < GS_PEGS; i++) m |= (gs_mask)1 << c[i];
        if (gs_canon(m) == m) {
            if (n == cap) { cap *= 2; list = realloc(list, cap * sizeof *list); }
            list[n++] = rank;
        }
        /* odometer step */
        int i = GS_PEGS - 1;
        while (i >= 0 && c[i] == GS_CELLS - GS_PEGS + i) i--;
        if (i < 0) break;
        c[i]++;
        for (int j = i + 1; j < GS_PEGS; j++) c[j] = c[j - 1] + 1;
    }
    *n_out = n;
    return list;
}

uint32_t *gs_canon_slot(const uint32_t *list, uint32_t n)
{
    gs_init_all();
    uint32_t *slot = malloc((size_t)GS_NBOARDS * sizeof *slot);
    if (!slot) return NULL;
    /* first mark the canonical boards, then propagate over each orbit */
    for (uint32_t i = 0; i < GS_NBOARDS; i++) slot[i] = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; i++) slot[list[i]] = i;
    for (uint32_t i = 0; i < n; i++) {
        gs_mask m = gs_unrank(list[i]);
        for (int g = 1; g < GS_NSYM; g++) slot[gs_rank(gs_apply(g, m))] = i;
    }
    return slot;
}

uint8_t *gs_canon_orbit_sizes(const uint32_t *list, uint32_t n)
{
    gs_init_all();
    uint8_t *sz = malloc(n);
    if (!sz) return NULL;
    for (uint32_t i = 0; i < n; i++) {
        gs_mask orb[GS_NSYM];
        sz[i] = (uint8_t)gs_orbit(gs_unrank(list[i]), orb);
    }
    return sz;
}

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */
static int wr(FILE *f, const void *p, size_t n)
{
    return fwrite(p, 1, n, f) == n ? 0 : -1;
}
static int rd(FILE *f, void *p, size_t n)
{
    return fread(p, 1, n, f) == n ? 0 : -1;
}

static int write_deflated(FILE *f, const unsigned char *buf, size_t n)
{
    uLongf clen = compressBound((uLong)n);
    unsigned char *cbuf = malloc(clen);
    if (!cbuf) return -1;
    if (compress2(cbuf, &clen, buf, (uLong)n, 9) != Z_OK) { free(cbuf); return -1; }
    uint64_t hdr[2] = { (uint64_t)clen, (uint64_t)n };
    int rc = wr(f, hdr, sizeof hdr) || wr(f, cbuf, clen);
    free(cbuf);
    return rc;
}

static unsigned char *read_inflated(FILE *f, size_t *n_out)
{
    uint64_t hdr[2];
    if (rd(f, hdr, sizeof hdr)) return NULL;
    unsigned char *cbuf = malloc((size_t)hdr[0]);
    unsigned char *buf  = malloc((size_t)hdr[1]);
    if (!cbuf || !buf) { free(cbuf); free(buf); return NULL; }
    if (rd(f, cbuf, (size_t)hdr[0])) { free(cbuf); free(buf); return NULL; }
    uLongf dlen = (uLongf)hdr[1];
    int rc = uncompress(buf, &dlen, cbuf, (uLong)hdr[0]);
    free(cbuf);
    if (rc != Z_OK || dlen != hdr[1]) { free(buf); return NULL; }
    *n_out = (size_t)hdr[1];
    return buf;
}

/* ------------------------------------------------------------------ */
/* flat format                                                         */
/* ------------------------------------------------------------------ */
int gs_write_full(const char *path, const uint32_t *counts)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }
    uint32_t hdr[4] = { 0x46515347u /* "GSQF" */, 1u, GS_NBOARDS, 0u };
    int rc = wr(f, hdr, sizeof hdr)
          || wr(f, counts, (size_t)GS_NBOARDS * sizeof *counts);
    fclose(f);
    if (rc) fprintf(stderr, "%s: short write\n", path);
    else    fprintf(stderr, "  wrote %s (%.1f MB)\n", path,
                    (16.0 + 4.0 * GS_NBOARDS) / 1e6);
    return rc;
}

uint32_t *gs_read_full(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    uint32_t hdr[4];
    if (rd(f, hdr, sizeof hdr) || hdr[0] != 0x46515347u || hdr[2] != GS_NBOARDS) {
        fprintf(stderr, "%s: bad header\n", path); fclose(f); return NULL;
    }
    uint32_t *c = malloc((size_t)GS_NBOARDS * sizeof *c);
    if (!c || rd(f, c, (size_t)GS_NBOARDS * sizeof *c)) {
        fprintf(stderr, "%s: short read\n", path); free(c); fclose(f); return NULL;
    }
    fclose(f);
    return c;
}

/* ------------------------------------------------------------------ */
/* canonical + byte-plane + deflate                                    */
/* ------------------------------------------------------------------ */
int gs_write_canon(const char *path, const uint32_t *counts_full)
{
    uint32_t n;
    uint32_t *list = gs_canon_list(&n);
    if (!list) return -1;

    unsigned char *plane = malloc((size_t)n);
    FILE *f = fopen(path, "wb");
    if (!plane || !f) { perror(path); free(list); free(plane); if (f) fclose(f); return -1; }

    uint32_t hdr[6] = { 0x43515347u /* "GSQC" */, 1u, GS_NBOARDS, n, 4u, 0u };
    int rc = wr(f, hdr, sizeof hdr);
    for (int b = 0; b < 4 && !rc; b++) {
        for (uint32_t i = 0; i < n; i++)
            plane[i] = (unsigned char)(counts_full[list[i]] >> (8 * b));
        rc = write_deflated(f, plane, n);
    }
    long sz = ftell(f);
    fclose(f);
    free(plane);
    free(list);
    if (rc) fprintf(stderr, "%s: write failed\n", path);
    else    fprintf(stderr, "  wrote %s (%u canonical boards, %.2f MB, "
                            "%.2f bytes/board)\n",
                    path, n, sz / 1e6, (double)sz / n);
    return rc;
}

uint32_t *gs_read_canon(const char *path, uint32_t *n_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    uint32_t hdr[6];
    if (rd(f, hdr, sizeof hdr) || hdr[0] != 0x43515347u) {
        fprintf(stderr, "%s: bad header\n", path); fclose(f); return NULL;
    }
    uint32_t n = hdr[3];
    uint32_t *c = calloc(n, sizeof *c);
    if (!c) { fclose(f); return NULL; }
    for (int b = 0; b < 4; b++) {
        size_t len = 0;
        unsigned char *plane = read_inflated(f, &len);
        if (!plane || len != n) {
            fprintf(stderr, "%s: corrupt plane %d\n", path, b);
            free(plane); free(c); fclose(f); return NULL;
        }
        for (uint32_t i = 0; i < n; i++) c[i] |= (uint32_t)plane[i] << (8 * b);
        free(plane);
    }
    fclose(f);
    *n_out = n;
    return c;
}

uint32_t *gs_expand_canon(const uint32_t *canon, const uint32_t *list, uint32_t n)
{
    gs_init_all();
    uint32_t *full = malloc((size_t)GS_NBOARDS * sizeof *full);
    if (!full) return NULL;
    for (uint32_t i = 0; i < n; i++) {
        gs_mask m = gs_unrank(list[i]);
        gs_mask orb[GS_NSYM];
        int k = gs_orbit(m, orb);
        for (int j = 0; j < k; j++) full[gs_rank(orb[j])] = canon[i];
    }
    return full;
}

/* ------------------------------------------------------------------ */
int gs_check_equivariance(const uint32_t *counts_full, uint32_t nsamples)
{
    gs_init_all();
    uint64_t s = 88172645463325252ull;
    for (uint32_t k = 0; k < nsamples; k++) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        uint32_t r = (uint32_t)(s % GS_NBOARDS);
        gs_mask m = gs_unrank(r);
        uint32_t c0 = counts_full[r];
        for (int g = 1; g < GS_NSYM; g++)
            if (counts_full[gs_rank(gs_apply(g, m))] != c0) return -1;
    }
    return 0;
}

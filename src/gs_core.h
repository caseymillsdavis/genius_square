/* gs_core.h -- shared board / piece / symmetry machinery for Genius Square.
 *
 * Board: 6x6, cells numbered row-major, cell = row*6 + col, row 0 at top.
 * Algebraic names follow the physical game: columns A..F, rows 1..6, so
 * cell 0 == "A1", cell 5 == "F1", cell 35 == "F6".
 *
 * A *board* (a.k.a. puzzle) is a choice of 7 blocker cells ("pegs").
 * There are C(36,7) = 8,347,680 of them.  The remaining 29 cells must be
 * tiled exactly by the 9 pieces, whose areas sum to 29.
 */
#ifndef GS_CORE_H
#define GS_CORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GS_N            6
#define GS_CELLS        36
#define GS_PEGS         7
#define GS_NPIECES      9
#define GS_AREA         29              /* sum of piece areas          */
#define GS_NBOARDS      8347680u        /* C(36,7)                     */
#define GS_NSYM         8               /* |D4|                        */

typedef uint64_t gs_mask;               /* 36 significant bits         */
#define GS_FULL         ((gs_mask)0xFFFFFFFFFULL)

/* ------------------------------------------------------------------ */
/* pieces                                                              */
/* ------------------------------------------------------------------ */
enum {
    GS_P_MONO = 0,   /* 1x1                    */
    GS_P_DOMINO,     /* 2x1                    */
    GS_P_TRI_I,      /* 3x1                    */
    GS_P_TRI_L,      /* 3-cell L (corner)      */
    GS_P_TET_L,      /* 4-cell L               */
    GS_P_TET_S,      /* 4-cell S/Z             */
    GS_P_TET_I,      /* 4x1                    */
    GS_P_TET_O,      /* 2x2                    */
    GS_P_TET_T       /* 4-cell T               */
};
#define GS_ALL_PIECES   ((1u << GS_NPIECES) - 1u)

extern const char * const gs_piece_name[GS_NPIECES];
extern const char         gs_piece_char[GS_NPIECES];   /* for ASCII art */
extern const uint8_t      gs_piece_size[GS_NPIECES];

/* ------------------------------------------------------------------ */
/* placements                                                          */
/* ------------------------------------------------------------------ */
#define GS_MAXPLACE 1024

typedef struct {
    gs_mask mask;      /* occupied cells                                */
    uint8_t piece;     /* piece id                                      */
    uint8_t anchor;    /* lowest occupied cell index                    */
} gs_placement;

typedef struct {
    gs_placement p[GS_MAXPLACE];
    /* masks only, same indexing as p[] -- halves the bytes touched by the
     * innermost loop of the search */
    gs_mask      pmask[GS_MAXPLACE];
    int          n;
    /* p[] is sorted by (anchor, piece).  slot[cell][piece] gives the
     * half-open range [lo,hi) of placements with that anchor and piece. */
    int          lo[GS_CELLS][GS_NPIECES];
    int          hi[GS_CELLS][GS_NPIECES];
    /* all placements anchored at a cell, regardless of piece           */
    int          cell_lo[GS_CELLS], cell_hi[GS_CELLS];
} gs_ptable;

/* Build the (deterministic) placement table.  625 placements total. */
void gs_build_placements(gs_ptable *t);

/* ------------------------------------------------------------------ */
/* combinatorics: colex ranking of 7-subsets                           */
/* ------------------------------------------------------------------ */
extern uint32_t gs_binom[GS_CELLS + 1][GS_PEGS + 2];

void     gs_init_binom(void);
uint32_t gs_rank(gs_mask m);            /* mask with 7 bits -> 0..NBOARDS-1 */
gs_mask  gs_unrank(uint32_t r);

/* ------------------------------------------------------------------ */
/* symmetry: the dihedral group D4 acting on the 6x6 grid              */
/* ------------------------------------------------------------------ */
extern uint8_t gs_perm[GS_NSYM][GS_CELLS];
extern const char * const gs_sym_name[GS_NSYM];

void    gs_init_sym(void);
gs_mask gs_apply(int g, gs_mask m);
/* Canonical representative of the D4 orbit of m: the image with the
 * smallest 36-bit integer value (works for any mask, not just 7-subsets).
 * Deterministic: gs_canon(gs_apply(g,m)) == gs_canon(m) for every g. */
gs_mask gs_canon(gs_mask m);
/* Size of the orbit of m (1..8) and, optionally, the distinct images. */
int     gs_orbit(gs_mask m, gs_mask out[GS_NSYM]);

/* ------------------------------------------------------------------ */
/* misc helpers                                                        */
/* ------------------------------------------------------------------ */
void gs_init_all(void);                 /* binom + sym; idempotent      */

/* "A1" / "a1" / "0" -> cell index, or -1 */
int  gs_parse_cell(const char *s);
/* cell -> "A1" (writes 3 bytes incl. NUL) */
void gs_cell_name(int cell, char out[4]);
/* Parse a whitespace/comma separated list of 7 cells.  Returns mask or 0. */
gs_mask gs_parse_board(const char *s);

/* Render a board (pegs) plus an optional piece assignment (cell->piece id
 * or 0xFF) into a human readable grid.  buf must hold >= 512 bytes. */
void gs_render(gs_mask pegs, const uint8_t *cell_piece, char *buf, size_t cap);

/* connected-component helper: grow x inside region by one step */
static inline gs_mask gs_grow(gs_mask x, gs_mask region)
{
    const gs_mask NOT_C5 = 0x7DF7DF7DFULL; /* cells whose col != 5 */
    const gs_mask NOT_C0 = 0xFBEFBEFBEULL; /* cells whose col != 0 */
    gs_mask r = x | (x << GS_N) | (x >> GS_N)
                  | ((x & NOT_C5) << 1) | ((x & NOT_C0) >> 1);
    return r & region;
}

#ifdef __cplusplus
}
#endif
#endif /* GS_CORE_H */

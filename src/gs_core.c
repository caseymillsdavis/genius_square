#include "gs_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* piece metadata                                                      */
/* ------------------------------------------------------------------ */
const char * const gs_piece_name[GS_NPIECES] = {
    "mono1",   /* 1x1        */
    "domino2", /* 2x1        */
    "line3",   /* 3x1        */
    "ell3",    /* 3-cell L   */
    "ell4",    /* 4-cell L   */
    "ess4",    /* 4-cell S/Z */
    "line4",   /* 4x1        */
    "square4", /* 2x2        */
    "tee4"     /* 4-cell T   */
};
const char gs_piece_char[GS_NPIECES] = { '1','2','3','L','J','S','I','O','T' };
const uint8_t gs_piece_size[GS_NPIECES] = { 1,2,3,3,4,4,4,4,4 };

/* base shapes as (row,col) offsets */
static const int8_t base_shape[GS_NPIECES][4][2] = {
    { {0,0} },                            /* mono   */
    { {0,0},{0,1} },                      /* domino */
    { {0,0},{0,1},{0,2} },                /* line3  */
    { {0,0},{1,0},{1,1} },                /* ell3   */
    { {0,0},{1,0},{2,0},{2,1} },          /* ell4   */
    { {0,0},{0,1},{1,1},{1,2} },          /* ess4   */
    { {0,0},{0,1},{0,2},{0,3} },          /* line4  */
    { {0,0},{0,1},{1,0},{1,1} },          /* square */
    { {0,0},{0,1},{0,2},{1,1} }           /* tee4   */
};

/* ------------------------------------------------------------------ */
/* binomials + colex rank                                              */
/* ------------------------------------------------------------------ */
uint32_t gs_binom[GS_CELLS + 1][GS_PEGS + 2];

void gs_init_binom(void)
{
    for (int n = 0; n <= GS_CELLS; n++) {
        for (int k = 0; k <= GS_PEGS + 1; k++) {
            if (k == 0)      gs_binom[n][k] = 1;
            else if (k > n)  gs_binom[n][k] = 0;
            else             gs_binom[n][k] = gs_binom[n-1][k-1] + gs_binom[n-1][k];
        }
    }
}

/* Lexicographic rank of the sorted 7-tuple of peg cells.
 *
 * Lex (not colex) is deliberate: fixing the first j pegs confines the rank to
 * a contiguous interval of length C(35-h_j, 7-j).  The global enumeration in
 * gs_countall decides cells in increasing order, so a whole DFS subtree writes
 * into one small window of the counter array.  That turns what would be 1.2e10
 * random scattered increments into cache-resident ones.
 */
uint32_t gs_rank(gs_mask m)
{
    uint32_t r = 0;
    int prev = -1;
    for (int i = 1; i <= GS_PEGS; i++) {
        int c = __builtin_ctzll(m);
        m &= m - 1;
        r += gs_binom[35 - prev][8 - i] - gs_binom[36 - c][8 - i];
        prev = c;
    }
    return r;
}

gs_mask gs_unrank(uint32_t r)
{
    gs_mask m = 0;
    int c = 0;
    for (int i = 1; i <= GS_PEGS; i++) {
        for (;;) {
            uint32_t blk = gs_binom[35 - c][GS_PEGS - i];
            if (r < blk) break;
            r -= blk;
            c++;
        }
        m |= (gs_mask)1 << c;
        c++;
    }
    return m;
}

/* ------------------------------------------------------------------ */
/* symmetry                                                            */
/* ------------------------------------------------------------------ */
uint8_t gs_perm[GS_NSYM][GS_CELLS];
const char * const gs_sym_name[GS_NSYM] = {
    "identity", "rot90", "rot180", "rot270",
    "mirror-v", "mirror-h", "transpose", "anti-transpose"
};

void gs_init_sym(void)
{
    for (int r = 0; r < GS_N; r++)
    for (int c = 0; c < GS_N; c++) {
        int s = r * GS_N + c, R = GS_N - 1;
        gs_perm[0][s] = (uint8_t)(r * GS_N + c);              /* identity   */
        gs_perm[1][s] = (uint8_t)(c * GS_N + (R - r));        /* rot90 cw   */
        gs_perm[2][s] = (uint8_t)((R - r) * GS_N + (R - c));  /* rot180     */
        gs_perm[3][s] = (uint8_t)((R - c) * GS_N + r);        /* rot270     */
        gs_perm[4][s] = (uint8_t)(r * GS_N + (R - c));        /* mirror-v   */
        gs_perm[5][s] = (uint8_t)((R - r) * GS_N + c);        /* mirror-h   */
        gs_perm[6][s] = (uint8_t)(c * GS_N + r);              /* transpose  */
        gs_perm[7][s] = (uint8_t)((R - c) * GS_N + (R - r));  /* anti-diag  */
    }
}

gs_mask gs_apply(int g, gs_mask m)
{
    const uint8_t *p = gs_perm[g];
    gs_mask out = 0;
    while (m) {
        int c = __builtin_ctzll(m);
        m &= m - 1;
        out |= (gs_mask)1 << p[c];
    }
    return out;
}

/* Order masks by the lexicographic order of their sorted cell lists: at the
 * lowest cell where they differ, the mask that *contains* that cell is the
 * smaller one.  For 7-subsets this coincides with the order of gs_rank, so a
 * canonical board always has the smallest rank in its orbit. */
static inline int lex_less(gs_mask a, gs_mask b)
{
    gs_mask d = a ^ b;
    if (!d) return 0;
    return (a & (d & (~d + 1))) != 0;
}

gs_mask gs_canon(gs_mask m)
{
    gs_mask best = m;
    for (int g = 1; g < GS_NSYM; g++) {
        gs_mask x = gs_apply(g, m);
        if (lex_less(x, best)) best = x;
    }
    return best;
}

int gs_orbit(gs_mask m, gs_mask out[GS_NSYM])
{
    int n = 0;
    for (int g = 0; g < GS_NSYM; g++) {
        gs_mask x = gs_apply(g, m);
        int dup = 0;
        for (int i = 0; i < n; i++) if (out[i] == x) { dup = 1; break; }
        if (!dup) out[n++] = x;
    }
    return n;
}

void gs_init_all(void)
{
    static int done = 0;
    if (done) return;
    done = 1;
    gs_init_binom();
    gs_init_sym();
}

/* ------------------------------------------------------------------ */
/* placement generation                                                */
/* ------------------------------------------------------------------ */
typedef struct { int8_t rc[4][2]; int n; } shape_t;

static void normalize(shape_t *s)
{
    int mr = 127, mc = 127;
    for (int i = 0; i < s->n; i++) { if (s->rc[i][0] < mr) mr = s->rc[i][0];
                                     if (s->rc[i][1] < mc) mc = s->rc[i][1]; }
    for (int i = 0; i < s->n; i++) { s->rc[i][0] -= mr; s->rc[i][1] -= mc; }
    /* sort cells row-major so that identical shapes compare equal */
    for (int i = 1; i < s->n; i++)
        for (int j = i; j > 0; j--) {
            int a = s->rc[j][0]   * 8 + s->rc[j][1];
            int b = s->rc[j-1][0] * 8 + s->rc[j-1][1];
            if (a < b) { int8_t t0 = s->rc[j][0], t1 = s->rc[j][1];
                         s->rc[j][0] = s->rc[j-1][0]; s->rc[j][1] = s->rc[j-1][1];
                         s->rc[j-1][0] = t0;          s->rc[j-1][1] = t1; }
            else break;
        }
}

static int same(const shape_t *a, const shape_t *b)
{
    if (a->n != b->n) return 0;
    for (int i = 0; i < a->n; i++)
        if (a->rc[i][0] != b->rc[i][0] || a->rc[i][1] != b->rc[i][1]) return 0;
    return 1;
}

/* apply the g-th element of D4 to a (row,col) offset pair */
static void xform(int g, int r, int c, int *or_, int *oc)
{
    switch (g) {
        case 0: *or_ =  r; *oc =  c; break;   /* identity  */
        case 1: *or_ =  c; *oc = -r; break;   /* rot90     */
        case 2: *or_ = -r; *oc = -c; break;   /* rot180    */
        case 3: *or_ = -c; *oc =  r; break;   /* rot270    */
        case 4: *or_ =  r; *oc = -c; break;   /* mirror-v  */
        case 5: *or_ = -r; *oc =  c; break;   /* mirror-h  */
        case 6: *or_ =  c; *oc =  r; break;   /* transpose */
        default:*or_ = -c; *oc = -r; break;   /* anti-diag */
    }
}

static int cmp_place(const void *A, const void *B)
{
    const gs_placement *a = A, *b = B;
    if (a->anchor != b->anchor) return (int)a->anchor - (int)b->anchor;
    if (a->piece  != b->piece)  return (int)a->piece  - (int)b->piece;
    return (a->mask < b->mask) ? -1 : (a->mask > b->mask);
}

void gs_build_placements(gs_ptable *t)
{
    t->n = 0;
    for (int pc = 0; pc < GS_NPIECES; pc++) {
        int nc = gs_piece_size[pc];
        shape_t orient[8];
        int no = 0;
        for (int g = 0; g < GS_NSYM; g++) {
            shape_t s; s.n = nc;
            for (int i = 0; i < nc; i++) {
                int r2, c2;
                xform(g, base_shape[pc][i][0], base_shape[pc][i][1], &r2, &c2);
                s.rc[i][0] = (int8_t)r2; s.rc[i][1] = (int8_t)c2;
            }
            normalize(&s);
            int dup = 0;
            for (int i = 0; i < no; i++) if (same(&s, &orient[i])) { dup = 1; break; }
            if (!dup) orient[no++] = s;
        }
        for (int o = 0; o < no; o++) {
            int h = 0, w = 0;
            for (int i = 0; i < nc; i++) {
                if (orient[o].rc[i][0] + 1 > h) h = orient[o].rc[i][0] + 1;
                if (orient[o].rc[i][1] + 1 > w) w = orient[o].rc[i][1] + 1;
            }
            for (int r0 = 0; r0 + h <= GS_N; r0++)
            for (int c0 = 0; c0 + w <= GS_N; c0++) {
                gs_mask m = 0;
                for (int i = 0; i < nc; i++)
                    m |= (gs_mask)1 << ((r0 + orient[o].rc[i][0]) * GS_N
                                       + (c0 + orient[o].rc[i][1]));
                gs_placement *p = &t->p[t->n++];
                p->mask   = m;
                p->piece  = (uint8_t)pc;
                p->anchor = (uint8_t)__builtin_ctzll(m);
            }
        }
    }
    qsort(t->p, (size_t)t->n, sizeof(gs_placement), cmp_place);
    for (int i = 0; i < t->n; i++) t->pmask[i] = t->p[i].mask;

    for (int c = 0; c < GS_CELLS; c++) {
        t->cell_lo[c] = t->cell_hi[c] = 0;
        for (int q = 0; q < GS_NPIECES; q++) t->lo[c][q] = t->hi[c][q] = 0;
    }
    int i = 0;
    for (int c = 0; c < GS_CELLS; c++) {
        t->cell_lo[c] = i;
        for (int q = 0; q < GS_NPIECES; q++) {
            t->lo[c][q] = i;
            while (i < t->n && t->p[i].anchor == c && t->p[i].piece == q) i++;
            t->hi[c][q] = i;
        }
        t->cell_hi[c] = i;
    }
}

/* ------------------------------------------------------------------ */
/* parsing / rendering                                                 */
/* ------------------------------------------------------------------ */
int gs_parse_cell(const char *s)
{
    if (!s || !*s) return -1;
    if (isalpha((unsigned char)s[0])) {
        int col = tolower((unsigned char)s[0]) - 'a';
        if (col < 0 || col >= GS_N) return -1;
        if (!isdigit((unsigned char)s[1])) return -1;
        int row = s[1] - '1';
        if (row < 0 || row >= GS_N) return -1;
        if (s[2] != '\0') return -1;
        return row * GS_N + col;
    }
    char *end;
    long v = strtol(s, &end, 10);
    if (*end != '\0' || v < 0 || v >= GS_CELLS) return -1;
    return (int)v;
}

void gs_cell_name(int cell, char out[4])
{
    out[0] = (char)('A' + cell % GS_N);
    out[1] = (char)('1' + cell / GS_N);
    out[2] = '\0';
    out[3] = '\0';
}

gs_mask gs_parse_board(const char *s)
{
    char buf[256];
    snprintf(buf, sizeof buf, "%s", s);
    for (char *p = buf; *p; p++) if (*p == ',' || *p == ';') *p = ' ';
    gs_mask m = 0;
    int n = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, " \t\n", &save); tok;
         tok = strtok_r(NULL, " \t\n", &save)) {
        int c = gs_parse_cell(tok);
        if (c < 0) return 0;
        if (m & ((gs_mask)1 << c)) return 0;   /* duplicate */
        m |= (gs_mask)1 << c;
        n++;
    }
    return (n == GS_PEGS) ? m : 0;
}

void gs_render(gs_mask pegs, const uint8_t *cell_piece, char *buf, size_t cap)
{
    size_t k = 0;
    k += (size_t)snprintf(buf + k, cap - k, "   A B C D E F\n");
    for (int r = 0; r < GS_N; r++) {
        k += (size_t)snprintf(buf + k, cap - k, "%d ", r + 1);
        for (int c = 0; c < GS_N; c++) {
            int cell = r * GS_N + c;
            char ch;
            if (pegs & ((gs_mask)1 << cell))      ch = '#';
            else if (cell_piece && cell_piece[cell] < GS_NPIECES)
                                                  ch = gs_piece_char[cell_piece[cell]];
            else                                  ch = '.';
            k += (size_t)snprintf(buf + k, cap - k, " %c", ch);
        }
        k += (size_t)snprintf(buf + k, cap - k, "\n");
    }
}

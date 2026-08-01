/* gs.js -- the Genius Square engine in the browser.
 *
 * A direct port of src/gs_core.c and src/gs_search.c: same cell numbering,
 * same piece list, same placement table, same DFS.  The placement table is
 * sorted by (anchor, piece, mask) exactly as in C, and the search picks the
 * lowest empty cell and iterates pieces in bit order, so this file visits
 * solutions in *the same order* as `bin/gs_solve --all`.  web/verify.mjs
 * checks that against the real binary; keep the two in step.
 *
 * Masks are 36 bits, which does not fit JavaScript's 32-bit bitwise ops, so
 * every mask is a pair: `lo` holds cells 0..31 (as an int32) and `hi` holds
 * cells 32..35 in its low nibble.  The hot paths pass the two halves as
 * separate arguments and never allocate.
 */

export const N        = 6;
export const CELLS    = 36;
export const NPEGS    = 7;
export const NPIECES  = 9;
export const AREA     = 29;
export const NBOARDS  = 8347680;

export const PIECE_NAME = ["mono1", "domino2", "line3", "ell3", "ell4",
                           "ess4", "line4", "square4", "tee4"];
export const PIECE_LABEL = ["1x1", "2x1", "3x1", "L-tromino", "L-tetromino",
                            "S-tetromino", "I-tetromino", "2x2", "T-tetromino"];
export const PIECE_CHAR = ["1", "2", "3", "L", "J", "S", "I", "O", "T"];
export const PIECE_SIZE = [1, 2, 3, 3, 4, 4, 4, 4, 4];

const ALL_PIECES = (1 << NPIECES) - 1;
const FULL_LO = -1;          /* 0xFFFFFFFF as int32 */
const FULL_HI = 0xF;

/* base shapes as (row,col) offsets -- must match base_shape[] in gs_core.c */
const BASE_SHAPE = [
    [[0, 0]],                                   /* mono   */
    [[0, 0], [0, 1]],                           /* domino */
    [[0, 0], [0, 1], [0, 2]],                   /* line3  */
    [[0, 0], [1, 0], [1, 1]],                   /* ell3   */
    [[0, 0], [1, 0], [2, 0], [2, 1]],           /* ell4   */
    [[0, 0], [0, 1], [1, 1], [1, 2]],           /* ess4   */
    [[0, 0], [0, 1], [0, 2], [0, 3]],           /* line4  */
    [[0, 0], [0, 1], [1, 0], [1, 1]],           /* square */
    [[0, 0], [0, 1], [0, 2], [1, 1]]            /* tee4   */
];

/* ------------------------------------------------------------------ */
/* 36-bit mask helpers                                                 */
/* ------------------------------------------------------------------ */
const NOTC5_LO = 0xDF7DF7DF | 0, NOTC5_HI = 0x7;   /* cells with col != 5 */
const NOTC0_LO = 0xBEFBEFBE | 0, NOTC0_HI = 0xF;   /* cells with col != 0 */

function ctz32(x) { return 31 - Math.clz32(x & -x); }

function popcount32(x) {
    x = x - ((x >>> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >>> 2) & 0x33333333);
    x = (x + (x >>> 4)) & 0x0F0F0F0F;
    return (x * 0x01010101) >>> 24;
}

/* Lowest set cell of a non-empty pair. */
function lowestCell(lo, hi) { return lo !== 0 ? ctz32(lo) : 32 + ctz32(hi); }

/* One step of the flood fill, result left in _gLo/_gHi (see gs_grow). */
let _gLo = 0, _gHi = 0;
function grow(xLo, xHi, regLo, regHi) {
    const upLo = xLo << 6;
    const upHi = ((xHi << 6) | (xLo >>> 26)) & 0xF;
    const dnLo = (xLo >>> 6) | (xHi << 26);
    const dnHi = xHi >>> 6;
    const aLo = xLo & NOTC5_LO, aHi = xHi & NOTC5_HI;
    const rtLo = aLo << 1, rtHi = ((aHi << 1) | (aLo >>> 31)) & 0xF;
    const bLo = xLo & NOTC0_LO, bHi = xHi & NOTC0_HI;
    const lfLo = (bLo >>> 1) | (bHi << 31), lfHi = bHi >>> 1;
    _gLo = (xLo | upLo | dnLo | rtLo | lfLo) & regLo;
    _gHi = (xHi | upHi | dnHi | rtHi | lfHi) & regHi & 0xF;
}

export function maskFromCells(cells) {
    let lo = 0, hi = 0;
    for (const c of cells) {
        if (c < 32) lo |= 1 << c; else hi |= 1 << (c - 32);
    }
    return [lo | 0, hi & 0xF];
}

export function cellsOfMask(lo, hi) {
    const out = [];
    for (let c = 0; c < 32; c++) if ((lo >>> c) & 1) out.push(c);
    for (let c = 0; c < 4; c++) if ((hi >>> c) & 1) out.push(32 + c);
    return out;
}

/* ------------------------------------------------------------------ */
/* placement table                                                     */
/* ------------------------------------------------------------------ */
function xform(g, r, c) {
    switch (g) {
        case 0: return [ r,  c];
        case 1: return [ c, -r];
        case 2: return [-r, -c];
        case 3: return [-c,  r];
        case 4: return [ r, -c];
        case 5: return [-r,  c];
        case 6: return [ c,  r];
        default: return [-c, -r];
    }
}

function normalize(cells) {
    let mr = Infinity, mc = Infinity;
    for (const [r, c] of cells) { if (r < mr) mr = r; if (c < mc) mc = c; }
    const out = cells.map(([r, c]) => [r - mr, c - mc]);
    out.sort((a, b) => (a[0] * 8 + a[1]) - (b[0] * 8 + b[1]));
    return out;
}

function shapeKey(cells) { return cells.map(([r, c]) => r + "," + c).join(" "); }

function buildPlacements() {
    const rec = [];
    for (let pc = 0; pc < NPIECES; pc++) {
        const seen = new Set(), orients = [];
        for (let g = 0; g < 8; g++) {
            const s = normalize(BASE_SHAPE[pc].map(([r, c]) => xform(g, r, c)));
            const k = shapeKey(s);
            if (!seen.has(k)) { seen.add(k); orients.push(s); }
        }
        for (const o of orients) {
            let h = 0, w = 0;
            for (const [r, c] of o) { if (r + 1 > h) h = r + 1; if (c + 1 > w) w = c + 1; }
            for (let r0 = 0; r0 + h <= N; r0++)
            for (let c0 = 0; c0 + w <= N; c0++) {
                const cells = o.map(([r, c]) => (r0 + r) * N + (c0 + c));
                const [lo, hi] = maskFromCells(cells);
                rec.push({ lo, hi, piece: pc, anchor: Math.min(...cells) });
            }
        }
    }
    /* same order as cmp_place() in gs_core.c: (anchor, piece, mask) */
    rec.sort((a, b) => a.anchor - b.anchor || a.piece - b.piece ||
                       (a.hi >>> 0) - (b.hi >>> 0) || (a.lo >>> 0) - (b.lo >>> 0));

    const n = rec.length;
    const t = {
        n,
        plo: new Int32Array(n),
        phi: new Int32Array(n),
        piece: new Uint8Array(n),
        anchor: new Uint8Array(n),
        lo: new Int32Array(CELLS * NPIECES),
        hi: new Int32Array(CELLS * NPIECES)
    };
    for (let i = 0; i < n; i++) {
        t.plo[i] = rec[i].lo; t.phi[i] = rec[i].hi;
        t.piece[i] = rec[i].piece; t.anchor[i] = rec[i].anchor;
    }
    let i = 0;
    for (let c = 0; c < CELLS; c++)
        for (let q = 0; q < NPIECES; q++) {
            t.lo[c * NPIECES + q] = i;
            while (i < n && t.anchor[i] === c && t.piece[i] === q) i++;
            t.hi[c * NPIECES + q] = i;
        }
    return t;
}

export const T = buildPlacements();      /* 625 placements */

/* ------------------------------------------------------------------ */
/* sub-multiset-sum feasibility tables (gs_init_search)                */
/* ------------------------------------------------------------------ */
const SUMSET = new Int32Array(512);
const DENSE0 = new Uint8Array(512);
(function initSearch() {
    for (let s = 0; s < 512; s++) {
        let reach = 1, area = 0;
        for (let p = 0; p < NPIECES; p++)
            if (s & (1 << p)) { reach |= reach << PIECE_SIZE[p]; area += PIECE_SIZE[p]; }
        SUMSET[s] = reach;
        /* h = 0 is the only row the single-board search uses */
        const need = area >= 31 ? 0xFFFFFFFE | 0 : (((1 << (area + 1)) - 1) & ~1);
        DENSE0[s] = ((reach & need) === need) ? 1 : 0;
    }
})();

/* Every connected component of the empty region must have a size that some
 * sub-multiset of the remaining pieces can add up to. */
function feasible(emptyLo, emptyHi, rem) {
    if (DENSE0[rem]) return true;
    const ok = SUMSET[rem];
    let eLo = emptyLo, eHi = emptyHi;
    while (eLo !== 0 || eHi !== 0) {
        let cLo, cHi;
        if (eLo !== 0) { cLo = eLo & -eLo; cHi = 0; }
        else           { cLo = 0; cHi = eHi & -eHi; }
        for (;;) {
            grow(cLo, cHi, eLo, eHi);
            if (_gLo === cLo && _gHi === cHi) break;
            cLo = _gLo; cHi = _gHi;
        }
        const sz = popcount32(cLo) + popcount32(cHi);
        if (sz > 29 || !((ok >>> sz) & 1)) return false;
        eLo ^= cLo; eHi ^= cHi;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* the search                                                          */
/* ------------------------------------------------------------------ */
/* One DFS pass answers everything the page shows: the exact solution count,
 * the first `keep` solutions, and per-piece freedom (how many distinct
 * placements each piece takes over the whole solution set).  A piece that
 * takes exactly one is pinned by the pegs -- the "trap" of docs/HARDNESS.md. */
export function solve(pegLo, pegHi, keep = 200) {
    const plo = T.plo, phi = T.phi, tlo = T.lo, thi = T.hi;
    const stack = new Int32Array(NPIECES);
    const kept = [];
    const seen = new Uint8Array(T.n);
    const nplaces = new Int32Array(NPIECES);
    const firstPlace = new Int32Array(NPIECES).fill(-1);
    const reachLo = new Int32Array(NPIECES), reachHi = new Int32Array(NPIECES);
    let count = 0;

    function record() {
        for (let d = 0; d < NPIECES; d++) {
            const i = stack[d], p = T.piece[i];
            if (!seen[i]) {
                seen[i] = 1;
                if (nplaces[p]++ === 0) firstPlace[p] = i;
                reachLo[p] |= plo[i]; reachHi[p] |= phi[i];
            }
        }
        if (kept.length < keep) {
            const cp = new Uint8Array(CELLS).fill(0xFF);
            for (let d = 0; d < NPIECES; d++) {
                const i = stack[d];
                for (const c of cellsOfMask(plo[i], phi[i])) cp[c] = T.piece[i];
            }
            kept.push(cp);
        }
    }

    function rec(filledLo, filledHi, used, depth) {
        if (used === ALL_PIECES) { count++; record(); return; }
        const emptyLo = ~filledLo, emptyHi = ~filledHi & FULL_HI;
        const rem = ALL_PIECES & ~used;
        if (!feasible(emptyLo, emptyHi, rem)) return;

        const c = lowestCell(emptyLo, emptyHi);
        let r = rem;
        while (r !== 0) {
            const q = ctz32(r);
            r &= r - 1;
            const end = thi[c * NPIECES + q];
            for (let i = tlo[c * NPIECES + q]; i < end; i++) {
                const mLo = plo[i], mHi = phi[i];
                if ((mLo & filledLo) !== 0 || (mHi & filledHi) !== 0) continue;
                stack[depth] = i;
                rec(filledLo | mLo, filledHi | mHi, used | (1 << q), depth + 1);
            }
        }
    }

    rec(pegLo | 0, pegHi & FULL_HI, 0, 0);

    const freedom = [];
    for (let p = 0; p < NPIECES; p++)
        freedom.push({
            piece: p,
            nplaces: nplaces[p],
            pinned: count > 0 && nplaces[p] === 1,
            first: firstPlace[p] >= 0 ? [plo[firstPlace[p]], phi[firstPlace[p]]] : null,
            reach: [reachLo[p], reachHi[p]]
        });
    return { count, solutions: kept, freedom, truncated: count > kept.length };
}

/* ------------------------------------------------------------------ */
/* geometry of the empty region (gs_geometry)                          */
/* ------------------------------------------------------------------ */
export function geometry(pegLo, pegHi) {
    let eLo = ~pegLo, eHi = ~pegHi & FULL_HI;
    const sizes = [];
    let isolated = [];
    while (eLo !== 0 || eHi !== 0) {
        let cLo, cHi;
        if (eLo !== 0) { cLo = eLo & -eLo; cHi = 0; }
        else           { cLo = 0; cHi = eHi & -eHi; }
        for (;;) {
            grow(cLo, cHi, eLo, eHi);
            if (_gLo === cLo && _gHi === cHi) break;
            cLo = _gLo; cHi = _gHi;
        }
        const sz = popcount32(cLo) + popcount32(cHi);
        sizes.push(sz);
        if (sz === 1) isolated.push(cellsOfMask(cLo, cHi)[0]);
        eLo ^= cLo; eHi ^= cHi;
    }
    sizes.sort((a, b) => b - a);
    return { ncomp: sizes.length, sizes, isolated };
}

/* ------------------------------------------------------------------ */
/* ranking and symmetry (gs_rank / gs_canon / gs_orbit)                */
/* ------------------------------------------------------------------ */
const BINOM = [];
for (let n = 0; n <= CELLS; n++) {
    BINOM[n] = [];
    for (let k = 0; k <= NPEGS + 1; k++)
        BINOM[n][k] = k === 0 ? 1 : k > n ? 0 : BINOM[n - 1][k - 1] + BINOM[n - 1][k];
}

/* Lexicographic rank of the sorted 7-tuple of peg cells. */
export function rank(cells) {
    const s = [...cells].sort((a, b) => a - b);
    let r = 0, prev = -1;
    for (let i = 1; i <= NPEGS; i++) {
        const c = s[i - 1];
        r += BINOM[35 - prev][8 - i] - BINOM[36 - c][8 - i];
        prev = c;
    }
    return r;
}

const PERM = [];
for (let g = 0; g < 8; g++) PERM.push(new Uint8Array(CELLS));
for (let r = 0; r < N; r++)
for (let c = 0; c < N; c++) {
    const s = r * N + c, R = N - 1;
    PERM[0][s] = r * N + c;
    PERM[1][s] = c * N + (R - r);
    PERM[2][s] = (R - r) * N + (R - c);
    PERM[3][s] = (R - c) * N + r;
    PERM[4][s] = r * N + (R - c);
    PERM[5][s] = (R - r) * N + c;
    PERM[6][s] = c * N + r;
    PERM[7][s] = (R - c) * N + (R - r);
}

export function applySym(g, cells) { return cells.map(c => PERM[g][c]); }

/* Canonical representative: the image whose sorted cell list is
 * lexicographically smallest, which for 7-subsets is the smallest rank. */
export function canon(cells) {
    let best = null;
    for (let g = 0; g < 8; g++) {
        const img = applySym(g, cells).sort((a, b) => a - b);
        if (best === null || cmpCellLists(img, best) < 0) best = img;
    }
    return best;
}

function cmpCellLists(a, b) {
    for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return a[i] - b[i];
    return 0;
}

export function orbitSize(cells) {
    const seen = new Set();
    for (let g = 0; g < 8; g++)
        seen.add(applySym(g, cells).sort((a, b) => a - b).join(","));
    return seen.size;
}

/* ------------------------------------------------------------------ */
/* naming and parsing (gs_parse_cell / gs_cell_name / gs_parse_board)  */
/* ------------------------------------------------------------------ */
export function cellName(cell) {
    return String.fromCharCode(65 + (cell % N)) + String(1 + Math.floor(cell / N));
}

export function parseCell(s) {
    if (!s) return -1;
    if (/^[A-Fa-f][1-6]$/.test(s))
        return (s.charCodeAt(1) - 49) * N + (s.toLowerCase().charCodeAt(0) - 97);
    if (/^\d+$/.test(s)) { const v = +s; return v >= 0 && v < CELLS ? v : -1; }
    return -1;
}

/* Returns { cells } or { error }.  Accepts any mix of spaces and commas. */
export function parseBoard(s) {
    const toks = String(s).split(/[\s,;]+/).filter(x => x.length);
    const cells = [];
    for (const tok of toks) {
        const c = parseCell(tok);
        if (c < 0) return { error: `"${tok}" is not a cell (use A1..F6)` };
        if (cells.includes(c)) return { error: `${cellName(c)} listed twice` };
        cells.push(c);
    }
    if (cells.length !== NPEGS)
        return { error: `need exactly ${NPEGS} pegs, got ${cells.length}`, cells };
    return { cells };
}

export function formatBoard(cells) {
    return [...cells].sort((a, b) => a - b).map(cellName).join(" ");
}

/* ------------------------------------------------------------------ */
/* where this board sits in the global distribution                    */
/* ------------------------------------------------------------------ */
/* Histogram of all 8,347,680 boards by solution count, copied from
 * docs/RESULTS.md (generated by gs_stats from data/counts.gsc).
 * [upper bound of bucket, boards in bucket, cumulative boards]. */
export const HISTOGRAM = [
    [0,      172440,  172440],
    [1,         800,  173240],
    [3,        2652,  175892],
    [7,        8416,  184308],
    [15,      25636,  209944],
    [31,      75992,  285936],
    [63,     210584,  496520],
    [127,    512308, 1008828],
    [255,    921320, 1930148],
    [511,   1395264, 3325412],
    [1023,  1779672, 5105084],
    [2047,  1659352, 6764436],
    [4095,  1035956, 7800392],
    [8191,   418664, 8219056],
    [16383,  109376, 8328432],
    [32767,   17864, 8346296],
    [65535,    1368, 8347664],
    [100593,     16, 8347680]
];
export const MEDIAN_SOLUTIONS = 719;
export const MAX_SOLUTIONS = 100593;

/* The bucket a count falls in, with the share of boards at or below it. */
export function bucketOf(count) {
    for (let i = 0; i < HISTOGRAM.length; i++) {
        const [upper, boards, cum] = HISTOGRAM[i];
        if (count <= upper) {
            const lower = i === 0 ? 0 : HISTOGRAM[i - 1][0] + 1;
            return {
                lower, upper, boards, cum,
                sharePct: 100 * boards / NBOARDS,
                cumPct: 100 * cum / NBOARDS
            };
        }
    }
    return null;
}

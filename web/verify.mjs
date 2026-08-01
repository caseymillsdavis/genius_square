/* verify.mjs -- cross-check web/gs.js against the C engine.
 *
 *   make            # need bin/gs_solve
 *   node web/verify.mjs [nboards]
 *
 * node is not a dependency of the project; this is an optional check that
 * exists because web/gs.js is a hand port of gs_core.c + gs_search.c and
 * nothing else would catch a divergence.  For each random board it compares:
 *
 *   - the solution count against `gs_solve --count`
 *   - the first solution's grid against `gs_solve` (same DFS order)
 *   - the board rank and canonical rank against `gs_solve`
 *   - the empty-region component sizes against `gs_solve`
 *   - the per-piece pinned set against `gs_solve --freedom`
 *
 * plus, engine-internally, that the count is D4-invariant -- the invariant
 * AGENTS.md calls the most useful one in the codebase.
 */
import { execFileSync } from "node:child_process";
import * as gs from "./gs.js";

const BIN = new URL("../bin/gs_solve", import.meta.url).pathname;
const NBOARDS = Number(process.argv[2] || 200);

/* deterministic PRNG so a failure is reproducible */
let seed = 0x9e3779b9;
function rnd() {
    seed ^= seed << 13; seed |= 0;
    seed ^= seed >>> 17;
    seed ^= seed << 5;  seed |= 0;
    return (seed >>> 0) / 4294967296;
}

function randomBoard() {
    const cells = [];
    while (cells.length < gs.NPEGS) {
        const c = Math.floor(rnd() * gs.CELLS);
        if (!cells.includes(c)) cells.push(c);
    }
    return cells.sort((a, b) => a - b);
}

function cSolve(args) {
    return execFileSync(BIN, args, { encoding: "utf8" });
}

/* The 6-row ASCII grid gs_render() prints, as a single normalised string. */
function gridFrom(text) {
    const rows = text.split("\n").filter(l => /^[1-6] /.test(l));
    return rows.slice(0, 6).map(l => l.slice(2).replace(/ /g, "")).join("/");
}

function jsGrid(pegCells, cp) {
    const pegs = new Set(pegCells);
    let out = [];
    for (let r = 0; r < 6; r++) {
        let row = "";
        for (let c = 0; c < 6; c++) {
            const cell = r * 6 + c;
            row += pegs.has(cell) ? "#"
                 : cp && cp[cell] < 9 ? gs.PIECE_CHAR[cp[cell]] : ".";
        }
        out.push(row);
    }
    return out.join("/");
}

/* Random boards are ~98% solvable, so the interesting cases are pinned here:
 * unsolvable boards, unique-solution (fully trapped) boards, the trap-free
 * minimum, and the board with the most solutions.  See docs/RESULTS.md. */
const FIXED = [
    "25 11 29 4 34 20 13",          /* unsolvable */
    "9 28 11 14 19 7 21",           /* unsolvable */
    "1 18 25 6 13 27 3",            /* unsolvable */
    "A1 C1 B2 D2 B3 A4 B5",         /* 1 solution, every piece pinned */
    "A1 C1 F2 C4 D5 C6 E6",         /* 1 solution */
    "C1 B2 E2 C4 F4 B5 E5",         /* 2 solutions, trap-free */
    "B2 C2 E2 D3 C4 B5 E5",         /* 2 solutions, trap-free */
    "A1 B1 C1 D1 E1 F1 A2",         /* 100593 solutions, the maximum */
    "A1 C1 E1 B4 D4 F4 C6"          /* the README's example board */
];

let fail = 0, nsolvable = 0;
const t0 = Date.now();

for (let b = 0; b < NBOARDS + FIXED.length; b++) {
    const cells = b < FIXED.length
        ? FIXED[b].split(" ").map(gs.parseCell).sort((x, y) => x - y)
        : randomBoard();
    const names = cells.map(gs.cellName);
    const [lo, hi] = gs.maskFromCells(cells);
    const res = gs.solve(lo, hi, 1);

    const err = (what, got, want) => {
        console.error(`FAIL ${names.join(" ")}: ${what}\n  js: ${got}\n   c: ${want}`);
        fail++;
    };

    /* count */
    const cCount = Number(cSolve(["--count", ...names]).trim());
    if (cCount !== res.count) err("solution count", res.count, cCount);
    if (cCount > 0) nsolvable++;

    /* rank, canonical rank, components, first solution */
    const text = cSolve(names);
    const m = text.match(/rank (\d+), canonical rank (\d+)/);
    if (Number(m[1]) !== gs.rank(cells)) err("rank", gs.rank(cells), m[1]);
    const cRank = gs.rank(gs.canon(cells));
    if (Number(m[2]) !== cRank) err("canonical rank", cRank, m[2]);

    const comps = text.match(/component\(s\), sizes ([\d ]+)/)[1].trim()
                      .split(/\s+/).map(Number);
    const geom = gs.geometry(lo, hi);
    if (comps.join(",") !== geom.sizes.join(","))
        err("component sizes", geom.sizes.join(","), comps.join(","));

    if (cCount > 0) {
        const want = gridFrom(text.split("solution 1:")[1]);
        const got = jsGrid(cells, res.solutions[0]);
        if (want !== got) err("first solution (DFS order)", got, want);
    }

    /* pinned pieces */
    const ftext = cSolve(["--freedom", ...names]);
    const cPinned = [...ftext.matchAll(/^(\S+)\s+(\d+)\s+PINNED/gm)].map(x => x[1]).sort();
    const jsPinned = res.freedom.filter(f => f.pinned)
                         .map(f => gs.PIECE_NAME[f.piece]).sort();
    if (cPinned.join(",") !== jsPinned.join(","))
        err("pinned pieces", jsPinned.join(","), cPinned.join(","));

    /* counts must be constant on the D4 orbit */
    for (let g = 1; g < 8; g++) {
        const img = gs.applySym(g, cells);
        const [ilo, ihi] = gs.maskFromCells(img);
        const n = gs.solve(ilo, ihi, 0).count;
        if (n !== res.count) err(`D4-invariance under ${g}`, n, res.count);
    }
}

/* worst case for the page: the board with the most solutions */
const worst = "A1 B1 C1 D1 E1 F1 A2".split(" ").map(gs.parseCell);
const tw = Date.now();
const w = gs.solve(...gs.maskFromCells(worst), 200);
const wms = Date.now() - tw;
if (w.count !== gs.MAX_SOLUTIONS) {
    console.error(`FAIL worst board: ${w.count} != ${gs.MAX_SOLUTIONS}`);
    fail++;
}

console.log(`${NBOARDS} random boards (${nsolvable} solvable) in ` +
            `${((Date.now() - t0) / 1000).toFixed(1)}s`);
console.log(`placements: ${gs.T.n} (expect 625)`);
console.log(`worst board (${gs.MAX_SOLUTIONS} solutions): ${wms} ms in JS`);
if (gs.T.n !== 625) { console.error("FAIL placement count"); fail++; }
console.log(fail ? `*** ${fail} FAILURE(S) ***` : "js engine agrees with the C engine");
process.exit(fail ? 1 : 0);

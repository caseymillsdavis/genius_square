/* app.js -- the page around web/gs.js.
 *
 * All the arithmetic lives in gs.js; this file only deals with the DOM, the
 * URL (?pegs=A1+C1+...) and the presentation of one board's result.
 */
import * as gs from "./gs.js";

const KEEP = 60;             /* solutions kept for browsing */

const el = id => document.getElementById(id);
const grid       = el("grid");
const pegsInput  = el("pegs-input");
const pegCount   = el("pegcount");
const parseError = el("parse-error");
const idleBox    = el("idle");
const busyBox    = el("busy");
const resultBox  = el("result");
const solGrid    = el("solution-grid");
const solLabel   = el("sol-label");
const thumbs     = el("thumbs");
const thumbsNote = el("thumbs-note");

let pegs = [];               /* cell indices, unsorted */
let current = null;          /* last solve() result */
let shown = 0;               /* index into current.solutions */
let notableIdx = -1;

/* Boards worth a look, all from docs/RESULTS.md except the unsolvable ones,
 * which were picked with `bin/gs_solve --count`. */
const NOTABLE = [
    ["A1 B1 C1 D1 E1 F1 A2", "the most solutions of any board: 100,593"],
    ["A1 C1 B2 D2 B3 A4 B5", "a unique solution — all nine pieces pinned"],
    ["A1 C1 F2 C4 D5 C6 E6", "another of the 100 unique-solution orbits"],
    ["C1 B2 E2 C4 F4 B5 E5", "the hardest trap-free board: 2 solutions, nothing pinned"],
    ["B2 C2 E2 D3 C4 B5 E5", "the only other trap-free board with 2 solutions"],
    ["A1 E2 D3 E4 B5 D5 C6", "trap-free with 4 solutions"],
    ["E1 F2 B3 C4 B5 F5 E6", "unsolvable — one of the 172,440"],
    ["B2 D2 F2 C3 B4 D4 E5", "unsolvable, with no isolated cell to blame"],
    ["B1 D1 A2 B3 A4 B5 D5", "unsolvable"],
    ["A1 C1 E1 B4 D4 F4 C6", "the board in the project README"]
];

/* ---------------------------------------------------------------- board */
function buildGrid() {
    grid.append(Object.assign(document.createElement("div"), { className: "corner" }));
    for (let c = 0; c < gs.N; c++)
        grid.append(Object.assign(document.createElement("div"),
            { className: "head", textContent: String.fromCharCode(65 + c) }));
    for (let r = 0; r < gs.N; r++) {
        grid.append(Object.assign(document.createElement("div"),
            { className: "side", textContent: String(r + 1) }));
        for (let c = 0; c < gs.N; c++) {
            const cell = r * gs.N + c;
            const b = document.createElement("button");
            b.type = "button";
            b.className = "cell";
            b.dataset.cell = String(cell);
            b.setAttribute("aria-label", gs.cellName(cell));
            b.addEventListener("click", () => toggle(cell));
            grid.append(b);
        }
    }
}

function toggle(cell) {
    const i = pegs.indexOf(cell);
    if (i >= 0) pegs.splice(i, 1);
    else if (pegs.length < gs.NPEGS) pegs.push(cell);
    else return flash(`already ${gs.NPEGS} pegs — remove one first`);
    notableIdx = -1;
    afterChange();
}

function flash(msg, kind = "error") {
    parseError.textContent = msg;
    parseError.hidden = false;
    parseError.classList.toggle("info", kind === "info");
    clearTimeout(flash.t);
    flash.t = setTimeout(() => { parseError.hidden = true; }, 3000);
}

function paintBoard() {
    const set = new Set(pegs);
    for (const b of grid.querySelectorAll(".cell"))
        b.classList.toggle("peg", set.has(+b.dataset.cell));
    pegCount.textContent = `${pegs.length} of ${gs.NPEGS} pegs`;
    pegCount.classList.toggle("full", pegs.length === gs.NPEGS);
}

/* ---------------------------------------------------------------- solve */
function afterChange({ fromInput = false } = {}) {
    paintBoard();
    if (!fromInput) pegsInput.value = gs.formatBoard(pegs);
    syncUrl();

    if (pegs.length !== gs.NPEGS) {
        current = null;
        resultBox.hidden = true;
        busyBox.hidden = true;
        idleBox.hidden = false;
        idleBox.querySelector("p").textContent = pegs.length === 0
            ? "Place seven pegs to solve."
            : `${gs.NPEGS - pegs.length} more peg${gs.NPEGS - pegs.length === 1 ? "" : "s"} to go.`;
        return;
    }

    idleBox.hidden = true;
    resultBox.hidden = true;
    busyBox.hidden = false;
    /* let the browser paint "Solving…" before the DFS blocks the thread;
     * the worst board in the game (100,593 solutions) takes ~0.25 s */
    requestAnimationFrame(() => requestAnimationFrame(runSolve));
}

function runSolve() {
    const [lo, hi] = gs.maskFromCells(pegs);
    const t0 = performance.now();
    current = gs.solve(lo, hi, KEEP);
    current.ms = performance.now() - t0;
    shown = 0;
    busyBox.hidden = true;
    resultBox.hidden = false;
    render();
}

/* --------------------------------------------------------------- render */
function render() {
    const n = current.count;
    el("count-value").textContent = n === 0 ? "0" : n.toLocaleString();
    el("count-label").textContent = n === 1 ? "solution" : "solutions";
    document.querySelector(".verdict").classList.toggle("zero", n === 0);

    renderVerdictNote();
    renderDistribution();
    renderSolutions();
    renderFreedom();
    renderFacts();
}

function renderVerdictNote() {
    const note = el("verdict-note");
    if (current.count === 0) {
        const g = gs.geometry(...gs.maskFromCells(pegs));
        const bits = [];
        if (g.isolated.length)
            bits.push(`${g.isolated.length} isolated cell${g.isolated.length > 1 ? "s" : ""} ` +
                      `(${g.isolated.map(gs.cellName).join(", ")}) but only one 1×1 piece`);
        else
            bits.push(`the empty region is in ${g.ncomp} piece${g.ncomp > 1 ? "s" : ""} ` +
                      `of size ${g.sizes.join(" + ")}, and no assignment of the nine ` +
                      `pieces fills them`);
        note.innerHTML = `<strong>Unsolvable.</strong> ${bits[0]}. ` +
            `2.07% of all boards are — the game's dice are shaped to never roll one.`;
    } else {
        const pinned = current.freedom.filter(f => f.pinned).length;
        note.textContent = pinned
            ? `${pinned} of 9 pieces are pinned — this board is trapped.`
            : "No piece is pinned — this board is trap-free.";
    }
}

/* Where this board's count sits in the distribution of all 8,347,680 boards.
 * One series, so no legend; the board's own bucket is the only highlight. */
function renderDistribution() {
    const box = el("distribution");
    const b = gs.bucketOf(current.count);
    const max = Math.max(...gs.HISTOGRAM.map(h => h[1]));
    /* the tail buckets hold a few dozen boards out of 8.3M, so two decimals
     * would round them to 0.00% */
    const pct = x => x >= 0.01 ? x.toFixed(2) : x.toPrecision(2);

    const bars = gs.HISTOGRAM.map(([upper, boards, cum], i) => {
        const lower = i === 0 ? 0 : gs.HISTOGRAM[i - 1][0] + 1;
        const on = b && b.upper === upper;
        const range = upper === 0 ? "unsolvable"
                    : lower === upper ? `exactly ${lower}`
                    : `${lower.toLocaleString()}–${upper.toLocaleString()}`;
        const share = pct(100 * boards / gs.NBOARDS);
        return `<div class="bar${on ? " on" : ""}" tabindex="0"
                     style="height:${Math.max(2, 100 * boards / max)}%"
                     aria-label="${range} solutions: ${share}% of boards">
                  <span class="tip">${range} · ${share}% of boards</span>
                </div>`;
    }).join("");

    const caption = current.count === 0
        ? `172,440 boards (2.07%) have no solution at all.`
        : `${b.lower === b.upper ? "Exactly " + b.lower : b.lower.toLocaleString() + "–" + b.upper.toLocaleString()}` +
          ` solutions covers ${pct(b.sharePct)}% of boards; ` +
          `${pct(b.cumPct)}% of all boards have ${b.upper.toLocaleString()} solutions or fewer. ` +
          `The median board has ${gs.MEDIAN_SOLUTIONS}.`;

    box.innerHTML =
        `<div class="chart">${bars}</div>
         <div class="chart-axis"><span>unsolvable</span><span>${gs.MAX_SOLUTIONS.toLocaleString()} solutions</span></div>
         <p class="chart-caption">${caption}</p>`;
}

function renderSolutions() {
    const block = el("solution-block");
    if (current.count === 0) { block.hidden = true; return; }
    block.hidden = false;

    thumbsNote.textContent = current.truncated
        ? `Showing the first ${current.solutions.length.toLocaleString()} of ` +
          `${current.count.toLocaleString()} solutions, in the order the depth-first ` +
          `search finds them.`
        : current.count > 1
          ? `All ${current.count.toLocaleString()} solutions, in search order.`
          : "";

    renderLegend();
    renderThumbs();
    showSolution(0);
}

function renderLegend() {
    el("legend").innerHTML = gs.PIECE_NAME.map((name, p) =>
        `<span class="item"><span class="swatch p${p}">${gs.PIECE_CHAR[p]}</span>` +
        `${gs.PIECE_LABEL[p]}</span>`).join("");
}

function renderThumbs() {
    const set = new Set(pegs);
    thumbs.innerHTML = current.solutions.map((cp, i) => {
        const cells = [];
        for (let c = 0; c < gs.CELLS; c++)
            cells.push(set.has(c) ? `<i class="pegcell"></i>`
                                  : `<i class="p${cp[c]}"></i>`);
        return `<button type="button" class="thumb" data-i="${i}"
                        aria-label="solution ${i + 1}">${cells.join("")}</button>`;
    }).join("");
    for (const t of thumbs.querySelectorAll(".thumb"))
        t.addEventListener("click", () => showSolution(+t.dataset.i));
}

function showSolution(i) {
    shown = Math.max(0, Math.min(i, current.solutions.length - 1));
    const cp = current.solutions[shown];
    const set = new Set(pegs);

    solGrid.innerHTML = "";
    solGrid.append(Object.assign(document.createElement("div"), { className: "corner" }));
    for (let c = 0; c < gs.N; c++)
        solGrid.append(Object.assign(document.createElement("div"),
            { className: "head", textContent: String.fromCharCode(65 + c) }));
    for (let r = 0; r < gs.N; r++) {
        solGrid.append(Object.assign(document.createElement("div"),
            { className: "side", textContent: String(r + 1) }));
        for (let c = 0; c < gs.N; c++) {
            const cell = r * gs.N + c;
            const d = document.createElement("div");
            if (set.has(cell)) {
                d.className = "cell peg";
                d.setAttribute("aria-label", `${gs.cellName(cell)}: peg`);
            } else {
                const p = cp[cell];
                d.className = `cell filled p${p}`;
                d.textContent = gs.PIECE_CHAR[p];
                d.setAttribute("aria-label", `${gs.cellName(cell)}: ${gs.PIECE_LABEL[p]}`);
            }
            solGrid.append(d);
        }
    }
    solGrid.setAttribute("aria-label",
        `solution ${shown + 1} of ${current.count}`);

    solLabel.textContent =
        `${shown + 1} of ${current.solutions.length.toLocaleString()}`;
    el("btn-prev").disabled = shown === 0;
    el("btn-next").disabled = shown >= current.solutions.length - 1;

    for (const t of thumbs.querySelectorAll(".thumb"))
        t.classList.toggle("on", +t.dataset.i === shown);
}

function renderFreedom() {
    const rows = current.count === 0
        ? `<tr><td colspan="3" class="key">No solutions, so no piece has a position.</td></tr>`
        : current.freedom.map(f =>
            `<tr>
               <td><span class="tag-sw p${f.piece}"></span>${gs.PIECE_LABEL[f.piece]}</td>
               <td class="num">${f.nplaces.toLocaleString()}</td>
               <td>${f.pinned ? '<span class="pin">pinned</span>' : ""}</td>
             </tr>`).join("");
    el("freedom").innerHTML =
        `<tr><th>piece</th><th class="num">positions</th><th></th></tr>${rows}`;
}

function renderFacts() {
    const g = gs.geometry(...gs.maskFromCells(pegs));
    const r = gs.rank(pegs);
    const cr = gs.rank(gs.canon(pegs));
    const rows = [
        ["pegs", `<code>${gs.formatBoard(pegs)}</code>`],
        ["board rank", r.toLocaleString()],
        ["canonical rank", cr.toLocaleString() + (cr === r ? " (this board is canonical)" : "")],
        ["D4 orbit size", `${gs.orbitSize(pegs)} of 8`],
        ["empty region", `${g.ncomp} component${g.ncomp > 1 ? "s" : ""}: ${g.sizes.join(" + ")}`],
        ["search time", `${current.ms.toFixed(1)} ms in this tab`]
    ];
    el("facts").innerHTML = rows.map(([k, v]) =>
        `<tr><td class="key">${k}</td><td>${v}</td></tr>`).join("");
}

/* ------------------------------------------------------------------ URL */
function syncUrl() {
    const u = new URL(location.href);
    /* assigned rather than set through searchParams, which would percent-encode
     * the separating commas and make the link unreadable */
    u.search = pegs.length ? "pegs=" + gs.formatBoard(pegs).replace(/ /g, ",") : "";
    history.replaceState(null, "", u);
}

function loadFromUrl() {
    const q = new URL(location.href).searchParams.get("pegs");
    if (!q) return false;
    const p = gs.parseBoard(q);
    if (p.error || !p.cells) return false;
    pegs = p.cells;
    return true;
}

/* -------------------------------------------------------------- wiring */
function randomBoard() {
    const cells = [];
    while (cells.length < gs.NPEGS) {
        const c = Math.floor(Math.random() * gs.CELLS);
        if (!cells.includes(c)) cells.push(c);
    }
    return cells;
}

function init() {
    buildGrid();

    pegsInput.addEventListener("input", () => {
        const p = gs.parseBoard(pegsInput.value);
        parseError.hidden = true;
        if (p.cells) {
            pegs = p.cells;
            notableIdx = -1;
            afterChange({ fromInput: true });
        }
        if (p.error && pegsInput.value.trim() && !p.error.startsWith("need exactly")) {
            parseError.textContent = p.error;
            parseError.hidden = false;
        }
    });

    el("btn-clear").addEventListener("click", () => {
        pegs = []; notableIdx = -1; afterChange();
    });
    el("btn-random").addEventListener("click", () => {
        pegs = randomBoard(); notableIdx = -1; afterChange();
    });
    el("btn-hard").addEventListener("click", () => {
        notableIdx = (notableIdx + 1) % NOTABLE.length;
        pegs = gs.parseBoard(NOTABLE[notableIdx][0]).cells;
        afterChange();
        flash(NOTABLE[notableIdx][1], "info");
    });

    el("btn-prev").addEventListener("click", () => showSolution(shown - 1));
    el("btn-next").addEventListener("click", () => showSolution(shown + 1));
    addEventListener("keydown", e => {
        if (!current || current.count === 0) return;
        if (e.target.tagName === "INPUT") return;
        if (e.key === "ArrowLeft") { showSolution(shown - 1); e.preventDefault(); }
        if (e.key === "ArrowRight") { showSolution(shown + 1); e.preventDefault(); }
    });

    if (!loadFromUrl()) pegs = gs.parseBoard("A1 C1 E1 B4 D4 F4 C6").cells;
    afterChange();
}

init();

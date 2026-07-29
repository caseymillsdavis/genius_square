/* gs_countall -- solution counts for every one of the C(36,7) = 8,347,680
 * Genius Square boards, in a single global enumeration.
 *
 * The key observation is that we do not have to solve 8.3 million puzzles.
 * Every packing of the nine pieces into the 6x6 grid covers exactly 29 cells
 * and therefore leaves exactly 7 empty ones -- i.e. every packing *is* a
 * solution of exactly one board, namely the one whose pegs sit on the 7 holes.
 * So a single enumeration of all packings, bucketed by hole set, yields the
 * complete count vector:
 *
 *      sum over boards B of solutions(B) = total number of packings.
 *
 * The search walks cells in increasing index order.  At the lowest still
 * undecided cell it either declares the cell a hole (budget permitting) or
 * covers it with an unused piece.  Because the anchor of a placement is its
 * lowest cell, only placements anchored at that cell need to be considered,
 * and every packing is generated exactly once.
 *
 * Board indices are lexicographic ranks (see gs_rank), maintained
 * incrementally so that a DFS subtree touches a contiguous slice of the
 * counter array.
 */
#include "gs_core.h"
#include "gs_search.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#include "gs_io.h"

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + 1e-9 * ts.tv_nsec;
}

/* ------------------------------------------------------------------ */
typedef struct {
    gs_mask  filled;      /* cells decided so far (pieces + holes)     */
    gs_mask  holes;       /* the subset of `filled` left empty         */
    uint32_t rankpart;    /* partial lexicographic rank of the holes   */
    uint16_t used;        /* 9-bit set of placed pieces                */
    int8_t   nholes;      /* holes committed so far                    */
    int8_t   prev;        /* cell index of the last hole, -1 if none   */
    int8_t   mvdone;      /* 1 once the mirror test has been decided   */
    int8_t   mvrow;       /* rows already compared against the mirror  */
} state_t;

static gs_ptable T;
static int use_sym = 1;

/* ------------------------------------------------------------------ */
/* symmetry reduction                                                  */
/*
 * Solution counts are D4-equivariant (the nine pieces are free polyominoes,
 * so the piece set is closed under the eight symmetries of the square), and
 * we only ever want one board per orbit.  Turning that into a *search* cut
 * requires the symmetry to be testable from a prefix of the decisions, and
 * the DFS decides cells in row-major order.  The left-right mirror is the one
 * element of D4 that maps every row onto itself, so as soon as a row is fully
 * decided its hole pattern can be compared with its own bit-reversal.
 *
 * We keep only hole sets that are lexicographically <= their mirror image.
 * Every D4-canonical board satisfies that (its mirror is in its orbit), so
 * the canonical counts survive intact, and the search does about half the
 * work.  The other elements of D4 (rot90, the up-down mirror, the diagonal
 * flips) map row 0 to a column or to row 5, so they cannot be settled until
 * the very end of the scan and are not worth testing here -- see
 * docs/ALGORITHMS.md.
 */
static uint8_t rev6[64];

static void init_rev6(void)
{
    for (int b = 0; b < 64; b++) {
        int r = 0;
        for (int i = 0; i < 6; i++) if (b & (1 << i)) r |= 1 << (5 - i);
        rev6[b] = (uint8_t)r;
    }
}

/* Compare row `r` of the hole set against its mirror.
 * returns  0 = tied, 1 = we are strictly smaller (canonical, stop testing),
 *         -1 = the mirror is smaller, so this branch is redundant. */
static inline int mv_row_cmp(gs_mask holes, int r)
{
    unsigned b  = (unsigned)((holes >> (GS_N * r)) & 63u);
    unsigned rb = rev6[b];
    unsigned d  = b ^ rb;
    if (!d) return 0;
    return ((b >> __builtin_ctz(d)) & 1u) ? 1 : -1;
}

typedef struct {
    uint32_t *counts;
    uint64_t  leaves;
    uint64_t  nodes;
} acc_t;

/* Necessary condition: every connected component of the still-empty region
 * must have a size reachable as (sub-multiset of remaining piece areas) plus
 * at most `hl` further hole cells. */
static inline int feasible_h(gs_mask empty, unsigned rem, int hl)
{
    if (gs_dense_h[rem][hl]) return 1;
    uint32_t ok = gs_sumset_h[rem][hl];
    while (empty) {
        gs_mask comp = empty & (~empty + 1), prev;
        do { prev = comp; comp = gs_grow(comp, empty); } while (comp != prev);
        int sz = __builtin_popcountll(comp);
        if (!((ok >> sz) & 1u)) return 0;
        empty ^= comp;
    }
    return 1;
}

static void rec(acc_t *a, gs_mask filled, gs_mask holes, unsigned used,
                int nh, int prev, uint32_t rp, int mvdone, int mvrow)
{
    a->nodes++;
    if (used == GS_ALL_PIECES) {
        /* every remaining cell is a hole; finish the rank incrementally */
        gs_mask empty = ~filled & GS_FULL;
        holes |= empty;
        while (empty) {
            int c = __builtin_ctzll(empty);
            empty &= empty - 1;
            nh++;
            rp += gs_binom[35 - prev][8 - nh] - gs_binom[36 - c][8 - nh];
            prev = c;
        }
        if (!mvdone) {                  /* finish the mirror comparison */
            for (int r = mvrow; r < GS_N; r++) {
                int v = mv_row_cmp(holes, r);
                if (v < 0) return;      /* mirror image is the canonical one */
                if (v > 0) break;
            }
        }
        a->counts[rp]++;
        a->leaves++;
        return;
    }

    gs_mask  empty = ~filled & GS_FULL;
    unsigned rem   = GS_ALL_PIECES & ~used;
    int      hl    = GS_PEGS - nh;

    if (!feasible_h(empty, rem, hl)) return;

    int c = __builtin_ctzll(empty);

    /* every row below c is fully decided, so its mirror test can be settled */
    if (!mvdone) {
        int done = c / GS_N;
        while (mvrow < done) {
            int v = mv_row_cmp(holes, mvrow++);
            if (v < 0) return;
            if (v > 0) { mvdone = 1; break; }
        }
    }

    if (hl > 0) {                       /* leave this cell empty */
        int i = nh + 1;
        gs_mask bit = (gs_mask)1 << c;
        rec(a, filled | bit, holes | bit, used, i, c,
            rp + gs_binom[35 - prev][8 - i] - gs_binom[36 - c][8 - i],
            mvdone, mvrow);
    }
    unsigned r = rem;
    while (r) {                         /* or cover it */
        int q = __builtin_ctz(r);
        r &= r - 1;
        const int hi = T.hi[c][q];
        for (int i = T.lo[c][q]; i < hi; i++) {
            gs_mask m = T.pmask[i];
            if (m & filled) continue;
            rec(a, filled | m, holes, used | (1u << q), nh, prev, rp,
                mvdone, mvrow);
        }
    }
}

/* ------------------------------------------------------------------ */
/* breadth-first expansion of the root into independent tasks          */
/* ------------------------------------------------------------------ */
static state_t *tasks;
static size_t   ntasks, captasks;

static void push(state_t s)
{
    if (ntasks == captasks) {
        captasks = captasks ? captasks * 2 : 1024;
        tasks = realloc(tasks, captasks * sizeof *tasks);
        if (!tasks) { fprintf(stderr, "oom\n"); exit(1); }
    }
    tasks[ntasks++] = s;
}

/* Expand states until we have at least `target` of them.  Leaves met during
 * expansion are counted straight into `a`. */
static void expand(acc_t *a, size_t target)
{
    state_t root = { 0, 0, 0, 0, 0, -1, (int8_t)(use_sym ? 0 : 1), 0 };
    push(root);
    size_t head = 0;
    while (ntasks - head < target && head < ntasks) {
        state_t s = tasks[head++];

        if (s.used == GS_ALL_PIECES) {
            rec(a, s.filled, s.holes, s.used, s.nholes, s.prev, s.rankpart,
                s.mvdone, s.mvrow);
            continue;
        }

        gs_mask  empty = ~s.filled & GS_FULL;
        unsigned rem   = GS_ALL_PIECES & ~s.used;
        int      hl    = GS_PEGS - s.nholes;
        if (!feasible_h(empty, rem, hl)) continue;

        int c = __builtin_ctzll(empty);
        if (!s.mvdone) {
            int done = c / GS_N, dead = 0;
            while (s.mvrow < done) {
                int v = mv_row_cmp(s.holes, s.mvrow++);
                if (v < 0) { dead = 1; break; }
                if (v > 0) { s.mvdone = 1; break; }
            }
            if (dead) continue;
        }

        if (hl > 0) {
            int i = s.nholes + 1;
            state_t t = s;
            gs_mask bit = (gs_mask)1 << c;
            t.filled |= bit;
            t.holes  |= bit;
            t.nholes  = (int8_t)i;
            t.rankpart = s.rankpart
                       + gs_binom[35 - s.prev][8 - i] - gs_binom[36 - c][8 - i];
            t.prev    = (int8_t)c;
            push(t);
        }
        unsigned r = rem;
        while (r) {
            int q = __builtin_ctz(r);
            r &= r - 1;
            for (int i = T.lo[c][q]; i < T.hi[c][q]; i++) {
                gs_mask m = T.pmask[i];
                if (m & s.filled) continue;
                state_t t = s;
                t.filled |= m;
                t.used   |= (uint16_t)(1u << q);
                push(t);
            }
        }
    }
    /* compact: drop the already-expanded prefix */
    memmove(tasks, tasks + head, (ntasks - head) * sizeof *tasks);
    ntasks -= head;
}

/* ------------------------------------------------------------------ */
static size_t          next_task;
static size_t          task_limit;      /* benchmark: stop after this many */
static int             verify_n = 2000;
static pthread_mutex_t task_lock = PTHREAD_MUTEX_INITIALIZER;
static int             verbose;

typedef struct { acc_t acc; int id; } worker_t;

static void *worker(void *arg)
{
    worker_t *w = arg;
    w->acc.counts = calloc(GS_NBOARDS, sizeof(uint32_t));
    if (!w->acc.counts) { fprintf(stderr, "oom in worker\n"); exit(1); }
    for (;;) {
        pthread_mutex_lock(&task_lock);
        size_t i = next_task++;
        if (verbose && (i % 8192) == 0 && i < ntasks) {
            fprintf(stderr, "\r  tasks %zu/%zu (%.1f%%)   ",
                    i, ntasks, 100.0 * i / ntasks);
            fflush(stderr);
        }
        pthread_mutex_unlock(&task_lock);
        if (i >= ntasks || i >= task_limit) break;
        state_t s = tasks[i];
        rec(&w->acc, s.filled, s.holes, s.used, s.nholes, s.prev, s.rankpart,
            s.mvdone, s.mvrow);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
static void usage(const char *p)
{
    fprintf(stderr,
        "usage: %s [-j threads] [-o out.gsc] [--full out.u32] [-t tasks] [-q]\n"
        "  -j N        worker threads (default: 4)\n"
        "  -o FILE     write canonical-orbit counts (compressed .gsc)\n"
        "  --full FILE also write the raw 8347680-entry uint32 array\n"
        "  -t N        target number of parallel tasks (default 200000)\n"
        "  -L N        benchmark: run only the first N tasks, write nothing\n"
        "  --nosym     disable the mirror-symmetry cut (~2x slower; for checking)\n"
        "  --verify N  re-solve N random boards with the single-board engine\n"
        "  -q          quiet\n", p);
}

int main(int argc, char **argv)
{
    int nthreads = 4;
    const char *out = "data/counts.gsc", *fullout = NULL;
    size_t target = 200000;
    verbose = 1;
    task_limit = (size_t)-1;
    init_rev6();

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-L") && i + 1 < argc)          task_limit = (size_t)atoll(argv[++i]);
        else if (!strcmp(argv[i], "--nosym"))                use_sym = 0;
        else if (!strcmp(argv[i], "--verify") && i + 1 < argc) verify_n = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-j") && i + 1 < argc)     nthreads = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc)     out = argv[++i];
        else if (!strcmp(argv[i], "--full") && i + 1 < argc) fullout = argv[++i];
        else if (!strcmp(argv[i], "-t") && i + 1 < argc)     target = (size_t)atoll(argv[++i]);
        else if (!strcmp(argv[i], "-q"))                     verbose = 0;
        else { usage(argv[0]); return 1; }
    }

    gs_init_all();
    gs_init_search();
    gs_build_placements(&T);

    double t0 = now();
    acc_t seed = {0};
    seed.counts = calloc(GS_NBOARDS, sizeof(uint32_t));
    if (!seed.counts) { fprintf(stderr, "oom\n"); return 1; }
    expand(&seed, target);
    if (verbose) fprintf(stderr, "  expanded root into %zu tasks in %.2f s\n",
                         ntasks, now() - t0);

    worker_t *w = calloc((size_t)nthreads, sizeof *w);
    pthread_t *th = calloc((size_t)nthreads, sizeof *th);
    for (int i = 0; i < nthreads; i++) {
        w[i].id = i;
        if (pthread_create(&th[i], NULL, worker, &w[i])) {
            fprintf(stderr, "pthread_create: %s\n", strerror(errno));
            return 1;
        }
    }
    for (int i = 0; i < nthreads; i++) pthread_join(th[i], NULL);
    if (verbose) fprintf(stderr, "\r  search done in %.2f s%20s\n", now() - t0, "");

    /* merge */
    uint32_t *counts = seed.counts;
    uint64_t leaves = seed.leaves, nodes = seed.nodes;
    for (int i = 0; i < nthreads; i++) {
        const uint32_t *c = w[i].acc.counts;
        for (uint32_t k = 0; k < GS_NBOARDS; k++) counts[k] += c[k];
        leaves += w[i].acc.leaves;
        nodes  += w[i].acc.nodes;
        free(w[i].acc.counts);
    }

    uint64_t sum = 0;
    for (uint32_t k = 0; k < GS_NBOARDS; k++) sum += counts[k];
    double dt = now() - t0;
    fprintf(stderr,
        "  leaves        = %llu\n"
        "  sum of counts = %llu  %s\n"
        "  nodes         = %llu  (%.2f nodes/leaf)\n"
        "  wall time     = %.2f s  (%.1f Mleaf/s)\n",
        (unsigned long long)leaves, (unsigned long long)sum,
        sum == leaves ? "OK" : "MISMATCH",
        (unsigned long long)nodes, (double)nodes / (double)leaves,
        dt, leaves / dt / 1e6);
    if (sum != leaves) return 2;
    if (task_limit != (size_t)-1) {   /* benchmark run: nothing is complete */
        fprintf(stderr, "  (benchmark run of %zu/%zu tasks -- results discarded)\n",
                task_limit, ntasks);
        free(counts);
        return 0;
    }

    /* With the mirror cut on, only the mirror-canonical half of the table was
     * filled in.  Rebuild the whole thing from the D4-canonical entries (which
     * are always mirror-canonical) and cross-check the two against each other. */
    uint32_t ncanon;
    uint32_t *list = gs_canon_list(&ncanon);
    if (!list) return 4;
    uint32_t *canon = malloc((size_t)ncanon * sizeof *canon);
    for (uint32_t i = 0; i < ncanon; i++) canon[i] = counts[list[i]];
    uint32_t *full = gs_expand_canon(canon, list, ncanon);
    if (!full) return 4;

    /* the mirror-canonical half must reproduce the same numbers */
    uint64_t total = 0;
    for (uint32_t k = 0; k < GS_NBOARDS; k++) total += full[k];
    if (use_sym) {
        uint64_t recon = 0;
        for (uint32_t k = 0; k < GS_NBOARDS; k++) {
            gs_mask m  = gs_unrank(k);
            gs_mask mv = gs_apply(4, m);          /* left-right mirror */
            if (m == mv)                 recon += counts[k];
            else if (gs_rank(mv) > k)    recon += 2u * (uint64_t)counts[k];
            else if (counts[k] != 0) {
                fprintf(stderr, "  ERROR: non-canonical board %u was counted\n", k);
                return 6;
            }
        }
        fprintf(stderr, "  mirror-fold identity: %llu vs %llu  %s\n",
                (unsigned long long)recon, (unsigned long long)total,
                recon == total ? "OK" : "MISMATCH");
        if (recon != total) return 6;
    }

    /* independent check: re-solve random boards with the single-board engine */
    {
        uint64_t s = 0x9E3779B97F4A7C15ull;
        int bad = 0;
        for (int i = 0; i < verify_n; i++) {
            s ^= s << 13; s ^= s >> 7; s ^= s << 17;
            uint32_t r = (uint32_t)(s % GS_NBOARDS);
            uint64_t c = gs_solve_count(&T, gs_unrank(r));
            if (c != full[r]) {
                fprintf(stderr, "  ERROR: board %u table=%u solver=%llu\n",
                        r, full[r], (unsigned long long)c);
                if (++bad > 5) return 7;
            }
        }
        if (bad) return 7;
        fprintf(stderr, "  re-solved %d random boards independently: all match\n",
                verify_n);
    }

    uint64_t solvable = 0, maxc = 0;
    for (uint32_t k = 0; k < GS_NBOARDS; k++) {
        if (full[k]) solvable++;
        if (full[k] > maxc) maxc = full[k];
    }
    fprintf(stderr,
        "  total packings = %llu\n"
        "  solvable       = %llu / %u  (%.4f%%)\n"
        "  max count      = %llu\n",
        (unsigned long long)total, (unsigned long long)solvable, GS_NBOARDS,
        100.0 * (double)solvable / GS_NBOARDS, (unsigned long long)maxc);

    if (fullout && gs_write_full(fullout, full) != 0) return 4;
    if (out     && gs_write_canon(out, full)    != 0) return 5;
    free(counts); free(full); free(canon); free(list);
    return 0;
}

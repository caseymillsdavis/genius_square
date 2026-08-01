# Genius Square -- exhaustive solver and statistics
#
#   make            build everything into bin/
#   make test       run the self-test
#   make counts     compute data/counts.gsc  (the long one)
#   make report     regenerate docs/RESULTS.md and docs/ANALYSIS*.md
#   make all-data   counts + report
#   make web-verify cross-check web/gs.js against the C engine (needs node)

CC      ?= gcc
# gnu11 rather than c11: clock_gettime and strtok_r are POSIX, not ISO C
CFLAGS  ?= -O3 -march=native -std=gnu11 -Wall -Wextra -Wno-unused-result
LDLIBS  := -lm -lz
LDLIBS_MT := -lpthread

SRC     := src
BIN     := bin
DATA    := data
DOCS    := docs

CORE    := $(SRC)/gs_core.c $(SRC)/gs_search.c $(SRC)/gs_io.c $(SRC)/gs_dlx.c

TOOLS   := $(BIN)/gs_selftest $(BIN)/gs_countall $(BIN)/gs_solve \
           $(BIN)/gs_stats $(BIN)/gs_analyze

# number of worker threads for the big enumeration
J       ?= $(shell nproc 2>/dev/null || echo 4)

.PHONY: all test counts report all-data web-verify clean

all: $(TOOLS)

$(BIN):
	@mkdir -p $(BIN)
$(DATA):
	@mkdir -p $(DATA)

$(BIN)/gs_selftest: $(CORE) $(SRC)/gs_selftest.c | $(BIN)
	$(CC) $(CFLAGS) -I$(SRC) -o $@ $^ $(LDLIBS)

$(BIN)/gs_countall: $(CORE) $(SRC)/gs_countall.c | $(BIN)
	$(CC) $(CFLAGS) -I$(SRC) -o $@ $^ $(LDLIBS) $(LDLIBS_MT)

$(BIN)/gs_solve: $(CORE) $(SRC)/gs_solve.c | $(BIN)
	$(CC) $(CFLAGS) -I$(SRC) -o $@ $^ $(LDLIBS)

$(BIN)/gs_stats: $(CORE) $(SRC)/gs_stats.c | $(BIN)
	$(CC) $(CFLAGS) -I$(SRC) -o $@ $^ $(LDLIBS)

$(BIN)/gs_analyze: $(CORE) $(SRC)/gs_analyze.c | $(BIN)
	$(CC) $(CFLAGS) -I$(SRC) -o $@ $^ $(LDLIBS)

test: $(BIN)/gs_selftest
	$(BIN)/gs_selftest

$(DATA)/counts.gsc: $(BIN)/gs_countall | $(DATA)
	$(BIN)/gs_countall -j $(J) -o $@

counts: $(DATA)/counts.gsc

report: $(BIN)/gs_stats $(BIN)/gs_analyze $(DATA)/counts.gsc
	$(BIN)/gs_stats   -c $(DATA)/counts.gsc -o $(DOCS)/RESULTS.md --threshold 400 --top 20
	$(BIN)/gs_analyze -c $(DATA)/counts.gsc --deg3 -o $(DOCS)/ANALYSIS_UNSOLVABLE.md
	$(BIN)/gs_analyze -c $(DATA)/counts.gsc --deg3 --hard 100 -o $(DOCS)/ANALYSIS_HARD.md

all-data: counts report

# web/gs.js is a hand port of gs_core.c + gs_search.c; this checks it still
# agrees with the original.  node is not a project dependency -- this is the
# one target that needs it.
web-verify: $(BIN)/gs_solve
	node web/verify.mjs 300

clean:
	rm -rf $(BIN)

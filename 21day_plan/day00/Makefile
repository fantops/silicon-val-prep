# Day 0 — C Ramp-up
#
# Layout:
#   exercises/   YOU write code here (stubs provided)
#   solutions/   reference implementations (peek only after attempting)
#
# Targets:
#   make ex-<name>    build + run YOUR exercise (e.g. make ex-my_memcpy)
#   make sol-<name>   build + run the reference solution
#   make all-ex       build every exercise stub (will fail until you fill TODOs)
#   make all-sol      build every reference solution
#   make clean

CC      = clang
CFLAGS  = -Wall -Wextra -Wpedantic -std=c11 -g -O0 -I.
BIN     = bin

EX_SRCS  = $(wildcard exercises/*.c)
SOL_SRCS = $(filter-out solutions/bench.c solutions/helper.c solutions/csr_init.c solutions/csr_model.c,$(wildcard solutions/*.c))

EX_BINS  = $(patsubst exercises/%.c,$(BIN)/ex-%,$(EX_SRCS))
SOL_BINS = $(patsubst solutions/%.c,$(BIN)/sol-%,$(SOL_SRCS))

all-sol: $(SOL_BINS)
all-ex:  $(EX_BINS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/ex-%: exercises/%.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

# bench.c references csr_init + pm_step which live (with their own main()) in
# csr_init.c / csr_model.c. Build it explicitly by stubbing those externs;
# until then `make sol-bench` is opt-in and excluded from `all-sol`.

$(BIN)/sol-%: solutions/%.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

ex-%: $(BIN)/ex-%
	./$<

sol-%: $(BIN)/sol-%
	./$<

clean:
	rm -rf $(BIN)

.PHONY: all-sol all-ex clean

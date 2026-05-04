# Day 0 — C Ramp-up exercises
# Usage:
#   make            # build everything
#   make reverse    # build + run reverse_list
#   make cycle      # build + run cycle_detect
#   make memcpy     # build + run my_memcpy
#   make memmove    # build + run my_memmove
#   make sizes      # build + run struct_sizes
#   make clean

CC      = clang
CFLAGS  = -Wall -Wextra -Wpedantic -std=c11 -g -O0
BIN_DIR = bin

SRCS = reverse_list.c cycle_detect.c my_memcpy.c my_memmove.c struct_sizes.c
BINS = $(SRCS:%.c=$(BIN_DIR)/%)

all: $(BINS)

$(BIN_DIR)/%: %.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

reverse: $(BIN_DIR)/reverse_list
	./$<

cycle: $(BIN_DIR)/cycle_detect
	./$<

memcpy: $(BIN_DIR)/my_memcpy
	./$<

memmove: $(BIN_DIR)/my_memmove
	./$<

sizes: $(BIN_DIR)/struct_sizes
	./$<

clean:
	rm -rf $(BIN_DIR) a.out

.PHONY: all clean reverse cycle memcpy memmove sizes

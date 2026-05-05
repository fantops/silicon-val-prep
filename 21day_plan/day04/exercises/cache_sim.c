/* E1 — Address decode + set-associative cache + LRU simulator.
 *
 * Geometry: LINE=64, SETS=8, WAYS=4.
 *
 * Implement (see ../EXERCISES.md):
 *   bool access(uint32_t addr);   // hit -> true; updates LRU
 *
 * Driver:
 *   sequential addresses (i*4)    -> high hit rate
 *   stride-1024 (i*1024)          -> near-100% miss
 *   print hits, misses, hit%.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define LINE 64
#define SETS 8
#define WAYS 4

typedef struct { bool valid; uint32_t tag; uint8_t lru; } line_t;
static line_t cache[SETS][WAYS];
static uint32_t hits, misses;

/* TODO: bool access(uint32_t addr) */

int main(void) {
    /* TODO: run both patterns, print stats */
    return 0;
}

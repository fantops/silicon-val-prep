/* E1 — SQ ring + doorbell harness with explicit ordering.
 *
 * Implement (see ../EXERCISES.md):
 *   int  sq_submit(uint32_t cmd, uint32_t arg);  // descriptor + barrier + doorbell
 *   void device_consume_one(void);
 *
 * Critical: barrier MUST sit between the descriptor stores and the doorbell store.
 * Driver: submit two entries, consume three (last is empty).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define SQ_ENTRIES 16
#define COMPILER_BARRIER() __asm__ volatile("" ::: "memory")
#define MEMORY_BARRIER()   __sync_synchronize()
#define CACHE_CLEAN(p, n)  (void)(p)

typedef struct { uint32_t cmd; uint32_t arg; } sqe_t;

static sqe_t sq[SQ_ENTRIES];
static volatile uint32_t sq_tail_db;
static uint32_t sq_tail = 0, sq_head = 0;

/* TODO: sq_submit, device_consume_one */

int main(void) {
    /* TODO */
    return 0;
}

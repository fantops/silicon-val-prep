/* E1 — Spinlock + SPSC ring with cache-line padded head/tail.
 *
 * Implement (see ../EXERCISES.md):
 *   spin_lock / spin_unlock  (test-and-test-and-set, acq/rel)
 *   spsc_push / spsc_pop     (relaxed + acquire/release; head & tail on separate lines)
 *
 * Driver:
 *   2 pthreads each bumping `counter` 100000 times under spin_lock; assert 200000.
 *   SPSC push {0,1,4,9,16}, pop all, print.
 *
 * Build: cc -std=c11 -O2 -pthread spinlock_spsc.c -o /tmp/spsc
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <pthread.h>

typedef _Atomic int spinlock_t;

/* TODO: spin_lock, spin_unlock */

#define R 1024
typedef struct {
    int buf[R];
    alignas(64) _Atomic uint32_t head;
    alignas(64) _Atomic uint32_t tail;
} spsc_t;

/* TODO: spsc_push, spsc_pop */

int main(void) {
    /* TODO: counter test + SPSC drive */
    return 0;
}

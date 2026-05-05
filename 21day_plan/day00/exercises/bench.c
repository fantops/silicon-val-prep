/* E11 — microbenchmark scaffold.
 *
 *   uint64_t now_ns(void);                    // CLOCK_MONOTONIC
 *   double   bench(void (*fn)(void), int iters);
 *
 * Use it to time my_memcpy vs glibc memcpy across:
 *   4 KiB, 64 KiB, 1 MiB, 4 MiB, 64 MiB
 *
 * Predict where L1 / L2 / LLC cliffs land on your laptop. Then measure.
 * This same harness is reused on Day 4 (cache lines / AMAT).
 */
#include <stdint.h>
#include <stdio.h>
#include <time.h>

/* TODO */

int main(void) { return 0; }

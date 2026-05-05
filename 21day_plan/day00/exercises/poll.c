/* E3 — bounded polling.
 *
 * int wait_for_bit_set(volatile uint32_t *reg, uint8_t bit, uint32_t timeout_us);
 *   return 0 on success, -1 (or -ETIMEDOUT) on timeout.
 *
 * Driver:
 *   1. "Producer" flips the bit after N internal iterations.
 *   2. Show timeout path by never flipping the bit.
 *   3. Print iterations consumed.
 *
 * Be ready to explain (write as comments before main):
 *   - Why never poll without a timeout?
 *   - Why is udelay() between MMIO polls usually wrong?
 *   - ISR vs sleep-capable context — what changes?
 */
#include <stdint.h>
#include <stdio.h>
#include "macros.h"

/* TODO: implement wait_for_bit_set + a producer hook. */

int main(void) {
    /* TODO */
    return 0;
}

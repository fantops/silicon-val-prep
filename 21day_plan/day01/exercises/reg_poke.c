/* E1 — Register-poke harness with bit/field macros, RMW, bounded poll.
 *
 * Implement (see ../EXERCISES.md):
 *   BIT(n), MASK(msb,lsb), FIELD_GET, FIELD_PREP
 *   reg_read32 / reg_write32 / reg_field_write
 *   wait_for_bit_set(reg, mask, timeout_us)  -> 0 on success, -1 on timeout
 *
 * Driver:
 *   1. Start fake_reg = 0xDEADBEEF, RMW field [15:8] = 0xAA, expect 0xDEADAAEF.
 *   2. Timeout case: fake_reg = 0, wait_for_bit_set BIT(0), 5  -> rc = -1.
 *   3. Success case: fake_reg = BIT(0), wait_for_bit_set BIT(0), 5 -> rc = 0.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* TODO: BIT / MASK / FIELD_GET / FIELD_PREP */

static volatile uint32_t fake_reg = 0;

/* TODO: reg_read32, reg_write32, reg_field_write */

/* TODO: wait_for_bit_set(volatile uint32_t *r, uint32_t mask, int timeout_us) */

int main(void) {
    /* TODO */
    return 0;
}

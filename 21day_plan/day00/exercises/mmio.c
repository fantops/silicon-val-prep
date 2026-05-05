/* E2 — MMIO accessors against a fake register block.
 *
 * Implement (volatile-correct):
 *   uint32_t reg_read32(volatile uint32_t *reg);
 *   void     reg_write32(volatile uint32_t *reg, uint32_t v);
 *   void     reg_field_write(volatile uint32_t *reg, uint8_t msb, uint8_t lsb, uint32_t v);
 *
 * Driver:
 *   - allocate 4 KiB, treat as a register block
 *   - write field [7:4] = 0xC at register index 2
 *   - read back, assert neighbours untouched
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "macros.h"

/* TODO: implement reg_read32, reg_write32, reg_field_write */

int main(void) {
    /* TODO: allocate, exercise, assert, print. */
    return 0;
}

/* E1 — Device init harness: reset, poll-ready, RMW enable+start, R/W DATA.
 *
 * Implement (see ../EXERCISES.md):
 *   reg_read / reg_write / reg_rmw on a uint8_t mmio[256] backing array.
 *   poll_ready(off, mask, exp, tries) that simulates HW asserting ready.
 *
 * Map: CTRL=0x00, STATUS=0x04, DATA=0x08
 * Bits: CTRL_EN=BIT(0), CTRL_START=BIT(1), CTRL_RESET=BIT(31), STATUS_RDY=BIT(0)
 *
 * Driver:
 *   reset -> poll_ready -> RMW EN -> RMW START -> write DATA -> read DATA.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define BIT(n) (UINT32_C(1) << (n))

static uint8_t mmio[256];

/* TODO: reg_read, reg_write, reg_rmw */

/* TODO: poll_ready(off, mask, exp, tries) */

int main(void) {
    memset(mmio, 0, sizeof mmio);
    /* TODO: reset, poll, enable, start, write/read DATA */
    return 0;
}

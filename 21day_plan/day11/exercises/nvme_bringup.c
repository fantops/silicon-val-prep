/* E1 — NVMe bring-up: read CAP, disable, program admin queue, enable, poll RDY.
 *
 * Implement (see ../EXERCISES.md):
 *   r32 / w32 / w64 accessors over uint8_t bar[BAR_SZ].
 *      Side effect: writing CC.EN=1 causes CSTS.RDY to follow EN.
 *   sim_init():       populate CAP = MQES=255, TO=10, DSTRD=0.
 *   db_off(q, is_cq, dstrd) = 0x1000 + (2*q + cq) * (4 << dstrd).
 *   nvme_bringup():   the 5-step canonical bring-up.
 *
 * Map: CAP=0x00, VS=0x08, CC=0x14, CSTS=0x1C, AQA=0x24, ASQ=0x28, ACQ=0x30, DB=0x1000.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define BAR_SZ 0x2000
static uint8_t bar[BAR_SZ];

#define R_CAP 0x00
#define R_VS  0x08
#define R_CC  0x14
#define R_CSTS 0x1C
#define R_AQA 0x24
#define R_ASQ 0x28
#define R_ACQ 0x30
#define R_DB  0x1000

/* TODO: r32, w32, w64, sim_init, db_off, nvme_bringup */

int main(void) {
    /* TODO: sim_init -> nvme_bringup */
    return 0;
}

/* E1 — ECAM compute, BAR-size readback, and LTSSM transition tracker.
 *
 * Implement (see ../EXERCISES.md):
 *   uint64_t ecam_addr(base, bus, dev, fn, reg);
 *   uint64_t bar_size_from_readback(uint32_t rb);
 *   bool go(link_t *l, ltssm_t to);
 *
 * Driver:
 *   ECAM 01:02.0 BAR0 against 0xE0000000.
 *   bar_size_from_readback(0xFFF00004).
 *   LTSSM walk including a Recovery loop and an illegal transition.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* TODO: ecam_addr, bar_size_from_readback */

typedef enum {
    LTSSM_DETECT, LTSSM_POLLING, LTSSM_CONFIG, LTSSM_L0,
    LTSSM_RECOVERY, LTSSM_L0S, LTSSM_L1, LTSSM_L1_1, LTSSM_L1_2, LTSSM_L2,
    LTSSM_NUM
} ltssm_t;

typedef struct { ltssm_t st; unsigned recoveries; } link_t;

/* TODO: const char *names[]; const uint8_t ok[LTSSM_NUM][LTSSM_NUM]; go() */

int main(void) {
    /* TODO */
    return 0;
}

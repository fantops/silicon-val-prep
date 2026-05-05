/* E1 — DVFS OPP lookup with thermal cap + V-F sequencing + IPC.
 *
 * Implement (see ../EXERCISES.md):
 *   const opp_t *select_opp(uint32_t target_mhz, int tj_c);   // tj>=90 -> cap 1200
 *   void apply_opp(const opp_t *cur, const opp_t *next);      // V-first up, F-first down
 *   double ipc(const pmu_t *p);
 *
 * Table: { (400,750), (800,850), (1200,950), (1800,1050), (2400,1150) } MHz/mV.
 *
 * Driver:
 *   select_opp(400,50) -> select_opp(2000,50)   ramp up
 *   select_opp(2400,95)                          thermal cap, ramp down
 *   ipc(insns=15e9, cycles=12.34e9)              warn if < 1.0
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef struct { uint32_t freq_mhz; uint32_t volt_mv; } opp_t;

/* TODO: static const opp_t table[]; size_t N */
/* TODO: select_opp, apply_opp */

typedef struct { uint64_t insns, cycles; } pmu_t;
/* TODO: ipc */

int main(void) {
    /* TODO */
    return 0;
}

/* E1 — Tj RC model + PROCHOT + RAPL delta + Top-down dominant bucket.
 *
 * Implement (see ../EXERCISES.md):
 *   void   thermal_step(thermal_t *m, double power, double dt);
 *   double rapl_watts(uint64_t e0, uint64_t e1, uint64_t unit_uj, double dt);
 *   const char *tma_dominant(const tma_t *t);
 *
 * Driver:
 *   30-step thermal sim (Ta=25, theta=0.3, prochot=95).
 *   RAPL delta to watts.
 *   TMA dominant for sample fractions.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct { double tj, ta, theta, prochot; bool throttled; } thermal_t;
typedef struct { double frontend_bound, bad_spec, retiring, mem_bound, core_bound; } tma_t;

/* TODO: thermal_step, rapl_watts, tma_dominant */

int main(void) {
    /* TODO */
    return 0;
}

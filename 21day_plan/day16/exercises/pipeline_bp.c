/* E1 — 2-bit BP + license-level effective freq + 5-stage pipeline diagram.
 *
 * Implement (see ../EXERCISES.md):
 *   bool predict(void);
 *   void update(bool taken);
 *   double effective_freq_ghz(double scalar, double avx2, double avx512, double base);
 *   void pipeline_demo(void);
 */
#include <stdio.h>
#include <stdbool.h>

static int bp = 1;
static unsigned hits, total;

/* TODO: predict, update, effective_freq_ghz, pipeline_demo */

int main(void) {
    /* TODO: run BP stream, print accuracy; print effective freq for two mixes; pipeline_demo() */
    return 0;
}

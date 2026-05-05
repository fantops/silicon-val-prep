/* E1 — NPU regression-gate harness.
 *
 * Implement (see ../EXERCISES.md):
 *   bool gate_check(const gate_t *g, const run_t *r);
 *
 * Print one FAIL line per violated metric; return overall pass/fail.
 *
 * Driver:
 *   gate = {90, 10000, 350, 4.0, 80}
 *   Run A known-good (95, 7045, 300, 3.0, 85)   -> PASS
 *   Run B regressed  (92, 102445, 502, 3.1, 84) -> FAIL (faults + VM)
 *   Run C fallback   (40, 8000, 310, 5.0, 84)   -> FAIL (npu_op_pct + per_tile_ms)
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    const char *ep_version, *ort_version, *model_build;
    int     npu_op_pct;
    int     page_faults;
    int     peak_vm_mb;
    double  per_tile_ms;
    int     s0ix_residency_pct;
} run_t;

typedef struct {
    int    min_npu_op_pct;
    int    max_page_faults;
    int    max_peak_vm_mb;
    double max_per_tile_ms;
    int    min_s0ix_residency_pct;
} gate_t;

/* TODO: gate_check */

int main(void) {
    /* TODO: define gate + 3 runs, score each */
    return 0;
}

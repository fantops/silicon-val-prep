/* E1 — NPU host harness: per-op EP assignment + boundary cost simulator.
 *
 * Implement (see ../EXERCISES.md):
 *   void run_graph(const op_t *graph, int n, stats_t *s);
 *
 * Costs: NPU op = 0.2 us, CPU op = 5.0 us, EP boundary copy = 50 us.
 *
 * Driver:
 *   CLEAN graph (all npu_supported)            -> 0 boundaries
 *   FALLBACK graph (insert one unsupported op) -> 2 boundaries, ~100 us added
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef enum { EP_NPU, EP_CPU } ep_t;

typedef struct {
    const char *name;
    bool        npu_supported;
    int         flops;
} op_t;

typedef struct {
    int total_ops, npu_ops, cpu_ops;
    int partition_boundaries;
    double sim_latency_us;
} stats_t;

/* TODO: run_graph */

int main(void) {
    /* TODO: build clean and fallback graphs, run, print stats */
    return 0;
}

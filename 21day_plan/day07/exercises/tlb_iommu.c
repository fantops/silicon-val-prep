/* E1 — TLB FIFO + IOMMU 2-level translate.
 *
 * Implement (see ../EXERCISES.md):
 *   uint32_t translate(uint32_t va);                        // 4-entry FIFO TLB
 *   bool iommu_translate(iommu_ctx_t *c, uint32_t iova,
 *                        bool wr, uint64_t *pa);            // L1 (1024) -> L2 (1024)
 *
 * Driver:
 *   Run a 7-VA access pattern through translate(); print hits, misses.
 *   Build iommu_ctx; show L1-missing, L2-missing, RO-write faults.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define PG 12u
#define MASK ((1u << PG) - 1u)
#define ENTS 4

typedef struct { uint32_t vpn, pfn; bool valid; } tlb_t;
static tlb_t tlb[ENTS];
static int next_evict = 0;
static unsigned hits, misses;

/* TODO: page_walk(vpn), translate(va) */

typedef struct { uint32_t pfn; bool present, writable; } io_pte_t;
typedef struct { io_pte_t pt[1024]; bool present; } io_pd_t;
typedef struct { uint16_t bdf; io_pd_t *pd[1024]; } iommu_ctx_t;

/* TODO: iommu_translate */

int main(void) {
    /* TODO */
    return 0;
}

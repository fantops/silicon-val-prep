/* E1 — 4-level page walk (PML4 / PDPT / PD / PT).
 *
 * Implement (see ../EXERCISES.md):
 *   uint64_t walk(uint64_t va, const char **why);
 *      returns physical addr on hit, 0 on fault (and *why set).
 *
 * Driver:
 *   Map VA 0x0000 -> PA 0x1000, VA 0x1000 -> PA 0x2000.
 *   Walk 0, 0x1000 (success) and 0x2000 (fault).
 */
#include <stdio.h>
#include <stdint.h>

#define PG_SHIFT 12
#define IDX(va, lvl) (((va) >> (PG_SHIFT + 9 * (lvl))) & 0x1FFu)

typedef uint64_t pte_t;
#define P  (1ull << 0)
#define RW (1ull << 1)
#define PFN(p) ((p) & ~0xFFFull)

static pte_t pml4[512], pdpt[512], pd[512], pt[512];

/* TODO: walk */

int main(void) {
    /* TODO: populate tables, walk three VAs, print results */
    return 0;
}

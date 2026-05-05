/* E1 — C-state request + PkgC6 PCU gate model.
 *
 * Implement (see ../EXERCISES.md):
 *   bool request_core_cs(int id, cstate_t target);
 *   bool try_pkg_c6(void);
 *   const char *blocker(void);
 *
 * Driver:
 *   All 4 cores -> C6. try_pkg_c6() blocked by PCIe (default false).
 *   Set pcie_in_l1_2 = true, retry -> PkgC6 ENTERED.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum { C0 = 0, C1 = 1, C1E = 2, C6 = 6 } cstate_t;

#define NCORES 4
typedef struct { cstate_t cs; bool work_pending; } core_t;
typedef struct {
    bool llc_idle, mc_idle, pcie_in_l1_2, display_idle, intc_idle;
} uncore_t;

static core_t cores[NCORES];
static uncore_t un = { true, true, false, true, true };

/* TODO: blocker, request_core_cs, try_pkg_c6 */

int main(void) {
    /* TODO */
    return 0;
}

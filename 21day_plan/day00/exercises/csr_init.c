/* E10 — table-driven CSR init.
 *
 *   struct csr_init { uintptr_t off; uint32_t val; uint32_t mask; };
 *
 *   void apply_csr_init(volatile uint8_t *base,
 *                       const struct csr_init *tbl, size_t n);
 *
 * Rules:
 *   - mask == 0xFFFFFFFF  -> full write of val
 *   - else                 -> RMW: *(base+off) = (*(base+off) & ~mask) | (val & mask)
 *   - print every applied step (offset, before, after)
 *
 * Driver:
 *   - allocate 256 bytes "MMIO"
 *   - run a table of ~8 init writes (mix of full and RMW)
 *   - dump final block
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* TODO */

int main(void) { return 0; }

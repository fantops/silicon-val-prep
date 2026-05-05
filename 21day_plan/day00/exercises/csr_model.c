/* E9 — fake silicon CSR model (seed of a pre-Si reference model).
 *
 * Build a tiny in-memory model:
 *   struct csr_model {
 *       uint32_t CTRL;
 *       uint32_t STATUS;
 *       uint32_t DATA;
 *       int     internal_step;
 *   };
 *
 * Behaviour:
 *   - Writing CTRL bit 0 = 1 starts an "operation".
 *   - After 3 calls to step(model), STATUS bit 0 (READY) becomes 1, and
 *     DATA holds 0xCAFEBABE.
 *   - Top-level driver writes CTRL, polls STATUS via your wait_for_bit_set
 *     (call step() inside the poll loop to advance time), reads DATA.
 *
 * Why this matters:
 *   This is the structure of a pre-silicon reference model used to validate
 *   firmware before RTL is available. Connects directly to Day 21.
 */
#include <stdint.h>
#include <stdio.h>

/* TODO */

int main(void) { return 0; }

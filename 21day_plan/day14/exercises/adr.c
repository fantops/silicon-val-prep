/* E1 — ADR opcode-sequence simulator (buggy + fixed).
 *
 * Implement (see ../EXERCISES.md):
 *   int adr_run_buggy(adr_state_t *s);   // WPQ_FLUSH before IO_QUIESCE_ACK
 *   int adr_run_fixed(adr_state_t *s);   // explicit handshakes
 *
 * Driver: run both; print in_self_refresh after fixed.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    OP_IO_QUIESCE, OP_IO_QUIESCE_ACK, OP_FABRIC_QUIESCE, OP_LLC_WRITEBACK,
    OP_WPQ_FLUSH, OP_WPQ_FLUSH_ACK, OP_DDR_SELF_REFRESH, OP_NVDIMM_SAVE, OP_DONE
} adr_op_t;

typedef struct {
    bool io_quiesced, fabric_quiesced, wpq_drained, in_self_refresh, eadr;
} adr_state_t;

/* TODO: adr_run_buggy, adr_run_fixed */

int main(void) {
    /* TODO */
    return 0;
}

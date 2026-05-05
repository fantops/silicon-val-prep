/* E1 — PSCI dispatcher + RPMh vote aggregator (Snapdragon-flavoured).
 *
 * Implement (see ../EXERCISES.md):
 *   int psci_call(uint32_t fid, uint64_t a1, uint64_t a2);
 *      handle CPU_ON, CPU_OFF, SYSTEM_SUSPEND, SYSTEM_RESET.
 *   vote_t rpmh_aggregate(void);   // per-field max across reqs[N_REQ]
 *
 * Driver:
 *   CPU_ON 1 @ 0x80000000  -> success
 *   CPU_ON 1 @ 0           -> already-on error
 *   populate 4 votes, aggregate, print
 *   SYSTEM_SUSPEND with resume entry
 */
#include <stdio.h>
#include <stdint.h>

#define PSCI_CPU_ON         0x84000003
#define PSCI_CPU_OFF        0x84000002
#define PSCI_SYSTEM_SUSPEND 0xC400000E
#define PSCI_SYSTEM_RESET   0x84000009

#define MAX_CPUS 8
typedef enum { CPU_OFF, CPU_ON } cpu_state_t;
static cpu_state_t cpus[MAX_CPUS] = { CPU_ON, [1 ... MAX_CPUS-1] = CPU_OFF };

/* TODO: psci_call */

#define N_REQ 4
typedef struct { uint32_t freq_khz; uint32_t volt_mv; } vote_t;
static vote_t reqs[N_REQ];

/* TODO: rpmh_aggregate */

int main(void) {
    /* TODO */
    return 0;
}

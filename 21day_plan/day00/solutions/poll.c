#include <stdio.h>
#include <stdint.h>
#include "macros.h"

/* ---------- low-level register helpers ---------------------------------- */
/* Centralizing reads/writes here means every caller goes through the same
   `volatile`-correct path. If we ever need to add memory barriers, logging,
   or trace hooks for MMIO, we change ONE place instead of every call site. */
static inline uint32_t reg_read32(volatile uint32_t *reg) {
    return *reg;
}
static inline void reg_write32(volatile uint32_t *reg, uint32_t val) {
    *reg = val;
}

/* RMW helper: set bitfield [msb:lsb] = val, leave other bits alone.
   Both the "polling" side and the "producing" side use this -- so we can't
   accidentally clobber neighbor bits in one place but not the other. */
static inline void reg_set_field(volatile uint32_t *reg,
                                 uint8_t msb, uint8_t lsb, uint32_t val) {
    reg_write32(reg, FIELD_WRITE(reg_read32(reg), val, msb, lsb));
}

/* ---------- producer (the "different caller in the middle") ------------- */
/* Imagine this is another thread, an ISR, or a remote agent that eventually
   writes the "ready" pattern into the register. For the demo we trigger it
   from inside the poll loop after a few iterations. */
static void producer_make_ready(volatile uint32_t *reg) {
    /* Write 0xF into bits [5:2] -- this is the value the poller waits for. */
    reg_set_field(reg, 5, 2, 0xF);
    printf("  [producer] wrote field [5:2]=0xF, reg=0x%08X\n", reg_read32(reg));
}

/* ---------- poll helper ------------------------------------------------- */
/* Wait until (*reg & mask) == mask, up to `tries` attempts.
   Returns 1 on success, 0 on timeout. */
static int wait_for_bits_set(volatile uint32_t *reg, uint32_t mask, uint8_t tries) {
    for (uint8_t i = 0; i < tries; i++) {
        uint32_t v = reg_read32(reg);
        printf("  [poll %2u] reg=0x%08X  (reg & mask)=0x%08X  want=0x%08X\n",
               i, v, v & mask, mask);

        if ((v & mask) == mask)
            return 1;

        /* Simulate "something else runs between poll iterations" --
           in real code this is the scheduler, an ISR, or another core. */
        if (i == 3)
            producer_make_ready(reg);

        /* In real userspace: usleep(1); in a kernel: udelay(1) / cpu_relax(). */
    }
    return 0;
}

/* ---------- driver ------------------------------------------------------ */
int main(void) {
    /* Fake CSR. `volatile` so the compiler doesn't cache reads. */
    volatile uint32_t r = 0xDEAD0000U;     /* bits [5:2] start as 0 */

    /* The mask is "all ones inside the field". FIELD_PREP(0xF, 5, 2) and
       MASK(5, 2) both give 0x3C here. Using MASK is the clearer intent. */
    uint32_t mask = MASK(5, 2);

    printf("initial reg = 0x%08X, waiting for bits %s set...\n",
           r, "[5:2]");

    if (wait_for_bits_set(&r, mask, 10))
        printf("OK: bits became set. Final reg = 0x%08X\n", r);
    else
        printf("TIMEOUT: bits never set. Final reg = 0x%08X\n", r);

    return 0;
}
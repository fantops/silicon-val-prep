#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "macros.h"

/* Fake CSR living in normal memory. `volatile` tells the compiler not to
   cache or reorder accesses through this pointer, the same way real MMIO
   registers must be treated. */
static volatile uint32_t fake_csr;

static inline uint32_t reg_read32(volatile uint32_t *reg) {
    return *reg;
}

static inline void reg_write32(volatile uint32_t *reg, uint32_t val) {
    *reg = val;
}

/* Read-Modify-Write a bitfield [msb:lsb] with `val`. */
static inline void reg_rmw_field(volatile uint32_t *reg,
                                 uint8_t msb, uint8_t lsb, uint32_t val) {
    uint32_t cur = reg_read32(reg);
    uint32_t new_val = FIELD_WRITE(cur, val, msb, lsb);
    reg_write32(reg, new_val);
}

static void dump(const char *tag, uint32_t v) {
    printf("%-22s = 0x%08X\n", tag, v);
}

int main(void) {
    /* Seed the CSR with a recognizable pattern. */
    reg_write32(&fake_csr, 0xDEADBEEFU);
    dump("initial fake_csr", fake_csr);

    /* --- RMW: write 0x12 into bits [8:4] --- */
    const uint8_t MSB = 8, LSB = 4;
    const uint32_t NEW_FIELD = 0x12U;             /* 5-bit value */
    const uint32_t FIELD_MASK = MASK(MSB, LSB);   /* = 0x000001F0 */

    uint32_t before    = reg_read32(&fake_csr);
    uint32_t untouched = before & ~FIELD_MASK;    /* bits we must preserve */

    reg_rmw_field(&fake_csr, MSB, LSB, NEW_FIELD);

    uint32_t after = reg_read32(&fake_csr);
    dump("after RMW [8:4]=0x12", after);

    /* --- Proofs --- */
    /* 1. Field now holds the new value. */
    assert(FIELD_READ(after, MSB, LSB) == NEW_FIELD);
    /* 2. Every bit OUTSIDE the field is identical to before. */
    assert((after & ~FIELD_MASK) == untouched);
    /* 3. after == (before with field cleared) | (new field shifted in) */
    assert(after == (untouched | (NEW_FIELD << LSB)));

    /* --- Second RMW on a different field shouldn't disturb the first --- */
    const uint8_t MSB2 = 27, LSB2 = 24;           /* nibble at [27:24] */
    const uint32_t NEW2 = 0x5U;
    uint32_t before2 = reg_read32(&fake_csr);

    reg_rmw_field(&fake_csr, MSB2, LSB2, NEW2);
    uint32_t after2 = reg_read32(&fake_csr);
    dump("after RMW [27:24]=0x5", after2);

    assert(FIELD_READ(after2, MSB2, LSB2) == NEW2);
    /* Bits [8:4] from the first RMW must still be 0x12. */
    assert(FIELD_READ(after2, MSB, LSB) == NEW_FIELD);
    /* All bits outside [27:24] equal what was there before this RMW. */
    assert((after2 & ~MASK(MSB2, LSB2)) == (before2 & ~MASK(MSB2, LSB2)));

    puts("RMW preserved untouched bits. All assertions passed.");
    return 0;
}
#include <assert.h>
#include <stdio.h>
#include "macros.h"

int main(void) {
    /* ---------- BIT ---------- */
    assert(BIT(0)  == 0x00000001U);
    assert(BIT(1)  == 0x00000002U);
    assert(BIT(7)  == 0x00000080U);
    assert(BIT(31) == 0x80000000U);

    /* ---------- MASK(msb, lsb) ---------- */
    assert(MASK(0, 0)   == 0x00000001U);
    assert(MASK(3, 0)   == 0x0000000FU);   /* bits [3:0]  */
    assert(MASK(8, 4)   == 0x000001F0U);   /* bits [8:4]  */
    assert(MASK(15, 8)  == 0x0000FF00U);   /* bits [15:8] */
    assert(MASK(7, 7)   == 0x00000080U);   /* single bit  */
    assert(MASK(31, 0)  == 0xFFFFFFFFU);   /* full word, no 1U << 32 */

    /* ---------- FIELD_READ ---------- */
    /* r = 0xDEADBEEF; bits [8:4]:
       byte0 (bits 7:0)  = 0xEF = 1110_1111
       byte1 (bits 15:8) = 0xBE = 1011_1110
       bits[8:4] = bit8(0) bit7(1) bit6(1) bit5(1) bit4(0) = 01110 = 0xE   */
    assert(FIELD_READ(0xDEADBEEFU, 8, 4)  == 0xEU);
    assert(FIELD_READ(0xDEADBEEFU, 3, 0)  == 0xFU);
    assert(FIELD_READ(0xDEADBEEFU, 31, 28) == 0xDU);
    assert(FIELD_READ(0x00000000U, 15, 0) == 0x0U);
    assert(FIELD_READ(0xFFFFFFFFU, 15, 8) == 0xFFU);

    /* ---------- FIELD_WRITE ---------- */
    /* Replace bits [8:4] of 0xDEADBEEF with 0x12 (= 0b10010)      */
    {
        uint32_t r   = 0xDEADBEEFU;
        uint32_t out = FIELD_WRITE(r, 0x12U, 8, 4);
        /* old field was 0xE (<<4 = 0x0E0); new field 0x12 (<<4 = 0x120) */
        /* expected = (r & ~0x1F0) | 0x120                               */
        uint32_t expected = (r & ~0x1F0U) | 0x120U;
        assert(out == expected);
        assert(FIELD_READ(out, 8, 4) == 0x12U);
        /* bits outside [8:4] must be untouched */
        assert((out & ~0x1F0U) == (r & ~0x1F0U));
    }

    /* writing 0 clears the field */
    assert(FIELD_READ(FIELD_WRITE(0xFFFFFFFFU, 0, 8, 4), 8, 4) == 0U);
    /* writing all-ones sets the field fully */
    assert(FIELD_READ(FIELD_WRITE(0x00000000U, 0x1FU, 8, 4), 8, 4) == 0x1FU);

    /* ---------- ALIGN_UP / ALIGN_DOWN ---------- */
    assert(ALIGN_UP(0U,    16U) == 0U);
    assert(ALIGN_UP(1U,    16U) == 16U);
    assert(ALIGN_UP(15U,   16U) == 16U);
    assert(ALIGN_UP(16U,   16U) == 16U);
    assert(ALIGN_UP(17U,   16U) == 32U);
    assert(ALIGN_UP(0x1001U, 0x1000U) == 0x2000U);

    assert(ALIGN_DOWN(0U,    16U) == 0U);
    assert(ALIGN_DOWN(15U,   16U) == 0U);
    assert(ALIGN_DOWN(16U,   16U) == 16U);
    assert(ALIGN_DOWN(31U,   16U) == 16U);
    assert(ALIGN_DOWN(0x1FFFU, 0x1000U) == 0x1000U);

    /* ---------- IS_ALIGNED ---------- */
    assert( IS_ALIGNED(0U,    16U));
    assert( IS_ALIGNED(16U,   16U));
    assert( IS_ALIGNED(0x1000U, 0x1000U));
    assert(!IS_ALIGNED(1U,    16U));
    assert(!IS_ALIGNED(15U,   16U));
    assert(!IS_ALIGNED(0x1001U, 0x1000U));

    /* ---------- POW2 ---------- */
    assert( POW2(1U));
    assert( POW2(2U));
    assert( POW2(4U));
    assert( POW2(0x80000000U));
    assert(!POW2(0U));     /* 0 is not a power of 2 */
    assert(!POW2(3U));
    assert(!POW2(6U));
    assert(!POW2(0xFFU));

    puts("All macro assertions passed.");
    return 0;
}

/* macros.h — shared bit / field / alignment macros for all coding exercises. */
#ifndef CODING_MACROS_H
#define CODING_MACROS_H

#include <stdint.h>

#define BIT(n)                  (UINT32_C(1) << (n))
#define MASK(msb, lsb)          ((UINT32_MAX >> (31 - ((msb) - (lsb)))) << (lsb))
#define FIELD_GET(r, msb, lsb)  (((r) & MASK(msb, lsb)) >> (lsb))
#define FIELD_READ(r, msb, lsb) FIELD_GET(r, msb, lsb)  /* legacy alias */
#define FIELD_PREP(v, msb, lsb) (MASK(msb, lsb) & ((v) << (lsb)))
#define FIELD_RESET(r, msb, lsb) ((r) & ~MASK(msb, lsb))
#define FIELD_WRITE(r, v, msb, lsb) (FIELD_PREP(v, msb, lsb) | FIELD_RESET(r, msb, lsb))

#define ALIGN_UP(x, a)   (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)
#define POW2(n)          ((n) && !((n) & ((n) - 1)))
#define ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))

#endif /* CODING_MACROS_H */

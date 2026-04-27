#include <stdint.h>

#define BIT(n) (1U<<(n))
#define MASK(msb,lsb) (((BIT(msb-lsb+1)) - 1) << lsb)
#define FIELD_READ(r,msb,lsb) (((r) & (MASK(msb,lsb))) >> lsb)
#define FIELD_PREP(val, msb, lsb) (MASK(msb,lsb) & (val << lsb))
#define FIELD_RESET(r,msb,lsb) (((r) & ~(MASK(msb,lsb))))
#define FIELD_WRITE(r,val,msb,lsb) ((FIELD_PREP(val,msb,lsb)) | (FIELD_RESET(r,msb,lsb)))
#define ALIGN_UP(x,a) (((x)+a-1) & (~(a-1)))
#define ALIGN_DOWN(x,a) ((x) & (~(a-1)))
#define IS_ALIGNED(x,a) (((x) & (a-1)) == 0)
#define POW2(n) (n && !(n & (n-1)))
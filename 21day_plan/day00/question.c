#include <stdint.h>
#include <stdio.h>

#define BIT(n) (1U<<(n))
#define MASK(msb,lsb) ((((1U<<((msb)-(lsb)+1))-1))<<(lsb))
#define FIELD_GET(reg,msb,lsb) (((reg)&MASK(msb,lsb))>>(lsb))
#define FIELD_PREP(val,msb,lsb) (((val)<<(lsb)) & MASK(msb,lsb))
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

static void print_bin32(const char *label, uint32_t v) {
    printf("%-22s 0x%08X  0b", label, v);
    for (int i = 31; i >= 0; i--) {
        putchar((v >> i) & 1 ? '1' : '0');
        if (i % 4 == 0 && i) putchar('_');
    }
    putchar('\n');
}

static void printf_bin32(const char *label, uint32_t v){
    for (int i=31;i>=0;i--){
        printf("%d ",(((v>>i)&1) ? 1 : 0));
    }
    printf("\n");
}

int main(){
    uint32_t r = 0xDEADBEEF;

    uint32_t field   = FIELD_GET(r, 8, 4);
    uint32_t mask    = MASK(8, 4);
    uint32_t prep    = FIELD_PREP(0x12, 8, 4);
    uint32_t updated = prep | (r & ~mask);
    uint32_t bit26   = (r & BIT(26)) >> 26;

    printf_bin32("r          =", r);
    printf_bin32("MASK(8,4)  =", mask);
    printf_bin32("r & ~MASK  =", r & ~mask);
    printf_bin32("FIELD_PREP =", prep);
    printf_bin32("updated    =", updated);

    printf("\nFIELD_GET(r,8,4)  = 0x%X (expect 0x1E)\n", field);
    printf("updated field    = 0x%X (expect 0x12)\n", FIELD_GET(updated, 8, 4));
    printf("BIT(26) of r     = %u  (expect 1)\n", bit26);

    return 0;
}
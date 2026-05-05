/* E1 — bit & field macros driver
 *
 * Goal: prove your macros in ../macros.h are correct.
 *
 * Tasks (see ../EXERCISES.md):
 *   1. Print MASK(15,8), FIELD_GET(0xDEADBEEF, 15, 8) -> expect 0xBE
 *   2. FIELD_WRITE(0xDEADBEEF, 0xAA, 15, 8) -> expect 0xDEADAAEF
 *   3. Show BIT(31) == 0x80000000 without UB
 *   4. Assert IS_ALIGNED + POW2 behave
 *
 * Build:
 *   cc -Wall -Wextra -Wpedantic -std=c11 -I.. ../macros.h-includer ...
 *   (Easiest: from day00/  ->  cc -I. exercises/test_macros.c -o /tmp/tm)
 */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "macros.h"

static void print_bin32(const char *label, uint32_t v) {
    printf("%-18s 0x%08X  0b", label, v);
    for (int i = 31; i >= 0; i--) {
        putchar((v >> i) & 1 ? '1' : '0');
        if (i % 4 == 0 && i) putchar('_');
    }
    putchar('\n');
}

int main(void) {
    /* TODO: implement and assert each task above. */
    (void)print_bin32;
    return 0;
}

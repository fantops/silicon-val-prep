/* E8 — struct sizes + padding.
 *
 * Predict sizeof BEFORE running. Then print and explain padding for each.
 *
 * For each struct write a comment:
 *   - field offsets
 *   - where the padding bytes live
 *   - natural alignment of each field on x86-64 / ARMv8
 *   - why pragma pack(1) matters for MMIO/PCIe overlays
 *   - and why pack(1) is dangerous on ARM (alignment trap)
 */
#include <stdio.h>
#include <stddef.h>

struct A { char a; int b; char c; };
struct B { int b; char a; char c; };
struct C { char a; char c; int b; };
struct D { char a; double b; char c; };
#pragma pack(push,1)
struct E { char a; int b; char c; };
#pragma pack(pop)

int main(void) {
    /* TODO: print sizeof + offsetof for each member of each struct */
    return 0;
}

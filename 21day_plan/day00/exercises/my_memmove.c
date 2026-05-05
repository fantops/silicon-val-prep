/* E5 — memmove (overlap-safe).
 *
 * void *my_memmove(void *dst, const void *src, size_t n);
 *
 * Cases to test:
 *   1. dst < src overlap   (forward copy is safe)
 *   2. dst > src overlap   (must copy backwards)
 *   3. dst == src          (no-op)
 *
 * Be ready to defend the exact safety check:
 *   forward-safe iff (dst < src) || (dst >= src + n)
 */
#include <stddef.h>
#include <stdio.h>

/* TODO: implement my_memmove */

int main(void) {
    /* TODO: tests + prints */
    return 0;
}

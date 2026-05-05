/* E4 — your own memcpy.
 *
 * void *my_memcpy(void *dst, const void *src, size_t n);
 *
 * Constraints:
 *   - byte-by-byte first; clean under -Wall -Wextra -Wpedantic
 *   - test n=0, n=1, no-overlap, then a deliberately-overlapping case
 *     (note in a comment: "overlap is UB for memcpy; use memmove")
 *
 * Stretch: word-aligned fast path. Why does it win on cache lines? (Day 4)
 */
#include <stddef.h>
#include <stdio.h>

/* TODO: implement my_memcpy */

int main(void) {
    /* TODO: tests + prints */
    return 0;
}

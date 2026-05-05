/* E2 — First Bad Version (LC 278). Predicate binary search.
 *
 * int first_bad_version(int n);     // smallest v in [1, n] with is_bad(v)
 *
 * Provide a stub is_bad(v) that returns v >= FIRST_BAD (set FIRST_BAD in main).
 * Use mid = lo + (hi - lo) / 2 to avoid overflow.
 */
#include <stdio.h>
#include <stdbool.h>

static int first_bad_global = 0;
static bool is_bad(int v) { return v >= first_bad_global; }

/* TODO: first_bad_version(int n) */

int main(void) {
    /* TODO: set first_bad_global, call first_bad_version, print result */
    return 0;
}

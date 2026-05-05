/* E2 — Two Sum (LC 1)
 *
 * Given nums[n] and target, return indices i, j with nums[i] + nums[j] == target.
 * Single-pass hash map; O(n) time, O(n) space.
 *
 * Signature:
 *   int two_sum(const int *nums, int n, int target, int *out_i, int *out_j);
 *     return 1 if found (and fill outs), 0 otherwise.
 */
#include <stdio.h>
#include <stdlib.h>

/* TODO: implement two_sum (you may use a simple open-address hash table). */

int main(void) {
    int nums[] = {2, 7, 11, 15};
    int n = sizeof(nums) / sizeof(nums[0]);
    int i, j;
    /* TODO: call two_sum, print result, expect (0, 1) for target = 9 */
    (void)nums; (void)n; (void)i; (void)j;
    return 0;
}

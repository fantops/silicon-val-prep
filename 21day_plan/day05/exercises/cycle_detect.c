/* E2 — Linked-list cycle detection (LC 141, Floyd's tortoise-and-hare).
 *
 * int has_cycle(struct node *head);    // 1 if cycle, 0 otherwise
 *
 * Driver:
 *   build acyclic 1->2->3->4->5      -> expect 0
 *   build cyclic where tail->next = node 3 -> expect 1
 */
#include <stdio.h>
#include <stdlib.h>

struct node { int v; struct node *next; };

/* TODO: has_cycle */

int main(void) {
    /* TODO: construct both lists, test, free safely (avoid double-free on cycle) */
    return 0;
}

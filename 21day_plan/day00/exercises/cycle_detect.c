/* E7 — Floyd's cycle detection (tortoise and hare).
 *
 * int  has_cycle(struct node *head);
 * struct node *cycle_start(struct node *head);   // bonus
 *
 * Tests:
 *   - acyclic list  -> 0
 *   - tail->next = node3 -> 1; cycle_start->v == 3
 *
 * Talking points (write as comments):
 *   - Why O(1) memory beats a 'seen[]' set in firmware
 *   - Where this pattern shows up in IRQ-storm / log replay
 */
#include <stdio.h>
#include <stdlib.h>

struct node { int v; struct node *next; };

/* TODO */

int main(void) { return 0; }

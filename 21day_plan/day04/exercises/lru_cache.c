/* E2 — LRU Cache (LC 146): O(1) get/put.
 *
 * typedef struct LRU LRU;
 * LRU *lru_create(int cap);
 * int  lru_get(LRU *l, int key);    // -1 if missing
 * void lru_put(LRU *l, int key, int val);
 * void lru_free(LRU *l);
 *
 * Strategy: hash map (key -> node*) + intrusive doubly-linked list.
 * Move-to-front on access. Evict tail on overflow.
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct LRU LRU;

/* TODO: define node + LRU; implement create/get/put/free */

int main(void) {
    /* TODO: drive a small sequence and print get results */
    return 0;
}

/* E1 — Aligned allocator + container_of demo.
 *
 * Implement (see ../EXERCISES.md):
 *   ALIGN_UP, IS_ALIGNED, IS_POW2
 *   container_of(ptr, type, member)
 *   void *aligned_malloc(size_t size, size_t align);
 *   void  aligned_free(void *p);
 *
 * Driver:
 *   1. aligned_malloc(4096, 64) -> print, assert addr % 64 == 0, free.
 *   2. container_of demo via intrusive list_head member of struct work.
 *   3. Print sizeof for two structs differing only in field order; explain padding.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

/* TODO: ALIGN_UP, IS_ALIGNED, IS_POW2, container_of */

/* TODO: aligned_malloc / aligned_free */

struct list_head { struct list_head *next; };
struct work { int id; struct list_head node; };

int main(void) {
    /* TODO */
    return 0;
}

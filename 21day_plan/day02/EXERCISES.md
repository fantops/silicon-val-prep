# Day 02 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day02_pointers_memory_layout_alignment.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: aligned allocator + container_of

**Problem statement:** Build a manual aligned allocator that satisfies arbitrary power-of-2 alignment on top of plain `malloc`, plus a Linux-style `container_of` macro. Demonstrate both on an intrusive list. Also print `sizeof` for two structs that differ only in field order to show padding is layout-driven.

**You must implement:**
```c
#define ALIGN_UP(x, a)
#define IS_ALIGNED(x, a)
#define IS_POW2(n)
#define container_of(ptr, type, member)

void *aligned_malloc(size_t size, size_t align);
void  aligned_free(void *p);
```

**Driver / tests to write:**
- `aligned_malloc(4096, 64)` → print pointer, assert `addr % 64 == 0`, then `aligned_free`.
- Build `struct work { int id; struct list_head node; };`, take `&w.node`, recover `&w` via `container_of`, print `back->id`.
- Compare `sizeof(struct {char a; int b; char c;})` vs reordered version; explain padding.

**Be ready to explain in interview:**
- Why does the manual aligned allocator stash the original pointer one slot before the aligned address?
- Why is `container_of` foundational to Linux callbacks? What does `offsetof` rely on?
- What does `__attribute__((packed))` cost on ARM that it doesn't on x86?

## E2 — DS&A: Reverse Linked List (LC 206)

**Problem statement:** Reverse a singly-linked list in place using 3 pointers (`prev`, `curr`, `next`).
**Signature:** `struct node *reverse_list(struct node *head);` where `struct node { int v; struct node *next; };`
**Constraints / edge cases:** empty list, single node, two-node, general N.
**Talking points:** O(n) time, O(1) space; same 3-pointer dance shows up in any in-place pointer surgery (queue rotation, stack reversal).

## E3 — Architect Drills (no code, write your answers)

**D1.** Why is alignment a hardware requirement at all — why don't CPUs just rotate-and-mask any byte boundary? Why did ARMv7 trap on unaligned and ARMv8 mostly allow it?

**D2.** Why does a 60-byte DMA buffer at a 16-byte-aligned address corrupt silently while a 64-byte buffer at the same address is fine? Walk the cache-line straddle timing diagram.

**D3.** Struct field reordering can save 33% of memory. Why doesn't C reorder automatically? Why does Rust?

**D4.** `container_of` is the foundation of Linux callbacks. What would force a future kernel to drop it?

**D5.** Bare-metal firmware bans `malloc`. Construct the workload where you'd need it back. What does Zephyr's `k_heap` cost in worst-case latency?

**D6.** `__attribute__((packed))` is a footgun on ARM. What architectural choice would make it safe — and at what cost?

**D7.** Five segments (text/data/bss/heap/stack) is textbook. On a 64 KB-SRAM MCU with no MMU, does segmentation still earn its keep?

## E4 — Bug Anchor

**Scenario:** A 60-byte DMA descriptor sits at offset 56 in a 64-byte cache line. The device fetches it and the second half arrives stale. Walk the debug tree: what tool surfaces the straddle? What allocator change fixes it? Cross-link to Day 4 (cache lines), Day 8 (DMA visibility).

Also debug: `*regs = defaults;` (struct overlay onto MMIO) silently clears W1C status bits. Why?

## Reference solution

Once you've attempted, see `../day02_pointers_memory_layout_alignment.md` sections "Coding A" and "Coding B" for full solutions and discussion.

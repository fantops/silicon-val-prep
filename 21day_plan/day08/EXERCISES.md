# Day 08 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day08_dma_doorbells_ordering_barriers.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: SQ ring + doorbell harness

**Problem statement:** Model the producer side of an NVMe-style submission queue. Build a 16-entry ring, a `volatile` MMIO doorbell stand-in, a `sq_submit(cmd, arg)` that fills the descriptor, issues a memory barrier, advances the tail, and writes the doorbell. Then a `device_consume_one()` that simulates the device picking up the next entry from `sq_head`.

**You must implement:**
```c
typedef struct { uint32_t cmd; uint32_t arg; } sqe_t;
int  sq_submit(uint32_t cmd, uint32_t arg);  /* 0 ok, -1 full */
void device_consume_one(void);

#define COMPILER_BARRIER() __asm__ volatile("" ::: "memory")
#define MEMORY_BARRIER()   __sync_synchronize()
#define CACHE_CLEAN(p, n)  /* stub */
```

**Driver / tests to write:**
- `sq_submit(0x01, 0xAAAA)`, `sq_submit(0x02, 0xBBBB)`.
- `device_consume_one()` × 3 (third should be empty).
- Print the head/tail movement after each.

**Be ready to explain in interview:**
- Why descriptor → barrier → doorbell, *always* in that order? Why is the failure intermittent?
- `volatile` vs `dmb st` vs cache clean — what does each enforce?
- ARM `DMB ISH` vs `OSH` vs `SY` — when does each apply?
- IO-coherent vs non-coherent DMA paths — what changes in the driver?

## E2 — DS&A: Merge Two Sorted Lists (LC 21)

**Problem statement:** Merge two sorted singly-linked lists into one sorted list, splicing nodes in place (no new allocation).
**Signature:** `struct node *merge_two(struct node *a, struct node *b);`
**Constraints / edge cases:** either list empty, lists of very different lengths, ties (stable order).
**Talking points:** O(n+m), O(1) extra; same shape as draining two CQs into one event log, or merging two snoop streams.

## E3 — Architect Drills (no code, write your answers)

**D1.** Why doorbells at all — why not stream descriptors over a memory-mapped FIFO?

**D2.** Why does ARM expose weak ordering when x86 TSO "just works"? What would break if ARM moved to TSO?

**D3.** Why per-device IOMMU translation when the CPU already has an MMU?

**D4.** Why are MMIO writes posted by default? When do you actually want non-posted?

**D5.** Three ARM barrier scopes (`ish`/`osh`/`sy`). Why three? What's the cost of always using `sy`?

**D6.** With fully-coherent accelerators, which driver-stack artefacts become obsolete? Which survive?

**D7.** Why is the descriptor → barrier → doorbell → DMA → completion → MSI pipeline 6 stages, not 4?

## E4 — Bug Anchor

**Symptoms:** Driver works on x86 but hangs on ARM under sustained load. At low rate it's fine; at line rate completions stall and descriptors look corrupted. Walk the debug tree: what's the missing barrier, what scope, where does it go in the submit path?

Also: status register reads return the same value forever. PTE attributes? `MAIR` setting? `ioremap` flag?

## Reference solution

Once you've attempted, see `../day08_dma_doorbells_ordering_barriers.md` sections "Coding A" and "Coding B" for full solutions and discussion.

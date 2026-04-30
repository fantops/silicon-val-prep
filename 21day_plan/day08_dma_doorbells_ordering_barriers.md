# Day 08 — DMA, Doorbells, Memory Ordering, Barriers

## Concept

A typical device-submission flow is **descriptor → barrier → MMIO doorbell → device DMA → completion**. Each arrow has a real failure mode: descriptor stuck in a dirty cache line, doorbell reordered ahead of the descriptor write on weak memory, IOMMU mapping missing, MMIO accidentally cached. Today consolidates all four into one mental model and one C harness.

## Why this day in the flow

Days 4-5 gave you cache lines and coherence. Days 6-7 gave you VM and IOMMU. Day 8 is the day they all converge: **the device is just another agent that needs to see the latest copy of memory, in the right order, at the right physical address.** Once today is solid, NVMe (Day 11), PCIe TLP/credit (Day 10), and NPU runtime DMA (Day 18-19) are scaffolding work.

In the Layer map this is the **Layer 2 → Layer 3 bridge**.

---

## Deep Study Notes

### 1. The canonical descriptor + doorbell flow

```
1. Driver fills a descriptor / queue entry in normal memory.
2. (If non-coherent) Clean / flush the descriptor cache line.
3. Memory barrier — descriptor must be visible before the doorbell.
4. MMIO doorbell write — uncached, side-effecting.
5. Device DMA — sends IOVA → IOMMU/SMMU → PA → reads descriptor.
6. Device performs work and writes completion to host memory.
7. (If non-coherent) Invalidate completion line; CPU reads completion.
```

Every step has a failure mode. The picture below is the one to draw on the whiteboard.

```
   CPU                     |  Device
   ───                     |  ──────
   write descriptor  ──┐   |
   clean cache line  ─►│   |
   memory barrier    ─►│   |
   MMIO doorbell  ─────┼──►│ DMA reads descriptor (via IOMMU)
                       │   │ DMA reads payload
                       │   │ DMA writes completion
   invalidate cache ◄──┼───┤
   read completion  ◄──┘   │
```

### 2. `volatile` vs barrier vs cache maintenance

| Mechanism | What it controls | Where it ends |
|---|---|---|
| `volatile` | compiler load/store emission and ordering w.r.t. other `volatile` | does **not** affect CPU reorder |
| memory barrier (`dmb`, `dsb`, `mfence`, `__sync_synchronize`) | CPU reorder | does **not** flush cache |
| cache maintenance (`dc cvac`, `clflush`, `clwb`) | data movement between cache and memory | does **not** order independent loads/stores |

You sometimes need all three, in that order: `volatile` ensures the store is emitted, the barrier ensures the device sees it after the descriptor, the maintenance ensures the descriptor reaches DRAM if the device is non-coherent.

### 3. Memory models in one paragraph

- **x86 (Total Store Order):** loads do not reorder with loads; stores do not reorder with stores; loads can be reordered before older stores to a different address. For descriptor-then-doorbell on x86, `volatile` plus the implicit `sfence` from the doorbell store-to-uncached is usually enough. Add `mfence` for descriptor-vs-completion ordering.
- **ARM / RISC-V (weakly ordered):** loads and stores can be reordered widely. The descriptor write must be ordered before the doorbell write with **`dmb st`** (Data Memory Barrier, store side) or **`dsb`** (Data Synchronization Barrier — stronger). Use `dsb sy` between stores and a subsequent device-side action.

The single most-asked exam question: *"On ARM, why does the device DMA-read garbage from the descriptor?"* — missing barrier between descriptor write and doorbell write.

### 4. Cache and DMA — coherent vs non-coherent

| System | Behaviour |
|---|---|
| Coherent DMA (Intel server, modern Apple Silicon, ARMv8 with hardware coherence) | Device DMA participates in the coherence protocol; CPU caches stay coherent automatically. |
| Non-coherent DMA (many embedded, some accelerators) | Driver must clean (CPU→DRAM) before the device reads, and invalidate (drop CPU copy) after the device writes. |

A **clean** writes a dirty line to DRAM and keeps it valid in cache. **Invalidate** drops the CPU copy without writing back. **Clean+invalidate** does both; safest after a device write.

DMA buffers should be **cache-line aligned** and sized in multiples of one line so that maintenance ops don't touch neighbour data.

### 5. MMIO accidentally cached

If the page-table cache attribute (`PCD/PWT` on x86, MAIR on ARM) marks the device range as cacheable, register reads can be cached. The CPU re-reads its cached value, status updates from the device side never appear, and the polling loop spins forever even though the device asserted the bit. Always make MMIO ranges **uncached / device-typed** in the PTE; the OS does this for you when you `ioremap` correctly, but bring-up code that maps memory by hand needs to set the attribute explicitly.

### 6. The doorbell write itself

A doorbell is a single store to an MMIO register. Three properties matter:

- **Side-effecting:** writing the doorbell triggers the device to do work; this is **not** memory in the cacheable sense.
- **Non-cacheable:** the store posts on the bus immediately.
- **Posted (PCIe):** the store has no completion the CPU waits for; once on the bus, the CPU moves on.

That last property means: on PCIe, the CPU can issue a doorbell store before the descriptor write has reached system memory. A barrier between them on the CPU side ensures the descriptor is at least globally visible before the device fetches it; the IOMMU/PCIe fabric handles the rest.

### 7. Ring buffers (preview of NVMe SQ/CQ on Day 11)

A submission queue (SQ) is a producer/consumer ring of fixed-size entries:

- Host writes the next entry at `tail`, then **rings the doorbell** (writes the new tail to an MMIO register).
- Device reads from `head`, processes, advances `head`.
- Host reads `head` to know how many entries the device has consumed.

The mirror, **completion queue (CQ)**:

- Device writes a completion at its tail with a **phase bit** that flips each lap.
- Host polls the next CQ entry's phase bit; when it matches the expected phase, the entry is valid.
- Host writes the new CQ head doorbell to tell the device space is free.

The phase bit avoids the host needing a separate "valid" doorbell from the device for each completion — the device's write-with-flipped-phase is self-signalling.

### 8. The five things that go wrong in this picture

1. **Cache stale descriptor:** clean missing on non-coherent path.
2. **Reordered doorbell on weak memory:** missing `dmb st` / `dsb sy`.
3. **MMIO cached:** wrong PTE cache attribute.
4. **IOMMU fault:** OS forgot `dma_map_*`, or device used a stale IOVA.
5. **Wrong doorbell stride / register:** NVMe `DSTRD` (CAP[35:32]) miscomputed; doorbell goes to the wrong offset, device never sees it.

Each maps to a Day on this plan: 1 → Day 4-5, 2 → today, 3 → Day 6, 4 → Day 7, 5 → Day 11.

---

## Bugs in this Topic

- **Doorbell visible before descriptor on ARM:** weak-memory reorder; device DMA-reads stale descriptor and either ignores it or executes garbage. Fix: `__asm__ volatile("dmb st" ::: "memory")` between descriptor write and doorbell.
- **Polling loop spins on cached MMIO:** PTE marked cacheable; status reads return the same cached value forever. Fix: ensure PTE is device/uncached; on bare-metal, set MAIR or the right `ioremap` flag.
- **IOMMU fault on first submission:** OS never mapped the descriptor buffer; fault log shows BDF + IOVA + "no entry". Fix: `dma_map_single` (Linux) or equivalent.
- **NVMe `DSTRD` miscomputed (Day 11 anchor):** SQ/CQ doorbells are at `0x1000 + (2*i + (CQ?1:0)) * (4 << CAP.DSTRD)`; getting `DSTRD` wrong puts the doorbell at the wrong offset; device never sees the kick. The `CSTS.RDY` bug surfaces this when the host can't even bring up the admin queue.
- **PCU/ADR `WPQ_FLUSH` before `IO_QUIESCE_ACK` (Day 14 link):** the firmware analogue of issuing the doorbell before the descriptor — same bug class, different domain.

---

## Interview Explanation (60-90 sec)

> "Device submission is descriptor, barrier, MMIO doorbell, device DMA via the IOMMU, completion. Each arrow has a failure mode. On ARM the descriptor write can be reordered after the doorbell, so I always put a `dmb st` between them; `volatile` on its own only orders compiler-side, not CPU-side. On non-coherent DMA paths I clean the descriptor line so the device reads DRAM, not stale cache; after the device writes the completion I invalidate before reading. MMIO ranges must be uncached in the PTE — the classic bring-up bug is a status register that reads back the same value forever because the cache cached it. The IOMMU is the device-side MMU and its fault register tells me whether the OS forgot to map a buffer or the device sent a stale IOVA. The whole picture is the same physics as MESI on Day 5 — the device is just another coherence agent that has to see the latest copy in the right order at the right physical address."

---

## Practice Q&A

**Q1. Why does `volatile` not save you on ARM for descriptor + doorbell?**
`volatile` orders compiler emissions; the CPU is still free to reorder the descriptor store after the doorbell store on weak memory. Need a `dmb st` (or `dsb sy`).

**Q2. Coherent vs non-coherent DMA?**
Coherent: device participates in coherence; nothing for the driver to do. Non-coherent: driver must clean before device reads, invalidate after device writes.

**Q3. Why must MMIO mappings be uncached?**
Otherwise reads can return cached values and status bits appear stuck.

**Q4. Walk an IOMMU fault to a culprit.**
Read fault register → BDF + IOVA + reason. If buffer was mapped: device sent unexpected IOVA (FW/driver). If not: OS missed `dma_map_*` (driver).

**Q5. What is a posted MMIO write?**
A store with no return completion the CPU waits for. Once on the bus, CPU moves on.

**Q6. NVMe doorbell offset for SQ tail of queue 1, given `CAP.DSTRD = 0`?**
`0x1000 + 2*1 * (4 << 0) = 0x1008`. With `DSTRD = 1`: `0x1000 + 2 * 8 = 0x1010`.

**Q7. Phase bit in NVMe completion queue — purpose?**
Self-signal validity; flips each lap so host knows a CQ entry is fresh without a per-completion device→host doorbell.

**Q8. Cache maintenance after device write — clean, invalidate, or both?**
Invalidate (or clean+invalidate). The device wrote DRAM; CPU cached copy is stale; invalidate drops it.

---

## Coding A — Systems C: SQ ring + doorbell harness

```c
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define SQ_ENTRIES 16

typedef struct {
    uint32_t cmd;
    uint32_t arg;
} sqe_t;

static sqe_t sq[SQ_ENTRIES];
static volatile uint32_t sq_tail_db;   /* MMIO doorbell stand-in */
static uint32_t sq_tail = 0;
static uint32_t sq_head = 0;           /* device-maintained, simulated */

/* Compiler+CPU barrier stubs. On ARM, replace with `dmb st`/`dsb sy`. */
#define COMPILER_BARRIER() __asm__ volatile("" ::: "memory")
#define MEMORY_BARRIER()   __sync_synchronize()
#define CACHE_CLEAN(ptr, sz) (void)(ptr) /* no-op on coherent host */

static int sq_submit(uint32_t cmd, uint32_t arg) {
    uint32_t next = (sq_tail + 1) % SQ_ENTRIES;
    if (next == sq_head) return -1;        /* full */
    sq[sq_tail].cmd = cmd;
    sq[sq_tail].arg = arg;
    CACHE_CLEAN(&sq[sq_tail], sizeof(sqe_t));
    MEMORY_BARRIER();                       /* descriptor visible BEFORE doorbell */
    sq_tail = next;
    sq_tail_db = sq_tail;                   /* MMIO doorbell write */
    return 0;
}

/* Simulate device side picking up the entry. */
static void device_consume_one(void) {
    if (sq_head == sq_tail) return;         /* empty */
    sqe_t e = sq[sq_head];                  /* DMA read */
    printf("device: cmd=0x%X arg=0x%X (head %u -> %u)\n",
           e.cmd, e.arg, sq_head, (sq_head + 1) % SQ_ENTRIES);
    sq_head = (sq_head + 1) % SQ_ENTRIES;
}

int main(void) {
    sq_submit(0x01, 0xAAAA);
    sq_submit(0x02, 0xBBBB);
    device_consume_one();
    device_consume_one();
    device_consume_one();   /* empty case */
    return 0;
}
```

## Coding B — Interview DS&A: Merge Two Sorted Lists (LC 21)

Two-pointer merge — same shape as draining a CQ across two ring buffers.

```python
class ListNode:
    def __init__(self, v=0, n=None): self.val, self.next = v, n

def mergeTwoLists(a, b):
    dummy = tail = ListNode()
    while a and b:
        if a.val <= b.val: tail.next, a = a, a.next
        else:              tail.next, b = b, b.next
        tail = tail.next
    tail.next = a or b
    return dummy.next
```

---

## System Thinking — Why CPU↔device handoff has its own physics

**Frame:** when CPU writes to RAM, the cache and the coherence protocol guarantee a consistent view across cores (Day 5). When a *device* reads RAM via DMA, **none of those guarantees apply by default**. The device is on a different bus, with its own ordering rules, looking at memory the CPU may not have flushed yet. Every barrier, every cache-clean operation, every doorbell pattern in this day exists to bridge two worlds that don't share a clock or a coherence protocol.

### Q: Why the descriptor-then-doorbell pattern, *always in that order*?

Because the device reads the descriptor on receiving the doorbell. If the doorbell write reaches the device *before* the descriptor write reaches RAM (or before it's visible to the device's coherent path), the device reads garbage. Three things must happen in order:
1. CPU writes descriptor fields → flushed/visible to device.
2. **Memory barrier** (`DMB ST` on ARM, `sfence` on x86) — guarantees descriptor write completes before any later store.
3. CPU writes doorbell (MMIO write to a device register).

The barrier is the *whole job* of `wmb()` / `dma_wmb()` in Linux drivers. Skip it and you get "works at low rate, fails at line rate" — at slow rates the writes naturally retire in order; at high rates the CPU's store buffer reorders them.

### Q: Why does DMA need cache management when the cache is supposed to be transparent?

Because the cache is *transparent to CPU memory accesses*, not to device memory accesses. Two failure modes:
- **Stale device read:** CPU writes a buffer, lines sit dirty in L1. Device DMAs from RAM and reads old data. **Fix:** `clean` the cache (write dirty lines back to RAM) before triggering DMA.
- **Stale CPU read:** Device DMAs new data into RAM. CPU still has the old value cached. **Fix:** `invalidate` the cache (drop cached lines) before CPU reads.

On IO-coherent systems (modern x86, ARM with `ACE`/`CCI`), the device participates in the coherence protocol and these are handled in hardware. On non-coherent paths (most embedded, many DSPs/NPUs — Day 19), the driver must do explicit `clean`/`invalidate` ops. **Getting this wrong is the canonical NPU/HTP DMA bug: half-coherent fabric, software assumes full coherence, occasional stale reads under load.**

### Q: Why barriers? Doesn't `volatile` already enforce order?

No. `volatile` keeps the compiler from reordering loads/stores to *that one address* (Day 3). Barriers keep the **CPU itself** from reordering loads/stores across the barrier. Modern OoO cores have store buffers, load queues, and out-of-order retirement — they can hold a store for hundreds of cycles before it's visible to other agents. A barrier is the only way to force "all stores before this point are globally visible before any store after." Different barriers for different scopes:
- **Inner shareable** (`DMB ISH`): coherent within the multicore cluster.
- **Outer shareable** (`DMB OSH`): includes IO-coherent devices.
- **System** (`DMB SY`): everything, including non-coherent agents.

Using the wrong scope = subtle correctness bug that passes most tests.

### One-liner

*"CPU↔device handoff is the meeting of two ordering worlds. The descriptor-then-barrier-then-doorbell pattern is the contract: descriptor visible to the device, barrier ensures global ordering, doorbell triggers the device read. Cache management bridges the cache, which is transparent to CPUs but not to non-coherent DMA. Get the order, the barrier scope, or the cache op wrong and you get the classic 'works at low rate, fails at line rate' bug."*

---

## Bug Anchor

- **Day 11 (NVMe `CSTS.RDY` / AQA / DSTRD):** the canonical bring-up bug for this picture.
- **Day 14 (PCU/ADR opcode ordering):** firmware analogue of "doorbell before descriptor."
- **Day 19 (QNN/ORT):** the same picture appears in NPU submission, just with VTCM tiles instead of NVMe blocks.

## Exit Check

- [ ] Drew descriptor → barrier → doorbell → DMA → completion picture from memory.
- [ ] Stated `volatile` vs barrier vs cache maintenance distinctions.
- [ ] Stated x86 TSO vs ARM weak ordering implications for the doorbell pattern.
- [ ] Walked an IOMMU fault to a one-of-two suspect class.
- [ ] Stated why MMIO mappings must be uncached.
- [ ] Computed an NVMe doorbell offset from `DSTRD`.
- [ ] Stated the role of the CQ phase bit.

# Day 06 — Virtual Memory, Page Tables, Page Faults

> **Source merge:** [30day_plan/day12_virtual_memory.md](../30day_plan/day12_virtual_memory.md), [30day_plan/TLB_IOMMU_MEMORY_BRIDGE_NOTES.md](../30day_plan/TLB_IOMMU_MEMORY_BRIDGE_NOTES.md). 30-day files are unchanged.

## Concept

Each process gets its own **virtual address space**; the **MMU** translates virtual to physical at page granularity (typically 4 KB) using a multi-level **page table** anchored by `CR3` (x86) / `TTBRn_EL1` (ARM). Each PTE carries permissions (P / R/W / U/S / NX) and cache attributes (PWT / PCD on x86, MAIR on ARM). A **page fault** is the OS-visible result of a translation that fails — minor (resolvable from RAM) or major (needs I/O).

## Why this day in the flow

After caches (Days 4-5), the next layer is *what address the cache is even asked about*. Every later concept layers on this:

- Day 7 — TLB caches the translations and adds shootdown / huge pages.
- Day 8 — IOMMU/SMMU is the same idea for devices, plus MMIO PTE attributes that decide whether a register access is cached or device-typed.
- Day 19 — the QNN/ORT page-fault bug is *exactly* a virtual-memory regression, not a cache miss.

This is **Layer 2** of the 30-day system flow.

---

## Deep Study Notes

### 1. Why virtual memory exists

Without VM, every process reads/writes physical RAM directly. Three bad consequences:

- **No isolation** — any process can corrupt any other process or the kernel.
- **No abstraction** — programs would need to know what physical addresses are free.
- **No protection** — no way to mark pages read-only, no-execute, kernel-only.

VM gives every process its own private address space; the OS owns the page tables; the MMU enforces translation and permissions on every load/store.

### 2. Page-based translation

Memory is divided into fixed-size **pages**, usually 4 KB on x86/ARM.

```
Virtual:  [ Virtual Page Number (VPN) ] [ Page Offset ]
                       │
                  ┌────▼────┐
                  │ MMU /   │  reads page tables
                  │ TLB     │
                  └────┬────┘
                       │
Physical: [ Physical Page Number (PPN) ] [ Page Offset ]
```

The 12-bit page offset passes through unchanged because the page is contiguous in both spaces. Only the page number is translated.

### 3. The 4-level walk on x86-64

A 64-bit virtual address actually uses 48 bits (top 16 are sign-extended). The VPN splits into four 9-bit indices into four 4 KB tables:

```
[47:39] PML4 | [38:30] PDPT | [29:21] PD | [20:12] PT | [11:0] offset
```

Walk:

1. `CR3` holds the **physical** address of the PML4.
2. PML4[bits 47:39] → PDPT base.
3. PDPT[bits 38:30] → PD base.
4. PD[bits 29:21] → PT base.
5. PT[bits 20:12] → final PPN.
6. PPN || offset = physical address.

Each table is 4 KB (512 × 8 B entries).

ARM is similar (4-level on AArch64 with 48-bit VAs); the names differ (`L0..L3`, `TTBRn_EL1` instead of `CR3`), the principle is the same.

### 4. Cost of a full walk

Up to **4 memory accesses**, each ~100 ns if uncached. ~400 ns just to translate a single address. That is unacceptable, so the **TLB** caches recent translations (Day 7).

### 5. The PTE bits a validation engineer cares about

| Bit | Name | Meaning |
|---|---|---|
| 0 | P (Present) | page is in RAM; if 0, every access faults |
| 1 | R/W | write permission |
| 2 | U/S | user vs supervisor |
| 3 | PWT | page-level write-through |
| 4 | PCD | page-level cache disable — **set for MMIO** |
| 5 | A | accessed (set by HW on read) |
| 6 | D | dirty (set by HW on write) |
| 7 | PS | page size (1 = huge page) |
| 63 | NX | no-execute |

Misprogrammed PWT/PCD on MMIO is a classic SoC bring-up bug: register accesses get cached and behave nondeterministically.

### 6. Page faults — minor vs major

When the MMU sees `P=0` (or a permission violation), it raises `#PF`. The OS handler reads `CR2` (faulting VA), `CR3` (PT base), and the error-code on the stack to decide what to do:

| Class | Cause | Cost |
|---|---|---|
| **Minor** | page is in RAM but PTE wasn't present (e.g., demand-zero, COW, file-backed re-touch) | µs — no I/O |
| **Major** | page is on disk / not yet read from a file mapping | ms — disk I/O |
| **Invalid** | unmapped VA, permission denied, kernel addr from user | SIGSEGV / kill |

In production, **major faults dominate latency** under memory pressure — every fault costs a disk read. **Minor faults are cheap individually but spike CPU and dirty TLB if you take millions per second** (this is the QNN/ORT picture).

### 7. Demand paging, COW, file-backed mmap

- **Demand-zero** — fresh anonymous pages aren't allocated until first touch (minor fault).
- **Copy-on-write (COW)** — `fork()` shares pages read-only; first write triggers a minor fault that allocates a private copy.
- **File-backed mmap** — pages are read from disk on first access (major fault).
- **Lazy faulting in big allocators** — `malloc` of a huge buffer returns instantly; minor faults populate as you touch each page. Spec'd memory pressure can manifest as an unexpected fault storm at "the wrong time."

### 8. CR3 / context switch cost

Every context switch reloads `CR3`, which (without PCID/ASID tagging) flushes the TLB. Modern CPUs use **PCID (x86) / ASID (ARM)** to avoid the flush by tagging TLB entries with the process ID — major performance lever in busy systems.

### 9. Two address worlds — the mental model

```
Software pointer
  → CPU virtual address
  → TLB lookup
  → page table walk on TLB miss
  → physical address
  → cache / DRAM / MMIO access
```

For devices (Day 7-8):

```
Device DMA address (IOVA)
  → IOMMU / SMMU lookup
  → IO page table walk on miss
  → physical DRAM address
```

Same physics, different agent.

### 10. The vocabulary trap

- **Page fault** — translation/allocation event at 4 KB granularity, OS-managed.
- **TLB miss** — translation cache miss; HW page walk happens; resolves silently (unless the walk also faults).
- **Cache miss** — locality event at 64 B granularity, HW-managed, never OS-visible.

These are distinct layers. Calling the QNN/ORT regression a "cache miss" is a vocabulary bug interviewers catch.

---

## Bugs in this Topic

- **QNN/ORT page-fault + virtual-memory regression (Day 19 anchor):** after a context-loading change, ORT's QNN execution provider began touching context buffers in a different lifetime pattern. Many host virtual pages were touched per inference, spiking minor faults and bumping virtual-memory footprint. **Mechanism:** demand-zero faults at 4 KB granularity, not cache misses. **Surface:** ETL/WPA showed `MajorFault`/`MinorFault` events correlating with the new code path. **Fix:** persist context buffers across iterations rather than rebuild per inference; pre-touch and lock pages where appropriate.
- **MMIO accidentally cached:** PTE PWT/PCD wrong for the device range; register accesses get cached; status bits don't update from the device side. Symptom: status register reads stale, often "stuck" at the value it had at first read.
- **Unexpected SIGSEGV in driver:** kernel handed user-space a buffer it shouldn't have access to; PTE permission bits wrong (U/S, R/W).
- **`fork()` storm causes minor-fault thrash:** every COW page in the parent becomes a private copy on first child write; under heavy concurrency this dirties cache, swamps the page allocator, and hurts tail latency.

---

## Interview Explanation (60-90 sec)

> "Virtual memory gives every process its own address space; the MMU translates virtual to physical at page granularity using a 4-level page table on x86-64, anchored by `CR3`. Each PTE carries the present bit, permission bits, accessed/dirty bits, and cache attributes — getting the cache attributes wrong on an MMIO range is a classic bring-up bug because register reads get silently cached. A page fault is the OS-visible result: minor faults are resolvable in microseconds with no I/O, major faults take milliseconds because they need a disk read. The QNN/ORT regression I worked on was a virtual-memory bug, not a cache bug — context-buffer lifetime changed and minor faults spiked at 4 KB granularity per inference. The fix was to persist context buffers across iterations. The vocabulary I always keep straight: page fault = translation event, TLB miss = HW page walk, cache miss = locality event — three different layers."

---

## Practice Q&A

**Q1. Walk `0x0000_7FFF_FFFF_E000` through 4-level x86-64 paging.**
PML4[0xFF], PDPT[0x1FF], PD[0x1FF], PT[0x1FE], offset 0x000.

**Q2. Minor vs major page fault?**
Minor: page is in RAM but PTE wasn't present (COW, demand-zero, freshly touched). Microseconds. Major: page must be read from disk. Milliseconds.

**Q3. Why does the page offset pass through translation unchanged?**
The page is contiguous in both VA and PA spaces; the offset identifies a byte within the page.

**Q4. Which PTE bits matter for an MMIO mapping?**
P, R/W, U/S, **PWT/PCD set so the page is uncached**, NX. PWT/PCD wrong is a textbook MMIO bug.

**Q5. Cost of a full 4-level walk?**
Up to 4 memory accesses (~400 ns uncached); reason TLB exists.

**Q6. PCID / ASID — what do they buy?**
Tag TLB entries with process ID so a context switch doesn't flush the TLB.

**Q7. Was the QNN/ORT regression a cache problem?**
No — it was a virtual-memory / minor-fault problem at 4 KB page granularity, not a 64 B cache locality problem.

**Q8. How does a 2 MB huge page skip a level?**
The PD entry has the PS bit set and points directly to a 2 MB-aligned frame; the PT level is skipped, bits [20:0] become offset.

---

## Coding A — Systems C: 4-level page walk

```c
#include <stdio.h>
#include <stdint.h>

#define PG_SHIFT 12
#define IDX(va, lvl) (((va) >> (PG_SHIFT + 9 * (lvl))) & 0x1FFu)

typedef uint64_t pte_t;
#define P  (1ull << 0)
#define RW (1ull << 1)
#define PFN(p) ((p) & ~0xFFFull)

/* 4 small static tables stand in for real PML4/PDPT/PD/PT. */
static pte_t pml4[512], pdpt[512], pd[512], pt[512];

/* Returns physical addr, or 0 on fault. */
static uint64_t walk(uint64_t va, const char **why) {
    pte_t e;
    e = pml4[IDX(va, 3)]; if (!(e & P)) { *why="PML4 not present"; return 0; }
    e = pdpt[IDX(va, 2)]; if (!(e & P)) { *why="PDPT not present"; return 0; }
    e = pd  [IDX(va, 1)]; if (!(e & P)) { *why="PD not present";   return 0; }
    e = pt  [IDX(va, 0)]; if (!(e & P)) { *why="PT not present";   return 0; }
    return PFN(e) | (va & 0xFFFull);
}

int main(void) {
    /* Map VA 0 → PA 0x1000, VA 0x1000 → 0x2000, leave 0x2000 unmapped */
    pml4[0] = ((uint64_t)pdpt) | P | RW;  /* not realistic addresses, just non-zero */
    pdpt[0] = ((uint64_t)pd)   | P | RW;
    pd  [0] = ((uint64_t)pt)   | P | RW;
    pt[0] = 0x1000ull | P | RW;
    pt[1] = 0x2000ull | P | RW;

    const char *why = "";
    uint64_t pa = walk(0x0000ull, &why);    printf("VA 0x0000 -> PA 0x%lx\n", pa);
    pa = walk(0x1000ull, &why);              printf("VA 0x1000 -> PA 0x%lx\n", pa);
    pa = walk(0x2000ull, &why);              printf("VA 0x2000 -> %s\n", pa ? "PA" : why);
    return 0;
}
```

## Coding B — Interview DS&A: Binary Search (LC 704)

The canonical `lo + (hi-lo)/2` form, overflow-safe.

```python
def search(nums, target):
    lo, hi = 0, len(nums) - 1
    while lo <= hi:
        mid = lo + (hi - lo) // 2
        if nums[mid] == target: return mid
        if nums[mid] < target:  lo = mid + 1
        else:                   hi = mid - 1
    return -1
```

---

## Bug Anchor

- **Day 19 (QNN/ORT page-fault):** the canonical bug for this layer.
- **Day 8 (MMIO PTE attributes):** PWT/PCD wrong = silent MMIO caching.
- **Day 7 (TLB / huge pages):** the cost lever for VM-heavy workloads.

## Exit Check

- [ ] Drew the 4-level walk and named CR3 / PML4 / PDPT / PD / PT from memory.
- [ ] Listed PTE bits a validation engineer cares about.
- [ ] Distinguished page fault vs TLB miss vs cache miss in 30 seconds.
- [ ] Stated minor vs major fault and their cost order.
- [ ] Stated why MMIO mappings need PWT/PCD set.
- [ ] Owned the QNN/ORT distinction: VM, not cache.

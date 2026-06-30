# Day 19 — TLB, Huge Pages, IOMMU / SMMU

## Concept

The **TLB** caches recent VA→PA translations. **Huge pages** (2 MB / 1 GB) multiply each TLB entry's coverage and are the single biggest perf lever for AI/HPC working sets. The **IOMMU / SMMU / VT-d** is the device-side MMU that translates **IOVA → PA** and sandboxes DMA. **TLB shootdown** uses an IPI to invalidate TLB entries on remote cores.

## Why this day in the flow

Day 18 built the page-table walk and the cost of a full walk. Day 19 explains why the cost is tolerable in practice (TLB), the lever to make it cheaper (huge pages), the cross-core cost when the OS edits a PTE (shootdown), and the device-side analogue (IOMMU). All four are needed for Day 20 (DMA / doorbells / barriers) and Day 23 (NVMe queues).

In the Layer map this completes **Layer 2**.

---

## Deep Study Notes

### 1. TLB structure

A small, fast cache of recent VA→PA translations.

```
CPU issues VA
   │
   ▼
┌──────┐ hit  → PA in 1 cycle
│ TLB  │
└──┬───┘ miss
   ▼
HW page walker → page table → fill TLB → continue
```

Typical x86 layout:

- **L1 iTLB** — 64-128 entries, fully associative, instructions.
- **L1 dTLB** — 64-128 entries, fully associative, data.
- **L2 STLB** — 1024-2048 entries, shared.

A TLB hit is ~1 cycle. A miss costs ~7-30 cycles if the page-walk hits cache, ~400+ cycles if the walk goes to DRAM. In random-access workloads with large working sets, TLB misses dominate.

### 2. Why TLB **coverage** beats TLB **entry count**

The metric that matters is `entries × page_size`.

| Entries | Page size | Coverage |
|---|---|---|
| 1024 | 4 KB | 4 MB |
| 1024 | 2 MB | 2 GB |
| 1024 | 1 GB | 1 TB |

Same hardware, 512× more coverage just from page size. AI workloads with multi-GB tensors miss the TLB constantly with 4 KB pages; switching to huge pages can return 20-30% performance with **zero hardware change**.

### 3. Huge pages — how they skip a level

Normal walk: `PML4 → PDPT → PD → PT → frame`.

- **2 MB page:** PD entry has PS=1 and points directly to a 2 MB-aligned frame; PT level skipped; bits `[20:0]` become offset.
- **1 GB page:** PDPT entry has PS=1; PD and PT levels skipped; bits `[29:0]` become offset.

Saves a memory access per walk and one level of TLB pressure.

Linux exposes them via `MAP_HUGETLB` (`hugetlbfs`) or **transparent huge pages (THP)**. THP is opportunistic — the kernel coalesces 4 KB pages into 2 MB when possible. Easy to enable, sometimes hard to predict for production tail latency.

### 4. TLB shootdown

When the OS modifies a PTE (e.g., `munmap`, page reclaim, COW), every core that may have cached the old translation must invalidate it. The originating core sends an **IPI (Inter-Processor Interrupt)** to all relevant cores; each receiver runs `INVLPG` and acks; the originator waits for all acks before returning.

Cost: hundreds of cycles for the IPI round-trip, plus pipeline disruption on every receiver. Heavy `mmap`/`munmap` workloads can cause **shootdown storms** that pin a core in IPI processing.

`INVLPG va` invalidates one entry; full `mov cr3, cr3` flushes the whole TLB on the local core (rarely used). PCID/ASID lets you avoid the flush across context switches.

### 5. IOMMU / SMMU / VT-d — the device-side MMU

The CPU is not the only memory client. NVMe controllers, GPUs, NPUs, NICs, any PCIe bus master can DMA host memory. Without an IOMMU they would use **physical** addresses, and a buggy or hostile device could read or write any RAM.

```
Without IOMMU:
  Device sends PA → DMA → reads/writes RAM   (no sandboxing)

With IOMMU:
  Device sends IOVA → IOMMU → PA → reads/writes RAM
                       │
                       └── enforces per-device permission and identity
```

- **Intel:** **VT-d**.
- **ARM:** **SMMU** (System MMU, often labelled SMMUv2 / SMMUv3).
- AMD: AMD-Vi.

The IOMMU has its own per-device IO page tables (keyed by the device's `BDF` on PCIe). The OS programs these tables when the driver calls a DMA-mapping API (`dma_map_single`, `iommu_map`, etc.).

Why it matters in validation:

- **Security/safety:** a buggy PCIe device can't DMA anywhere it likes.
- **Virtualisation:** SR-IOV / device passthrough relies on the IOMMU to isolate VM-exposed devices.
- **AI accelerators:** GPUs/NPUs DMA host memory; the IOMMU keeps each accelerator inside its allocated buffers.

### 6. IOMMU faults — the debug entry point

When a device sends an IOVA that has no mapping (or wrong permissions), the IOMMU logs a fault carrying:

- **BDF** — which PCIe device.
- **Faulting IOVA**.
- **Reason** — no entry, write to RO, permission denied, addressing beyond the device's window.

Debug strategy:

1. Read the IOMMU fault register and pull `BDF + IOVA + reason`.
2. Did the OS map this buffer for this device? Compare with the device's IOMMU domain.
3. If yes, the device is sending an unexpected address — firmware/driver bug.
4. If no, the OS forgot to call the DMA-mapping API — driver bug.

Either way you've narrowed the suspect to one of two layers fast.

### 7. The four-table picture (do not confuse them)

| Structure | Caches what? | Granularity | Owner |
|---|---|---|---|
| **TLB** | VA → PA translations | page (4 KB / 2 MB / 1 GB) | hardware (filled by walker) |
| **Cache** | actual data/instructions | 64 B line | hardware |
| **Page table** | translations + permissions | page | OS / kernel |
| **IOMMU page table** | IOVA → PA | IO page | OS / IOMMU driver |

A typical CPU load checks the TLB first (translation), then the cache hierarchy (data). They are independent — you can hit the TLB and miss the cache, or vice versa.

### 8. The doorbell / DMA picture (preview of Day 20)

```
1. Driver fills descriptor in normal memory.
2. Cache maintenance (clean) if non-coherent.
3. Memory barrier so descriptor is visible before the doorbell.
4. MMIO doorbell — uncached, side-effecting.
5. Device DMA — IOVA goes through IOMMU → PA → reads descriptor.
6. Device writes completion.
7. CPU invalidates / reads completion.
```

Every layer in this picture comes from Days 16-19.

---

## Bugs in this Topic

- **TLB shootdown storm under heavy `mmap`/`munmap`:** a core ends up spending cycles in IPI handlers, latency tail rises, throughput drops. Symptom: high `INTERRUPTS` / `IRQ` counts correlated with mapping churn. Fix: batch unmaps, use huge pages to reduce PTE count, lifecycle long-lived mappings.
- **Random-access perf collapse on a model that fits in DRAM but not in TLB coverage:** with 4 KB pages, a 4 GB tensor needs 1 M TLB entries; STLB has thousands. Fix: enable transparent huge pages (THP) or `MAP_HUGETLB` for tensor arenas.
- **IOMMU fault on first DMA after driver init:** OS never called the DMA-mapping API. Fault register shows the device's IOVA with "no entry". Fix: add the missing `dma_map_*` call.
- **IOMMU fault from a device sending a stale IOVA:** OS unmapped the buffer but the device used a queued IOVA from before the unmap. Fix: synchronise device queue draining before unmap; revisit lifetime contract.
- **Cross-link to QNN/ORT page-fault story (Day 34):** the bug looked superficially like "the model just got slower," but ETL traces showed minor-fault spikes — that's a TLB pressure / page-table population symptom, not an IOMMU symptom.

---

## Interview Explanation (60-90 sec)

> "The TLB caches recent virtual-to-physical translations so the hardware doesn't have to do a 4-level page walk on every load. The metric that actually matters is TLB coverage — entries times page size — which is why huge pages are the single biggest performance lever for AI and HPC working sets: a 1024-entry TLB covers 4 MB at 4 KB pages but 2 GB at 2 MB pages, with no hardware change. When the OS edits a PTE, it has to invalidate the TLB on every core that may have cached the old translation, which it does with an IPI — that's TLB shootdown, and heavy `mmap` workloads can spawn shootdown storms. The IOMMU is the device-side MMU; it translates IOVA to physical and sandboxes DMA, so a buggy PCIe device can't read arbitrary memory. When DMA fails, I read the IOMMU fault register first — `BDF + IOVA + reason` narrows the bug to either a missing OS mapping or a device sending an unexpected address."

---

## Architect Drills

*TLBs and IOMMUs are area/latency/policy compromises. Reason about why each landed where it did, and what changes when CXL or accelerators rewrite the constraints. No answer key.*

**D1. TLB coverage = entries × page size. Why hasn't anyone shipped a TLB with 100K entries to win on coverage that way?**
*Constraints to weigh:* CAM access time scales with entries (linear on FA, log on associativity), per-entry area + leakage, fill bandwidth from page walker, real working-set distribution. What's the actual bottleneck — area, latency, or fill rate?

**D2. Huge pages are the single biggest perf lever, yet THP is off by default in many distros. Reason why.**
*Constraints to weigh:* coalescing latency variability, fragmentation under long uptime, tail-latency vs throughput, NUMA-locality cost of moving 2 MB, accidental write-amplification on COW. Construct the workload where THP is a net loss — and the one where you'd force `MAP_HUGETLB`.

**D3. Why does the device need a separate IOMMU when the host has an MMU — why not share the page tables?**
*Constraints to weigh:* devices don't take page faults the way CPUs do (no pre-PRI), per-device isolation (one bad NIC), multiple address spaces (passthrough VMs), legacy 32-bit DMA windows, ATC semantics. What does ATS+PRI buy and what does it cost in latency and complexity?

**D16. SMMU/VT-d/AMD-Vi all solved the same problem differently. Pick two and reason about why each landed where it did.**
*Constraints to weigh:* stream-ID vs requester-ID granularity, two-stage translation for virtualisation, fault-queue semantics, in-flight ATC invalidation. Why did ARM ship a stream table and Intel a domain table — what does the difference reveal about the design priors?

**D17. Imagine ARM's hardware-broadcast TLBI lands on x86. What changes about kernel design — and what new bug class appears?**
*Constraints to weigh:* serialisation cost on the originator, TLB coherence traffic on the interconnect, NUMA latency, kernel `flush_tlb_*` API surface area, lost-IPI debug stories. Why didn't x86 add it — area, ABI, or simply the constraint hadn't shifted?

**D18. The QNN/ORT page-fault story looked like a TLB problem but wasn't. Construct the triage path that would identify it as virtual-memory rather than TLB.**
*Constraints to weigh:* page-fault counter vs `dtlb_load_misses.walk_completed`, kernel-time spike, allocator call-site correlation, repro with hugepages forced on (does it disappear?). What single counter ratio decides the layer before you read any logs?

**D19. Imagine a future SoC with fully-coherent accelerators sharing the CPU page tables (CXL.mem + ATS). Which Day-7 artefacts survive?**
*Constraints to weigh:* DMA-mapping API, bounce buffers, IOVA allocator, IOMMU fault register (still needed for device-side faults), TLB shootdown radius (now includes accelerators). What new shootdown class appears, and which IP becomes the new on-call problem?

---

## Coding A — Systems C: TLB lookup with FIFO + IOMMU translate

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define PG 12u
#define MASK ((1u<<PG)-1u)
#define ENTS 4

typedef struct { uint32_t vpn, pfn; bool valid; } tlb_t;
static tlb_t tlb[ENTS];
static int next_evict = 0;
static unsigned hits, misses;

static uint32_t page_walk(uint32_t vpn) { return (vpn * 0x9E3779B1u) & 0xFFFFFu; }

static uint32_t translate(uint32_t va) {
    uint32_t vpn = va >> PG, off = va & MASK;
    for (int i = 0; i < ENTS; i++)
        if (tlb[i].valid && tlb[i].vpn == vpn) { hits++; return (tlb[i].pfn << PG) | off; }
    misses++;
    uint32_t pfn = page_walk(vpn);
    tlb[next_evict] = (tlb_t){vpn, pfn, true};
    next_evict = (next_evict + 1) % ENTS;
    return (pfn << PG) | off;
}

/* Tiny IOMMU: per-BDF L1->L2 IO page table. Returns false on fault. */
typedef struct { uint32_t pfn; bool present, writable; } io_pte_t;
typedef struct { io_pte_t pt[1024]; bool present; } io_pd_t;
typedef struct { uint16_t bdf; io_pd_t *pd[1024]; } iommu_ctx_t;

static bool iommu_translate(iommu_ctx_t *c, uint32_t iova, bool wr, uint64_t *pa) {
    uint32_t l1 = (iova >> 22) & 0x3FF, l2 = (iova >> 12) & 0x3FF, off = iova & 0xFFF;
    io_pd_t *pd = c->pd[l1];
    if (!pd || !pd->present) { fprintf(stderr,"IOMMU FAULT BDF %04X iova %08X: no L1\n", c->bdf, iova); return false; }
    io_pte_t *e = &pd->pt[l2];
    if (!e->present)        { fprintf(stderr,"IOMMU FAULT BDF %04X iova %08X: no L2\n", c->bdf, iova); return false; }
    if (wr && !e->writable) { fprintf(stderr,"IOMMU FAULT BDF %04X iova %08X: write to RO\n", c->bdf, iova); return false; }
    *pa = ((uint64_t)e->pfn << 12) | off;
    return true;
}

int main(void) {
    uint32_t accs[] = { 0x1000, 0x2040, 0x1080, 0x3000, 0x4000, 0x5000, 0x1000 };
    for (size_t i = 0; i < sizeof accs / sizeof *accs; i++)
        printf("VA %08X -> PA %08X\n", accs[i], translate(accs[i]));
    printf("hits=%u misses=%u\n", hits, misses);
    return 0;
}
```

---

## System Thinking — Why translation needs its *own* cache, and why devices need it too

**Frame:** virtual memory (Day 18) gave us the page table. But every load now needs to walk it — 4 levels deep on x86-64, 100+ cycles to DRAM if cold. That cost would erase the entire VM benefit. The TLB exists for the same reason L1 cache exists: the *translation* itself has locality. Huge pages and IOMMUs are the natural extensions: huge pages stretch TLB reach, IOMMU gives devices the same protection model as processes.

### Q: Why is the TLB so critical that it's a separate structure from L1?

Because it's on the *translation* path, not the data path. Every load needs `VA→PA` *before* it can even index L1 (PIPT) or for the tag compare (VIPT). A TLB miss stalls the load until the page walk completes — 4 memory accesses worst case. With a TLB hit rate of 99%, the AMAT for translation alone could exceed the AMAT for data. So the TLB is small (~64 entries L1, ~1500 L2), fully or highly associative, and parallel to L1 indexing. **A TLB miss is often the most expensive event a hot loop hits.**

### Q: Huge pages — pure win or tradeoff?

**Tradeoff.** Huge pages (2 MB, 1 GB on x86; 16 KB, 64 KB, 2 MB on ARM) increase TLB **reach** — coverage = entries × page size. With 64 entries and 4 KB pages, reach = 256 KB. Same TLB with 2 MB pages, reach = 128 MB. *That's the whole point: applications with multi-MB working sets stop missing in the TLB.*

But you pay:
- **Internal fragmentation:** allocate a 2 MB page for a 5 KB allocation → 1.99 MB wasted.
- **Allocator pressure:** the kernel must find 512 contiguous 4 KB frames to back a 2 MB page. Under fragmentation pressure (long-running system) this fails or needs compaction.
- **Coarser permissions / TLB shootdowns:** an mprotect on 1 byte of a huge page changes permission for 2 MB.

Answer: huge pages for **databases, JVM heaps, ML model weights, hugetlbfs DMA buffers** — anything with large, hot, contiguous working sets. **Not** for general-purpose userspace.

### Q: Why IOMMU/SMMU? Devices already work without it on small embedded systems.

Because as soon as a device can DMA, it can DMA *anywhere* — including kernel memory or another VM's memory. Three problems IOMMU solves:
1. **Isolation.** Each device gets its own translation context (Stream ID on ARM SMMU, BDF/RID on x86 IOMMU). A bad/malicious device firmware cannot reach memory not mapped to it. *This is what enables PCIe device passthrough to VMs.*
2. **Scatter-gather.** A buffer that's contiguous in VA can be physically scattered (it's heap memory). The IOMMU translates a contiguous device VA (IOVA) to scattered physical pages → drivers stop needing physically-contiguous DMA buffers (which were a major Linux pain pre-IOMMU).
3. **32-bit device on a 64-bit system.** A legacy device with 32-bit DMA reach can be given an IOVA in its 4 GB window that maps to physical RAM above 4 GB.

**Same idea as the MMU, applied to devices.** The validation surface (Stream ID assignment, ATC invalidation, SMMU fault queue) is large and underexplored — bugs here are interview gold (Day 34 QNN/HTP angle).

### One-liner

*"The TLB exists because translation itself has locality and a cold page walk would erase the VM benefit. Huge pages are a tradeoff — bigger TLB reach for fragmentation and coarser permissions, worth it for large hot working sets. IOMMU is the MMU re-applied to devices: per-device isolation, scatter-gather, and 32-bit-device support. Same pattern at a different layer."*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **TLB** — *what:* on-die cache of PTEs / *why:* a walk is 4–5 memory accesses / *how:* tagged by ASID/PCID, split iTLB/dTLB, multi-level / *connects:* TLB shootdown IPI cost, miss penalty
- **hugepages** — *what:* 2 MB / 1 GB pages / *why:* fewer TLB entries cover more RAM / *how:* reserved at boot or THP / *connects:* DB / NIC ring / GPU buffer wins, fragmentation cost
- **IOMMU / SMMU** — *what:* MMU for devices / *why:* DMA isolation + 32-bit-device support + virt passthrough / *how:* per-device page tables / *connects:* VFIO, SR-IOV, why bad IOMMU = security hole
- **ASID / PCID** — *what:* per-process tag in TLB / *why:* avoid full flush on context switch / *how:* tag entries with PCID / *connects:* Meltdown KPTI cost without PCID
- **page walk** — *what:* HW walks tables on TLB miss / *why:* avoid trap to OS for every miss / *how:* dedicated page-walk caches + L2 TLB / *connects:* nested paging = 2D walk for VMs

---

## Bug Anchor

- **Day 20 (DMA/doorbells):** the doorbell example is where TLB + IOMMU + cache + barrier all converge.
- **Day 34 (QNN/ORT):** minor-fault spike on context-buffer touch is the symptom that links VM and TLB pressure.
- **Day 23 (NVMe):** doorbell + descriptor + IOMMU mapping for SQ/CQ.

## Exit Check

- [ ] Stated TLB coverage formula and the 4 KB → 2 MB lever.
- [ ] Walked a TLB shootdown end-to-end with IPI cost.
- [ ] Distinguished IOMMU vs MMU vs cache vs page table.
- [ ] Walked an IOMMU fault to a one-of-two-layer suspect.
- [ ] Named SMMU / VT-d / AMD-Vi correctly.
- [ ] Stated why a 2 MB page skips the PT level.

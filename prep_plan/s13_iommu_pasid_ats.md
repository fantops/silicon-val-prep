# S13 - IOMMU, PASID, and ATS

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [D18 virtual memory](archive/day18_virtual_memory_page_faults.md) (page walk and faults), [D19 TLB/IOMMU](archive/day19_tlb_huge_pages_iommu_smmu.md), [DSA fundamentals §6](../dsa-val/fundamentals/dsa_ip_fundamentals.md).
- **Must answer:**
  - CPU page walk + TLB. The IOMMU does the same for devices (requester ID → context → page table). A **DMA remapping fault** is the device's segfault.
  - **PASID** lets a device use a process's virtual addresses (SVA).
  - **ATS** caches translations on the device; **PRI** asks the OS to fault pages in. What happens if an invalidation is slow or missed?
- **Work link:** the last translation or page-fault related failure you saw at work. Which arrow on the translation path broke?
- **Story re-tell (story 1):** the **QNN page-fault / IOMMU-restore race**. On wake, the first DMA went out before the translation tables were restored, causing a fault with no recovery path. Tell it in ≤ 90 s using IOVA, page table, fault, and restore-before-first-DMA.
- **Handoff →** Translation is solved. How does the accelerator get configured and enabled?

## Study notes

### Concept - [day18_virtual_memory_page_faults.md](archive/day18_virtual_memory_page_faults.md)

Each process gets its own **virtual address space**; the **MMU** translates virtual to physical at page granularity (typically 4 KB) using a multi-level **page table** anchored by `CR3` (x86) / `TTBRn_EL1` (ARM). Each PTE carries permissions (P / R/W / U/S / NX) and cache attributes (PWT / PCD on x86, MAIR on ARM). A **page fault** is the OS-visible result of a translation that fails — minor (resolvable from RAM) or major (needs I/O).

### Deep Study Notes - [day18_virtual_memory_page_faults.md](archive/day18_virtual_memory_page_faults.md)

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

Up to **4 memory accesses**, each ~100 ns if uncached. ~400 ns just to translate a single address. That is unacceptable, so the **TLB** caches recent translations (Day 19).

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

For devices (Day 19-20):

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

### Bugs in this Topic - [day18_virtual_memory_page_faults.md](archive/day18_virtual_memory_page_faults.md)

- **QNN/ORT page-fault + virtual-memory regression (Day 34 anchor):** after a context-loading change, ORT's QNN execution provider began touching context buffers in a different lifetime pattern. Many host virtual pages were touched per inference, spiking minor faults and bumping virtual-memory footprint. **Mechanism:** demand-zero faults at 4 KB granularity, not cache misses. **Surface:** ETL/WPA showed `MajorFault`/`MinorFault` events correlating with the new code path. **Fix:** persist context buffers across iterations rather than rebuild per inference; pre-touch and lock pages where appropriate.
- **MMIO accidentally cached:** PTE PWT/PCD wrong for the device range; register accesses get cached; status bits don't update from the device side. Symptom: status register reads stale, often "stuck" at the value it had at first read.
- **Unexpected SIGSEGV in driver:** kernel handed user-space a buffer it shouldn't have access to; PTE permission bits wrong (U/S, R/W).
- **`fork()` storm causes minor-fault thrash:** every COW page in the parent becomes a private copy on first child write; under heavy concurrency this dirties cache, swamps the page allocator, and hurts tail latency.

---

### Interview Explanation (60-90 sec) - [day18_virtual_memory_page_faults.md](archive/day18_virtual_memory_page_faults.md)

> "Virtual memory gives every process its own address space; the MMU translates virtual to physical at page granularity using a 4-level page table on x86-64, anchored by `CR3`. Each PTE carries the present bit, permission bits, accessed/dirty bits, and cache attributes — getting the cache attributes wrong on an MMIO range is a classic bring-up bug because register reads get silently cached. A page fault is the OS-visible result: minor faults are resolvable in microseconds with no I/O, major faults take milliseconds because they need a disk read. The QNN/ORT regression I worked on was a virtual-memory bug, not a cache bug — context-buffer lifetime changed and minor faults spiked at 4 KB granularity per inference. The fix was to persist context buffers across iterations. The vocabulary I always keep straight: page fault = translation event, TLB miss = HW page walk, cache miss = locality event — three different layers."

---

### Architect Drills - [day18_virtual_memory_page_faults.md](archive/day18_virtual_memory_page_faults.md)

*Virtual memory is a contract between OS, MMU, and DRAM. Each design choice trades latency, area, and software policy. Derive — don't recall. No answer key.*

**D1. Why a 4-level page table on x86-64 — why not a single flat translation array?**
*Constraints to weigh:* 256 TB VA space → flat would need $2^{36}$ PTEs = 256 GB per process; sparse address space is the norm; walk cost vs page-table memory cost; tradeoff between depth (walk latency) and breadth (radix). 5-level for 57-bit VA — what forced the addition, and what's the next inflection?

**D2. Why are minor page faults charged to the OS, not the hardware?**
*Constraints to weigh:* page-table state lives in OS-owned memory, allocator policies (COW, demand-zero, swap-in) are policy not mechanism, hardware would need to know file-backed vs anonymous, security boundary. What would a "hardware-resolves-minor-faults" SoC look like, and what would it cost in flexibility?

**D3. Why is the page offset never translated — what design freedom does that lock in?**
*Constraints to weigh:* pages are physically contiguous within themselves, offset bits skip the TLB entirely, page-size choice trades TLB coverage vs internal fragmentation. If pages were *not* required to be physically contiguous, what does the MMU need to look like instead?

**D16. MMIO PTEs need PWT/PCD set. Why doesn't the CPU just detect "this address is in a device range" and bypass the cache automatically?**
*Constraints to weigh:* range registers (MTRRs) actually do this on x86 but are coarse and limited; per-page attribute is finer; who owns the device-range knowledge (BIOS/firmware, OS, neither). What does the hybrid (MTRR + PAT) buy — and what bug class would PAT-only invite?

**D17. TLB shootdown is an IPI storm. Construct the workload where it dominates wall time, and reason about how to design it away.**
*Constraints to weigh:* `munmap` rate, cores-per-process, per-PTE invalidation cost, ASID/PCID amortisation, range-flush hardware (`TLBI VMALLE1` vs `TLBI VAALE1`). ARM has hardware-broadcast TLBI; x86 doesn't. Why the divergence — area vs ABI?

**D18. Imagine DRAM becomes byte-addressable persistent (NVDIMM-style throughout). Does the page-fault model survive?**
*Constraints to weigh:* zero-copy memory-mapped IO, CoW semantics on persistent state, what "minor fault" means when the page is always present, durability barriers, file-system-as-memory-allocator. Where do faults still earn their place, and where do they collapse?

**D19. Page faults vs TLB misses vs cache misses are three different layers. From a perf trace alone — without naming any counter first — derive a heuristic to tell them apart.**
*Constraints to weigh:* user/kernel time split, per-event latency distributions (ns vs µs vs ms), correlation with `mmap`/`brk` syscalls, CPI breakdown shape. What does the latency histogram tell you before you read any specific counter?

---

### Exit Check - [day18_virtual_memory_page_faults.md](archive/day18_virtual_memory_page_faults.md)

- [ ] Drew the 4-level walk and named CR3 / PML4 / PDPT / PD / PT from memory.
- [ ] Listed PTE bits a validation engineer cares about.
- [ ] Distinguished page fault vs TLB miss vs cache miss in 30 seconds.
- [ ] Stated minor vs major fault and their cost order.
- [ ] Stated why MMIO mappings need PWT/PCD set.
- [ ] Owned the QNN/ORT distinction: VM, not cache.

### Concept - [day19_tlb_huge_pages_iommu_smmu.md](archive/day19_tlb_huge_pages_iommu_smmu.md)

The **TLB** caches recent VA→PA translations. **Huge pages** (2 MB / 1 GB) multiply each TLB entry's coverage and are the single biggest perf lever for AI/HPC working sets. The **IOMMU / SMMU / VT-d** is the device-side MMU that translates **IOVA → PA** and sandboxes DMA. **TLB shootdown** uses an IPI to invalidate TLB entries on remote cores.

### Deep Study Notes - [day19_tlb_huge_pages_iommu_smmu.md](archive/day19_tlb_huge_pages_iommu_smmu.md)

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

### Bugs in this Topic - [day19_tlb_huge_pages_iommu_smmu.md](archive/day19_tlb_huge_pages_iommu_smmu.md)

- **TLB shootdown storm under heavy `mmap`/`munmap`:** a core ends up spending cycles in IPI handlers, latency tail rises, throughput drops. Symptom: high `INTERRUPTS` / `IRQ` counts correlated with mapping churn. Fix: batch unmaps, use huge pages to reduce PTE count, lifecycle long-lived mappings.
- **Random-access perf collapse on a model that fits in DRAM but not in TLB coverage:** with 4 KB pages, a 4 GB tensor needs 1 M TLB entries; STLB has thousands. Fix: enable transparent huge pages (THP) or `MAP_HUGETLB` for tensor arenas.
- **IOMMU fault on first DMA after driver init:** OS never called the DMA-mapping API. Fault register shows the device's IOVA with "no entry". Fix: add the missing `dma_map_*` call.
- **IOMMU fault from a device sending a stale IOVA:** OS unmapped the buffer but the device used a queued IOVA from before the unmap. Fix: synchronise device queue draining before unmap; revisit lifetime contract.
- **Cross-link to QNN/ORT page-fault story (Day 34):** the bug looked superficially like "the model just got slower," but ETL traces showed minor-fault spikes — that's a TLB pressure / page-table population symptom, not an IOMMU symptom.

---

### Interview Explanation (60-90 sec) - [day19_tlb_huge_pages_iommu_smmu.md](archive/day19_tlb_huge_pages_iommu_smmu.md)

> "The TLB caches recent virtual-to-physical translations so the hardware doesn't have to do a 4-level page walk on every load. The metric that actually matters is TLB coverage — entries times page size — which is why huge pages are the single biggest performance lever for AI and HPC working sets: a 1024-entry TLB covers 4 MB at 4 KB pages but 2 GB at 2 MB pages, with no hardware change. When the OS edits a PTE, it has to invalidate the TLB on every core that may have cached the old translation, which it does with an IPI — that's TLB shootdown, and heavy `mmap` workloads can spawn shootdown storms. The IOMMU is the device-side MMU; it translates IOVA to physical and sandboxes DMA, so a buggy PCIe device can't read arbitrary memory. When DMA fails, I read the IOMMU fault register first — `BDF + IOVA + reason` narrows the bug to either a missing OS mapping or a device sending an unexpected address."

---

### Architect Drills - [day19_tlb_huge_pages_iommu_smmu.md](archive/day19_tlb_huge_pages_iommu_smmu.md)

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

### Exit Check - [day19_tlb_huge_pages_iommu_smmu.md](archive/day19_tlb_huge_pages_iommu_smmu.md)

- [ ] Stated TLB coverage formula and the 4 KB → 2 MB lever.
- [ ] Walked a TLB shootdown end-to-end with IPI cost.
- [ ] Distinguished IOMMU vs MMU vs cache vs page table.
- [ ] Walked an IOMMU fault to a one-of-two-layer suspect.
- [ ] Named SMMU / VT-d / AMD-Vi correctly.
- [ ] Stated why a 2 MB page skips the PT level.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S12](s12_dma_doorbells_interrupts.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S14](s14_accelerator_bringup.md)

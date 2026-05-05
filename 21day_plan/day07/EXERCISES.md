# Day 07 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day07_tlb_huge_pages_iommu_smmu.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: TLB lookup with FIFO + IOMMU translate

**Problem statement:** Build two collaborating models.

(a) A 4-entry FIFO TLB that caches `vpn → pfn` mappings. On miss, call a fake `page_walk(vpn)` (any deterministic hash will do), install at `next_evict`, advance the index. Track `hits` and `misses`.

(b) A two-level IOMMU per BDF. `iommu_translate(ctx, iova, write, *pa)` walks an L1 (1024 entries) → L2 (1024 entries) page table. Faults: missing L1, missing L2, write-to-RO. Print fault as `IOMMU FAULT BDF %04X iova %08X: <reason>`.

**You must implement:**
```c
typedef struct { uint32_t vpn, pfn; bool valid; } tlb_t;
uint32_t translate(uint32_t va);

typedef struct { uint32_t pfn; bool present, writable; } io_pte_t;
typedef struct { io_pte_t pt[1024]; bool present; } io_pd_t;
typedef struct { uint16_t bdf; io_pd_t *pd[1024]; } iommu_ctx_t;
bool iommu_translate(iommu_ctx_t *c, uint32_t iova, bool wr, uint64_t *pa);
```

**Driver / tests to write:**
- Run a 7-access pattern through `translate` (mix of repeats and new VPNs); print `hits`, `misses`.
- Construct an IOMMU ctx; demonstrate (i) L1 missing, (ii) L2 missing, (iii) RO write fault.

**Be ready to explain in interview:**
- TLB coverage = entries × page size; the 4KB→2MB lever.
- TLB shootdown via IPI, ARM hardware TLBI broadcast vs x86.
- Why does the device need a separate IOMMU when the host has an MMU?
- ATS+PRI — what does it buy and what does it cost?

## E2 — DS&A: First Bad Version (LC 278)

**Problem statement:** Given an API `bool is_bad(int v)` that returns true once `v >= first_bad`, find the smallest `v` in `[1, n]` for which `is_bad(v)` returns true. Predicate binary search.
**Signature:** `int first_bad_version(int n);`
**Constraints / edge cases:** `n = 1`, first version is bad, last version is bad, none bad (defined behavior: returns `n`). Avoid integer overflow on `(lo + hi) / 2`.
**Talking points:** O(log n) calls to `is_bad`; same monotonic-predicate shape as scanning a sorted PTE array for first-set.

## E3 — Architect Drills (no code, write your answers)

**D1.** Why hasn't anyone shipped a TLB with 100K entries?

**D2.** Huge pages are the biggest perf lever, yet THP is off by default in many distros. Reason why.

**D3.** Why does the device need a separate IOMMU when the host has an MMU?

**D4.** SMMU/VT-d/AMD-Vi solved the same problem differently. Pick two; explain why each landed where it did.

**D5.** Imagine ARM's hardware-broadcast TLBI lands on x86. What changes about kernel design — what new bug class appears?

**D6.** Construct the triage path that identifies the QNN/ORT regression as VM rather than TLB. What single counter ratio decides?

**D7.** With CXL.mem + ATS, accelerators share CPU page tables. Which Day-7 artefacts survive?

## E4 — Bug Anchor

**Symptoms:** First DMA after driver init triggers an IOMMU fault. Fault register shows BDF + IOVA + "no entry". Walk the debug tree to confirm the missing `dma_map_*` call.

Second scenario: device sends stale IOVA after `dma_unmap_*`. Why? How to fix the queue-drain ordering?

## Reference solution

Once you've attempted, see `../day07_tlb_huge_pages_iommu_smmu.md` sections "Coding A" and "Coding B" for full solutions and discussion.

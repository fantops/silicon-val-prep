# Day 06 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day06_virtual_memory_page_faults.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: 4-level page walk

**Problem statement:** Implement an x86-64-style 4-level page walk in C, using four 512-entry tables (`pml4`, `pdpt`, `pd`, `pt`). Each PTE carries Present (`P`) and Read/Write (`RW`) flags plus a frame-aligned PFN. The walk function takes a 64-bit VA, walks the 4 levels by 9-bit indices, and returns either a physical address or 0 with a reason string on fault.

**You must implement:**
```c
#define PG_SHIFT 12
#define IDX(va, lvl) (((va) >> (PG_SHIFT + 9 * (lvl))) & 0x1FFu)
typedef uint64_t pte_t;
#define P  (1ull << 0)
#define RW (1ull << 1)
#define PFN(p) ((p) & ~0xFFFull)

uint64_t walk(uint64_t va, const char **why);
```

**Driver / tests to write:**
- Map `VA 0x0000 → PA 0x1000` and `VA 0x1000 → PA 0x2000` by populating the four tables.
- Walk `0x0000`, `0x1000` → expect successful PA returns.
- Walk `0x2000` → expect fault with reason `"PT not present"`.

**Be ready to explain in interview:**
- Why 4 levels and not a flat table?
- Why is the page offset never translated?
- What's the difference between PFN and physical address?
- Page fault vs TLB miss vs cache miss in 30 seconds.

## E2 — DS&A: Binary Search (LC 704)

**Problem statement:** Return the index of `target` in a sorted array `nums`, or -1 if absent.
**Signature:** `int binary_search(const int *nums, int n, int target);`
**Constraints / edge cases:** empty array, single element, target smaller than min / larger than max, duplicates (return any matching index). Use overflow-safe `mid = lo + (hi - lo) / 2`.
**Talking points:** O(log n); same predicate-driven search shape recurs in Day 7 First-Bad-Version, Day 11 NVMe head-tail bisection.

## E3 — Architect Drills (no code, write your answers)

**D1.** Why a 4-level page table on x86-64 — why not a single flat translation array? What forced the 5-level addition?

**D2.** Why are minor page faults charged to the OS, not the hardware? What would a hardware-resolves-minor-faults SoC look like?

**D3.** Why is the page offset never translated? What design freedom does that lock in?

**D4.** MMIO PTEs need PWT/PCD set. Why doesn't the CPU detect device ranges automatically?

**D5.** TLB shootdown is an IPI storm. Construct the workload where it dominates wall time. ARM has hardware-broadcast TLBI; x86 doesn't — why?

**D6.** Imagine DRAM becomes byte-addressable persistent. Does the page-fault model survive?

**D7.** Page fault vs TLB miss vs cache miss are 3 different layers. From a perf trace alone, derive a heuristic to tell them apart.

## E4 — Bug Anchor

**Symptoms:** After a refactor, an ML inference path runs slower. ETL/WPA shows `MinorFault` spikes correlated with the new code. Walk the debug tree: this is virtual memory (demand-zero faults at 4KB), not cache-miss. What single counter ratio decides the layer? What's the fix (persist context buffers, pre-touch, mlock)?

Cross-link to Day 8: MMIO accidentally cached when PTE PWT/PCD is wrong — symptom is "stuck" status reads.

## Reference solution

Once you've attempted, see `../day06_virtual_memory_page_faults.md` sections "Coding A" and "Coding B" for full solutions and discussion.

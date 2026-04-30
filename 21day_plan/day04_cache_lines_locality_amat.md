# Day 04 — Cache Lines, Locality, Write Policies, AMAT

## Concept

Caches exist because DRAM is hundreds of cycles away. Data moves in fixed-size **cache lines** (64 B on x86/ARM); an address splits into **tag / set / offset**; **associativity** controls how many lines can co-exist in one set; **write-back + write-allocate** is the dominant policy; AMAT quantifies all of it. Today is the foundation of every later "device read stale data" / "false sharing" / "DMA visibility" bug.

## Why this day in the flow

After mastering addresses (Day 1), layout (Day 2), and registers (Day 3), this is the first day where the *path between CPU and memory* matters. Every later concept builds on it:

- Day 5 — coherence + false sharing (cache lines crossing cores).
- Day 8 — DMA / doorbells (cache-line clean/flush before device DMA).
- Day 14 — ADR (write-back makes the WPQ a thing in the first place).
- Day 19 — NPU memory bugs (page faults vs cache misses are not the same thing — today is where you draw the line).

This is **Layer 1** of the system flow.

---

## Deep Study Notes

### 1. Why caches exist

```
CPU op:           sub-ns to a few ns
L1 hit:           ~4 cycles  (~1 ns)
L2 hit:           ~12 cycles (~3 ns)
LLC hit:          ~30-40 cycles (~10 ns)
DRAM access:      ~100 ns / hundreds of cycles
```

Without caches, every load would stall the pipeline for 200-400 cycles. Caches keep recently used data near the core, exploiting **temporal locality** (same address again soon) and **spatial locality** (nearby addresses soon).

### 2. Hierarchy

```
  Core
    ├── L1 (32-64 KB, ~4 cyc) — split I/D, per core
    ├── L2 (256 KB-1 MB, ~12 cyc) — unified, per core
    └── LLC / L3 (8-64 MB, ~30-40 cyc) — shared across cores
                  └── DRAM (GB, ~200+ cyc)
```

- L1 split into L1-I and L1-D — different access patterns.
- L2 unified per core.
- LLC shared; on server SKUs up to 64 MB+.
- Modern x86 server adds **CHA / mesh** between LLC slices and home agents.

### 2b. SRAM vs DRAM (why caches are SRAM, why main memory is DRAM)

The whole hierarchy above is one cell-tech tradeoff repeated:

| Property | **SRAM** (caches, register file, TCM, VTCM) | **DRAM** (DDR4/5, LPDDR5, HBM) |
|---|---|---|
| Bit cell | 6 transistors (cross-coupled inverters + 2 access) | 1 transistor + 1 capacitor |
| Density | low (big cell) | high (~10× denser per mm²) |
| Speed | ~1 ns access | ~15 ns row activate, ~50-100 ns full random access |
| Refresh? | **No** — stable as long as Vdd is on | **Yes** — every cell leaks; refresh every ~64 ms (per row, every ~7.8 µs) |
| Volatile? | yes (loses state on power-off) | yes |
| Power | leaky when on, **0 when power-gated** (C6 saves L1/L2 this way) | needs constant refresh → always burns *some* power even idle (DRAM self-refresh in S3) |
| $/bit | expensive | cheap |
| Where | on-die: L1, L2, L3, register file, TLB, BTB; on-package SRAM (eDRAM rare today) | DIMMs / on-package LPDDR / HBM stacks |

**Why caches are SRAM:** speed + no refresh. A read is just a gated voltage compare — no row activate, no precharge. The cost is area: an L1D that's 64 KB of SRAM is ~6 × 64 KB × (transistors) on the die.

**Why main memory is DRAM:** density + cost. You can't put 32 GB of SRAM on a die budget. The price is the row-activate / precharge / refresh dance and the ~100 ns latency you spend the rest of this day learning to hide.

**Validation-relevant gotchas:**
- **DRAM refresh stalls** — a refresh on the bank you're about to read costs you ~100 ns of extra latency. Looks like a perf jitter bug; root cause is just refresh collision.
- **DRAM self-refresh entry/exit** — entering self-refresh saves power but exit latency is microseconds; PkgC6 / S0ix entry has to coordinate this with the memory controller (Day 13).
- **SRAM retention voltage** — in C6 the core's V-rail drops to a *retention* voltage that keeps SRAM state (arch regs, L2 if retained) but can't switch. Drop too low → silent SRAM bit flip on wake. Classic pre-Si validation corner.
- **eDRAM / on-die DRAM** existed (Intel Crystalwell L4) but is rare now — HBM stacks took over for high-BW use cases.
- **3D-stacked HBM** — still DRAM cells; gets bandwidth via wide bus + TSVs, not faster cells.

**One-liner for the interview:** *"SRAM is fast and doesn't refresh, so it's where caches live; DRAM is dense and cheap so it's main memory; the whole memory hierarchy is just hiding the ~100 ns DRAM cost behind layers of SRAM."*

### 3. Cache lines (64 bytes is the answer)

Data moves between memory and cache in fixed-size **cache lines**, **64 bytes** on x86 and ARM. A single-byte load brings in the whole 64 B line.

```c
uint8_t x = buffer[8];  /* hardware fetches buffer[0..63] */
```

Why 64 B? Trade-off between spatial locality (bigger is better) and false sharing / wasted bandwidth (bigger is worse). 64 B has been the sweet spot for ~20 years. Some HPC chips use 128 B; smaller would raise tag overhead.

Loop friendliness:

```c
for (int i = 0; i < n; i++)        sum += a[i];        /* friendly  */
for (int i = 0; i < n; i += 1024)  sum += a[i];        /* unfriendly */
```

The first reuses the line; the second pays a miss per iteration and wastes 60 of every 64 bytes fetched.

### 4. Address decode: tag / set / offset

For a 32 KB, 4-way set-associative cache with 64 B lines:

- Lines = 32 KB / 64 = 512.
- Sets = 512 / 4 ways = 128.
- Offset bits = log₂(64) = 6.
- Set index bits = log₂(128) = 7.
- Tag bits = remaining (52 on x86-64 virtual; 40-46 physical).

```
Address: [      Tag       |  Set Index  |  Byte Offset  ]
```

Analogy: set = which shelf, tag = which box label, offset = which byte in the box.

### 5. Associativity

| Type | Idea | Pros | Cons |
|---|---|---|---|
| Direct-mapped | 1 slot per set | simple, fast | conflict misses |
| N-way set-associative | N slots per set | fewer conflicts | more comparators |
| Fully associative | any line anywhere | no conflicts | only feasible for tiny caches (TLB) |

Typical: L1 4-8 way, L2 8-16 way, L3 16-32 way.

### 6. Write policies

**Write-back (dominant for L1/L2/L3):**

- Write hits cache; line marked **dirty**.
- Memory written later on eviction or flush.
- Saves bandwidth; adds the "WPQ must drain before power loss" problem (Day 14 ADR).

**Write-through:**

- Write hits cache and memory simultaneously.
- Memory always coherent; no dirty lines to flush.
- Burns memory bandwidth; only used for small write-buffer caches, scratchpads, some MCUs.

**Write-allocate** (paired with write-back): on a write miss, fetch the line first, then modify.
**No-write-allocate** (paired with write-through): on a write miss, write to memory; do not pull into cache.

### 7. Replacement policies

- **LRU** — evict the least recently used. Common, but exact LRU is expensive at high associativity.
- **Pseudo-LRU** — tree-based approximation. Used in high-associativity L3.
- **Random** — surprisingly competitive on many workloads.

### 8. The four C's of misses

| Type | Cause | Fix lever |
|---|---|---|
| Compulsory (cold) | first access ever | prefetch |
| Capacity | working set > cache | bigger cache or better locality |
| Conflict | two addresses map to same set | higher associativity, address layout |
| **Coherence** | invalidations from peer cores | layout, padding, true-vs-false sharing analysis |

Coherence misses are the bridge to Day 5.

### 9. AMAT (the one formula to remember)

$$\text{AMAT} = \text{HitTime}_{L1} + \text{MissRate}_{L1} \times (\text{HitTime}_{L2} + \text{MissRate}_{L2} \times \text{DRAM})$$

Worked example:

$$1\,\text{ns} + 0.05 \times (3\,\text{ns} + 0.20 \times 100\,\text{ns}) = 2.15\,\text{ns}$$

Even though DRAM is huge, the L1 hit dominates because the probability of reaching DRAM is tiny ($0.05 \times 0.20 = 1\%$). But if L1 miss rate doubles to 10%, AMAT becomes 2.4 ns — the miss path is the lever.

### 10. Cache lines and DMA — the system bridge

The CPU writes a descriptor, then rings an MMIO doorbell. If the descriptor sits in a dirty L1 line that has not been cleaned to DRAM (or, on weak memory, is reordered after the doorbell), the device's DMA reads stale memory:

```
CPU fills descriptor  → may sit in CPU cache line
clean / flush descriptor cache line if non-coherent
memory barrier
MMIO doorbell
device DMA reads descriptor
```

This is why DMA buffers are aligned to a cache line and sized in multiples of one. If a descriptor shares a line with unrelated data, a clean/invalidate on that line affects the neighbour.

You will revisit this on Day 8.

### 11. What this is **not** — the QNN/ORT distinction

The QNN/ORT page-fault story (Day 19) is **not** a cache-line bug:

- **Cache miss** = locality event, 64 B granularity, hardware-managed.
- **Page fault** = translation/allocation event, 4 KB granularity, OS-managed.

Saying "the page-fault regression was a cache miss problem" is a vocabulary mistake interviewers catch. The QNN/ORT bug was about virtual-memory allocation lifetime under a new context-loading path, not 64-byte locality.

Likewise, **VTCM** in Hexagon HTP (Day 18) is **software-managed** local SRAM, not a CPU cache. Same word "memory," different layer.

| Layer | Granularity | Managed by | Concern |
|---|---|---|---|
| CPU cache | 64 B line | hardware | locality, coherence |
| Virtual memory | 4 KB page | OS / MMU | translation, faults |
| IOMMU/SMMU | IO page | OS / device map | safe DMA |
| VTCM | tile / buffer | NPU compiler/runtime | NPU local-store fit |

---

## Bugs in this Topic

- **Device reads stale descriptor:** descriptor still in CPU's dirty L1; missing clean/flush before doorbell on a non-coherent DMA path. Picture: CPU's M state, device DMA reads DRAM. Fix: cache-line-aligned descriptor + clean + barrier + doorbell.
- **Strided loop bandwidth collapse:** stride > line size wastes 60/64 bytes per fetch and pulls the whole working set through DRAM. Symptom: the same loop is 5-10× slower with `i += 1024` than `i += 1`.
- **Conflict-miss thrash on power-of-2 stride:** two hot arrays separated by a multiple of (set count × line size) constantly evict each other in a low-associativity cache. Fix: pad arrays by one line, or use a higher-associativity SKU.
- **W1C-style mistake on cache maintenance:** invalidating a dirty line without a clean first **discards** the writes silently. Always clean-then-invalidate (or use a single "clean+invalidate" op) on dirty data.

---

## Interview Explanation (60-90 sec)

> "Caches exist because DRAM is hundreds of cycles away. Data moves in 64-byte cache lines, so reading one byte fetches the neighbours — that's the spatial-locality lever. An address splits into tag, set, and offset; associativity controls how many lines can co-exist in one set. Almost all CPU L1/L2/L3 are write-back with write-allocate, which is why dirty lines exist and why ADR has to drain the IMC write-pending queue before rails collapse. AMAT — `hit + miss_rate × penalty` chained per level — is the one formula I use to predict the impact of a cache change. The system-validation use of cache lines is DMA: a descriptor in a dirty cache line that hasn't been cleaned will be read stale by the device, so I always cache-line-align DMA buffers and put a clean+barrier between the descriptor write and the doorbell. The vocabulary trap I avoid is calling page-fault regressions cache-miss problems — they're at different layers."

---

## Practice Q&A

**Q1. Why is the cache line 64 bytes?**
Trade-off between spatial locality and wasted bandwidth / false sharing. 64 B has been the sweet spot for x86/ARM for two decades.

**Q2. Decode `0xCAFE_0040` for a 32 KB, 8-way, 64 B-line cache.**
Lines=512, sets=64. Offset=6 bits → `0x0`. Set index=6 bits → `(0xCAFE0040 >> 6) & 0x3F = 0x01`. Tag = upper bits.

**Q3. AMAT for L1 1 ns, miss 5%, L2 4 ns, miss 10%, DRAM 100 ns?**
$1 + 0.05(4 + 0.10 \times 100) = 1 + 0.05 \times 14 = 1.7\,\text{ns}$.

**Q4. Why is fully associative used for TLBs but not L2/L3?**
TLBs are tiny (64-1024 entries); a parallel comparator across all entries is feasible. L2/L3 have thousands to millions of lines — fully associative is infeasible.

**Q5. Write-back vs write-through — when do you actually pick write-through?**
Write-through for caches that must stay coherent with non-coherent agents (some DMA paths, debug observation buffers) or where writes are rare and read latency matters most. Most CPU caches are write-back.

**Q6. Classify misses for `A, B, A, C, A` on a 2-line direct-mapped cache where B and C collide with A.**
Compulsory(A), compulsory(B)+evict(A), conflict(A)+evict(B), compulsory(C)+evict(A), conflict(A). Three conflict misses on A.

**Q7. Why do write-back + write-allocate pair, and write-through + no-write-allocate?**
Write-back already pays for line-fetch on read miss; allocating on write keeps subsequent writes hitting. Write-through gains nothing from caching writes; allocating would only pollute the cache.

**Q8. The page-fault regression in QNN/ORT — was it a cache problem?**
No. Page faults are translation events at 4 KB; cache misses are locality events at 64 B. Different layer.

---

## Coding A — Systems C: address decode + LRU cache simulator

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define LINE       64
#define SETS       8
#define WAYS       4

typedef struct { bool valid; uint32_t tag; uint8_t lru; } line_t;
static line_t cache[SETS][WAYS];
static uint32_t hits, misses;

static void touch(line_t *l, uint8_t age) {
    for (int w = 0; w < WAYS; w++)
        if (cache[/*set*/0][w].valid && cache[0][w].lru > age) cache[0][w].lru--;
    l->lru = WAYS - 1;
}

static bool access(uint32_t addr) {
    uint32_t set = (addr / LINE) % SETS;
    uint32_t tag = (addr / LINE) / SETS;
    for (int w = 0; w < WAYS; w++) {
        if (cache[set][w].valid && cache[set][w].tag == tag) {
            hits++;
            /* update LRU within this set */
            uint8_t old = cache[set][w].lru;
            for (int x = 0; x < WAYS; x++)
                if (cache[set][x].valid && cache[set][x].lru > old) cache[set][x].lru--;
            cache[set][w].lru = WAYS - 1;
            return true;
        }
    }
    misses++;
    int victim = 0; uint8_t min_lru = 255;
    for (int w = 0; w < WAYS; w++) {
        if (!cache[set][w].valid) { victim = w; break; }
        if (cache[set][w].lru < min_lru) { min_lru = cache[set][w].lru; victim = w; }
    }
    cache[set][victim].valid = true;
    cache[set][victim].tag = tag;
    cache[set][victim].lru = WAYS - 1;
    return false;
}

int main(void) {
    /* sequential — high hit rate */
    memset(cache, 0, sizeof cache); hits = misses = 0;
    for (int i = 0; i < 64; i++) access(i * 4);
    printf("seq: hits=%u misses=%u (%.0f%%)\n", hits, misses, 100.0*hits/(hits+misses));

    /* large stride — every access misses */
    memset(cache, 0, sizeof cache); hits = misses = 0;
    for (int i = 0; i < 64; i++) access(i * 1024);
    printf("stride1024: hits=%u misses=%u (%.0f%%)\n", hits, misses, 100.0*hits/(hits+misses));
    return 0;
}
```

## Coding B — Interview DS&A: LRU Cache (LC 146)

The headline problem. Hash map for O(1) lookup + doubly-linked list for O(1) usage reorder. Move on `get`, move on `put` hit, evict tail on overflow.

```python
class Node:
    __slots__ = ('k','v','p','n')
    def __init__(self,k,v): self.k,self.v,self.p,self.n = k,v,None,None

class LRUCache:
    def __init__(self, cap):
        self.cap = cap
        self.m = {}
        self.h = Node(0,0); self.t = Node(0,0)
        self.h.n = self.t; self.t.p = self.h
    def _add(self, n):
        n.p = self.h; n.n = self.h.n
        self.h.n.p = n; self.h.n = n
    def _rm(self, n):
        n.p.n = n.n; n.n.p = n.p
    def get(self, k):
        if k not in self.m: return -1
        n = self.m[k]; self._rm(n); self._add(n); return n.v
    def put(self, k, v):
        if k in self.m: self._rm(self.m[k])
        n = Node(k, v); self.m[k] = n; self._add(n)
        if len(self.m) > self.cap:
            ev = self.t.p; self._rm(ev); del self.m[ev.k]
```

---

## Bug Anchor

- **Day 5:** false sharing is a coherence-miss class.
- **Day 8:** DMA visibility = clean line + barrier + doorbell.
- **Day 14:** write-back is *why* WPQ exists and *why* ADR has to drain it.
- **Day 19:** the QNN/ORT bug is page-fault, not cache-miss — own that distinction.

## Exit Check

- [ ] Drew the cache hierarchy with latencies from memory.
- [ ] Decoded an address into tag/set/offset for a given config.
- [ ] Stated AMAT and computed it for a worked example.
- [ ] Stated write-back + write-allocate pairing and the policy reason.
- [ ] Explained the descriptor-then-doorbell DMA pattern.
- [ ] Distinguished cache miss vs TLB miss vs page fault in 30 seconds.
- [ ] Stated the four C's of misses.

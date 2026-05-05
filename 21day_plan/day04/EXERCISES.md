# Day 04 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day04_cache_lines_locality_amat.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: address decode + LRU cache simulator

**Problem statement:** Build a software model of a set-associative cache. Decode each access into (tag, set, offset), look it up across the ways, update LRU bits on hits, evict on misses. Run two access patterns to demonstrate locality vs stride collapse.

**Geometry:** `LINE = 64`, `SETS = 8`, `WAYS = 4`.

**You must implement:**
```c
typedef struct { bool valid; uint32_t tag; uint8_t lru; } line_t;
static line_t cache[SETS][WAYS];

bool access(uint32_t addr);     /* return true on hit, false on miss; update LRU */
```

**Driver / tests to write:**
- Sequential pattern: `for (i=0;i<64;i++) access(i*4);` → high hit rate.
- Stride-1024 pattern: `for (i=0;i<64;i++) access(i*1024);` → near-100% miss.
- Print `hits, misses, hit%` for each.

**Be ready to explain in interview:**
- Decode `tag | set | offset` for the chosen geometry.
- Why does stride > line size collapse bandwidth?
- LRU vs PLRU vs Random — area/perf tradeoff.
- Compute AMAT for `(L1: 4c, miss 5%) → (L2: 12c, miss 30%) → DRAM 200c`.

## E2 — DS&A: LRU Cache (LC 146)

**Problem statement:** Implement an LRU cache supporting `get(key)` and `put(key, value)` in O(1). Use a hash map + intrusive doubly-linked list; move-to-front on access; evict tail on overflow.
**Signature (C):**
```c
typedef struct LRU LRU;
LRU *lru_create(int cap);
int  lru_get(LRU *l, int key);   /* -1 if missing */
void lru_put(LRU *l, int key, int val);
void lru_free(LRU *l);
```
**Constraints / edge cases:** capacity = 0/1, repeated put on same key (refresh, don't duplicate), get on missing key.
**Talking points:** O(1) for both operations; eviction policy choice; firmware connection — every TLB and CPU cache uses an LRU approximation in silicon.

## E3 — Architect Drills (no code, write your answers)

**D1.** Why a multi-level hierarchy (L1/L2/L3) and not one large cache? What would force a future SoC to add an L4?

**D2.** Why is the line 64B — and what would force 32B or 128B?

**D3.** Why an associativity ladder (L1 8-way, L3 16-way) and not all fully associative?

**D4.** Inclusive vs exclusive vs NINE — what workload makes each the right call? Why do AMD and Intel differ?

**D5.** Imagine DRAM latency dropped 10× tomorrow. Does the cache hierarchy still earn its area?

**D6.** Caches are transparent to software but TLBs need explicit invalidation. Why the asymmetry?

**D7.** Construct a workload where write-through + no-write-allocate beats write-back + write-allocate.

## E4 — Bug Anchor

**Symptoms:** Device DMA reads a stale descriptor. The descriptor was written by the CPU but the line is still in M state in L1; non-coherent device fetch sees stale DRAM.

Walk the debug tree: how is this caught? Required fix sequence (alignment + clean + barrier + doorbell)? How does this differ from a "cache miss" bug?

Also: explain the bandwidth collapse on stride > line size, and the conflict-miss thrash on power-of-2 strides in low-associativity caches.

## Reference solution

Once you've attempted, see `../day04_cache_lines_locality_amat.md` sections "Coding A" and "Coding B" for full solutions and discussion.

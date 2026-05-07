# Coding — Single Exercise Document

> One folder, one document. All 21 days of coding live here. Read the prompt, write the code in `exercises/dNN_<name>.c`, build, run, explain *why*. Only then peek at `solutions/dNN_<name>.c`.

## How to use

```
coding/
├── EXERCISES.md      ← this file. Follow it top to bottom.
├── macros.h          ← shared bit/field/align macros (Day 0). Used everywhere.
├── Makefile          ← unified build for everything in exercises/ and solutions/
├── exercises/        ← YOU WRITE CODE HERE. One file per problem.
│   └── dNN_<name>.c
└── solutions/        ← reference. Peek only after attempting.
    └── dNN_<name>.c
```

Build / run a single exercise (replace `<target>` with e.g. `d00_my_memcpy`):

```sh
make ex-<target>     # build + run YOUR attempt from exercises/
make sol-<target>    # build + run the reference solution
make all-ex          # build every exercise stub (will fail until filled in)
make all-sol         # build every reference solution
make clean
```

Naming convention: every file is prefixed `dNN_` where `NN` is the day number. Day 0 = `d00_*.c`, Day 1 = `d01_*.c`, etc. This is the *only* coding folder — old per-day `dayNN/` directories are gone.

## Map: what to do each study day

| Day | Topic | Systems C exercise | DS&A exercise |
|---|---|---|---|
| 0 | C basics + MMIO ramp | see [day00_c_basics.md](../21day_plan/day00_c_basics.md) | — |
| 1 | C address fluency | `d01_reg_poke` | `d01_two_sum` |
| 2 | Pointers / alignment | `d02_aligned_alloc` | `d02_reverse_list` |
| 3 | Bits / MMIO / volatile | `d03_dev_init` | `d03_popcount` |
| 4 | Cache lines / locality | `d04_cache_sim` | `d04_lru_cache` |
| 5 | Coherence / MESI / false sharing | `d05_mesi_sim` | `d05_cycle_detect` |
| 6 | Virtual memory / page walk | `d06_page_walk` | `d06_binary_search` |
| 7 | TLB / IOMMU | `d07_tlb_iommu` | `d07_first_bad` |
| 8 | DMA / doorbells / barriers | `d08_sq_doorbell` | `d08_merge_two` |
| 9 | ARM / Qualcomm platform | `d09_psci_rpmh` | `d09_valid_parens` |
| 10 | PCIe + LTSSM | `d10_ecam_ltssm` | `d10_climb_stairs` |
| 11 | NVMe queues + bring-up | `d11_nvme_bringup` | `d11_group_anagrams` |
| 12 | DVFS / P-states | `d12_dvfs` | `d12_max_profit` |
| 13 | C-states / PkgC / S0ix | `d13_pkgc6` | `d13_climb_stairs` |
| 14 | ADR / PCU / Core C6 bugs | `d14_adr` | `d14_num_islands` |
| 15 | Thermal / perf counters | `d15_tj_rapl_tma` | `d15_sliding_max` |
| 16 | Pipeline / memory model / SIMD | `d16_pipeline_bp` | `d16_count_bits` |
| 17 | Concurrency / lock-free | `d17_spinlock_spsc` | `d17_eval_rpn` |
| 18 | NPU dispatch / EP | `d18_npu_dispatch` | `d18_top_k` |
| 19 | NPU memory / runtime | `d19_regression_gate` | `d19_lru_cache` |
| 20 | Debug method | `d20_triage` | `d20_num_islands` |

Each Day 1–20 entry below has two sections: **E1 Systems C** and **E2 DS&A**. Architect drills and bug anchors live in the matching `21day_plan/dayNN_*.md` reading file (kept in one place to avoid duplication).

Build flags used everywhere:

```sh
cc -Wall -Wextra -Wpedantic -std=c11 -g -O0 -I. exercises/<file>.c -o /tmp/<file> && /tmp/<file>
```

---

# Day 1 — C address fluency

## D01-E1 — Systems C: full register-poke harness (`d01_reg_poke`)

**Problem statement.** Build a simulated MMIO harness from scratch. Back a register with a single `volatile uint32_t` (stand-in for a real BAR offset). Implement bit/field macros, typed accessors, an RMW field-write, and a bounded poll loop. Drive it end-to-end: program a field, demonstrate the timeout path, demonstrate the success path.

**You must implement:**
```c
#define BIT(n)
#define MASK(msb, lsb)
#define FIELD_GET(r, msb, lsb)
#define FIELD_PREP(v, msb, lsb)

static inline uint32_t reg_read32(volatile uint32_t *r);
static inline void     reg_write32(volatile uint32_t *r, uint32_t v);
static inline void     reg_field_write(volatile uint32_t *r, int msb, int lsb, uint32_t v);
static int             wait_for_bit_set(volatile uint32_t *r, uint32_t mask, int timeout_us);
```

**Driver / tests:**
- `fake_reg = 0xDEADBEEF`, `reg_field_write(&fake_reg, 15, 8, 0xAA)`, assert `0xDEADAAEF`.
- Timeout path: `fake_reg = 0`, `wait_for_bit_set(&fake_reg, BIT(0), 5)`, assert `rc == -1`.
- Success path: `fake_reg = BIT(0)`, call again, assert `rc == 0`.

**Interview prompts:** 3 things `volatile` prevents. Why `volatile` ≠ memory barrier. Where does the timeout come from for `CSTS.RDY`? Why is RMW on a W1C status register a bug?

## D01-E2 — DS&A: Two Sum (LC 1) (`d01_two_sum`)

Single-pass hash map. `int *twoSum(int *nums, int n, int target, int *out_i, int *out_j);` Exactly one solution, no element reused, negatives allowed. O(n)/O(n). Firmware analogue: "have I seen this descriptor index before?" lookup table.

---

# Day 2 — Pointers, memory layout, alignment

## D02-E1 — Systems C: aligned allocator + container_of (`d02_aligned_alloc`)

**Problem statement.** Build a manual aligned allocator that satisfies arbitrary power-of-2 alignment on top of plain `malloc`, plus a Linux-style `container_of` macro. Demonstrate both on an intrusive list. Also print `sizeof` for two structs that differ only in field order to show padding is layout-driven.

**Implement:**
```c
#define ALIGN_UP(x, a)
#define IS_ALIGNED(x, a)
#define IS_POW2(n)
#define container_of(ptr, type, member)

void *aligned_malloc(size_t size, size_t align);
void  aligned_free(void *p);
```

**Driver:**
- `aligned_malloc(4096, 64)` → assert `addr % 64 == 0`, then `aligned_free`.
- Build `struct work { int id; struct list_head node; };`, take `&w.node`, recover `&w` via `container_of`, print `back->id`.
- Compare `sizeof(struct {char a; int b; char c;})` vs reordered version; explain padding.

**Interview prompts:** Why does the aligned allocator stash the original pointer one slot before the aligned address? Why is `container_of` foundational to Linux callbacks (what does `offsetof` rely on)? What does `__attribute__((packed))` cost on ARM that it doesn't on x86?

## D02-E2 — DS&A: Reverse Linked List (LC 206) (`d02_reverse_list`)

In-place 3-pointer dance (`prev`, `curr`, `next`). `struct node *reverse_list(struct node *head);` Edge cases: empty, single, two, general N. O(n)/O(1). Same shape as queue rotation, stack reversal.

---

# Day 3 — Bits, MMIO, volatile, polling

## D03-E1 — Systems C: device init harness with poll + RMW (`d03_dev_init`)

**Problem statement.** Build a fake-device init harness using a small `uint8_t mmio[256]` backing array. Implement typed accessors (`reg_read`, `reg_write`, `reg_rmw`), a bounded `poll_ready` that simulates HW asserting ready after N iterations, and a top-level main that resets the device, polls for ready, enables and starts it, then writes/reads `DATA`.

**Implement:**
```c
static inline uint32_t reg_read(uint32_t off);
static inline void     reg_write(uint32_t off, uint32_t v);
static inline void     reg_rmw(uint32_t off, uint32_t mask, uint32_t v);
static bool            poll_ready(uint32_t off, uint32_t mask, uint32_t exp, uint32_t tries);
```

**Register map:** `CTRL=0x00, STATUS=0x04, DATA=0x08`. Bits: `CTRL_EN=BIT(0), CTRL_START=BIT(1), CTRL_RESET=BIT(31), STATUS_RDY=BIT(0)`.

**Driver:**
- Reset (write `CTRL_RESET` then 0).
- `poll_ready(STATUS, STATUS_RDY, STATUS_RDY, 5)` — must return true; fail = "device never ready".
- RMW set `CTRL_EN`, then `CTRL_START` (preserve other bits).
- Write `0xDEADBEEF` to `DATA` and assert readback.

**Interview prompts:** 3 ways the compiler miscompiles missing-`volatile` poll loops at `-O2`. Why is RMW on a W1C status register catastrophic? Why does `volatile` not imply a memory barrier on ARM?

## D03-E2 — DS&A: Number of 1 Bits (LC 191) (`d03_popcount`)

`int hamming_weight(uint32_t n);` Kernighan's `n &= n - 1` is O(set-bits); contrast with naive O(32) and SWAR/`__builtin_popcount`. Edge: 0, `UINT32_MAX`. Where popcount shows up: error-counting, mask-density, perf counters.

---

# Day 4 — Cache lines, locality, AMAT

## D04-E1 — Systems C: address decode + cache simulator (`d04_cache_sim`)

**Problem statement.** Software model of a set-associative cache. Decode each access into `(tag, set, offset)`, look it up across the ways, update LRU bits on hits, evict on misses. Run two access patterns to demonstrate locality vs stride collapse.

**Geometry:** `LINE = 64`, `SETS = 8`, `WAYS = 4`.

**Implement:**
```c
typedef struct { bool valid; uint32_t tag; uint8_t lru; } line_t;
static line_t cache[SETS][WAYS];
bool access(uint32_t addr);   /* return true on hit, false on miss; update LRU */
```

**Driver:**
- Sequential: `for (i=0;i<64;i++) access(i*4);` → high hit rate.
- Stride-1024: `for (i=0;i<64;i++) access(i*1024);` → near-100% miss.
- Print `hits, misses, hit%`.

**Interview prompts:** Decode `tag|set|offset` for the geometry. Why does stride > line size collapse bandwidth? LRU vs PLRU vs Random — area/perf tradeoff. Compute AMAT for `(L1: 4c, miss 5%) → (L2: 12c, miss 30%) → DRAM 200c`.

## D04-E2 — DS&A: LRU Cache (LC 146) (`d04_lru_cache`)

O(1) `get` and `put`. Hash map + intrusive doubly-linked list; move-to-front on access; evict tail on overflow.

```c
typedef struct LRU LRU;
LRU *lru_create(int cap);
int  lru_get(LRU *l, int key);   /* -1 if missing */
void lru_put(LRU *l, int key, int val);
void lru_free(LRU *l);
```

Edge cases: cap = 0/1, repeated `put` (refresh, don't duplicate), `get` on missing. Every TLB and CPU cache uses an LRU approximation.

---

# Day 5 — Coherence, MESI, false sharing

## D05-E1 — Systems C: MESI step-machine + false-sharing layout check (`d05_mesi_sim`)

**Problem statement.** Model two cores' L1 lines and one main-memory cell. Implement `rd(who)` and `wr(who, v)` that mutate `(cline_t c0, c1)` according to MESI rules — including HITM (other core in M forwards and both go to S) and write-invalidate. After each step, print `C0`, `C1`, `mem`.

Also: `struct bad { int a; int b; }` vs padded `struct good { int a; char pad[60]; int b; }`. Print whether `&a` and `&b` land on the same 64-byte line.

**Implement:**
```c
typedef enum { I = 0, S, E, M } mesi_t;
typedef struct { mesi_t st; int v; } cline_t;
void rd(int who);
void wr(int who, int v);
```

**Driver:** sequence `rd(0)→E. rd(1)→both S. wr(0,42)→C0 M, C1 I. rd(1)→HITM, both S. wr(1,99)→C1 M, C0 I.` Plus the layout check.

**Interview prompts:** MESI vs MOESI vs MESIF in one sentence each. Why does false sharing cost ~50–80 cycles per write? HITM counter and `perf c2c` — what do they observe? Why does padding fail if the struct itself isn't 64-B aligned?

## D05-E2 — DS&A: Linked-List Cycle (LC 141, Floyd) (`d05_cycle_detect`)

`int has_cycle(struct node *head);` O(n)/O(1). Edge: empty, single, self-loop, cycle at head, cycle mid-list. Analogue: snoop-race detection (two pointers, different speeds).

---

# Day 6 — Virtual memory, page faults

## D06-E1 — Systems C: 4-level page walk (`d06_page_walk`)

**Problem statement.** x86-64-style 4-level page walk in C: four 512-entry tables (`pml4`, `pdpt`, `pd`, `pt`). Each PTE carries `P` and `RW` flags plus a frame-aligned PFN. The walk takes a 64-bit VA, walks the four levels by 9-bit indices, returns either a physical address or 0 with a reason string on fault.

**Implement:**
```c
#define PG_SHIFT 12
#define IDX(va, lvl) (((va) >> (PG_SHIFT + 9 * (lvl))) & 0x1FFu)
typedef uint64_t pte_t;
#define P  (1ull << 0)
#define RW (1ull << 1)
#define PFN(p) ((p) & ~0xFFFull)

uint64_t walk(uint64_t va, const char **why);
```

**Driver:** Map `VA 0x0000 → PA 0x1000` and `VA 0x1000 → PA 0x2000`. Walk both → success. Walk `0x2000` → fault `"PT not present"`.

**Interview prompts:** Why 4 levels and not flat? Why is page offset never translated? PFN vs physical address? Page fault vs TLB miss vs cache miss in 30 sec.

## D06-E2 — DS&A: Binary Search (LC 704) (`d06_binary_search`)

`int binary_search(const int *nums, int n, int target);` Edge: empty, single, target outside range, duplicates. Use overflow-safe `mid = lo + (hi - lo) / 2`. Same predicate-driven shape as Day 7 First-Bad-Version, Day 11 NVMe head-tail bisection.

---

# Day 7 — TLB, huge pages, IOMMU/SMMU

## D07-E1 — Systems C: TLB lookup with FIFO + IOMMU translate (`d07_tlb_iommu`)

**Problem statement.** Two collaborating models.

(a) **4-entry FIFO TLB** caching `vpn → pfn`. On miss, call a fake `page_walk(vpn)` (any deterministic hash), install at `next_evict`, advance index. Track `hits`/`misses`.

(b) **Two-level IOMMU per BDF.** `iommu_translate(ctx, iova, write, *pa)` walks L1 (1024 entries) → L2 (1024 entries). Faults: missing L1, missing L2, write-to-RO. Print `IOMMU FAULT BDF %04X iova %08X: <reason>`.

**Implement:**
```c
typedef struct { uint32_t vpn, pfn; bool valid; } tlb_t;
uint32_t translate(uint32_t va);

typedef struct { uint32_t pfn; bool present, writable; } io_pte_t;
typedef struct { io_pte_t pt[1024]; bool present; } io_pd_t;
typedef struct { uint16_t bdf; io_pd_t *pd[1024]; } iommu_ctx_t;
bool iommu_translate(iommu_ctx_t *c, uint32_t iova, bool wr, uint64_t *pa);
```

**Driver:** 7-access pattern through `translate`; print `hits/misses`. IOMMU ctx demonstrating (i) L1 missing, (ii) L2 missing, (iii) RO write fault.

**Interview prompts:** TLB coverage = entries × page size; the 4KB→2MB lever. TLB shootdown via IPI; ARM hardware TLBI broadcast vs x86. Why does the device need a separate IOMMU when the host has an MMU? ATS+PRI — what does it buy and what does it cost?

## D07-E2 — DS&A: First Bad Version (LC 278) (`d07_first_bad`)

Predicate binary search. `int first_bad_version(int n);` over an API `bool is_bad(int v)`. Edge: `n=1`, first version bad, last bad, none bad. Avoid overflow on `(lo+hi)/2`. Same monotonic-predicate shape as scanning sorted PTEs for first-set.

---

# Day 8 — DMA, doorbells, ordering, barriers

## D08-E1 — Systems C: SQ ring + doorbell harness (`d08_sq_doorbell`)

**Problem statement.** Producer side of an NVMe-style submission queue. Build a 16-entry ring, a `volatile` MMIO doorbell stand-in, a `sq_submit(cmd, arg)` that fills the descriptor, issues a memory barrier, advances the tail, writes the doorbell. Then `device_consume_one()` simulates the device picking up the next entry from `sq_head`.

**Implement:**
```c
typedef struct { uint32_t cmd; uint32_t arg; } sqe_t;
int  sq_submit(uint32_t cmd, uint32_t arg);   /* 0 ok, -1 full */
void device_consume_one(void);

#define COMPILER_BARRIER() __asm__ volatile("" ::: "memory")
#define MEMORY_BARRIER()   __sync_synchronize()
#define CACHE_CLEAN(p, n)  /* stub */
```

**Driver:** `sq_submit(0x01, 0xAAAA)`, `sq_submit(0x02, 0xBBBB)`, then `device_consume_one()` × 3 (third = empty). Print head/tail movement.

**Interview prompts:** Why descriptor → barrier → doorbell, *always* in that order? Why is the failure intermittent? `volatile` vs `dmb st` vs cache clean — what does each enforce? ARM `DMB ISH` vs `OSH` vs `SY` — when does each apply? IO-coherent vs non-coherent DMA — what changes in the driver?

## D08-E2 — DS&A: Merge Two Sorted Lists (LC 21) (`d08_merge_two`)

Splice nodes in place, no new allocation. `struct node *merge_two(struct node *a, struct node *b);` Edge: either empty, very different lengths, ties (stable). O(n+m)/O(1). Same shape as draining two CQs into one event log.

---

# Day 9 — ARM / Qualcomm platform vocabulary

## D09-E1 — Systems C: PSCI dispatcher + RPMh vote aggregator (`d09_psci_rpmh`)

**Problem statement.** Two small models that mirror the Qualcomm/ARM power stack.

(a) **PSCI dispatcher.** `psci_call(fid, a1, a2)` recognising `CPU_ON (0x84000003)`, `CPU_OFF (0x84000002)`, `SYSTEM_SUSPEND (0xC400000E)`, `SYSTEM_RESET (0x84000009)`. Maintain `cpus[MAX_CPUS]`; CPU0 initially `ON`. `CPU_ON` validates target index, errors if already on, prints entry-point.

(b) **RPMh aggregator.** Each of N=4 voters submits a `vote_t { freq_khz, volt_mv }`. Aggregator returns per-field max across all voters (each rail floor).

**Implement:**
```c
int     psci_call(uint32_t fid, uint64_t a1, uint64_t a2);
typedef struct { uint32_t freq_khz; uint32_t volt_mv; } vote_t;
vote_t  rpmh_aggregate(void);
```

**Driver:** `CPU_ON` for CPU 1 entry `0x80000000` → success. Repeat → already-on error. Populate 4 voter slots (Apps/Modem/CDSP/idle), call `rpmh_aggregate`. `SYSTEM_SUSPEND` with resume entry.

**Interview prompts:** ARM EL0/1/2/3 vs x86 ring 0/3 — why four levels? `TTBR0/1_EL1 + ASID` vs `CR3 + PCID`; granule sizes in `TCR_EL1`. Why does Snapdragon need RPMh on top of PSCI when x86 has PCU alone? GIC SGI/PPI/SPI/LPI; how does ITS handle PCIe MSI?

## D09-E2 — DS&A: Valid Parentheses (LC 20) (`d09_valid_parens`)

`int is_valid_parens(const char *s);` Empty = true. Edge: unbalanced opener/closer, nested mismatch, long input. O(n) stack walk. Same shape as walking PCIe capability chains (Day 10) or matching `BEGIN`/`END` markers in a debug log (Day 20).

---

# Day 10 — PCIe fundamentals + LTSSM

## D10-E1 — Systems C: ECAM walker + LTSSM tracker (`d10_ecam_ltssm`)

**Problem statement.** Two helpers.

(a) **ECAM address compute.** Given `(base, bus, dev, fn, reg)`, return the 64-bit ECAM address `bus<<20 | dev<<15 | fn<<12 | (reg & 0xFFF)`. Also `bar_size_from_readback(uint32_t rb)`: write `0xFFFFFFFF` to BAR, read back, mask type bits, invert and add 1 to get size.

(b) **LTSSM tracker.** Define the 10 states (Detect, Polling, Config, L0, Recovery, L0s, L1, L1.1, L1.2, L2). Build an allowed-transition table. `go(link, to)` enforces legality, prints transitions, counts `recoveries` whenever entering Recovery.

**Implement:**
```c
uint64_t ecam_addr(uint64_t base, uint8_t bus, uint8_t dev, uint8_t fn, uint16_t reg);
uint64_t bar_size_from_readback(uint32_t rb);

typedef enum { LTSSM_DETECT, LTSSM_POLLING, LTSSM_CONFIG, LTSSM_L0,
               LTSSM_RECOVERY, LTSSM_L0S, LTSSM_L1, LTSSM_L1_1,
               LTSSM_L1_2, LTSSM_L2, LTSSM_NUM } ltssm_t;
typedef struct { ltssm_t st; unsigned recoveries; } link_t;
bool go(link_t *l, ltssm_t to);
```

**Driver:** ECAM `01:02.0` BAR0 against `base=0xE0000000`. `bar_size_from_readback(0xFFF00004)` → `0x100000`. Walk `Detect → Polling → Config → L0 → Recovery → L0 → L1 → L1.2 → Recovery → L0`. One illegal `Detect → L0` → `ILLEGAL`.

**Interview prompts:** 3 PCIe layers (PHY/DLL/TL); what each isolates. Posted vs non-posted; why doorbell-as-MWr won. L1 vs L1.1 vs L1.2; PkgC6 dependency on L1.2. Credit-based flow control vs ACK/NAK pacing.

## D10-E2 — DS&A: Climbing Stairs (LC 70) (`d10_climb_stairs`)

`long long climb_stairs(int n);` 1 or 2 steps. Edge: `n=0` (1 way), `n=1`, `n=45` (overflow on `int`). O(n) DP, O(1) space. Same recurrence as counting LTSSM substate paths or C-state transition paths (Day 13).

---

# Day 11 — NVMe queues + bring-up

## D11-E1 — Systems C: NVMe bring-up + SQ submit + CQ poll (`d11_nvme_bringup`)

**Problem statement.** Software model of NVMe controller bring-up on top of an in-memory `bar[BAR_SZ]` array.

Implement:
- `r32(off)`, `w32(off, v)`, `w64(off, v)` accessors (with the side-effect that writing `CC.EN=1` causes `CSTS.RDY` to follow EN — model stand-in for the device asserting ready).
- `sim_init()` populates `CAP` with `MQES=255, TO=10, DSTRD=0`.
- `db_off(q, is_cq, dstrd)` returns `0x1000 + (2*q + (is_cq?1:0)) * (4 << dstrd)`.
- `nvme_bringup()` performs the canonical 5-step bring-up: read CAP; disable + poll RDY=0; program AQA/ASQ/ACQ (admin queue, size = 32, AQA encodes `size-1` in both halves); enable with `IOSQES=6, IOCQES=4, EN=1`; poll RDY=1; print computed doorbell offsets.

**Register map:** `CAP=0x00, VS=0x08, CC=0x14, CSTS=0x1C, AQA=0x24, ASQ=0x28, ACQ=0x30, DB=0x1000`.

**Implement:**
```c
uint32_t r32(uint32_t off);
void     w32(uint32_t off, uint32_t v);
void     w64(uint32_t off, uint64_t v);
uint32_t db_off(uint16_t q, bool is_cq, uint8_t dstrd);
bool     nvme_bringup(void);
```

**Driver:** `sim_init()` then `nvme_bringup()`. Expect printed CAP fields, "Controller READY", four doorbell offsets for queues 0 and 1. **Stretch:** simulate the AQA off-by-one bug and show the failure path.

**Interview prompts:** Walk `CSTS.RDY` debug through 5+ suspects (CFS, AQA off-by-one, DSTRD, PRP IOVA, IOMMU EVTQ, MSI-X timing). PRP vs SGL — what workload forced SGLs? Phase bit on CQ — when does it lose? Per-core SQ/CQ pair scaling vs AHCI.

## D11-E2 — DS&A: Group Anagrams (LC 49) (`d11_group_anagrams`)

```c
typedef struct { char **items; int count; } group_t;
group_t *group_anagrams(char **strs, int n, int *out_groups);
```

Edge: empty, single, all anagrams, no dupes. Sort-as-key → O(n·k log k); count-tuple key → O(n·k). Hash-key choice equivalent to descriptor-bucket assignment in firmware log triage.

---

# Day 12 — DVFS / P-states / PVT

## D12-E1 — Systems C: DVFS table lookup with thermal cap + IPC counter (`d12_dvfs`)

**Problem statement.** Build a DVFS controller. Given a 5-entry P-state table (P4..P0 from `(400 MHz, 750 mV)` to `(2400 MHz, 1150 mV)`), implement `select_opp(target_mhz, tj_c)` that picks the lowest OPP meeting target frequency, **capped to ≤1200 MHz when `tj_c >= 90`** (thermal throttle). Then `apply_opp(cur, next)` that prints the correct sequencing — **V before F when ramping up, F before V when ramping down**.

Plus a tiny `pmu_t { insns, cycles }` and `ipc()` helper.

**Implement:**
```c
typedef struct { uint32_t freq_mhz; uint32_t volt_mv; } opp_t;
const opp_t *select_opp(uint32_t target_mhz, int tj_c);
void         apply_opp(const opp_t *cur, const opp_t *next);

typedef struct { uint64_t insns, cycles; } pmu_t;
double ipc(const pmu_t *p);
```

**Driver:** OPP for `(400, 50)`, then `(2000, 50)` → ramp-up sequence printed. `(2400, 95)` → thermal cap to 1200 MHz, ramp-down sequence printed. IPC for `insns=15e9, cycles=12.34e9` and flag if `< 1.0`.

**Interview prompts:** Derive $P_{dyn} = \tfrac{1}{2}CV^2F$; explain V² dominance. Why V-before-F up, F-before-V down? Setup-time slack argument. Why discrete P-states vs continuous? Validation-matrix argument. Snapdragon DVFS through RPMh + PMIC (per-cluster rail vs per-core).

## D12-E2 — DS&A: Best Time to Buy and Sell Stock (LC 121) (`d12_max_profit`)

`int max_profit(const int *prices, int n);` O(n) single pass tracking running min. Edge: empty, monotone-decreasing (profit 0), single. Same shape as a power-counter rolling baseline.

---

# Day 13 — C-states / PkgC / S0ix

## D13-E1 — Systems C: C-state request + PkgC6 PCU gate model (`d13_pkgc6`)

**Problem statement.** Model the PCU's PkgC6 gating. 4 cores (`cores[NCORES]`), each with `cstate_t cs` and a `work_pending` flag. `request_core_cs(id, target)` refuses any state deeper than C1 if work is pending. `try_pkg_c6()` checks **all** cores in C6 *and* every uncore IP (`llc_idle`, `mc_idle`, `pcie_in_l1_2`, `display_idle`, `intc_idle`) reports idle. If blocked, print which IP blocks. Default `pcie_in_l1_2 = false` so the first attempt fails.

**Implement:**
```c
typedef enum { C0=0, C1=1, C1E=2, C6=6 } cstate_t;
typedef struct { cstate_t cs; bool work_pending; } core_t;
typedef struct { bool llc_idle, mc_idle, pcie_in_l1_2, display_idle, intc_idle; } uncore_t;

bool request_core_cs(int id, cstate_t target);
bool try_pkg_c6(void);
const char *blocker(void);
```

**Driver:** All 4 cores → request C6. `try_pkg_c6()` → "BLOCKED: PCIe link not in L1.2". Set `un.pcie_in_l1_2 = true`, retry → "PkgC6 ENTERED".

**Interview prompts:** C1 vs C1E vs C6 vs C10 — clock-gated vs power-gated, retention SRAM, wake latency. Why does PkgC6 need *every* uncore IP idle? Shared-rail and snoop-traffic arguments. Break-even residency $T_r > (E_{\text{in}} + E_{\text{out}}) / \Delta P$. L1.2 dependency for PkgC6; LTR/CLKREQ#/ASPM bring-up checklist.

## D13-E2 — DS&A: Climbing Stairs (LC 70) — reused (`d13_climb_stairs`)

Same as Day 10 — count distinct ways to reach step `n` with steps of 1 or 2. Reused as the path-counting analogue for C-state transitions through a strict-ordering ladder.

---

# Day 14 — ADR, PCU, Core C6 bugs

## D14-E1 — Systems C: ADR opcode-sequence simulator (`d14_adr`)

**Problem statement.** Model the PCU's ADR (Asynchronous DRAM Refresh) flow as an opcode sequence. Two versions:

(a) **Buggy** — issues `WPQ_FLUSH` without first waiting for `IO_QUIESCE_ACK`. Model detects this and prints `BUG: WPQ_FLUSH issued before IO_QUIESCE_ACK -> WPQ refills, hang`.

(b) **Fixed** — strict ordering: `IO_QUIESCE` + ack → `FABRIC_QUIESCE` → optional `LLC_WRITEBACK` (eADR) → `WPQ_FLUSH` + ack → `DDR_SELF_REFRESH` → `NVDIMM_SAVE`. Report `in_self_refresh = true` on success.

**Implement:**
```c
typedef enum {
    OP_IO_QUIESCE, OP_IO_QUIESCE_ACK, OP_FABRIC_QUIESCE, OP_LLC_WRITEBACK,
    OP_WPQ_FLUSH, OP_WPQ_FLUSH_ACK, OP_DDR_SELF_REFRESH, OP_NVDIMM_SAVE, OP_DONE
} adr_op_t;

typedef struct {
    bool io_quiesced, fabric_quiesced, wpq_drained, in_self_refresh, eadr;
} adr_state_t;

int adr_run_buggy(adr_state_t *s);
int adr_run_fixed(adr_state_t *s);
```

**Driver:** Buggy → expect failure print. Fixed → expect `rc=0`, `in_self_refresh=1`.

**Interview prompts:** Cambria PCU FW bug (`WPQ_FLUSH` before `IO_QUIESCE_ACK`); Zebu opcode trace; Perspec assertion gating regression. Core C6 wake-lost CFC FIFO race; Lauterbach trigger `wake_in==1 && cfc_init_busy==1`; fix is hold latch in always-on until `init_done && drain_done`. ADR hold-up budget physics (10–30 ms). Why PCU is firmware, not RTL.

## D14-E2 — DS&A: Number of Islands (LC 200) (`d14_num_islands`)

`int num_islands(char **grid, int rows, int cols);` 4-directional flood-fill, BFS or DFS. Edge: empty, all water, all land (1), single cell. O(R·C). Same shape as connected-component analysis on a die-thermal hotspot map or fault-cluster localisation in JTAG scan.

---

# Day 15 — Thermal, perf counters, multi-IP

## D15-E1 — Systems C: Tj model + PROCHOT + RAPL delta + IPC (`d15_tj_rapl_tma`)

**Problem statement.** Three composing models for a perf/thermal validation harness.

(a) **Thermal step.** $T_j = T_a + P \cdot \theta_{JA}$ with first-order RC smoothing (tau=5s). On each `dt`, push `tj` toward steady state. Assert PROCHOT when `tj >= prochot`; clear when `tj < prochot - 5` (5°C hysteresis). When throttled, the step uses 75 W instead of 125 W (caller decides, model just observes).

(b) **RAPL delta → watts.** `rapl_watts(e0, e1, unit_uj, dt)` converts a counter delta + LSB-µJ + duration to watts.

(c) **Top-down dominant bucket.** Given fractions `frontend_bound, bad_spec, retiring, mem_bound, core_bound`, return the name of the largest.

**Implement:**
```c
typedef struct { double tj, ta, theta, prochot; bool throttled; } thermal_t;
void thermal_step(thermal_t *m, double power, double dt);

double rapl_watts(uint64_t e0, uint64_t e1, uint64_t unit_uj, double dt);

typedef struct { double frontend_bound, bad_spec, retiring, mem_bound, core_bound; } tma_t;
const char *tma_dominant(const tma_t *t);
```

**Driver:** 30-second sim with ambient 25, theta 0.3, prochot 95, power 125 W (throttled → 75) — print PROCHOT enter/clear events, final `tj` and `throttled`. `rapl_watts(0, 65000000, 1, 1.0)` → 65.0 W. TMA `(0.15, 0.05, 0.25, 0.45, 0.10)` → "Backend Memory".

**Interview prompts:** $T_j = T_a + P\theta_{JA}$ as lumped RC; when the model breaks (transient PROCHOT, hotspot). Why RAPL underestimates wall by 20–30%; what closes each gap. Top-down's 4 buckets; what they isolate. HW prefetcher hides missing software prefetch — why disabling HW PF is triage, not fix.

## D15-E2 — DS&A: Sliding Window Maximum (LC 239) — counter-burst peak detector (`d15_sliding_max`)

`int *sliding_max(const int *nums, int n, int k, int *out_len);` O(n) using a monotonic deque of indices. Edge: `k=1`, `k=n`, all-equal, strictly decreasing. Same shape as a perf-counter peak-detector over a rolling window.

---

# Day 16 — Pipeline, memory model, SIMD

## D16-E1 — Systems C: pipeline simulator + 2-bit branch predictor + license-level model (`d16_pipeline_bp`)

**Problem statement.** Three small models you can talk through in 60 seconds.

(a) **2-bit saturating branch predictor.** State `bp ∈ [0,3]`. Predict NT if `bp < 2`, T if `bp ≥ 2`. Update saturates: `bp++` on taken, `bp--` on not-taken. Score against a fixed branch stream and report accuracy.

(b) **License-level effective frequency.** Given fractions of time spent in scalar / AVX2 / AVX-512 plus a base GHz, return effective frequency where AVX2 = 0.90×, AVX-512 = 0.75× base.

(c) **5-stage pipeline diagram.** Print `IF/ID/EX/ME/WB` for 4 instructions with one load-use stall after the first instruction (a 1-cycle bubble before instruction 2).

**Implement:**
```c
bool   predict(void);
void   update(bool taken);
double effective_freq_ghz(double scalar_pct, double avx2_pct, double avx512_pct, double base_ghz);
void   pipeline_demo(void);
```

**Driver:** Stream `{1,1,1,0,1,1,0,1,1,1,0,0,1}` → print BP accuracy. `(1.0, 0, 0, 4.0)` → 4.00 GHz; `(0.9, 0, 0.1, 4.0)` → ~3.90 (note net loss vs pure scalar). `pipeline_demo()` prints the 4-row diagram.

**Interview prompts:** OoO vs in-order; what extracts ILP. TSO vs weak; producer-consumer ring race; `dmb st` between data and flag. AVX-512 license-level pessimisation; when sprinkling AVX-512 is a net loss. HW prefetcher disable (`MSR 0x1A4`) as triage when silicon A passes / B regresses.

## D16-E2 — DS&A: Counting Bits (LC 338) (`d16_count_bits`)

`int *count_bits(int n, int *out_len);` DP recurrence `bits[i] = bits[i >> 1] + (i & 1)`. Edge: `n=0`, `n=1`, large `n`. O(n), beats per-element popcount. Mask-density precomputation for perf counters.

---

# Day 17 — Concurrency, lock-free, DSA

## D17-E1 — Systems C: spinlock + SPSC ring (full) (`d17_spinlock_spsc`)

**Problem statement.** Two lock-free / lock-light primitives end-to-end.

(a) **Spinlock** — test-and-test-and-set. `spin_lock` busy-loads with `memory_order_relaxed`, then tries `compare_exchange_weak` with `memory_order_acquire`. `spin_unlock` does `memory_order_release`. Drive with two pthreads each bumping a shared `counter` 100 000 times; assert final value `== 2 * 100000`.

(b) **SPSC ring** — `R = 1024` slots. `head` and `tail` MUST sit on separate 64-byte cache lines (`alignas(64)`). Push: load tail relaxed, head acquire, write slot, store tail release. Pop: mirror. Drive: push 5 squares, pop them all, print.

**Implement:**
```c
typedef _Atomic int spinlock_t;
void spin_lock(spinlock_t *l);
void spin_unlock(spinlock_t *l);

typedef struct {
    int buf[R];
    alignas(64) _Atomic uint32_t head;
    alignas(64) _Atomic uint32_t tail;
} spsc_t;
bool spsc_push(spsc_t *r, int v);
bool spsc_pop (spsc_t *r, int *v);
```

**Driver:** Two-thread counter test → expect `200000`. SPSC push 5 squares (`0,1,4,9,16`), drain, print.

**Interview prompts:** 3 atomics guarantees: atomicity, visibility, ordering. `volatile` gives only compiler ordering. Pick weakest correct order; relaxed for counters, acq/rel for locks and producer-consumer flags, seq_cst rarely. Why SPSC needs no CAS; what changes at MPMC. ABA problem; defenses (tagged pointer, hazard pointer, RCU). Cache-line padding between `head`/`tail` is the difference between toy and shipped code.

## D17-E2 — DS&A: Evaluate Reverse Polish Notation (LC 150) (`d17_eval_rpn`)

`int eval_rpn(char **tokens, int n);` Postfix; integer or `+ - * /`. Stack walk. Edge: single number, division truncates toward zero, assume well-formed. O(n). Same shape as walking nested capability chains in PCIe config (Day 10) or matching `BEGIN`/`END` in a debug log.

---

# Day 18 — NPU dispatch / EP

## D18-E1 — Systems C: NPU host harness model (`d18_npu_dispatch`)

**Problem statement.** Tiny simulator of the host side of NPU dispatch. Each `op_t` says whether it is `npu_supported`. `run_graph(graph, n, &stats)` walks the ops, assigns each to NPU or CPU EP, charges latency per op (NPU = 0.2 µs, CPU = 5.0 µs), and adds a 50 µs "tensor copy" cost at every EP boundary (transition from one EP to the other). Track `total_ops, npu_ops, cpu_ops, partition_boundaries, sim_latency_us`.

Use it to demonstrate the **silent EP fallback cliff**: a clean 6-op graph runs on NPU; inserting a single unsupported `CustomOp` adds two EP boundaries and ~100 µs of pure copy cost.

**Implement:**
```c
typedef enum { EP_NPU, EP_CPU } ep_t;
typedef struct { const char *name; bool npu_supported; int flops; } op_t;
typedef struct { int total_ops, npu_ops, cpu_ops, partition_boundaries; double sim_latency_us; } stats_t;
void run_graph(const op_t *graph, int n, stats_t *s);
```

**Driver:** Clean graph: `Conv1, Relu1, Conv2, Add, Conv3, Softmax` (all NPU-supported) → 0 boundaries. Fallback graph: insert `{"CustomOp", false, 1}` after `Relu1` → 2 boundaries, ~100 µs added. Print `[CLEAN]` and `[FALLBACK]` summaries.

**Interview prompts:** 6-layer stack (Framework → ONNX → ORT → EP → vendor runtime → silicon). QDQ fusion; what happens when QDQ doesn't fuse. VTCM tile-size cliff; why it's sharp not smooth. Context cache; why first-frame cost is seconds without it. Why NPU at all (energy/MAC table vs CPU AVX-512 / GPU SIMT).

## D18-E2 — DS&A: Top-K Frequent Elements (LC 347) (`d18_top_k`)

`int *top_k_frequent(const int *nums, int n, int k, int *out_len);` Bucket sort by frequency in O(n). Edge: `k=1`, all unique, all same, ties broken arbitrarily. Same shape as bucketing kernel-launch counts when triaging an EP-fallback log.

---

# Day 19 — NPU memory / runtime / scheduling

## D19-E1 — Systems C: NPU regression-gate harness (`d19_regression_gate`)

**Problem statement.** Build `gate_check(g, r)` that takes a `gate_t` of thresholds and a `run_t` of measured values from one inference run, prints a `FAIL` line for each violated metric, and returns overall pass/fail.

Metrics tracked: `npu_op_pct (≥)`, `page_faults (≤)`, `peak_vm_mb (≤)`, `per_tile_ms (≤)`, `s0ix_residency_pct (≥)`. Use these to score three runs (known-good, regressed, fallback-only) and prove the gate catches each different failure mode.

**Implement:**
```c
typedef struct { const char *ep_version, *ort_version, *model_build;
                 int npu_op_pct, page_faults, peak_vm_mb;
                 double per_tile_ms; int s0ix_residency_pct; } run_t;
typedef struct { int min_npu_op_pct, max_page_faults, max_peak_vm_mb;
                 double max_per_tile_ms; int min_s0ix_residency_pct; } gate_t;
bool gate_check(const gate_t *g, const run_t *r);
```

**Driver:** Gate: `min_npu_op_pct=90, max_page_faults=10000, max_peak_vm_mb=350, max_per_tile_ms=4.0, min_s0ix=80`.
Run A (known-good): `(95, 7045, 300, 3.0, 85)` → PASS.
Run B (regressed): `(92, 102445, 502, 3.1, 84)` → FAIL on page faults + VM.
Run C (fallback): `(40, 8000, 310, 5.0, 84)` → FAIL on npu_op_pct + per_tile_ms.

**Interview prompts:** QNN/ORT page-fault story (~100K minor faults, VM 300→500 MB) — version-matrix isolation, "page faults alone are not the bug, correlate with virtual-alloc." Paint SR vs Camera (single non-preemptive HTP queue, 108 ms wait in GPUView). Fix is framework priority lanes, not silicon preempt. Silent EP fallback after rebuild without `keep_quantized_weights=true`. NPU keep-alive blocking PkgC10/S0ix. "Never blame silicon first."

## D19-E2 — DS&A: LRU Cache (LC 146) — reused (`d19_lru_cache`)

Same as Day 4 — O(1) LRU. Reused as the data structure backing an EP context cache. Move-to-front on `get` matters for production NPU runtimes that re-use compiled artifacts.

---

# Day 20 — Debug methodology

## D20-E1 — Systems C: debug-log triage helper (`d20_triage`)

**Problem statement.** `triage(repro_rate, unique_units, corner_dependent)` classifier:

- `CLASS_FUNCTIONAL` — design/FW (`repro_rate ≥ 0.99`)
- `CLASS_RACE` — race/timing (`repro_rate < 0.50`)
- `CLASS_CORNER` — PVT (V/T/process) when corner_dependent and seen on multiple units
- `CLASS_DEFECT` — silicon defect (single unit)
- `CLASS_UNKNOWN` — otherwise

Print a small table over a fixed list of reports.

**Implement:**
```c
typedef enum { CLASS_FUNCTIONAL, CLASS_RACE, CLASS_CORNER, CLASS_DEFECT, CLASS_UNKNOWN } class_t;
const char *class_name(class_t c);
class_t triage(double repro_rate, int unique_units, int corner_dependent);

typedef struct { const char *name; double repro; int units; int corner; } report_t;
```

**Driver:**
- `{"ADR hang on Zebu",       1.00, 5, 0}` → FUNCTIONAL
- `{"C6 ARAT wake loss",      0.05, 4, 0}` → RACE
- `{"V-droop crash @105C",    0.40, 6, 1}` → CORNER
- `{"Single-unit boot fail",  0.95, 1, 0}` → DEFECT
- `{"NVMe CSTS.RDY hang",     1.00, 3, 0}` → FUNCTIONAL

**Interview prompts:** 7-step framework: observe → reproduce → isolate → hypothesise → test → root-cause → fix. Reproducibility rate maps to bug class (100% / intermittent / corner / single-unit). Cost rises 10× per stage RTL → emul → FPGA → post-Si. Bug-story shape: symptom → hypothesis → bounding experiment → fix → regression test, ≤90 sec. Honest framing ("platform validation, not Hexagon RTL").

## D20-E2 — DS&A: Number of Islands (LC 200) — reused (`d20_num_islands`)

Same as Day 14 — 4-connected `'1'` regions. Reused as the canonical "isolation" pattern; also models clustering of fault sites in scan dump.

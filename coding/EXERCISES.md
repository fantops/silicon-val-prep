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
| 0 | C basics + MMIO ramp | `d00_test_macros` … `d00_bench` (11 exercises) | — |
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

Each Day 1–20 entry below has four sections: **E1 Systems C**, **E2 DS&A**, **E3 Architect Drills** (no code, write your answers), **E4 Bug Anchor** (the failure story this concept owns).

Build flags used everywhere:

```sh
cc -Wall -Wextra -Wpedantic -std=c11 -g -O0 -I. exercises/<file>.c -o /tmp/<file> && /tmp/<file>
```

---

# Day 0 — Embedded-C basics ramp

> Sequential warm-up before Day 1. Each exercise wires one piece of MMIO/embedded fluency. Do them in order.

## D00-E1 — `d00_test_macros` — bit/field macros

Implement these macros in `macros.h` (stubs are already there):

```c
BIT(n)
MASK(msb, lsb)
FIELD_GET(reg, msb, lsb)
FIELD_PREP(val, msb, lsb)
FIELD_WRITE(reg, val, msb, lsb)   // RMW
ALIGN_UP(x, a)  ALIGN_DOWN(x, a)  IS_ALIGNED(x, a)
POW2(n)                           // true if n is a power of 2
ARRAY_SIZE(a)
```

**Driver to write in `exercises/d00_test_macros.c`:**

1. Take `r = 0xDEADBEEF`.
2. Print `MASK(15,8)`, `FIELD_GET(r,15,8)` (must print `0xBE`).
3. Use `FIELD_WRITE(r, 0xAA, 15, 8)` and assert result is `0xDEADAAEF`.
4. Show that `BIT(31)` is `0x80000000` *without* triggering UB (explain why `1 << 31` is wrong).
5. Assert `IS_ALIGNED(0x1000, 0x1000)` and `!IS_ALIGNED(0x1004, 8)` is false.

**Gotchas you must be able to explain in interview:**
- Why `UINT32_C(1)` and not `1U`?
- Why does `MASK(31, 0)` need the `UINT32_MAX >> ...` form rather than `((1U << 32) - 1)`?
- Why is `FIELD_PREP` masked *after* shifting?

## D00-E2 — `d00_mmio` — MMIO accessors

Write `volatile`-correct accessors against a software-simulated register block (a `uint32_t` array on the heap stands in for a BAR).

```c
uint32_t reg_read32(volatile uint32_t *reg);
void     reg_write32(volatile uint32_t *reg, uint32_t v);
void     reg_field_write(volatile uint32_t *reg, uint8_t msb, uint8_t lsb, uint32_t v);
```

**Driver:**
1. Allocate 4 KiB; treat as 1024 `uint32_t` registers.
2. `reg_field_write(&block[2], 7, 4, 0xC)` and read it back.
3. Print before/after binary; assert neighbour bits are untouched.

**Be ready to answer:**
- What does `volatile` actually prevent the compiler from doing? (3 things.)
- `volatile` is not a memory barrier — what does that mean in practice?
- Why must MMIO writes be sized exactly (no `memcpy` into a register)?

## D00-E3 — `d00_poll` — bounded polling

```c
int wait_for_bit_set(volatile uint32_t *reg, uint8_t bit, uint32_t timeout_us);
/* return 0 on bit observed set; -ETIMEDOUT on timeout */
```

**Driver:**
1. Spawn a "producer" call from inside the poll loop that flips the bit after N iterations (success path).
2. Demonstrate the *timeout* path by *not* flipping the bit, with `timeout_us = 1000`.
3. Print number of iterations consumed.

**Comment-in-code answers:**
- Why never `while (!(*reg & BIT(x)));` without a timeout?
- Why is `udelay()` between polls usually wrong on MMIO? (Hint: read latency, posted-write fence, NVMe `CSTS.RDY`.)
- How would you change this for a sleep-capable context vs. ISR?

## D00-E4 — `d00_my_memcpy`

Reimplement `memcpy(void *dst, const void *src, size_t n)` from scratch.

**Constraints:** byte-by-byte first; clean under `-Wall -Wextra -Wpedantic`. Unit-test with `n=0`, 1 byte, and *deliberately* show overlap is UB. Print `src`, `dst`, `dst[i]` after copy.

**Stretch:** word-aligned fast path with byte head/tail. Why does it win on cache lines? (Day 4.)

## D00-E5 — `d00_my_memmove`

Same prototype as `memcpy`, but **must handle overlap**.

**Cases:** `dst < src` (forward safe), `dst > src` (must copy backward), `dst == src` (no-op).
**Be ready to derive:** the *exact* check `dst < src || dst >= src + n` for forward-safe.

## D00-E6 — `d00_reverse_list`

Singly-linked list. Implement `struct node *reverse(struct node *head);` for `struct node { int v; struct node *next; };`.
Build `1→2→3→4→5`, reverse, print. Implement **iterative** and **recursive**; explain the stack-depth cost of recursion.

## D00-E7 — `d00_cycle_detect`

Floyd's tortoise-and-hare. Return 1 if cycle present, else 0. Bonus: return the *cycle-start* node.
**Driver:** acyclic list (returns 0); cyclic list with `tail->next = node3` (returns 1, bonus prints 3).
**Talking points:** Why O(1) memory beats a `seen[]` set in firmware. Where this shows up in IRQ-storm detection / log replay.

## D00-E8 — `d00_struct_sizes`

Print `sizeof` for these and explain the padding:

```c
struct A { char a; int b; char c; };
struct B { int b; char a; char c; };
struct C { char a; char c; int b; };
struct D { char a; double b; char c; };
#pragma pack(push,1)
struct E { char a; int b; char c; };
#pragma pack(pop)
```

For each: predict size **before** running. Where is the padding? Natural alignment of each field on x86-64 vs ARMv8? Why does `pragma pack(1)` matter for MMIO/PCIe header overlays — and why is it dangerous on ARM (alignment trap)?

## D00-E9 — `d00_csr_model` — fake silicon

A struct with `CTRL`, `STATUS`, `DATA` registers. A `step(model)` advances internal state (e.g. `STATUS.bit0 = ready` after 3 steps). Top-level driver writes `CTRL`, polls `STATUS` via your `wait_for_bit_set`, reads `DATA`.

This is the **seed of a pre-silicon C model** you'd write before RTL exists.

## D00-E10 — `d00_csr_init` — register init table

```c
struct csr_init { uintptr_t off; uint32_t val; uint32_t mask; };
void apply_csr_init(volatile uint8_t *base, const struct csr_init *tbl, size_t n);
```

`mask == 0xFFFFFFFF` → full write. Otherwise → RMW with that mask. Print every applied step. **Why this matters:** every SoC bring-up has thousands of these.

## D00-E11 — `d00_bench` — microbenchmark scaffold

```c
uint64_t now_ns(void);             // CLOCK_MONOTONIC
double   bench(void (*fn)(void), int iters);
```

Time `d00_my_memcpy` vs glibc `memcpy` on 4 KiB / 64 KiB / 4 MiB. Predict where L1/L2/LLC cliffs are on your laptop. Reused on Day 4.

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

## D01-E3 — Architect Drills (write your answers)

**D1.** Why bother with `reg_read32`/`reg_write32` accessors when a `volatile uint32_t *` cast compiles to the same instruction? Reason from `-O3`, barrier-injection points, traceability hooks, test doubles.
**D2.** Why expose W1C status bits at all instead of plain RW? Reason about the lost-event race; when is W1S preferred?
**D3.** Polling vs interrupts — under what residency/wake distribution does polling win? Derive the NVMe completion-queue crossover.
**D4.** `volatile` is necessary but not sufficient for MMIO. Why doesn't C make `volatile` imply a full barrier?
**D5.** RMW on a control register isn't atomic across cores. Why doesn't HW provide an `atomic_field_write` MMIO opcode? Compare to coherent CXL.cache.
**D6.** Derive the right `wait_for_bit_set` timeout for `CSTS.RDY` from first principles. Why is "1 second" wrong?
**D7.** The same `read/wait/write` pattern recurs in NVMe, PCIe, clocking, bare-metal. What would force a future SoC to abandon it?

## D01-E4 — Bug Anchor

NVMe `CSTS.RDY` never asserts during bring-up — host wrote `AQA` with the wrong field width. How does the timeout in `wait_for_bit_set` surface this? What logs would you add? What's the symptom if the timeout were missing? Cross-link: PCU/ADR opcode-ordering bug (Day 14) — `WPQ_FLUSH` issued before `IO_QUIESCE_ACK` is the same shape as forgetting to wait for a status bit.

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

## D02-E3 — Architect Drills

**D1.** Why is alignment a HW requirement at all — why don't CPUs just rotate-and-mask any byte boundary? Why did ARMv7 trap on unaligned and ARMv8 mostly allow it?
**D2.** Why does a 60-byte DMA buffer at a 16-byte-aligned address corrupt silently while a 64-byte buffer at the same address is fine? Walk the cache-line straddle timing.
**D3.** Struct field reordering can save 33% of memory. Why doesn't C reorder automatically? Why does Rust?
**D4.** `container_of` is the foundation of Linux callbacks. What would force a future kernel to drop it?
**D5.** Bare-metal firmware bans `malloc`. Construct the workload where you'd need it back. What does Zephyr's `k_heap` cost in worst-case latency?
**D6.** `__attribute__((packed))` is a footgun on ARM. What architectural choice would make it safe — at what cost?
**D7.** Five segments (text/data/bss/heap/stack) is textbook. On a 64 KB-SRAM MCU with no MMU, does segmentation still earn its keep?

## D02-E4 — Bug Anchor

A 60-byte DMA descriptor at offset 56 in a 64-byte cache line. Device fetches it; second half arrives stale. What tool surfaces the straddle? What allocator change fixes it? Cross-link: Day 4 (cache lines), Day 8 (DMA visibility). Also: `*regs = defaults;` (struct overlay onto MMIO) silently clears W1C status bits — why?

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

## D03-E3 — Architect Drills

**D1.** Why is endianness a thing at all — why didn't industry standardise on one? What concrete things break if you forced one today?
**D2.** PCIe is LE on the wire even though PCI was endian-neutral. Who paid the cost and why?
**D3.** Why doesn't the compiler insert a memory barrier automatically around every `volatile` access on weak-ordered targets?
**D4.** Polling-loop timeouts are universal. Derive when polling beats interrupts for a given event arrival rate.
**D5.** W1C exists because of an HW/SW race. Why isn't read-to-clear used everywhere instead?
**D6.** Struct-overlay over an MMIO block is forbidden but ergonomic. What would the language need to add to make it safe?
**D7.** Imagine MMIO is replaced by CXL.io with full request/response semantics. Which Day-3 disciplines stay load-bearing?

## D03-E4 — Bug Anchor

Polling loop spins forever at `-O2` but works at `-O0`. What's missing, why does `-O0` mask it, what compiler pass is responsible, how does `objdump -d` confirm? Also: a `*regs = defaults;` bulk struct write silently clears every set W1C status bit — explain the read-side-effect.

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

## D04-E3 — Architect Drills

**D1.** Why a multi-level hierarchy (L1/L2/L3) and not one large cache? What forces an L4?
**D2.** Why is the line 64B — what would force 32B or 128B?
**D3.** Why an associativity ladder (L1 8-way, L3 16-way) and not all fully associative?
**D4.** Inclusive vs exclusive vs NINE — what workload makes each right? Why do AMD and Intel differ?
**D5.** Imagine DRAM latency dropped 10× tomorrow. Does the hierarchy still earn its area?
**D6.** Caches are SW-transparent, TLBs need explicit invalidation. Why the asymmetry?
**D7.** Construct a workload where write-through + no-write-allocate beats write-back + write-allocate.

## D04-E4 — Bug Anchor

Device DMA reads a stale descriptor. CPU wrote it; line still M in L1; non-coherent device fetch sees stale DRAM. How is this caught? Required fix sequence (alignment + clean + barrier + doorbell)? How does this differ from a "cache miss" bug? Also: bandwidth collapse on stride > line size; conflict-miss thrash on power-of-2 strides in low-associativity caches.

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

## D05-E3 — Architect Drills

**D1.** Why MESI specifically — why not just M and I? Construct a workload where M+I alone catastrophically loses.
**D2.** Why did AMD add O (MOESI) and Intel add F (MESIF)? Workload that forces each.
**D3.** Snoop vs directory coherence. Derive the core count where each wins.
**D4.** If line size dropped to 16B, what bug classes disappear and which appear?
**D5.** Coherence is "free" up to snoop-fabric saturation. At 256 cores, what changes?
**D6.** Padded per-thread counters waste DRAM. Construct a counter that's both compact and contention-free.
**D7.** CXL.cache makes accelerators peers in coherence. Which Day-5 bug classes follow the accelerator?

## D05-E4 — Bug Anchor

Multi-thread per-CPU counter array is 5–10× slower than single-thread. `perf c2c` shows HITM on the array address. How to confirm false sharing, what to fix, why `__attribute__((aligned(64)))` on each counter is necessary but not sufficient (struct itself must also be 64-B aligned). Cross-link Day 8: stale device descriptor read = same MESI picture with the device as the second agent.

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

## D06-E3 — Architect Drills

**D1.** Why a 4-level page table on x86-64 — not a flat array? What forced the 5-level addition?
**D2.** Why are minor page faults charged to the OS, not HW? What would a HW-resolves-minor-faults SoC look like?
**D3.** Why is the page offset never translated? What design freedom does that lock in?
**D4.** MMIO PTEs need PWT/PCD set. Why doesn't the CPU detect device ranges automatically?
**D5.** TLB shootdown is an IPI storm. Construct workload where it dominates wall time. ARM has HW-broadcast TLBI; x86 doesn't — why?
**D6.** If DRAM becomes byte-addressable persistent, does the page-fault model survive?
**D7.** Page fault vs TLB miss vs cache miss — derive a heuristic to tell them apart from a perf trace.

## D06-E4 — Bug Anchor

After a refactor, ML inference runs slower. ETL/WPA shows `MinorFault` spikes correlated with the new code. This is virtual memory (demand-zero faults at 4KB), not cache-miss. What single counter ratio decides the layer? Fix: persist context buffers, pre-touch, mlock. Cross-link Day 8: MMIO accidentally cached when PTE PWT/PCD wrong — symptom is "stuck" status reads.

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

## D07-E3 — Architect Drills

**D1.** Why hasn't anyone shipped a TLB with 100K entries?
**D2.** Huge pages are the biggest perf lever, yet THP is off by default in many distros — why?
**D3.** Why does the device need a separate IOMMU when the host has an MMU?
**D4.** SMMU/VT-d/AMD-Vi solved the same problem differently. Pick two; explain why each landed where it did.
**D5.** ARM hardware-broadcast TLBI lands on x86 — what changes in kernel design? What new bug class appears?
**D6.** Construct the triage path that identifies the QNN/ORT regression as VM rather than TLB. What single counter ratio decides?
**D7.** With CXL.mem + ATS, accelerators share CPU page tables. Which Day-7 artefacts survive?

## D07-E4 — Bug Anchor

First DMA after driver init triggers an IOMMU fault. Fault register shows BDF + IOVA + "no entry". Walk the tree to confirm the missing `dma_map_*` call. Second: device sends stale IOVA after `dma_unmap_*` — why? How to fix the queue-drain ordering?

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

## D08-E3 — Architect Drills

**D1.** Why doorbells at all — why not stream descriptors over a memory-mapped FIFO?
**D2.** Why does ARM expose weak ordering when x86 TSO "just works"? What would break if ARM moved to TSO?
**D3.** Why per-device IOMMU translation when the CPU already has an MMU?
**D4.** Why are MMIO writes posted by default? When do you actually want non-posted?
**D5.** Three ARM barrier scopes (`ish`/`osh`/`sy`). Why three? Cost of always using `sy`?
**D6.** With fully-coherent accelerators, which driver-stack artefacts become obsolete? Which survive?
**D7.** Why is the descriptor → barrier → doorbell → DMA → completion → MSI pipeline 6 stages, not 4?

## D08-E4 — Bug Anchor

Driver works on x86 but hangs on ARM under sustained load. At low rate fine; at line rate completions stall and descriptors look corrupted. What's the missing barrier, what scope, where in the submit path? Also: status reads return the same value forever — PTE attributes? `MAIR`? `ioremap` flag?

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

## D09-E3 — Architect Drills

**D1.** Why does ARM expose four exception levels when x86 collapsed to two practical rings?
**D2.** Why does Snapdragon need RPMh on top of PSCI when x86 has PCU alone?
**D3.** ARM is weak, x86 is TSO. If ARM moved to TSO tomorrow, who wins and who loses?
**D4.** PSCI is an OS↔firmware spec. What does it actually save?
**D5.** Snapdragon boots PBL → SBL → ATF → ABL → kernel with sig check at every handoff. Why so many stages? What does merging SBL+ATF break?
**D6.** Per-core voltage on DynamIQ is mostly a lie because cores share a rail. What architectural choice makes it real?
**D7.** GIC routes SGI/PPI/SPI/LPI; ITS handles MSI. Why was MSI retrofitted via the ITS rather than as a fifth class?

## D09-E4 — Bug Anchor

Driver passes on x86, hangs on Snapdragon. Missing `DMB ST` between descriptor write and doorbell — why does it manifest only on ARM? What scope barrier is correct? Cold boot fails at -20 °C, works at room temp — what fails? (LPDDR retraining across PVT.)

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

## D10-E3 — Architect Drills

**D1.** Why three layers (PHY/DLL/TL) and not two or four?
**D2.** Why did PCIe move from 8b/10b → 128b/130b at Gen3 instead of 256b/258b?
**D3.** Posted writes are fire-and-forget; reads block. Why doorbell-as-MWr won.
**D4.** LTSSM has 11 states. Why so many — could it be 4? Which are vestigial in 2025?
**D5.** PCIe trains at Gen1 first, then negotiates up. Why doesn't any spec start at the highest rate?
**D6.** Flow-control credits — why aren't they implicit?
**D7.** With CXL.cache, which LTSSM/TLP artefacts survive? What new failure mode appears?

## D10-E4 — Bug Anchor

Link stuck in Polling. Refclk, receiver termination, lane polarity. Why is Polling almost always physical/electrical? Other anchors: width downgrade (x16 trains as x4) → faulty lanes; frequent Recovery → SI/EQ Tx coefficients; link up but no traffic → flow-control credit return broken; `0xFFFFFFFF` Vendor ID after PM transition → link did not survive.

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

## D11-E3 — Architect Drills

**D1.** Which design choice (per-CPU queues / MSI-X-per-queue / 64B commands / posted doorbells / no SATA TL) mattered most to NVMe scaling? Defend the ranking.
**D2.** Why must the host disable `CC.EN=0` before writing AQA/ASQ/ACQ?
**D3.** Phase bit on completions saves a doorbell. Construct the workload where it loses.
**D4.** PRPs vs SGLs — why did NVMe start with PRPs and add SGLs later?
**D5.** Derive the `CSTS.RDY` standard suspect list from the bring-up state machine alone.
**D6.** With memory-semantic SSD over CXL.mem, which NVMe artefacts survive?
**D7.** Per-CPU queue is the scaling unlock. What limits it as core counts hit 1024?

## D11-E4 — Bug Anchor

`CSTS.RDY` never asserts after `CC.EN=1`. Check `CSTS.CFS` for fatal; verify nothing wrote AQA/ASQ/ACQ while controller was still enabled; recompute `DSTRD` from `CAP[35:32]` (not `CAP[3:0]`); inspect IOMMU event queue for first SQE fetch; verify `AQA` encodes `size-1` not size. Cross-link: NVMe descriptor + doorbell follows Day 8 pattern; PCIe link state + AER first check (Day 10); IOMMU mapping for PRPs (Day 7).

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

## D12-E3 — Architect Drills

**D1.** Derive $P_{dyn} = \tfrac{1}{2}CV^2F$ from first principles. Where does it break (NTV, leakage-dominated)?
**D2.** With per-IP V/F rails, does V-before-F still apply? Construct workload where it reverses.
**D3.** Why discrete P-states vs continuous? What does Apple's near-continuous OPP cost vs Intel's ladder?
**D4.** Speed binning sorts post-fab silicon. With perfect process control, what would the industry redesign?
**D5.** Leakage = 30–40% at 7nm. What's pushing toward 50%? Which architectural choices are responses?
**D6.** DynamIQ shares a cluster rail. Construct workload where this loses *more* than it saves.
**D7.** Aging-related Vth shift over 2 years. What does a self-tuning DVFS table look like? What stops shipping today?

## D12-E4 — Bug Anchor

Intermittent data corruption when OS jumps from P3 to P0. V-F transition reversed (raised F before V) → setup-timing violation. Where in microcode/firmware does sequence enforcement live? Other anchors: turbo never engages on single-thread (DTS calibration); throttling under sustained load (PROCHOT, fan curve); cold-boot hang at SS corner (DVFS bin margin); per-core DVFS appears wasted on Snapdragon (cluster rail = max).

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

## D13-E3 — Architect Drills

**D1.** Why a discrete C-state ladder vs a continuous knob? What would a 5th rung between C6 and C10 buy?
**D2.** Why does PkgC6 require all uncore IPs idle? Shared-rail + snoop-quiesce reasoning.
**D3.** Why retention SRAM in C6? When does the trade flip?
**D4.** S0ix needs PCIe L1.2. What architectural change would let a platform reach S0ix without L1.2?
**D5.** Derive break-even residency. What is the ping-pong failure mode?
**D6.** If wake latency dropped to ns, what changes about the ladder?
**D7.** PkgC6 residency = 0 on a quiet system. Where do you look *before* reading any blocking-reason MSR?

## D13-E4 — Bug Anchor

PkgC6 never enters even at full system idle. PCIe endpoint missing L1.2 in L1 PM Substates; or ASPM disabled in BIOS; or LTR pinning the link awake. Where in `lspci -vvv` and PCU blocking-reason MSR do you confirm? Cross-link Day 14: Core C6 wake-vector lost (CFC reset clear race); ADR data loss (`WPQ_FLUSH` before `IO_QUIESCE_ACK`).

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

## D14-E3 — Architect Drills

**D1.** Why is the ADR hold-up budget 10–30 ms — what physics sets it?
**D2.** Why is the ADR opcode order exactly `IO_QUIESCE → WPQ_FLUSH → SR_ENTRY → NVDIMM_SAVE`? Walk each pair-swap failure mode.
**D3.** Why isn't the PCU opcode table burned into hardware?
**D4.** Why is the always-on / CFC FIFO crossing the bug-prone domain in C6? What does a 2-FF synchroniser fix and what does it not?
**D5.** Construct the workload where eADR is necessary, not luxury. What does CXL.mem persistence change?
**D6.** Why doesn't the PCU just force-quiesce uncore IPs instead of waiting?
**D7.** "Works in unit-sim, fails in full-die emulation" — what's the cheapest pre-Si hook that finds 80% of these?

## D14-E4 — Bug Anchor

**Story 1 — ADR opcode-ordering hang (Cambria):** PSU loss simulated, PLI asserted, PCU FW started its sequence, DDR never reached self-refresh. Hypothesis (IMC issue) → evidence (Zebu PCU opcode log) → root cause (`WPQ_FLUSH` before `IO_QUIESCE_ACK`) → fix (explicit wait-for-ack, Perspec assertion) → lesson (firmware sequences need explicit handshakes).

**Story 2 — Core C6 wake lost on back-to-back ARAT (Cambria):** sporadic 50–500 ms hangs; perfmon shows C6 residency past expected exit. Hypothesis (timer/IRQ delivery) → evidence (Lauterbach trigger on `wake_in==1 && cfc_init_busy==1`) → root cause (two-clock-domain race during CFC FIFO mid-reset) → fix (hold wake latch until `init_done && drain_done`) → lesson (always-on → power-gated handoff must hold intent until sink clock + reset stable).

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

## D15-E3 — Architect Drills

**D1.** Why is $T_j = T_a + P\theta_{JA}$ useful when the die is non-uniform?
**D2.** Why isn't per-IP thermal throttling more aggressive? What stops a future SoC from per-tile DVFS?
**D3.** RAPL underestimates wall by 20–30%. Construct the cause stack; which gap is fixable?
**D4.** Why exactly 4 top-down buckets (not 2 or 8)?
**D5.** HW prefetcher can hide a missing SW prefetch. Why is disabling HW PF triage, not a fix?
**D6.** AVX-512 license-level cuts F. Construct workload where AVX-512 loses to AVX-256.
**D7.** Five multi-IP interference classes (BW, LLC, NoC, power, IOTLB). Build the diagnostic decision tree.

## D15-E4 — Bug Anchor

Throttle at Tj=85°C with spec Tjmax=100°C. DTS calibration fuses missing → sensor reads 15°C high → external thermocouple confirms. Other anchors: throttle without thermal cause (RAPL PL1/PL2 too aggressive in BIOS — `MSR_CORE_PERF_LIMIT_REASONS`); IPC unchanged but ref cycles ≫ core cycles (frequency dropped — throttle reason MSR); LLC thrash from GPU streaming DMA (CAT/CLOS); NPU EP fallback dominating inference (count fallbacks); RAPL=45W vs wall=65W is not a bug (VR loss + DRAM + platform + PSU).

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

## D16-E3 — Architect Drills

**D1.** Why do deep OoO pipelines beat in-order at the same clock? When does in-order win?
**D2.** x86 needs a μop cache; ARM doesn't. Why? What does RISC-V compressed mode threaten to bring back?
**D3.** TSO vs weak. If ARM moved to TSO tomorrow, who wins/loses? Why didn't ARM v8 make the change?
**D4.** Branch mispredict cost ≈ depth × IPC. Where does a perfect predictor still lose to branchless?
**D5.** SIMD width doubling slowed at 512. Why? What comes after?
**D6.** Why is the producer-consumer ring race the canonical TSO-vs-weak bug? Construct the trace and the minimal fix.
**D7.** If a future ISA mandated seq_cst as default, who wins, what dies?

## D16-E4 — Bug Anchor

Producer-consumer ring on ARM: consumer occasionally sees `ready=1` with stale payload. 24-hour x86 stress passes; ARM dev-kit fails in minutes. Missing `dmb st` between data store and flag store on producer; consumer needs `dmb ld` (or use `STLR/LDAR`). Other anchors: branchy hot loop (`BR_MISP_RETIRED.ANY`); AVX-512 net loss (`CORE_POWER.LVL2_TURBO_LICENSE`); HW PF hides bug; I-cache thrash from huge code; Spectre-class side channels.

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

## D17-E3 — Architect Drills

**D1.** "Pick the weakest correct memory order." What's wrong with always using seq_cst?
**D2.** SPSC is lock-free; MPSC needs CAS. Why does one writer change the algorithm class?
**D3.** Why doesn't HW provide a wider native CAS that bakes in a counter?
**D4.** Construct (if you can) a UP workload where spinlocks beat mutexes.
**D5.** Construct an SPSC ring contention-free without padding.
**D6.** Why do beginners think `volatile` is a synchronisation primitive? What language change would make it sufficient?
**D7.** Construct a workload requiring wait-free, not just lock-free.

## D17-E4 — Bug Anchor

Two threads bump a shared counter without atomics; final value < expected. Fix: `_Atomic` + `atomic_fetch_add(memory_order_relaxed)`. Other anchors: CAS spin without test-first → cache-line ping-pong; SPSC `head`/`tail` on same cache line → false sharing; ABA on lock-free pop; `volatile` mistaken for atomic; spinlock held across a sleeping call → deadlock; lock-acquisition order inversion → deadlock.

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

## D18-E3 — Architect Drills

**D1.** Why six layers? What collapses if Framework→silicon directly?
**D2.** Why doesn't ORT refuse to run if any op falls back? What would a strict mode look like?
**D3.** Why doesn't the framework emit fused INT8 directly? What does ONNX QOperator buy?
**D4.** Why is VTCM software-managed (not coherent)? When CXL.cache makes coherent NPUs viable, what dies first?
**D5.** Tile-size cliff: barely overflow → 2× DMA → 2× latency. What makes the cliff sharp?
**D6.** Where does compile cost live (IR opt vs scheduling vs codegen)? Which is amortisable?
**D7.** NPU vs GPU vs CPU for inference — which converges first?

## D18-E4 — Bug Anchor

Functional pass, perf 10× off. Count `EP_assignment == CPU` ops in the partition log; ETW shows QNN DLL loaded but CPU% high. Cause: silent EP fallback from one unsupported op. Other anchors: VTCM tile-size cliff (per-tile latency from QNN profile); quant-accuracy regression after rebuild without `keep_quantized_weights=true`; missing context cache (re-compile per launch); ORT × QNN EP × QNN SDK version-matrix regression; DLL load failure (`QnnHtpV79Stub.dll` missing → unload → silent CPU fallback).

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

## D19-E3 — Architect Drills

**D1.** "Page faults spike" is almost never silicon. What single signal *would* point at silicon?
**D2.** Should an NPU implement preemption like a GPU? Why might priority lanes from the framework be the right fix instead?
**D3.** EP fallback is silent. Construct the gate that catches it without false positives.
**D4.** Why does the runtime have a keep-alive policy at all? Who decides — runtime, framework, or OS?
**D5.** What's the smallest version matrix (EP × ORT × SDK × model build) that catches 90% of regressions? Which axis is most often the culprit?
**D6.** "Never blame silicon first." When does the rule reverse? What evidence is sufficient to escalate?
**D7.** With a coherent (CXL.cache peer) NPU, which Day-19 bug classes survive and which die? What new class appears?

## D19-E4 — Bug Anchor

**Story 1 — QNN/ORT page-fault spike.** NOT silicon. Build version matrix across EP × ORT; new EP + old ORT gives different footprint → isolates to host runtime context-loading path. Page faults alone are not the bug — correlate with virtual-alloc and peak memory.

**Story 2 — Paint SR starves Camera.** GPUView shows Camera waiting 108 ms behind SR's 36 tiles on a single non-preemptive HTP queue. Fix: framework priority lanes.

**Story 3 — Silent EP fallback.** Rebuilt without `keep_quantized_weights=true` → QDQ doesn't fuse → ops drop to CPU. Detect: EP-assignment log + INT8 op count.

**Story 4 — Version-matrix regression.** ORT × QNN EP × QNN SDK forms a 3D space; "good combo" identified by the matrix.

**Story 5 — NPU blocks S0ix.** Runtime keep-alive policy holds device active; PkgC10 residency tanks.

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

## D20-E3 — Architect Drills

**D1.** Which step of the 7-step framework is most often skipped, and what failure does each skip predict?
**D2.** How does repro rate determine triage tooling? Why does "intermittent" cost the most engineering hours per bug?
**D3.** Cost rises 10× per stage. Where can future methodology (formal, AI-assisted) flatten the curve?
**D4.** Why "lead with the symptom" in the 90-sec story? Construct symptom-first vs story-first to show the cost.
**D5.** Why does honest framing ("I don't debug Hexagon RTL") win? Construct the question chain that exposes a bluff.
**D6.** What makes a signal "controlled" vs an anecdote? Why does the difference predict whether a fix holds?
**D7.** With 100× better debug tooling, which steps of the framework still survive? What's the irreducible human work?

## D20-E4 — Bug Anchor (the 90-second story shape)

Practice each in ≤ 90 sec leading with the symptom (full 7-step versions in [day20_debug_method_bug_polish.md](../day20_debug_method_bug_polish.md) §7-§11b):

- **Story 1 — ADR opcode-ordering hang (Day 14).** Symptom → Zebu opcode log → root cause (`WPQ_FLUSH` before `IO_QUIESCE_ACK`) → fix → lesson.
- **Story 2 — NVMe `CSTS.RDY` never asserts (Day 11).** Symptom → CSR snapshot → root cause (AQA off-by-one) → fix → lesson.
- **Story 3 — Core C6 wake-loss CFC race (Day 14).** Symptom → Lauterbach trigger → root cause (FIFO mid-reset on cross-clock-domain wake) → fix → lesson.
- **Story 4 — QNN/ORT page-fault spike (Day 19).** Symptom → version matrix → root cause (host runtime context-loading regression) → fix → lesson ("page faults alone are not the bug").
- **Story 4b — Paint SR vs Camera (Day 19).** Symptom → GPUView 108 ms wait → root cause (single non-preemptive HTP queue) → fix → lesson.
- **Story 6 — PCIe Gen6 L0p lane-scaling (Day 20 §11a).** Link-level DVFS; transition mid-traffic → fix → lesson (PM transitions must align to a transaction boundary).
- **Story 7 — NVMe HMB silent corruption above WCTEMP (Day 20 §11b).** Thermal-throttle PM action × data path → fix → lesson (PM bugs that couple to data-path correctness are post-Si-only).

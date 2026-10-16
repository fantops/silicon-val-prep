# S08 - Memory and caches

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [D15 memory/ECC](archive/day15_memory_systems_ecc.md) (DRAM, refresh, ECC), [D16 cache lines/AMAT](archive/day16_cache_lines_locality_amat.md).
- **Must answer:**
  - DRAM refresh and **self-refresh** (deep package idle needs it).
  - Write-back caches and dirty lines. Why caches must be **flushed before power gating**.
  - Why MMIO is **uncached** while DMA buffers live in **cacheable** memory.
- **Work link:** the cache-flush and memory-move style DSA operations, and how a cache-control hint changes where written data ends up.
- **Handoff →** How does memory stay consistent when many cores and devices touch it?

## Study notes

### Concept - [day15_memory_systems_ecc.md](archive/day15_memory_systems_ecc.md)

Beneath the **cache organization** of [Day 16](archive/day16_cache_lines_locality_amat.md) (sets, ways, AMAT — the *architectural* view) sits the **physical memory array**: the **cell** that stores a bit (6T SRAM, 1T1C DRAM, ROM, NAND/NOR flash), the **address decode** that selects *which* cell (row/column decoders → word/bit lines → sense amps), the **organization** (banks/rows/columns), the **memory controller**, and **ECC** (parity, Hamming SECDED). The goal: answer memory questions from the cell up — **address vs data**, read/write behaviour, SRAM internals, address mapping.

### Deep Study Notes - [day15_memory_systems_ecc.md](archive/day15_memory_systems_ecc.md)

### 1. Address vs data — the framing to lead with

> **The address selects *where*; the data is *what*.** They are different buses with different widths and different decode.

```
        ADDRESS bus (N bits)            DATA bus (W bits)
   ┌──────────────────────┐         ┌──────────────────────┐
   │ picks one of 2^N      │         │ the W-bit value read │
   │ locations  ──────────►│  array  │◄──── or written      │
   └──────────────────────┘         └──────────────────────┘
   N address bits  →  2^N locations  ;  each location holds W bits
```

A memory is `2^N × W`: `N` address bits choose a location, `W` data bits flow in/out. **Byte-addressable** vs **word-addressable** changes what "a location" means (and the low address bits become a byte offset). Saying this distinction first — *"address decodes to a location, data is the payload"* — is exactly the clarity the Broadcom memory question wanted.

### 2. Memory cell types — the comparison that anchors everything

| Type | Cell | Volatile? | Density | Speed | Refresh | Use |
|---|---|---|---|---|---|---|
| **SRAM** | **6 transistors** (2 cross-coupled inverters + 2 access) | yes | low (big cell) | **fastest** | no | caches, register files, on-chip buffers |
| **DRAM** | **1 transistor + 1 capacitor** | yes | **high** | medium, high latency | **yes** (leaky cap) | main memory |
| **ROM / mask** | hard-wired | no | high | fast read | n/a | boot code, constants |
| **NOR flash** | floating-gate, random read | no | medium | fast **read**, slow write/erase | n/a | **code** (XIP), firmware |
| **NAND flash** | floating-gate, page/block | no | **highest** | page read, block erase | n/a | **storage** (SSD) |

**SRAM 6T** stores a bit in two cross-coupled inverters (a 1-bit bistable latch — same regenerative element as a flop, [Day 12](archive/day12_metastability_synchronizers.md)); two **access transistors** connect it to the bitlines when the **wordline** is asserted. Fast and refresh-free, but **6 transistors per bit** = expensive area → why caches are small. **DRAM 1T1C** stores charge on a capacitor; tiny (1T) so dense, but the cap **leaks** → must be **refreshed**, and reading **drains** the cap (**destructive read** → write-back). That single cell-level difference explains the whole hierarchy: SRAM near the core, DRAM as bulk.

### 3. How an address selects a cell — decode, wordline, bitline, sense

```
 row addr ─►┌──────────┐  wordline (one row hot)
            │   ROW    │═══════════════╗
            │ DECODER  │   ┌──┬──┬──┬──╫──┐
            └──────────┘   │  │  │  │ cell│   ← 2^r rows × 2^c columns
                           ├──┼──┼──┼──╫──┤
            bitlines (precharged) ║  ║  ║ ║
                           ▼  ▼  ▼  ▼  ▼
                        ┌──────────────────┐
                        │  SENSE AMPS       │  (detect tiny bitline swing)
                        └──────────────────┘
 col addr ─►┌──────────┐        │
            │   COL    │◄───────┘   selects W bits out of the row
            │  MUX     │──► DATA
            └──────────┘
```

1. **Precharge** the bitlines to a reference level.
2. **Row decoder** (a [Day 5](archive/day05_boolean_combinational_logic.md) `r→2^r` decoder) drives **one wordline** high → that row's cells tip the bitlines.
3. **Sense amplifiers** detect the tiny differential swing and amplify to full logic levels.
4. **Column mux/decoder** selects the addressed `W` bits from the row → **data out**.

**Write** is the same up to step 3, then the write drivers **force** the bitlines to the new value, overpowering the cell through the access transistors. The address is split into a **row part** and a **column part** — that split *is* the "address mapping" Broadcom asked about.

### 4. Array organization — banks, rows, columns, the address split

A real memory is `2^N × W` carved into **banks** for parallelism. The physical address decomposes:

```
 physical address = [ bank | row | column | byte-offset ]
                       │      │       │         └ which byte in the word
                       │      │       └ column within the activated row
                       │      └ which row (page) to activate
                       └ which bank (independent sub-array)
```

- **Banks** let multiple accesses proceed in parallel (activate row in bank 0 while reading bank 1) → **bank interleaving** hides latency.
- **Row buffer / page mode (DRAM):** once a row is **activated**, its whole contents sit in the sense-amp **row buffer**; consecutive accesses to the **same row** ("page hit") are fast; a different row needs **precharge + activate** ("page miss") → the `tRP + tRCD` penalty.
- This is the structural cousin of [Day 16](archive/day16_cache_lines_locality_amat.md)'s set/way address decomposition — same idea (split the address into selection fields), different layer.

### 5. SRAM read/write cycle (the "explain SRAM" answer)

```
READ : precharge bitlines → assert wordline → cell pulls one bitline
       slightly → sense amp resolves → data out.  (non-destructive)
WRITE: drive bitline pair to new value (strong drivers) → assert wordline
       → access transistors let the drivers flip the cell.  (must overpower
       the cell → write-driver strength vs cell stability is the 6T sizing trade)
```

The 6T sizing balances **read stability** (don't disturb the cell when reading) against **writeability** (be able to flip it) against **speed/area** — the classic SRAM bitcell trade. Multi-port SRAMs (register files, [Day 21](archive/day21_pipeline_microarch.md)) add wordlines/bitlines per port.

### 6. DRAM specifics — refresh and the timing parameters

- **Refresh:** each cell must be re-read-and-rewritten within its **retention time** (~64 ms typical) or it loses charge. The controller issues **refresh** commands across all rows on a schedule; refresh steals bandwidth.
- **Destructive read:** activating a row drains the caps into the sense amps, which then **write the row back** — so a read *is* a read-modify-restore.
- **Key timings:** `tRCD` (activate→read), `tCAS/CL` (read→data), `tRP` (precharge), `tRAS` (activate→precharge min). Page-hit vs page-miss is the `tRCD/tRP` story.
- **RAS/CAS:** the row address and column address are sent in two phases (Row Address Strobe, Column Address Strobe) — historically to share address pins.

### 7. The memory controller — an FSMD

The controller ([Day 9](archive/day09_fsm_sequence_detectors.md) FSM + [Day 5–7](archive/day05_boolean_combinational_logic.md) datapath) turns load/store/DMA requests into command sequences:

- **Address mapping policy** — how physical addr → bank/row/col (interleave for parallelism vs locality).
- **Page policy** — **open-page** (leave the row activated, bet on locality) vs **closed-page** (precharge after each access, bet on randomness).
- **Scheduling** — reorder reads/writes for row-buffer hits and bus turnaround; **refresh** insertion; **read/write grouping** (bus direction changes are expensive).
- Feeds the [Day 20](archive/day20_dma_doorbells_ordering_barriers.md) DMA path and sits under the [Day 16](archive/day16_cache_lines_locality_amat.md) hierarchy.

### 8. ECC — parity and Hamming SECDED

Memories flip bits (alpha particles, noise, wear). **Error-correcting codes** add redundancy:

- **Parity (1 bit):** XOR of all data bits → **detects** a single-bit error, **corrects nothing**. `parity = ^data`.
- **Hamming SECDED (Single-Error-Correct, Double-Error-Detect):**
  - Place **check bits at power-of-2 positions** (1,2,4,8…); each covers a fixed subset of data positions (by the bit of the position index it owns).
  - On read, recompute the checks → the **syndrome**. **Syndrome = 0** → clean; **syndrome = k ≠ 0** → the error is at **bit position k** → flip it (**correct**).
  - Add **one overall parity bit** over everything → distinguishes **1-bit (correctable)** from **2-bit (detectable, not correctable)**: syndrome≠0 with overall-parity error = single correctable; syndrome≠0 with overall-parity *ok* = **double error, flag uncorrectable**.
  - **Check-bit count `p`:** smallest `p` with $2^p \ge n + p + 1$ (e.g., 8 data bits → 4 check + 1 overall = 5 for SECDED).
- **Where:** register files, caches, on-chip SRAMs (inline ECC), and **DRAM** (often `+8 bits per 64` = SECDED per word). **Scrubbing** = background read-correct-rewrite to stop single errors accumulating into double errors. **ChipKill / advanced ECC** survives a whole DRAM chip failing.

ECC is a **major validation surface**: inject single-bit errors (assert correction + counter increment), inject double-bit errors (assert detection + uncorrectable flag), and check the error-logging registers — a recurring "write the test plan for ECC" interview ask ([Day 24](archive/day24_verification_planning_coverage.md)).

### 9. Address-decoding worked example (the interview drill)

```
Q: A byte-addressable memory is 64 KiB, 32-bit (4-byte) data word. Address layout?
   64 KiB = 2^16 bytes → 16 address bits total.
   4-byte word → low 2 bits = byte offset within the word.
   → [15:2] = word index (16384 words),  [1:0] = byte-in-word.

Q: Organize as 4 banks × 4096 words. Split:
   bank = addr[15:14] (4 banks),  word-in-bank = addr[13:2],  byte = addr[1:0].
```

Be fluent moving between **size ↔ address bits ↔ field split** — that's the core memory-mapping skill.

### 10. Memory as a verification surface — MBIST + march tests

On-chip memories are tested with **MBIST** (Memory BIST: an on-chip engine writes/reads patterns) using **march algorithms** that catch the standard fault models:

| Fault | What it is | March catches via |
|---|---|---|
| **Stuck-at** | cell stuck 0/1 | write/read complementary values |
| **Transition** | cell can't 0→1 or 1→0 | up and down marches |
| **Coupling** | writing cell A flips cell B | ascending+descending address order |
| **Address-decoder** | wrong cell selected | unique data per address, read back |

`March C-` (a standard sequence of up/down read-write passes) covers stuck-at, transition, coupling, and decoder faults — a good name to drop. ECC injection layers on top.

---

### Bugs in this Topic - [day15_memory_systems_ecc.md](archive/day15_memory_systems_ecc.md)

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Address-decoder fault** | read/write hits wrong cell | decoder/mux miswire | unique-data march test |
| **Stuck-at / coupling cell** | bit always 0/1 or flips with neighbour | cell/process defect | MBIST march + repair (redundant rows) |
| **Refresh starvation** | DRAM data decays | controller starves refresh under load | guarantee refresh scheduling |
| **ECC double-bit miscorrect** | silent data corruption | SEC-only code "corrects" a 2-bit error wrongly | SECDED + flag uncorrectable; scrub |
| **Byte-enable/write-mask bug** | partial-word write corrupts neighbours | wrong column/byte mask | verify byte-enables per lane |
| **Off-by-one address map** | first/last location wrong | field-split math error | re-derive size↔bits↔fields |

---

### Interview Explanation (60–90 sec) - [day15_memory_systems_ecc.md](archive/day15_memory_systems_ecc.md)

> "I start by separating address from data: the address bus decodes to *which* location, the data bus is *what's* stored there — a memory is two-to-the-N locations of W bits each. The cell sets the character: SRAM is six transistors, two cross-coupled inverters holding the bit plus two access transistors — fast, no refresh, but big, so it's caches and register files; DRAM is one transistor and a capacitor — tiny and dense, but the cap leaks so it needs refresh, and reading drains it so a read is destructive and writes back. Addressing works by precharging the bitlines, the row decoder driving one wordline so that row's cells tip the bitlines, sense amps amplifying the tiny swing, and a column mux selecting the addressed bits; write just forces the bitlines hard enough to flip the cell. Real arrays split the address into bank, row, and column, with banks giving parallelism and the activated row sitting in a row buffer so same-row accesses are page hits. For reliability I add ECC — parity detects one bit; Hamming SECDED puts check bits at power-of-two positions so the syndrome points at the failing bit to correct it, plus an overall parity bit to flag uncorrectable double errors — and I scrub in the background so single errors don't pile up into doubles. Memories are also the biggest test surface, so I'd run MBIST march patterns for stuck-at, coupling, and decoder faults and inject single and double-bit errors to prove the ECC."

---

### Architect Drills (derive, no answer key) - [day15_memory_systems_ecc.md](archive/day15_memory_systems_ecc.md)

1. Draw the 6T SRAM cell and walk a read then a write. Where's the read-stability vs writeability trade?
2. Why is DRAM read destructive and SRAM read not? What does each imply for the controller?
3. A 1 MiB, 64-bit-word, byte-addressable memory in 8 banks: give the full `[bank|row|col|byte]` address split.
4. Hamming-encode the 4-bit value `1011` (place check bits, compute them). Corrupt one bit; show the syndrome locating it. Corrupt two; show SECDED flags uncorrectable.
5. Compute SECDED check bits needed for a 64-bit word. Why `2^p ≥ n+p+1`?
6. Open-page vs closed-page policy: which wins for streaming, which for random? Tie to row-buffer hit/miss.
7. Write the ECC test plan: what do you inject and what does each test assert? (Hand off to [Day 24](archive/day24_verification_planning_coverage.md).)

---

### Exit Check - [day15_memory_systems_ecc.md](archive/day15_memory_systems_ecc.md)

- [ ] State "address vs data" in one sentence and size a `2^N × W` memory both ways.
- [ ] Draw the 6T SRAM and 1T1C DRAM cells; explain volatile/refresh/destructive-read differences.
- [ ] Walk address decode: precharge → row decoder → wordline → sense amp → column mux.
- [ ] Split a physical address into `[bank|row|col|byte]` for a given geometry.
- [ ] Hamming SECDED: place check bits, compute the syndrome, correct 1 / detect 2.
- [ ] Explain open vs closed page and row-buffer hit/miss.
- [ ] Name the march-test fault models and the ECC injection tests.

### Concept - [day16_cache_lines_locality_amat.md](archive/day16_cache_lines_locality_amat.md)

Caches exist because DRAM is hundreds of cycles away. Data moves in fixed-size **cache lines** (64 B on x86/ARM); an address splits into **tag / set / offset**; **associativity** controls how many lines can co-exist in one set; **write-back + write-allocate** is the dominant policy; AMAT quantifies all of it. Today is the foundation of every later "device read stale data" / "false sharing" / "DMA visibility" bug.

### Deep Study Notes - [day16_cache_lines_locality_amat.md](archive/day16_cache_lines_locality_amat.md)

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
- **DRAM self-refresh entry/exit** — entering self-refresh saves power but exit latency is microseconds; PkgC6 / S0ix entry has to coordinate this with the memory controller (Day 27).
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
- Saves bandwidth; adds the "WPQ must drain before power loss" problem (Day 28 ADR).

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

Coherence misses are the bridge to Day 17.

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

You will revisit this on Day 20.

### 11. What this is **not** — vocabulary discipline

Don't conflate layers (interviewers catch this):

| Layer | Granularity | Managed by |
|---|---|---|
| CPU cache | 64 B line | hardware (locality, coherence) |
| Virtual memory | 4 KB page | OS / MMU (translation, faults) — Day 18 |
| IOMMU/SMMU | IO page | OS / device map — Day 19 |
| VTCM | tile / buffer | NPU compiler/runtime — Day 33 |

The Day 34 QNN/ORT page-fault story is a *page-table* event (4 KB, OS-managed), **not** a cache-line miss. VTCM in Hexagon HTP is a *software-managed* local store, **not** a CPU cache.

---

### Bugs in this Topic - [day16_cache_lines_locality_amat.md](archive/day16_cache_lines_locality_amat.md)

- **Device reads stale descriptor:** descriptor still in CPU's dirty L1; missing clean/flush before doorbell on a non-coherent DMA path. Picture: CPU's M state, device DMA reads DRAM. Fix: cache-line-aligned descriptor + clean + barrier + doorbell.
- **Strided loop bandwidth collapse:** stride > line size wastes 60/64 bytes per fetch and pulls the whole working set through DRAM. Symptom: the same loop is 5-10× slower with `i += 1024` than `i += 1`.
- **Conflict-miss thrash on power-of-2 stride:** two hot arrays separated by a multiple of (set count × line size) constantly evict each other in a low-associativity cache. Fix: pad arrays by one line, or use a higher-associativity SKU.
- **W1C-style mistake on cache maintenance:** invalidating a dirty line without a clean first **discards** the writes silently. Always clean-then-invalidate (or use a single "clean+invalidate" op) on dirty data.

---

### Interview Explanation (60-90 sec) - [day16_cache_lines_locality_amat.md](archive/day16_cache_lines_locality_amat.md)

> "Caches exist because DRAM is hundreds of cycles away. Data moves in 64-byte cache lines, so reading one byte fetches the neighbours — that's the spatial-locality lever. An address splits into tag, set, and offset; associativity controls how many lines can co-exist in one set. Almost all CPU L1/L2/L3 are write-back with write-allocate, which is why dirty lines exist and why ADR has to drain the IMC write-pending queue before rails collapse. AMAT — `hit + miss_rate × penalty` chained per level — is the one formula I use to predict the impact of a cache change. The system-validation use of cache lines is DMA: a descriptor in a dirty cache line that hasn't been cleaned will be read stale by the device, so I always cache-line-align DMA buffers and put a clean+barrier between the descriptor write and the doorbell. The vocabulary trap I avoid is calling page-fault regressions cache-miss problems — they're at different layers."

---

### Architect Drills - [day16_cache_lines_locality_amat.md](archive/day16_cache_lines_locality_amat.md)

*Reason from constraints. Don't recall a textbook answer — derive why the design has to look this way given physics, area, power, and workload. No answer key. If you can't derive it in 5 min, that's the layer to re-read.*

**D1. Why a multi-level hierarchy (L1/L2/L3) and not one large cache?**
*Constraints to weigh:* SRAM access time scales with $\sqrt{\text{capacity}}$; a 32 MB single-level cache would have L3-class latency on every hit. Per-port area cost. Hit-rate vs latency curve. What does the AMAT formula say if you collapse the levels? What would force a future SoC to add an L4?

**D2. Why is the line 64 B — and what would force a future SoC to 32 B or 128 B?**
*Constraints to weigh:* spatial locality vs false-sharing penalty vs DRAM burst length vs tag-array overhead vs interconnect width. If DDR burst doubles, does the line follow? If cores go to 1024 and shared-data working sets shrink, which way does it move?

**D3. Why an associativity ladder (L1 8-way, L3 16-way) and not all fully associative?**
*Constraints to weigh:* parallel tag comparator cost (energy + area) vs conflict-miss rate vs hit-time on critical path. Why can a TLB get away with FA but L3 cannot? What's the cost curve as ways double?

**D16. Inclusive vs exclusive vs non-inclusive-non-exclusive — what workload makes each the right call?**
*Constraints to weigh:* snoop-filter cost (inclusive lets L3 act as a directory), effective capacity (exclusive sums), back-invalidation traffic, multi-socket coherence. What does AMD vs Intel do, and why are they different?

**D17. Imagine DRAM latency dropped 10× tomorrow. Does the cache hierarchy still earn its area?**
*Constraints to weigh:* AMAT recomputation, energy-per-access (cache hit ≪ DRAM access even at parity latency), bandwidth wall, what "hit" means if DRAM is as fast as L3. Where does the hierarchy collapse, and where does it survive?

**D18. Caches are transparent to software but TLBs need explicit invalidation. Why the asymmetry?**
*Constraints to weigh:* coherence protocol scope (caches snoop, TLBs don't), what the "truth" being cached is (data lives in DRAM and is owned by the coherence domain; translations live in page tables owned by the OS), cost of broadcasting page-table writes. What would a coherent-TLB system look like, and why don't we have one?

**D19. Write-back + write-allocate is the default pairing. Construct a workload where write-through + no-write-allocate wins.**
*Constraints to weigh:* streaming stores that won't be re-read, observation/debug buffers, coherence with non-snooping agents, DRAM write bandwidth headroom. Why does the GPU/NPU world use streaming-store hints?

---

### Exit Check - [day16_cache_lines_locality_amat.md](archive/day16_cache_lines_locality_amat.md)

- [ ] Drew the cache hierarchy with latencies from memory.
- [ ] Decoded an address into tag/set/offset for a given config.
- [ ] Stated AMAT and computed it for a worked example.
- [ ] Stated write-back + write-allocate pairing and the policy reason.
- [ ] Explained the descriptor-then-doorbell DMA pattern.
- [ ] Distinguished cache miss vs TLB miss vs page fault in 30 seconds.
- [ ] Stated the four C's of misses.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S07](s07_registers_mmio.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S09](s09_coherence_ordering.md)

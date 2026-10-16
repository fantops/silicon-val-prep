# Day 15 — Memory Systems: SRAM / DRAM / ROM / Flash + ECC

## Concept

Beneath the **cache organization** of [Day 16](day16_cache_lines_locality_amat.md) (sets, ways, AMAT — the *architectural* view) sits the **physical memory array**: the **cell** that stores a bit (6T SRAM, 1T1C DRAM, ROM, NAND/NOR flash), the **address decode** that selects *which* cell (row/column decoders → word/bit lines → sense amps), the **organization** (banks/rows/columns), the **memory controller**, and **ECC** (parity, Hamming SECDED). The goal: answer memory questions from the cell up — **address vs data**, read/write behaviour, SRAM internals, address mapping.

## Why this day in the flow

This is the *structural* memory floor — the cell and array level — that the *architectural* cache view ([Day 16](day16_cache_lines_locality_amat.md): hierarchy, hit/miss, set/way math) sits directly on top of, which is why it comes first. Broadcom R2 probed exactly this level: "address mapping, memory organization, read/write behaviour, address vs data, SRAM." It reuses the Phase 1 toolkit: **address decode is a [Day 5](day05_boolean_combinational_logic.md) decoder**, the **memory controller is a [Day 9](day09_fsm_sequence_detectors.md) FSM**, **sense/precharge timing is [Day 7](day07_sequential_logic.md)**, and **ECC is a [Day 5](day05_boolean_combinational_logic.md) XOR tree**. Memory is also the single largest **verification surface** on a chip (MBIST, march tests, ECC injection), so it feeds [Day 24–25](day24_verification_planning_coverage.md).

**Broadcom R2 gap this closes:** the memory question (self-rated 4/10) — SRAM, address-vs-data, organization, read/write.

---

## Deep Study Notes

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

**SRAM 6T** stores a bit in two cross-coupled inverters (a 1-bit bistable latch — same regenerative element as a flop, [Day 12](day12_metastability_synchronizers.md)); two **access transistors** connect it to the bitlines when the **wordline** is asserted. Fast and refresh-free, but **6 transistors per bit** = expensive area → why caches are small. **DRAM 1T1C** stores charge on a capacitor; tiny (1T) so dense, but the cap **leaks** → must be **refreshed**, and reading **drains** the cap (**destructive read** → write-back). That single cell-level difference explains the whole hierarchy: SRAM near the core, DRAM as bulk.

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
2. **Row decoder** (a [Day 5](day05_boolean_combinational_logic.md) `r→2^r` decoder) drives **one wordline** high → that row's cells tip the bitlines.
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
- This is the structural cousin of [Day 16](day16_cache_lines_locality_amat.md)'s set/way address decomposition — same idea (split the address into selection fields), different layer.

### 5. SRAM read/write cycle (the "explain SRAM" answer)

```
READ : precharge bitlines → assert wordline → cell pulls one bitline
       slightly → sense amp resolves → data out.  (non-destructive)
WRITE: drive bitline pair to new value (strong drivers) → assert wordline
       → access transistors let the drivers flip the cell.  (must overpower
       the cell → write-driver strength vs cell stability is the 6T sizing trade)
```

The 6T sizing balances **read stability** (don't disturb the cell when reading) against **writeability** (be able to flip it) against **speed/area** — the classic SRAM bitcell trade. Multi-port SRAMs (register files, [Day 21](day21_pipeline_microarch.md)) add wordlines/bitlines per port.

### 6. DRAM specifics — refresh and the timing parameters

- **Refresh:** each cell must be re-read-and-rewritten within its **retention time** (~64 ms typical) or it loses charge. The controller issues **refresh** commands across all rows on a schedule; refresh steals bandwidth.
- **Destructive read:** activating a row drains the caps into the sense amps, which then **write the row back** — so a read *is* a read-modify-restore.
- **Key timings:** `tRCD` (activate→read), `tCAS/CL` (read→data), `tRP` (precharge), `tRAS` (activate→precharge min). Page-hit vs page-miss is the `tRCD/tRP` story.
- **RAS/CAS:** the row address and column address are sent in two phases (Row Address Strobe, Column Address Strobe) — historically to share address pins.

### 7. The memory controller — an FSMD

The controller ([Day 9](day09_fsm_sequence_detectors.md) FSM + [Day 5–7](day05_boolean_combinational_logic.md) datapath) turns load/store/DMA requests into command sequences:

- **Address mapping policy** — how physical addr → bank/row/col (interleave for parallelism vs locality).
- **Page policy** — **open-page** (leave the row activated, bet on locality) vs **closed-page** (precharge after each access, bet on randomness).
- **Scheduling** — reorder reads/writes for row-buffer hits and bus turnaround; **refresh** insertion; **read/write grouping** (bus direction changes are expensive).
- Feeds the [Day 20](day20_dma_doorbells_ordering_barriers.md) DMA path and sits under the [Day 16](day16_cache_lines_locality_amat.md) hierarchy.

### 8. ECC — parity and Hamming SECDED

Memories flip bits (alpha particles, noise, wear). **Error-correcting codes** add redundancy:

- **Parity (1 bit):** XOR of all data bits → **detects** a single-bit error, **corrects nothing**. `parity = ^data`.
- **Hamming SECDED (Single-Error-Correct, Double-Error-Detect):**
  - Place **check bits at power-of-2 positions** (1,2,4,8…); each covers a fixed subset of data positions (by the bit of the position index it owns).
  - On read, recompute the checks → the **syndrome**. **Syndrome = 0** → clean; **syndrome = k ≠ 0** → the error is at **bit position k** → flip it (**correct**).
  - Add **one overall parity bit** over everything → distinguishes **1-bit (correctable)** from **2-bit (detectable, not correctable)**: syndrome≠0 with overall-parity error = single correctable; syndrome≠0 with overall-parity *ok* = **double error, flag uncorrectable**.
  - **Check-bit count `p`:** smallest `p` with $2^p \ge n + p + 1$ (e.g., 8 data bits → 4 check + 1 overall = 5 for SECDED).
- **Where:** register files, caches, on-chip SRAMs (inline ECC), and **DRAM** (often `+8 bits per 64` = SECDED per word). **Scrubbing** = background read-correct-rewrite to stop single errors accumulating into double errors. **ChipKill / advanced ECC** survives a whole DRAM chip failing.

ECC is a **major validation surface**: inject single-bit errors (assert correction + counter increment), inject double-bit errors (assert detection + uncorrectable flag), and check the error-logging registers — a recurring "write the test plan for ECC" interview ask ([Day 24](day24_verification_planning_coverage.md)).

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

## Bugs in this Topic

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Address-decoder fault** | read/write hits wrong cell | decoder/mux miswire | unique-data march test |
| **Stuck-at / coupling cell** | bit always 0/1 or flips with neighbour | cell/process defect | MBIST march + repair (redundant rows) |
| **Refresh starvation** | DRAM data decays | controller starves refresh under load | guarantee refresh scheduling |
| **ECC double-bit miscorrect** | silent data corruption | SEC-only code "corrects" a 2-bit error wrongly | SECDED + flag uncorrectable; scrub |
| **Byte-enable/write-mask bug** | partial-word write corrupts neighbours | wrong column/byte mask | verify byte-enables per lane |
| **Off-by-one address map** | first/last location wrong | field-split math error | re-derive size↔bits↔fields |

---

## Interview Explanation (60–90 sec)

> "I start by separating address from data: the address bus decodes to *which* location, the data bus is *what's* stored there — a memory is two-to-the-N locations of W bits each. The cell sets the character: SRAM is six transistors, two cross-coupled inverters holding the bit plus two access transistors — fast, no refresh, but big, so it's caches and register files; DRAM is one transistor and a capacitor — tiny and dense, but the cap leaks so it needs refresh, and reading drains it so a read is destructive and writes back. Addressing works by precharging the bitlines, the row decoder driving one wordline so that row's cells tip the bitlines, sense amps amplifying the tiny swing, and a column mux selecting the addressed bits; write just forces the bitlines hard enough to flip the cell. Real arrays split the address into bank, row, and column, with banks giving parallelism and the activated row sitting in a row buffer so same-row accesses are page hits. For reliability I add ECC — parity detects one bit; Hamming SECDED puts check bits at power-of-two positions so the syndrome points at the failing bit to correct it, plus an overall parity bit to flag uncorrectable double errors — and I scrub in the background so single errors don't pile up into doubles. Memories are also the biggest test surface, so I'd run MBIST march patterns for stuck-at, coupling, and decoder faults and inject single and double-bit errors to prove the ECC."

---

## Architect Drills (derive, no answer key)

1. Draw the 6T SRAM cell and walk a read then a write. Where's the read-stability vs writeability trade?
2. Why is DRAM read destructive and SRAM read not? What does each imply for the controller?
3. A 1 MiB, 64-bit-word, byte-addressable memory in 8 banks: give the full `[bank|row|col|byte]` address split.
4. Hamming-encode the 4-bit value `1011` (place check bits, compute them). Corrupt one bit; show the syndrome locating it. Corrupt two; show SECDED flags uncorrectable.
5. Compute SECDED check bits needed for a 64-bit word. Why `2^p ≥ n+p+1`?
6. Open-page vs closed-page policy: which wins for streaming, which for random? Tie to row-buffer hit/miss.
7. Write the ECC test plan: what do you inject and what does each test assert? (Hand off to [Day 24](day24_verification_planning_coverage.md).)

---

## Coding A — `d15_mem_ecc`

Build a **memory model + ECC in C**: an `addr_split()` that decomposes an address into `[bank|row|col|byte]` for given geometry (assert round-trip), a **Hamming SECDED** `ecc_encode()` / `ecc_decode()` that **corrects any single-bit** error (syndrome → flip) and **detects double-bit** errors (overall parity), and a **march-test generator** (`march_c`) over a fake array that catches an injected stuck-at / decoder fault. Full spec in [coding/EXERCISES.md](../../coding/EXERCISES.md#day-15--memory-systems--ecc).

**Interview prompts:** Address vs data in one sentence. Why power-of-2 check-bit positions? How does SECDED tell 1-bit from 2-bit errors?

---

## Bug Anchor / cross-links

- **Built from:** [Day 5](day05_boolean_combinational_logic.md) (row/column decoders, ECC XOR trees, comparators), [Day 7](day07_sequential_logic.md) (sense/precharge timing, the bistable cell), [Day 9](day09_fsm_sequence_detectors.md) (memory controller FSM).
- **Sits under:** [Day 16](day16_cache_lines_locality_amat.md) (cache organization is this array + tag/index decode), [Day 20](day20_dma_doorbells_ordering_barriers.md) (DMA targets the memory controller).
- **Feeds:** [Day 24](day24_verification_planning_coverage.md)/[Day 25](day25_testbench_assertions_debug.md) (MBIST/march + ECC-injection test plans and scoreboards).

---

## Exit Check

- [ ] State "address vs data" in one sentence and size a `2^N × W` memory both ways.
- [ ] Draw the 6T SRAM and 1T1C DRAM cells; explain volatile/refresh/destructive-read differences.
- [ ] Walk address decode: precharge → row decoder → wordline → sense amp → column mux.
- [ ] Split a physical address into `[bank|row|col|byte]` for a given geometry.
- [ ] Hamming SECDED: place check bits, compute the syndrome, correct 1 / detect 2.
- [ ] Explain open vs closed page and row-buffer hit/miss.
- [ ] Name the march-test fault models and the ECC injection tests.

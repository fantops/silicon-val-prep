# 42-Day Silicon Foundation Plan

> A **42-day, bottom-up foundation** for silicon **IP Functional Validation** — built to be run as a **continuous 12–24 month deepening while at Intel**, not a one-off interview cram. It walks from the **RTL floor up**: C → digital design → clocking/CDC → memory → CPU architecture → verification → power management → interconnect/PCIe → reasoning/narrative. Every day cross-links to the days it depends on and the days it feeds.

> **Context (Jun 2026):** the **Intel IP-Val offer is accepted** (joining 27 Jul). The plan's purpose has shifted from "interview cram" to **continuous expertise-building** — interviews become a by-product, not the goal. The **Broadcom Switch Pre-Si R2 diagnostic** ([INTERVIEW_LOG](../INTERVIEW_LOG.md)) exposed that *experience was ahead of fundamentals* (CDC, verification planning, memory, digital design, bit-manip, puzzles); this structure is the deliberate fix — **reordered so each foundation comes *before* the layer that needs it.**

This is the primary study plan: **42 sequential days (Day 0–41)**, every phase incremental on the previous one. **No DSA / LeetCode** — every coding exercise is systems-C tied to the day, in [`coding/EXERCISES.md`](../coding/EXERCISES.md).

## Daily Structure — ~2 hours

```text
20 min   Read the day file end-to-end; speak the Interview Explanation out loud.
30 min   Deep Study Notes — draw the diagram, derive the equation/FSM, name the tools.
30-45 min  Coding A (systems C) — type from scratch, compile, run, fix warnings.
20-30 min  Architect Drills — derive from constraint hints, no answer key.
10 min   Exit Check + spoken summary.
```

Each day file follows the same shape:

- **Concept** — one-line purpose.
- **Why this day in the flow** — what it depends on (earlier days), what it feeds (later days).
- **Deep Study Notes** — the real explanation, with diagrams and register/code/RTL patterns.
- **Bring-up flow / What's required / Tools** *(Phase 4, 6, 7 days)* — how this layer comes up on real silicon, and the named pre-Si + post-Si tooling.
- **Bugs in this Topic** — failure modes inlined with the concept.
- **Interview Explanation** — 60–90 sec spoken answer.
- **Architect Drills** — first-principles questions, no answer key.
- **Coding A** — systems-C tied to the day.
- **Bug Anchor / cross-links** — bidirectional links to the days this depends on and feeds.
- **Exit Check** — checklist before moving on.

> Phases 0–3, 5, 8 (RTL / methodology / reasoning) replace the silicon **Bring-up / Tools** subsections with expanded **Deep Study Notes + Architect Drills**. Phases 4, 6, 7 (CPU arch, PM, PCIe) carry the full silicon bring-up + tools sections.

## Day 0 — start here

[Day 0 — Embedded-C basics ramp](day00_c_basics.md) is the one pure-C ramp day. Compile/run discipline, functions, arrays, pointers, structs, linked lists, `memcpy`/`memmove`, size/alignment. Goal: arrive at Day 1 ready to type MMIO-style C without freezing on syntax. All hands-on exercises live in [`coding/`](../coding/).

```sh
cd ../coding
make sol-d00_my_memcpy   # build + run a reference solution
make ex-d00_my_memcpy    # build + run YOUR attempt
```

## Daily Skim (every study day)

Re-read the **Exit Check** of the previous day and the **Concept + Why this day in the flow** of the current day before starting. That 5-minute reset is the daily skim.

---

## The Sequence — 8 Phases, Bottom-Up, Strictly Incremental

The 42 days build one silicon-validation engineer from the RTL floor up. Each phase reuses the previous phase's vocabulary; the foundation layers (digital design → CDC → memory) come **before** the systems layers (CPU arch → PM → PCIe) that are built on them.

### Phase 0 — C & Programming Foundations (D0–D4)

| Day | Topic |
|---|---|
| [Day 0 — Embedded-C basics ramp](day00_c_basics.md) | compile/run, pointers, structs, `memcpy`/`memmove` |
| [Day 1 — C address fluency](day01_c_address_fluency.md) | macros, MMIO accessors, CSR walk |
| [Day 2 — Pointers, memory layout, alignment](day02_pointers_memory_layout_alignment.md) | 5 segments, padding, `container_of` |
| [Day 3 — Bits, MMIO, volatile, polling, endianness](day03_bits_mmio_volatile_polling.md) | RMW, `volatile`, W1C, timeouts |
| [Day 4 — Bit manipulation mastery](day04_bit_manipulation.md) | pow2 / **floor-pow2**, popcount, masks, rotations, XOR tricks |

### Phase 1 — Digital Design (D5–D10)

| Day | Topic |
|---|---|
| [Day 5 — Boolean + combinational logic](day05_boolean_combinational_logic.md) | K-maps, MUX/decoder/priority-encoder, adders, hazards, inferred latches |
| [Day 6 — Arithmetic circuits](day06_arithmetic_circuits.md) | ripple/CLA/prefix adders, Booth + Wallace multiplier, fixed/floating-point |
| [Day 7 — Sequential logic + flip-flops](day07_sequential_logic.md) | latch vs FF, timing contract, counters, LFSR, Gray |
| [Day 8 — Timing & STA](day08_timing_sta.md) | slack, skew/jitter/OCV, false/multicycle/CDC exceptions, timing closure |
| [Day 9 — FSM design + sequence detectors](day09_fsm_sequence_detectors.md) | Moore/Mealy, encoding, minimization, `101`/`1011` overlap, safe FSM |
| [Day 10 — Verilog & SystemVerilog](day10_verilog_systemverilog.md) | 4-state, blocking/nonblocking, X-prop, Verdi RTL reading |

### Phase 2 — Clocking, Reset & CDC (D11–D14) · *the biggest Broadcom gap*

| Day | Topic |
|---|---|
| [Day 11 — Clocking & reset architecture](day11_clocking_reset_architecture.md) | PLL, clock tree/CTS, glitchless mux, reset synchronizer, sequencing |
| [Day 12 — Metastability + synchronizers](day12_metastability_synchronizers.md) | physics, MTBF, 2-FF synchronizer, design rules |
| [Day 13 — CDC techniques](day13_cdc_techniques.md) | pulse/handshake/Gray/reset sync, RDC |
| [Day 14 — Async FIFO + CDC verification](day14_async_fifo_cdc_verification.md) | Gray pointers, full/empty, **depth = 32**, CDC lint + RDC + injection |

### Phase 3 — Memory Systems (D15–D16)

| Day | Topic |
|---|---|
| [Day 15 — SRAM/DRAM/ROM/Flash + ECC](day15_memory_systems_ecc.md) | cells, address decode, banks, controller, Hamming SECDED, MBIST/march |
| [Day 16 — Cache lines, locality, AMAT](day16_cache_lines_locality_amat.md) | hierarchy, $\text{AMAT}$, set/way math |

### Phase 4 — CPU Architecture (D17–D23)

| Day | Topic |
|---|---|
| [Day 17 — Coherence, MESI, false sharing](day17_coherence_mesi_false_sharing.md) | 4-step MESI walk, ping-pong, snoop filter |
| [Day 18 — Virtual memory, page faults](day18_virtual_memory_page_faults.md) | 4-level x86-64 walk, PTE bits, COW |
| [Day 19 — TLB, huge pages, IOMMU/SMMU](day19_tlb_huge_pages_iommu_smmu.md) | TLB coverage, shootdown, SMMU/VT-d |
| [Day 20 — DMA, doorbells, ordering, barriers](day20_dma_doorbells_ordering_barriers.md) | descriptor → barrier → doorbell → DMA → MSI |
| [Day 21 — CPU pipeline + OoO microarchitecture](day21_pipeline_microarch.md) | 5-stage + OoO (RAT/ROB/RS/LSQ), hazards, mispredict |
| [Day 22 — Memory consistency + atomics](day22_memory_consistency_atomics.md) | TSO vs weak, acq/rel, LDAR/STLR, SPSC pattern |
| [Day 23 — CPU bring-up + IP functional val](day23_cpu_bringup_functional_val.md) | reset → MMU → cache → SMP → boot; coverage axes, checkers, vplan |

### Phase 5 — Verification Engineering (D24–D25)

| Day | Topic |
|---|---|
| [Day 24 — Verification planning + coverage](day24_verification_planning_coverage.md) | test-plan-from-spec scaffold, functional vs code coverage, CDV closure |
| [Day 25 — Testbench arch + assertions + debug](day25_testbench_assertions_debug.md) | driver/monitor/scoreboard, CRV, SVA, **functional vs error tests**, first-divergence debug |

### Phase 6 — CPU Power Management (D26–D31)

| Day | Topic |
|---|---|
| [Day 26 — DVFS, P-states, PVT](day26_dvfs_pstates_pvt.md) | $P=CV^2F$, V-first/F-first, Turbo, FF/SS corners |
| [Day 27 — C-states, PkgC, S0ix](day27_cstates_pkgc_s0ix.md) | C0-C6, P-channel, PkgC6 entry/exit, PCIe L1.2 dependency |
| [Day 28 — Clock + power gating + UPF](day28_clock_power_gating_upf.md) | ICG cells, header/footer FETs, retention flops, isolation, level-shifters, UPF |
| [Day 29 — Thermal, perf counters, multi-IP](day29_thermal_power_perf_counters.md) | Tj, PROCHOT, RAPL, throttle-reason MSR, TMA, EDP |
| [Day 30 — Pre-Si PM validation](day30_pre_si_pm_validation.md) | coverage axes (state × transition × residency), UPF assertions, Perspec |
| [Day 31 — Post-Si PM validation + ADR/C6 bug stories](day31_post_si_pm_validation.md) | PM bring-up, governors, residency, MSR sweeps, ADR opcode-ordering + Core C6 wake-loss |

### Phase 7 — Interconnect & PCIe (D32–D38)

| Day | Topic |
|---|---|
| [Day 32 — AMBA AXI & CHI](day32_amba_axi_chi.md) | AXI 5 channels + VALID/READY, ordering model, CHI coherence (RN/HN/SN) |
| [Day 33 — PCIe fundamentals + LTSSM](day33_pcie_fundamentals_ltssm.md) | 3 layers, TLPs, ECAM, LTSSM, Gen6 FLIT/L0p |
| [Day 34 — PCIe enumeration, config, BARs, MSI/MSI-X](day34_pcie_enumeration_config_msi.md) | depth-first walk, header types, capability chain, BAR sizing, MSI-X |
| [Day 35 — PCIe power management](day35_pcie_power_management.md) | ASPM L0s/L1/L1.1/L1.2, D0-D3hot/D3cold, CLKREQ#, PME, LTR |
| [Day 36 — PCIe bring-up + functional val](day36_pcie_bringup_functional_val.md) | link-train debug, LTSSM stuck-state ladder, compliance, AER, eye/SI, analyser |
| [Day 37 — PCIe × PM intersection](day37_pcie_pm_intersection.md) | L1.2 ↔ PkgC dependency, root-complex PM, ADR's PCIe quiesce, cross-IP negotiation |
| [Day 38 — NVMe storage-controller SoC val](day38_nvme_storage_controller_val.md) | queues/doorbells/CQ phase, PRP/SGL, APST × L1.2, SDExpress bring-up |

### Phase 8 — Reasoning, Narrative & Wrap (D39–D41)

| Day | Topic |
|---|---|
| [Day 39 — Logic puzzles + estimation](day39_logic_puzzles_estimation.md) | method + strategy catalog + **100-coin pack** + Fermi + thinking-aloud |
| [Day 40 — Intel IP-Val role narrative + career story](day40_intel_role_narrative.md) | Intel current role, résumé-trace drill, retained cross-IHV/cross-IP bug stories |
| [Day 41 — Final mock interview day](day41_final_mock.md) | timed 90-min mock loop, revision sweep, flashcards, pre-call checklist |

---

## How Each Phase Builds (the bottom-up spine)

```
P0 C foundations (D0-D4)          ── address-space + bit fluency
      │
      ▼
P1 Digital design (D5-D10)        ── Boolean → arithmetic → sequential → STA → FSM → Verilog
      │                              (the RTL floor everything sits on)
      ▼
P2 Clocking & CDC (D11-D14)       ── clocks/reset → metastability → CDC → async FIFO
      │
      ▼
P3 Memory (D15-D16)               ── cells/ECC → cache organization
      │
      ▼
P4 CPU architecture (D17-D23)     ── coherence → VM → IOMMU → DMA → pipeline → consistency → bring-up
      │
      ▼
P5 Verification (D24-D25)         ── vplan/coverage → testbench/assertions/debug
      │
      ▼
P6 CPU power management (D26-D31) ── DVFS → C-states → gating → thermal → pre-Si → post-Si
      │
      ▼
P7 Interconnect & PCIe (D32-D38)  ── AXI/CHI → PCIe stack → enum → ASPM → bring-up → PCIe×PM → NVMe
      │
      ▼
P8 Reasoning + narrative (D39-D41)── puzzles → Intel-role story → mock
```

Each layer reuses the one below: the **FSM** (D9) is the LTSSM (D33/D36), MESI (D17), and C-state machines (D27); **CDC** (D12–14) is the first-principles version of the back-to-back-wake bug (D31); **arithmetic** (D6) is the pipeline ALU (D21); **memory cells** (D15) sit under cache organization (D16); **verification planning** (D24) generalizes the bring-up vplan (D23) and PM coverage (D30); **AXI/CHI** (D32) is the on-chip cousin of PCIe TLPs (D33).

## Bring-up + Tools — Phases 4, 6, 7

Every day in Phases 4 (CPU arch), 6 (CPU PM), and 7 (interconnect/PCIe) has dedicated **Bring-up flow**, **What's required**, and **Tools** subsections, split into:

- **Pre-Si**: VCS / Xcelium / Verdi (FSDB) / Zebu / Palladium / HAPS / Perspec / JasperGold / VC Formal / UVM / SVA / UPF (Synopsys VC-LP, Cadence Conformal-LP) / **SpyGlass-CDC + RDC** / FastModel / Spike
- **Post-Si**: JTAG / Lauterbach TRACE32 / DCI / ITP / Teledyne-LeCroy PCIe analyser / `lspci` / `setpci` / sysfs / `turbostat` / `socwatch` / `perf` / `rdmsr`/`wrmsr` / RAPL / Quarch / Monsoon / scope + rail probes

This bring-up + tools fluency is the most underrated topic at IP-Val interviews.

## Tailoring & Acceleration

- **RTL / DV-shaped loop (Broadcom / switch pre-Si / Arm / DV roles):** front-load Phases 1–2–5: D5→D10 (digital design) → D11→D14 (clocking & CDC, the biggest gap) → D24→D25 (verification) → D39 (puzzles). Pull D32 (AXI/CHI) and D15 (memory/ECC) as needed. Deliver the D24 test-plan scaffold and the D9 `101` detector cold.
- **CPU-IP-Val-shaped loop (Intel / AMD / QC):** prioritise D16 (cache) → D17 (MESI) → D23 (bring-up) → D27 (C-states) → D31 (post-Si PM + ADR/C6) → D33 (LTSSM) → D37 (PCIe×PM) → D41 (mock), with Phases 1–2 as the EE-fundamentals backstop.
- **Continuous build (default, while at Intel):** run **0 → 41** straight; the order is already incremental. Re-run the D41 mock + flashcard sweep before any conversation.

The night-before re-read order is the **Exit Check** of every day in your chosen chain.

## A Note on Bug Stories

A tight bug bank, each mapping to a concept day + a story day:

| Bug | Concept day(s) | Story day |
|---|---|---|
| ADR opcode-ordering hang | D20 (ordering), D27 (PkgC), D28 (gating), **D12–14 (CDC)** | D31 |
| Core C6 wake-loss (CFC FIFO race) | D27 (PkgC), D28 (gating), **D12–14 (CDC/async-FIFO)** | D31 |
| Producer-consumer with no barrier | D20, D22 | D22 |
| HW prefetcher hides a software bug | D21, D29 | D21 |
| False sharing (head/tail on a line) | D17, D22 | D22 |
| PCIe link stuck in Polling | D33, D36 | D36 |
| PkgC6 blocked by PCIe not in L1.2 | D27, D35, D37 | D37 |
| L0p re-scales mid-FLIT | D33, D35 | D37 |
| AXI VALID-waits-on-READY deadlock | D32 | D32 |
| Inferred latch / incomplete case | D5, D9, D10 | D10 |
| ECC double-bit miscorrection | D15 | D15 |
| QNN page-fault + IOMMU-restore race *(cross-IHV anchor)* | D18/D19 (page-fault/IOMMU), D12–13 (wake CDC) | D40 |

The intentional duplication (concept day + story day) is the point — a topic and its real failure mode are cheaper to remember together. The CDC structures (D12–14) now give the **first-principles** explanation of the PM wake races that used to be told only as stories.

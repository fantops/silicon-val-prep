# Silicon Validation Learning Plan

> **Active direction (Oct 2026):** build on existing strength — **CPU power-management flows and post-silicon debug** — plus the fundamentals that explain PM failures. This is not a pivot into verification engineering. The 42 day files are now a **reference library**, not a sequence to complete.

> **Out of scope here:** DSA is learned through daily work and lives in [`dsa-val/`](../dsa-val/). The foundation uses it only as the worked accelerator example (S13–S15). The pre-silicon verification-engineering track (D24–D25: testbench, UVM/CRV, SVA, coverage closure) is parked as interview-only reference.

## Priorities

1. **Priority 0 — office work.** Never trade delivery or sleep for prep.
2. **Priority 1 — stay interview-ready in the CPU-PM debug lane.** Continuous, low-volume, never fully paused.

## Topic tiers

| Tier | Purpose | Days |
|---|---|---|
| **Core — CPU PM debug** | the differentiator; go deep | [D26 DVFS](archive/day26_dvfs_pstates_pvt.md) · [D27 C-states/PkgC/S0ix](archive/day27_cstates_pkgc_s0ix.md) · [D28 gating/UPF](archive/day28_clock_power_gating_upf.md) · [D29 thermal/telemetry](archive/day29_thermal_power_perf_counters.md) · [D31 post-Si PM debug](archive/day31_post_si_pm_validation.md) · [D35 PCIe PM](archive/day35_pcie_power_management.md) · [D37 PCIe×PM](archive/day37_pcie_pm_intersection.md) |
| **Support — explains PM bugs** | study only as far as a PM failure needs it | [D11 clock/reset](archive/day11_clocking_reset_architecture.md) · [D12](archive/day12_metastability_synchronizers.md)–[D14](archive/day14_async_fifo_cdc_verification.md) CDC/wake races · [D9 FSMs](archive/day09_fsm_sequence_detectors.md) · [D16](archive/day16_cache_lines_locality_amat.md)–[D17](archive/day17_coherence_mesi_false_sharing.md) cache/coherence (flush on C6) · [D20](archive/day20_dma_doorbells_ordering_barriers.md)/[D22](archive/day22_memory_consistency_atomics.md) ordering · [D23 bring-up](archive/day23_cpu_bringup_functional_val.md) · [D33](archive/day33_pcie_fundamentals_ltssm.md)/[D36](archive/day36_pcie_bringup_functional_val.md) LTSSM |
| **Interview maintenance** | keep stories and recall warm | [D40 narrative](archive/day40_intel_role_narrative.md) · bug-story bank below · [REVISION.md](REVISION.md) · light [D39 puzzles](archive/day39_logic_puzzles_estimation.md) / [D4 bit-manip](archive/day04_bit_manipulation.md) |
| **Backstop — refresh on demand** | only when an interview or bug requires it | D0–D3 C · D5–D8, D10 digital design · D15 memory/ECC · D18–D19 VM/IOMMU · D21 pipeline · D32 AXI/CHI · D34 enum · D38 NVMe · D41 mock |
| **Parked** | not an active track | [D24](archive/day24_verification_planning_coverage.md)–[D25](archive/day25_testbench_assertions_debug.md) verification engineering · D30 pre-Si PM coverage/UPF-assertion parts |

Keep the practical validation habits you already use in debug — requirement → observable check, state × transition thinking, negative cases, debug from the first divergence. Those live inside PM debug; they don't need a separate track.

## Start here: foundation plan (sequential, starts Mon 19 Oct 2026)

**Current plan:** [FOUNDATION_PLAN.md](FOUNDATION_PLAN.md). It is self-contained: a big-picture stack map, a dated calendar, 18 sessions plus a weekly coding slot, a DSA work link per session, buffer and mock weeks, and a tracker. The order is electrical signal → logic → clocks/reset/CDC → power-on → CPU boot → memory/coherence → **PCIe boot → enumeration → driver** → DMA/IOMMU → **accelerator bring-up and one job end to end (DSA as the worked example)** → gating/DVFS → C-states/S0ix + device PM → post-silicon debug. Finish it (≈ 13 Dec) before starting the 8-week block below.

## Cadence

**Three ~60 min sessions per week.** During the foundation plan, all three sessions follow the sequence, plus one weekend coding slot. During the 8-week depth block:

- **2 × Core session**: one PM flow or failure mode per session.
- **1 × Maintenance session**: sweep one phase of REVISION.md, then tell one bug story out loud (≤90 s).

Heavy office week → do only one session. Don't make up missed sessions.

**Session output (definition of done):** a half-page note answering *expected flow → what's observable (MSRs/counters/residency/trace) → where it breaks → how I'd isolate the first divergence.* If you can explain it without notes, it's done.

## The next 8 weeks (after the foundation)

| Weeks | Focus | Days | Deliverable |
|---|---|---|---|
| **1–2** | Idle-state entry/exit end-to-end: core C-states → PkgC → S0ix; what each gates, who votes, blockers | D27, D28 | Whiteboard-able PkgC6 entry/exit flow + top 5 "PkgC won't enter" debug ladder |
| **3–4** | Wake-path failures from first principles: reset/clock sequencing, CDC, lost-pulse and ordering races | D11–D13 (as needed), D31, D20 | Rewrite the Core C6 wake-loss and ADR ordering-hang stories with the mechanism explained, not just told |
| **5–6** | Active-power side: DVFS sequencing, Turbo limits, thermal/RAPL, throttle-reason telemetry | D26, D29, D31 | "Why is frequency/power not what I expect" debug ladder with the counters/tools you'd check |
| **7–8** | Cross-IP PM: PCIe ASPM/L1.2/LTR ↔ PkgC, device D-states | D35, D37 (D33 only if needed) | "PkgC6 blocked by PCIe" story + P/C/D-state comparison from memory; refresh D40 narrative |

At the end of week 8: re-read your notes, pick the next 8-week block based on what was weakest (likely candidates: S0ix/platform PM depth, PM firmware/PCU flows, or a backstop gap an interview exposed).

**If an interview gets scheduled:** pause the cycle, run 1–2 weeks on the role-shaped chain under *Tailoring* below + [D41 mock](archive/day41_final_mock.md), then resume.

## C ramp (backstop, on demand)

[Day 0 — Embedded-C basics ramp](archive/day00_c_basics.md) is the pure-C refresher, useful before a coding round. All hands-on exercises live in [`coding/`](../coding/).

```sh
cd ../coding
make sol-d00_my_memcpy   # build + run a reference solution
make ex-d00_my_memcpy    # build + run YOUR attempt
```

## Before each session

Re-read your previous session note, then skim the matching session guide linked in [FOUNDATION_PLAN.md](FOUNDATION_PLAN.md). Follow its archived-note links only when you need more depth. That 5-minute reset is enough.

---

## Reference sequence — foundations available on demand

The original day sequence is a useful dependency map when a topic needs a refresher. It is not the active schedule: select the smallest prerequisite chain that explains the CPU-PM topic in front of you.

### Phase 0 — C & Programming Foundations (D0–D4)

| Day | Topic |
|---|---|
| [Day 0 — Embedded-C basics ramp](archive/day00_c_basics.md) | compile/run, pointers, structs, `memcpy`/`memmove` |
| [Day 1 — C address fluency](archive/day01_c_address_fluency.md) | macros, MMIO accessors, CSR walk |
| [Day 2 — Pointers, memory layout, alignment](archive/day02_pointers_memory_layout_alignment.md) | 5 segments, padding, `container_of` |
| [Day 3 — Bits, MMIO, volatile, polling, endianness](archive/day03_bits_mmio_volatile_polling.md) | RMW, `volatile`, W1C, timeouts |
| [Day 4 — Bit manipulation mastery](archive/day04_bit_manipulation.md) | pow2 / **floor-pow2**, popcount, masks, rotations, XOR tricks |

### Phase 1 — Digital Design (D5–D10)

| Day | Topic |
|---|---|
| [Day 5 — Boolean + combinational logic](archive/day05_boolean_combinational_logic.md) | K-maps, MUX/decoder/priority-encoder, adders, hazards, inferred latches |
| [Day 6 — Arithmetic circuits](archive/day06_arithmetic_circuits.md) | ripple/CLA/prefix adders, Booth + Wallace multiplier, fixed/floating-point |
| [Day 7 — Sequential logic + flip-flops](archive/day07_sequential_logic.md) | latch vs FF, timing contract, counters, LFSR, Gray |
| [Day 8 — Timing & STA](archive/day08_timing_sta.md) | slack, skew/jitter/OCV, false/multicycle/CDC exceptions, timing closure |
| [Day 9 — FSM design + sequence detectors](archive/day09_fsm_sequence_detectors.md) | Moore/Mealy, encoding, minimization, `101`/`1011` overlap, safe FSM |
| [Day 10 — Verilog & SystemVerilog](archive/day10_verilog_systemverilog.md) | 4-state, blocking/nonblocking, X-prop, Verdi RTL reading |

### Phase 2 — Clocking, Reset & CDC (D11–D14) · *the biggest Broadcom gap*

| Day | Topic |
|---|---|
| [Day 11 — Clocking & reset architecture](archive/day11_clocking_reset_architecture.md) | PLL, clock tree/CTS, glitchless mux, reset synchronizer, sequencing |
| [Day 12 — Metastability + synchronizers](archive/day12_metastability_synchronizers.md) | physics, MTBF, 2-FF synchronizer, design rules |
| [Day 13 — CDC techniques](archive/day13_cdc_techniques.md) | pulse/handshake/Gray/reset sync, RDC |
| [Day 14 — Async FIFO + CDC verification](archive/day14_async_fifo_cdc_verification.md) | Gray pointers, full/empty, **depth = 32**, CDC lint + RDC + injection |

### Phase 3 — Memory Systems (D15–D16)

| Day | Topic |
|---|---|
| [Day 15 — SRAM/DRAM/ROM/Flash + ECC](archive/day15_memory_systems_ecc.md) | cells, address decode, banks, controller, Hamming SECDED, MBIST/march |
| [Day 16 — Cache lines, locality, AMAT](archive/day16_cache_lines_locality_amat.md) | hierarchy, $\text{AMAT}$, set/way math |

### Phase 4 — CPU Architecture (D17–D23)

| Day | Topic |
|---|---|
| [Day 17 — Coherence, MESI, false sharing](archive/day17_coherence_mesi_false_sharing.md) | 4-step MESI walk, ping-pong, snoop filter |
| [Day 18 — Virtual memory, page faults](archive/day18_virtual_memory_page_faults.md) | 4-level x86-64 walk, PTE bits, COW |
| [Day 19 — TLB, huge pages, IOMMU/SMMU](archive/day19_tlb_huge_pages_iommu_smmu.md) | TLB coverage, shootdown, SMMU/VT-d |
| [Day 20 — DMA, doorbells, ordering, barriers](archive/day20_dma_doorbells_ordering_barriers.md) | descriptor → barrier → doorbell → DMA → MSI |
| [Day 21 — CPU pipeline + OoO microarchitecture](archive/day21_pipeline_microarch.md) | 5-stage + OoO (RAT/ROB/RS/LSQ), hazards, mispredict |
| [Day 22 — Memory consistency + atomics](archive/day22_memory_consistency_atomics.md) | TSO vs weak, acq/rel, LDAR/STLR, SPSC pattern |
| [Day 23 — CPU bring-up + IP functional val](archive/day23_cpu_bringup_functional_val.md) | reset → MMU → cache → SMP → boot; coverage axes, checkers, vplan |

### Optional interview reference — verification engineering (D24–D25)

| Day | Topic |
|---|---|
| [Day 24 — Verification planning + coverage](archive/day24_verification_planning_coverage.md) | broad test-plan-from-spec, functional/code coverage, and closure material; park unless an interview specifically requires it |
| [Day 25 — Testbench arch + assertions + debug](archive/day25_testbench_assertions_debug.md) | UVM-shaped testbench, CRV, SVA, and scoreboard material; park unless an interview specifically requires it |

This is **not an active career-development track**. Retain only the practical validation habits needed in IP work: map requirements to observable checks, cover meaningful modes and transitions, include negative cases, and debug from the first divergence. Apply those habits inside CPU-PM debug rather than studying testbench architecture or formal verification as separate specialties.

### Phase 6 — CPU Power Management (D26–D31)

| Day | Topic |
|---|---|
| [Day 26 — DVFS, P-states, PVT](archive/day26_dvfs_pstates_pvt.md) | $P=CV^2F$, V-first/F-first, Turbo, FF/SS corners |
| [Day 27 — C-states, PkgC, S0ix](archive/day27_cstates_pkgc_s0ix.md) | C0-C6, P-channel, PkgC6 entry/exit, PCIe L1.2 dependency |
| [Day 28 — Clock + power gating + UPF](archive/day28_clock_power_gating_upf.md) | ICG cells, header/footer FETs, retention flops, isolation, level-shifters, UPF |
| [Day 29 — Thermal, perf counters, multi-IP](archive/day29_thermal_power_perf_counters.md) | Tj, PROCHOT, RAPL, throttle-reason MSR, TMA, EDP |
| [Day 30 — Pre-Si PM validation](archive/day30_pre_si_pm_validation.md) | coverage axes (state × transition × residency), UPF assertions, Perspec |
| [Day 31 — Post-Si PM validation + ADR/C6 bug stories](archive/day31_post_si_pm_validation.md) | PM bring-up, governors, residency, MSR sweeps, ADR opcode-ordering + Core C6 wake-loss |

### Phase 7 — Interconnect & PCIe (D32–D38)

| Day | Topic |
|---|---|
| [Day 32 — AMBA AXI & CHI](archive/day32_amba_axi_chi.md) | AXI 5 channels + VALID/READY, ordering model, CHI coherence (RN/HN/SN) |
| [Day 33 — PCIe fundamentals + LTSSM](archive/day33_pcie_fundamentals_ltssm.md) | 3 layers, TLPs, ECAM, LTSSM, Gen6 FLIT/L0p |
| [Day 34 — PCIe enumeration, config, BARs, MSI/MSI-X](archive/day34_pcie_enumeration_config_msi.md) | depth-first walk, header types, capability chain, BAR sizing, MSI-X |
| [Day 35 — PCIe power management](archive/day35_pcie_power_management.md) | ASPM L0s/L1/L1.1/L1.2, D0-D3hot/D3cold, CLKREQ#, PME, LTR |
| [Day 36 — PCIe bring-up + functional val](archive/day36_pcie_bringup_functional_val.md) | link-train debug, LTSSM stuck-state ladder, compliance, AER, eye/SI, analyser |
| [Day 37 — PCIe × PM intersection](archive/day37_pcie_pm_intersection.md) | L1.2 ↔ PkgC dependency, root-complex PM, ADR's PCIe quiesce, cross-IP negotiation |
| [Day 38 — NVMe storage-controller SoC val](archive/day38_nvme_storage_controller_val.md) | queues/doorbells/CQ phase, PRP/SGL, APST × L1.2, SDExpress bring-up |

### Phase 8 — Reasoning, Narrative & Wrap (D39–D41)

| Day | Topic |
|---|---|
| [Day 39 — Logic puzzles + estimation](archive/day39_logic_puzzles_estimation.md) | method + strategy catalog + **100-coin pack** + Fermi + thinking-aloud |
| [Day 40 — Intel IP-Val role narrative + career story](archive/day40_intel_role_narrative.md) | Intel current role, résumé-trace drill, retained cross-IHV/cross-IP bug stories |
| [Day 41 — Final mock interview day](archive/day41_final_mock.md) | timed 90-min mock loop, revision sweep, flashcards, pre-call checklist |

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
P5 Optional interview reference (D24-D25) ── broad verification planning/testbench material
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

Each layer reuses the one below: the **FSM** (D9) is the LTSSM (D33/D36), MESI (D17), and C-state machines (D27); **CDC** (D12–14) is the first-principles version of the back-to-back-wake bug (D31); **arithmetic** (D6) is the pipeline ALU (D21); **memory cells** (D15) sit under cache organization (D16); lightweight requirement-to-check thinking supports CPU-PM validation (D23/D30); **AXI/CHI** (D32) is the on-chip cousin of PCIe TLPs (D33).

## Bring-up + Tools — Phases 4, 6, 7

Every day in Phases 4 (CPU arch), 6 (CPU PM), and 7 (interconnect/PCIe) has dedicated **Bring-up flow**, **What's required**, and **Tools** subsections, split into:

- **Pre-Si**: VCS / Xcelium / Verdi (FSDB) / Zebu / Palladium / HAPS / Perspec / JasperGold / VC Formal / UVM / SVA / UPF (Synopsys VC-LP, Cadence Conformal-LP) / **SpyGlass-CDC + RDC** / FastModel / Spike
- **Post-Si**: JTAG / Lauterbach TRACE32 / DCI / ITP / Teledyne-LeCroy PCIe analyser / `lspci` / `setpci` / sysfs / `turbostat` / `socwatch` / `perf` / `rdmsr`/`wrmsr` / RAPL / Quarch / Monsoon / scope + rail probes

This bring-up + tools fluency is the most underrated topic at IP-Val interviews.

## Tailoring & Acceleration

- **Verification-engineering interview loop:** the historical D24–D25 material remains available as targeted interview prep. It is not part of the current role-aligned study cadence.
- **CPU-IP-Val-shaped loop (Intel / AMD / QC):** prioritise D16 (cache) → D17 (MESI) → D23 (bring-up) → D27 (C-states) → D31 (post-Si PM + ADR/C6) → D33 (LTSSM) → D37 (PCIe×PM) → D41 (mock), with Phases 1–2 as the EE-fundamentals backstop.
- **CPU-PM debug focus (default):** follow the 8-week block and topic tiers at the top of this file. Do not treat D0→D41 or the interview mock as a required continuous sequence.

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

# 25-Day Silicon Validation Prep Plan

> Restructured 23 May 2026 for **CPU IP Functional Validation** loops. The old plan covered too many surfaces (ARM vocab, NVMe, NPU stack, DSA grinding). This version sticks to **three tracks** that map directly to what an IP Functional Val engineer ramps on: **CPU Architecture**, **CPU Power Management (pre-Si + post-Si)**, and **PCIe (functional + PM + bring-up)**. The aim is "join with context" — be able to follow the team's whiteboard on day one, not ask "what is L1.2."

This is the primary study plan: **25 sequential days** — Day 0 (C-basics ramp) + Days 1-24 (interview prep), every phase incremental on the previous one. **No DSA / LeetCode** — every coding exercise is systems-C tied to the day. Current-role / NPU content is collapsed to a single exit-pointer day.

## Daily Structure — 2 hours (revised)

```text
20 min   Read the day file end-to-end; speak the Interview Explanation out loud.
30 min   Bring-up flow + tools section — draw the diagram, name the tools.
30-45 min  Coding A (systems C) — type from scratch, compile, run, fix warnings.
20-30 min  Architect Drills — derive from constraint hints, no answer key.
10 min   Exit Check + spoken summary.
```

Coding scope shrinks from ~90 min/day to **30-45 min** and is **systems-C only** — bit/MMIO/CSR harnesses, FSM models, PM/PCIe simulators. Daily total ~2 hr.

Each day file follows the same shape:

- **Concept** — one-line purpose.
- **Why this day in the flow** — what it depends on, what it feeds.
- **Deep Study Notes** — the real explanation, with diagrams and register/code patterns.
- **Bring-up flow** *(Phase 2-4 days)* — how this layer is brought up on real silicon.
- **What's required** *(Phase 2-4 days)* — signals, infra, golden refs, stim/checkers.
- **Tools** *(Phase 2-4 days)* — pre-Si (sim/emul/formal/UPF) and post-Si (lab/MSR/perf/analyser) tooling, named.
- **Bugs in this Topic** — failure modes inlined with the concept.
- **Interview Explanation** — 60-90 sec spoken answer.
- **Architect Drills** — first-principles questions, no answer key.
- **Coding A** — systems-C tied to the day.
- **Bug Anchor** — cross-links to days where this concept's bugs are referenced.
- **Exit Check** — checklist before moving on.

## Day 0 — Embedded-C basics ramp (do before Day 1)

[Day 0 — Embedded-C basics ramp](day00_c_basics.md) is the one pure-C ramp day. Compile/run discipline, functions, arrays, pointers, structs, linked lists, `memcpy`/`memmove`, size/alignment. Goal: arrive at Day 1 ready to type MMIO-style C without freezing on syntax.

All hands-on exercises live in [`coding/`](../coding/) — one source tree, one document ([coding/EXERCISES.md](../coding/EXERCISES.md)).

```sh
cd ../coding
make sol-d00_my_memcpy   # build + run a reference solution
make ex-d00_my_memcpy    # build + run YOUR attempt
```

## Daily Skim (do every study day)

Re-read the **Exit Check** of the previous day and the **Concept + Why this day in the flow** of the current day before starting. That 5-minute reset is the daily skim — no separate guide needed.

---

## The Sequence — Three Tracks, Strictly Incremental

The 25 days walk one IP-Val ramp end-to-end. Each phase reuses the previous phase's mental model.

### Phase 0 — Foundations (Day 0 + Days 1-4)

The address-space + memory-hierarchy fluency every silicon role is built on.

| Day | Topic |
|---|---|
| [Day 0 — Embedded-C basics ramp](day00_c_basics.md) | compile/run, pointers, structs, `memcpy`/`memmove` |
| [Day 1 — C address fluency](day01_c_address_fluency.md) | macros, MMIO accessors, CSR walk |
| [Day 2 — Pointers, memory layout, alignment](day02_pointers_memory_layout_alignment.md) | 5 segments, padding, `container_of` |
| [Day 3 — Bits, MMIO, volatile, polling, endianness](day03_bits_mmio_volatile_polling.md) | RMW, `volatile`, W1C, timeouts |
| [Day 4 — Cache lines, locality, AMAT](day04_cache_lines_locality_amat.md) | hierarchy, $\text{AMAT}$, set/way math |

### Phase 1 — CPU Architecture (Days 5-11)

The core IP and its memory subsystem — the substrate every other layer depends on. Bring-up sections show how a CPU IP comes up from reset.

| Day | Topic |
|---|---|
| [Day 5 — Coherence, MESI, false sharing](day05_coherence_mesi_false_sharing.md) | 4-step MESI walk, ping-pong, snoop filter |
| [Day 6 — Virtual memory, page faults](day06_virtual_memory_page_faults.md) | 4-level x86-64 walk, PTE bits, COW |
| [Day 7 — TLB, huge pages, IOMMU/SMMU](day07_tlb_huge_pages_iommu_smmu.md) | TLB coverage, shootdown, SMMU/VT-d |
| [Day 8 — DMA, doorbells, ordering, barriers](day08_dma_doorbells_ordering_barriers.md) | descriptor → barrier → doorbell → DMA → MSI |
| [Day 9 — CPU pipeline + OoO microarchitecture](day09_pipeline_microarch.md) | 5-stage + OoO (RAT/ROB/RS/LSQ), hazards, mispredict |
| [Day 10 — Memory consistency + atomics](day10_memory_consistency_atomics.md) | TSO vs weak, acq/rel, LDAR/STLR, SPSC pattern |
| [Day 11 — CPU bring-up + IP functional val](day11_cpu_bringup_functional_val.md) | reset → MMU → cache → SMP → boot; coverage axes, checkers, what a CPU IP val team owns |

### Phase 2 — CPU Power Management (Days 12-17)

The full active + idle power chapter, ending with **two dedicated days** for pre-Si vs post-Si PM validation — the exact split a CPU IP Functional Val team operates across.

| Day | Topic |
|---|---|
| [Day 12 — DVFS, P-states, PVT](day12_dvfs_pstates_pvt.md) | $P=CV^2F$, V-first/F-first, Turbo, FF/SS corners |
| [Day 13 — C-states, PkgC, S0ix](day13_cstates_pkgc_s0ix.md) | C0-C6, P-channel, PkgC6 entry/exit, PCIe L1.2 dependency |
| [Day 14 — Clock + power gating + UPF/power-intent](day14_clock_power_gating_upf.md) | ICG cells, header/footer FETs, retention flops, isolation, level-shifters, UPF |
| [Day 15 — Thermal, perf counters, multi-IP](day15_thermal_power_perf_counters.md) | Tj, PROCHOT, RAPL, throttle reason MSR, TMA, EDP |
| [Day 16 — Pre-Si PM validation](day16_pre_si_pm_validation.md) | coverage axes (state × transition × residency), UPF assertions, low-power sims, Perspec, PCU opcode tracing |
| [Day 17 — Post-Si PM validation + ADR/C6 bug stories](day17_post_si_pm_validation.md) | PM bring-up (PMIC/PCU/FW), governors, residency, MSR sweeps, lab tools, ADR opcode-ordering + Core C6 wake-loss |

### Phase 3 — PCIe (Days 18-22)

The PCIe layer, taken seriously: stack + enumeration + PM + bring-up + the **PCIe × PM intersection** that dominates real platform debug.

| Day | Topic |
|---|---|
| [Day 18 — PCIe fundamentals + LTSSM](day18_pcie_fundamentals_ltssm.md) | 3 layers, TLPs, ECAM, LTSSM, Gen6 FLIT/L0p |
| [Day 19 — PCIe enumeration, config, BARs, MSI/MSI-X](day19_pcie_enumeration_config_msi.md) | depth-first walk, header types, capability chain, BAR sizing, MSI-X table |
| [Day 20 — PCIe power management](day20_pcie_power_management.md) | ASPM L0s/L1/L1.1/L1.2, D0-D3hot/D3cold, CLKREQ#, PME, LTR |
| [Day 21 — PCIe bring-up + functional val](day21_pcie_bringup_functional_val.md) | link train debug, LTSSM stuck-state ladder, compliance, AER, eye/SI, analyser usage |
| [Day 22 — PCIe × PM intersection](day22_pcie_pm_intersection.md) | L1.2 ↔ PkgC dependency, root-complex PM, ADR's PCIe quiesce, cross-IP negotiation |

### Phase 4 — Wrap (Days 23-24)

| Day | Topic |
|---|---|
| [Day 23 — MSFT/NPU exit pointers + retained QNN story](day23_msft_npu_exit_pointers.md) | 1-day distillation: what to probe at MSFT before leaving + one anchor bug story (QNN page-fault) for "tell me about a hard bug" |
| [Day 24 — Final mock interview day](day24_final_mock.md) | timed 90-min mock loop, revision sweep, bug-story rehearsal, flashcards, pre-call checklist |

---

## How Each Phase Builds on the Previous

```
Phase 0 (D0-D4)   →   address-space + cache fluency
        │
        ▼
Phase 1 (D5-D11)  →   coherence → VM → IOMMU → DMA → pipeline → consistency → bring-up
        │             (the CPU IP you're going to validate)
        ▼
Phase 2 (D12-D17) →   active power → idle power → gating → thermal → pre-Si val → post-Si val
        │             (the dimension a CPU IP Func Val team actually owns)
        ▼
Phase 3 (D18-D22) →   PCIe stack → enum → ASPM → bring-up → PCIe×PM
        │             (the bus that carries every cross-IP test)
        ▼
Phase 4 (D23-D24) →   distil + mock
```

Each new phase explicitly references the previous one's vocabulary. Example: PCIe L1.2 (D20) reuses C-state mechanics from D13; pre-Si PM val (D16) reuses UPF from D14 and microarch from D9; post-Si PM val (D17) reuses MSR/perf-counter vocabulary from D15.

## Bring-up + Tools — Every Phase 1-3 Day

To keep "what do you mean by bring-up" from being a vague hand-wave, every day in Phases 1-3 has dedicated subsections:

- **Bring-up flow** — the canonical sequence at this layer (reset → train → enable → enumerate → ...).
- **What's required** — signals, infra, golden references, stimulus + checker pairs, fuses/straps if relevant.
- **Tools** — split into:
  - **Pre-Si**: VCS / Xcelium / Verdi (FSDB) / Zebu / Palladium / HAPS / Perspec / JasperGold / VC Formal / UVM / SVA / UPF (Synopsys VC-LP, Cadence Conformal-LP) / FastModel
  - **Post-Si**: JTAG / Lauterbach / SVTools / DCI / ITP / Lecroy/Teledyne PCIe analyser / `lspci` / `setpci` / sysfs / `turbostat` / `socwatch` / `perf` / `rdmsr/wrmsr` / RAPL / Quarch / Monsoon / scope + rail probes

This is the part the original plan glossed over and is the most underrated topic at IP Functional Val interviews.

## Tailoring & Acceleration

If your loop is in <14 days, prioritise: D4 (cache/AMAT) → D5 (MESI) → D11 (bring-up) → D13 (C-states) → D17 (post-Si PM + ADR/C6 stories) → D18 (LTSSM) → D22 (PCIe×PM) → D24 (mock). Skip Phase 0 days you already know cold. The night-before re-read order is the **Exit Check** of every day in that chain.

## A Note on Bug Stories

The plan keeps a tight bug bank that all map to the three tracks plus the one retained NPU anchor:

| Bug | Concept day | Story day |
|---|---|---|
| ADR opcode-ordering hang | Day 8 (ordering), Day 13 (PkgC), Day 14 (gating) | Day 17 |
| Core C6 wake-loss (CFC FIFO race) | Day 13 (PkgC), Day 14 (gating) | Day 17 |
| Producer-consumer with no ARM barrier | Day 8, Day 10 | Day 10 |
| HW prefetcher hides a software bug | Day 9, Day 15 | Day 9 |
| False sharing (head/tail on same line) | Day 5, Day 10 | Day 10 |
| PCIe link stuck in Polling | Day 18, Day 21 | Day 21 |
| PkgC6 blocked by PCIe link not in L1.2 | Day 13, Day 20, Day 22 | Day 22 |
| L0p re-scales mid-FLIT | Day 18, Day 20 | Day 22 |
| QNN page-fault + VM regression *(retained anchor)* | Day 6 (page faults) | Day 23 |

The intentional duplication (concept day + story day) is the whole point — a topic and its real failure mode are cheaper to remember together than apart.

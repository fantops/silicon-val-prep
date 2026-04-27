# 21-Day Silicon Validation Prep Plan

This is the primary plan. It is a re-derivation of the [30-day plan](../30day_plan/) into 21 sequential days that follow the **system flow** — from the smallest fluency (pointers, bits, MMIO) up through caches, memory, buses, storage, power/thermal, CPU microarch, concurrency, NPU, and finally debug and mock. The 30-day plan is unchanged; every 21-day file links to the 30-day source(s) it merged.

Each day is one self-contained study file with this structure:

- **Source merge** — links to the 30-day file(s) the day was built from.
- **Concept** — one-line purpose.
- **Why this day in the flow** — what it depends on, what it feeds.
- **Deep Study Notes** — the real explanation, with diagrams, register/code patterns, and tables.
- **Bugs in this Topic** — bug stories inlined alongside the concept so the topic and the failure mode are learned together.
- **Interview Explanation** — a 60-90 sec spoken answer.
- **Practice Q&A** — short questions that test understanding.
- **Coding A** — systems-C code tied to the day's concept.
- **Coding B** — interview-style DS&A code (LeetCode-class) for the round.
- **Bug Anchor** — cross-links to the days where this concept's bugs are referenced.
- **Exit Check** — checklist that must be clear before moving on.

## Daily Structure

Plan for ~2-3 hours per day:

```text
30 min   Read the day file end-to-end; speak the Interview Explanation out loud.
60 min   Coding A (systems C) — type from scratch, no copy-paste.
30 min   Coding B (DS&A) — pattern recognition + clean implementation.
30 min   Practice Q&A out loud + close with the Exit Check.
```

## Day 0 — Daily Skim Guide (do this every day)

[Day 0 — Career narrative + revision skim guide](day00_revision_guide.md) — 5-minute daily skim covering Introduction, current Microsoft work, Intel work, L&T work, why-I-left-Intel, per-company tailoring (Qualcomm CPU sysval / Intel NPU post-Si / Microsoft lateral), the 60-sec NPU explanation, and a 10-question Q&A bank. **Read this before every study block.** Use the 30-sec / 60-sec / 2-min versions in §10 as warm-up.

## The Sequence — System-Flow Layers

The 21 days walk the platform from the bottom up. Each layer corresponds to a section of the 30-day plan; the merge condenses without losing material.

### Layer 1 — C, pointers, bits, MMIO fluency (Days 1-3)

The address-space and register-access fluency every silicon role is built on.

| Day | Topic | 30-day source |
|---|---|---|
| [Day 1 — C address fluency](day01_c_address_fluency.md) | macros, MMIO accessors, CSR walk | day00 + day22 |
| [Day 2 — Pointers, memory layout, alignment](day02_pointers_memory_layout_alignment.md) | 5 segments, padding, `container_of` | day02 |
| [Day 3 — Bits, MMIO, volatile, polling, endianness](day03_bits_mmio_volatile_polling.md) | RMW, `volatile`, LE/BE, timeouts | day03 + day22 + day23 |

### Layer 2 — Memory hierarchy, MMU, IOMMU, DMA (Days 4-8)

How data physically gets between cores, devices and DRAM.

| Day | Topic | 30-day source |
|---|---|---|
| [Day 4 — Cache lines, locality, AMAT](day04_cache_lines_locality_amat.md) | hierarchy, $\text{AMAT}$, set/way math | day08 + day09 |
| [Day 5 — Coherence, MESI, false sharing](day05_coherence_mesi_false_sharing.md) | 4-step MESI walk, ping-pong | day10 + day11 |
| [Day 6 — Virtual memory, page faults](day06_virtual_memory_page_faults.md) | 4-level x86-64 walk, PTE bits, COW | day12 |
| [Day 7 — TLB, huge pages, IOMMU/SMMU](day07_tlb_huge_pages_iommu_smmu.md) | TLB coverage, shootdown, SMMU/VT-d | day13 |
| [Day 8 — DMA, doorbells, ordering, barriers](day08_dma_doorbells_ordering_barriers.md) | descriptor → barrier → doorbell → DMA → MSI | day13 + day22 |

### Layer 3 — Platform vocabulary (Day 9)

The ARM/Snapdragon side that platform-validation interviews keep returning to.

| Day | Topic | 30-day source |
|---|---|---|
| [Day 9 — ARM/Qualcomm platform vocabulary](day09_arm_qualcomm_platform_vocabulary.md) | EL0-3, GIC, SMMU, RPMh, PSCI, LPDDR training, boot chain | day06b + day19b |

### Layer 4 — Buses & storage (Days 10-11)

The link between SoC and the outside world.

| Day | Topic | 30-day source |
|---|---|---|
| [Day 10 — PCIe fundamentals + LTSSM](day10_pcie_fundamentals_ltssm.md) | 3 layers, TLPs, ECAM, BAR sizing, full LTSSM, Gen6 FLIT/L0p | day15 + day16 + day17 + day18 + day19 |
| [Day 11 — NVMe queues + bring-up](day11_nvme_queues_bringup.md) | SQ/CQ rings, doorbell formula, full bring-up, CSTS.RDY 10-suspect ladder | day20 + day21 |

### Layer 5 — Power, thermal, perf-counters (Days 12-15)

The dimensions a modern SoC actually negotiates.

| Day | Topic | 30-day source |
|---|---|---|
| [Day 12 — DVFS, P-states, PVT](day12_dvfs_pstates_pvt.md) | $P=CV^2f$, V-first/F-first, Turbo, FF/SS corners, RPMh | day04 + day06 |
| [Day 13 — C-states, PkgC, S0ix](day13_cstates_pkgc_s0ix.md) | C0-C6, P-channel, full PkgC6 entry/exit, PCIe L1.2 dependency | day05 |
| [Day 14 — ADR, PCU, Core C6 bugs](day14_adr_pcu_core_c6_bugs.md) | ADR flow, two polished bug stories | day05 + day29 (Bugs 1, 3) |
| [Day 15 — Thermal, perf counters, multi-IP](day15_thermal_power_perf_counters.md) | Tj, PROCHOT, 5 multi-IP classes, TMA, RAPL, EDP, geomean | day26 + day27 + day27b |

### Layer 6 — CPU microarchitecture & concurrency (Days 16-17)

The CPU itself, and the primitives you'll be asked to write under interview pressure.

| Day | Topic | 30-day source |
|---|---|---|
| [Day 16 — Pipeline, memory model, SIMD](day16_pipeline_memory_consistency_simd.md) | 5-stage + OoO (RAT/ROB/RS/LSQ), TSO vs weak, AVX-512 license level | day13b |
| [Day 17 — Concurrency, lock-free queues, DSA](day17_concurrency_queues_dsa.md) | atomics, spinlock, SPSC ring, ABA, false sharing, DSA pattern bank | day28b |

### Layer 7 — NPU runtime (Days 18-19)

The workload your current Microsoft job and any Qualcomm/Intel NPU role centres on.

| Day | Topic | 30-day source |
|---|---|---|
| [Day 18 — NPU stack: ORT, EP, Hexagon HTP](day18_npu_ort_ep_hexagon_htp.md) | 6-layer stack, EP partition + fallback, QDQ fusion, VTCM, HMX/HVX/DMA pipeline | day01 + SPRINT_CONCEPT_TRANSCRIPTS |
| [Day 19 — NPU memory, runtime & scheduling bugs](day19_npu_memory_runtime_bugs.md) | QNN page-fault story, Paint SR vs Camera, silent EP fallback, VTCM cliff, NPU-blocks-S0ix | day29 (Bugs 4, 4b) + day01 |

### Layer 8 — Debug method + mock (Days 20-21)

Turning the technical material into hire-no-hire signal.

| Day | Topic | 30-day source |
|---|---|---|
| [Day 20 — Debug methodology + polished bug stories](day20_debug_method_bug_polish.md) | 7-step framework, bug-cost ladder, 5 polished 90-sec stories | day29 + day24 + SPRINT_3DAY_PLAN |
| [Day 21 — Final mock + per-company tailoring](day21_final_mock_company_tailoring.md) | timed mock loop, flashcard sweep, Qualcomm/Intel/Microsoft anchor sentences | day30 + day01 + QUALCOMM_CHEATSHEET |

## How Each Day's Bugs Connect

Bug stories are deliberately woven across the days so the same root mechanism appears in two layers — once as a concept on the day it lives, once as a story on Day 14 / 19 / 20. Cross-reference table:

| Bug | Concept day | Story day |
|---|---|---|
| ADR opcode-ordering hang | Day 8 (ordering / barriers), Day 13 (PkgC) | Day 14, Day 20 (story 1) |
| Core C6 wake-loss (CFC FIFO race) | Day 13 (PkgC), Day 16 (clock domain) | Day 14, Day 20 (story 3) |
| NVMe CSTS.RDY hang (AQA mis-sized) | Day 11 (bring-up) | Day 20 (story 2) |
| QNN/ORT page-fault + VM regression | Day 6 (page faults), Day 18 (EP / context cache) | Day 19, Day 20 (story 4) |
| Paint SR starves Camera preview | Day 17 (single-queue scheduling), Day 18 (NPU dispatch) | Day 19, Day 20 (story 4b) |
| Silent EP fallback after model rebuild | Day 18 (QDQ fusion / EP) | Day 19, Day 20 (story 5) |
| False sharing (head/tail on same line) | Day 5, Day 17 | Day 17 |
| Producer-consumer with no ARM barrier | Day 8, Day 16 | Day 16 |
| HW prefetcher hides a software bug | Day 16, Day 15 | Day 16 |

## Tailoring & Acceleration

If your loop is in <14 days, follow the **Tier-1 path** in [Day 21 §3](day21_final_mock_company_tailoring.md). The night-before re-read order is also there.

## Source Mapping

The complete 30-day → 21-day source map (each 30-day file is referenced by at least one 21-day day):

```
day00 c_rampup                 → Day 1
day02 pointers_memory          → Day 2
day03 bit_manipulation         → Day 3
day04 dvfs_power               → Day 12
day05 cstates_idle             → Day 13, Day 14
day06 pvt_corners              → Day 12
day06b arm_qualcomm_power      → Day 9
day08 cache_fundamentals       → Day 4
day09 cache_policies           → Day 4
day10 mesi_coherence           → Day 5
day11 false_sharing            → Day 5
day12 virtual_memory           → Day 6
day13 tlb_iommu                → Day 7
day13b pipeline_isa_simd       → Day 16
day15 pcie_physical            → Day 10
day16 pcie_transactions        → Day 10
day17 pcie_config              → Day 10
day18 ltssm                    → Day 10
day19 ltssm_power_gen6         → Day 10
day19b clocks_lpddr_boot       → Day 9
day20 nvme_architecture        → Day 11
day21 nvme_bringup_coding      → Day 11
day22 volatile_mmio            → Day 1, Day 3, Day 8
day23 endianness               → Day 3
day24 sim_vs_emul              → Day 20
day26 thermal                  → Day 15
day27 multi_ip_perf            → Day 15
day27b perf_counters           → Day 15
day28b concurrency_dsa_drills  → Day 17
day29 debug_bugs               → Day 14, Day 19, Day 20
day30 mock_final               → Day 21
day01 career_narrative         → Day 18, Day 19, Day 21
QUALCOMM_CHEATSHEET            → Day 21
SPRINT_3DAY_PLAN               → Day 20, Day 21
SPRINT_CONCEPT_TRANSCRIPTS     → Day 18
```

## A Note on the Bug Stories

The **same** four polished stories from `day29_debug_bugs.md` appear in two places by design:

- The **concept** day (e.g., Day 11 for NVMe CSTS.RDY, Day 14 for ADR + Core C6, Day 19 for QNN page-fault) — so the failure mode is learned with the topic.
- **Day 20** — the polished 90-second telling for the loop.

This intentional duplication is the whole point of the 21-day re-derivation: a topic and its real failure mode are cheaper to remember together than apart.

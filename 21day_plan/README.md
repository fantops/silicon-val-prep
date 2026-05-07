# 21-Day Silicon Validation Prep Plan

This is the primary study plan. It has one technical ramp day plus **21 sequential interview-prep days** following the system flow — from embedded-C basics, pointers, bits, and MMIO up through caches, memory, buses, storage, power/thermal, CPU microarch, concurrency, NPU, and finally debug and mock.

The operating principle is **learn by doing**: every day has reading, spoken interview rehearsal, systems-C coding, DS&A coding, and architect drills. Do not treat the files as notes to memorise. Treat them as prompts to type, explain, debug, and derive.

Each day is one self-contained study file with this structure:

- **Concept** — one-line purpose.
- **Why this day in the flow** — what it depends on, what it feeds.
- **Deep Study Notes** — the real explanation, with diagrams, register/code patterns, and tables.
- **Bugs in this Topic** — bug stories inlined alongside the concept so the topic and the failure mode are learned together.
- **Interview Explanation** — a 60-90 sec spoken answer.
- **Architect Drills** — first-principles questions with constraint hints only; no answer key.
- **Coding A** — systems-C code tied to the day's concept.
- **Coding B** — interview-style DS&A code (LeetCode-class) for the round.
- **Bug Anchor** — cross-links to the days where this concept's bugs are referenced.
- **Exit Check** — checklist that must be clear before moving on.

## Daily Structure

Plan for ~2-3 hours per day:

```text
25 min   Read the day file end-to-end; speak the Interview Explanation out loud.
55 min   Coding A (systems C) — type from scratch, compile, run, and fix warnings.
35 min   Coding B (DS&A) — pattern recognition + clean implementation.
25 min   Architect Drills — derive from constraint hints, no answer key.
20 min   Speak one drill answer out loud + close with the Exit Check.
```

## Day 0 — Embedded-C basics ramp (do before Day 1)

[Day 0 — Embedded-C basics ramp](day00_c_basics.md) is the one pure C-basics day before the 21-day system flow starts. It covers compile/run discipline, functions, arrays, strings, pointers, structs, linked lists, `memcpy`/`memmove`, and size/alignment checks. The goal is not mastery of all C; the goal is to arrive at Day 1 ready to type and debug MMIO-style C without freezing on syntax.

All hands-on exercises — for Day 0 *and* Days 1–20 — live in the sibling [`coding/`](../coding/) folder (moved out of `21day_plan/` so the source tree is independent of the daily reading). One document, one source tree:

```sh
cd ../coding
make sol-d00_my_memcpy   # build + run a reference solution
make ex-d00_my_memcpy    # build + run YOUR attempt
```

Read [coding/EXERCISES.md](../coding/EXERCISES.md) — every problem statement, signature, and driver is there in one place. (Architect drills and bug anchors live in each day file.)

## Daily Skim Guide (do this every study day)

**Generic 30-min pre-interview skim** — repo root: [INTERVIEW_REVISION_GUIDE.md](../INTERVIEW_REVISION_GUIDE.md). Topic pointers, ARM↔x86 mapping, methodology reflexes — no personal narrative. The personal pitch + per-company tailoring (Qualcomm / Intel NPU / Intel Ireland / Microsoft) lives in [Day 21 §3 + §Personal Narrative](day21_final_mock_company_tailoring.md). **Read the revision guide before every study block.**

## The Sequence — System-Flow Layers

The 21 days walk the platform from the bottom up.

### Layer 1 — C, pointers, bits, MMIO fluency (Day 0 + Days 1-3)

The address-space and register-access fluency every silicon role is built on.

| Day | Topic |
|---|---|
| [Day 0 — Embedded-C basics ramp](day00_c_basics.md) | compile/run, functions, arrays, strings, pointers, structs, linked lists, `memcpy`/`memmove` |
| [Day 1 — C address fluency](day01_c_address_fluency.md) | macros, MMIO accessors, CSR walk |
| [Day 2 — Pointers, memory layout, alignment](day02_pointers_memory_layout_alignment.md) | 5 segments, padding, `container_of` |
| [Day 3 — Bits, MMIO, volatile, polling, endianness](day03_bits_mmio_volatile_polling.md) | RMW, `volatile`, LE/BE, timeouts |

### Layer 2 — Memory hierarchy, MMU, IOMMU, DMA (Days 4-8)

How data physically gets between cores, devices and DRAM.

| Day | Topic |
|---|---|
| [Day 4 — Cache lines, locality, AMAT](day04_cache_lines_locality_amat.md) | hierarchy, $\text{AMAT}$, set/way math |
| [Day 5 — Coherence, MESI, false sharing](day05_coherence_mesi_false_sharing.md) | 4-step MESI walk, ping-pong |
| [Day 6 — Virtual memory, page faults](day06_virtual_memory_page_faults.md) | 4-level x86-64 walk, PTE bits, COW |
| [Day 7 — TLB, huge pages, IOMMU/SMMU](day07_tlb_huge_pages_iommu_smmu.md) | TLB coverage, shootdown, SMMU/VT-d |
| [Day 8 — DMA, doorbells, ordering, barriers](day08_dma_doorbells_ordering_barriers.md) | descriptor → barrier → doorbell → DMA → MSI |

### Layer 3 — Platform vocabulary (Day 9)

The ARM/Snapdragon side that platform-validation interviews keep returning to.

| Day | Topic |
|---|---|
| [Day 9 — ARM/Qualcomm platform vocabulary](day09_arm_qualcomm_platform_vocabulary.md) | EL0-3, GIC, SMMU, RPMh, PSCI, LPDDR training, boot chain |

### Layer 4 — Buses & storage (Days 10-11)

The link between SoC and the outside world.

| Day | Topic |
|---|---|
| [Day 10 — PCIe fundamentals + LTSSM](day10_pcie_fundamentals_ltssm.md) | 3 layers, TLPs, ECAM, BAR sizing, full LTSSM, Gen6 FLIT/L0p |
| [Day 11 — NVMe queues + bring-up](day11_nvme_queues_bringup.md) | SQ/CQ rings, doorbell formula, full bring-up, CSTS.RDY 10-suspect ladder |

### Layer 5 — Power, thermal, perf-counters (Days 12-15)

The dimensions a modern SoC actually negotiates.

| Day | Topic |
|---|---|
| [Day 12 — DVFS, P-states, PVT](day12_dvfs_pstates_pvt.md) | $P=CV^2f$, V-first/F-first, Turbo, FF/SS corners, RPMh |
| [Day 13 — C-states, PkgC, S0ix](day13_cstates_pkgc_s0ix.md) | C0-C6, P-channel, full PkgC6 entry/exit, PCIe L1.2 dependency |
| [Day 14 — ADR, PCU, Core C6 bugs](day14_adr_pcu_core_c6_bugs.md) | ADR flow, two polished bug stories |
| [Day 15 — Thermal, perf counters, multi-IP](day15_thermal_power_perf_counters.md) | Tj, PROCHOT, 5 multi-IP classes, TMA, RAPL, EDP, geomean |

### Layer 6 — CPU microarchitecture & concurrency (Days 16-17)

The CPU itself, and the primitives you'll be asked to write under interview pressure.

| Day | Topic |
|---|---|
| [Day 16 — Pipeline, memory model, SIMD](day16_pipeline_memory_consistency_simd.md) | 5-stage + OoO (RAT/ROB/RS/LSQ), TSO vs weak, AVX-512 license level |
| [Day 17 — Concurrency, lock-free queues, DSA](day17_concurrency_queues_dsa.md) | atomics, spinlock, SPSC ring, ABA, false sharing, DSA pattern bank |

### Layer 7 — NPU runtime (Days 18-19)

The workload your current Microsoft job and any Qualcomm/Intel NPU role centres on.

| Day | Topic |
|---|---|
| [Day 18 — NPU stack: ORT, EP, Hexagon HTP](day18_npu_ort_ep_hexagon_htp.md) | 6-layer stack, EP partition + fallback, QDQ fusion, VTCM, HMX/HVX/DMA pipeline |
| [Day 19 — NPU memory, runtime & scheduling bugs](day19_npu_memory_runtime_bugs.md) | QNN page-fault story, Paint SR vs Camera, silent EP fallback, VTCM cliff, NPU-blocks-S0ix |

### Layer 8 — Debug method + mock (Days 20-21)

Turning the technical material into hire-no-hire signal.

| Day | Topic |
|---|---|
| [Day 20 — Debug methodology + polished bug stories](day20_debug_method_bug_polish.md) | 7-step framework, bug-cost ladder, 5 polished 90-sec stories |
| [Day 21 — Final mock + per-company tailoring](day21_final_mock_company_tailoring.md) | timed mock loop, flashcard sweep, Qualcomm/Intel/Microsoft anchor sentences |

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

If your loop is in <14 days, follow the **Tier-1 path** in [Day 21 section 3](day21_final_mock_company_tailoring.md). The night-before re-read order is also there.

**Per-company tailoring lives in [Day 21 §3](day21_final_mock_company_tailoring.md):**
- Qualcomm — Snapdragon / Hexagon HTP / NVMe.
- Intel (generic) — Cambria / PCIe Gen6 / ADR / CXL.
- **Intel Ireland (NPU validation, Leixlip / Shannon)** — pre-Si + post-Si seam, JD line-by-line mapping, country/culture framing. Pair with [INTERVIEW_REVISION_GUIDE.md §0B / §4A / §6 / §8 / §10](../INTERVIEW_REVISION_GUIDE.md) for the active interview intersections, bug stories, NPU depth, EE refresh, and final checklist.
- Microsoft — Copilot+ PC / EdgeOptimizer / multi-vendor breadth.

## A Note on the Bug Stories

The **same** four polished bug stories appear in two places by design:

- The **concept** day (e.g., Day 11 for NVMe CSTS.RDY, Day 14 for ADR + Core C6, Day 19 for QNN page-fault) — so the failure mode is learned with the topic.
- **Day 20** — the polished 90-second telling for the loop.

This intentional duplication is the whole point of the plan: a topic and its real failure mode are cheaper to remember together than apart.

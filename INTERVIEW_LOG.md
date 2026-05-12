# Interview Log — Apr 2026 Cycle

> One row per round. Keep it light: what happened, what I learned, status, next action. Detailed prep frameworks live in [INTERVIEW_REVISION_GUIDE.md](INTERVIEW_REVISION_GUIDE.md) and the [21-day plan](21day_plan/).

---

## 📅 Timeline

### 13 Apr — Intel · Power & Performance — ❌ Rejected
- **What happened:** R1 ok. R2 — couldn't write a struct-based C program under pressure; missed the *why* behind PkgC entry/exit (break-even, residency, wake cost).
- **Learning:** Knew *what*, missed *why*. Live-C must be reflex.
- **Suggestion:** Daily Day 0 C drill; rehearse C-state ladder as break-even residency.

### 15 Apr — Arm · Verification — 🟡 Mismatch
- **What happened:** SV/UVM-heavy DV screen; not my track.
- **Learning:** Filter pure DV roles upstream.

### 20 Apr — Qualcomm · Pre-Si Verification — 🟡 Mismatch
- **What happened:** Same shape as Arm; not pursued.
- **Learning:** Confirmed DV-pre-Si is out of arc.

### 28 Apr — Qualcomm · CPU Core (R1) — 🟢 Cleared
- **What happened:** Pipeline stages, `a = b` data movement, MS NPU walkthrough. Coding solid.
- **Learning:** High-level concept + concrete project narrative lands cleanly.
- **Status:** Advanced to R2.

### 28 Apr — Qualcomm · PnP / Thermal (Hyderabad, 2 rounds) — 🟢 Feedback pending
- **What happened:** Power/perf, thermal basics, system debug. Domain alignment + cross-vendor NPU breadth landed. Thermal control-loop depth and leadership/ownership framing slightly soft.
- **Learning:** Frame thermal as a control loop; tighten ownership STAR.
- **Suggestion:** Section 0A behavioural rehearsal; Day 15 control-loop framing.

### 29 Apr — Intel Ireland · NPU — 🟢 Feedback pending
- **What happened:** Rapid-fire, 2 interviewers. ~70-80% landed. Cross-vendor NPU + EdgeOptimizer story strong. EE fundamentals weak; bug story tangled under pace.
- **Learning:** Slow ~10%; STAR-strict bug stories; refresh EE basics.
- **Suggestion:** 90-sec timer on Bug 2 / Bug 3; quick CMOS / setup-hold / CDC / SI refresh (Section 8).

### 30 Apr — Qualcomm · CPU Core (R2) — � Advanced despite weak fundamentals
- **What happened:** "Probe-then-push" style — drilled C-state *why-a-ladder*, clock gating *how*, leakage as separate mechanism, cache hierarchy *why multi-level*, generic boot flow. CFC FIFO / Core C6 wake-loss bug story landed cleanly; textbook depth ran out at layer 2-3.
- **Learning:** Failure mode is shallow ownership, not pressure handling. Need to rehearse the *what → why → how → connected-concept* chain out loud, not just read it. Bug-story ownership + system breadth carried the round even when textbook depth ran out — signal that owned narrative compensates for layer-3 gaps.
- **Status:** Advanced to Hiring Manager (12 May) despite weak fundamentals call. Gap-fills landed in repo (Day 04, Day 13, Day 09; Architect Drills; Section 0B / 4B).
- **Suggestion:** Daily what/why/how/bug-hook chain on one concept, no notes.

### 07 May — Renesas · R1 — ❌ Rejected (role-fit miss)
- **What happened:** L&T PCIe/NVMe + MS NPU walkthrough flowed; PCIe/NVMe protocol depth rusty (5-6 yrs since daily use). Bug story itself landed.
- **Learning:** Role likely wanted automotive protocol fluency (CAN/LIN/FlexRay/AVB/SOME-IP) + live PCIe/NVMe — neither is current stack.
- **Suggestion:** Do **not** add automotive track. Filter Renesas-shape roles upstream. Keep PCIe/NVMe at bug-story + first-principles depth ([Day 10 enumeration / boot-flow placement](21day_plan/day10_pcie_fundamentals_ltssm.md), [Day 09 Section 12a PCIe in boot flow](21day_plan/day09_arm_qualcomm_platform_vocabulary.md), [Day 11 NVMe bring-up + HMB-throttle bug](21day_plan/day11_nvme_queues_bringup.md), [Day 20 Section 5a CPU+PCIe coverage matrix + Stories 6/7](21day_plan/day20_debug_method_bug_polish.md)).

### 12 May — Intel Bangalore · IP Functional Validation (F2F, 2 rounds) — 🟢 Feedback pending
- **What happened (R1, 2 engineers):** Msft/Intel walkthrough + how I narrow down a model regression. CPU core states — pushed on **negative scenarios** (e.g. requested C3 not achieved, peripheral blocking entry — what can happen, not specific). ISRs. Page table / page fault / MMU. C storage classes + usage. Program memory layout. C coding: **reverse a string using pointers, no extra variable**.
- **What happened (R2):** Explain **ADR flow** — how to decide *which data* needs to be copied (answered: maintain a table of regions / persistent-state descriptors). ISRs — *when* served, *where* the data is stored, what mechanism is used (context save / stack / vector table). Behavioural: why left Intel, why switching again; interviewer framed *"most work everywhere is prep activity, only 5-10% translates to CV"* — testing if I'd flinch. Each topic ran long.
- **Learning:** Negative-scenario framing for C-states (blocker → why entry denied → who owns the veto) is a recurring probe — needs a reflex chain. ADR "what gets copied" needs a crisp answer: persistent-region table + flush list + ordering, not hand-wavy. ISR storage answer needs the **hardware (vector table, stack, banked regs) + software (context frame)** split. Behavioural push on "prep vs CV work" needs an honest, non-defensive answer.
- **Suggestion:** Add C-state negative-path drill (peripheral veto, broadcast-block, LTR, demotion) to [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md). Tighten ADR "copy table" framing in [Day 14](21day_plan/day14_adr_pcu_core_c6_bugs.md). ISR context-save chain into [Day 03](21day_plan/day03_bits_mmio_volatile_polling.md) / [Day 09](21day_plan/day09_arm_qualcomm_platform_vocabulary.md). Reverse-string-in-place into [Day 00](21day_plan/day00_c_basics.md) reflex set. Rehearse "why switching" / "prep-vs-CV" answer in [Section 0A](INTERVIEW_REVISION_GUIDE.md).

### 12 May — Qualcomm · CPU Core (Hiring Manager) — 🟢 Positive signal
- **What happened:** Basics on what I did and how, then mostly behavioural. Pushed on *no CPU-core experience — why hire you?* and *what value-add after joining?* (expecting "hard work + dedication" framing). Direct ask: *if switching for money, don't come to QC*. Asked about other offers — said Arm in discussion. *Arm vs QC?* → answered QC for engineering scope. Overall positive; they will combine all interview outputs.
- **Learning:** HM rounds at QC weight **intent + attitude** over depth — "hard work / dedication / scope" framing lands; money-led framing is an instant red flag. Cross-offer question is a real signal probe — answer with *fit reasoning*, not leverage.
- **Status:** Awaiting combined-loop decision.
- **Suggestion:** Lock in a 30-sec "why QC over Arm / why CPU core despite no prior CPU-core work" answer in [Section 0A](INTERVIEW_REVISION_GUIDE.md) — value-add = validation rigor + cross-vendor system view + ownership pattern, not domain seniority.

---

## 📊 Snapshot

| Date | Company | Role | Outcome | Status |
|---|---|---|---|---|
| 13 Apr | Intel | Power & Perf | ❌ Rejected | Closed |
| 15 Apr | Arm | Verification | 🟡 Mismatch | Closed |
| 20 Apr | Qualcomm | Pre-Si Verification | 🟡 Mismatch | Closed |
| 28 Apr | Qualcomm | CPU Core (R1) | 🟢 Cleared | Advanced to R2 |
| 28 Apr | Qualcomm | PnP / Thermal | 🟢 Good | Feedback pending |
| 29 Apr | Intel Ireland | NPU | 🟢 Strong | Feedback pending |
| 30 Apr | Qualcomm | CPU Core (R2) | � Weak fundamentals, advanced anyway | Advanced to HM |
| 07 May | Renesas | Automotive-leaning | ❌ Rejected | Closed — role-fit |
| 12 May | Intel Bangalore | IP Functional Validation (F2F, 2 rounds) | 🟢 Good | Feedback pending |
| 12 May | Qualcomm | CPU Core (Hiring Manager) | 🟢 Positive | Awaiting combined decision |

---

## 🧩 Interview intersections → revision map

| Intersection | Seen in | Anchor | Source | Rehearsal |
|---|---|---|---|---|
| C under pressure | Intel PnP R2 | [Revision Guide](INTERVIEW_REVISION_GUIDE.md), [Revision Guide](INTERVIEW_REVISION_GUIDE.md) | [Day 00](21day_plan/day00_c_basics.md), [Day 01](21day_plan/day01_c_address_fluency.md), [Day 02](21day_plan/day02_pointers_memory_layout_alignment.md) | One Day 0 C exercise daily |
| The *why* behind concepts | Qualcomm CPU R2 | [Revision Guide](INTERVIEW_REVISION_GUIDE.md), [Revision Guide](INTERVIEW_REVISION_GUIDE.md) | [Day 04](21day_plan/day04_cache_lines_locality_amat.md), [Day 12](21day_plan/day12_dvfs_pstates_pvt.md), [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md), [Day 16](21day_plan/day16_pipeline_memory_consistency_simd.md) | One what/why/how/bug-hook chain daily |
| C-states, clock gating, leakage | Intel PnP, Qualcomm CPU R2 | [Revision Guide](INTERVIEW_REVISION_GUIDE.md) | [Day 12](21day_plan/day12_dvfs_pstates_pvt.md), [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md), [Day 15](21day_plan/day15_thermal_power_perf_counters.md) | Draw entry/exit + clock-gating path from memory |
| PM coverage matrix (the one I lived) | Intel PnP, Qualcomm PnP, NVMe-SSD-flavoured roles | [Day 20 Section 5a](21day_plan/day20_debug_method_bug_polish.md#5a-coverage-plan--power-management-across-pre-si-and-post-si-the-one-i-lived) | [Day 12](21day_plan/day12_dvfs_pstates_pvt.md), [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md), [Day 14](21day_plan/day14_adr_pcu_core_c6_bugs.md), [Day 15](21day_plan/day15_thermal_power_perf_counters.md) | Recite three axes (state / transition / residency) + name one owned cell per axis |
| Cache hierarchy internals | Qualcomm CPU R2 | [Revision Guide](INTERVIEW_REVISION_GUIDE.md) | [Day 04](21day_plan/day04_cache_lines_locality_amat.md), [Day 05](21day_plan/day05_coherence_mesi_false_sharing.md) | Derive hierarchy from latency/capacity/area |
| Generic boot flow | Qualcomm CPU R2 | [Revision Guide](INTERVIEW_REVISION_GUIDE.md) | [Day 09](21day_plan/day09_arm_qualcomm_platform_vocabulary.md) | POWER → ROM → PLATFORM INIT → BOOTLOADER → OS, then vendor map |
| NPU + EdgeOptimizer | Intel Ireland, Qualcomm probes | [Revision Guide](INTERVIEW_REVISION_GUIDE.md), [Revision Guide](INTERVIEW_REVISION_GUIDE.md) | [Day 18](21day_plan/day18_npu_ort_ep_hexagon_htp.md), [Day 19](21day_plan/day19_npu_memory_runtime_bugs.md) | Lead with six-layer stack + per-inference attribution |
| Bug story under rapid-fire | Intel Ireland | [Revision Guide](INTERVIEW_REVISION_GUIDE.md) | [Day 20](21day_plan/day20_debug_method_bug_polish.md), [Day 21](21day_plan/day21_final_mock_company_tailoring.md) | 90-sec STAR timer |
| EE fundamentals | Intel Ireland | [Revision Guide](INTERVIEW_REVISION_GUIDE.md) | [Day 10](21day_plan/day10_pcie_fundamentals_ltssm.md), [Day 16](21day_plan/day16_pipeline_memory_consistency_simd.md) | Setup/hold, CDC, CMOS, PCIe SI one-liners |
| Leadership / ownership | Qualcomm PnP | [Revision Guide](INTERVIEW_REVISION_GUIDE.md) | [Day 20](21day_plan/day20_debug_method_bug_polish.md), [Day 21](21day_plan/day21_final_mock_company_tailoring.md) | One real example per framework |

---

## 🧭 Patterns

**Strong:** cross-vendor NPU experience, concrete bug stories with measurable impact, EdgeOptimizer/tooling narrative, high-level concept walks.

**Weak:** the *why* behind concepts (layer 2-3 depth), live C under pressure, bug-story discipline under rapid-fire, EE fundamentals, leadership framing.

**Meta-skill being tested:** "probe-then-push" — *what → why → how → connected concept*. First layer fluent, second thin, third empty. Bug stories survive the push because owned end-to-end; textbook topics don't because learned as facts, not derivations. **Fix: rehearse the chain out loud, not read it.**

**Improving:** role-fit filtering (forced misfit → early recognition → upstream filter), domain-fit clarity, same-day self-diagnosis.

**Strategic principle:** build on what I own (NPU validation, post-Si platform, power/perf, system debug). Defend PCIe/NVMe and automotive at story depth only — don't grow new limbs for adjacent role families.

---

## 🔁 Flywheel (after every round)

1. **Same day:** add row to timeline above — what happened, learning, status, suggestion.
2. **Within 24h:** map each "didn't land" to a Day file Architect Drill — add one if missing.
3. **Within 48h:** ensure [Revision Guide](INTERVIEW_REVISION_GUIDE.md) Section 0B / 4B / 8 covers the gap.
4. **Before next interview:** run [Revision Guide](INTERVIEW_REVISION_GUIDE.md) with the new gap-fill in rotation.

---

## 📝 End-of-cycle retro (fill when all feedback in)

- [ ] Top gap closed: _____
- [ ] Top gap still open: _____
- [ ] Best-fitting role: _____
- [ ] Companies to re-approach: _____
- [ ] Companies to deprioritise: _____
- [ ] One thing I'd do differently: _____

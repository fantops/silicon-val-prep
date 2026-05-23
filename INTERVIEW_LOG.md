# Interview Log — Apr 2026 Cycle

> One row per round. Keep it light: what happened, what I learned, status, next action. Detailed prep frameworks live in the [25-day plan](prep_plan/) (Day 24 = final mock loop / flashcard sweep).

---

## 📈 Cycle scoreboard (as of 22 May 2026)

**Total rounds: 15** across **9 companies** / **12 distinct role-loops**.

Round-by-round breakdown:

| Loop | Rounds | Detail |
|---|---|---|
| Intel · Power & Perf (Apr) | 2 | R1 + R2 — ❌ |
| Arm · Verification | 1 | 🟡 mismatch |
| Qualcomm · Pre-Si Verification | 1 | 🟡 mismatch |
| Qualcomm · CPU Core | 3 | R1 + R2 + HM — 🔴 stalled, no reply 6+ days |
| Qualcomm · PnP / Thermal (Hyderabad) | 2 | both rounds same day — 🔴 stalled, no reply 6+ days |
| Intel Ireland · NPU | 1 | rapid-fire 2 interviewers — ❌ rejected 21 May |
| Renesas | 1 | R1 — ❌ role-fit |
| Intel Bangalore · IP Functional Validation | 2 | F2F R1 + R2 — ✅ **Selected, final offer 21 May** |
| Cerebras | 1 | R1 — ❌ not fit for role |
| AMD · Core PnP Post-Si | 1 | R1 — 🔴 stalled, no reply 6+ days |
| **Amazon Lab126 · AZ1 ML SoC** | 0 | 🟢 **Revived 22 May** — loop scheduled next week |
| **Nvidia · role TBD** | 0 | 🟡 **Recruiter call 22 May** — loop pending scheduling |
| **Total** | **15** | |

Loop status:

| Bucket | Count | Loops |
|---|---|---|
| ✅ Selected (final offer) | **1** | Intel Bangalore |
| 🟢 Active — loop scheduled / inbound | **2** | Amazon Lab126 · Nvidia |
| 🔴 Stalled (no reply 6+ days after leverage email) | **3** | QC CPU Core · QC PnP/Thermal · AMD |
| ❌ Rejected / closed | **4** | Intel Power & Perf (Apr) · Renesas · Cerebras · Intel Ireland NPU |
| 🟡 Mismatch — filtered upstream | **2** | Arm Verification · QC Pre-Si Verification |
| **Total loops** | **12** | |

**Conversion (loops, not rounds):** 3 / 12 landed (1 selected + 2 active inbound) = **25%** (denominator grew from 10 → 12 with Amazon-revive + Nvidia-add; stalled-3 not counted as landed).

**Velocity:** 15 rounds over ~32 days (13 Apr → 15 May, last round). Pipeline reopened 22 May — 2 new loops queued; first round of new wave likely week of 26 May.

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

### 29 Apr — Intel Ireland · NPU — ❌ Rejected (closed 21 May)
- **What happened:** Rapid-fire, 2 interviewers. ~70-80% landed. Cross-vendor NPU + EdgeOptimizer story strong. EE fundamentals weak; bug story tangled under pace.
- **Learning:** Slow ~10%; STAR-strict bug stories; refresh EE basics. Confirmed: EE-fundamentals weakness + tangled bug-story-under-pace is the failure mode that closes NPU-heavy loops — same pattern that cost Intel P&P Apr R2.
- **Status:** ❌ **Rejected 21 May.** No further rounds.
- **Suggestion:** 90-sec timer on Bug 2 / Bug 3; quick CMOS / setup-hold / CDC / SI refresh (Section 8). Keep EE basics in rotation — this is now a 2x-closed gap.

### 30 Apr — Qualcomm · CPU Core (R2) — � Advanced despite weak fundamentals
- **What happened:** "Probe-then-push" style — drilled C-state *why-a-ladder*, clock gating *how*, leakage as separate mechanism, cache hierarchy *why multi-level*, generic boot flow. CFC FIFO / Core C6 wake-loss bug story landed cleanly; textbook depth ran out at layer 2-3.
- **Learning:** Failure mode is shallow ownership, not pressure handling. Need to rehearse the *what → why → how → connected-concept* chain out loud, not just read it. Bug-story ownership + system breadth carried the round even when textbook depth ran out — signal that owned narrative compensates for layer-3 gaps.
- **Status:** Advanced to Hiring Manager (12 May) despite weak fundamentals call. Gap-fills landed in repo (Day 04, Day 13, Day 09; Architect Drills; Section 0B / 4B).
- **Suggestion:** Daily what/why/how/bug-hook chain on one concept, no notes.

### 07 May — Renesas · R1 — ❌ Rejected (role-fit miss)
- **What happened:** L&T PCIe/NVMe + MS NPU walkthrough flowed; PCIe/NVMe protocol depth rusty (5-6 yrs since daily use). Bug story itself landed.
- **Learning:** Role likely wanted automotive protocol fluency (CAN/LIN/FlexRay/AVB/SOME-IP) + live PCIe/NVMe — neither is current stack.
- **Suggestion:** Do **not** add automotive track. Filter Renesas-shape roles upstream. Keep PCIe/NVMe at bug-story + first-principles depth ([Day 18 LTSSM fundamentals](prep_plan/day18_pcie_fundamentals_ltssm.md), [Day 19 enumeration / config / MSI](prep_plan/day19_pcie_enumeration_config_msi.md), [Day 21 PCIe bring-up + functional val](prep_plan/day21_pcie_bringup_functional_val.md), [Day 22 PCIe × PM intersection](prep_plan/day22_pcie_pm_intersection.md)). *(NVMe-specific day dropped from plan; pre-existing bug-story narrative is sufficient.)*

### 12 May — Intel Bangalore · IP Functional Validation (F2F, 2 rounds) — ✅ Selected — offer discussion in progress
- **What happened (R1, 2 engineers):** Msft/Intel walkthrough + how I narrow down a model regression. CPU core states — pushed on **negative scenarios** (e.g. requested C3 not achieved, peripheral blocking entry — what can happen, not specific). ISRs. Page table / page fault / MMU. C storage classes + usage. Program memory layout. C coding: **reverse a string using pointers, no extra variable**.
- **What happened (R2):** Explain **ADR flow** — how to decide *which data* needs to be copied (answered: maintain a table of regions / persistent-state descriptors). ISRs — *when* served, *where* the data is stored, what mechanism is used (context save / stack / vector table). Behavioural: why left Intel, why switching again; interviewer framed *"most work everywhere is prep activity, only 5-10% translates to CV"* — testing if I'd flinch. Each topic ran long.
- **Learning:** Negative-scenario framing for C-states (blocker → why entry denied → who owns the veto) is a recurring probe — needs a reflex chain. ADR "what gets copied" needs a crisp answer: persistent-region table + flush list + ordering, not hand-wavy. ISR storage answer needs the **hardware (vector table, stack, banked regs) + software (context frame)** split. Behavioural push on "prep vs CV work" needs an honest, non-defensive answer.
- **Suggestion:** Add C-state negative-path drill (peripheral veto, broadcast-block, LTR, demotion) to [Day 13](prep_plan/day13_cstates_pkgc_s0ix.md). Tighten ADR "copy table" framing in [Day 17 (post-Si PM val + ADR/C6)](prep_plan/day17_post_si_pm_validation.md). ISR context-save chain into [Day 03](prep_plan/day03_bits_mmio_volatile_polling.md). Reverse-string-in-place into [Day 00](prep_plan/day00_c_basics.md) reflex set. Rehearse "why switching" / "prep-vs-CV" answer in [Day 24 final mock](prep_plan/day24_final_mock.md) §5 behavioural STAR.

### 12 May — Qualcomm · CPU Core (Hiring Manager) — 🟢 Positive signal
- **What happened:** Basics on what I did and how, then mostly behavioural. Pushed on *no CPU-core experience — why hire you?* and *what value-add after joining?* (expecting "hard work + dedication" framing). Direct ask: *if switching for money, don't come to QC*. Asked about other offers — said Arm in discussion. *Arm vs QC?* → answered QC for engineering scope. Overall positive; they will combine all interview outputs.
- **Learning:** HM rounds at QC weight **intent + attitude** over depth — "hard work / dedication / scope" framing lands; money-led framing is an instant red flag. Cross-offer question is a real signal probe — answer with *fit reasoning*, not leverage.
- **Status:** Awaiting combined-loop decision.
- **Suggestion:** Lock in a 30-sec "why QC over Arm / why CPU core despite no prior CPU-core work" answer in [Day 24 final mock](prep_plan/day24_final_mock.md) §5 — value-add = validation rigor + cross-vendor system view + ownership pattern, not domain seniority.

### 13 May — Cerebras · R1 — ❌ Not fit for role
- **What happened:** (1) Cerebras-context warmup — what they do (wafer-scale engine, MemoryX/SwarmX). (2) Intel work walk-through — what + how. (3) **Test-plan design from scratch** for a device — took PCIe as the example and walked: spec → features → modes → stimulus + checks → coverage axes → negative + error injection → perf/power → silicon-vs-pre-Si split. (4) Microsoft NPU work walkthrough. (5) **Coding** — implement `grep` in Python (regex + file walk + flags); moving-average in C (circular buffer / running sum). Small issues on edge cases but recovered. (6) Closing chat on team-size expansion + responsibilities scope.
- **Learning:** Test-plan-from-spec is a recurring opener at AI-accelerator shops — having a reusable scaffold (spec → features → modes → stimulus/checks → coverage axes → negative → perf/power → pre-Si vs post-Si split) lands cleanly. Two-language coding (scripting + C) is becoming the norm — keep both reflexes warm. Edge-case discipline (empty input, single element, wrap-around in moving-avg, null pattern in grep) is what separated "works" from "clean."
- **Status:** ❌ **Closed 18 May — not fit for role.** No further rounds.
- **Suggestion:** Promote test-plan scaffold into [Day 17 post-Si PM val](prep_plan/day17_post_si_pm_validation.md) and [Day 11 CPU bring-up + functional val](prep_plan/day11_cpu_bringup_functional_val.md) as a **generic device test-plan template** (not just PM coverage). Add `grep`-clone + moving-average to [Day 00](prep_plan/day00_c_basics.md) reflex set. Drill edge-case pre-amble: *"before I code, the cases I'll handle are…"* in [Day 24 final mock](prep_plan/day24_final_mock.md) §5 default-answer shape.

### 14 May — AMD · Core Power & Performance Post-Si (R1) — 🟢 Positive
- **What happened:** (1) Walk through every prior role with a deep-dive at each — what I did, issues hit, how I unblocked. (2) Open-ended design probe: *how would I design an NPU + CPU + multi-SoC power/perf tradeoff?* — answered with workload-driven budget split, shared rail vs independent rail tradeoff, DVFS coordination across SoCs (single PMIC arbiter vs distributed), thermal-envelope sharing, and the validation-side coverage matrix that falls out. (3) Role framing from interviewer: **core PnP post-Si team — heavy execution + heavy debug after data lands; needs architecture exposure + system understanding.** Direct fit with my Intel PnP + MS NPU arc.
- **Learning:** Multi-SoC PnP design questions are the new shape at AMD/Cerebras-tier shops — the answer scaffold is **workload → budget split → rail topology → DVFS coordination → thermal envelope → validation matrix**, in that order. Role pitch ("execution + debug + architecture exposure") is exactly what my CV reads as — lean into it in the next round.
- **Status:** Awaiting next-round signal.
- **Suggestion:** Add **multi-SoC PnP design scaffold** to [Day 15](prep_plan/day15_thermal_power_perf_counters.md) (thermal envelope sharing) and [Day 12](prep_plan/day12_dvfs_pstates_pvt.md) (DVFS coordination across SoCs / shared vs independent rails). Add a one-liner answer to [Day 24 final mock](prep_plan/day24_final_mock.md) §5 on "design an NPU+CPU+multi-SoC PnP tradeoff" so it's a reflex.

---

##  Patterns

**Strong:** cross-vendor NPU experience, concrete bug stories with measurable impact, EdgeOptimizer/tooling narrative, high-level concept walks, **test-plan-from-spec scaffold**, **multi-SoC PnP design scaffold**, role-fit framing in HM/behavioural rounds.

**Still soft (watch):** layer-3 textbook depth under "probe-then-push", live-C edge-case discipline (mention cases *before* coding), EE fundamentals refresh, leadership/ownership framing under pace.

**Role-fit shift (mid-May):** the loop has converged on roles that match what I actually own — IP/platform functional validation, post-Si PnP, multi-SoC + accelerator power/perf. Intel Bangalore (✅ selected, offer in progress), AMD Core PnP Post-Si (🟢), Qualcomm CPU Core HM (🟢) all sit in this band. Mismatched shapes (pure DV / pre-Si, automotive protocol-heavy, Cerebras role-fit) are filtered out and no longer worth prepping for.

**Meta-skill being tested:** "probe-then-push" — *what → why → how → connected concept*. Bug stories and design-scaffold answers (test-plan, multi-SoC PnP) survive the push because they're structured derivations; flat-fact recall (textbook C-state ladder, cache hierarchy verbatim) doesn't. **Fix: every concept lives as a scaffold to walk, not a paragraph to recite.**

**Improving:** role-fit filtering (mismatch detected same-day, not after 3 rounds), behavioural framing (why-switching / prep-vs-CV / cross-offer answers now reflex), design-question scaffolds (test-plan, multi-SoC PnP) replacing ad-hoc answers.

**Strategic principle (revised for offer-stage):** with Intel offer in hand and 3 active loops in the aligned band, **prep narrows to the rounds actually scheduled**, not the full Day 0-21 spread. Drill: (1) the design scaffolds (test-plan, multi-SoC PnP) for AMD/Cerebras follow-ups, (2) C edge-case pre-amble for any coding round, (3) HM-style behavioural answers for QC combined-loop decision. Defend PCIe/NVMe and automotive at story depth only — don't grow new limbs.

---

## 🔁 Flywheel (after every round)

1. **Same day:** add row to timeline above — what happened, learning, status, suggestion.
2. **Within 24h:** map each "didn't land" to a Day file Architect Drill — add one if missing.
3. **Within 48h:** ensure the relevant Day file's drills + Exit Check cover the gap.
4. **Before next interview:** run [Day 24 final mock](prep_plan/day24_final_mock.md) flashcard sweep with the new gap-fill in rotation.

---

## 📝 End-of-cycle retro (fill when all feedback in)

- [ ] Top gap closed: _____
- [ ] Top gap still open: _____
- [ ] Best-fitting role: _____
- [ ] Companies to re-approach: _____
- [ ] Companies to deprioritise: _____
- [ ] One thing I'd do differently: _____

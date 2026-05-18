# Interview Log — Apr 2026 Cycle

> One row per round. Keep it light: what happened, what I learned, status, next action. Detailed prep frameworks live in [INTERVIEW_REVISION_GUIDE.md](INTERVIEW_REVISION_GUIDE.md) and the [21-day plan](21day_plan/).

---

## 📈 Cycle scoreboard (as of 16 May 2026)

**Total rounds: 15** across **8 companies** / **10 distinct role-loops**.

Round-by-round breakdown:

| Loop | Rounds | Detail |
|---|---|---|
| Intel · Power & Perf (Apr) | 2 | R1 + R2 — ❌ |
| Arm · Verification | 1 | 🟡 mismatch |
| Qualcomm · Pre-Si Verification | 1 | 🟡 mismatch |
| Qualcomm · CPU Core | 3 | R1 + R2 + HM — 🟢 awaiting combined |
| Qualcomm · PnP / Thermal (Hyderabad) | 2 | both rounds same day — 🟢 pending |
| Intel Ireland · NPU | 1 | rapid-fire 2 interviewers — 🟢 pending |
| Renesas | 1 | R1 — ❌ role-fit |
| Intel Bangalore · IP Functional Validation | 2 | F2F R1 + R2 — ✅ **Selected, offer in progress** |
| Cerebras | 1 | R1 — ❌ not fit for role |
| AMD · Core PnP Post-Si | 1 | R1 — 🟢 awaiting next |
| **Total** | **15** | |

Loop status:

| Bucket | Count | Loops |
|---|---|---|
| ✅ Selected (offer in progress) | **1** | Intel Bangalore |
| 🟢 Active / awaiting next or combined decision | **4** | QC CPU Core · QC PnP/Thermal · Intel Ireland NPU · AMD |
| ❌ Rejected / closed | **3** | Intel Power & Perf (Apr) · Renesas · Cerebras |
| 🟡 Mismatch — filtered upstream | **2** | Arm Verification · QC Pre-Si Verification |
| **Total loops** | **10** | |

**Conversion (loops, not rounds):** 5 / 10 landed (1 selected + 4 active strong-positive) = **50%**.

**Velocity:** 15 rounds over ~33 days (13 Apr → 16 May) ≈ **one round every 2.2 days**.

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

### 12 May — Intel Bangalore · IP Functional Validation (F2F, 2 rounds) — ✅ Selected — offer discussion in progress
- **What happened (R1, 2 engineers):** Msft/Intel walkthrough + how I narrow down a model regression. CPU core states — pushed on **negative scenarios** (e.g. requested C3 not achieved, peripheral blocking entry — what can happen, not specific). ISRs. Page table / page fault / MMU. C storage classes + usage. Program memory layout. C coding: **reverse a string using pointers, no extra variable**.
- **What happened (R2):** Explain **ADR flow** — how to decide *which data* needs to be copied (answered: maintain a table of regions / persistent-state descriptors). ISRs — *when* served, *where* the data is stored, what mechanism is used (context save / stack / vector table). Behavioural: why left Intel, why switching again; interviewer framed *"most work everywhere is prep activity, only 5-10% translates to CV"* — testing if I'd flinch. Each topic ran long.
- **Learning:** Negative-scenario framing for C-states (blocker → why entry denied → who owns the veto) is a recurring probe — needs a reflex chain. ADR "what gets copied" needs a crisp answer: persistent-region table + flush list + ordering, not hand-wavy. ISR storage answer needs the **hardware (vector table, stack, banked regs) + software (context frame)** split. Behavioural push on "prep vs CV work" needs an honest, non-defensive answer.
- **Suggestion:** Add C-state negative-path drill (peripheral veto, broadcast-block, LTR, demotion) to [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md). Tighten ADR "copy table" framing in [Day 14](21day_plan/day14_adr_pcu_core_c6_bugs.md). ISR context-save chain into [Day 03](21day_plan/day03_bits_mmio_volatile_polling.md) / [Day 09](21day_plan/day09_arm_qualcomm_platform_vocabulary.md). Reverse-string-in-place into [Day 00](21day_plan/day00_c_basics.md) reflex set. Rehearse "why switching" / "prep-vs-CV" answer in [Section 0A](INTERVIEW_REVISION_GUIDE.md).

### 12 May — Qualcomm · CPU Core (Hiring Manager) — 🟢 Positive signal
- **What happened:** Basics on what I did and how, then mostly behavioural. Pushed on *no CPU-core experience — why hire you?* and *what value-add after joining?* (expecting "hard work + dedication" framing). Direct ask: *if switching for money, don't come to QC*. Asked about other offers — said Arm in discussion. *Arm vs QC?* → answered QC for engineering scope. Overall positive; they will combine all interview outputs.
- **Learning:** HM rounds at QC weight **intent + attitude** over depth — "hard work / dedication / scope" framing lands; money-led framing is an instant red flag. Cross-offer question is a real signal probe — answer with *fit reasoning*, not leverage.
- **Status:** Awaiting combined-loop decision.
- **Suggestion:** Lock in a 30-sec "why QC over Arm / why CPU core despite no prior CPU-core work" answer in [Section 0A](INTERVIEW_REVISION_GUIDE.md) — value-add = validation rigor + cross-vendor system view + ownership pattern, not domain seniority.

### 13 May — Cerebras · R1 — ❌ Not fit for role
- **What happened:** (1) Cerebras-context warmup — what they do (wafer-scale engine, MemoryX/SwarmX). (2) Intel work walk-through — what + how. (3) **Test-plan design from scratch** for a device — took PCIe as the example and walked: spec → features → modes → stimulus + checks → coverage axes → negative + error injection → perf/power → silicon-vs-pre-Si split. (4) Microsoft NPU work walkthrough. (5) **Coding** — implement `grep` in Python (regex + file walk + flags); moving-average in C (circular buffer / running sum). Small issues on edge cases but recovered. (6) Closing chat on team-size expansion + responsibilities scope.
- **Learning:** Test-plan-from-spec is a recurring opener at AI-accelerator shops — having a reusable scaffold (spec → features → modes → stimulus/checks → coverage axes → negative → perf/power → pre-Si vs post-Si split) lands cleanly. Two-language coding (scripting + C) is becoming the norm — keep both reflexes warm. Edge-case discipline (empty input, single element, wrap-around in moving-avg, null pattern in grep) is what separated "works" from "clean."
- **Status:** ❌ **Closed 18 May — not fit for role.** No further rounds.
- **Suggestion:** Promote test-plan scaffold into [Day 20 Section 5a](21day_plan/day20_debug_method_bug_polish.md) as a **generic device test-plan template** (not just PM coverage). Add `grep`-clone + moving-average to [Day 00 Coding B](21day_plan/day00_c_basics.md) reflex set. Drill edge-case pre-amble: *"before I code, the cases I'll handle are…"* in [Section 8](INTERVIEW_REVISION_GUIDE.md).

### 14 May — AMD · Core Power & Performance Post-Si (R1) — 🟢 Positive
- **What happened:** (1) Walk through every prior role with a deep-dive at each — what I did, issues hit, how I unblocked. (2) Open-ended design probe: *how would I design an NPU + CPU + multi-SoC power/perf tradeoff?* — answered with workload-driven budget split, shared rail vs independent rail tradeoff, DVFS coordination across SoCs (single PMIC arbiter vs distributed), thermal-envelope sharing, and the validation-side coverage matrix that falls out. (3) Role framing from interviewer: **core PnP post-Si team — heavy execution + heavy debug after data lands; needs architecture exposure + system understanding.** Direct fit with my Intel PnP + MS NPU arc.
- **Learning:** Multi-SoC PnP design questions are the new shape at AMD/Cerebras-tier shops — the answer scaffold is **workload → budget split → rail topology → DVFS coordination → thermal envelope → validation matrix**, in that order. Role pitch ("execution + debug + architecture exposure") is exactly what my CV reads as — lean into it in the next round.
- **Status:** Awaiting next-round signal.
- **Suggestion:** Add **multi-SoC PnP design scaffold** to [Day 15](21day_plan/day15_thermal_power_perf_counters.md) (thermal envelope sharing) and [Day 12](21day_plan/day12_dvfs_pstates_pvt.md) (DVFS coordination across SoCs / shared vs independent rails). Add a one-liner answer to [Section 0A](INTERVIEW_REVISION_GUIDE.md) on "design an NPU+CPU+multi-SoC PnP tradeoff" so it's a reflex.

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

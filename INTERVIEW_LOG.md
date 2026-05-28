# Interview Log — Apr 2026 Cycle

> One row per round. Keep it light: what happened, what I learned, status, next action. Detailed prep frameworks live in the [25-day plan](prep_plan/) (Day 24 = final mock loop / flashcard sweep).

**Intel BLR joining date:** **27 July 2026** (confirmed). **Cycle closed 28 May 2026** — Intel offer accepted, all other loops rejected / stalled.

---

## 📈 Cycle scoreboard (as of 28 May 2026)

**Total rounds: 16** across **9 companies** / **12 distinct role-loops**.

Round-by-round breakdown:

| Loop | Rounds | Detail |
|---|---|---|
| Intel · Power & Perf (Apr) | 2 | R1 + R2 — ❌ rejected |
| Arm · Verification | 1 | 🟡 mismatch (filtered) |
| Qualcomm · Pre-Si Verification | 1 | 🟡 mismatch (filtered) |
| Qualcomm · CPU Core | 3 | R1 + R2 + HM — 🔴 stalled 16+ d |
| Qualcomm · PnP / Thermal (Hyderabad) | 2 | both rounds 28 Apr — 🔴 stalled, no reply since (30+ days) |
| Intel Ireland · NPU | 1 | rapid-fire 2 interviewers — ❌ rejected 21 May |
| Renesas | 1 | R1 — ❌ role-fit miss |
| Intel Bangalore · IP Functional Validation | 2 | F2F R1 + R2 — ✅ **Selected, offer accepted 21 May · joining 27 Jul** |
| Cerebras | 1 | R1 — ❌ not fit for role (closed 18 May) |
| AMD · Core PnP Post-Si | 1 | R1 (14 May) — 🔴 stalled, no reply since (14+ days) |
| **Nvidia · role TBD** | 0 | 🔴 Recruiter call 22 May — stalled, no scheduling reply (6+ days) |
| **Amazon Lab126 · AZ1 ML SoC (Echo full-Si val)** | 1 | R1 done 26 May — ❌ **Rejected 28 May** (HW-depth gap confirmed) |
| **Total** | **16** | |

Loop status:

| Bucket | Count | Loops |
|---|---|---|
| ✅ Selected (final offer) | **1** | Intel Bangalore (joining 27 Jul) |
| 🔴 Stalled — no reply (treat as closed) | **4** | QC CPU Core (16+ d) · QC PnP/Thermal (30+ d) · AMD (14+ d) · Nvidia (6+ d) |
| ❌ Rejected / closed | **5** | Intel Power & Perf (Apr) · Renesas · Cerebras · Intel Ireland NPU · Amazon Lab126 |
| 🟡 Mismatch — filtered upstream | **2** | Arm Verification · QC Pre-Si Verification |
| **Total loops** | **12** | |

**Conversion (loops, not rounds):** 1 / 12 landed = **8%** (Intel Bangalore, accepted). **Cycle is closed** — all other loops stalled / rejected / mismatched.

**Velocity:** 16 rounds over ~44 days (13 Apr → 26 May). Pipeline complete; Intel join proceeds 27 Jul.

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

### 28 Apr — Qualcomm · CPU Core (R1) — 🟢 Cleared → 🔴 loop stalled
- **What happened:** Pipeline stages, `a = b` data movement, MS NPU walkthrough. Coding solid.
- **Learning:** High-level concept + concrete project narrative lands cleanly.
- **Status:** Advanced to R2. Loop ultimately stalled after HM (12 May), no reply since.

### 28 Apr — Qualcomm · PnP / Thermal (Hyderabad, 2 rounds) — 🔴 Stalled (30+ days, no reply)
- **What happened:** Power/perf, thermal basics, system debug. Domain alignment + cross-vendor NPU breadth landed. Thermal control-loop depth and leadership/ownership framing slightly soft.
- **Learning:** Frame thermal as a control loop; tighten ownership STAR.
- **Status:** 🔴 Stalled — no feedback or next-round signal since 28 Apr. Treat as closed.
- **Suggestion:** Section 0A behavioural rehearsal; Day 15 control-loop framing.

### 29 Apr — Intel Ireland · NPU — ❌ Rejected (closed 21 May)
- **What happened:** Rapid-fire, 2 interviewers. ~70-80% landed. Cross-vendor NPU + EdgeOptimizer story strong. EE fundamentals weak; bug story tangled under pace.
- **Learning:** Slow ~10%; STAR-strict bug stories; refresh EE basics. Confirmed: EE-fundamentals weakness + tangled bug-story-under-pace is the failure mode that closes NPU-heavy loops — same pattern that cost Intel P&P Apr R2.
- **Status:** ❌ **Rejected 21 May.** No further rounds.
- **Suggestion:** 90-sec timer on Bug 2 / Bug 3; quick CMOS / setup-hold / CDC / SI refresh (Section 8). Keep EE basics in rotation — this is now a 2x-closed gap.

### 30 Apr — Qualcomm · CPU Core (R2) — 🟢 Advanced despite weak fundamentals
- **What happened:** "Probe-then-push" style — drilled C-state *why-a-ladder*, clock gating *how*, leakage as separate mechanism, cache hierarchy *why multi-level*, generic boot flow. CFC FIFO / Core C6 wake-loss bug story landed cleanly; textbook depth ran out at layer 2-3.
- **Learning:** Failure mode is shallow ownership, not pressure handling. Need to rehearse the *what → why → how → connected-concept* chain out loud, not just read it. Bug-story ownership + system breadth carried the round even when textbook depth ran out — signal that owned narrative compensates for layer-3 gaps.
- **Status:** Advanced to Hiring Manager (12 May). Loop ultimately stalled post-HM — see 12 May entry.
- **Suggestion:** Daily what/why/how/bug-hook chain on one concept, no notes.

### 07 May — Renesas · R1 — ❌ Rejected (role-fit miss)
- **What happened:** L&T PCIe/NVMe + MS NPU walkthrough flowed; PCIe/NVMe protocol depth rusty (5-6 yrs since daily use). Bug story itself landed.
- **Learning:** Role likely wanted automotive protocol fluency (CAN/LIN/FlexRay/AVB/SOME-IP) + live PCIe/NVMe — neither is current stack.
- **Suggestion:** Do **not** add automotive track. Filter Renesas-shape roles upstream. Keep PCIe/NVMe at bug-story + first-principles depth ([Day 18 LTSSM fundamentals](prep_plan/day18_pcie_fundamentals_ltssm.md), [Day 19 enumeration / config / MSI](prep_plan/day19_pcie_enumeration_config_msi.md), [Day 21 PCIe bring-up + functional val](prep_plan/day21_pcie_bringup_functional_val.md), [Day 22 PCIe × PM intersection](prep_plan/day22_pcie_pm_intersection.md)).

### 12 May — Intel Bangalore · IP Functional Validation (F2F, 2 rounds) — ✅ Selected · offer accepted 21 May · joining 27 Jul 2026
- **What happened (R1, 2 engineers):** Msft/Intel walkthrough + how I narrow down a model regression. CPU core states — pushed on **negative scenarios** (e.g. requested C3 not achieved, peripheral blocking entry — what can happen, not specific). ISRs. Page table / page fault / MMU. C storage classes + usage. Program memory layout. C coding: **reverse a string using pointers, no extra variable**.
- **What happened (R2):** Explain **ADR flow** — how to decide *which data* needs to be copied (answered: maintain a table of regions / persistent-state descriptors). ISRs — *when* served, *where* the data is stored, what mechanism is used (context save / stack / vector table). Behavioural: why left Intel, why switching again; interviewer framed *"most work everywhere is prep activity, only 5-10% translates to CV"* — testing if I'd flinch. Each topic ran long.
- **Learning:** Negative-scenario framing for C-states (blocker → why entry denied → who owns the veto) is a recurring probe — needs a reflex chain. ADR "what gets copied" needs a crisp answer: persistent-region table + flush list + ordering, not hand-wavy. ISR storage answer needs the **hardware (vector table, stack, banked regs) + software (context frame)** split. Behavioural push on "prep vs CV work" needs an honest, non-defensive answer.
- **Suggestion:** Add C-state negative-path drill (peripheral veto, broadcast-block, LTR, demotion) to [Day 13](prep_plan/day13_cstates_pkgc_s0ix.md). Tighten ADR "copy table" framing in [Day 17 (post-Si PM val + ADR/C6)](prep_plan/day17_post_si_pm_validation.md). ISR context-save chain into [Day 03](prep_plan/day03_bits_mmio_volatile_polling.md). Reverse-string-in-place into [Day 00](prep_plan/day00_c_basics.md) reflex set. Rehearse "why switching" / "prep-vs-CV" answer in [Day 24 final mock](prep_plan/day24_final_mock.md) Sec 5 behavioural STAR.

### 12 May — Qualcomm · CPU Core (Hiring Manager) — 🟢 Positive signal → 🔴 loop stalled
- **What happened:** Basics on what I did and how, then mostly behavioural. Pushed on *no CPU-core experience — why hire you?* and *what value-add after joining?* (expecting "hard work + dedication" framing). Direct ask: *if switching for money, don't come to QC*. Asked about other offers — said Arm in discussion. *Arm vs QC?* → answered QC for engineering scope. Overall positive; they will combine all interview outputs.
- **Learning:** HM rounds at QC weight **intent + attitude** over depth — "hard work / dedication / scope" framing lands; money-led framing is an instant red flag. Cross-offer question is a real signal probe — answer with *fit reasoning*, not leverage.
- **Status:** 🔴 Stalled — no combined-loop decision in 16+ days. Treat as closed.
- **Suggestion:** Lock in a 30-sec "why QC over Arm / why CPU core despite no prior CPU-core work" answer in [Day 24 final mock](prep_plan/day24_final_mock.md) Sec 5 — value-add = validation rigor + cross-vendor system view + ownership pattern, not domain seniority.

### 13 May — Cerebras · R1 — ❌ Not fit for role
- **What happened:** (1) Cerebras-context warmup — what they do (wafer-scale engine, MemoryX/SwarmX). (2) Intel work walk-through — what + how. (3) **Test-plan design from scratch** for a device — took PCIe as the example and walked: spec → features → modes → stimulus + checks → coverage axes → negative + error injection → perf/power → silicon-vs-pre-Si split. (4) Microsoft NPU work walkthrough. (5) **Coding** — implement `grep` in Python (regex + file walk + flags); moving-average in C (circular buffer / running sum). Small issues on edge cases but recovered. (6) Closing chat on team-size expansion + responsibilities scope.
- **Learning:** Test-plan-from-spec is a recurring opener at AI-accelerator shops — having a reusable scaffold (spec → features → modes → stimulus/checks → coverage axes → negative → perf/power → pre-Si vs post-Si split) lands cleanly. Two-language coding (scripting + C) is becoming the norm — keep both reflexes warm. Edge-case discipline (empty input, single element, wrap-around in moving-avg, null pattern in grep) is what separated "works" from "clean."
- **Status:** ❌ **Closed 18 May — not fit for role.** No further rounds.
- **Suggestion:** Promote test-plan scaffold into [Day 17 post-Si PM val](prep_plan/day17_post_si_pm_validation.md) and [Day 11 CPU bring-up + functional val](prep_plan/day11_cpu_bringup_functional_val.md) as a **generic device test-plan template** (not just PM coverage). Add `grep`-clone + moving-average to [Day 00](prep_plan/day00_c_basics.md) reflex set. Drill edge-case pre-amble: *"before I code, the cases I'll handle are…"* in [Day 24 final mock](prep_plan/day24_final_mock.md) Sec 5 default-answer shape.

### 14 May — AMD · Core Power & Performance Post-Si (R1) — 🟢 Positive → 🔴 loop stalled
- **What happened:** (1) Walk through every prior role with a deep-dive at each — what I did, issues hit, how I unblocked. (2) Open-ended design probe: *how would I design an NPU + CPU + multi-SoC power/perf tradeoff?* — answered with workload-driven budget split, shared rail vs independent rail tradeoff, DVFS coordination across SoCs (single PMIC arbiter vs distributed), thermal-envelope sharing, and the validation-side coverage matrix that falls out. (3) Role framing from interviewer: **core PnP post-Si team — heavy execution + heavy debug after data lands; needs architecture exposure + system understanding.** Direct fit with my Intel PnP + MS NPU arc.
- **Learning:** Multi-SoC PnP design questions are the new shape at AMD/Cerebras-tier shops — the answer scaffold is **workload → budget split → rail topology → DVFS coordination → thermal envelope → validation matrix**, in that order. Role pitch ("execution + debug + architecture exposure") is exactly what my CV reads as.
- **Status:** 🔴 Stalled — no next-round signal in 14+ days. Treat as closed.
- **Suggestion:** Add **multi-SoC PnP design scaffold** to [Day 15](prep_plan/day15_thermal_power_perf_counters.md) (thermal envelope sharing) and [Day 12](prep_plan/day12_dvfs_pstates_pvt.md) (DVFS coordination across SoCs / shared vs independent rails). Add a one-liner answer to [Day 24 final mock](prep_plan/day24_final_mock.md) Sec 5 on "design an NPU+CPU+multi-SoC PnP tradeoff" so it's a reflex.

### 26 May — Amazon Lab126 · AZ1 ML SoC (Echo full-Si val) — R1 — ❌ Rejected 28 May (HW-depth gap)
- **What happened:** Echo full silicon validation role. HW-bug-heavy — deep follow-ups on prior bug stories. Design probe 1: **"design a core C-state for a PC from scratch"** — entry/exit, residency, break-even, wake sources, demotion *why* (false-wake penalty, break-even violation) + *how* (governor weighting, HW-side promotion-block, LTR / peripheral blocker interaction). Design probe 2: **"create a test plan for an IP"** — walked the spec → features → modes → stimulus/checks → coverage axes → negative → perf/power → pre-Si vs post-Si scaffold (same shape that landed at Cerebras). Coding: **digital root** (`9875 → 29 → 11 → 2`).
- **Coding bug:** outer loop was `while (sum % 10)` — wrong (exits on any multiple of 10, e.g. `20 → 2` exits early). Correct: `while (sum >= 10)`. Termination-condition slip, same edge-case-preamble gap Cerebras flagged.
- **Learning:** Echo full-Si is HW-heavier than Intel BLR / AMD shape — they want board / lab / debug-fluency, not just architecture. C-state design-from-scratch is the **2nd design-scaffold probe** (after AMD multi-SoC PnP); scaffold = **entry triggers → eligibility → save/restore → power/clock action → wake sources → exit → demotion + governor feedback → measurement → validation matrix**.
- **Status:** ❌ **Rejected 28 May.** Soft-negative signal from R1 confirmed — mismatch was structural (CV reads architecture + system-val, role wanted board / lab / HW-fluency for Echo full-Si). No further rounds.
- **Suggestion:** Add C-state design-from-scratch scaffold to [Day 13](prep_plan/day13_cstates_pkgc_s0ix.md). Add digital-root + verbalise-loop-invariant to [Day 00](prep_plan/day00_c_basics.md). For any follow-up: lean into L&T board bring-up + Intel post-Si lab debug to close HW-exposure perception.

---

##  Patterns

**Strong:** bug stories with measurable impact, cross-vendor NPU breadth, **design-from-scratch scaffolds** (test-plan, multi-SoC PnP, C-state), role-fit framing in HM rounds.

**Still soft:** layer-3 depth under probe-then-push, live-C **terminate-condition + edge-case pre-amble** (Cerebras + Amazon both), EE fundamentals, **HW-exposure breadth** for board/lab-fluency roles (Amazon Echo signal).

**Meta-skill:** probe-then-push — *what → why → how → connected*. Scaffolds survive the push; flat-fact recall doesn't. Every concept = scaffold to walk, not paragraph to recite.

**Strategic principle (cycle-closed):** Intel offer accepted, joining 27 Jul. No live loops. Carry-forward gaps for future cycles: (1) design-from-scratch scaffolds (test-plan, multi-SoC PnP, C-state — lock as reflex), (2) C edge-case + loop-invariant pre-amble (Cerebras + Amazon both flagged), (3) HW-fluency framing (L&T board bring-up + Intel post-Si lab) when role reads HW-heavy, (4) EE fundamentals refresh — 2x-closed gap (Intel P&P Apr + Intel Ireland NPU).

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

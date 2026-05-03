# Interview Log — Apr 2026 Cycle

> Master record of each interview: format, questions, what went well, what didn't, outcome, lessons. Update after every round, same day.

---

## 📅 Timeline (chronological)

### 🟥 13 Apr 2026 — Intel · Power & Performance

| Field | Detail |
|---|---|
| **Format** | 2 rounds (technical) |
| **Role** | Power & Performance Validation |

**Round 1:** Decent / acceptable performance.

**Round 2 — what didn't land:**
- ❌ Could not write a struct-based C program cleanly under pressure
- ❌ Confusion in **Package C-state (PkgC)** entry/exit flow
- ❌ Explained C-state mechanics but missed the **why** (break-even, residency, wake cost)

**Outcome:** ❌ Rejected.
**Signal:** Fundamentals + live-coding-under-pressure gap. Knew the *what*, missed the *why*.

**Action items closed since:**
- ✅ Day 13 Architect Drills now frame C-state ladder as *break-even residency vs entry/exit cost* — the "why" answer that was missing
- ✅ C struct/pointer drills back in rotation (Day 1-2)

---

### 🟡 15 Apr 2026 — Arm · Verification

| Field | Detail |
|---|---|
| **Format** | 1 screening round |
| **Role** | SystemVerilog / UVM Verification |

- Questions aligned to SV/UVM-heavy DV role.
- Role not aligned with my background (validation, not pure DV).

**Outcome:** 🟡 Mismatch — not pursued.
**Signal:** Role-filtering improving — recognised the misfit early instead of forcing it.

---

### 🟡 20 Apr 2026 — Qualcomm · Pre-Silicon Verification

| Field | Detail |
|---|---|
| **Format** | 1 screening round |
| **Role** | Pre-Si Verification (DV-heavy) |

- Same shape as the Arm role: verification-heavy, not validation.
- Did not pursue.

**Outcome:** 🟡 Mismatch.
**Signal:** Continuing to filter generic DV roles in favour of validation/system roles.

---

### 🟢 28 Apr 2026 — Qualcomm · CPU Core (Round 1)

| Field | Detail |
|---|---|
| **Format** | 1 round, technical + work walkthrough |
| **Role** | CPU Core Validation |

**Questions:**
- CPU pipeline stages
- Instruction/data movement walkthrough (e.g., `a = b`)
- Current work at Microsoft

**What went well:**
- ✅ Coding solid
- ✅ High-level CPU concepts handled cleanly
- ✅ Strong explanation of current Microsoft NPU work

**Outcome:** 🟢 **Cleared Round 1 → Round 2 scheduled (30 Apr).**
**Signal:** Strong at high-level conceptual + concrete project discussion. Pipeline depth was sufficient at this gate.

---

### 🟢 28 Apr 2026 — Qualcomm · PnP / Thermal (Hyderabad, both rounds)

| Field | Detail |
|---|---|
| **Format** | 2 rounds, same day |
| **Role** | Power & Performance / Thermal Validation |

**Questions:**
- Power / perf concepts
- Thermal basics
- System-level debugging

**What went well:**
- ✅ Overall good performance — domain alignment with current work
- ✅ Cross-vendor breadth landed

**What didn't land:**
- ⚠️ Some thermal depth gaps (control-loop framing, throttle-reason MSRs)
- ⚠️ Leadership/ownership answers slightly unclear under behavioural drill

**Outcome:** 🟢 Both rounds completed — **feedback pending**.
**Signal:** Strong domain fit; behavioural framing needs sharpening.

**Action items closed since:**
- ✅ Day 15 Architect Drills now frame thermal as a *control loop* (negative feedback breaking the leakage runaway)
- ✅ Section 0A behavioural rehearsal list updated for ownership/leadership questions

---

### 🟢 29 Apr 2026 — Intel Ireland · NPU

| Field | Detail |
|---|---|
| **Format** | Mixed, 2 interviewers, rapid-fire |
| **Role** | NPU Validation |

**What went well:**
- ✅ ~70–80% of questions handled well
- ✅ Cross-vendor NPU experience landed (LNL / Strix / X Elite)
- ✅ EdgeOptimizer + tooling story landed

**What didn't land:**
- ⚠️ Electronics fundamentals weak (transistor-level, signal integrity)
- ⚠️ Bug story got tangled — too many threads, lost the STAR shape
- ⚠️ Some pressure moments where pace ran ahead of clarity

**Outcome:** 🟢 **Awaiting next steps / feedback** — likely strongest overall round so far.
**Signal:** NPU domain owned; tighten bug-story polish and basic EE refresh.

**Action items:**
- [ ] Rehearse Bug 2 (QNN context-cache) and Bug 3 (silent EP fallback) tighter — strict STAR, 90 sec each (Section 4A)
- [ ] Quick EE-fundamentals refresh: setup/hold, metastability, CDC, basic CMOS, signal integrity at PCIe rates (Interview Revision Guide Section 8)
- [ ] Slow the pace 10% — clarity > speed under rapid-fire

---

### 🟥 30 Apr 2026 — Qualcomm · CPU Core (Round 2)

| Field | Detail |
|---|---|
| **Format** | Deep fundamentals + behavioural |
| **Role** | CPU Core Validation |

**Interviewer pattern (read in hindsight):**
The interviewer was running a **"probe-then-push"** style — pick a term I introduced, then keep drilling deeper to see whether I owned it or just knew the keyword. She was not testing breadth; she was testing **whether I could keep generating the next layer of *why* under sustained pressure**. The drill chain ran like this:

1. Started with **C fundamentals** as a warm-up / floor-check
2. I mentioned **C-states** → push: *"why do we need multiple C-states?"* (not "list them" — *justify the ladder*)
3. I used the term **clock gating** → push: *"how is it done?"* (not "what is it" — implementation depth)
4. Even after the clock-gating answer, she pivoted: *"and leakage?"* — testing whether I could connect the dots between gating, dynamic power, and leakage as **separate loss mechanisms**
5. Finally asked for a **bug story** → I explained the **CFC FIFO / Core C6 wake-loss bug clearly and confidently**

The pattern is unmistakable: she was probing **thinking capability under depth pressure**, not knowledge breadth. Every push was *"go one layer deeper than your last answer."*

**What went well:**
- ✅ **Bug story landed cleanly** — CFC FIFO / Core C6 wake-loss explained end-to-end (setup → repro → root cause → fix → coverage). This is what I own most confidently and it showed.
- ✅ Current work explained in depth
- ✅ Cross-vendor NPU framing landed

**What didn't land — and why (in hindsight):**
- ❌ **C-states "why a ladder"** — I had the *what* (list of states) but not the *why* (break-even residency vs entry/exit cost). The push exposed that I'd memorised the ladder, not derived it.
- ❌ **Clock gating "how"** — I knew it gates the clock; I couldn't go down to ICG cells, glitch-prevention, or fine-vs-coarse-grain insertion. One layer shallow.
- ❌ **Leakage** (the follow-up to clock gating) — couldn't connect that clock gating kills *dynamic* but leakage is a separate mechanism with its own formula and temperature dependence.
- ❌ **Cache hierarchy + internals** — knew the layers, couldn't justify *why multi-level* or walk *how lookup happens*.
- ❌ **Boot flow** — mixed Snapdragon specifics with generic flow.

**The real diagnosis:**
This wasn't "I don't know the material" — it was **"I haven't trained the muscle of generating the next *why* on demand."** The fundamentals were there at first-layer depth; under push for second/third-layer reasoning, I ran out of pre-built framing. **The interviewer's instinct was right** — she was probing thinking capability, and the cracks that showed are the exact gap a non-CPU-core background creates: I have the validator's lens, not the architect's lens.

**Outcome:** 🔴 Likely reject for CPU core specifically.
**Signal:** Two distinct skills exposed:
1. **Knowledge depth** — gap, but closeable (the gap-fills below now exist).
2. **Thinking capability under sustained depth-push** — the meta-skill she was actually testing. This needs *practice rehearsing the chain*, not just reading the answers.

**Silver lining:** the bug story landed cleanly under the same pressure. That tells me **I *can* think on my feet when I own the material end-to-end** — the failure mode is shallow ownership, not pressure handling. The fix is making the rest of the syllabus as deeply owned as the CFC bug already is.

**Action items closed since (gap-fill done in repo):**
- ✅ [Day 04](21day_plan/day04_cache_lines_locality_amat.md) — cache hierarchy "why", lookup walk, SRAM/DRAM as a story not a fact
- ✅ [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md) — static vs dynamic power formulas; clock gating end-to-end (RTL → ICG → tiers → verification); break-even residency framing for C-state ladder
- ✅ [Day 09](21day_plan/day09_arm_qualcomm_platform_vocabulary.md) — vendor-agnostic 5-phase boot flow, with vendor-mapping table
- ✅ Technical days now carry **Architect Drills** with tradeoff-first constraint hints
- ✅ Interview revision guide [Section 0B](INTERVIEW_REVISION_GUIDE.md#section-0b---current-interview-intersections) maps this cycle's actual interview intersections to guide sections and day files
- ✅ Interview revision guide [Section 4B](INTERVIEW_REVISION_GUIDE.md#section-4b---21-day-architect-drill-index) consolidates architect-drill constraints from Day 0 + Days 1-21

---

## 📊 Snapshot

| Date | Company | Role | Outcome | Status |
|---|---|---|---|---|
| 13 Apr | Intel | Power & Perf | ❌ Rejected | Closed |
| 15 Apr | Arm | Verification | 🟡 Mismatch | Closed |
| 20 Apr | Qualcomm | Pre-Si Verification | 🟡 Mismatch | Closed |
| 28 Apr | Qualcomm | CPU Core (R1) | 🟢 Cleared | Advanced to R2 |
| 28 Apr | Qualcomm | PnP Thermal | 🟢 Good | Feedback pending |
| 29 Apr | Intel Ireland | NPU | 🟢 Strong | Feedback pending |
| 30 Apr | Qualcomm | CPU Core (R2) | 🔴 Weak fundamentals | Outcome pending |

---

## 🎯 One-line arc

> Misaligned roles → strong domain interviews → progression now bottlenecked only by **deep fundamentals (the *why* behind concepts)**, live-C fluency, and rapid-fire story discipline. The 21-day plan, Day 0 C ramp, Interview Revision Guide Section 0B, and Section 4B are now designed to close exactly those intersections.

---

## 🧩 Interview intersections → revision map

> This is the operating bridge between this log and the revision guide. Each row says: this came up in an interview, this is what it means, and this is where to rehearse it.

| Interview intersection | Seen in | Signal | Revision guide anchor | 21-day source | Next rehearsal |
|---|---|---|---|---|---|
| C under pressure | Intel PnP R2; general screens | C syntax/pointers must be automatic before silicon questions start | [Section 0B](INTERVIEW_REVISION_GUIDE.md#section-0b---current-interview-intersections), [Section 9](INTERVIEW_REVISION_GUIDE.md#section-9---quick-technical-reflexes) | [Day 00](21day_plan/day00_c_basics.md), [Day 01](21day_plan/day01_c_address_fluency.md), [Day 02](21day_plan/day02_pointers_memory_layout_alignment.md) | One Day 0 C exercise daily until warning-free typing feels boring |
| The "why" behind concepts | Qualcomm CPU Core R2 | Senior-bar probe is derivation, not recall | [Section 0B](INTERVIEW_REVISION_GUIDE.md#section-0b---current-interview-intersections), [Section 4B](INTERVIEW_REVISION_GUIDE.md#section-4b---21-day-architect-drill-index) | [Day 04](21day_plan/day04_cache_lines_locality_amat.md), [Day 12](21day_plan/day12_dvfs_pstates_pvt.md), [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md), [Day 16](21day_plan/day16_pipeline_memory_consistency_simd.md) | Speak one what/why/how/bug-hook chain daily |
| Power/PnP: C-states, clock gating, leakage | Intel PnP; Qualcomm CPU Core R2 | Needs mechanism depth: break-even, ICG cells, dynamic vs leakage | [Sections 3-5](INTERVIEW_REVISION_GUIDE.md#section-3---thermal--power-behavior-backup-if-asked), [Section 9](INTERVIEW_REVISION_GUIDE.md#section-9---quick-technical-reflexes) | [Day 12](21day_plan/day12_dvfs_pstates_pvt.md), [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md), [Day 15](21day_plan/day15_thermal_power_perf_counters.md) | Draw C-state entry/exit and clock-gating path from memory |
| Cache hierarchy internals | Qualcomm CPU Core R2 | Know not just L1/L2/L3, but why hierarchy and how lookup works | [Section 4B](INTERVIEW_REVISION_GUIDE.md#section-4b---21-day-architect-drill-index) | [Day 04](21day_plan/day04_cache_lines_locality_amat.md), [Day 05](21day_plan/day05_coherence_mesi_false_sharing.md) | Derive cache hierarchy from latency/capacity/area constraints |
| Generic boot flow | Qualcomm CPU Core R2 | Separate universal boot phases from Snapdragon-specific names | [Section 0A](INTERVIEW_REVISION_GUIDE.md#section-0a---day-of-interview-prep), [Section 9](INTERVIEW_REVISION_GUIDE.md#section-9---quick-technical-reflexes) | [Day 09](21day_plan/day09_arm_qualcomm_platform_vocabulary.md) | Say POWER → ROM → PLATFORM INIT → BOOTLOADER → OS first, then vendor map |
| NPU domain + EdgeOptimizer | Intel Ireland NPU; Qualcomm R1/R2 current-work probes | Strong differentiator; keep honest platform-validation framing | [Section 1](INTERVIEW_REVISION_GUIDE.md#section-1---career-narrative-own-this-cold), [Section 6](INTERVIEW_REVISION_GUIDE.md#section-6---npu-post-silicon-validation-your-current-ms-work--only-if-pivoted) | [Day 18](21day_plan/day18_npu_ort_ep_hexagon_htp.md), [Day 19](21day_plan/day19_npu_memory_runtime_bugs.md) | Lead with six-layer stack + per-inference attribution |
| Bug story under rapid-fire | Intel Ireland NPU | Content is strong; delivery tangled when too many threads open | [Section 4A](INTERVIEW_REVISION_GUIDE.md#section-4a---bug-story-bank-pick-by-lens-rehearse-the-top-3) | [Day 20](21day_plan/day20_debug_method_bug_polish.md), [Day 21](21day_plan/day21_final_mock_company_tailoring.md) | 90-sec timer: context, symptom, repro, root cause, fix, lesson |
| EE fundamentals | Intel Ireland NPU | Small gap, but can affect senior-IC signal | [Section 8](INTERVIEW_REVISION_GUIDE.md#section-8---ee-fundamentals-crash-refresh) | [Day 10](21day_plan/day10_pcie_fundamentals_ltssm.md), [Day 16](21day_plan/day16_pipeline_memory_consistency_simd.md) | Setup/hold, CDC, CMOS, PCIe SI one-liners |
| Leadership / ownership | Qualcomm PnP; senior loops | Need clearer STAR and ownership boundaries | [Section 2](INTERVIEW_REVISION_GUIDE.md#section-2---behavioral--leadership-questions-senior-bar), [Section 10](INTERVIEW_REVISION_GUIDE.md#section-10---final-10-min-checklist) | [Day 20](21day_plan/day20_debug_method_bug_polish.md), [Day 21](21day_plan/day21_final_mock_company_tailoring.md) | One real example per framework, no generic-only answers |

---

## 🔮 Possible outcomes per active role

> For every pending role: best case, likely case, worst case, and **what I should do regardless of outcome**.
>
> **Public-data sanity check — 03 May 2026:** internet data cannot verify my private interview result or recruiter decision. It can only verify whether the role family appears active and whether the public JD matches the interview signal. The probabilities below are therefore **role-fit priors**, not outcome facts. Recruiter feedback overrides this section.

### Public role-family evidence checked

| Pipeline | Public evidence found | Effect on prediction |
|---|---|---|
| **Intel Ireland — NPU** | Exact official Intel req active: [Embedded Systems Engineer — Post-Silicon Validation (NPU), JR0281979](https://intel.wd1.myworkdayjobs.com/external/job/Ireland-Leixlip/Embedded-Systems-Engineer---Post-Silicon-Validation--NPU-_JR0281979). Adjacent official pre-Si AI accelerator req also active: [JR0282957](https://intel.wd1.myworkdayjobs.com/en-US/External/job/Pre-Silicon-Verification-Engineer--AI-Accelerator-_JR0282957). | Strongly supports that the NPU pipeline is real and still hiring. Slightly improves "advance" prior; keeps EE fundamentals as the risk. |
| **Qualcomm — PnP / Thermal Hyderabad** | Public Hyderabad power/thermal role-family evidence exists: [Power and Thermal Engineer, Staff - LPM](https://in.linkedin.com/jobs/view/power-and-thermal-engineer-staff-lpm-at-qualcomm-4330255174), plus PPAT/server power-thermal listings such as [Qualcomm Engineer — PPAT Hyderabad](https://in.linkedin.com/jobs/view/engineer-at-qualcomm-4372529242). Qualcomm official careers also shows active India thermal/limits hiring, though one official listing found was Bangalore: [Thermal and Limits Management, Power Performance Engineer, Staff](https://careers.qualcomm.com/careers/job/446704974927). | Supports active role-family demand. No reason to reduce PnP odds; still watch seniority/leadership signal. |
| **Qualcomm — CPU Core R2** | Public exact "CPU Core Validation Hyderabad" evidence was not found. Public evidence points more clearly to adjacent Qualcomm CPU/PPAT/power/server roles than to this exact CPU-core validation loop. | Reduces confidence in CPU-core-specific positive outcome; increases probability of adjacent-team pivot if recruiter keeps profile warm. |

### 🟢 Qualcomm — PnP / Thermal (Hyderabad)

| Outcome | Probability | What it looks like |
|---|---|---|
| **Best:** offer at target band | ~30% | Domain fit + cross-vendor NPU + thermal depth was enough; behavioural softness forgiven |
| **Likely:** advance to next round (manager or director) | ~45% | Tech rounds passed; one more loop to confirm leadership framing |
| **Possible:** downlevel offer | ~15% | Tech accepted but seniority concerns from behavioural unclarity |
| **Worst:** reject | ~10% | Thermal-depth gap weighed more than cross-vendor breadth |

**Regardless of outcome:**
- Sharpen leadership/ownership answers using Section 2 frameworks (lead-team-of-15, underperforming teammate, conflict)
- Internalise Day 15 thermal-as-control-loop framing for any future PnP round

---

### 🟢 Intel Ireland — NPU

| Outcome | Probability | What it looks like |
|---|---|---|
| **Best:** advance to manager/HM round | ~45% | Strongest performance so far; exact public NPU post-Si req is active; NPU/tooling fit recognised |
| **Likely:** mixed feedback, advance with reservations on EE fundamentals | ~35% | Clear NPU strength; ask follow-up questions on basics |
| **Possible:** advance but downlevel discussion opens | ~15% | EE gap reads as IC-level rather than senior-IC |
| **Worst:** reject on EE fundamentals + bug-story polish | ~5% | Pace + tangled story under pressure tipped against despite role fit |

**Regardless of outcome:**
- Tighten bug-story bank to STAR-strict 90 sec (Section 4A — Bug 2, Bug 3 specifically)
- Schedule 1-week EE-fundamentals refresh: CMOS, setup/hold, CDC, signal integrity
- Even if accepted, **negotiate role scope** to ensure NPU work is the bulk, not adjacent

---

### 🔴 Qualcomm — CPU Core (Round 2)

| Outcome | Probability | What it looks like |
|---|---|---|
| **Best:** advance with caveats / take-home | ~5% | Current-work depth + bug stories carried enough to overlook fundamentals gap |
| **Possible:** downlevel / pivot to PnP, PPAT, or platform team instead of CPU core | ~30% | Public data supports adjacent Qualcomm power/thermal/PPAT role-family demand more clearly than exact CPU-core validation |
| **Likely:** reject for CPU core, kept warm for adjacent role | ~45% | Honest signal: CPU core needs deeper micro-arch fluency than I have natively |
| **Worst:** full reject across Qualcomm pipeline | ~20% | Round 2 weakness colours the PnP loop too — *coordinate recruiter messaging carefully* |

**Regardless of outcome:**
- Treat the gap as the syllabus, not the verdict — the 21-day plan now covers it
- If rejected, **request specific feedback** (which fundamentals were weakest) and use it as next-cycle prep
- If pivoted to PnP, lean into the PnP/Thermal interview already in flight; present as one coherent Qualcomm story

---

## 🧭 Patterns across the cycle

### What I'm consistently strong at
- Cross-vendor NPU experience (Intel/AMD/Qualcomm) — lands in every round
- Concrete bug stories with measurable impact
- Tooling/EdgeOptimizer narrative — differentiator at the senior IC bar
- High-level concept walks (when not pressed for the *why*)

### What I'm consistently weak at
- **The "why" behind well-known concepts** (cache hierarchy, C-states, power) — this is the recurring gap; addressed in [Section 0B](INTERVIEW_REVISION_GUIDE.md#section-0b---current-interview-intersections), [Section 4B](INTERVIEW_REVISION_GUIDE.md#section-4b---21-day-architect-drill-index), and Architect Drills in each technical day file
- **Live coding under pressure** — particularly C structs / pointer-heavy
- **Bug-story discipline under rapid-fire** — answers tangle when interviewer pace is fast
- **EE fundamentals** — transistor-level, CDC, signal integrity (relevant for pre-Si rounds)
- **Leadership framing** — "tell me about a time" answers occasionally lose framework

### The meta-skill being tested (recognised after 30 Apr Qualcomm R2)

**"Probe-then-push" interview style:** senior interviewers — particularly at vendor pre-Si CPU teams — pick a term *I* introduce and keep drilling one layer deeper until either I run out of *why* or they're satisfied. The chain is always:

> *what* → *why* → *how* → *and the related concept I haven't named yet?*

**My pattern of failure under this:** first-layer answers fluent; second-layer thin; third-layer empty. I have the validator's lens (find the bug, characterise the silicon), not yet the architect's lens (derive *why the design is shaped this way*). The bug story always lands because there I own the chain end-to-end; the textbook concepts don't because I learned them as facts, not as derivations.

**The fix is structural, not study-more:**
- Every technical day in the [21-day plan](21day_plan/) now carries **Architect Drills** that lead with the *tradeoff* (the *why*), not the fact (the *what*) — but **reading them is not the same as rehearsing the chain out loud**.
- Practice drill: pick a concept, force myself to answer *what / why / how / what-it-connects-to* in 4 sentences without notes. If any sentence stalls, that's the layer to revise.
- The CFC bug worked under the same push because I've told it 30+ times. Every fundamentals topic needs that same repetition to reach reflex depth.

### What's improving (visible across the cycle)
- Role-fit filtering: 13 Apr → forced misfit; 15/20 Apr → recognised early; 28 Apr → only pursuing aligned roles
- Domain-fit recognition: PnP/NPU/system rounds now landing; pure DV/CPU-micro-arch rounds revealing the boundary
- Self-diagnosis sharpness: every weak round now produces specific action items (visible in this log)

---

## 🔁 Interview-prep flywheel (what to do after every round)

1. **Same day:** add to this log under the date — questions, what landed, what didn't, signal
2. **Within 24h:** map each "didn't land" to a day file in the [21-day plan](21day_plan/) — does an Architect Drill cover the constraint? If not, add one.
3. **Within 48h:** does the [Interview Revision Guide](INTERVIEW_REVISION_GUIDE.md) one-liner or intersection row exist for the gap? If not, add it to Section 0B / 4B / 8 as appropriate.
4. **Before next interview:** run the [Section 0A](INTERVIEW_REVISION_GUIDE.md#section-0a---day-of-interview-prep) day-of prep, with the new gap-fill in rotation.
5. **End of cycle (when all feedback in):** retro section below.

---

## 📝 End-of-cycle retro (fill when all feedback received)

- [ ] **Top gap closed during the cycle:** _____
- [ ] **Top gap still open:** _____
- [ ] **Best-fitting role going forward:** _____
- [ ] **Companies to re-approach in next cycle:** _____
- [ ] **Companies to deprioritise:** _____
- [ ] **One thing I'd do differently next cycle:** _____

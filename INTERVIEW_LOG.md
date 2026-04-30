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
- ✅ Day 13 system-thinking section now frames C-state ladder as *break-even residency vs entry/exit cost* — the "why" answer that was missing
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
- ✅ Day 15 system-thinking section now frames thermal as a *control loop* (negative feedback breaking the leakage runaway)
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
- [ ] Quick EE-fundamentals refresh: setup/hold, metastability, CDC, basic CMOS, signal integrity at PCIe rates
- [ ] Slow the pace 10% — clarity > speed under rapid-fire

---

### 🟥 30 Apr 2026 — Qualcomm · CPU Core (Round 2)

| Field | Detail |
|---|---|
| **Format** | Deep fundamentals + behavioural |
| **Role** | CPU Core Validation |

**What went well:**
- ✅ Current work explained in depth, with strong concrete bug stories
- ✅ Cross-vendor NPU framing landed

**What didn't land:**
- ❌ **Cache hierarchy + internals** — knew the layers, couldn't explain *why* multi-level / *how* lookup happens / SRAM vs DRAM tradeoff
- ❌ **Power concepts** — dynamic vs leakage formulas, clock gating mechanism
- ❌ **C-states reasoning** — could list states, couldn't justify *why a ladder* (break-even logic)
- ❌ **Boot flow** — unclear, mixed Snapdragon-specifics with generic flow

**Outcome:** 🔴 Likely reject or downlevel.
**Signal:** Deep fundamentals gap exposed. Knew the names; didn't own the *tradeoffs that justify them*.

**Action items closed since (gap-fill done in repo):**
- ✅ [Day 04](21day_plan/day04_cache_lines_locality_amat.md) — cache hierarchy "why", lookup walk, SRAM/DRAM as a story not a fact
- ✅ [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md) — static vs dynamic power formulas; clock gating end-to-end (RTL → ICG → tiers → verification); break-even residency framing for C-state ladder
- ✅ [Day 09](21day_plan/day09_arm_qualcomm_platform_vocabulary.md) — vendor-agnostic 5-phase boot flow, with vendor-mapping table
- ✅ All 21 days now carry **System Thinking** sections with tradeoff-first framing
- ✅ Interview revision guide [Section 4B](INTERVIEW_REVISION_GUIDE.md) consolidates one-liners from all 20 days

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

> Misaligned roles → strong domain interviews → progression now bottlenecked only by **deep fundamentals (the *why* behind concepts)**, which is the gap the 21-day plan + System Thinking sections + Interview Revision Guide Section 4B are now designed to close.

---

## 🔮 Possible outcomes per active role

> For every pending role: best case, likely case, worst case, and **what I should do regardless of outcome**.

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
| **Best:** advance to manager/HM round | ~40% | Strongest performance so far; NPU domain ownership recognised |
| **Likely:** mixed feedback, advance with reservations on EE fundamentals | ~35% | Clear NPU strength; ask follow-up questions on basics |
| **Possible:** advance but downlevel discussion opens | ~15% | EE gap reads as IC-level rather than senior-IC |
| **Worst:** reject on EE fundamentals + bug-story polish | ~10% | Pace + tangled story under pressure tipped against |

**Regardless of outcome:**
- Tighten bug-story bank to STAR-strict 90 sec (Section 4A — Bug 2, Bug 3 specifically)
- Schedule 1-week EE-fundamentals refresh: CMOS, setup/hold, CDC, signal integrity
- Even if accepted, **negotiate role scope** to ensure NPU work is the bulk, not adjacent

---

### 🔴 Qualcomm — CPU Core (Round 2)

| Outcome | Probability | What it looks like |
|---|---|---|
| **Best:** advance with caveats / take-home | ~10% | Current-work depth + bug stories carried enough to overlook fundamentals gap |
| **Possible:** downlevel offer to PnP/platform team instead of CPU core | ~25% | Recruiter pivots me to Qualcomm PnP loop (28 Apr) where I'm a stronger fit |
| **Likely:** reject for CPU core, kept warm for adjacent role | ~40% | Honest signal: CPU core needs deeper micro-arch fluency than I have natively |
| **Worst:** full reject across Qualcomm pipeline | ~25% | Round 2 weakness colours the PnP loop too — *coordinate recruiter messaging carefully* |

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
- **The "why" behind well-known concepts** (cache hierarchy, C-states, power) — this is the recurring gap; closed in [Section 4B](INTERVIEW_REVISION_GUIDE.md) of the revision guide and System Thinking sections of every day file
- **Live coding under pressure** — particularly C structs / pointer-heavy
- **Bug-story discipline under rapid-fire** — answers tangle when interviewer pace is fast
- **EE fundamentals** — transistor-level, CDC, signal integrity (relevant for pre-Si rounds)
- **Leadership framing** — "tell me about a time" answers occasionally lose framework

### What's improving (visible across the cycle)
- Role-fit filtering: 13 Apr → forced misfit; 15/20 Apr → recognised early; 28 Apr → only pursuing aligned roles
- Domain-fit recognition: PnP/NPU/system rounds now landing; pure DV/CPU-micro-arch rounds revealing the boundary
- Self-diagnosis sharpness: every weak round now produces specific action items (visible in this log)

---

## 🔁 Interview-prep flywheel (what to do after every round)

1. **Same day:** add to this log under the date — questions, what landed, what didn't, signal
2. **Within 24h:** map each "didn't land" to a day file in the [21-day plan](21day_plan/) — does the System Thinking section cover it? If not, add it.
3. **Within 48h:** does the [Interview Revision Guide](INTERVIEW_REVISION_GUIDE.md) one-liner exist for the gap? If not, add to Section 4B.
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

# Interview Log — Apr 2026 Cycle

> One row per round. Keep it light: what happened, what I learned, status, next action. Detailed prep frameworks live in the [25-day plan](prep_plan/) (Day 24 = final mock loop / flashcard sweep).

**Intel BLR joining date:** **27 July 2026** (confirmed). **Cycle reopened 1 Jun 2026** for separate Qualcomm CPU post-silicon and AMD CPU post-silicon follow-up loops; evaluate only after written offer / level / scope are clear.

---

## 📈 Cycle scoreboard (as of 11 Jun 2026)

**Total rounds: 20 completed** across **10 companies** / **15 distinct role-loops**.

Round-by-round breakdown:

| Loop | Rounds | Detail |
|---|---|---|
| Intel · Power & Perf (Apr) | 2 | R1 + R2 — ❌ rejected |
| Arm · Verification | 1 | 🟡 mismatch (filtered) |
| Qualcomm · Pre-Si Verification | 1 | 🟡 mismatch (filtered) |
| Qualcomm · CPU Core (Pre-Si) | 3 | R1 + R2 + HM — 🔴 stalled 16+ d |
| Qualcomm · CPU Post-Silicon | 2 | R1 + R2 on 2 Jun (planned 3, actually 2) — 🟡 results pending |
| Qualcomm · PnP / Thermal (Hyderabad) | 2 | both rounds 28 Apr — 🔴 stalled, no reply since (30+ days) |
| Intel Ireland · NPU | 1 | rapid-fire 2 interviewers — ❌ rejected 21 May |
| Renesas | 1 | R1 — ❌ role-fit miss |
| Intel Bangalore · IP Functional Validation | 2 | F2F R1 + R2 — ✅ **Selected, offer accepted 21 May · joining 27 Jul** |
| Cerebras | 1 | R1 — ❌ not fit for role (closed 18 May) |
| AMD · Core PnP Post-Si | 2 | R1 (14 May) + R2 (5 Jun) — 🟡 active/recent |
| **Nvidia · Verification Engineer** | 1 | ❌ rejected this cycle (screening 11 Jun — not ready) · 🔵 warm contact via Ranjith retained, can reconnect next cycle |
| **Amazon Lab126 · AZ1 ML SoC (Echo full-Si val)** | 1 | R1 done 26 May — ❌ **Rejected 28 May** (HW-depth gap confirmed) |
| **Broadcom · Switch Pre-Si Validation** | 0 | 🟢 **2 roles: direct app ❌ rejected · referral 🟢 screening scheduled 22 Jun** — applied 1 role directly (rejected), friend referred for a separate switch pre-si val role; prep ask = electronics + digital design + CDC + Intel work |
| **Qualcomm · System Performance** | 0 | 🔵 **R1 + R2 scheduled 20 Jun** — new QC role; update with topics / coding / outcome after |
| **Micron · HBM Customer Memory Validation** | 0 | ⬜ **2 roles; 1 interview scheduled — declined by me** (role off-track: HBM customer memory val, not target arc) |
| **Total** | **20 completed** | |

Loop status:

| Bucket | Count | Loops |
|---|---|---|
| ✅ Selected (final offer) | **1** | Intel Bangalore (joining 27 Jul) |
| 🟡 Active gate | **2** | Qualcomm CPU Post-Silicon — 2 rounds done 2 Jun, results pending · AMD Core PnP Post-Si — R2 done 5 Jun |
| 🔵 Future warm contact / next-cycle option | **1** | Nvidia (Verification Engineer) — ❌ rejected this cycle; Ranjith contact retained, reconnect next cycle after boot-flow + PCIe-emulation gap closes |
| 🟢 Referral in — prep stage (no round yet) | **1** | Broadcom Switch Pre-Si Validation — friend referral 18 Jun; prep on electronics + digital design + CDC + Intel work before loop opens |
| � Scheduled — upcoming round | **1** | Qualcomm System Performance — R1 + R2 on 20 Jun |
| ⬜ Declined by me (role fit) | **1** | Micron HBM Customer Memory Validation — interview scheduled, opted out (off-track) |
| 🔴 Stalled — no reply (treat as closed) | **2** | QC CPU Core Pre-Si (16+ d) · QC PnP/Thermal (30+ d) |
| ❌ Rejected / closed | **5** | Intel Power & Perf (Apr) · Renesas · Cerebras · Intel Ireland NPU · Amazon Lab126 |
| 🟡 Mismatch — filtered upstream | **2** | Arm Verification · QC Pre-Si Verification |
| **Total loops** | **15** | |

**Conversion (loops, not rounds):** 1 / 13 decided loops landed = **7.7%** (Intel Bangalore, accepted). Two new loops added 18–19 Jun are not yet decided and excluded from the denominator: Qualcomm System Performance (R1+R2 scheduled 20 Jun) and Micron HBM Customer Memory Validation (declined by me pre-interview). **Current-cycle evaluation is only confirmed Intel plus open Qualcomm CPU Post-Silicon and AMD CPU post-silicon follow-up**; Nvidia is parked for a future cycle via warm contact.

**Velocity:** 20 completed rounds over ~60 days (13 Apr → 11 Jun). Intel join proceeds 27 Jul unless Qualcomm or AMD produces a clearly superior written offer. Nvidia should not be treated as current-cycle leverage: even if re-requested after ~2 weeks, the interview loop is unlikely to complete in time for the July decision.

---

## 📬 Application funnel (top of cycle → offer)

*Approximate — a few apps may be uncounted.*

| Company | Apps | Loops opened | App→Loop % | Outcome |
|---|---:|---:|---:|---|
| Intel | 10 | 3 | 30% | 1 offer (BLR IFV) + 2 rejects (P&P, Ireland NPU) |
| Qualcomm | 21 | 4 | 19% | 1 active (CPU Post-Silicon) + 1 upcoming (System Performance, 2 rounds 20 Jun) + 2 stalled (CPU Core Pre-Si, PnP/Thermal) + 1 mismatch (Pre-Si Verification) |
| AMD | 14 | 1 | 7% | 1 active/recent (Core PnP Post-Si R2 done 5 Jun) |
| Arm | 10 | 1 | 10% | 1 mismatch (Verification) |
| Nvidia | 7 | 1 | 14% | 1 technical screen (Verification Engineer) — ❌ rejected this cycle; warm contact retained for next cycle |
| Amazon | 2 | 1 | 50% | 1 reject (Lab126 Echo full-Si) |
| Broadcom | 2 | 0 | — | 1 reject (direct-applied role, pre-interview) + 🟢 1 referral loop opening (switch pre-si val, screening 22 Jun — not yet a completed loop) |
| Micron | 2 | 0 | — | 2 roles; 1 interview scheduled — **declined by me** (HBM customer memory validation, off-track) |
| Cerebras | 1 | 1 | 100% | 1 reject (role-fit miss) |
| Renesas | 2 | 1 | 50% | 1 reject (automotive mismatch) |
| Apple | 4 | 0 | 0% | no response |
| Meta | 5 | 0 | 0% | no response |
| Google | 4 | 0 | 0% | no response |
| Qualinx Netherlands | 2 | 0 | 0% | no response |
| Texas Instruments | 2 | 0 | 0% | no response |
| Graphcore | 1 | 0 | 0% | no response |
| NXP | 1 | 0 | 0% | no response |
| Marvell | 1 | 0 | 0% | no response |
| **Total** | **91** | **13** | **14.3%** | **1 offer = 1.10% app→offer, 7.7% loop→offer** |

**Funnel signal:**
- **8 of 16 companies returned zero** (Apple, Meta, Google, Qualinx, TI, Graphcore, NXP, Marvell) = 20 apps / 23% of total effort wasted on cold-apply portals where the no-referral conversion rate is ~0%.
- **Highest-volume targets** (Qualcomm 21 · AMD 14 · Intel 10 · Arm 10) = 55 apps for 9 loops — spray-and-pray with low per-app yield.
- **Highest hit-rate shops** were the smaller/warm ones (Cerebras 100%, Amazon 50%, Renesas 50%, Intel 30%) — the loops that opened easiest were where the app was targeted, not bulk.

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

### 14 May — AMD · Core Power & Performance Post-Si (R1) — 🟢 Positive → 🟡 advanced to R2
- **What happened:** (1) Walk through every prior role with a deep-dive at each — what I did, issues hit, how I unblocked. (2) Open-ended design probe: *how would I design an NPU + CPU + multi-SoC power/perf tradeoff?* — answered with workload-driven budget split, shared rail vs independent rail tradeoff, DVFS coordination across SoCs (single PMIC arbiter vs distributed), thermal-envelope sharing, and the validation-side coverage matrix that falls out. (3) Role framing from interviewer: **core PnP post-Si team — heavy execution + heavy debug after data lands; needs architecture exposure + system understanding.** Direct fit with my Intel PnP + MS NPU arc.
- **Learning:** Multi-SoC PnP design questions are the new shape at AMD/Cerebras-tier shops — the answer scaffold is **workload → budget split → rail topology → DVFS coordination → thermal envelope → validation matrix**, in that order. Role pitch ("execution + debug + architecture exposure") is exactly what my CV reads as.
- **Status:** Advanced to R2 on 5 Jun — see 5 Jun entry.
- **Suggestion:** Add **multi-SoC PnP design scaffold** to [Day 15](prep_plan/day15_thermal_power_perf_counters.md) (thermal envelope sharing) and [Day 12](prep_plan/day12_dvfs_pstates_pvt.md) (DVFS coordination across SoCs / shared vs independent rails). Add a one-liner answer to [Day 24 final mock](prep_plan/day24_final_mock.md) Sec 5 on "design an NPU+CPU+multi-SoC PnP tradeoff" so it's a reflex.

### 26 May — Amazon Lab126 · AZ1 ML SoC (Echo full-Si val) — R1 — ❌ Rejected 28 May (HW-depth gap)
- **What happened:** Echo full silicon validation role. HW-bug-heavy — deep follow-ups on prior bug stories. Design probe 1: **"design a core C-state for a PC from scratch"** — entry/exit, residency, break-even, wake sources, demotion *why* (false-wake penalty, break-even violation) + *how* (governor weighting, HW-side promotion-block, LTR / peripheral blocker interaction). Design probe 2: **"create a test plan for an IP"** — walked the spec → features → modes → stimulus/checks → coverage axes → negative → perf/power → pre-Si vs post-Si scaffold (same shape that landed at Cerebras). Coding: **digital root** (`9875 → 29 → 11 → 2`).
- **Coding bug:** outer loop was `while (sum % 10)` — wrong (exits on any multiple of 10, e.g. `20 → 2` exits early). Correct: `while (sum >= 10)`. Termination-condition slip, same edge-case-preamble gap Cerebras flagged.
- **Learning:** Echo full-Si is HW-heavier than Intel BLR / AMD shape — they want board / lab / debug-fluency, not just architecture. C-state design-from-scratch is the **2nd design-scaffold probe** (after AMD multi-SoC PnP); scaffold = **entry triggers → eligibility → save/restore → power/clock action → wake sources → exit → demotion + governor feedback → measurement → validation matrix**.
- **Status:** ❌ **Rejected 28 May.** Soft-negative signal from R1 confirmed — mismatch was structural (CV reads architecture + system-val, role wanted board / lab / HW-fluency for Echo full-Si). No further rounds.
- **Suggestion:** Add C-state design-from-scratch scaffold to [Day 13](prep_plan/day13_cstates_pkgc_s0ix.md). Add digital-root + verbalise-loop-invariant to [Day 00](prep_plan/day00_c_basics.md). For any follow-up: lean into L&T board bring-up + Intel post-Si lab debug to close HW-exposure perception.

### 02 Jun — Qualcomm · CPU Post-Silicon — 2 rounds done (planned 3) — 🟡 results pending
- **Correction:** Loop completed as **2 rounds**, not 3. Both rounds happened on 2 Jun.
- **R1 — what happened:** Discussion on PM flows, especially what I did at Intel, with probes on **why** and **how** behind the flows. Coding/design probe around **64B cache lines**, 16B writes, and generating write addresses that overlap cache-line boundaries / stress write overlap behavior.
- **R1 coding shape:** Given base/end address range and cache-line size, compute number of cache lines, iterate line-by-line, choose an offset near the end of each 64B line, and issue a 16B write so it overlaps into the next line. Key intent: target boundary conditions, not just random writes inside a line.
- **R2 — what happened:** More normal work discussion — what I want to do, prior work shape, and fit. Coding probe: combine two 32-bit register reads from a 32-bit data bus into one 64-bit tick value. Example: low register `0x1111ABC2`, high register `0xABDC1113` → `0xABDC11131111ABC2`.
- **R2 coding answer:** Read low/high as `uint32_t`, cast each to `uint64_t` before shifting, compute `((uint64_t)high << 32) | low`. The important trap is not shifting a 32-bit value by 32 before widening.
- **Learning:** Qualcomm is probing PM-flow ownership plus hardware-adjacent C reflexes: cache-line boundary stress, register-width/data-bus mismatch, and correct integer promotion. Need to narrate the intent of the test, not only write code.
- **Suggestion:** Add cache-line-overlap write generator and 32-bit-register-to-64-bit-tick routine to [Day 00 C basics](prep_plan/day00_c_basics.md) / [Day 03 bits + MMIO](prep_plan/day03_bits_mmio_volatile_polling.md). Practice saying: *"because the bus/registers are 32-bit, I widen before shifting and preserve low/high ordering."*

### 05 Jun — AMD · CPU Post-Silicon / Core Power & Performance (R2) — 🟡 active/recent
- **What happened:** Discussion stayed close to normal work and role fit. Main technical area was **PM flows, power, and performance**, with questions around whether I know benchmark/perf tooling such as Geekbench / perfbench, TDP framing, and related performance characterization. I said I did not know those tools deeply.
- **Role clarification from interviewer:** This team is currently doing **CPU core post-silicon on fixed-frequency performance**. They are expanding the team to add power on top of that. When issues appear, debug can go down to **core pipeline-level identification** and can require **multi-IP debug** to understand why performance or power behavior fails.
- **Learning:** AMD shape is more CPU-core-performance/debug-heavy than broad PnP alone. They value fixed-frequency perf characterization, benchmark literacy, TDP/power framing, and the ability to debug from system symptom → core/pipeline/IP interaction.
- **Gap:** Benchmark/tooling vocabulary is soft: Geekbench / perfbench style workloads, fixed-frequency normalization, IPC / CPI / stall breakdown, perf counters, TDP vs sustained power, and how to separate core pipeline bottleneck from memory/interconnect/firmware effects.
- **Suggestion:** Add a focused CPU post-silicon perf drill: fixed frequency setup → benchmark run → counter collection → IPC/CPI/stall analysis → compare against golden → isolate core pipeline vs memory/uncore/multi-IP. Tie this into [Day 15 power/perf counters](prep_plan/day15_thermal_power_perf_counters.md) and [Day 09 pipeline microarchitecture](prep_plan/day09_pipeline_microarch.md).

### 11 Jun — Nvidia · Verification Engineer · screening / readiness discussion — ❌ Rejected this cycle · 🔵 warm contact retained
- **What happened:** Screening became a technical readiness probe. Main topics: **boot flow**, especially *who triggers startup code* after reset; **how a PCIe write actually happens** from software/store → MMIO/BAR → TLP → endpoint; whether prep covers **LTSSM**; and **what is not covered in emulation for PCIe**. I forgot / did not land the PCIe-emulation limitations clearly.
- **Interviewer signal:** He said I am **not ready for the interview yet** — a **rejection for this cycle's Verification Engineer seat**. He asked me to let him know once ready and said we can have the **same discussion again**. So the door is open: treat as **future-cycle only** via the retained warm contact, not a hard permanent reject.
- **Role/team signal:** They are building a team equivalent to **PythonSV**: heavy hardware debug, boot-flow knowledge, PCIe/system bring-up, and similar SoC-validation tooling/debug work. The role likely values hands-on debug fluency more than generic architecture recall.
- **Learning:** The gap is not "do I know PCIe exists" — it is **system debug narration**: reset actor → ROM/firmware actor → host/RC actor → config/MMIO actor → LTSSM/tooling visibility → emulation-vs-silicon boundary. Need answer as an execution trace, not a definition.
- **Suggestion:** Keep Ranjith as a warm future contact. Do **not** plan current-cycle decisions around Nvidia. For a later re-approach, drill four answer chains until 60-90 sec each: (1) who starts startup code after reset, (2) PCIe posted memory write / doorbell path, (3) LTSSM bring-up ladder + stuck-state debug, (4) what emulation covers vs cannot cover for PCIe. Map to [Day 11 CPU bring-up](prep_plan/day11_cpu_bringup_functional_val.md), [Day 18 PCIe LTSSM](prep_plan/day18_pcie_fundamentals_ltssm.md), [Day 21 PCIe bring-up](prep_plan/day21_pcie_bringup_functional_val.md), and [Day 24 final mock](prep_plan/day24_final_mock.md).

### 18 Jun — Broadcom · Switch Pre-Si Validation — 🟢 Referral in, prep stage (no round yet)
- **What happened:** Two separate Broadcom roles. (1) I **applied to one role directly — that one got rejected.** (2) Separately, a **friend referred me for a different role** — a **switch team pre-silicon validation** position. The referral is the live one. He asked me to **prepare on basics of electronics, digital design concepts, CDC (clock domain crossing) and similar concepts**, plus be ready to walk through my **Intel pre-silicon work**.
- **Role signal:** Switch ASIC pre-si val (networking/Ethernet switch silicon). Expect emphasis on **RTL-adjacent fundamentals** — CMOS/logic basics, sequential design (setup/hold, metastability), **synchronizers + CDC + RDC**, FIFOs across clock domains, FSMs, reset architecture, low-power intent — framed for a **pre-Si verification/validation** seat, not just architecture recall. Intel Zebu pre-Si C-state / PM-handshake / cross-IP bug work is the strongest CV bridge.
- **Learning:** This is the **EE-fundamentals + digital-design gap** that closed Intel P&P (Apr) and Intel Ireland NPU — now showing up as an *explicit prep ask*. Treat as the trigger to finally lock that gap. The switch-domain twist adds CDC depth (multi-clock SerDes/MAC/PHY + packet pipelines) on top of generic digital design.
- **Status:** 🟢 Referral in. **First screening round scheduled 22 Jun 2026.** Prep window: 18–22 Jun. Standalone HTML study notes created (see [broadcom_switch_presi_notes.html](broadcom_switch_presi_notes.html)) covering electronics + digital design + CDC + cross-interview gaps + coding practice.
- **Referrer intel (Sparsh, 18 Jun) — his own interview shape (high-signal):** CDC in depth · async FIFO in depth · state machines · gate-type digital logic questions · **puzzles** · bit manipulation · **memory-subsystem verification** · **verifying a FIFO** · **DMA controller** · STA focused on **metastability, data incoherency, FIFO-pointer gray-code conversion** · **C array problems (moderate)** · scenario design: **devise a strategy to find a stuck-at fault on a memory-subsystem address line** (his team does heavy memory-subsystem verification) · scenario: **given Ethernet config/features, devise verification strategies** for different queueing methods and **how to verify a FIFO never overflows across all switches**. Process shape: **early rounds = basics, later rounds = your own work**. He worked mostly on **reset, boot, low-power flows** at Intel and got asked on those later — maps directly to my Intel Zebu PM/reset/clock-gating arc.
- **Suggestion:** Work the dedicated HTML notes (electronics → digital design → CDC → metastability/synchronizers → switch-silicon pre-si val → Intel bridge). Rehearse a 60-90 sec answer for: *two-FF synchronizer + MTBF*, *why async FIFO + gray-code pointers*, *setup/hold + metastability*, *CDC bug classes (data loss, reconvergence, glitch)*, and *map my Intel cross-IP / PM-handshake bug to a CDC/RTL framing*. **Per referrer: add memory-subsystem verification (March/stuck-at, address-line stuck-at strategy), FIFO verification, DMA controller, gate-level logic puzzles, and C array problems; lead my own-work story with reset/boot/low-power flows for later rounds.**

### 19 Jun — Micron · HBM Customer Memory Validation (2 roles) — ⬜ Declined by me (role fit)
- **What happened:** Two Micron roles in play; one had an interview scheduled. I **chose not to attend** because the role was **HBM customer memory validation** — off my target track (CPU/SoC power-perf + pre-Si/post-Si system validation).
- **Learning:** Filter HBM / memory-customer-validation shapes upstream — not aligned with the silicon system-val / PnP arc. Declining preserves prep bandwidth for the Qualcomm / AMD / Broadcom gates.
- **Status:** ⬜ **Declined pre-interview.** No round attended.

### 20 Jun — Qualcomm · System Performance (2 rounds) — 🔵 Scheduled
- **What happened:** New Qualcomm role — **System Performance**. Two rounds scheduled for 20 Jun. *(Will update with exact topics / coding / outcome after the rounds.)*
- **Status:** 🔵 **Upcoming** — R1 + R2 on 20 Jun. Update post-round.
- **Suggestion:** Lean on PM-flow + perf-counter + system-val framing; keep the same hardware-adjacent C reflexes that came up in QC CPU Post-Si (cache-line boundary stress, register-width / data-bus mismatch, integer promotion). Tie to [Day 15 power/perf counters](prep_plan/day15_thermal_power_perf_counters.md) and [Day 09 pipeline microarch](prep_plan/day09_pipeline_microarch.md). Fill in specifics after the round.

---

##  Patterns

**Strong:** bug stories with measurable impact, cross-vendor NPU breadth, **design-from-scratch scaffolds** (test-plan, multi-SoC PnP, C-state), role-fit framing in HM rounds.

**Still soft:** layer-3 depth under probe-then-push, live-C **terminate-condition + edge-case pre-amble** (Cerebras + Amazon both), EE fundamentals, **HW-exposure breadth** for board/lab-fluency roles (Amazon Echo signal), **boot-flow + PCIe execution-trace narration** for Nvidia/PythonSV-shape debug roles, **digital-design + CDC fundamentals** for RTL-adjacent pre-si seats (Broadcom switch ask, 18 Jun).

**Meta-skill:** probe-then-push — *what → why → how → connected*. Scaffolds survive the push; flat-fact recall doesn't. Every concept = scaffold to walk, not paragraph to recite.

**Strategic principle:** Intel offer accepted, joining 27 Jul, remains the default anchor. Qualcomm CPU Post-Silicon and AMD CPU post-silicon are the only active written-offer gates. Nvidia is **future warm-contact only**, not a July decision input. Carry-forward gaps for tomorrow/future cycles: (1) design-from-scratch scaffolds (test-plan, multi-SoC PnP, C-state — lock as reflex), (2) C edge-case + loop-invariant pre-amble (Cerebras + Amazon both flagged), (3) HW-fluency framing (L&T board bring-up + Intel post-Si lab) when role reads HW-heavy, (4) EE fundamentals refresh — 2x-closed gap (Intel P&P Apr + Intel Ireland NPU), (5) CPU fixed-frequency perf / benchmark / counter-debug vocabulary for AMD-shape roles, (6) Nvidia/PythonSV-shape boot-flow + PCIe execution traces.

---

## 🔁 Flywheel (after every round)

1. **Same day:** add row to timeline above — what happened, learning, status, suggestion.
2. **Within 24h:** map each "didn't land" to a Day file Architect Drill — add one if missing.
3. **Within 48h:** ensure the relevant Day file's drills + Exit Check cover the gap.
4. **Before next interview:** run [Day 24 final mock](prep_plan/day24_final_mock.md) flashcard sweep with the new gap-fill in rotation.

---

## 📝 Retro / carry-forward

- **Top gap closed:** design-from-scratch scaffolds — multi-SoC PnP (AMD) + test-plan-from-spec (Cerebras/Amazon) now landed in [Day 11](prep_plan/day11_cpu_bringup_functional_val.md), [Day 12](prep_plan/day12_dvfs_pstates_pvt.md), [Day 13](prep_plan/day13_cstates_pkgc_s0ix.md), [Day 15](prep_plan/day15_thermal_power_perf_counters.md).
- **Top gap still open:** EE-fundamentals refresh — 2x-closed (Intel P&P Apr + Intel Ireland NPU). Quick-refresh now consolidated in [Day 24 Sec 5](prep_plan/day24_final_mock.md) but needs live drilling, not just text.
- **Best-fitting role:** Intel Bangalore IP Functional Validation — spec→vplan→UVM→coverage→CR-triage matches the CV one-for-one; ADR + C-state negative-path probes were directly answerable from prior Intel work.
- **Companies to re-approach / evaluate:** Qualcomm CPU Post-Silicon is a separate active role with 2 rounds done on 2 Jun; AMD Core PnP / CPU Post-Si revived with R2 on 5 Jun and is worth evaluating carefully against Intel if it advances. Nvidia is a future warm-contact option through Ranjith; keep the relationship, but exclude it from current-cycle offer evaluation.
- **Companies to deprioritise:** Renesas (automotive shape) · Arm Verification · Qualcomm Pre-Si Verification — known role-shape mismatches. **Cold-apply portals at FAANG (Apple, Meta, Google, Marvell, TI, NXP)** — 0/20 yield; referral-only next time.
- **One thing I'd do differently:** Front-load the funnel with **referrals**, not portal applications. 20 cold apps at FAANG-tier returned zero loops; even 3-4 warm referrals via LinkedIn would likely have opened 1-2 loops at the same effort. And: filter JD-shape upstream (no automotive, no pure SV-UVM) to stop burning loops on known-mismatch roles — cost 3 of 13 loops this cycle.

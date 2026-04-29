# Senior Validation Engineer - Interview Revision Guide

> Generic interview prep for senior silicon/platform/NPU validation roles. Targets:
> - **Behavioral / leadership** (senior-level expects this)
> - **Career narrative** (own your last 8 yrs)
> - **Thermal + power behavior** (asked last round)
> - **DVFS / C-states / power management** (your strongest tech ground)
> - **NPU post-silicon validation** (tomorrow's focus)
> - **Quick puzzles** (the 3 you missed last time + classic warmups)
>
> Strategy for a senior IC + lite-managerial role: be **clear, specific, calm**. One concrete example beats three textbook bullets. If you don't know - say so cleanly and pivot to closest thing you do know.

---

## SECTION 0 - Last-10-min cheat sheet

| Theme | One-liner reflex |
|---|---|
| Why this role | NPU post-Si is where my Microsoft EdgeOptimizer work + Intel pre-Si experience converge - I've validated NPU silicon from bring-up to OEM ship |
| Why Intel Ireland | Closes the loop — pre-Si + post-Si on one product, design centre on-site, EU base, English-speaking, deliberate long-horizon move |
| Why relocate | Role-first not place-first; Leixlip is Intel's flagship non-US engineering site; family-stable, EU base, strong eng culture |
| Lead-team-of-15 | I'd run as a tech lead, not pure manager. Daily standups, weekly 1:1s, clear ownership matrix, escalate blockers fast. Most senior IC + small-team lead is my comfort zone |
| Underperforming teammate | First conversation: understand if it's **can't** (skill/blocker) or **won't** (motivation). Different responses for each |
| Thermal goes up, don't want perf drop | Headroom buying: better cooling, lower V at same F (binning), spread load across cores, schedule heavy work to cool periods, use bigger/LITTLE cluster |
| C-state entry | flush dirty $, save arch state, gate clocks, gate power, drop voltage |
| C-state exit | wake event > restore voltage > ungate power > ungate clocks > restore arch state > resume |
| DVFS one-line | (V, F) operating point chosen by PCU/RPMh based on load + thermal + power budget |
| NPU silent fallback | unsupported op > drops to CPU EP > perf collapses without functional fail. Detect via per-op EP-assignment log |
| 2-egg puzzle | optimal first drop = floor 14 (sqrt(2N) approx, formula k(k+1)/2 >= N) |
| Sum 1-15 odd = 30 | impossible (odd count of odd nums = odd sum); trick puzzle |
| x^y > y^x | true when x < y for x,y >= 3; flips for small values (2^4 = 4^2) |

---

## SECTION 1 - Career narrative (own this cold)

### 60-second version
> "Silicon validation engineer with 8 years across the full silicon lifecycle. Currently at Microsoft doing NPU post-Si platform validation on Copilot+ PCs - across Intel Lunar Lake, AMD Strix, and Qualcomm Snapdragon X Elite. Built EdgeOptimizer, an internal tool that uses ETW + WPA + RAPL to do per-inference attribution of CPU/NPU utilization, energy, and silent-fallback detection. Before that, Intel Cambria Xeon pre-Si emulation - owned C-states, ADR, PCIe Gen6 transactor on Zebu, won Star of the Month for a CXL firmware bug debug. Started at L&T@SanDisk on NVMe post-Si bring-up - JTAG, Metaware, LeCroy, Python power-scenario harness, SDExpress POC Spot Award. M.Tech CSE NIT Durgapur 2018. Through-line is silicon validation across the stack from RTL traces up to OS scheduler."

### 30-second version
> "8 years silicon validation. Microsoft now - NPU platform validation Copilot+ PCs (Intel/AMD/QC), built EdgeOptimizer. Intel before - Cambria Xeon pre-Si emulation (C-states, ADR, PCIe Gen6). L&T@SanDisk before that - NVMe post-Si bring-up. M.Tech NIT Durgapur."

### Why this role / why now
- "I've done pre-Si and post-Si on three different vendors' silicon. NPU post-Si specifically is the layer I've been pushing hardest in - EdgeOptimizer was my way of building tooling that scaled across vendors. I want to do that work where the silicon is being designed, not just consumed."

### Strengths to lead with
- **Cross-stack debug:** comfortable from RTL/emulation traces up through firmware, kernel drivers, ETW, OS scheduler.
- **Cross-vendor experience:** validated NPU on three architectures - know what's common, what's vendor-specific.
- **Tool-builder mindset:** EdgeOptimizer + agentic GitHub Copilot CLI usage at scale - I automate before doing the same debug twice.
- **Bug-story portfolio:** Core C6 wake-loss (CFC FIFO), NVMe CSTS.RDY, QNN page-fault regression - all owned end-to-end.

### Honest framing
- "I'm an experienced IC who has led small teams (2-4) inside larger orgs. Comfortable as tech lead; would grow into a 10-15 person manager but not claiming to have done it at scale yet."
- "ARM/Snapdragon-native vocabulary still building - I map from Intel equivalents I know deeply (PCU > RPMh, IOMMU > SMMU, PCID > ASID)."

---

## SECTION 2 - Behavioral / leadership questions (senior bar)

### Q: "Can you handle a team of 15?"

**Answer (90 sec):**
> "Today I'm comfortable as a tech lead for 4-6 engineers, which is what I've been doing at Microsoft inside a larger team. A team of 15 is a different shape - you stop being able to debug everything yourself and you become a multiplier. The way I'd approach scaling there is:
> 1. **Structure** - split into 2-3 sub-teams with senior ICs leading each technical area (e.g., bring-up / platform / regression / tooling). Don't try to be the only person making decisions.
> 2. **Cadence** - daily 15-min sub-team standups, weekly 1:1s with each direct, weekly tech sync with sub-leads, monthly all-hands.
> 3. **Ownership matrix** - every workstream has one DRI, one backup, escalation path documented.
> 4. **Time split** - 40% people/process, 40% technical strategy and design reviews, 20% hands-on debug on the hardest issues so I stay credible.
> Honest version: I'd want a 6 month ramp from 6 to 15. I'd lean on a senior peer-mentor or skip-level for the first few months."

### Q: "IC or group? Where do you want to grow?"

**Answer:**
> "Senior IC + tech lead is where I'm strongest right now. Long term I see myself as a Principal Engineer or Engineering Manager of a tight 10-15 person team - I want both the technical depth and the team-multiplier impact. I don't want to fully leave technical work; I want to shape what gets built and unblock the team."

### Q: "Teammate isn't delivering. What do you do?"

**Answer (the framework they want):**
> "First step is the conversation - I never escalate before talking to the person directly. The conversation is to figure out: is it **can't** or **won't**?
>
> **Can't (skill or blocker):**
> - Missing context > pair them with a senior, do a knowledge transfer session.
> - Blocked on dependency > I unblock as lead (escalate, get the right people in a room, take it off their plate).
> - Skill gap > scope down their workstream to something they can succeed on while building up; document the gap with HR/manager so we plan training.
> - Personal situation > offer flexibility, loop in manager/HR for support.
>
> **Won't (motivation / disengagement):**
> - Direct conversation: 'I've noticed X over the last two sprints. What's going on?' Listen first.
> - If it's misalignment ('I want to work on Y, not this') > see if I can re-scope; if not, agree they should look at other teams internally.
> - If it's pure disengagement and the role isn't changing > I document specific examples (missed commits, no review participation), set clear written expectations with timelines, and loop in their manager. After that, it's a performance issue and follows the company's performance process. I don't carry someone silently - it's unfair to the rest of the team.
>
> Throughout: keep it private, keep it factual, never blame in public. The other team members will notice how you handle it - that sets the team's culture."

### Q: "Tell me about a time you had to deliver under pressure."

**Pick one and rehearse:**
- **EdgeOptimizer launch** for OEM validation cycle - 2 weeks to ship per-inference attribution in time for the Copilot+ PC certification window.
- **Cambria Core C6 bug** caught pre-Si - if missed, would have been a P0 silicon respin issue.

STAR format: Situation, Task, Action, Result. Quantify where possible.

### Q: "Tell me about a conflict with a peer / manager."

**Frame:** disagree-and-commit. One concrete example - design choice you pushed back on, the data you brought, the eventual decision (yours or theirs), and what you learned. Avoid blaming.

### Q: "Strength / weakness."

- **Strength:** "Cross-stack debug + tool-building mindset. I'm comfortable in any layer from RTL traces to OS, and I'd rather build a 1-day tool than do the same debug 5 times."
- **Weakness:** "I can over-invest in tooling before validating the manual approach has the right shape. Mitigation: I now do a manual run first to confirm the workflow, then automate."
- *(Avoid: "I work too hard" / "perfectionist" - boring, hiring managers tune out.)*

### Q: "Why leaving Microsoft?"

**Frame positively:**
> "Microsoft has been excellent for breadth - three vendors' NPUs and a tooling charter. The next thing I want is depth at the silicon side - working where the NPU silicon is being designed, not where it's being qualified. That's why a vendor-side post-Si role is the right next step."

### Q: "Where do you see yourself in 5 years?"

> "Principal-level IC leading a small team on a critical silicon platform - or an engineering manager of a 10-15 person validation team. Either way, I want to be the person OEMs and product teams escalate to when something doesn't make sense at silicon level."

### Q: "Question for us?"

Have 3 ready:
1. "What does the team's split look like between pre-Si and post-Si work?"
2. "What's the regression cadence? How does a silicon escape get diagnosed?"
3. "Where is the team in its growth - hiring to fill gaps, or hiring to scale?"
4. "What does success look like in this role at 6 months and 12 months?"

---

## SECTION 2.5 - Intel Ireland: relocation, culture, and "why us" behavioral block

> Intel Ireland (Leixlip / Shannon) means a country move, a culture move, and an Intel-specific values move. Senior interviews here drill on **intent** ("why Ireland, why Intel, why now") far more than US loops do. Have these answers concrete, calm, and short. Never sound like Ireland is "a backup" or "for the visa."

### Q: "Why Ireland? Why are you willing to relocate?"

**Answer (60-90 sec):**
> "Three reasons, in order. First, the **role** — Intel Ireland's NPU validation team owns the seam between pre-Si and post-Si on AI silicon, which is exactly the loop I want to close. I've done both halves separately (pre-Si Cambria at Intel India, post-Si NPU at Microsoft); I want to do them on one product. Second, the **site** — Leixlip is one of Intel's two flagship sites globally and the largest non-US engineering campus, with the design centre on-site, so validation findings feed directly back into the next stepping. Third, the **country** — Ireland is English-speaking, EU-based, has a strong engineering culture, and a large international tech community. For my family it's a stable, long-horizon move, not a stepping-stone. I've thought about this seriously; I'm relocating with intent."

**Watch-outs:**
- Don't list weather, taxes, or pay as drivers — even if true, they read as transactional.
- If asked about family / dependants, answer factually and briefly, then return to the role.
- Do **not** compare unfavourably to current location ("India is too crowded" etc.) — read as negative.

### Q: "Have you worked with European / Irish teams before?"

> "Yes — at Intel India I worked daily with Hudson (US) and Haifa (Israel) on Cambria, with overlap windows that taught me how to write decisions down so the next time-zone could act without me. At Microsoft I work with the Redmond and Vancouver teams. Ireland-time UTC+0/+1 is similar to working with Israel and gives clean overlap with both US-East and India, so I'd be a useful timezone bridge for a globally distributed silicon team."

### Q: "What do you know about Intel's culture, especially in Ireland?"

> "Three things stand out. First, Intel's engineering culture is **disagree-and-commit**, with a strong written-decision habit — that fits how I work. Second, the Ireland site is fab + design + validation co-located, which is unusual; it means validation engineers see manufacturing reality, not just lab silicon. Third, post-IDM 2.0 there's been heavy investment in Ireland (Fab 34, AI silicon programs), so it's a growth site, not a maintenance site. I'd want to be where the next-gen NPU is being built, and that's there."

*(If you don't know Fab 34 / IDM 2.0, drop those names and just say "the AI silicon investment is concentrated there.")*

### Q: "How do you handle a long-distance / remote-from-HQ working setup?"

> "Two habits. First, default to writing — design docs, decision logs, async reviews — so the absence of a corridor conversation isn't a blocker. Second, protect one or two real-time overlap windows per week with the partner timezone for the conversations that genuinely need voice. I learned this on Cambria where the architects were in Hudson and the IP owners in Haifa; the teams that wrote things down moved faster than the teams that relied on calls."

### Q: "Why are you leaving Microsoft / India?"

**Frame positively, role-first not place-first:**
> "Microsoft has been excellent — three vendors of NPU silicon, full ETW/RAPL tooling charter, and the agentic-Copilot CLI tooling I built has scaled across the team. But I'm one layer away from where the silicon is designed; I qualify it, I don't shape it. The next thing for me is depth at the silicon side — owning validation end-to-end at the vendor, on one product, across pre-Si and post-Si. Intel Ireland is exactly that role. The country move is a side-effect of where the right role lives, not the driver."

### Q: "Are you prepared for a pay / cost-of-living change?"

> "I've researched Dublin / Leixlip cost-of-living, housing, and the Intel Ireland comp band. I've planned for the transition (initial accommodation, schooling if relevant, first-year ramp). I'd like to discuss the package as part of the offer conversation, but it's not a blocker."

*(Don't open numbers in the loop. Defer to recruiter.)*

### Q: "What's your visa / work-permit situation?"

**Be factual, brief, and proactive:**
> "I'd be on Intel-sponsored Critical Skills Employment Permit / Stamp 1; that's the standard route for senior engineering hires into Ireland. I'm aware of the process and timelines and I'd work with Intel's mobility team. I have no current visa restrictions that would slow this down."

*(If you're not sure of the actual visa class — say "Intel's standard sponsored route" and don't invent specifics.)*

### Q: "What if you're asked to be on-site full-time?"

> "I'd expect a silicon-validation role to be predominantly on-site — the lab, scopes, JTAG rigs, and emulators aren't remote-friendly. I plan to relocate to Leixlip / Maynooth area and be on-site by default; I'd value flexibility for occasional remote work on the spec-reading and analysis side, but I'm not coming in expecting a hybrid contract."

### Q: "Tell me about a time you adapted to a new culture / team."

**Pick one and rehearse (STAR):**
- Joining Microsoft from Intel: different review style (PR/code-review heavy vs design-doc heavy), different on-call rhythm, different tool ecosystem. Action: spent first 30 days just learning, didn't push my Intel patterns; by day 60 I'd contributed an EdgeOptimizer prototype using *their* WPA conventions, not mine.
- Or any cross-team rotation you've done.

**Lesson to land:** "I assume the new culture is right until I understand why it's that way. Pushing your old patterns on day one is the fastest way to lose credibility with a new team."

### Q: "How would your day 1 / day 30 / day 90 look at Intel Ireland?"

> "**Day 1-30:** lab access, dev env, build & boot the validation rigs, shadow the senior validators on a live workload, read the IP arch spec for the NPU I'd be assigned, learn the regression flow. No new ideas yet — earn the right to propose by understanding what already exists.
>
> **Day 30-60:** own a small workstream — a workload, a regression bucket, or one piece of infra. Ship one improvement (a script, a triage doc, a missing test). File at least one bug found end-to-end.
>
> **Day 60-90:** lead a debug from symptom to root cause in front of the team; propose one infra improvement that scales beyond me. By 90 days I'd want a peer to say 'I trust their findings,' which is the only metric that matters in validation."

### Q: "Why now? Why not in 2 years when you have more NPU vendor depth?"

> "Two reasons. One — the AI silicon design cycles are accelerating; the team that ships the NPU in the next 3-year stepping is being formed now, and being part of that arc is more valuable than coming in two years and inheriting decisions. Two — my pre-Si muscle from Cambria is still recent enough to be sharp; if I wait another two years on pure post-Si, that re-engagement gets harder."

### Q: "What are you most worried about in this move?"

**Show self-awareness without selling fear:**
> "Honest answer — first 6 months of context-load. New silicon, new team norms, new country at the same time. My mitigation: ask for a buddy on the team for the first quarter, write everything I learn down for my own re-reference, and resist the urge to over-volunteer in the first month. Beyond that, I don't have specific worries — the role and the site are well-matched to where I want to go."

### Q: "What questions do *you* have about Ireland / the team?" (have 4 ready)

1. "How is the team split between the Leixlip and Shannon sites, and what's the cadence of cross-site collaboration?"
2. "What does the relocation support look like — is there a buddy / mentor program for international hires in the first quarter?"
3. "How does the site interact with the US-based architecture and design teams — overlap hours, decision forums, escalation paths?"
4. "What's the mix between pre-Si validation infra work and post-Si bring-up in this role on a typical sprint?"

### Cultural reflexes (small things that read big)

- **Tone:** Irish workplace tone is **direct but warm and understated**. Don't oversell. Confidence is fine; bravado is not. "I think I can help here" lands better than "I will absolutely transform this team."
- **Humour:** self-deprecating is welcome; punching at others is not. Safe topic.
- **Punctuality:** show up 5 min early to the call. Don't apologise repeatedly if you're 1 min late — apologise once and move on.
- **Names:** use first names in interviews unless explicitly told otherwise. Hierarchy is flat in tone.
- **Disagreement:** allowed and expected, but lead with the data and frame as a question, not a verdict ("Has the team considered ... ?" beats "That's wrong because ...").
- **Closing the loop:** send a short thank-you email within 24h to the recruiter; one or two lines, no need to re-pitch yourself.

---

## SECTION 3 - Thermal + Power Behavior (asked last round - know cold)

### Question that came up: "If thermal goes up and we don't want perf to degrade, how to handle?"

**Layered answer (60-90 sec):**

> "Two parts: why does perf degrade, and what knobs hold it.
>
> **Why perf degrades:** at higher Tj, leakage current grows exponentially, so total power grows. The platform protects silicon by **throttling F/V** - PCU/RPMh drops the P-state to keep Tj < Tjmax (100C consumer / 105C server). That's the perf hit.
>
> **Knobs to preserve perf:**
> 1. **Cooling headroom** - bigger heatsink, better TIM, vapor chamber, more airflow. Drops theta_JA so same power = lower Tj.
> 2. **Voltage binning** - at the same F, a better-binned die runs at lower V > less power > less heat. This is why premium SKUs hold perf longer.
> 3. **Workload spreading** - migrate hot threads off the hot core. Modern schedulers do thread shuffling around DTS readings.
> 4. **big.LITTLE / hybrid** - move background work to E-cores (Intel) or LITTLE cores (ARM); reserve P-cores/big cores for foreground. Lower aggregate power for same user-perceived perf.
> 5. **DVFS curve tuning** - operate further down the perf-per-watt curve: trade 5% F for 15% V drop. Cubic relationship V^2*F means tiny F drop = big P drop = more thermal headroom > can sustain that point longer.
> 6. **Burst-and-rest pattern** - allow short Turbo bursts above sustained TDP, then let cores cool in C-states between bursts. Intel PL1/PL2/Tau encode this - tune Tau higher to allow longer bursts if the workload is bursty.
> 7. **Workload-aware power gating** - aggressively gate IPs not in use (NPU, GPU, ISP) to free thermal budget for the hot IP.
>
> **What I'd actually do as a validation engineer:** measure the workload's real thermal envelope first (DTS + RAPL), identify whether you're hitting Tjmax or PL1/PL2 limit (read MSR_CORE_PERF_LIMIT_REASONS 0x64F), then pick the lever. Wrong fix is worse than no fix - if you're PL1-limited, more cooling won't help."

### Tj equation (sketch on whiteboard)

```
Tj = T_ambient + P_total * theta_JA

theta_JA = theta_JC + theta_CS + theta_SA
           junction-case + case-sink + sink-ambient
```

- **Dried TIM** doubles theta_CS > Tj jumps for same workload > throttle. Classic field bug.
- **Tjmax** = spec limit. Cross > throttle. Continuous near-Tjmax > electromigration accelerates > reliability degrades.

### Why power = V^2 * F * C_eff (the cubic relationship)

Dynamic switching power per CMOS gate:

```
P_dyn = alpha * C * V^2 * F
```

where alpha = activity factor, C = switched capacitance.

- F scales linearly. V scales **quadratically**. To run at higher F, the silicon needs higher V (to meet timing) > P scales **cubically** with operating point.
- That's why DVFS works: drop F 10%, drop V too > P drops ~25-30%.

### Static (leakage) power - the temperature trap

```
P_leak ~ exp(T) approximately
```

- At 50C, leakage might be 20% of total. At 100C, can be 50%+. **Hotter = leakier = even hotter > thermal runaway risk if not throttled.**
- This is why C-state power-gating matters so much in modern SoCs - cuts leakage of the gated rail to ~zero.

### RAPL (energy counters - what EdgeOptimizer reads)

| Domain | MSR (Intel) | What it covers |
|---|---|---|
| PKG | MSR_PKG_ENERGY_STATUS | whole package |
| PP0 | MSR_PP0_ENERGY_STATUS | cores |
| PP1 | MSR_PP1_ENERGY_STATUS | client iGPU |
| DRAM | MSR_DRAM_ENERGY_STATUS | server only |

**Read MSR twice, subtract joules, divide by elapsed > avg watts.**

**Critical caveat (always say):** RAPL is **VR-input estimate, not wall power**. Typical gap **20-30%** (VR loss + DRAM not in client RAPL + platform devices + PSU efficiency). Ground truth = external Quarch / Monsoon.

ARM equivalent: SCMI/SCP firmware exposes per-rail; Snapdragon Profiler reads PMIC directly.

### Throttle reasons - the diagnostic MSR

`MSR_CORE_PERF_LIMIT_REASONS` (0x64F) bits tell you which limit fired:
- bit 0: PROCHOT# asserted (external thermal)
- bit 1: Thermal (Tjmax hit)
- bit 4: PL1 (long-term TDP)
- bit 5: PL2 (short-term burst limit)
- bit 6: Iccmax (current limit)
- bit 11: voltage regulator thermal

Reading this is step 1 of any "why is this slow under load?" debug.

### Multi-IP thermal - SoC reality

```
CPU   GPU   NPU   ISP
 |     |     |     |
shared LLC > shared NoC > shared DDR > shared package > shared cooling
```

- **Power budget conflict:** RAPL package limit forces dynamic budget arbitration. NPU running flat-out = CPU loses headroom and throttles.
- **Hotspot interaction:** NPU hot corner of die can throttle adjacent CPU cores even if the CPU itself isn't hot.
- Validation reflex: always measure workloads in **single-IP baseline** first, then pair-wise, then full-system. Skipping baseline = #1 mistake.

---

## SECTION 4 - DVFS, P-states, C-states (your strongest tech ground)

### DVFS = Dynamic Voltage and Frequency Scaling

**Concept:** the silicon has many stable operating points (V, F) - the **P-states**. Software (or hardware in HWP mode) picks one based on:
- current workload demand,
- thermal headroom,
- power budget (PL1/PL2 / TDP),
- battery state.

**Why it works:** P_dyn ~ V^2 * F. Dropping F lets you also drop V > cubic power saving. At idle or light load, run at low (V, F) > save power. At full load, run at high (V, F) for perf.

### P-state mechanics on x86

| Component | Role |
|---|---|
| **PCU / PUnit** | on-die microcontroller running power state machine |
| **HWP** | Hardware P-states - HW autonomously picks; OS hints via EPP register |
| **Legacy P-state** | OS picks via MSR write (`IA32_PERF_CTL`) |
| **FIVR / IVR** | integrated voltage regulator that actually moves V-rail |
| **PL1 / PL2 / Tau** | long-term TDP / short burst / time-window for burst |
| **EPP** | Energy/Performance Preference (0=max perf, 255=max power save) |

### P-state mechanics on ARM/Snapdragon

| Component | Role |
|---|---|
| **PSCI** | Power State Coordination Interface - SMC ABI for OS > firmware power requests |
| **RPMh** | Resource Power Manager in Hardware - aggregates votes from Apps/Modem/ADSP/CDSP, programs PMIC |
| **TCS** | Trigger Command Set - pre-loaded sequences for fast wake (us not ms) |
| **PMIC over SPMI** | actual rail control |
| **CPRh** | Closed-loop process-aware voltage scaling (DVFS + AVS combined) |

### C-states (covered well below)

C-states = idle states. Orthogonal to P-states (which is the active operating point).

### DVFS bug shapes

- **Wrong V at F (binning escape):** silicon shipped at lower V than it actually needs > timing failures at corner > intermittent crashes. Validation needs full PVT corner sweep.
- **PCU FW opcode-ordering bug:** like your ADR experience - V/F sequence wrong order causes V-droop / hang.
- **AVX license + DVFS interplay:** AVX-512 kicks license 2 > F drops 25% > scalar code around it slower > net throughput LOWER than AVX2.
- **Per-cluster vs per-core voltage:** ARM clusters often share V-rail. Per-core F is real; per-core V is `max(per-core requests)` on that rail. Validation finds where you're paying for someone else's V.

### Real bug to tell (own this story)

> "On Intel Cambria Xeon, PCU firmware had an opcode-ordering issue in the C6 entry sequence. Under back-to-back snoop + timer wake-up traffic, a 1-deep CFC FIFO between core PMA and PCU could drop a wake acknowledgment. Symptom: thread blocked forever after a timer interrupt. Caught pre-Si on Zebu emulation by stress-pattern with overlapping snoop+IRQ. Fix: PCU FW added explicit re-arm after timeout. Would have been a P0 silicon escape."

---

## SECTION 5 - C-states (full flow + bug)

### Ladder

| State | What's saved | Wake latency |
|---|---|---|
| C0 | nothing - active | 0 |
| C1 | core clock gated | ~1 us (HLT) |
| C1E | C1 + lower V | ~1-10 us |
| C3 | L1/L2 flushed, PLL off | ~50 us |
| **C6** | **arch state to SRAM, core power-gated** | ~100 us |
| C7-C10 | + LLC flush, package-level | 100s us - ms |

ARM equivalent: WFI / WFE + PSCI `CPU_OFF`, `CPU_SUSPEND`. RPMh arbitrates.

### Entry flow (8 steps)

```
1. OS scheduler: no runnable thread > emit MWAIT(target) or WFI
2. Core HW: drain pipeline, retire in-flight, reach quiescence
3. Save arch state (GPRs, MSRs, FPU/vec) to power-gated SRAM
4. Flush dirty L1/L2 lines per target depth (C6+ flushes private $)
5. Notify PCU/RPMh: "ready for C6 - pull my V rail"
6. PCU asserts handshake to FIVR/IVR > V-rail to retention voltage
7. PCU gates clocks > power-gates the core > core dark
8. PCU stays awake, monitors wake events (interrupts, snoop, IPI, timer)
```

### Exit flow

```
1. Wake event arrives at PCU (APIC IRQ / IPI / timer / snoop / MSI-X)
2. PCU starts wake sequence:
   a. ungate power (V-rail back to operating)
   b. release reset + ungate clocks (PLL relock = biggest latency)
   c. core fetches reset vector > microcode restores arch state
   d. caches re-warm via demand fetches (cold-cache penalty)
3. Core re-enters C0; resumes the instruction after MWAIT/WFI
```

### Trade-offs

- **Saving** scales with depth: C1 saves clock-tree leakage; C6 saves entire core leakage (~70% of idle power).
- **Cost** scales with depth: wake latency, cold-cache penalty.
- **Break-even:** worth entering only if expected idle > 2x wake latency. cpuidle governor enforces this.
- **Risk:** thrashing C6 in/out on bursty load > more energy than staying in C1.

### PkgC6 vs Core C6

- Core C6 = single core idle.
- PkgC6 = all cores in C6 + uncore conditions (no IO traffic, no DMA, no LLC dirty needing snoop, PCIe in L1.2). One stray driver wakeup blocks the whole package.
- S0ix = modern Connected Standby = deep PkgC10 with S0 nominally active.

---

## SECTION 6 - NPU Post-Silicon Validation (TOMORROW'S FOCUS)

### What does NPU post-Si validation actually mean?

Post-Si = silicon back from fab, working in real platform. Validation = prove it works correctly + meets perf/power/thermal/quality targets across full PVT and workload matrix, find escapes before customer ships.

For NPU specifically:
- Functional: every model in qualification suite produces correct output (vs reference CPU/GPU run).
- Perf: TOPS, per-inference latency, throughput at batch=1, batch=N.
- Power: energy/inference, idle power, energy at sustained load.
- Thermal: sustained perf at TDP, throttling behavior.
- Stress: long-run, mixed workload, multi-IP concurrent (NPU + CPU + GPU + camera).
- Compatibility: across OS versions, driver versions, runtime versions, model formats.

### The NPU stack (must explain in 90 sec)

```
[ App: Photoshop, Teams, Paint SR ]
          v
[ Runtime: ONNX Runtime / DirectML / TFLite ]
          v
[ Execution Provider (EP) ]
   QNN (QC) | OpenVINO (Intel) | XDNA (AMD) | DML fallback (any GPU)
          v
[ Vendor driver + firmware ]
          v
[ NPU silicon (HMX + HVX + DMA on Hexagon) ]
          v
[ Shared DDR / VTCM SRAM ]
```

### Key concepts to know cold

**Execution Provider (EP):** the per-vendor backend ORT plugs into. ORT asks each EP "can you run this op?" - assigns contiguous subgraphs to NPU EP, falls back rest to CPU EP.

**Silent EP fallback (THE post-Si bug):** one unsupported op > whole graph silently runs on CPU > functional pass, perf collapses. Detect via per-node EP_assignment log. **This is the most common NPU validation finding.**

**QDQ format:** ONNX models for NPU are quantized as `Quantize > Op > Dequantize`. Vendor compiler fuses to single INT8 op. Fusion fail > drops to CPU EP.

**VTCM (Vector Tightly Coupled Memory):** NPU on-chip SRAM. Hexagon HTP v79 = 8 MB. Compiler tiles activations to fit.

**Context cache:** first run = ONNX > partition > fuse > tile > compile (slow). Save bin > load on next run = ms. Cache miss = pay compile every launch.

**FastRPC:** Snapdragon mechanism for CPU > Hexagon DSP/NPU communication. Wake latency ~100 us.

**Hexagon HTP family (anchors):**

| HTP | SoC | TOPS | VTCM |
|---|---|---|---|
| v73 | SD 8 Gen 2 | ~26 | 8 MB |
| v75 | SD 8 Gen 3 | ~36 | 8 MB |
| **v79** | **SD X Elite** | **~45** | **8 MB** |

**Real bottleneck on most workloads:** DDR bandwidth (~60 GB/s shared), not TOPS.

### Post-Si validation methodology - the workflow

1. **Bring-up sanity** - power on, driver loads, smoke model runs (ResNet50, MobileNet).
2. **Functional matrix** - run every model in qual suite vs CPU reference. Diff outputs within tolerance.
3. **Perf characterization** - per-model latency, throughput, TOPS achieved. Compare vs spec.
4. **Power/energy** - per-inference energy via RAPL/PMIC; sustained-load avg power; idle.
5. **Thermal sustained** - run heavy workload until thermal steady state; measure perf delta from cold-run.
6. **Stress** - long-run (24h), mixed concurrent (NPU + camera + CPU), thermal cycling.
7. **Regression matrix** - across OS versions, driver versions, model versions. Bisect on regression.
8. **Field telemetry** - what's coming back from OEM/users.

### Bug shapes you'd hunt

| Bug shape | Detection signal | Root cause class |
|---|---|---|
| Silent EP fallback | per-op EP_assignment != NPU | new op in model not supported by vendor compiler |
| Perf cliff at large batch | latency/N collapses past N=4 | tile-too-big for VTCM, multiple DMA round-trips |
| Memory leak across runs | working set grows per inference | runtime not freeing context buffers |
| First-run latency huge, subsequent fine | first = compile, rest = cached | context cache miss / bin not saved |
| Functional pass, output drift | INT8 quantization scale mismatch | QDQ fusion broken / model rebuild |
| Thermal runaway under sustained load | Tj rises faster than expected | DVFS not stepping down cleanly |
| Wake latency regression | first-inference time per launch | FastRPC wake degraded / driver init slower |
| Multi-IP starvation | NPU + camera together = both slow | shared DDR bandwidth + thermal budget |

### Validation tools (Windows-side, what EdgeOptimizer uses)

| Tool | What it gives you |
|---|---|
| **WPR / WPA** | ETW capture and visualization |
| **GPUView** | NPU/GPU dispatch timeline (DxgKrnl events) |
| **ORT Profiler** | per-op time, EP assignment per node |
| **QNN Profiler API** | per-graph compile time, context create, FastRPC wake |
| **PerfMon / Get-Counter** | CPU%, working set, committed bytes |
| **Snapdragon Profiler** | PMIC telemetry, Hexagon counters (vendor side) |
| **External Quarch / Monsoon** | wall power ground truth |

### Bug story to tell (own this)

> "QNN page-fault regression at Microsoft - across a driver update, certain models showed inference latency spike at first run after launch, then settle. Root cause: context cache invalidation logic in the new driver was over-invalidating - on every cold launch it rebuilt the binary instead of loading from cache. Detected by ORT profiler showing compile-phase time on every cold launch instead of just the first ever. Filed against vendor with reproducer; fix shipped in next driver. Saved OEM 200ms first-launch latency for camera SR features."

### Honest framing (must say)

> "I do platform-side NPU validation - graph partitioning, EP fallback, energy/inference, peak VM, version-matrix regression, OEM workflows. I don't debug Hexagon RTL or claim VTCM defects - I correlate ETW + ORT + RAPL counters per inference and bisect across configurations. My value-add is the cross-stack debug from app down to vendor driver, plus tooling to make that scale across vendors."

---

## SECTION 7 - The 3 Puzzles (review + practice)

### Puzzle 1: 2-Egg Floor Test

**Setup:** N-floor building. 2 eggs. Find highest floor from which an egg won't break, in **fewest worst-case drops**.

**Insight:** if you drop from floor X, two outcomes:
- Egg breaks: now you have 1 egg, must linear-scan floors 1..X-1 > X-1 drops worst case.
- Egg survives: drop from higher floor. To keep worst-case constant, the next jump must be `X-1` (one less drop available now).

So floor sequence: X, X+(X-1), X+(X-1)+(X-2), ... must cover N.
> sum k(k+1)/2 >= N
> X(X+1)/2 >= N

**For N=100:** X(X+1)/2 >= 100 > X = 14 (since 14*15/2 = 105). **First drop = floor 14, worst case = 14 drops.**

**General formula:** X = ceil((-1 + sqrt(1+8N)) / 2)

**Step-by-step approach to remember:**
1. Drop egg 1 from floor 14. Break? scan 1-13 with egg 2 (worst 13 more drops = 14 total).
2. Survive? drop from 14+13=27. Break? scan 15-26 (worst 12 more drops = 14 total).
3. Survive? drop from 27+12=39. ... and so on: 14, 27, 39, 50, 60, 69, 77, 84, 90, 95, 99, 100.

**Variants:**
- 3 eggs > different math (cubic root-ish).
- "Optimal vs minimum" - same problem usually means optimal worst-case.

### Puzzle 2: Sum to 30 using 1, 3, 5, 7, 9, 11, 13, 15

**Question:** "Pick any 5 numbers from {1,3,5,7,9,11,13,15} (with repetition usually allowed) so their sum = 30."

**Insight - the trick:** sum of any number of **odd** numbers is **odd if count is odd**, **even if count is even**.
- 5 odd numbers > sum is odd > can't equal 30 (even).

**Trick answers (lateral thinking):**
- Use a number twice and call it different: not really fair.
- Use bases: write 13 in base-X to get an even...
- **Most accepted:** "(15) + (7+8) where 8 is from rotating 11 - if read upside down, 11 looks like..." > weak.
- **Honest answer:** "It's mathematically impossible with the standard interpretation. The puzzle is testing whether you spot the parity invariant."

**Say this:** "Five odd numbers summing to even is impossible by parity. If we're allowed to rotate, concatenate, or add operators (3+3=6 instead of 3 used twice as 33), there are tricks - but the math answer is no."

### Puzzle 3: x^y vs y^x

**Question:** "When is x^y > y^x?"

**Insight:** take ln of both sides:
> y * ln(x) > x * ln(y)
> ln(x)/x > ln(y)/y

So define f(t) = ln(t)/t. Compare f(x) vs f(y).

**Behavior of f(t) = ln(t)/t:**
- f'(t) = (1 - ln(t)) / t^2
- f'(t) = 0 at t = e (~2.718)
- f increases on (0, e), decreases on (e, infinity)
- Maximum at t = e

**Implications:**

| Range | Result |
|---|---|
| Both x, y > e (i.e., >= 3) | f decreasing > **smaller value > larger f > smaller^larger > larger^smaller**. So x^y > y^x iff x < y. |
| Special pair (2, 4) | 2^4 = 16, 4^2 = 16 > **equal** (the famous tie) |
| One < e, one > e | depends on actual values; case by case |
| Both < e (i.e., 1 or 2 only meaningful) | f increasing > larger^smaller > smaller^larger |

**Quick reference:**
- 2^3 = 8, 3^2 = 9 > 3^2 > 2^3 (3 > 2, both crossing e boundary)
- 2^4 = 4^2 = 16 (tie)
- 2^5 = 32, 5^2 = 25 > 2^5 > 5^2 (now 2 < e, 5 > e behavior)
- 3^4 = 81, 4^3 = 64 > 3^4 > 4^3 (both > e, smaller^larger wins)

**Say this:** "Take ln of both sides. The question reduces to comparing ln(x)/x vs ln(y)/y. That function peaks at e ~= 2.718 and decreases after. So for x, y both >= 3, the smaller base raised to the larger exponent wins. The famous tie is 2^4 = 4^2."

### Bonus: classic puzzle warmups (be ready)

- **100 prisoners, 100 boxes** (loop strategy, ~31% success).
- **Burning rope** (light both ends + one end of second to time 45 min from two 1-hr ropes).
- **Bridge crossing** (2 fastest cross first/return, slowest cross together).
- **Manhole covers round** (round shape can't fall in; weight; rolls).
- **Number of golf balls in a bus** (Fermi estimation - structure matters more than answer).

---

## SECTION 8 - Coding practice (5 problems, 2-3 hrs, do today)

> Mix of LC patterns + C systems concepts. Code these end-to-end today, rest tonight, skim notes tomorrow morning. Suggested order is warmup -> hardest.

### Problem 1 (C systems, 25 min): MMIO field write + polling with timeout

```c
/*
 * - REG_BASE = 0xFE000000, link-status reg at offset 0x10
 * - Bits [7:4] = link speed (4 bits)
 * - Bit  [0]   = link ready
 *
 * Implement:
 *   void  set_field   (uintptr_t addr, uint32_t shift, uint32_t mask, uint32_t val);
 *   int   wait_for_bit(uintptr_t addr, uint32_t bit, unsigned timeout_us);
 *
 * Stub the MMIO write/read with a static uint32_t for testing.
 */
```

Practice: `volatile`, mask + shift, RMW pattern, polling-with-timeout, `udelay` stub.
Probe-after: what if you forget `volatile`? What if two threads call `set_field` on the same reg?

### Problem 2 (C concepts, 30 min): Array of function pointers - opcode VM

```c
typedef struct {
    uint32_t reg[4];
    uint32_t pc;
    uint8_t  mem[256];
    int      halted;
} cpu_t;

/* Opcodes:
 *   0x01  ADD  Rd, Rs   (next byte: high nibble=Rd, low nibble=Rs)
 *   0x02  MOV  Rd, imm8 (next byte: imm; one after: Rd)
 *   0x03  JMP  addr8    (next byte: target)
 *   0xFF  HALT
 *
 * Implement:
 *   typedef void (*op_fn_t)(cpu_t *);
 *   static op_fn_t handlers[256] = { [0x01] = op_add, ... };
 *   void run(cpu_t *c);
 *
 * Test program: MOV r0,5; MOV r1,7; ADD r0,r1; HALT.  Expect r0 == 12.
 */
```

Practice: function pointers, designated initializers, sparse dispatch table, NULL check, state-machine pattern.

### Problem 3 (LC 206, 20 min): Reverse linked list - both ways

```c
struct ListNode { int val; struct ListNode *next; };

struct ListNode* reverseList_iter(struct ListNode *head);
struct ListNode* reverseList_rec (struct ListNode *head);
```

3-pointer iterative is the universal embedded-friendly answer. Bonus extension if time: LC 25 reverse-K-group.

Practice: pointer surgery, edge cases (empty/single), iter vs recursion trade-off.

### Problem 4 (LC 21, 25 min): Merge Two Sorted Lists

```c
struct ListNode* mergeTwoLists(struct ListNode* l1, struct ListNode* l2);
```

Two acceptable styles - know both:
1. **Iterative with dummy head** - cleaner, no special-case for empty lists.
2. **Recursive** - elegant but eats stack on long lists; mention you'd avoid in embedded.

**Bonus discussion (don't code, just be ready to talk):** how you'd extend to K lists. *"Pairwise via this same function = O(N log K) divide-and-conquer. Or push K heads into a min-heap and pop-then-push-next = O(N log K). Small K -> D&C; huge K -> heap amortizes better."*

Practice: dummy-head trick, pointer surgery, recursion-vs-iteration trade-off in embedded context.

### Problem 5 (LC 146, 50 min): LRU Cache - O(1) get/put

```c
typedef struct LRUCache LRUCache;

LRUCache* lRUCacheCreate(int capacity);
int       lRUCacheGet  (LRUCache* obj, int key);
void      lRUCachePut  (LRUCache* obj, int key, int value);
void      lRUCacheFree (LRUCache* obj);
```

Pattern: doubly-linked list (O(1) move-to-front, remove-tail) + hashmap (O(1) lookup -> node pointer).

Why this matters for your role: **LRU is the eviction model used in CPU caches, page caches, TLB pseudo-LRU, NPU context cache.** Knowing it cold lets you discuss real cache replacement policies in interviews.

Practice: dual data-structure design, struct layout, intrusive vs separate-allocation trade-off.

### Coverage check vs the rest of this guide

| Concept | Problem |
|---|---|
| MMIO + volatile + mask/shift + polling | #1 |
| Function pointers + array-of-fp + state machine | #2 |
| Pointers + linked lists + iter vs rec | #3 |
| Algorithmic thinking + heap vs D&C | #4 |
| Cache concepts + dual data structure | #5 |

---

## SECTION 9 - Quick technical reflexes (drop these in answers)

### One-liner reflexes

| Topic | Reflex |
|---|---|
| Why volatile for MMIO | tells compiler not to cache/elide/reorder; doesn't give CPU memory ordering |
| MMIO read | `*(volatile uint32_t*)addr` |
| Set bit n | `reg \|=  (1u << n)` |
| Clear bit n | `reg &= ~(1u << n)` |
| Read field | `(reg >> SHIFT) & MASK` |
| Function pointer | `int (*fp)(int, int) = &add; fp(2,3);` |
| Page fault first time | demand paging - kernel allocates physical page, fills PTE |
| TLB miss | HW page walk (x86) - up to 4-5 memory accesses |
| MESI false sharing | two threads write same 64B line > ping-pong > pad to 64B |
| ARM weak ordering | producer needs `dmb st` between data and flag |
| x86 TSO | only store > later-load can reorder |
| Branch mispredict cost | ~pipeline depth x IPC = 15-20 cycles |
| AVX-512 license | F drops 25% under sustained AVX-512 |
| RAPL caveat | VR-input estimate, 20-30% off wall power |
| C-state break-even | enter only if idle > 2x wake latency |

### "What would you do if..." frameworks

| Scenario | Framework |
|---|---|
| New silicon arrives, where do you start? | Smoke test > IO link bring-up > basic CPU/NPU functional > perf characterization > stress > corner cases |
| Regression appeared | Bisect (last known good vs first known bad) > minimize repro > isolate variable > hypothesis > test > fix > add to regression suite |
| Customer escalation | Reproduce locally > understand business impact > parallel: workaround for customer + root cause > document |
| Cross-team conflict | Get the data > 1:1 first, escalate only after > propose solution not just problem > document decision |

---

## SECTION 10 - Final 10-min checklist

- [ ] Career narrative (60 sec) rehearsed out loud
- [ ] Why this role / why now answer ready
- [ ] One bug story you OWN end-to-end (Core C6 OR NVMe CSTS.RDY OR QNN page-fault)
- [ ] Thermal-headroom answer (7 levers)
- [ ] DVFS in 60 sec (V^2*F, P-states, PCU/RPMh)
- [ ] C-state entry+exit flows (8 steps each)
- [ ] NPU stack diagram (5 layers, who does what)
- [ ] Silent EP fallback explanation
- [ ] One real NPU bug story
- [ ] 2-egg formula: X(X+1)/2 >= N, first drop = 14 for N=100
- [ ] x^y > y^x: smaller base wins for both >= 3, tie at (2,4)
- [ ] Sum 1-15 odd to 30: impossible by parity
- [ ] Underperforming-teammate framework (can't vs won't)
- [ ] 3 questions ready for them
- [ ] Honest framing: tech-lead-ready, growing into manager
- [ ] Breathe. Calm. Specific. Concrete. Let them drill if they want depth.

---

## SECTION 11 - Intel Ireland JD: line-by-line mapping (read morning of)

> The JD is your interview script. For each bullet they wrote, know the one example and the one day-anchor that backs it. Recruiters and hiring managers grade on "did they speak to our JD?" — answer in *their* vocabulary.

### "What you'll do" → my evidence

| JD bullet | One-sentence answer | Day anchor | Bug story to deploy |
|---|---|---|---|
| Validate next-gen AI silicon (NPU) across workloads | "I run NPU validation today on Intel LNL, AMD XDNA, and Qualcomm Hexagon HTP v79 across Photoshop, Paint SR, Camera SR, and Llama-class quant models." | Day 18-19 | Silent EP fallback (5) |
| Debug pre- and post-Si — intermittent, multi-domain | "Pre-Si: Cambria Core C6 wake-loss, 1-deep CFC FIFO race, caught only under overlapping snoop+IRQ. Post-Si: QNN context-cache invalidation regression across a driver bump." | Day 14, 19 | Core C6 (3), QNN page-fault (4) |
| Develop validation infra (sim / emul / FPGA / post-Si) | "Zebu emulation flows + Perspec test content + 10+ PCIe Gen6 transactor enhancements in C++ at Intel; EdgeOptimizer post-Si profiler at Microsoft." | Day 10, 21 | n/a |
| Characterise + profile silicon power/perf on AI workloads | "RAPL + ETW + WPA pipeline, energy-per-inference, EDP, geomean across model suite, per-IP RAPL domain attribution." | Day 15, 18 | n/a |
| Power/perf tuning with arch / design / SW | "Worked with ORT + driver teams to root-cause EP fallback regressions; tuned PL1/PL2/Tau for sustained NPU workloads." | Day 12, 15 | n/a |
| Read HW arch specs → derive targeted tests | "PCIe Gen6 spec → FLIT mode + L0p test scenarios for transactor; CXL spec → .io/.cache/.mem coverage planning." | Day 10 | n/a |
| Share findings + best practices | "Internal EdgeOptimizer docs, brown-bags, cross-vendor methodology write-ups; agentic-Copilot CLI workflow shared with the team." | n/a | n/a |

### "Required" → my evidence

| Required | Answer (one liner) |
|---|---|
| Strong debug instinct | "Observe → reproduce → isolate → hypothesise → test → root-cause → fix. Convert vague failure into one controlled signal." (Day 20.) |
| Full-stack curiosity | "I'm comfortable from RTL/emulation traces up through firmware, kernel drivers, ETW, and OS scheduler — that's literally how my four headline bug stories were debugged." |
| Learning agility | "Picked up QNN, Hexagon HTP, and ORT EP from spec + source in ~2 months at Microsoft, with no vendor training. Same with PCIe Gen6 at Intel." |
| C/C++ + memory model + concurrency | "C daily for systems code; understand TSO vs weak memory models, atomics, false sharing, SPSC ring patterns." (Day 5, 16, 17.) |
| Python scripting | "Python harness for NVMe power scenarios at L&T; ETW post-processing in EdgeOptimizer; data analysis with pandas." |
| Communication | "Three polished 90-sec bug stories ready; written design docs + decision logs are how I work." |

### "Desired" → my evidence

| Desired | Answer |
|---|---|
| OS-level driver experience (Linux/Windows) | "Windows: ETW, DxgKrnl events, WPA, WPR; KMDF basics. Linux: cpuidle / cpufreq governors, perf, ftrace, cgroups." |
| Embedded toolchains | "gcc/clang, gdb, JTAG/Metaware (L&T NVMe), Make/CMake, git, Perforce (Intel)." |
| FPGA / emulation / RTL sim | "Zebu emulation owner on Cambria; Perspec; PCIe Gen6 transactor in C++; pre-Si signal-level debug." |
| Bash / PowerShell / Tcl | "Bash + PowerShell daily; Tcl exposure from EDA flows." |
| Perf profiling / benchmarking / power measurement | "EdgeOptimizer = the answer. RAPL + ETW + per-inference attribution; Quarch external power as ground truth; energy-per-inference metric." |
| Silicon validation / hardware bring-up / FW | "L&T NVMe post-Si bring-up (JTAG, scope, Lecroy); Intel pre-Si Cambria PCU/PMA flows." |

### "Stand-out" angles to volunteer (don't wait to be asked)

- **Cross-vendor methodology** — most validators see one silicon family; I've validated three NPU architectures with one common methodology, so I know what's vendor-specific vs what's universal.
- **Tool-builder mindset** — EdgeOptimizer + agentic Copilot CLI scaled the team from 2-hour WPA triage to 15-min report. Any team I join, I look for the same multiplier.
- **Pre-Si + post-Si seam** — the JD specifically wants both, and I've done both (separately). Joining Intel Ireland is the chance to do both on one product.
- **Spec-to-test discipline** — at Intel I derived PCIe Gen6 transactor scenarios directly from the spec; same approach scales to NPU IP specs.

### What to bring to the loop (logistics)

- 2-3 printed copies of the 30 / 60 / 90 sec narratives.
- One whiteboard-ready diagram each: NPU stack (5 layers), ECAM formula, MESI, C-state entry/exit, RAPL domains.
- Photo IDs + visa/permit indicative answers ready in case HR asks.
- Bottle of water; arrive at the building 20 min early.

---

> **Senior-bar reminder:** at this level they're checking judgment more than knowledge. When they ask a tech question, give the **structured answer + the trade-off + what you'd actually do.** When they ask a behavioral question, give the **framework + a real example + the lesson.** When they ask a puzzle, **think aloud** - they want to see how you reason, not whether you've seen it before.



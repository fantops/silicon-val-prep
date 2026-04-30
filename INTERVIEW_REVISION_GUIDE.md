# Senior Silicon Validation Engineer — Interview Revision Guide

> **Generic reference for any silicon validation interview** — pre-Si CPU core, post-Si bring-up, NPU/AI accelerator, system/SoC integration, platform validation. Tune the lens (Section 2A) to the role.
>
> **Day-of interview prep** → [Section 0A](#section-0a---day-of-interview-prep). **Reflex cheat sheet** → [Section 0](#section-0---last-10-min-cheat-sheet). **20-day system-thinking one-liners** → [Section 4B](#section-4b---20-day-system-thinking-one-liner-reference).
>
> Doc map:
> - **Day-of prep** (5-min checklist + cultural notes per company type) — Section 0A
> - **Reflex cheat sheet** (single-line answers to recurring probes) — Section 0
> - **Career narrative** + 3 reusable current-work framings — Section 1
> - **Behavioral / leadership** — Section 2
> - **Adaptive answering** (pre-Si / post-Si / system-level) + universal frameworks (new-problem, coverage, workload distribution) — Section 2A
> - **Backup tech depth** — Sections 3 (thermal/power), 4 (DVFS), 5 (C-states)
> - **Bug story bank** (5 stories, tagged by lens) — Section 4A
> - **20-day system-thinking one-liner reference** — **Section 4B** *(distilled from the technical day notes — every concept boiled to a tradeoff one-liner)*
> - **Pre-Si CPU core depth** — Section 6A
> - **Post-Si NPU depth** — Section 6
> - **Puzzles + quick reflexes + final checklist** — Sections 7, 9, 10
>
> **Strategy at the senior IC + lite-managerial bar:** be **clear, specific, calm**. One concrete example beats three textbook bullets. Always frame the *tradeoff* before the *fact*. If you don't know — say so cleanly and pivot to the closest thing you do know.

---

## SECTION 0A - Day-of interview prep

> **Use this 30 minutes before any silicon-validation interview.** First detect the role lens (pre-Si / post-Si / system) and the company culture, then run the universal checklist.

### Step 1 — Detect the role lens (60 sec from the JD)

| Cues in JD / recruiter brief | Lens to lead with |
|---|---|
| "RTL", "DV", "UVM", "emulation", "Zebu/Palladium", "coverage plan", "pre-tape-out", "formal" | **Pre-Si** → Section 6A is your spine |
| "Bring-up", "first silicon", "EVT/DVT/PV", "JTAG", "lab", "OEM qualification", "field issue", "stepping" | **Post-Si** → Section 6 is your spine |
| "Platform", "SoC integration", "end-to-end", "cross-IP", "performance characterisation", "stack" | **System-level** → Section 2A Lens 3 is your spine |
| "NPU", "AI accelerator", "ORT", "EP", "inference", "Hexagon HTP", "OpenVINO" | **NPU/AI** → Section 6 + Day 18-19 one-liners |

If the JD mixes lenses → Section 2A "How to detect which lens to use" + the question *"Are you thinking pre-Si in emulation, post-Si on a board, or end-to-end on a platform?"* — that single question buys credibility.

### Step 2 — Match the company culture (60 sec)

| Company type | Tone | Common round 2 shape | Watch out for |
|---|---|---|---|
| **Big-die CPU vendor** (Intel, AMD, Apple silicon) | Methodical, framework-friendly, deep tech drilldowns | DV/coverage Q + a hazard/MESI/pipeline drilldown + behavioral | They will know more than you on their domain — humility wins |
| **ARM SoC vendor** (Qualcomm, MediaTek, NXP) | Direct, concise, data-led | Pre-Si CPU/cluster Q + ARM vocab probe + bug story | Don't fake ARM-native depth; *map cleanly from x86* instead |
| **GPU / Accelerator** (NVIDIA, AMD GPU, Cerebras, Tenstorrent) | Architecture-forward, perf-obsessed | Workload+memory-system Q + scheduling/concurrency + system bug story | Lead with workload-thinking, not just unit tests |
| **Hyperscaler silicon** (AWS Annapurna, Google TPU, Meta MTIA) | Product-impact framing, fleet-scale reasoning | Live-migration / blast-radius / counter-driven debug | Show you reason about *fleet* failure modes, not single dies |
| **Storage / Networking** (Marvell, Broadcom, Pure, NetApp) | Protocol-deep, spec-literate | NVMe/PCIe/RDMA bring-up + protocol corner | Lead with spec literacy + reproducer discipline |
| **Microsoft / Apple / Google platform** | OEM/system framing | Cross-stack regression + tooling + leadership | Lead with cross-vendor breadth + tooling impact |

### Step 3 — Pick your headline framings (memorise the spine)

Pick the 3 that land hardest for **this** company/lens:

1. **The role-fit one-liner.** "[My pre-Si experience / my post-Si bring-up experience / my platform-NPU experience] is exactly the [pre-Si / post-Si / system] depth this role wants — and the second half of my CV adds the [breadth / debugging / tooling] that closes the loop."
2. **The why-now one-liner.** Either *"I want to bring [the lens I have less of] back into my work"* or *"I want to deepen the lens I already have on [a more leveraged silicon programme]."*
3. **The why-this-company one-liner.** Two specific sentences about their product / team / programme. Generic "great company" framing kills the round.

### Step 4 — The universal cheat-line table (use verbatim if asked)

| Common Q | Universal one-liner |
|---|---|
| Why pre-Si after post-Si? | "Post-Si proved I debug across the stack. Pre-Si is where I want to apply that — find the bug before it costs a respin." |
| Why post-Si after pre-Si? | "Pre-Si gave me RTL discipline; post-Si is where I want to see what escapes look like in real silicon and feed that back upstream." |
| Why system-level / platform after IP-level? | "Owning one IP teaches you depth. Owning the platform teaches you which seams hide bugs no IP-level test will ever find." |
| What can you bring on day 1? | "[Tool / methodology / vocabulary] from my [previous lens] + low-ego learner on the parts I don't know yet." |
| Where are you weakest? | Name a *specific* gap (e.g. "ARM-native micro-arch vocab", "GPU-shader-level perf"), state the bridge ("I map fluently from X"), state the closing plan ("I closed a similar gap on Y in 2 months"). |
| Why are you leaving your current role? | Frame *toward* the new thing, never *away from* the old. "Going toward [X]; current role gave me [Y] which is the foundation." |

### Step 5 — Day-of behavioural rehearsal (most likely Qs in any round 2)

Each is 60–90 sec, STAR if applicable. Rehearse 5 of these out loud once before the call:

1. **"Walk me through your career arc."** Lead with the *arc*, not chronology. Why each move, what each layer taught you, what you want next.
2. **"Tell me about the hardest bug you've owned."** → Section 4A; pick the bug that matches the lens (table at end of 4A).
3. **"Tell me about a time you disagreed with a designer / architect."** → Section 2 template. Disagree-and-commit, data-led, no blame.
4. **"Tell me about a time you mentored a junior engineer."** → KT doc → pair-debug → solo with safety net → independent. Real example, not template.
5. **"How do you handle ambiguous specs?"** → "Spec ambiguity is a bug. Read, write down my interpretation, file a clarification with arch + IP owner together, document, add a coverpoint or assertion that pins the resolution."
6. **"Tell me about a time you missed something."** → Honest, short, not catastrophic. The lesson > the miss.
7. **"What does a good day look like for you?"** → "A coverage hole closed, a sporadic fail isolated to a deterministic repro, and a 1:1 where I unblocked someone else."
8. **"Why should we hire *you* for this team?"** → 30-sec close: lens-relevant experience + cross-stack context + tool-builder mindset + low-ego learner. Specific to *their* team.

### Step 6 — Tech questions you should be ready for in any silicon validation round

Pick the one that comes up — go deep, don't sprint. Cold rehearse 1, 2, 5.

1. **"How would you write a coverage plan for a new feature?"** → Section 2A "Planning coverage" 10-step recipe.
2. **"How do you debug a sporadic emulation / silicon failure?"** → "Lock seed → minimise repro → bisect (RTL/FW/test/seed) → waveform window around divergence → don't touch design until failure is deterministic and minimal."
3. **"Pipeline hazard / forwarding."** → Day 16 one-liner; RAW/WAR/WAW; rename eliminates WAR/WAW; ROB for precise exceptions; mispredict cost ≈ pipeline depth × IPC.
4. **"Memory ordering — strong vs weak."** → Day 16 one-liner; x86 TSO vs ARM weak; barriers; acquire/release.
5. **"Coherence — walk me through MESI."** → Day 5 one-liner; M/E/S/I; snoop vs directory; false sharing as 4th miss class.
6. **"Functional vs code coverage."** → "Code = did the line execute (necessary, not sufficient). Functional = did the *intended scenario* fire (the real metric)."
7. **"Failing assertion — test bug, RTL bug, or spec bug?"** → Reproduce → re-read assertion vs spec → check test preconditions → check RTL under legal stimulus → loop in arch when ambiguous; never silently disable.
8. **"How would you bring up a new CPU core in emulation, day 1?"** → Section 6A canonical walkthrough.
9. **"Walk me through system boot."** → Day 9 generic 5-phase boot (POWER → ROM → PLATFORM INIT → BOOTLOADER → OS+INIT) + chain of trust.
10. **"Why multi-level cache / SRAM vs DRAM / virtual memory / clock gating / multiple C-states / ..."** → Section 4B for the framing on each.

### Step 7 — What to NOT volunteer

- Don't lead with the role lens you have *less* of. If applying for pre-Si, lead with pre-Si — even if your most recent work is post-Si.
- Don't oversell vocabulary you don't have natively. *Showing* you map cleanly is stronger than *claiming* native depth.
- Don't anchor on stories that don't match the lens. (See Section 4A "How to pick which story" table.)
- Don't mention parallel processes / other interviews unless asked. If asked: *"I'm interviewing in the same band — silicon-side validation roles. This one is the most interesting because [specific reason about their product]."*

### Step 8 — Final 5-minute pre-call checklist (run literally 5 min before)

- [ ] Water in arm's reach. Phone on DND. Notebook + pen.
- [ ] 30-sec narrative said out loud once.
- [ ] One bug story rehearsed once (the one that matches the lens — see Section 4A table).
- [ ] One coverage-plan answer rehearsed (the "how would you cover X" question).
- [ ] One ARM↔x86 mapping ready if it's an ARM shop (PCU↔RPMh, IOMMU↔SMMU, PCID↔ASID, TSO vs weak).
- [ ] 3 questions for them written on a sticky:
   1. Sprint-level: *"What's the split between infra work, test content, and debug on this team in a typical sprint?"*
   2. Programme-level: *"Where is this product/IP in its programme cycle?"*
   3. Org-level: *"How does this team interact with [the adjacent team — pre-Si↔post-Si, vendor↔OEM, IP↔SoC] when issues cross the boundary?"*
- [ ] Camera on. Smile on first line.

---

## SECTION 0 - Last-10-min cheat sheet

> **Generic reflexes for any silicon-validation round.** Scan once before the call. Each row is a single line you can deliver verbatim under pressure.

### Spine reflexes (career + role-fit)

| Theme | One-liner reflex |
|---|---|
| **Why pre-Si after post-Si stint** | Post-Si proved I debug across the stack; pre-Si is where I want to apply that judgement and shape silicon before it's cast |
| **Why post-Si after pre-Si** | Pre-Si gave me RTL discipline; post-Si is where I see what escapes look like in real silicon and feed it back upstream |
| **Why system / platform** | Owning one IP teaches depth; owning the platform teaches which seams hide bugs no IP-level test will ever find |
| **What I bring on day 1** | Lens-relevant experience + cross-stack debug + tool-builder mindset + low-ego learner on what I don't know yet |

### Methodology reflexes (every silicon-validation interview hits these)

| Theme | One-liner reflex |
|---|---|
| **Sporadic emulation/silicon fail debug** | Lock seed → minimise repro → bisect (RTL/FW/test) → waveform window around divergence → don't touch design until failure is deterministic and minimal |
| **Coverage plan recipe** | Spec → state space → functional cover groups + crosses → directed corners → constrained-random fill → assertions for protocols → review with arch + design → exit criteria up front |
| **Functional vs code coverage** | Code = "did the line execute"; functional = "did the *intended scenario* fire". Functional matters more — 100% line + 0 functional intent finds nothing |
| **Failing assertion triage** | Reproduce → re-read assertion vs spec (spec ambiguity?) → check test preconditions → check RTL under legal stimulus → loop in arch when ambiguous; never silently disable |
| **Approaching a new problem** | Restate in one sentence → define done → inventory known-vs-assumed → find cheapest disambiguating signal → minimum reproducer → write down plan → timebox |

### Tech-spine reflexes (concept-per-row, distilled — full one-liners in [Section 4B](#section-4b---20-day-system-thinking-one-liner-reference))

| Theme | One-liner reflex |
|---|---|
| **Why multi-level cache** | Single cache cannot be 1-cycle fast AND 32 MB AND shared — physics, area, contention forbid it. Hierarchy is the resolution |
| **SRAM vs DRAM** | 6T latch (fast, sparse) vs 1T1C cap (dense, leaks, needs refresh). Use SRAM where latency dominates, DRAM where capacity does |
| **Cache coherence (MESI)** | Silicon tax for keeping the illusion of one memory across N private caches. False sharing = 4th miss class (coherence) |
| **Virtual memory** | One indirection that solves isolation + sharing + oversubscription. Page fault is the kernel's "do work lazily" signal |
| **TLB / huge pages** | Translation has its own locality, needs its own cache. Huge pages stretch TLB reach — tradeoff is fragmentation + coarser perms |
| **DMA / barriers** | Descriptor → barrier → doorbell. Two ordering universes meeting; cache management bridges non-coherent fabrics |
| **PCIe** | Packet-switched layered serial network. LTSSM trains because Gbps links can't just turn on. Credits eliminate idle on lossless link |
| **NVMe vs AHCI** | Per-core SQ/CQ kills the queue lock; MSI-X per CQ kills cross-core wakes; phase-bit polling kills IRQ at high IOPS |
| **DVFS / P-states** | $P_{dyn} = CV^2F$, V & F coupled by physics → near-cubic energy savings. Governor exists because HW sees load but not intent |
| **Static vs dynamic power** | Dynamic = $CV^2F$ (cost of toggling). Leakage = $V \cdot I_{leak}(T)$ (cost of being on, exponential in T) |
| **Clock gating** | RTL `if(en)` rewritten by synthesis into ICG cells (latch+AND, no glitch). Three tiers: leaf / regional / root |
| **Why C-state ladder** | Each state has a break-even residency — too short and entry/exit cost > savings. Single state is wrong for both short and long idles |
| **System boot** | 5 phases everywhere: POWER → ROM → PLATFORM INIT (DDR train) → BOOTLOADER → OS+INIT. Chain of trust at every hop, fuses pin the root |
| **OoO / consistency model** | OoO extracts ILP from a serial stream; consistency model trades HW freedom for SW programmability (TSO = strong, ARM = weak) |
| **Lock-free queues** | Universal primitive for decoupling; SPSC fastest, MPMC slowest; ABA needs tagged ptrs; head/tail must be on separate cache lines |
| **NPU vs CPU SIMD** | Dedicated matmul silicon is 100–1000× more energy-efficient than CPU SIMD; cost is separate ISA, memory, address space, power domain |
| **Where NPU bugs live** | At the boundary — CPU↔NPU memory, VA↔IOVA, coherent↔non-coherent, runtime↔driver. Each domain correct in isolation, contract is fragile |
| **Debugging method** | Method beats intuition. Bisection + symptom-vs-cause + hypothesis-and-bound. Story polish: precise symptom → first hypothesis → bound → fix → regression hook |

### ARM ↔ x86 mapping (memorise if interviewing at an ARM shop)

| ARM | x86 |
|---|---|
| RPMh | PCU |
| SMMU | IOMMU |
| ASID | PCID |
| WFI/WFE | MWAIT |
| System register (MSR-equiv) | MSR |
| OPP (Operating Performance Point) | P-state |
| PMIC over SPMI | FIVR |
| LMh (Limits Mgmt) | PL1/PL2 |
| EL0/1/2/3 + Secure | Ring 3/0 + SMM |
| PSCI SMC | INIT-SIPI / ACPI |
| LDAR / STLR / DMB ISH | implicit (TSO) |

### Behavioural / leadership reflexes

| Scenario | Reflex |
|---|---|
| Lead a team of 15 | Tech-lead + sub-leads, daily standups, weekly 1:1s, ownership matrix, escalate fast. Most senior IC + small-team lead is comfort zone |
| Underperforming teammate | First conversation: **can't** (skill/blocker — pair/scope/escalate) vs **won't** (motivation — direct conversation, written expectations, perf process if needed) |
| Disagree with designer | Disagree-and-commit, data-led, escalate via 1:1 first never email-war, document outcome |
| Strength | Cross-stack debug + tool-builder mindset |
| Weakness | Specific gap + bridge from adjacent fluency + closing plan with timeframe |
| Conflict resolution | Get the data → 1:1 first → propose solution not just problem → document the decision |

### Puzzle reflexes

| Puzzle | Reflex |
|---|---|
| 2-egg / N-floor | $X(X+1)/2 \geq N$ → first drop = 14 for N=100, worst case 14 drops |
| Sum of 5 odd nums = 30 | Impossible by parity (5 odd → sum is odd) |
| $x^y$ vs $y^x$ | Compare $\ln(x)/x$ vs $\ln(y)/y$; peaks at $e$; both $\geq 3$ → smaller base wins. Famous tie: $2^4 = 4^2$ |
| Burning rope | Light both ends of one + one end of other → 45 min from two 1-hr ropes |

---

## SECTION 1 - Career narrative (own this cold)

### 60-second version
> "Silicon validation engineer with 8 years across the full silicon lifecycle. Currently at Microsoft doing NPU post-Si platform validation on Copilot+ PCs - across Intel Lunar Lake, AMD Strix, and Qualcomm Snapdragon X Elite. Built EdgeOptimizer, an internal tool that uses ETW + WPA + RAPL to do per-inference attribution of CPU/NPU utilization, energy, and silent-fallback detection. Before that, Intel Cambria Xeon pre-Si emulation - owned C-states, ADR, PCIe Gen6 transactor on Zebu, won Star of the Month for a CXL firmware bug debug. Started at L&T@SanDisk on NVMe post-Si bring-up - JTAG, Metaware, LeCroy, Python power-scenario harness, SDExpress POC Spot Award. M.Tech CSE NIT Durgapur 2018. Through-line is silicon validation across the stack from RTL traces up to OS scheduler."

### 30-second version
> "8 years silicon validation. Microsoft now - NPU platform validation Copilot+ PCs (Intel/AMD/QC), built EdgeOptimizer. Intel before - Cambria Xeon pre-Si emulation (C-states, ADR, PCIe Gen6). L&T@SanDisk before that - NVMe post-Si bring-up. M.Tech NIT Durgapur."

### Current work — three reusable framings (pick to match the audience)

Use the framing that matches who's asking. **All three describe the same job; the lens shifts.**

1. **Silicon-side framing (use for pre-Si CPU/SoC interviewers — Qualcomm today):**
 > "I run platform-level silicon validation for the NPU subsystem on Copilot+ PCs across three vendors. My job is to find behaviours that only show up at the platform level — graph-partitioning escapes, driver/firmware/HW interaction bugs, power/perf regressions across DVFS and C-states — and feed them back to the silicon teams as actionable bug reports with reproducers. I work the layer that catches what pre-Si missed and tells the next-gen silicon team what to cover."

2. **System / cross-stack framing (use for SoC, system-architecture, platform roles):**
 > "I own NPU validation across the full stack — app, ORT runtime, execution provider, driver, firmware, silicon — on Intel, AMD and Qualcomm parts. The interesting bugs live at layer boundaries: an op the EP doesn't claim, a driver context-cache invalidation that triggers a recompile, a thermal lever that throttles the NPU when the CPU is hot. I built EdgeOptimizer to attribute energy and utilisation per-inference so we can argue about the right layer instead of guessing."

3. **Tooling / leadership framing (use for staff / lead / EM rounds):**
 > "I lead the platform NPU validation charter at Microsoft for Copilot+ PCs. Beyond the regression and bring-up work, I built the internal tooling — EdgeOptimizer, ETW pipelines, per-inference attribution — that the wider team and our OEM partners now use. Most of my recent leverage has been in turning one-off debug into reusable infra so the team scales without linear headcount."

*Rule of thumb: pick the framing that matches the role lens (see Section 0A step 1). Hold the other two for follow-ups.*

### Why this role / why now (pick the variant matching the lens)
- *(Default / vendor-side pivot)*: "I've done pre-Si and post-Si on three different vendors' silicon. NPU post-Si specifically is the layer I've been pushing hardest in — EdgeOptimizer was my way of building tooling that scaled across vendors. I want to do that work where the silicon is being designed, not just consumed."
- *(Pre-Si CPU pivot)*: "Post-Si NPU work taught me what escapes look like in the field. Pre-Si CPU core is where I want to apply that — close those classes upstream before they ship."
- *(System-level pivot)*: "I want a role where the same person who finds a bug also shapes the coverage plan that catches its siblings — pre-Si or system-level both let me do that. Post-Si alone doesn't."
- *(Deeper post-Si / NPU pivot)*: "Microsoft gave me cross-vendor NPU breadth; I want to take that into a single-vendor team where I own the silicon end-to-end from EVT through PV."

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

### Q: "Why leaving your current role?"

**Frame positively, toward the new thing — pick the variant matching the lens:**

- *(Pre-Si CPU pivot)*: "Microsoft has been excellent for breadth — three vendors' NPUs and a tooling charter. The next thing I want is depth at the silicon side: working pre-Si on a CPU core where I can shape what gets cast, not just qualify what comes back. Post-Si taught me what escapes look like; I want to bring that lens upstream."
- *(System / SoC pivot)*: "The next thing I want is to own a subsystem end-to-end inside a silicon team, rather than be the platform-side consumer of three vendors' silicon."
- *(Deeper post-Si / NPU pivot)*: "Microsoft gave me cross-vendor breadth; I want to take that into a single-vendor team where I own the silicon end-to-end and the feedback loop with design is shorter."

### Q: "Where do you see yourself in 5 years?"

> "Principal-level IC leading a small team on a critical silicon platform - or an engineering manager of a 10-15 person validation team. Either way, I want to be the person OEMs and product teams escalate to when something doesn't make sense at silicon level."

### Q: "Question for us?"

Have 3 ready:
1. "What does the team's split look like between pre-Si and post-Si work?"
2. "What's the regression cadence? How does a silicon escape get diagnosed?"
3. "Where is the team in its growth - hiring to fill gaps, or hiring to scale?"
4. "What does success look like in this role at 6 months and 12 months?"
---

## SECTION 2A - Adaptive answering: pre-Si / post-Si / system-level

> **Use this when the interviewer says "how would you approach…".** The question shape is usually generic; the *good* answer is shaped to the role. Read the room first — pre-Si team, post-Si team, or system/SoC team — then pick the lens.

### How to detect which lens to use

| Cue from interviewer | Lens |
|---|---|
| "emulation", "RTL", "coverage plan", "testbench", "pre-tape-out", "DV" | **Pre-Si** |
| "bring-up", "first silicon", "OEM", "field issue", "escape", "regression on platform" | **Post-Si** |
| "end-to-end", "stack", "SoC integration", "platform", "cross-IP", "performance at system level" | **System-level** |

If unclear → ask: *"Are you thinking pre-Si in emulation, post-Si on a board, or end-to-end on a platform? My approach is similar but the first move differs."* That single question buys credibility.

### Generic answer template (works for any "how would you approach X?")

> **1. Frame the goal** (what does "done" look like — coverage %, perf target, escape budget) → **2. Map the surface** (state space, IP boundaries, stakeholders) → **3. Pick the cheapest signal** (sim < emulation < FPGA < silicon; or log < trace < scope) → **4. Build the smallest reproducer** before scaling → **5. Distribute the work** (who owns what, what's the merge story) → **6. Define exit criteria up front**.

Then specialise step 3 and 4 to the lens.

### Lens 1 — Pre-Silicon (the answer for today)

**Default first move:** "Read the spec, write the coverage plan, agree it with arch + design before writing a single line of stimulus." Coverage is a contract.

**Tools by stage:**
- *Block-level:* RTL sim (VCS/Xcelium) + UVM + SVA + formal for protocol invariants.
- *Cluster / subsystem:* sim for focused regressions, emulation (Zebu/Palladium) for stress and OS boot.
- *SoC:* emulation + FPGA prototype for SW co-dev.

**Signals you trust:** functional coverage, assertion coverage, scoreboard mismatches, X-propagation reports. Code coverage is necessary not sufficient.

**Mindset:** *"The test fails until I can prove the design is right under that stimulus. Don't change the design until the failure is deterministic and minimal."*

**One-liner if asked "how would you approach a new pre-Si feature?":**
> "Spec → state-space enumeration → functional coverpoints + crosses → SVA for invariants → CR stimulus for the bulk + directed for corners → review with arch and design → run, triage, close holes, repeat. Exit criteria agreed up front, not negotiated at tape-out."

### Lens 2 — Post-Silicon (your current work)

**Default first move:** "Bring-up checklist first — power, clocks, reset, IO link, smoke. Don't run a workload on silicon you haven't proved comes out of reset cleanly."

**Tools by stage:**
- *Bring-up:* JTAG, scope, LeCroy/protocol analyser, lab PSU + Quarch/Monsoon for power, vendor debugger.
- *Functional:* OS-level test suites, vendor SDK conformance, golden-vector replays.
- *Perf / power:* RAPL + ETW + WPA, per-IP counters, thermal cameras, sustained-workload harness.
- *Field / OEM:* telemetry, crash dumps, repro on golden HW, A/B with last known good driver/FW.

**Signals you trust:** deterministic repro on a known-good HW + driver + FW combination. Everything else is noise.

**Mindset:** *"Silicon is fixed. My job is to characterise it, find the escapes, and either get a workaround into FW/driver or a fix into the next stepping. Every bug needs a reproducer that the silicon team can run without the platform."*

**One-liner if asked "how would you approach a new post-Si bring-up?":**
> "Power-on smoke → IO links up → boot to OS → vendor sanity suite → functional regression → perf characterisation → stress + thermal + power → corner cases. Each milestone gates the next; failures get a minimal reproducer before they leave my desk."

### Lens 3 — System-level / SoC integration

**Default first move:** "Map the IP boundaries and the data/control flows that cross them. The bugs live at the seams."

**Tools:** end-to-end traces (Perfetto, ETW, ftrace), per-IP perf counters, system-level SVA at AXI/CHI boundaries, fault injection, workload mixes (real apps not microbenchmarks).

**Signals you trust:** end-to-end latency/throughput under realistic mixed workloads; counter consistency across IPs (does CPU's view of memory traffic match the interconnect's view?).

**Mindset:** *"Each IP can pass its unit tests and the system can still be wrong. My job is to design the workloads and instrumentation that exercise the integration."*

**One-liner if asked "how would you approach a system-level perf regression?":**
> "Reproduce on golden config → instrument every IP on the path → bisect (workload, kernel, driver, FW, silicon stepping) → identify the IP whose counters diverged → hand off to that IP team with a minimal repro and the trace. Don't blame; bring data."

### Cross-cutting frameworks (every lens uses these)

#### Approaching a new problem (the universal recipe)

1. **Restate the problem in one sentence** out loud or in writing. If you can't, you don't understand it yet.
2. **Define done** — what observable, measurable thing tells you you're finished?
3. **Inventory what's known vs assumed** — list assumptions explicitly; each is a future bug.
4. **Find the cheapest signal that disambiguates** — a 10-min experiment that rules out a hypothesis is worth more than a 2-day perfect one.
5. **Build the minimum reproducer** before scaling.
6. **Write down the plan** (3-7 bullets), share with one peer, then execute.
7. **Timebox** — if no progress in N hours, change approach or ask for help. Stuck-time is the enemy.

*Use this verbatim when asked "how would you approach a problem you've never seen before?"*

#### Planning coverage (works for any IP, any lens)

1. **Spec read** — every legal state, transition, stimulus. Write down ambiguities; file them.
2. **State-space enumeration** — variables × ranges. Bin them; don't try to cover 2^N.
3. **Functional coverpoints** — per-state, per-transition, per-event. Tag each to a spec section.
4. **Crosses** — state × stimulus × neighbour-IP-state. This is where most real bugs live.
5. **Corner buckets** — min, max, zero, overflow, back-to-back, interrupted, error-injection.
6. **Assertions / invariants** — protocol rules expressed as SVA (pre-Si) or runtime checks (post-Si).
7. **Stimulus strategy** — directed for the ugly corners, constrained-random for the bulk, real workloads for system-level.
8. **Exit criteria** — % functional, % code, zero failing assertions, N clean regressions. **Agreed up front.**
9. **Review** — with arch + design + (post-Si: with FW + driver owners). Coverage plan is a contract.
10. **Iterate on escapes** — every found bug → "what coverpoint would have caught this?" → add it.

#### Distributing workload (the tech-lead answer)

When asked "you have N engineers and M weeks — how do you split the work?":

1. **Decompose by IP / feature / lifecycle stage**, not by person. Get the work shape right first.
2. **Identify the critical path** — what blocks tape-out / bring-up / ship. Staff that first, with your strongest IC + a backup.
3. **One DRI per workstream** — directly responsible individual. Backup named. Escalation path written.
4. **Match skill to workstream** — stretch assignments for growth, but never put a stretch on the critical path without a senior backup.
5. **Parallelise the independent, serialise the dependent** — draw the dependency graph; anything not on the critical path can run in parallel.
6. **Build shared infra once** — testbench, regression harness, triage portal. Don't let three people each build their own.
7. **Cadence** — daily 15-min standup per sub-team, weekly 1:1s, weekly tech sync across leads, fortnightly milestone review.
8. **Buffer** — keep ~20% capacity unallocated for triage, escapes, OEM escalations. A 100%-allocated team has zero resilience.
9. **Visible state** — single dashboard of workstream health (green/yellow/red, owner, blockers). Update weekly.
10. **Re-plan at every milestone** — not at every standup. Plans that change daily aren't plans.

*One-liner version:* "Decompose the work, name a DRI per stream, staff the critical path first with backup, parallelise the independent, keep 20% buffer, and re-plan at milestones not standups."

---

## SECTION 3 - Thermal + Power Behavior (backup if asked)

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

**Throttle-reason MSR (Intel):** `MSR_CORE_PERF_LIMIT_REASONS` (0x64F) tells you which limit fired — PROCHOT, Tjmax, PL1, PL2, Iccmax, VR thermal. Always step 1.

**RAPL caveat:** VR-input estimate, 20-30% off wall power. Ground truth = external Quarch / Monsoon.

---

## SECTION 4 - DVFS / P-states (backup)

**Concept:** silicon has stable (V, F) operating points = P-states. PCU (x86) / RPMh (Arm) picks one based on load, thermal headroom, power budget, battery. P_dyn ∼ V^2 * F → cubic savings from DVFS.

**x86 ↔ Arm vocab:** PCU ↔ RPMh, FIVR ↔ PMIC-over-SPMI, MSR_PERF_CTL ↔ PSCI, HWP ↔ OPP/CPRh, PL1/PL2 ↔ LMh.

**DVFS bug shapes:** binning escape (V too low at F), PCU FW opcode-ordering (Cambria-style), per-cluster V-rail penalty (paying for noisy neighbour's V).

*Headline pre-Si DVFS bug story (Cambria Core C6 wake-loss) lives in Section 4A as Bug 1.*

---

## SECTION 4A - Bug story bank (pick by lens; rehearse the top 3)

> **Pattern for every bug story (90 sec, STAR-ish):** *Context (1 line) → Symptom (1 line) → Hypothesis & repro (2 lines) → Root cause (2 lines) → Fix + impact (1 line) → Lesson / what I added to coverage (1 line).* End on the lesson, not the heroics.

### ⭐ Bug 1 — Cambria Core C6 wake-loss (PRE-SI, headline for today)

- **Context:** Intel Cambria Xeon, owned C6 entry/exit on Zebu emulation.
- **Symptom:** sporadic emulation hang — one thread never resumed after a timer wake under stress.
- **Repro:** built a directed stress pattern overlapping snoop traffic with timer IRQ at the C6 exit window; failure became deterministic in ~3 days of seed sweeping.
- **Root cause:** 1-deep CFC FIFO between core PMA and PCU dropped a wake-ack when an opcode arrived in the same cycle as the snoop drain. Pure pre-Si bug — the spec said the FIFO had backpressure; the RTL didn't.
- **Fix:** PCU FW added explicit re-arm-after-timeout; RTL added a 2-deep FIFO in the next stepping.
- **Lesson / coverage added:** new functional coverpoint = `{snoop_active, irq_pending, c6_exit_state}` cross. Caught two more siblings in regression that quarter. **"Would have been a P0 silicon escape — this is the bug that made me believe in pre-Si."**

### ⭐ Bug 2 — QNN driver context-cache invalidation (POST-SI, your current MS work)

- **Context:** Qualcomm Snapdragon X Elite, NPU EP via QNN; OEM camera SR pipeline regressed first-launch latency by ~200 ms after a driver bump.
- **Symptom:** every cold launch took the compile path instead of loading the cached binary; warm launches were fine.
- **Repro:** ORT profiler showed `compile` phase on every cold launch; minimal repro = single-op model + reboot loop.
- **Root cause:** driver's context-cache invalidation key over-invalidated on a benign metadata change → cache miss → full graph rebuild.
- **Fix:** filed with reproducer; vendor narrowed the invalidation key in next driver. Saved OEM ~200 ms first-launch on the camera SR scenario.
- **Lesson:** added cold-launch latency as a first-class regression metric in EdgeOptimizer (was previously only warm-launch). **"Post-Si bugs hide in the cold paths nobody runs in CI."**

### ⭐ Bug 3 — Silent EP fallback (SYSTEM-LEVEL, cross-stack)

- **Context:** Copilot+ workload, mixed-vendor (Intel/AMD/QC), one model started running on CPU instead of NPU after an ORT version bump.
- **Symptom:** functional pass, perf collapse — inference latency 8× higher, NPU utilisation 0.
- **Repro:** EdgeOptimizer's per-inference EP-assignment log showed every node assigned to CPU EP. Bisect on ORT version pinned it.
- **Root cause:** ORT graph partitioner now required a newer opset for an op the EP supported in the older opset → the EP refused the whole subgraph silently → CPU fallback.
- **Fix:** opset bump in model export pipeline; opened ORT issue to make silent-fallback noisy (warning by default).
- **Lesson / coverage added:** "silent-fallback detector" became a permanent EdgeOptimizer check; now part of every OEM pre-ship audit. **"Functional-pass perf-fail is the most expensive class of bug because nobody's alarms go off."**

### Bug 4 — NVMe CSTS.RDY hang (POST-SI bring-up, early-career, use sparingly)

- **Context:** L&T@SanDisk, NVMe SSD bring-up, JTAG + Metaware + LeCroy.
- **Symptom:** controller never asserted `CSTS.RDY=1` after `CC.EN=1` on cold boot in ~1/50 power cycles.
- **Repro:** Python power-scenario harness toggling rails at varied ramp rates; failure rate climbed at fast ramps.
- **Root cause:** PHY calibration FSM raced power-good signal at fast V-ramp; a state was sampled before stable.
- **Fix:** FW added a 2-ms guard delay before kicking the calibration FSM; later stepping fixed the FSM reset.
- **Lesson:** rail-ramp variance belongs in the standard bring-up matrix, not just a one-off characterisation. **"Bring-up bugs that only fire 1-in-N are almost always race conditions, not 'bad units'."**

### Bug 5 — CXL firmware ordering (PRE-SI, Star of the Month)

- **Context:** Cambria Xeon, CXL link bring-up on Zebu, joined the debug as the emulation owner.
- **Symptom:** intermittent CXL.cache transaction hang during link training under specific FW build.
- **Repro:** narrowed by bisecting FW images against last-known-good RTL; built a smaller traffic pattern that fired in <1 hr instead of overnight.
- **Root cause:** FW issued a config-write before a credit-return on the same VC; legal per spec but exposed an RTL ordering assumption.
- **Fix:** FW reordered; RTL added an SVA for the assumption so it can never silently re-emerge.
- **Lesson:** *"When the spec is ambiguous, write the assertion first — it pins the agreement."* Won Star of the Month.

### How to pick which story

| If they ask about… | Lead with |
|---|---|
| Pre-Si CPU / RTL / emulation | Bug 1 (Cambria C6) |
| Pre-Si subsystem / fabric / FW interaction | Bug 5 (CXL) |
| Post-Si platform / driver / FW | Bug 2 (QNN) |
| Post-Si bring-up / silicon characterisation | Bug 4 (NVMe CSTS.RDY) |
| System-level / cross-stack / perf regression | Bug 3 (silent EP fallback) |
| "Hardest bug you've owned" (open-ended) | Bug 1 if pre-Si audience, Bug 3 if system audience |
| "Most impactful bug" | Bug 2 (quantified OEM impact) or Bug 1 (would-have-been-respin) |

---

## SECTION 4B - 20-day system-thinking one-liner reference

> **The point of this section:** every interview "explain X" question maps to one of these tradeoffs. Lead with the *tradeoff*, then the mechanism. The interviewer is testing whether you reason like a systems engineer or recite facts.
>
> Each row: **the question** → **the one-liner** → **the day file with full reasoning**.

### Foundations (C, memory, hardware access)

| Concept | One-liner answer | Source |
|---|---|---|
| **Why C exposes addresses** | C is the only mainstream language that lets you see the machine; hardware is an address space and the language must let you say it. Pointer math is type-aware so array walks are correct by construction. `volatile` / `const` / `restrict` are not type decorations — they're contracts about the world *outside* the program, and the compiler optimises against those contracts. | [Day 01](21day_plan/day01_c_address_fluency.md) |
| **Why memory has a layout** | Lifetime, protection, and access pattern are three different questions; one region (stack/heap/BSS) cannot answer all three. Alignment matters because hardware loads in chunks — misaligned data costs atomicity, speed, and DMA correctness. Compiler padding is the visible cost of natural alignment. | [Day 02](21day_plan/day02_pointers_memory_layout_alignment.md) |
| **Why MMIO ≠ memory access** | Writing a hardware register is not writing memory. `volatile` stops the compiler caching loads in registers. Uncached/Device mappings stop the cache coalescing or reordering writes. Barriers stop the CPU's reorder buffer. Polling beats IRQs when events arrive faster than the IRQ entry/exit cost. Each keyword opts one optimisation layer back out — because hardware doesn't follow C's rules. | [Day 03](21day_plan/day03_bits_mmio_volatile_polling.md) |

### Cache, coherence, virtual memory

| Concept | One-liner answer | Source |
|---|---|---|
| **Why multi-level cache (not one big cache)** | You cannot build one cache that is simultaneously 1-cycle fast, 32 MB large, and shared across N cores — physics, area, and contention forbid it. The hierarchy is the resolution: each level trades one constraint for another. | [Day 04](21day_plan/day04_cache_lines_locality_amat.md) |
| **How CPU "decides" what to cache** | The CPU doesn't decide — it reacts. Demand fetch on miss, predict next line via prefetchers, evict via PLRU. The "decision" is locality: what you touched, you'll touch again; what's adjacent, you'll touch soon. | [Day 04](21day_plan/day04_cache_lines_locality_amat.md) |
| **Cache lookup walk** | TLB translates VA→PA in parallel with L1 indexing; PA splits into tag/set/offset; set read parallel-fetches all ways; tag compare picks the match (or miss); on hit mux out + update PLRU; on miss allocate victim, fill from L2 critical-word-first. Coherence wrinkle: write hit must be in M/E or send invalidate first. | [Day 04](21day_plan/day04_cache_lines_locality_amat.md) |
| **SRAM vs DRAM** | SRAM = 6T cross-coupled latch — fast, sparse, volatile only on power loss. DRAM = 1T1C capacitor — dense, cheap, leaks in milliseconds and needs refresh. SRAM where latency dominates, DRAM where capacity dominates. | [Day 04](21day_plan/day04_cache_lines_locality_amat.md) |
| **Why coherence (MESI)** | Coherence is the silicon tax for keeping the illusion of one memory across N private caches. Every line has a state, every state change is a protocol transaction, every transaction shows up as latency. False sharing, atomic contention, and lock costs are all the protocol charging you because data sharing was implicit in the layout. | [Day 05](21day_plan/day05_coherence_mesi_false_sharing.md) |
| **Why virtual memory** | An indirection that solves isolation, sharing, and oversubscription in one mechanism. Page fault is the kernel's signal to do allocation, swap-in, copy-on-write, or file-mapping lazily. Cost is a TLB lookup on every load + page walk on every TLB miss; we pay it because no simpler mechanism gives all three properties. | [Day 06](21day_plan/day06_virtual_memory_page_faults.md) |
| **Why TLB / huge pages / IOMMU** | TLB exists because translation has its own locality and a cold page walk would erase the VM benefit. Huge pages stretch TLB reach — tradeoff is fragmentation + coarser perms. IOMMU is the MMU re-applied to devices: per-device isolation, scatter-gather, 32-bit-device support. Same pattern at a different layer. | [Day 07](21day_plan/day07_tlb_huge_pages_iommu_smmu.md) |

### Device, interconnect, storage

| Concept | One-liner answer | Source |
|---|---|---|
| **CPU↔device DMA / barriers** | Two ordering universes meeting. The descriptor → barrier → doorbell pattern is the contract: descriptor visible to device, barrier ensures global ordering, doorbell triggers the device read. Cache management bridges the cache, which is transparent to CPUs but not to non-coherent DMA. Get any of these wrong and you get the classic "works at low rate, fails at line rate" bug. | [Day 08](21day_plan/day08_dma_doorbells_ordering_barriers.md) |
| **System boot flow (vendor-agnostic)** | Five phases everywhere: **POWER** (PMIC ramps rails, releases reset) → **ROM** (mask ROM verifies next stage from fuse-locked key) → **PLATFORM INIT** (DDR training in FSBL) → **BOOTLOADER** (loads OS kernel) → **OS+INIT** (kernel discovers HW, brings up cores, mounts rootfs). Chain of trust at every hop, fuses pin the root. Names change per vendor (UEFI/PEI/DXE on x86, BL1/BL2/BL31 on ARM TF-A, PBL/SBL/TZ on Snapdragon) — roles identical. | [Day 09](21day_plan/day09_arm_qualcomm_platform_vocabulary.md) |
| **PCIe** | A packet-switched, layered serial network that fits in a chassis. PHY/DLL/TLP layering enables generation-over-generation rate doubling without breaking drivers. LTSSM trains because Gbps serial links must train, not just turn on. Credit-based flow control wins on a lossless link by eliminating idle gaps. Power states (L0s/L1/L1.1/L1.2) mirror CPU C-states — deeper saves more, costs more wake. | [Day 10](21day_plan/day10_pcie_fundamentals_ltssm.md) |
| **NVMe** | AHCI's bottleneck list, inverted. Per-core SQ/CQ pairs kill the queue lock. MSI-X per CQ kills the cross-core IRQ wake. Polling with phase bits kills the IRQ entirely at high IOPS. The descriptor-doorbell pattern is unchanged from any modern DMA device. The hard part isn't the data path — it's the bring-up sequence (CSTS.RDY, AQA, DSTRD), where MMIO field widths and barriers cause "doesn't enumerate" bugs. | [Day 11](21day_plan/day11_nvme_queues_bringup.md) |

### Power, thermal, idle states

| Concept | One-liner answer | Source |
|---|---|---|
| **Why DVFS (V & F coupled)** | $P_{dyn} = \alpha C V^2 F$, and V & F are coupled by physics — lower F enables lower V, giving near-cubic energy savings. P-states are a curated list of safe (V, F) pairs the silicon was characterised at. Governors exist because hardware sees load but not intent. PVT sensors close the loop in real time because every die is different and worst-case static voltage wastes energy on the typical die. | [Day 12](21day_plan/day12_dvfs_pstates_pvt.md) |
| **Static vs dynamic power** | Dynamic = $\alpha C V^2 F$ — cost of toggling. Leakage = $V \cdot I_{leak}(T)$ — cost of being powered, exponential in temperature. Clock gating kills dynamic only. Power gating kills both, but you lose state and pay μs of wake-up. Every C-state and DVFS decision is choosing which lever to pull. | [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md) |
| **Why clock gating + ICG cells** | RTL `if(en)` patterns rewritten by synthesis into ICG cells — a latch+AND that gates the clock pin without glitching. Three tiers: leaf gates the tool inserts automatically, regional gates the architect hand-codes per unit, root gates the power FSM drives for whole IPs. Together they kill the 30-40% of dynamic power the clock tree would otherwise burn on idle flops. Validation: LEC, glitch checks, X-prop at reset, gating-efficiency coverage. | [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md) |
| **Why a C-state ladder** | Every idle state pays an entry/exit energy cost; only deeper states save more leakage. A single state is wrong for short idles (you waste more entering than you save) and wrong for long idles (you leak more than you need to). The OS picks the deepest state whose break-even residency fits the predicted idle window. Multiple C-states = multiple price points on the energy/latency curve. | [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md) |
| **PCU firmware / ADR / Core C6** | PCU is firmware (a microcontroller) because power policy is too complex, too SKU-specific, and too release-sensitive for RTL. ADR exists because persistent memory commits are in-flight in the WPQ on power loss — PCU has μs to drain it. Core C6 bugs are silent because the save/restore surface is wide and the bug only shows after a wake under specific timing. | [Day 14](21day_plan/day14_adr_pcu_core_c6_bugs.md) |
| **Thermal management = control loop** | Workload power isn't predictable and worst-case cooling is too expensive. Throttling is the negative feedback that breaks the leakage-temperature runaway. Perf counters are sampled because programmable beats fixed and statistical sampling is enough for most analysis. First diagnostic question for any "why is my CPU slow?" is: which throttle reason fired? | [Day 15](21day_plan/day15_thermal_power_perf_counters.md) |

### Pipeline, concurrency, accelerators

| Concept | One-liner answer | Source |
|---|---|---|
| **OoO + memory consistency + SIMD** | OoO and speculation extract ILP from a serial stream; SIMD extracts data parallelism orthogonally. Memory consistency models are a tradeoff between HW freedom and SW programmability — weaker (ARM) goes faster but needs explicit barriers; stronger (x86 TSO) is easier to program. Every concurrency bug is the CPU's reordering becoming visible across cores or to devices. Spectre-class side channels are the same optimisation leaking through a side channel — validation now tests both correctness and mitigation effectiveness. | [Day 16](21day_plan/day16_pipeline_memory_consistency_simd.md) |
| **Lock-free queues** | Queues are the universal primitive for decoupling producers and consumers across cores, IPs, and CPU↔device boundaries. Lock-free is required where locks can't reach (devices, real-time, hot paths). Pick the most-restrictive flavor that fits — SPSC is dramatically faster than MPMC. ABA is the value-matched-but-world-changed bug that makes naive CAS wrong; tagged pointers or hazard pointers fix it. Cache-line padding between head and tail is the difference between toy code and shipped code. | [Day 17](21day_plan/day17_concurrency_queues_dsa.md) |
| **Why NPUs exist** | Dedicated matmul silicon is 100–1000× more energy-efficient than CPU SIMD — the cost is a separate ISA, memory, address space, and power domain. The Execution Provider (EP) abstraction lets one model target many backends; each EP is a partitioner + a kernel library. The accelerator is a separate compute domain with its own everything; that isolation is its efficiency advantage and its debug pain. | [Day 18](21day_plan/day18_npu_ort_ep_hexagon_htp.md) |
| **Where NPU bugs live** | At the boundary — CPU↔NPU memory, VA↔IOVA, coherent↔non-coherent, runtime↔driver — because each domain is correct in isolation but the contract is fragile. First diagnostic: attribute slowdowns to a specific miss class (page fault, TLB, IOMMU, coherence), not to the model. FastRPC / EP runtimes are the marshalling layer where most leak/handle bugs hide. | [Day 19](21day_plan/day19_npu_memory_runtime_bugs.md) |

### Method (the meta-skill)

| Concept | One-liner answer | Source |
|---|---|---|
| **Debugging is method, not intuition** | Bisection + symptom-vs-cause + hypothesis-and-bound — because intuition has bias and method has guaranteed convergence. A polished bug story is method demonstrated through a concrete artifact: precise symptom, first hypothesis, bounding experiment, fix, and the test that prevents regression. The interviewer is hiring the method, not the trivia. | [Day 20](21day_plan/day20_debug_method_bug_polish.md) |

### How to use this section in an interview

When asked "explain X" or "why does Y exist":
1. **Lead with the tradeoff** (the *italic* part of the one-liner). 10 seconds.
2. **Name the mechanism** that resolves it. 20 seconds.
3. **Drop one validation hook** — what counter you'd watch, what bug class this enables, what test you'd add. 30 seconds.
4. **Stop.** Let them drill if they want depth.

Total: ~60-second answer that shows tradeoff thinking, mechanism literacy, and validation discipline. That's the senior bar.

---

## SECTION 5 - C-states (backup)

**Ladder:** C0 active → C1 clock-gate (~1us) → C1E +lowV → C3 flush L1/L2, PLL off (~50us) → **C6 arch-state to SRAM, core power-gated (~100us)** → C7-C10 LLC flush, package-level. Arm: WFI/WFE + PSCI `CPU_OFF`/`CPU_SUSPEND`, RPMh arbitrates.

**Entry (condensed):** scheduler emits MWAIT/WFI → drain pipeline → save arch state → flush private $ → notify PCU/RPMh → V-rail to retention → clock-gate → power-gate. PCU stays awake watching wake events.

**Exit:** wake event → ungate power → ungate clocks (PLL relock = biggest latency) → microcode restores arch state → resume after MWAIT.

**Break-even:** enter only if expected idle > 2× wake latency. cpuidle governor enforces.

---

## SECTION 6A - Pre-Silicon CPU Core Validation (depth reference)

> Deep reference for any pre-Si CPU/SoC tech drilldown. Read once before the call; don't try to memorise — internalise the *shapes*.

### What "pre-Si CPU core validation" actually means day-to-day

Pre-Si validation = prove the RTL is correct **before** tape-out. You live in three environments:

| Env | Speed | Use |
|---|---|---|
| **RTL simulation** (VCS / Xcelium / Riviera) | slow (~Hz–kHz) | unit tests, focused regressions, waveform debug |
| **Emulation** (Zebu / Palladium / Veloce) | fast (MHz) | full-chip / cluster, OS boot, stress, long random |
| **FPGA prototype** (HAPS / Protium) | faster (10–100 MHz) | software bring-up, firmware co-dev, perf approximation |
| Formal (JasperGold / VC Formal) | n/a | proof-based; protocol checks, deadlock, security |

A typical sprint: write/extend test content (C / SystemC / UVM), run regressions overnight on the farm, triage failures next morning, file bugs, code-review fixes, close coverage holes, attend arch / IP reviews.

### Core methodology terms (must know cold)

- **DV / pre-Si validation / verification:** synonyms in industry. Qualcomm uses "DV" (design verification).
- **Testbench (TB):** the harness around the DUT (design under test). Drives stimulus, checks responses, collects coverage.
- **UVM** (Universal Verification Methodology): SystemVerilog OOP framework. `uvm_env`, `uvm_agent` (driver + monitor + sequencer), `uvm_scoreboard`, `uvm_test`, `uvm_sequence`. Constrained-random stimulus + scoreboard-based checking.
- **Constrained-random vs directed:** directed = hand-written for one scenario; CR = random within `constraint` blocks, lets the seed explore corners. Modern flows: 80% CR + 20% directed corners.
- **Coverage**:
  - **Code coverage** — line, branch, toggle, FSM, expression. Tool-generated. Necessary not sufficient.
  - **Functional coverage** — `covergroup` with `coverpoints` and `crosses`, written by DV from the spec. The real metric.
  - **Assertion coverage** — were the assertions ever in their interesting state (not just passing trivially).
- **SVA** (SystemVerilog Assertions): `assert property (@(posedge clk) req |-> ##[1:5] ack);` Catches protocol violations as they happen, prints when they fire.
- **Scoreboard:** golden-model comparator. RTL output vs reference (ISA simulator, C model, prior-gen RTL) checked transaction-by-transaction.
- **Reference model / ISS:** Instruction Set Simulator (e.g. Arm FastModel, Spike for RISC-V). Runs the same program; DV diffs RTL retire stream against ISS retire stream.
- **Regression:** nightly / weekly run of the full test suite against latest RTL. Triage = bin failures (new bug / known bug / flake / infra) and assign.
- **Bring-up content:** smoke tests written before silicon — boot ROM, MMU enable, cache enable, multi-core, IRQ, full Linux/Android boot. Reused on FPGA, then on first silicon.

### CPU pipeline — the spine of any pre-Si CPU question

Classic in-order 5-stage (still useful for whiteboard): **IF → ID → EX → MEM → WB**. Modern OoO Arm/x86 core is much wider:

```
Fetch → Decode → Rename → Dispatch → Issue (scheduler/RS) → Execute (ALU/FPU/LSU/branch) → Writeback → Commit/Retire (ROB)
```

- **Hazards:**
  - **Structural** — two instructions need the same hardware (e.g. one write port). Solved by duplicating or stalling.
  - **Data** — RAW (true), WAR (anti), WAW (output). RAW solved by forwarding/bypass. WAR/WAW disappear with **register renaming** (physical reg file > arch reg file).
  - **Control** — branches. Solved by branch prediction + speculative execution + flush on mispredict.
- **Forwarding / bypass network:** EX→EX, MEM→EX paths to feed result before WB. Cost = wires + muxes; bug shape = forwarding from wrong stage.
- **Branch prediction:** modern = TAGE or perceptron; BTB for target; RAS for returns. Mispredict cost ≈ pipeline depth × IPC ≈ 15–20 cycles.
- **Out-of-order:**
  - **ROB** (reorder buffer) tracks in-program-order retirement → precise exceptions.
  - **Reservation stations / scheduler** holds ready-to-issue ops.
  - **Load/store queue (LSU)** orders memory ops, handles store-to-load forwarding.
  - **Rename table (RAT)** maps arch reg → physical reg; rolled back on misspec.
- **Common pre-Si bug shapes:**
  - Mispredict recovery doesn't fully drain the LSU → stale data forwarded.
  - Renamer free-list leak → physical reg starvation deadlock under stress seed.
  - WAW collision on a wide-issue cycle → wrong physical reg retired.
  - Forwarding from a flushed uop → silent data corruption (the worst kind).
  - Exception in the shadow of a mispredict → ROB unwind misorders.

### Memory subsystem (the other half of CPU validation)

- **Hierarchy:** L1I, L1D (private, 32–64 KB, 4–8 way, ~4-cycle), L2 (private or cluster-shared, 512 KB–1 MB, ~12-cycle), L3/SLC (shared, MB-scale, ~30-cycle), DRAM (~100 ns).
- **Coherence:**
  - **MESI** — Modified, Exclusive, Shared, Invalid.
  - **MOESI** (used in many Arm designs) — adds **Owned**: dirty *and* shared, owner forwards data without writing back yet.
  - Snoop filter / directory at the shared cache to avoid broadcast.
  - **CHI** (Coherent Hub Interface) is Arm's on-chip coherence protocol; AMBA family. CCN / CMN are the interconnect IPs.
- **Memory consistency models:**
  - **x86 TSO** — strong: only store → later-load can reorder.
  - **Arm v8/v9 weak** — any pair can reorder unless ordered by:
    - **DMB ISH** (data memory barrier, inner shareable),
    - **DSB ISH** (data sync barrier — also waits for prior ops to complete),
    - **ISB** (instruction sync barrier — context sync, e.g. after MMU/SCTLR change),
    - **LDAR / STLR** (acquire load / release store — one-sided ordering, the modern lockless primitive).
- **Store buffer:** holds retired stores until they drain to L1. Loads forward from store buffer if address matches. Bug shape: partial overlap (store 4B, load 8B from same addr) — must stall or merge.
- **MSHR** (Miss Status Holding Register): tracks outstanding misses; allows hit-under-miss / miss-under-miss. Bug shape: MSHR exhaustion under stress seed → deadlock.
- **Prefetcher:** stride / stream / next-line. Validation: confirm it doesn't pollute, doesn't violate coherence, throttles under bandwidth pressure.
- **Atomic / exclusives:** `LDXR / STXR` (load/store-exclusive) on Arm; Arm v8.1 added LSE atomics (`LDADD`, `CAS`, `SWP`). Validation: ensure correctness under contention from other cores.

### Common pre-Si CPU bug shapes (have one ready to discuss)

| Class | Example |
|---|---|
| Wake / power FSM | Core C6 wake handshake drops on overlapping snoop+IRQ (your Cambria story) |
| Renamer / free-list | Physical reg leak under sustained mis-spec |
| Coherence | Spurious upgrade M→I→M ping-pong from a fence + atomic combo |
| LSU ordering | Store-to-load forwarding from a store that later squashes |
| TLB / MMU | TLB miss + speculative load + ASID switch races |
| Power | Clock-gate enable too eager — flop holds wrong value next cycle |
| Reset / init | X-propagation through a non-reset flop on first cycle |
| Clock domain crossing | Async FIFO metastability not protected by sync flops on one of N paths |


### Sporadic emulation failure — the canonical debug answer

> "First, make it deterministic. Lock the seed, save replay, capture a waveform window. Second, minimise — strip the testbench down until removing the next thing makes the failure go away; that's your minimum repro. Third, bisect: revisions of RTL, of firmware, of testbench. Fourth, hypothesise from the waveform — is the assertion firing on the right signal at the right time, or is it the symptom and the root cause is upstream? Instrument with extra `$display` or covergroup, re-run, confirm. Only then propose an RTL fix and run it through regression. Golden rule: don't touch the design until the failure is deterministic and minimal."

### Tools you should name-drop comfortably

- Sim: VCS, Xcelium, Riviera-PRO. Coverage: Verdi (FSDB), URG.
- Emulation: Zebu (you owned this on Cambria), Palladium, Veloce. Run-time tools: zRun, zDebug.
- FPGA prototype: HAPS, Protium.
- Formal: JasperGold (Cadence), VC Formal (Synopsys).
- Waveform / debug: Verdi, Simvision, DVE.
- Test content: SystemVerilog/UVM, C/C++ test programs (bare-metal), Perspec (PSS-based portable stimulus), random instruction generators (Genesys, Force-RISCV-like).
- Reference: Arm FastModel (DSM), ISS for the architecture; for x86, the internal Intel IXP / SoftSDV-style.
- Regression infra: LSF / SLURM farms, internal triage portals.

### "Walk me through bring-up of a new CPU core in emulation" — canonical answer

> "Day 1 is reset/clock/power-on smoke — does it come out of reset cleanly with no X's. Day 2 is the trivial loop — `add r0, r0, #1; b .` — does the PC advance, do retirement counters tick. Then exception path — undefined instruction → handler. Then MMU on with identity map — does a translated load work. Then cache on — does coherence behave between two consecutive loads to the same line. Then second core — does the SMP boot path work, IPIs, spinlocks. Then SMMU / IO coherence. Then full Linux or Android boot to userspace. Each milestone has its own coverage and its own regression slot. The whole point is: each step builds on a working previous step, so when something breaks at step N you only have to debug step N's delta."

---

## SECTION 6 - NPU Post-Silicon Validation (your current MS work — only if pivoted)

**Stack:** App → ORT/DirectML → Execution Provider (QNN/OpenVINO/XDNA) → vendor driver+FW → NPU silicon (Hexagon HMX+HVX) → shared DDR / VTCM SRAM.

**What I actually do (generic, role-agnostic version):**
- Own the regression + bring-up matrix for new NPU silicon arrivals across three vendors. Each vendor = different EP, different FW cadence, different debug surface.
- Per-inference attribution: tie energy / utilisation / latency back to the specific node, EP, and HW IP that ran it. This is what makes silent fallbacks visible.
- Cross-stack triage: a regression could be model export, ORT, EP, driver, FW, or silicon stepping. My job is to bisect to the layer and hand off with a minimal repro the silicon team can run without my platform.
- Tooling: EdgeOptimizer (ETW + WPA + RAPL pipeline, per-inference attribution, silent-fallback detection). Reused by the team and OEMs.
- OEM-facing: when an OEM ships a Copilot+ device, I'm the escalation point for NPU-related issues found in their qualification.

**Silent EP fallback (THE post-Si NPU bug):** one unsupported op → whole graph silently drops to CPU EP → functional pass, perf collapses. Detect via per-node EP_assignment log. *Full story = Bug 3 in Section 4A.*

**QNN page-fault / context-cache bug:** *Full story = Bug 2 in Section 4A.* One-liner: driver context-cache over-invalidation → every cold launch recompiled → 200 ms first-launch hit on OEM camera SR → vendor narrowed the invalidation key.

**Honest framing:** "Platform-side NPU validation — graph partitioning, EP fallback, energy/inference, version-matrix regression. Not Hexagon RTL. Value-add is cross-stack debug app→driver and tooling that scales across vendors."

**How to pivot this work to a pre-Si CPU audience (today):** "The reason this work matters for a pre-Si role is that I see exactly which classes of bug escape pre-Si and surface at the platform. Silent fallbacks, cold-path bugs, version-matrix bugs — these are coverage holes upstream. I want to bring that 'what escapes look like' lens back into a pre-Si team."

---

## SECTION 7 - Puzzles (one-line reflexes only)

- **2-egg / N-floor:** X(X+1)/2 ≥ N → first drop = 14 for N=100, worst case 14 drops.
- **Sum 5 odd numbers from {1,3,...,15} = 30:** impossible by parity (5 odd → sum is odd).
- **x^y vs y^x:** take ln → compare ln(x)/x vs ln(y)/y; peaks at e ≈ 2.718. Both ≥ 3 → smaller base wins. Famous tie: 2^4 = 4^2 = 16.
- **Burning rope:** light both ends of one + one end of other → 45 min from two 1-hr ropes.

---

## SECTION 9 - Quick technical reflexes

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

> **Generic for any silicon-validation round.** Star (⭐) the items that match your role lens; skip the rest.

### Universal (every round, every lens)

- [ ] ⭐ Career narrative (60 sec) — lead with the lens that matches the role
- [ ] ⭐ Current-work framing (Section 1, pick #1 / #2 / #3 to match the audience)
- [ ] ⭐ "Why this role" answer ready — frame *toward* the new thing, not *away from* the old
- [ ] ⭐ One bug story rehearsed (Section 4A; pick by lens — table at end of 4A)
- [ ] ⭐ One backup bug ready (different lens, in case they pivot)
- [ ] ⭐ Adaptive-answer cue: clarify pre-Si vs post-Si vs system first if the question is ambiguous (Section 2A)
- [ ] ⭐ Universal frameworks rehearsed: new-problem (7 steps), coverage plan (10 steps), workload distribution (10 steps) — Section 2A
- [ ] ⭐ Sporadic-failure debug recipe (deterministic → minimal → bisect)
- [ ] ⭐ Functional vs code coverage one-liner
- [ ] ⭐ 3 questions for them written on a sticky (Section 0A step 8)

### If the role is **pre-Si CPU / SoC**

- [ ] ⭐ Pipeline hazards + OoO bug shapes (Section 6A table)
- [ ] ⭐ Memory consistency (Day 16 one-liner) + ARM↔x86 mapping (if ARM shop)
- [ ] ⭐ Coherence (Day 5 one-liner) — MESI walk + false sharing
- [ ] ⭐ "Bring up a new CPU core in emulation" canonical walkthrough (Section 6A)
- [ ] ⭐ Failing-assertion triage (test bug / RTL bug / spec bug)
- [ ] ⭐ Tools name-drop comfort: VCS/Xcelium, Zebu/Palladium, Verdi, JasperGold, UVM, SVA, Perspec

### If the role is **post-Si bring-up / platform**

- [ ] ⭐ Bring-up canonical sequence (power → IO → boot → functional → perf → stress → corners)
- [ ] ⭐ Throttle reason debug (which limit fired — PROCHOT, Tjmax, PL1/PL2)
- [ ] ⭐ Cross-stack triage: app → ORT → EP → driver → FW → silicon, with bisection at each layer
- [ ] ⭐ Reproducer discipline ("must run on golden HW without my platform")
- [ ] ⭐ Tools name-drop comfort: JTAG, Lecroy, scope, RAPL, ETW/WPA, perf, ftrace
- [ ] ⭐ Cold-path bugs (Day 19/20: bugs that hide in paths CI doesn't cover)

### If the role is **NPU / AI accelerator**

- [ ] ⭐ NPU stack walk (Day 18 one-liner): app → ORT → EP → vendor runtime → silicon
- [ ] ⭐ Silent EP fallback (the canonical NPU bug — Bug 3)
- [ ] ⭐ Where NPU bugs live (Day 19 one-liner): boundary attribution
- [ ] ⭐ Energy-per-inference vs raw TOPS framing
- [ ] ⭐ Per-inference attribution methodology

### If the role is **system / SoC integration**

- [ ] ⭐ Layer-boundary bug framing ("each IP passes its tests, system still wrong")
- [ ] ⭐ DMA / barrier / doorbell pattern (Day 8 one-liner)
- [ ] ⭐ PCIe + NVMe stack story (Days 10-11)
- [ ] ⭐ Counter-driven debug — instrument every IP on the path, hand off to whoever's diverged

### Backup tech (asked occasionally)

- [ ] Thermal-headroom answer (7 levers) — Section 3
- [ ] DVFS in 60 sec ($V^2F$, P-states, governor) — Section 4 + Day 12 one-liner
- [ ] C-state entry+exit flows — Section 5 + Day 13 one-liner
- [ ] Boot flow (5 generic phases, vendor mapping) — Day 9 one-liner
- [ ] SRAM vs DRAM (6T vs 1T1C, refresh) — Day 4 one-liner
- [ ] Static vs dynamic power (CV²F vs leakage) — Day 13 one-liner
- [ ] Clock gating (RTL→synth→ICG→tiers) — Day 13 one-liner

### Behavioural backups

- [ ] Underperforming-teammate framework (can't vs won't)
- [ ] Disagree-with-designer (data-led, disagree-and-commit)
- [ ] "Time you missed something" (lesson > miss)
- [ ] "What does a good day look like" (one-line ready)

### Final breath

- [ ] Honest framing: lens-relevant depth + adjacent breadth + low-ego learner
- [ ] Breathe. Calm. Specific. Concrete. Lead with tradeoff, then mechanism, then validation hook.
- [ ] Let them drill if they want depth.

---

> **Senior-bar reminder:** at this level they're checking judgment more than knowledge. Tech question → **structured answer + tradeoff + what you'd actually do.** Behavioural → **framework + real example + lesson.** Puzzle → **think aloud.** Always lead with the *tradeoff* — that's what separates a senior validator from someone who memorised the spec.



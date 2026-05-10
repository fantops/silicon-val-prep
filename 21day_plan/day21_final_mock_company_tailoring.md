# Day 21 — Final Mock + Per-Company Tailoring

## Concept

Final day. Three blocks: a **mock loop** (behavioral + technical + coding) timed end-to-end, a **flashcard sweep** to make sure the cheap facts are instant, and **per-company tailoring** so you walk into Qualcomm / Intel / Microsoft with the right vocabulary on top. The narrative is the **same** core (Microsoft post-Si platform validation → Intel pre-Si emulation on Cambria → L&T post-Si NVMe bring-up); only the **emphasis** changes per company.

## Why this day in the flow

Days 1-20 built the technical material; today **rehearses the loop**. Confidence comes from preparation, not from cramming new topics in the final 24 hours.

In the Layer map this is the **rehearsal layer**.

---

## Deep Study Notes

### 1. The mock-loop block

Time **60 minutes total**. No notes during answers.

| Round | Time | Format |
|---|---|---|
| Behavioural + career | 15 min | 4 prompts, 90-sec answers each |
| Technical concepts | 20 min | 8 prompts, 60-90 sec each |
| Coding | 25 min | 2 problems back-to-back, talk while you type |

#### Behavioural prompts

1. **"Tell me about yourself."** — 60-90 sec. Current role → past experience → why this role. Microsoft (multi-SoC post-Si NPU validation) → Intel (Cambria PCIe Gen6 + ADR pre-Si emul) → L&T/SanDisk (NVMe post-Si bring-up). Close: "I want to own silicon end-to-end at the vendor side."
2. **"Tell me about a challenging bug."** — Pick from Day 20 stories 1-4b. 90 sec. Lead with symptom.
3. **"Why this role / company?"** — research-driven. Map your experience to their product (see section 3 below).
4. **"Tell me about a cross-team / disagreement."** — Situation → evidence → escalation (1:1, then data review) → outcome → process change.

#### Technical prompts (60-90 sec each)

| # | Prompt | Day anchor |
|---|---|---|
| T1 | PCIe protocol stack (3 layers, posted/non-posted, TLP types) | Day 10 |
| T2 | NVMe command flow (SQE → doorbell → DMA → CQE → MSI-X) | Day 11 |
| T3 | MESI walk-through (E → S → M → I) | Day 5 |
| T4 | DVFS + why it matters for validation | Day 12 |
| T5 | Bugs only post-Si catches | Day 20 |
| T6 | New SoC bring-up sequence | Day 11 / 14 |
| T7 | MSI vs MSI-X | Day 8 |
| T8 | TSO vs weak memory model + a concrete bug | Day 16 |

#### Coding prompts (12-13 min each)

- **Problem 1: PCIe ECAM config-read simulator.** `ecam_address(base, bus, dev, func, reg)` + simulated config space + `enumerate_bus(bus)` + BAR decode. (Day 10.)
- **Problem 2: NVMe admin-queue init.** `nvme_init()` reading CAP, disabling, programming AQA/ASQ/ACQ, enabling, polling CSTS.RDY, computing doorbell offsets. (Day 11.)

Talk while typing: complexity stated unprompted, edge case named, embedded gotcha called out.

### 2. Flashcard sweep (cheap facts you should answer in <5 seconds)

Architecture & power:

| Front | Back |
|---|---|
| Dynamic power equation | $P = C V^2 f$ |
| Worst-case timing corner | SS, low V, high T |
| Worst-case leakage corner | FF, high V, high T |
| Tjmax formula | $T_j = T_a + P \cdot \theta_{JA}$ |
| PROCHOT# | thermal throttle signal asserted near Tjmax |
| P-states | performance: P0 max → Pn min |
| C-states | idle: C0 active → C6 deepest, context saved |

Cache & memory:

| Front | Back |
|---|---|
| Cache line size (typical) | 64 B |
| MESI states | Modified, Exclusive, Shared, Invalid |
| False sharing | two cores write different fields in same line → coherence ping-pong |
| TLB miss cost | 4-level walk ≈ 200 cycles |
| Huge pages | 2 MB or 1 GB; reduce TLB misses |
| IOMMU/SMMU | translate device DMA addresses; isolate VMs |

PCIe & NVMe:

| Front | Back |
|---|---|
| PCIe Gen4 lane rate | 16 GT/s |
| ECAM formula | `base + (bus<<20) | (dev<<15) | (func<<12) | reg` |
| Posted write | MWr — fire and forget |
| Non-posted | MRd, CfgRd/Wr — wait for CplD |
| LTSSM order | Detect → Polling → Configuration → L0 |
| NVMe SQE / CQE size | 64 B / 16 B |
| Doorbell stride | `4 << CAP.DSTRD` |
| SQ tail doorbell offset | `0x1000 + (2*QID) * stride` |
| Phase bit | distinguishes new completions without clearing memory |
| Bring-up | read CAP → disable → set AQ → enable → identify → create I/O queues |

Hardware C:

| Front | Back |
|---|---|
| Why `volatile`? | makes the access compiler-observable so it is not cached/elided; *not* a CPU/device-ordering barrier |
| RMW pattern | read, clear bits (`& ~mask`), set bits (`| val`), write back |
| LE layout of `0xDEADBEEF` | `EF BE AD DE` (LSB low addr) |
| Why not struct overlay for MMIO? | padding is implementation-defined; struct copy is `memcpy`; no order guarantee |

Validation:

| Front | Back |
|---|---|
| Sim speed | Hz-kHz |
| Emul speed | 1-10 MHz |
| FPGA proto | 10-100 MHz |
| Post-Si | GHz |
| What emulation misses | analog, process corners, real timing |
| Transactor | HW+SW bridge between host interface and emulated RTL |
| Debug framework | observe → reproduce → isolate → hypothesise → test → root-cause → fix |

NPU:

| Front | Back |
|---|---|
| 6-layer NPU stack | framework → ONNX → ORT → EP → vendor runtime → silicon |
| EP fallback | one unsupported op → CPU EP → 10× perf cliff |
| QDQ fusion | `Quantize → Op → Dequantize` collapsed to single INT8 op |
| VTCM / local SRAM | software-managed local store, not in CPU coherence; quote exact size only from internal platform docs |
| HTP execute pipeline | DMA + matrix/vector engines pipelined; Snapdragon X Elite-class NPU has publicly advertised peak TOPS |
| Context cache | serialised compiled artifact; second launch skips compile |
| Page-fault story takeaway | correlate with VirtualAlloc + peak VM + version matrix |

### 3. Per-company tailoring

Same core narrative; emphasis adjusted to the team's product.

#### Qualcomm (Snapdragon, Hexagon NPU, Auto, IoT)

**Lead with:** post-Si platform validation across **three NPU vendors** (Intel/AMD/**Qualcomm** Hexagon HTP v79); strong on **ARM/Snapdragon vocabulary** (EL0-3, GIC, SMMU, RPMh, PSCI, LPDDR training, big.LITTLE / DynamIQ); **NVMe post-Si bring-up** background.

**Anchor sentences:**

- "I do post-Si platform validation on **Hexagon HTP v79 / Snapdragon X Elite**, ONNX Runtime QNN EP path, including software-managed VTCM/local-SRAM tiling behaviour."
- "I'm comfortable with the ARM platform: EL0-3, GIC interrupt routing, SMMU stream IDs, RPMh / PMIC, and PSCI for power state coordination."
- "Hexagon's compute/vector/DMA pipeline is how Snapdragon X Elite-class NPUs reach their advertised peak TOPS; one stalled engine and the pipeline degrades toward serial execution."

**Bug stories to lead with:** Screen Region Detector / HMX tile (4d — vendor-FW escalation, your strongest current-job HW-pointed story) → QNN page-fault (4) → Paint SR vs Camera (4b) → NVMe CSTS.RDY (2). Save ADR (1) and Core C6 (3) for if Intel-side comes up.

**Day-anchors to revise night-before:** Day 9 (ARM/Snapdragon vocab), Day 11 (NVMe), Day 16 (memory model), Day 18 (NPU stack), Day 19 (NPU bugs), Day 20 (debug + stories).

#### Intel (Xeon, Client, NPU, FPGA)

**Lead with:** **Cambria pre-Si emulation on Zebu** (Intel role); **PCIe Gen6 transactor in C++** (10+ enhancements); **ADR / Core C6** bug stories from your Cambria PCU/PMA work; **CXL** familiarity (3 sub-protocols).

**Anchor sentences:**

- "At Intel I owned pre-Si validation of power-management flows on Cambria — C-states, ADR, PCIe Gen6 — on Zebu emulation, with Perspec test content and PCU opcode-level tracing."
- "I built 10+ PCIe Gen6 transactor enhancements in C++ and developed Perspec scenarios for cross-IP validation."
- "Star-of-the-month for a CXL FW issue — CXL.io / CXL.cache / CXL.mem are the three sub-protocols on the PCIe PHY."

**Bug stories to lead with:** ADR opcode-ordering (1) → Core C6 ARAT wake-loss (3) → NVMe CSTS.RDY (2). QNN story (4) is your bridge into modern AI work.

**Day-anchors:** Day 10-11 (PCIe + NVMe), Day 12-14 (DVFS, C-states, ADR), Day 15 (RAPL/perf-counters), Day 16 (TSO + pipeline), Day 20 (stories).

#### Intel Ireland — NPU Validation (Leixlip / Shannon)

This is a **specialisation** of the generic Intel framing above for the Ireland AI silicon (NPU) validation team. Same Cambria/PCIe Gen6/ADR foundation, but lead with the AI-silicon-validation framing the JD asks for.

**Lead with:** **NPU validation across pre-Si and post-Si** is exactly what I do today. Microsoft = post-Si NPU on three vendors (incl. Intel LNL NPU). Intel Cambria = pre-Si validation infra (Zebu, transactor C++, Perspec). The JD asks for both halves; I have both halves.

**JD ↔ my evidence (memorise the mapping):**

| JD bullet | My evidence |
|---|---|
| Validate next-gen AI silicon (NPU) across workloads | EdgeOptimizer across LNL / XDNA / Hexagon HTP v79; ResNet/MobileNet/Llama-class quant models; Photoshop / Paint SR / Camera SR field workloads |
| Debug pre- and post-Si — intermittent, multi-domain | Cambria Core C6 wake-loss (1-deep CFC FIFO race, pre-Si Zebu); QNN page-fault regression (post-Si, multi-driver bisect) |
| Develop validation infra (sim / emul / FPGA / post-Si) | Zebu emulation flows, Perspec test content, PCIe Gen6 transactor in C++ (10+ enhancements); EdgeOptimizer post-Si profiler |
| Characterise & profile silicon power/perf on AI workloads | RAPL + ETW + WPA, energy-per-inference, EDP, geomean across model suite; per-IP RAPL domain attribution |
| Power/perf tuning with arch / design / SW | EP fallback root-causing with ORT + driver teams; PL1/PL2 / Tau tuning, EPP exploration |
| Read HW arch specs → derive targeted tests | PCIe Gen6 spec → FLIT / L0p test scenarios; CXL.io/.cache/.mem coverage; NVMe spec → CSTS.RDY / AQA bring-up |
| Share findings, debug techniques | Internal Microsoft EdgeOptimizer docs / brown-bags; cross-vendor methodology write-ups |
| C/C++ + memory models + concurrency | Days 1-3, 5, 16, 17 — SPSC ring, MESI, TSO vs weak, atomics |
| Python for automation / data analysis | L&T NVMe Python power-scenario harness; EdgeOptimizer ETW post-processing in Python |
| OS-level driver exposure (Linux/Windows) | Windows: ETW / DxgKrnl / WPA / WPR; KMDF basics; Linux: cpuidle / cpufreq governors, perf, ftrace |
| Embedded toolchains (compiler, debugger, build, VCS) | gcc/clang, gdb, JTAG/Metaware (L&T), Make/CMake, git, Perforce (Intel) |
| FPGA / emulation / RTL sim | Zebu emulation owner (Cambria); Perspec, PCIe Gen6 transactor; pre-Si signal-level debug |
| Bash / PowerShell / Tcl | Bash + PowerShell daily; Tcl from EDA flow exposure |
| Perf profiling / benchmarking / power measurement | EdgeOptimizer (RAPL + ETW + per-inference attribution); Quarch external power as ground truth |
| Silicon validation / bring-up / FW | L&T NVMe post-Si bring-up; Intel pre-Si Cambria PCU/PMA flows |

**Anchor sentences:**

- "Intel Ireland is the team I'd join because the charter is **NPU validation across pre-Si and post-Si**, which is exactly the seam I sit on today: post-Si NPU at Microsoft on LNL/XDNA/Hexagon, and pre-Si Xeon at Intel India on Cambria. Closing that loop on one product is what I want next."
- "On the AI workload side I run the **same six-stage methodology** I built at Microsoft — bring-up sanity, functional matrix, perf characterisation, energy-per-inference, sustained-thermal, and stress — and I've already applied it to Intel LNL NPU on Copilot+ PCs, so I know the silicon's behaviour from the customer side."
- "On the pre-Si side I've owned C-state / ADR / PCIe Gen6 flows on **Zebu**, written **C++ transactor** enhancements, and developed **Perspec** test content for cross-IP validation — so I can build infra, not just consume it."
- "Spec → test case is how I work: I read the PCIe Gen6 FLIT mode and L0p sections, derived targeted scenarios for the transactor, and the same approach applies to NPU IP specs — read the architectural intent, then derive workloads that exercise the corner."

**Bug stories to lead with:** ADR opcode-ordering (1) → Core C6 ARAT wake-loss (3) → Screen Region Detector / HMX tile (4d, current-job vendor-FW escalation) → QNN page-fault regression (4) → silent EP fallback (5). The first two prove pre-Si depth; 4d + 4 prove post-Si NPU depth on real silicon. Save NVMe CSTS.RDY (2) for "any other bring-up experience?" follow-ups.

**Why Ireland specifically (sentence 4 of the "why this company" template):**

- "Intel Ireland has an active NPU validation charter in Leixlip, and the role sits close enough to design and silicon validation that findings can feed back into future steppings — that close-the-loop is the missing piece in my current OEM-side post-Si role."
- "Ireland is also a deliberate personal step — English-speaking, EU base, strong engineering culture, and Intel's biggest non-US site. I'm relocating with intent, not just for a role."

**Day-anchors to revise night-before (Intel Ireland order):**

1. Day 21 Section 3 (this section).
2. Day 18-19 (NPU stack + bugs) — the AI-silicon angle the JD leads with.
3. Day 14 (ADR + Core C6) — pre-Si Intel credibility.
4. Day 12-13 (DVFS + C-states) — the power/perf charter.
5. Day 15 (RAPL + multi-IP + EDP) — characterisation methodology.
6. Day 10 (PCIe Gen6 + LTSSM) — your transactor work.
7. Day 16-17 (memory model + concurrency) — C/C++ coding round.
8. Day 20 (debug method + 4 polished stories).

**Likely Intel-Ireland-specific drill questions:**

- *"How would you build pre-Si validation infra for an NPU IP that doesn't have post-Si silicon yet?"* → simulator (Hz–kHz) for unit-level functional, emulation (Zebu, MHz) for graph-level + driver bring-up, FPGA proto (10–100 MHz) for end-to-end perf with real ORT, post-Si for PVT corners. Build one harness that drives all four with the same test content (Perspec-style).
- *"How do you test an NPU spec corner you can't easily hit in software?"* → constrained-random workloads in the transactor; synthetic ops crafted to land on the corner (e.g., VTCM eviction, DMA-of-1-byte, op at tile boundary); inject from the EP layer down.
- *"You see a 10 % regression on geomean across 30 models on a new stepping. How do you proceed?"* → bisect by stepping/driver/runtime/model; rank by geomean contribution (one model can dominate); per-op profile the top contributor; correlate with ETW + RAPL; isolate to silicon vs FW vs SW; reproduce smallest case; file with hypothesis.
- *"How do you derive test cases from a HW arch spec?"* → read the spec section; list architectural intents (functional, perf, error, security); for each intent list observable signals; design one positive test, one negative test, one corner test; map each test to pre-Si (sim/emul/FPGA) or post-Si; document expected vs observed; review with the IP arch.

#### Microsoft (Copilot+ PC, Azure, Surface)

**Lead with:** the **current job** — multi-vendor post-Si platform validation, EdgeOptimizer profiler, ETL/WPA, agentic Copilot CLI workflows; the **breadth across Intel + AMD + Qualcomm** is the differentiator.

**Anchor sentences:**

- "I run post-Si platform validation across **three vendors** — Intel LNL NPU, AMD XDNA, Qualcomm Hexagon HTP v79 — on Copilot+ PC silicon, with the same workloads and a common measurement methodology."
- "I built **EdgeOptimizer**, a platform-level profiler that surfaces EP-fallback, NPU vs CPU utilisation, and energy-per-inference regressions before launch."
- "My agentic Copilot CLI workflow autoloads ETL traces, correlates CPU scheduler events with NPU EP events, and turns a 2-hour WPA triage into a 15-min report."

**Bug stories to lead with:** Screen Region Detector / HMX tile (4d, vendor-FW escalation) → QNN page-fault (4) → Paint SR vs Camera (4b) → silent EP fallback (5). The Intel/L&T stories are your foundation, not the headline.

**Day-anchors:** Day 15 (perf counters / RAPL / multi-IP), Day 18-19 (NPU stack + bugs), Day 20 (stories + honest framing).

### 4. The 4-sentence "why this company" template

1. What I do now (Microsoft post-Si platform validation, multi-vendor NPU).
2. What I want (closer to silicon, own the validation end-to-end).
3. Why my background fits (pre-Si Intel + post-Si L&T + platform Microsoft).
4. Why this company specifically (their product, your two-line research).

Customise sentence 4 per company; sentences 1-3 stay the same.

### 5. Day-of-interview quick-reference (40 min night-before reread)

In order:

1. **Day 21 section 3** — per-company tailoring + anchor sentences.
2. **Day 20** — 4-5 polished bug stories, time each to 90 sec.
3. **Day 16** — pipeline + memory model.
4. **Day 9 section on RPMh / EL0-3** — if Qualcomm.
5. **Day 11 section on boot flow** — if Qualcomm or NVMe role.
6. **Day 17 section on spinlock + SPSC ring** — coding-round template.

### 6. The night before

- No new topics. Light review only.
- Re-record the "tell me about yourself" answer once. Listen back.
- Pick the 3 bug stories you'd lead with for the company you're interviewing.
- Sleep. Confidence comes from preparation, not from cramming.

---

## Bugs in this Topic

This day is rehearsal of Day 20 stories — no new bugs. The reps are the work.

---

## Interview Explanation (60-90 sec — final "tell me about yourself")

> "I'm a post-silicon platform validation engineer at Microsoft, working on Copilot+ PC silicon across Intel, AMD and Qualcomm — three NPU architectures, one common methodology. My role is to answer one question per workload: when a Windows AI app runs, does the NPU actually do the work, and at what energy and latency? I built EdgeOptimizer, a platform-level profiler that surfaces EP fallback, scheduler stalls and energy-per-inference regressions, and I drive ETL/WPA-based triage with agentic Copilot CLI workflows on top. Before Microsoft I was at Intel, owning pre-silicon validation of power-management flows — C-states, ADR — and PCIe Gen6 on Cambria Xeon, on Zebu emulation, with C++ transactor work and Perspec test content. Before that I did post-silicon NVMe bring-up at L&T for SanDisk — JTAG, Lecroy, oscilloscope, Python automation. I want to move closer to the silicon — own validation end-to-end at the vendor side, the way I did at L&T on NVMe and supported pre-Si at Intel on Cambria. [Company] is where I close the loop and own vendor-side silicon end-to-end."

---

## Architect Drills

*Interview delivery is itself a constraint problem: a 60-90 second budget, an interviewer with priors, and finite working memory under stress. Reason about why each piece of the playbook lands where it does. No answer key.*

**D1. The 60-second "tell me about yourself" goes current → past → why this role. Reason about why this order beats chronological.**
*Constraints to weigh:* recency bias of interviewers, signal density of current role, reverse-chronological as default. What does the candidate signal by leading with present-day work — and what does chronological accidentally signal?

**D2. Tailoring per company (Qualcomm/Intel/Microsoft). Reason about which dimension of your background to lead with.**
*Constraints to weigh:* what the team actually builds, what bugs they value, vocabulary alignment (Hexagon vs Cambria vs Copilot+), bridge-stories that work for all three. Construct the lead sentence for each — and identify which sentence works for none.

**D3. The "why this company" 4-sentence template. Reason about why generic answers fail.**
*Constraints to weigh:* interviewer can detect a copy-paste, specificity buys credibility, research effort signals interest. What's the minimum-research move that delivers maximum specificity — and what's the failure mode of over-research?

**D4. Three bug stories per company. Reason about how to choose which three.**
*Constraints to weigh:* relevance to their stack, depth of mechanism, breadth of ownership, honest-framing constraints. What's the bug story that you should *not* lead with at any company, and why — even if it's your best story?

**D5. The night-before reread order. Reason from first principles about what you genuinely retain under interview stress.**
*Constraints to weigh:* recall vs recognition, working-memory limits, anchor-sentence priming, what sleep does to consolidation. What's the smallest possible reread set that survives a 9 AM interview — and what should you *not* read the night before?

**D6. "Tell me about a time you disagreed with a designer." Reason about what answer pattern wins.**
*Constraints to weigh:* situation specificity, evidence over opinion, escalation discipline, outcome honesty (both sides may be right), process-change afterwards. What's the failure mode of a "I was right, they were wrong" answer — even when you were?

**D7. Imagine you have one interview slot left and must pick one bug story to lead with. Reason about which story you choose, and why.**
*Constraints to weigh:* mechanism depth, your ownership clarity, interviewer follow-up surface area, recency, alignment with role. Defend the choice — or admit it varies by company and explain how you'd decide in the moment.

---

## Coding A — Systems C: timed mock — pick two from the list, 13 min each

The point today is **delivery under pressure**, not new code. Pick two:

1. **CSR init + poll** — `reg_field_write`, `wait_for_bit_set` with timeout, `volatile` MMIO accessors. (Day 1, Day 3.)
2. **Endian helpers** — `swap32`, `read_le32`, `read_be32`. (Day 3.)
3. **Ring buffer** — push/pop/full/empty with power-of-two mask. (Day 8 / Day 11 / Day 17.)
4. **NVMe admin-queue init** — full sequence per Day 11.
5. **Spinlock + counter** — C11 atomics, two pthreads, verify final value. (Day 17.)
6. **SPSC ring** — full implementation with `alignas(64)` padding. (Day 17.)

Talk through complexity, edge cases, and one embedded gotcha for each.

## Coding B — Interview DS&A: timed mock — pick two

1. **Two Sum** (LC 1) — hashmap one-pass.
2. **Reverse linked list** (LC 206) — iterative 3-pointer.
3. **Valid parentheses** (LC 20) — stack.
4. **Number of Islands** (LC 200) — DFS / BFS.
5. **LRU cache** (LC 146) — hashmap + doubly-linked list.
6. **Top-K frequent elements** (LC 347) — bucket sort.

Pick the two patterns least practiced in the last 7 days. State complexity unprompted.

---

## Final Self-Assessment

Score yourself 1-5 on each. Anything ≤ 3 → re-read the day file before the loop.

| Area | Day anchors | Score |
|---|---|---|
| Career narrative (30/60/90 sec) | Day 21 section 1 |  |
| C / pointers / bits / MMIO | Day 1-3 |  |
| Cache + MESI + false sharing | Day 4-5 |  |
| Virtual memory + TLB + IOMMU | Day 6-7 |  |
| DMA + doorbells + ordering | Day 8 |  |
| ARM / Snapdragon / boot | Day 9 |  |
| PCIe layers + LTSSM + Gen6 | Day 10 |  |
| NVMe queues + bring-up | Day 11 |  |
| DVFS + PVT + binning | Day 12 |  |
| C-states + PkgC + S0ix | Day 13 |  |
| ADR + Core C6 bug stories | Day 14 |  |
| Thermal + perf counters + multi-IP | Day 15 |  |
| Pipeline + memory model + SIMD | Day 16 |  |
| Concurrency + SPSC + DSA | Day 17 |  |
| NPU stack (ORT / EP / HTP) | Day 18 |  |
| NPU bug stories | Day 19 |  |
| Debug method + 90-sec stories | Day 20 |  |
| Per-company tailoring | Day 21 section 3 |  |

---

## System Thinking — Why the same skill sounds different per company

**Frame:** every silicon validation interview tests the same underlying skills (understand the layer, attribute symptoms to mechanisms, design a measurement, tell a clean bug story), but each company filters those skills through *their stack and their bugs*. The mistake is to memorize per-company facts. The right move is to **carry one toolkit and translate it into the company's vocabulary**. Days 1-20 are the toolkit; this day is the translator.

### Q: Why does each company emphasize a different layer?

Because that's where their bugs live and where their margin is.

| Company | Layer they live at | What they grill on |
|---|---|---|
| **Intel / AMD** | x86 cores, mesh fabric, server uncore | C-states, MESI variants, ADR, microcode, perf counters, Spectre-class side channels |
| **Qualcomm** | ARM SoC, modem, NPU, RPMh | Snapdragon boot chain, RPMh voting, Hexagon HTP, FastRPC, S2idle |
| **NVIDIA** | GPU SM + memory, NVLink, IOMMU | CUDA stream ordering, GPU page faults, NVLink bring-up, MIG |
| **Apple** | Custom ARM cores, AMX/ANE, unified memory | Cache hierarchy on UMA, Secure Enclave boot, ANE EP correctness |
| **AWS / Hyperscaler** | Nitro, custom NICs, Graviton fleets | Live migration, hot-plug, fleet-wide perf counters, blast-radius reasoning |
| **Storage (Pure / NetApp)** | NVMe-oF, RDMA, persistent memory | NVMe queue depth scaling, ADR, end-to-end CRC, dual-controller HA |

**You don't need to know all of them deeply.** You need to know your toolkit (Days 1-20) deeply, and then in interview prep map it onto the company's stack.

### Q: How do I translate "I worked on QNN" into Intel-speak?

By abstracting the *role*, not the *product*:
- "I validated the runtime path between CPU and an accelerator with a non-coherent fabric, focusing on memory consistency and IOMMU correctness."
- That's true at Qualcomm (CPU↔HTP), at Intel (CPU↔NPU/iGPU), at NVIDIA (CPU↔GPU), at Apple (CPU↔ANE).
- Then drop into specifics: "On QNN we hit X bug; in your stack the analog would be Y."

**This is what "system-level thinking" buys you in interviews:** the *pattern* (heterogeneous compute + memory contract + fault attribution) is universal; the *names* are local.

### Q: What's the universal interview structure for any silicon validation role?

Four predictable moves; rehearse all four:
1. **"Tell me about a hard bug."** → Day 20 polished story (symptom → hypothesis → bound → fix → regression hook).
2. **"How does X work?"** (X = cache, MESI, PCIe, DVFS, boot, etc.) → Days 1-19 reasoning packs (frame the tradeoff first, then mechanism).
3. **"How would you debug Y?"** (Y = a hypothetical failure) → method (bisection, counter selection, isolation), not guessed root-cause.
4. **"How would you design a test for Z?"** → measurement design (what counter, what stress, what oracle, what coverage closure).

Every question in every silicon interview maps to one of these four. Practice them as four-question categories, not as 100 separate facts.

### One-liner

*"Every company tests the same four skills: explain a layer with a tradeoff, attribute a symptom to a mechanism, design a measurement, tell a clean bug story. Each company filters those skills through their own product stack — you carry the toolkit, you translate the names. Days 1-20 are the toolkit; this day is the dictionary."*

---

## Personal Narrative — Long-Form Reference

> Single source for the long spoken pitches, anchor numbers, prior-role detail, NPU 60-sec explanation, agentic-CLI talking points, and the Q&A bank. Section 3 above (per-company tailoring) is the night-before card; this section is the underlying material.

### N1. Introduction (90 sec — version you say first)

> "I'm a Silicon Validation Engineer with approximately **eight years of experience across the full silicon lifecycle** — pre-silicon, post-silicon bring-up, and platform-level system validation. Currently at **Microsoft**, I lead post-silicon validation for **Copilot+ PCs across multiple IHV platforms — Intel, Qualcomm, and AMD**. Prior to that, at **Intel**, I owned power-management validation for idle flows — **C-states** and the **ADR reset flow** — and I also contributed to **PCIe Gen6 feature addition for the PCIe transactor**. Before Intel, I was at **L&T at client SanDisk**, part of the post-silicon bring-up and validation team for **NVMe SSDs**, where I created and ran power- and thermal-related post-silicon test scenarios. I hold an M.Tech in CSE from NIT Durgapur."

**Education:** M.Tech CSE — NIT Durgapur, 2018. B.Tech CSE — RGPV Bhopal, 2015.

### N2. Current — Microsoft (June 2024 → present)

**Title:** Device Validation Engineer 2 — Post-Si Platform Validation, Hyderabad.

> "I lead **post-silicon platform validation of NPUs** on Copilot+ PC silicon — Intel Lunar Lake, AMD Strix, Qualcomm Snapdragon X Elite — from EVT through PV. The work answers one question per workload: when a Windows AI app runs, does the NPU actually execute it correctly, fast enough, and within the power budget? I built **EdgeOptimizer**, a platform-level instrumentation tool that profiles NPU vs CPU utilisation, per-inference latency, and energy-per-inference on real devices. RCA uses WPR/WPA hardware traces and ETL logs. I partner with the silicon vendors to triage NPU scheduling stalls, EP-fallback regressions, and suspected silicon issues on the CPU↔NPU path. I've also deployed **agentic AI workflows with GitHub Copilot CLI** to parse and correlate trace data — what was a 2-hour WPA triage becomes a 15-minute report."

**Anchor numbers (drop in when relevant):**

- Multi-vendor NPU coverage: Intel LNL NPU, AMD XDNA, **Qualcomm Hexagon 900 / HTP v79** on Snapdragon X Elite.
- Hexagon HTP family: v68 (SD 888, ~15 TOPS / 4 MB VTCM) → v69 (SD 8 Gen 1, ~18 / 4) → **v73 (SD 8 Gen 2, ~26 / 8 — VTCM doubled)** → v75 (SD 8 Gen 3, ~36 / 8) → **v79 (X Elite, ~45 / 8)**.
- HMX 4096 MACs/instruction; 256×256 tile in INT8; 2-5 ms per tile burst-mode; FastRPC wake ~100 µs; shared DDR ~60 GB/s is the real bottleneck.
- DLL chain (Windows): `onnxruntime_providers_qnn.dll → QnnHtp.dll → QnnHtpV79Stub.dll`; on-device `libQnnHtp.so` is the HTP graph compiler.

**Honest framing:** OEM/platform validation, not vendor IP validation. I correlate ETW counters, EP/ORT versions, source pointers; I do not debug Hexagon RTL or claim VTCM defects.

### N3. Intel (July 2021 → June 2024)

**Title:** Emulation Engineer — Pre-Silicon Validation, Bangalore.

> "Before Microsoft I was at Intel as an Emulation Engineer on Cambria Xeon SoCs. I owned pre-silicon validation of power-management flows — C-states, idle entry/exit, ADR (the asynchronous DRAM refresh power-loss flow) — and the PCIe Gen6 interface. I built 10+ PCIe Gen6 transactor enhancements in C++ and developed Perspec scenario test content on Zebu. I debugged 15+ complex cross-IP issues — memory interfaces, protocol failures, timing mismatches — using bare-metal C and system-level analysis. I earned **Star of the Month** for a CXL firmware validation issue."

**Talking points:** Pre-Si on **Zebu** (Synopsys); Perspec for scenario test. **C-states, ADR, PCU opcode-level tracing** ([Day 14](day14_adr_pcu_core_c6_bugs.md)). **PCIe Gen6** transactor in C++ ([Day 10](day10_pcie_fundamentals_ltssm.md)). 15+ cross-IP bugs. Star of the Month — CXL FW validation.

**Why I left Intel** (neutral, no bridge-burning):

> "Two reasons. **First**, mid-2024 was the inflection point for **on-device AI** — Copilot+ PCs, NPUs shipping in every laptop SoC, the whole AI-PC category being defined. The Microsoft role looked like the chance to work **close to silicon on AI workloads** — NPU bring-up, ORT/EP/runtime path, energy-per-inference on real devices — across Intel, AMD and Qualcomm at the same time. After joining I learned the scope is more **platform/system-level NPU validation** than vendor-IP work, but the AI-silicon exposure (HTP v79, OpenVINO NPU, XDNA, ETW/WPA on real workloads) is still real, and that's exactly the muscle I want to take back to a vendor. **Second**, the Server team was being restructured around the time I moved; the Microsoft Hyderabad team gave me a stronger growth track and a closer feedback loop with vendors. Intel taught me pre-silicon ordering, opcode-level PCU/FW tracing and CXL — that foundation is what makes me effective in NPU and platform validation today."

### N4. L&T (Sep 2018 → July 2021)

**Title:** Software Engineer — Post-Silicon Validation, Bangalore (client: SanDisk).

> "At L&T Technology Services I did post-silicon bring-up and debugging of NVMe SoC platforms for SanDisk — Polaris and others. JTAG with Metaware debugger, Lecroy protocol analyser, oscilloscope for power rails. I developed Python automation covering 25+ power-management test scenarios. I diagnosed 10+ power-related silicon bugs through system-level debugging across firmware and platform layers. I also validated PCIe/NVMe protocol compliance and supported the SDExpress proof-of-concept through first power-on and bring-up — for which I earned a **Spot Award** from SanDisk."

**Talking points:** **JTAG (Metaware), Lecroy, oscilloscope**. Python — 25+ power scenarios. 10+ silicon power bugs root-caused. **SDExpress POC** — Spot Award. Bug story: NVMe CSTS.RDY hang from host AQA mis-sizing ([Day 11](day11_nvme_queues_bringup.md), [Day 20](day20_debug_method_bug_polish.md)).

### N5. NPU execution — 60-sec spoken explanation

> "An AI workload on an NPU runs through six layers — framework, ONNX, ONNX Runtime, Execution Provider, vendor runtime/compiler, silicon. ORT partitions the graph: contiguous subgraphs go to the chosen EP — QNN for Qualcomm Hexagon, OpenVINO for Intel, DirectML for any DX12 device — and CPU EP is always present as the fallback. The fallback is the silent perf cliff — one unsupported op forces a per-op partition and you spend most of your time copying tensors. The vendor compiler does **QDQ fusion** to collapse Quantize-Conv-Dequantize into a single INT8 op, then tiles activations so each tile fits in software-managed local SRAM — Qualcomm calls it VTCM. The compiled artifact is serialised as a context cache so the second launch skips compile. At inference the CPU writes a doorbell, the NPU DMAs input from DDR to local SRAM, and compute/vector/DMA engines run as a pipeline. On Windows I observe all of this with **ETW + WPA**, which is the equivalent of `perf` on Linux."

Long form + bug surface: [Day 18](day18_npu_ort_ep_hexagon_htp.md) + [Day 19](day19_npu_memory_runtime_bugs.md).

### N6. Agentic Copilot-CLI workflow (Microsoft differentiator)

- **GitHub Copilot CLI as an agent** with tool access to ETL/WPA exports and log files.
- The agent autonomously: loads a trace, identifies CPU scheduler + NPU EP events, correlates timestamps, flags anomalies (CPU spike during inference window, missing QNN event, unexpected EP fallback).
- Replaces manual WPA UI navigation for known patterns — 2-hour triage becomes 15-min report.
- Be precise: explain **what the agent does** (tool use, file parsing, pattern detection) — not "AI helped me."

### N7. 30 / 60 / 120-sec versions

- **30 sec:** "I'm a silicon validation engineer. Currently at Microsoft doing post-Si platform validation of NPUs across Intel, AMD, Qualcomm Copilot+ PC silicon. Before that I was at Intel on Cambria pre-Si emulation — C-states, ADR, PCIe Gen6. Before that L&T doing NVMe post-Si bring-up for SanDisk. M.Tech CSE NIT Durgapur. I want to own silicon end-to-end at the vendor side."
- **60 sec:** Section N1 above.
- **2 min:** Section N1 + Section N2 first paragraph + one sentence each from Section N3 and Section N4 + closing line about vendor-side ownership.

Practice each version once per skim. Should feel **natural**, not recited.

### N8. Q&A bank — fluency probes (answer in <60 sec)

**Q1. Pre-silicon vs post-silicon — fundamental difference and what bugs each catches?**
Pre-Si runs on RTL simulators / emulators (Zebu, Palladium) before tape-out; full visibility, infinite breakpoints, slow (kHz). Catches functional and protocol bugs. Post-Si runs on real silicon at full speed (GHz); limited to JTAG, traces and observable signals. Catches analog issues, timing margin, PVT-corner bugs, anything the simulation model didn't capture.

**Q2. Why is emulation faster than simulation but slower than real silicon, and why do we still need it?**
Simulation is software gate-by-gate (Hz–kHz). Emulation maps RTL onto FPGA/custom hardware (MHz). Real silicon is GHz. Emulation is fast enough to boot an OS pre-tape-out, letting firmware/OS teams develop in parallel with hardware. Multi-IP / cross-IP bugs surface there that pure unit simulation misses.

**Q3. "Validation" vs "verification" — how do you draw the line?**
Verification: "did we build the chip the spec said?" — RTL functional checking (DV). Validation: "does the chip do what the customer needs?" — system-level, real workloads, PVT corners, software stack. Verification = unit-test-of-design; validation = integration-test-of-product.

**Q4. Why is multi-SoC validation harder than single-SoC?**
Same workload behaves differently on Intel/AMD/Qualcomm because of cache hierarchy, NPU dispatch policy, frequency governors, thermal limits, driver maturity. Can't blame "the silicon" — must isolate which layer (HW, FW, driver, OS, app) caused a delta. Requires deep knowledge of three architectures plus a common measurement methodology.

**Q5. "Tell me about a time you disagreed with a designer or architect."**
Structure: situation (what you were validating, what designer claimed) → your evidence (waveforms, repro, perf counters) → escalation (1:1 first, then data review with both managers, never email war) → outcome (root cause confirmed on either side — both are valid endings) → process change afterwards. Pick a real disagreement; "I always agreed" is a red flag.

**Q6. "Tell me about a tough bug." — which one do you tell?**
Default: pick from [Day 20](day20_debug_method_bug_polish.md). **Qualcomm CPU system validation:** lead with Core C6 wake-loss. **Intel NPU post-Si:** lead with Screen Region Detector / HMX tile (4d) for HW-pointed credibility, then ADR for Intel-side credibility. **Microsoft lateral:** Screen Region Detector / HMX tile (4d) → QNN page-fault → Paint SR → silent EP fallback.

**Q7. "Why are you leaving Microsoft / your current role?"**
"Not running from Microsoft — going toward vendor-side ownership. Microsoft gave me three-vendor breadth and the EdgeOptimizer/ETL discipline. The next step is to take that platform-validation rigor inside a vendor and own the silicon end-to-end — pre-Si through PV — instead of partnering with the vendor on triage."

**Q8. "What does a typical day look like for you now?"**
Morning: triage previous night's automated runs (ETW traces from device farms across LNL / Strix / X Elite); flag any regression that crossed a gate (page faults, peak VM, EP-assignment %, S0ix residency). Mid-day: deep-dive one regression — version matrix across EP/ORT/SDK/model, correlate counters, file or update vendor partner ticket. Afternoon: EdgeOptimizer feature work or agentic-CLI workflow extension. End of day: cross-vendor review with the LNL / XDNA / Hexagon partners.

**Q9. "Walk me through your strongest technical project."**
EdgeOptimizer. One paragraph: problem (multi-vendor NPU perf comparison was manual, 2-hour triage per regression), solution (platform-level instrumentation tool: ETW provider for ORT/EP events, RAPL/energy collector, EP-assignment tracker, version-matrix dashboard), impact (regression triage time cut from hours to minutes; caught the QNN page-fault regression and the silent-EP-fallback class).

**Q10. "What would you do in your first 90 days here?"**
Days 1-30: shadow current validation flow, read recent bug postmortems, build my own first-pass profile of the silicon and toolchain. Days 31-60: take ownership of one regression class end-to-end; ship one improvement to the validation gate. Days 61-90: propose one cross-team workflow improvement (likely an agentic-trace pipeline) based on what's slow now.

### N9. Daily skim order (5 min flat)

1. Read Section N1 once aloud.
2. Read Section N2 first paragraph aloud.
3. Pick one Q from Section N8 and answer aloud without looking.
4. Pick one bug story from [Day 20](day20_debug_method_bug_polish.md) and tell it in 90 sec.
5. Move to today's technical day file.

If skimming runs past 5 min, you're procrastinating.

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **lens detection** — *what:* identify role flavor (pre-Si vs post-Si vs system vs NPU) / *why:* same skill, different framing per audience / *how:* JD keywords + interviewer team + first 2 questions / *connects:* Section 0A of revision guide, headline framing
- **headline framing** — *what:* one bug story with tradeoff lens / *why:* first 30s sets the room / *how:* problem → method → bug → impact, ≤ 90s / *connects:* CFC, QNN cache, NVMe CSTS.RDY — pick by lens
- **cheat-line per company** — *what:* one reflex line per culture / *why:* show alignment fast / *how:* mirror their vocabulary (PnP, post-Si, pre-Si, NPU) / *connects:* Intel = PnP/PVT, QC = power+perf, MS = SW/HW seam
- **what NOT to volunteer** — *what:* weak-area topics / *why:* don't open doors you can't walk through / *how:* redirect to adjacent strength / *connects:* behavioral judo, time-budget management
- **the four universal skills** — *what:* explain-with-tradeoff / attribute-symptom-to-mechanism / design-a-measurement / tell-a-clean-bug-story / *why:* every company filters these through its product stack / *how:* Days 1–20 are the toolkit, Day 21 is the translator / *connects:* why the same prep works across all targets

---

## Bug Anchor

- All 21 days. This is the rehearsal day; every story and every concept comes from a prior day.

## Exit Check

- [ ] Ran the 60-min mock loop end-to-end without notes.
- [ ] Recited the 60-sec "tell me about yourself" cleanly.
- [ ] Have the 3 bug stories chosen for the target company.
- [ ] Have the per-company anchor sentences memorised.
- [ ] Have the 4-sentence "why this company" template ready, sentence 4 customised.
- [ ] Self-assessment done; any ≤ 3 area re-read.
- [ ] Sleep.

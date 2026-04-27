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
| Why `volatile`? | prevents compiler caching/reordering/eliminating MMIO accesses; *not* a CPU barrier |
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
| VTCM (HTP v79) | 8 MB on-chip SRAM, software-managed, not in CPU coherence |
| HTP execute pipeline | DMA(N+1) + HMX(N) + HVX(N-1) — 3 engines, ~45 TOPS |
| Context cache | serialised compiled artifact; second launch skips compile |
| Page-fault story takeaway | correlate with VirtualAlloc + peak VM + version matrix |

### 3. Per-company tailoring

Same core narrative; emphasis adjusted to the team's product.

#### Qualcomm (Snapdragon, Hexagon NPU, Auto, IoT)

**Lead with:** post-Si platform validation across **three NPU vendors** (Intel/AMD/**Qualcomm** Hexagon HTP v79); strong on **ARM/Snapdragon vocabulary** (EL0-3, GIC, SMMU, RPMh, PSCI, LPDDR training, big.LITTLE / DynamIQ); **NVMe post-Si bring-up** background.

**Anchor sentences:**

- "I do post-Si platform validation on **Hexagon HTP v79 / Snapdragon X Elite**, **8 MB VTCM**, ONNX Runtime QNN EP path."
- "I'm comfortable with the ARM platform: EL0-3, GIC interrupt routing, SMMU stream IDs, RPMh / PMIC, and PSCI for power state coordination."
- "Hexagon's 3-engine pipeline — HMX matmul, HVX vector, DMA — is how you actually hit ~45 TOPS; one stalled engine and the pipeline degrades to serial."

**Bug stories to lead with:** QNN page-fault (4) → Paint SR vs Camera (4b) → NVMe CSTS.RDY (2). Save ADR (1) and Core C6 (3) for if Intel-side comes up.

**Day-anchors to revise night-before:** Day 9 (ARM/Snapdragon vocab), Day 11 (NVMe), Day 16 (memory model), Day 18 (NPU stack), Day 19 (NPU bugs), Day 20 (debug + stories).

#### Intel (Xeon, Client, NPU, FPGA)

**Lead with:** **Cambria pre-Si emulation on Zebu** (Intel role); **PCIe Gen6 transactor in C++** (10+ enhancements); **ADR / Core C6** bug stories from your Cambria PCU/PMA work; **CXL** familiarity (3 sub-protocols).

**Anchor sentences:**

- "At Intel I owned pre-Si validation of power-management flows on Cambria — C-states, ADR, PCIe Gen6 — on Zebu emulation, with Perspec test content and PCU opcode-level tracing."
- "I built 10+ PCIe Gen6 transactor enhancements in C++ and developed Perspec scenarios for cross-IP validation."
- "Star-of-the-month for a CXL FW issue — CXL.io / CXL.cache / CXL.mem are the three sub-protocols on the PCIe PHY."

**Bug stories to lead with:** ADR opcode-ordering (1) → Core C6 ARAT wake-loss (3) → NVMe CSTS.RDY (2). QNN story (4) is your bridge into modern AI work.

**Day-anchors:** Day 10-11 (PCIe + NVMe), Day 12-14 (DVFS, C-states, ADR), Day 15 (RAPL/perf-counters), Day 16 (TSO + pipeline), Day 20 (stories).

#### Microsoft (Copilot+ PC, Azure, Surface)

**Lead with:** the **current job** — multi-vendor post-Si platform validation, EdgeOptimizer profiler, ETL/WPA, agentic Copilot CLI workflows; the **breadth across Intel + AMD + Qualcomm** is the differentiator.

**Anchor sentences:**

- "I run post-Si platform validation across **three vendors** — Intel LNL NPU, AMD XDNA, Qualcomm Hexagon HTP v79 — on Copilot+ PC silicon, with the same workloads and a common measurement methodology."
- "I built **EdgeOptimizer**, a platform-level profiler that surfaces EP-fallback, NPU vs CPU utilisation, and energy-per-inference regressions before launch."
- "My agentic Copilot CLI workflow autoloads ETL traces, correlates CPU scheduler events with NPU EP events, and turns a 2-hour WPA triage into a 15-min report."

**Bug stories to lead with:** QNN page-fault (4) → Paint SR vs Camera (4b) → silent EP fallback (5). The Intel/L&T stories are your foundation, not the headline.

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

## Practice Q&A

**Q1. 60-second "tell me about yourself" structure?**
Current role → past experience → why this role. Microsoft → Intel → L&T → close.

**Q2. Tailoring for Qualcomm — what do you lead with?**
Multi-vendor NPU validation including Hexagon HTP v79, ARM/Snapdragon vocabulary, NVMe post-Si background.

**Q3. Tailoring for Intel — what do you lead with?**
Cambria Zebu pre-Si, PCIe Gen6 C++ transactor, ADR / Core C6 bug stories, CXL familiarity.

**Q4. Tailoring for Microsoft — what do you lead with?**
The current job — three-vendor breadth, EdgeOptimizer, ETL/WPA + agentic workflows.

**Q5. Three bug stories you'd lead with for Qualcomm?**
QNN page-fault (4), Paint SR vs Camera (4b), NVMe CSTS.RDY (2).

**Q6. Three bug stories you'd lead with for Intel?**
ADR (1), Core C6 (3), NVMe CSTS.RDY (2). QNN as bridge to modern AI work.

**Q7. The 4-sentence "why this company" template?**
What I do now → what I want → why my background fits → why this company specifically (research-driven).

**Q8. Night-before reread order?**
Per-company anchor sentences → Day 20 stories → Day 16 memory model → Day 9 ARM (if QC) → Day 11 boot (if QC) → Day 17 spinlock + SPSC.

**Q9. Honest framing for the QNN story when Microsoft asks?**
Platform validation; correlated EP/ORT versions, ETL counters and source pointers; did not debug Hexagon RTL or claim VTCM defects.

**Q10. The meta-answer — your debug method?**
Observe → reproduce → isolate → hypothesise → test → root-cause → fix. Convert vague failure into one controlled signal.

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

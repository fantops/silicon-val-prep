# Day 0 — Career Narrative + Daily Revision Skim Guide

## How to use this file

- **Daily skim (5 min):** read section 1 (Introduction) + section 2 (Current Work) out loud once. That's it.
- **Pre-loop reread (15 min):** sections 1-5 + the per-company anchor sentences (section 6) + the Q&A (section 9).
- The numbered sections below (1, 2, 3, …) are this file's table of contents. The technical days (1-21) are separate files — they hold the substance behind this narrative.

---

## 1. Introduction (90 sec, the version you say first)

> "I'm a Silicon Validation Engineer with approximately **eight years of experience across the full silicon lifecycle** — pre-silicon, post-silicon bring-up, and platform-level system validation. Currently at **Microsoft**, I lead post-silicon validation for **Copilot+ PCs across multiple IHV platforms — Intel, Qualcomm, and AMD**. Prior to that, at **Intel**, I owned power-management validation for idle flows — **C-states** and the **ADR reset flow** — and I also contributed to **PCIe Gen6 feature addition for the PCIe transactor**. Before Intel, I was at **L&T at client SanDisk**, part of the post-silicon bring-up and validation team for **NVMe SSDs**, where I created and ran power- and thermal-related post-silicon test scenarios. I hold an M.Tech in CSE from NIT Durgapur."

**Education:** M.Tech CSE — NIT Durgapur, 2018. B.Tech CSE — RGPV Bhopal, 2015.

---

## 2. Current Work — Microsoft (June 2024 → present)

**Title:** Device Validation Engineer 2 — Post-Si Platform Validation, Hyderabad.

**The 90-sec pitch:**

> "I lead **post-silicon platform validation of NPUs** on Copilot+ PC silicon — Intel Lunar Lake, AMD Strix, Qualcomm Snapdragon X Elite — from EVT through PV. The work answers one question per workload: when a Windows AI app runs, does the NPU actually execute it correctly, fast enough, and within the power budget? I built **EdgeOptimizer**, a platform-level instrumentation tool that profiles NPU vs CPU utilisation, per-inference latency, and energy-per-inference on real devices. RCA uses WPR/WPA hardware traces and ETL logs. I partner with the silicon vendors to triage NPU scheduling stalls, EP-fallback regressions, and suspected silicon issues on the CPU↔NPU path. I've also deployed **agentic AI workflows with GitHub Copilot CLI** to parse and correlate trace data — what was a 2-hour WPA triage becomes a 15-minute report."

**Anchor numbers (drop these in):**

- Multi-vendor NPU coverage: Intel LNL NPU, AMD XDNA, **Qualcomm Hexagon 900 / HTP v79** on Snapdragon X Elite.
- Hexagon HTP family: v68 (SD 888, ~15 TOPS / 4 MB VTCM) → v69 (SD 8 Gen 1, ~18 / 4) → **v73 (SD 8 Gen 2, ~26 / 8 — VTCM doubled)** → v75 (SD 8 Gen 3, ~36 / 8) → **v79 (X Elite, ~45 / 8)**.
- HMX 4096 MACs/instruction; 256×256 tile in INT8; 2-5 ms per tile burst-mode; FastRPC wake ~100 µs; shared DDR ~60 GB/s is the real bottleneck.
- DLL chain (Windows): `onnxruntime_providers_qnn.dll → QnnHtp.dll → QnnHtpV79Stub.dll`; on-device `libQnnHtp.so` is the HTP graph compiler.
- ETL evidence example: Drop 239 (SDK 1.8.6 / ORT 1.23.4) good vs Drop 245 + SDK 2-preview2 (ORT 1.24.4) regressed.

**Honest framing:** OEM/platform validation, not vendor IP validation. I correlate ETW counters, EP/ORT versions, source pointers; I do not debug Hexagon RTL or claim VTCM defects.

**Detailed NPU mechanism:** see [Day 18](day18_npu_ort_ep_hexagon_htp.md). Microsoft bug stories: see [Day 19](day19_npu_memory_runtime_bugs.md).

---

## 3. Intel Work (July 2021 → June 2024)

**Title:** Emulation Engineer — Pre-Silicon Validation, Bangalore.

**The 60-sec pitch:**

> "Before Microsoft I was at Intel as an Emulation Engineer on Cambria Xeon SoCs. I owned pre-silicon validation of power-management flows — C-states, idle entry/exit, ADR (the asynchronous DRAM refresh power-loss flow) — and the PCIe Gen6 interface. I built 10+ PCIe Gen6 transactor enhancements in C++ and developed Perspec scenario test content on Zebu. I debugged 15+ complex cross-IP issues — memory interfaces, protocol failures, timing mismatches — using bare-metal C and system-level analysis. I earned **Star of the Month** for a CXL firmware validation issue."

**Key talking points:**

- Pre-Si on **Zebu** emulation (Synopsys); Perspec for scenario test.
- Power management validation: **C-states, ADR, PCU opcode-level tracing** (see [Day 14](day14_adr_pcu_core_c6_bugs.md)).
- **PCIe Gen6** transactor work in C++ (see [Day 10](day10_pcie_fundamentals_ltssm.md)).
- 15+ cross-IP bugs found pre-Si.
- Star of the Month — CXL firmware validation.

**Bug stories from this role:** ADR opcode-ordering hang ([Day 14](day14_adr_pcu_core_c6_bugs.md)), Core C6 wake-loss on back-to-back ARAT ([Day 14](day14_adr_pcu_core_c6_bugs.md)).

### Why I left Intel

> "Two reasons. **First**, mid-2024 was the inflection point for **on-device AI** — Copilot+ PCs, NPUs shipping in every laptop SoC, the whole AI-PC category being defined. The Microsoft role looked like the chance to work **close to silicon on AI workloads** — NPU bring-up, ORT/EP/runtime path, energy-per-inference on real devices — across Intel, AMD and Qualcomm at the same time. After joining I learned the scope is more **platform/system-level NPU validation** than vendor-IP work, but the AI-silicon exposure (HTP v79, OpenVINO NPU, XDNA, ETW/WPA on real workloads) is still real, and that's exactly the muscle I want to take back to a vendor. **Second**, the Server team was being restructured around the time I moved; the Microsoft Hyderabad team gave me a stronger growth track and a closer feedback loop with vendors. Intel taught me pre-silicon ordering, opcode-level PCU/FW tracing and CXL — that foundation is what makes me effective in NPU and platform validation today."

Keep it neutral. No bridge-burning. The Intel time is an **asset** in your story, not a chapter you're escaping. The honest "I joined for AI-close-to-silicon, learned it's platform-level" framing is **strength**, not weakness — it shows self-awareness and explains the next move (back to vendor-side, but now with AI/NPU context).

---

## 4. L&T Work (Sep 2018 → July 2021)

**Title:** Software Engineer — Post-Silicon Validation, Bangalore (client: SanDisk).

**The 60-sec pitch:**

> "At L&T Technology Services I did post-silicon bring-up and debugging of NVMe SoC platforms for SanDisk — Polaris and others. JTAG with Metaware debugger, Lecroy protocol analyser, oscilloscope for power rails. I developed Python automation covering 25+ power-management test scenarios. I diagnosed 10+ power-related silicon bugs through system-level debugging across firmware and platform layers. I also validated PCIe/NVMe protocol compliance and supported the SDExpress proof-of-concept through first power-on and bring-up — for which I earned a **Spot Award** from SanDisk."

**Key talking points:**

- Real silicon bring-up — **JTAG (Metaware), Lecroy, oscilloscope**.
- Python test automation — 25+ power scenarios.
- 10+ silicon power bugs root-caused.
- **SDExpress POC** — Spot Award.

**Bug stories from this role:** NVMe CSTS.RDY hang from host AQA mis-sizing ([Day 11](day11_nvme_queues_bringup.md), [Day 20](day20_debug_method_bug_polish.md)).

---

## 5. The Why-this-Company template (4 sentences)

1. **What I do now** — Microsoft post-Si platform validation, multi-vendor NPU.
2. **What I want** — closer to silicon, own the validation end-to-end.
3. **Why my background fits** — pre-Si Intel + post-Si L&T + platform Microsoft = the full lifecycle.
4. **Why this company specifically** — research-driven, two lines about their product.

Sentences 1-3 stay the same. Customise sentence 4 per company.

---

## 6. Per-company flavour

### A. Qualcomm — CPU System Validation (Oryon / Snapdragon)

**Lead with:** post-Si platform validation on **three vendors including Qualcomm**; strong on **ARM/Snapdragon vocabulary** (EL0-3, GIC, SMMU, RPMh, PSCI, LPDDR training, DynamIQ); NVMe post-Si background; **Hexagon HTP v79 / 8 MB VTCM** familiarity from current job.

**Anchor sentences:**

- "On the current Microsoft platform side I validate workloads against **Snapdragon X Elite** with Hexagon HTP v79 — that's the ORT/QNN EP/HTP path with 8 MB VTCM."
- "I'm comfortable with the ARM platform stack: EL0-3 with TrustZone, GIC interrupt routing with ITS for MSIs, SMMU stream IDs, **RPMh** as the power-state coordinator over the **SPMI** bus to the PMIC, and **PSCI** for CPU-level power state requests."
- "On the CPU system side specifically — TSO vs ARM weak memory model, `dmb st` between data and doorbell, big.LITTLE / DynamIQ topology and per-core DVFS, LPDDR5X training and refresh."
- "On the validation method side — pre-Si Cambria PCU work on Zebu at Intel taught me opcode-level FW tracing and ordering, which transfers directly to RPMh and PSCI flows."

**Bug stories to lead with for QC CPU system validation:**

1. **Core C6 wake-loss (CFC FIFO race)** — clean fit for CPU power-state ownership work; same kind of bug RPMh/Oryon engineers chase. ([Day 14](day14_adr_pcu_core_c6_bugs.md), [Day 20 story 3](day20_debug_method_bug_polish.md))
2. **NVMe CSTS.RDY hang (AQA mis-sized)** — CPU-side spec literacy + bring-up. ([Day 11](day11_nvme_queues_bringup.md), [Day 20 story 2](day20_debug_method_bug_polish.md))
3. **QNN/ORT page-fault + VM regression** — the bridge story showing modern AI-workload exposure. ([Day 19](day19_npu_memory_runtime_bugs.md), [Day 20 story 4](day20_debug_method_bug_polish.md))

**Why Qualcomm (sentence 4):** "Snapdragon X Elite and the Oryon CPU are where laptop-class ARM is being defined. CPU system validation on Oryon — power-state coordination through RPMh, memory model on a weak-ordering ISA, big.LITTLE / DynamIQ scheduling under workloads — is exactly the cross of pre-Si discipline (from my Intel C-state work) and platform validation (from my Microsoft multi-vendor work) that I want to own end-to-end."

**Day-anchors to revise night-before for QC:** [Day 9](day09_arm_qualcomm_platform_vocabulary.md), [Day 11](day11_nvme_queues_bringup.md), [Day 13](day13_cstates_pkgc_s0ix.md), [Day 14](day14_adr_pcu_core_c6_bugs.md), [Day 16](day16_pipeline_memory_consistency_simd.md), [Day 18](day18_npu_ort_ep_hexagon_htp.md), [Day 19](day19_npu_memory_runtime_bugs.md), [Day 20](day20_debug_method_bug_polish.md).

### B. Intel — NPU Post-Silicon Validation (Lunar Lake / future client NPU)

**Lead with:** **post-Si NPU validation as the current job** + **Intel pre-Si insider context** (you've already worked on Cambria Xeon, you know the Intel debug culture, MSR / RAPL / ETL flows, and Perspec). Intel-NPU is the clean intersection of both halves of your CV.

**Anchor sentences:**

- "I already do post-Si NPU validation across three vendors — including the Intel **Lunar Lake NPU through OpenVINO EP**. So the workload mechanics — graph partition, EP fallback, context cache, energy-per-inference, Modern Standby — are my current daily work, just generalised across vendors."
- "I have prior Intel pre-Si experience on Cambria — C-state validation, ADR, PCIe Gen6 transactor on Zebu. I know the Intel debug-tool stack: MSR_CORE_PERF_LIMIT_REASONS, RAPL, ETL/WPA, Perspec scenario content, and HSD."
- "On the NPU Intel side specifically — the OpenVINO EP path lowers ONNX → IR → MYRIAD/NPU plugin; the LNL NPU has ~2 MB on-chip SRAM (vs Hexagon 8 MB), so tile-size discipline is even more critical, and energy-per-inference is the figure of merit, not raw TOPS."
- "Honest framing — at Microsoft I'm OEM-side platform validation; what I want at Intel is to **own the silicon at the vendor side**, take the inference path from EVT through PV with the design and FW teams in the same room."

**Bug stories to lead with for Intel NPU post-Si:**

1. **QNN/ORT page-fault + VM regression** — even though it's the QC EP, the *method* (version matrix, correlate counters together, never blame silicon first) is exactly Intel post-Si discipline. Translate to OpenVINO EP framing. ([Day 19](day19_npu_memory_runtime_bugs.md), [Day 20 story 4](day20_debug_method_bug_polish.md))
2. **Paint SR starves Camera preview** — single-queue NPU scheduler; same structural class on any NPU. ([Day 19](day19_npu_memory_runtime_bugs.md), [Day 20 story 4b](day20_debug_method_bug_polish.md))
3. **ADR opcode-ordering hang** — your strongest Intel-side technical story; shows you can read PCU opcode traces. ([Day 14](day14_adr_pcu_core_c6_bugs.md), [Day 20 story 1](day20_debug_method_bug_polish.md))

**Why Intel NPU post-Si (sentence 4):** "The Lunar Lake NPU and what comes after it is where Intel re-enters the AI accelerator conversation. Post-Si validation of that — from EVT silicon through PV, owning the EP/runtime/silicon interface end-to-end with the design team — combines my current platform-NPU work at Microsoft with the pre-Si discipline I built at Intel on Cambria. I want to close that loop on the vendor side."

**Day-anchors to revise night-before for Intel NPU:** [Day 10](day10_pcie_fundamentals_ltssm.md), [Day 12](day12_dvfs_pstates_pvt.md), [Day 13](day13_cstates_pkgc_s0ix.md), [Day 14](day14_adr_pcu_core_c6_bugs.md), [Day 15](day15_thermal_power_perf_counters.md), [Day 18](day18_npu_ort_ep_hexagon_htp.md), [Day 19](day19_npu_memory_runtime_bugs.md), [Day 20](day20_debug_method_bug_polish.md).

### C. Microsoft (current employer / lateral) — multi-vendor platform

See [Day 21 section 3 Microsoft](day21_final_mock_company_tailoring.md) — the current-job framing with EdgeOptimizer, ETL/WPA, agentic Copilot CLI.

---

## 7. NPU Execution — the 60-sec spoken explanation

> "An AI workload on an NPU runs through six layers — framework, ONNX, ONNX Runtime, Execution Provider, vendor runtime/compiler, silicon. ORT partitions the graph: contiguous subgraphs go to the chosen EP — QNN for Qualcomm Hexagon, OpenVINO for Intel, DirectML for any DX12 device — and CPU EP is always present as the fallback. The fallback is the silent perf cliff — one unsupported op forces a per-op partition and you spend most of your time copying tensors. The vendor compiler does **QDQ fusion** to collapse Quantize-Conv-Dequantize into a single INT8 op, then tiles activations so each tile fits in on-chip SRAM — Qualcomm calls it VTCM, 8 MB on HTP v79; Intel ~2 MB; AMD ~4 MB. The compiled artifact is serialised as a context cache so the second launch skips compile. At inference the CPU writes a doorbell, the NPU DMAs input from DDR to VTCM, and **HMX, HVX, DMA run as a 3-engine pipeline** — DMA on tile N+1, HMX on tile N, HVX on tile N-1 — that's how you hit ~45 TOPS on v79. On Windows I observe all of this with **ETW + WPA**, which is the equivalent of `perf` on Linux."

Long form and bug surface: [Day 18](day18_npu_ort_ep_hexagon_htp.md) + [Day 19](day19_npu_memory_runtime_bugs.md).

---

## 8. Agentic Workflow talking points (Microsoft differentiator)

- **GitHub Copilot CLI as an agent** with tool access to ETL/WPA exports and log files.
- The agent autonomously: loads a trace, identifies CPU scheduler + NPU EP events, correlates timestamps, flags anomalies (CPU spike during inference window, missing QNN event, unexpected EP fallback).
- Replaces manual WPA UI navigation for known patterns — a 2-hour triage becomes a 15-min report.
- Be precise: explain **what the agent does** (tool use, file parsing, pattern detection) — not "AI helped me."

---

## 9. Basic Q&A from this day (fluency probes — answer in <60 sec)

**Q1. Pre-silicon vs post-silicon — what's the fundamental difference and what bugs does each catch?**
Pre-Si runs on RTL simulators / emulators (Zebu, Palladium) before tape-out; you have full visibility, infinite breakpoints, but slow speed (kHz range). It catches functional and protocol bugs. Post-Si runs on real silicon at full speed (GHz); limited to JTAG, traces and observable signals. It catches analog issues, timing margin, PVT-corner bugs, and anything the simulation model didn't capture.

**Q2. Why is emulation faster than simulation but slower than real silicon, and why do we still need it?**
Simulation is software running gate-by-gate (Hz to kHz). Emulation maps RTL onto FPGA/custom hardware (MHz). Real silicon is GHz. Emulation is fast enough to boot an OS pre-tape-out, which lets firmware and OS teams develop in parallel with hardware. It's also where multi-IP / cross-IP bugs surface that pure unit simulation misses.

**Q3. "Validation" vs "verification" — how do you draw the line?**
Verification: "did we build the chip the spec said?" — typically RTL functional checking (DV). Validation: "does the chip do what the customer needs?" — system-level, real workloads, PVT corners, software stack. Verification is unit-test-of-design; validation is integration-test-of-product.

**Q4. Why is multi-SoC validation harder than single-SoC?**
The same workload behaves differently on Intel/AMD/Qualcomm because of cache hierarchy, NPU dispatch policy, frequency governors, thermal limits, and driver maturity. You can't blame "the silicon" — you must isolate which layer (HW, FW, driver, OS, app) caused a delta. That requires deep knowledge of three different architectures plus a common measurement methodology.

**Q5. "Tell me about a time you disagreed with a designer or architect."**
Structure: situation (what you were validating, what designer claimed) → your evidence (waveforms, repro, perf counters) → escalation (1:1 first, then data review with both managers, never email war) → outcome (root cause confirmed on either side — both are valid endings) → process change afterwards. Pick a real disagreement; "I just always agreed" is a red flag.

**Q6. "Tell me about a tough bug." — which one do you tell?**
Default: pick from [Day 20](day20_debug_method_bug_polish.md) section 6. **Qualcomm CPU system validation:** lead with Core C6 wake-loss (story 3). **Intel NPU post-Si:** lead with QNN page-fault (story 4) framed as method, then ADR (story 1) for Intel-side credibility. **Microsoft lateral:** QNN page-fault (4) → Paint SR (4b) → silent EP fallback.

**Q7. "Why are you leaving Microsoft / your current role?"**
"Not running from Microsoft — going toward vendor-side ownership. Microsoft gave me three-vendor breadth and the EdgeOptimizer/ETL discipline. The next step is to take that platform-validation rigor inside a vendor and own the silicon end-to-end — pre-Si through PV — instead of partnering with the vendor on triage."

**Q8. "What does a typical day look like for you now?"**
Morning: triage the previous night's automated runs (ETW traces from device farms across LNL / Strix / X Elite); flag any regression that crossed a gate (page faults, peak VM, EP-assignment %, S0ix residency). Mid-day: deep-dive one regression — version matrix across EP/ORT/SDK/model, correlate counters, file or update vendor partner ticket. Afternoon: EdgeOptimizer feature work or agentic-CLI workflow extension. End of day: cross-vendor review with the LNL / XDNA / Hexagon partners.

**Q9. "Walk me through your strongest technical project."**
EdgeOptimizer. One paragraph: problem (multi-vendor NPU perf comparison was manual, 2-hour triage per regression), solution (platform-level instrumentation tool: ETW provider for ORT/EP events, RAPL/energy collector, EP-assignment tracker, version-matrix dashboard), impact (regression triage time cut from hours to minutes; caught the QNN page-fault regression and the silent-EP-fallback class).

**Q10. "What would you do in your first 90 days here?"**
Three buckets, regardless of company. Days 1-30: shadow current validation flow, read recent bug postmortems, build my own first-pass profile of the silicon and toolchain. Days 31-60: take ownership of one regression class end-to-end; ship one improvement to the validation gate. Days 61-90: propose one cross-team workflow improvement (likely an agentic-trace pipeline) based on what's slow now.

---

## 10. The 30-sec / 60-sec / 2-min versions

**30 sec:** "I'm a silicon validation engineer. Currently at Microsoft doing post-Si platform validation of NPUs across Intel, AMD, Qualcomm Copilot+ PC silicon. Before that I was at Intel on Cambria pre-Si emulation — C-states, ADR, PCIe Gen6. Before that L&T doing NVMe post-Si bring-up for SanDisk. M.Tech CSE NIT Durgapur. I want to own silicon end-to-end at the vendor side."

**60 sec:** section 1 above.

**2 min:** section 1 + section 2 first paragraph + one sentence from section 3 + one sentence from section 4 + closing line about vendor-side ownership.

Practice each version once per skim. They should feel **natural**, not recited.

---

## Daily Skim Order (5 minutes flat)

1. Read section 1 once aloud.
2. Read section 2 first paragraph aloud.
3. Pick one Q from section 9 and answer it aloud without looking.
4. Pick one bug story from [Day 20](day20_debug_method_bug_polish.md) and tell it in 90 sec.
5. Move to today's technical day file.

That's it. Don't skim more than 5 minutes — if you're skimming for 20 minutes, you're using this as procrastination.

---

## Exit Check

- [ ] Can give the 30-sec version cleanly.
- [ ] Can give the 60-sec version cleanly.
- [ ] Have a clear, neutral "why I left Intel" answer.
- [ ] Have per-company sentence 4 ("why this company") drafted for QC and Intel NPU.
- [ ] Have 3 bug stories selected for QC and 3 for Intel NPU.
- [ ] Have answered 5 of the section 9 questions out loud, ≤60 sec each.

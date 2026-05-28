# Day 24 — Final Mock Interview Day

## Concept

The final day is a **rehearsal day, not a learning day.** You've covered Foundations (Phase 0), CPU architecture + IP functional val (Phase 1), CPU power management pre-Si and post-Si (Phase 2), and PCIe end-to-end including PCIe × PM (Phase 3). Today you run **one timed mock interview loop**, sweep the flashcards, and lock the bug stories. The goal is **density and recall**, not new material.

## Why this day in the flow

Days 0-22 built the technical foundation. Day 23 closed out any current-role hand-off and retained one anchor bug story. Today is the **packaging day** — speaking the material out loud, on a clock, in interview shape.

In the Layer map this is the **closing day of the plan.**

---

## 1. Mock Interview Loop Structure

Run one timed loop (90 minutes total). Run alone with a self-tape, with a peer, or with a paid mock service.

| Slot | Duration | Format |
|---|---|---|
| **Tell-me-about-yourself + why this role** | 5 min | One-take; record yourself; play back |
| **CPU architecture deep-dive** (pick at random: MESI / VM / TLB / DMA-barrier / memory order) | 15 min | Whiteboard-style; talk + draw; expect probing |
| **CPU PM deep-dive** (pick: C-states / UPF / pre-Si val / post-Si debug) | 15 min | Same; end with one bug story |
| **PCIe deep-dive** (pick: LTSSM / enum / L1.2 / PCIe×PM blocker chain) | 15 min | Same |
| **Hard bug story** | 10 min | One of ADR / Core C6 / L0p / QNN — 90 sec story + 8 min Q&A |
| **Coding** (systems C) | 20 min | One of: bring-up ladder, blocker arbiter, LTSSM classifier, residency parser |
| **Questions for them** | 10 min | Have 5 questions ready (see below) |

### Generic questions to ask the interviewer

1. "What's the IP / sub-system I'd be validating in the first six months? Where is it in its life-cycle (pre-RTL freeze, A0 bring-up, B-step characterisation)?"
2. "How does the team split work between unit val, subsystem, and SoC?"
3. "What's the regression environment — VCS or Xcelium? UVM, Perspec, or both?"
4. "What's the biggest open coverage gap right now, in your opinion?"
5. "What's the on-call / lab rotation like once silicon arrives?"

### Common mistakes to avoid

- **Spending more than 90 seconds on any past-role context** — current-role specifics belong in the opener, not in every answer.
- **Pretending to know an internal tool you don't.** Say "I haven't used `<tool>` directly, but I've used the equivalent `<open-source / adjacent>` stack and the concepts map; I'd ramp in week 1."
- **Skipping the "why this lens" answer.** Have one ready for IP-Val *vs* SoC *vs* SW: "the highest-leverage coverage work is at the IP boundary, where the spec is tight and the bug cost rises fastest if it leaks."
- **Forgetting to ask questions.** Always have 5 ready.

---

## 2. Self-Tape Loop

If running solo:

- Phone camera or Zoom record-to-self.
- Run each slot above on the timer. Don't pause.
- After each slot: 60-sec self-review — did I lead with the tradeoff? Did I draw the diagram? Did I land a concrete example? Did I stop when I should have?
- After the full 90 min: total-loop review — which slot felt loose? That's tomorrow's drill.

---

## 3. Revision Sweep — Flashcards (Self-Test)

Run these as 1-minute drills. Out loud. Time yourself.

### Foundations
- Cache geometry decoder (`tag | set | offset`) on a worked example.
- `volatile` — three things it prevents, two things it doesn't.
- W1C status bit — why RMW silently breaks it.
- Aligned-malloc trick + how `aligned_free` finds the original pointer.

### CPU architecture
- 5 MESI states + the 3 transitions on a write-from-Invalid.
- 4-level page walk; what each level holds; why not flat.
- TLB shootdown via IPI sequence; ARM TLBI broadcast contrast.
- Acquire vs release vs sequential consistency in one sentence each.
- Pipeline hazards: structural / data (RAW/WAR/WAW) / control + one mitigation each.
- Descriptor → barrier → doorbell — why exactly that order, why it fails at line rate.
- CPU bring-up 9-stage ladder (reset → clocks → BIST → MMU → cache → INT → timers → SMP → boot).

### CPU PM
- C0 / C1 / C1E / C6 / C10 — what's off in each.
- PkgC6 entry blockers (name 6).
- ICG cell internals + why the latch (vs plain AND).
- Header vs footer FET tradeoff.
- UPF declarations needed for a power domain (power, iso, retention, level-shifter).
- SAVE → ISO → CLAMP → POWER_OFF sequence; the three assertion violations if skipped.
- DVFS V-before-F up, F-before-V down — setup-slack argument.
- $T_j = T_a + P\theta_{JA}$; leakage-temperature runaway; throttle as negative feedback.
- 3-axis PM coverage (state × transition × residency).
- ADR sequence + the bug that broke it.

### PCIe
- LTSSM 10 states (Detect → Polling → Config → L0 → Recovery → L0s/L1/L1.1/L1.2/L2).
- BAR sizing trick — write all-1s, read back, invert+1.
- Capability chain — linked list at offset 0x34.
- MSI vs MSI-X scaling argument.
- ASPM ladder + what each level gates (link / refclk / Vcm / CLKREQ#).
- L1.2 entry sequence (5 events in order) + exit sequence (5 events in order).
- LTR — what the EP advertises, what the RC does with it.
- Gen3+ equalisation 4 phases.
- AER categories (correctable / non-fatal / fatal).
- L0p mid-FLIT — symptom + cause + fix.

### Tools
- Pre-Si: VCS / Xcelium / Verdi (FSDB) / Zebu / Palladium / HAPS / Perspec / JasperGold / VC Formal / **VC-LP / Conformal-LP / UPF (IEEE 1801)** / FastModel / Spike.
- Post-Si: Lauterbach TRACE32 / DCI / ITP / `turbostat` / `socwatch` / `perf` / `rdmsr` / `wrmsr` / RAPL / `lspci` / `setpci` / sysfs / Lecroy Summit T3 / Teledyne PETrainer / Keysight scope / Quarch PPM / Monsoon HV.

### Bug stories (90 sec each, STAR-shape)
- ADR opcode ordering (pre-Si PCU FW bug).
- Core C6 wake-loss (post-Si CFC FIFO race).
- L0p mid-FLIT rescale (Gen6 PCIe).
- QNN page-fault on wake (retained current-role anchor).

---

## 4. Final Mental Model

Carry into the interview:

1. **The Layer map** — Foundations → CPU IP → CPU PM → PCIe → Cross-IP. Every question lands on a layer; you know the layer.
2. **The 7-step debug framework** — observe → reproduce → isolate → hypothesise → test → root-cause → fix. Use it as the structure for any debug-style question.
3. **The "coverage closes, bugs escape, vplan grows" loop** — this is the day-to-day rhythm of IP Functional Val. Speak it.
4. **One sentence to lead with:**
   > *"I've built a structured CPU architecture + power management + PCIe foundation and a post-Si debug methodology; I want to apply it where the coverage leverage is highest — at the IP boundary."*

---

## 5. Default-Answer Shape (any technical Q)

> "First layer: **[what it is]**. It exists because **[constraint / tradeoff]**. Mechanism: **[implementation]**. Adjacent failure mode / validation hook: **[bug class or counter]**."

Four sentences, then stop. Let them drill.

### Coding pre-amble (before first line)

*Cerebras + Amazon both lost on edge-case / termination slips. Before writing code, say out loud:*

1. **Edge cases** I'll handle: empty / single / max / wrap-around / null — pick the 2-3 that apply.
2. **Loop invariant**: one sentence — what's true at the top of every iteration.
3. **Termination condition**: exactly what stops the loop, and why that's right for the edge cases above.

*Full drill + anchor bugs (digital-root `while (sum % 10)` slip, moving-avg wrap-around) in [Day 00 — Pre-amble discipline](day00_c_basics.md).*

### Behavioural questions (90-sec STAR)

*Context → Symptom → Repro → Root cause → Fix + impact → Lesson / coverage added.* End on the lesson, not the fix.

### HW-fluency framing (when role reads HW-heavy)

*Trigger: JD mentions board bring-up, lab debug, full-Si val, scope/LA, SI/PI, thermal chamber. Amazon Lab126 Echo 26 May closed on this gap — CV reads architecture/system-val, role wanted board/lab.*

Lead-in: *"My HW-exposure anchors are L&T board bring-up (PCIe/NVMe, scope + LA + protocol analyser, SI eye margin, rail droop) and Intel post-Si PM lab (turbostat / SoCWatch / MSR sweeps, thermal chamber, SVTools, PCU blocking-reason traces)."*

Bug anchors to pull:
- **PCIe lane-polarity reversed on board** ([Day 21 Sec 7](day21_pcie_bringup_functional_val.md)) — board-level catch, spec-mandated polarity reversal coverage.
- **Core C6 wake-vector lost via CFC FIFO race** ([Day 17](day17_post_si_pm_validation.md)) — post-Si lab debug, scope + MSR + PCU trace correlation.
- **ADR opcode-ordering hang** ([Day 17](day17_post_si_pm_validation.md)) — post-Si signature, lab repro, opcode reorder fix.

### EE basics quick-refresh (60-sec each)

*2x-closed gap (Intel P&P Apr R2 + Intel Ireland NPU 21 May). Material is scattered across day files — this is the consolidated one-page recall sweep for NPU / HW-heavy loops.*

- **CMOS power** — dynamic = `α·C·V²·f`, static = leakage (`V·I_leak`, grows with temp + shrinking nodes). Why DVFS targets V first (quadratic), f second (linear). Why power gating exists (kill leakage entirely).
- **Setup / hold + clock skew** — setup = data stable before clock edge; hold = data stable after. Skew shifts the window; positive skew helps setup, hurts hold. Clock-tree synthesis (CTS) balances skew. *Anchor:* hold violations don't fix with slower clock — must add buffer delay.
- **Metastability + 2-FF synchroniser** — async signal sampled near clock edge → output undefined for MTBF window. Fix: two cascaded flops on the destination clock; first flop may go metastable, second sees a settled value. MTBF improves exponentially per stage. *Anchor:* [Day 14 — header-on-but-logic-stuck metastability storm](day14_clock_power_gating_upf.md).
- **CDC handshake patterns** — 2-FF sync for single-bit; req/ack 4-phase for multi-bit; gray-coded pointers for async FIFOs. Lint tool: SpyGlass-CDC. *Anchor:* [Day 13 Sec clock-gating ICG glitch protection](day13_cstates_pkgc_s0ix.md) — runt pulse on `clk & en` without level-latch.
- **SI — eye / jitter / crosstalk** — eye diagram = overlaid bit periods; eye opening = margin. Jitter = timing variance (random + deterministic); crosstalk = aggressor-victim coupling. Mitigation: equalisation (CTLE / DFE), FEC, layout spacing. *Anchor:* [Day 21 Sec 7](day21_pcie_bringup_functional_val.md) PCIe eye-margin + lane polarity.

**Reflex one-liner:** *"Dynamic = αCV²f, static = leakage; setup/hold = window around edge; 2-FF for sync; CDC handshake for multi-bit; eye opening = SI margin. Five concepts, 60 sec each."*

---

## 6. Pre-Call 5-Min Checklist

- [ ] Water, DND, notebook, pen.
- [ ] 30-sec career narrative said out loud once.
- [ ] One bug story rehearsed (matched to the role's lens — pre-Si vs post-Si vs platform).
- [ ] One coverage-plan / triage answer rehearsed.
- [ ] 5 questions for them on a sticky.

---

## What's Required (For the Day)

- A 90-min uninterrupted slot for the mock loop.
- Recording app for the self-tape sections.
- Whiteboard / iPad / paper for the diagrams.
- A timer.

## Tools (Recap)

| Use | Tool |
|---|---|
| Self-tape | Phone camera, Loom, or Zoom record-to-self |
| Whiteboard | Physical board or Excalidraw |
| Mock peer | Friend / ex-colleague / paid mock service |
| Coding | Plain editor + terminal; run on the [`coding/`](../coding/) repo |

---

## Exit Check

- [ ] One full timed 90-min mock loop completed and self-reviewed.
- [ ] 5 questions for interviewer written down.
- [ ] All 4 bug stories told in 90 sec each, self-recorded.
- [ ] Flashcards run as a 1-minute drill across all sections.
- [ ] One-sentence opener memorised.
- [ ] You can name the right tool for any pre-Si or post-Si question without hesitating.

---

## Plan Complete

You've covered the full arc:

- **Phase 0 (D0-D4):** C / pointers / bits / cache / address fluency.
- **Phase 1 (D5-D11):** CPU architecture + IP functional validation.
- **Phase 2 (D12-D17):** CPU power management, pre-Si and post-Si.
- **Phase 3 (D18-D22):** PCIe functional + PM, cross-IP intersection.
- **Wrap (D23-D24):** Current-role hand-off + retained bug story + final mock.

Stay on this map; don't add new study areas in the last 48 hours.

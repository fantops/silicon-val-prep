# Day 11 — Clocking & Reset Architecture

## Concept

Before you can reason about clock-domain *crossings*, you need to know where clock domains **come from** and how reset is **distributed**. This is the infrastructure layer: **PLL** clock generation, the **clock distribution tree** (CTS, skew, insertion delay), **dividers and glitchless clock muxes**, **clock gating**, and the **reset architecture** (POR, reset tree, sync-vs-async, **reset synchronizer**, reset sequencing). It's the substrate that [Day 12–14](day12_metastability_synchronizers.md) (CDC) operate on, and the very first thing brought up on real silicon.

## Why this day in the flow

This **opens Phase 2 (Clocking, Reset & CDC)**. A clock domain ([Day 12](day12_metastability_synchronizers.md)) is "the set of flops on one clock" — but *why* are there multiple clocks, and how does reset reach every flop cleanly? That's today. It reuses [Day 7](day07_sequential_logic.md) (clock tree/skew as a first-class concern) and [Day 8](day08_timing_sta.md) (skew/uncertainty in STA), it **previews the reset synchronizer** that [Day 13](day13_cdc_techniques.md) details, and it feeds **clock/power gating** ([Day 28](day28_clock_power_gating_upf.md)) and **PM** ([Day 26](day26_dvfs_pstates_pvt.md)). On silicon, **clock+reset is the first rung of the bring-up ladder** ([Day 23](day23_cpu_bringup_functional_val.md)): rails → clocks valid → reset released → first instruction.

**Why it's new:** "how does a PLL work," "what's clock skew / a glitchless mux," "sync vs async reset and how do you release it safely" are core EE/clocking fundamentals the plan didn't cover as a first-class day.

---

## Deep Study Notes

### 1. Clock generation — the PLL

```
ref ──►[ PFD ]──►[ charge pump + loop filter ]──►[ VCO ]──┬──► clk_out (= ref × M/N)
         ▲                                                 │
         └────────────────[ ÷ feedback ]◄─────────────────┘
```

A **phase-locked loop** multiplies a slow, clean reference (crystal) up to a fast on-chip clock. The **phase-frequency detector** compares ref vs the divided feedback, the **charge pump + loop filter** turn phase error into a control voltage, and the **VCO** generates the output; the feedback divider sets the multiply ratio (`clk = ref × M/N`). Key facts: **lock time** (the PLL isn't usable until locked — using it early = garbage clock), **jitter** (VCO noise → the clock-uncertainty budget in [Day 8](day08_timing_sta.md)), and **PVT sensitivity**. A **DLL** (delay-locked loop) aligns phase without a VCO (no frequency multiply). **Spread-spectrum** modulates the clock slightly to spread EMI energy.

### 2. Clock distribution — the tree, CTS, skew

A clock must reach **millions of flops** at nearly the same instant. **Clock Tree Synthesis (CTS)** builds a balanced buffer tree (H-tree / mesh) to minimize **skew** (edge arrival difference between flops) and control **insertion delay** (root→leaf latency).

- **Low skew** is the goal: skew directly trades setup vs hold ([Day 7](day07_sequential_logic.md)/[Day 8](day08_timing_sta.md)). Unbalanced tree → hold violations.
- **Insertion delay** matters for I/O timing and cross-domain alignment.
- The clock tree is one of the **highest-power nets** on the chip (always toggling) → motivates clock gating (§5).

### 3. Dividers and the glitchless clock mux

- **Dividers (÷N):** even N is a toggle-flop chain; **odd N with 50% duty** needs both edges. Used to derive slower domain clocks from one PLL.
- **Glitchless clock mux** — switching a flop's clock source between two *running, asynchronous* clocks without producing a **runt pulse** that would corrupt every downstream flop:

```
Each clock's enable is gated by the OTHER clock being de-selected AND low,
synchronized into that clock's domain → the old clock fully stops (at a clean
low) before the new clock starts. A naive 2:1 mux on clocks produces runts.
```

This is a small synchronizer-based FSM — **never mux clocks with a plain combinational mux.** Glitchless clock muxing is a classic interview/whiteboard ask.

### 4. Why multiple clock domains exist

Different IPs run at different rates (CPU 3 GHz, DDR PHY, PCIe 8/16 GT/s, USB, audio 12.288 MHz...). Each is its own **clock domain**; every signal that leaves one domain and is sampled in another is a **CDC** ([Day 12–14](day12_metastability_synchronizers.md)). So clocking architecture *creates* the CDC problem — you can't reason about crossings without knowing the domain map.

### 5. Clock gating (recap, full treatment Day 28)

To save dynamic power ($P = \alpha C V^2 f$), idle clock branches are **gated off** with an **Integrated Clock Gating (ICG)** cell — a latch-based AND that suppresses the clock **without glitching** (the latch holds the enable stable across the clock high phase). Never gate a clock with a plain AND gate (glitch → false edge, [Day 5](day05_boolean_combinational_logic.md) §9). Clock gating is the single biggest dynamic-power lever and a major PM validation surface ([Day 28](day28_clock_power_gating_upf.md)).

### 6. Reset architecture — sources, tree, distribution

- **Sources:** power-on reset (**POR** — holds the chip in reset until rails/PLL are stable), watchdog reset, software reset, debug reset, security reset. A reset controller arbitrates them.
- **Reset tree:** like the clock tree, reset fans out to every flop; it must arrive cleanly and be **balanced** enough that release happens coherently.
- **POR sequencing:** rails good → PLL locks → POR releases → blocks come out of reset in order.

### 7. Sync vs async reset, and the reset synchronizer (preview Day 13)

| | **Sync reset** | **Async reset** |
|---|---|---|
| Effect | sampled on clock edge | immediate (works with clock stopped) |
| Risk | needs a running clock to reset | **release near an edge → metastability** |

The standard structure is **asynchronous assert, synchronous de-assert**: assert immediately (works even before the clock is alive — important for power-up/gating), but **de-assert through a 2-flop reset synchronizer** on the destination clock so the **release** is aligned and can't bring flops out of reset metastable ([Day 12](day12_metastability_synchronizers.md) physics). **One reset synchronizer per clock domain.**

### 8. Reset sequencing + reset domain crossing (preview)

- **Sequencing:** blocks must leave reset in a defined order (e.g., a block's clock and configuration must be live *before* it's released, or it samples garbage). Wrong order = a block released before its clock/config → undefined state. This is a real bring-up bug ([Day 23](day23_cpu_bringup_functional_val.md)).
- **Reset Domain Crossing (RDC):** a signal launched from a flop in one reset domain and captured in another — if the launch domain resets while the capture domain is live, you get corruption even on a shared clock. Detected by RDC analysis ([Day 13](day13_cdc_techniques.md)).

### 9. Where clocking/reset lives in validation

On silicon this is **rung 1 of bring-up** ([Day 23](day23_cpu_bringup_functional_val.md)): probe the **rails**, confirm the **PLL locked**, confirm **reset released** in order, then watch the PC leave the boot vector. Clock/reset bugs are catastrophic and early — a dead PLL, an unbalanced clock tree (hold fails), a glitchy clock mux, or a bad reset sequence stops the chip before any functional test runs. Pre-Si, these are checked with low-power/CDC/RDC tools + STA on the clock tree.

---

## Bugs in this Topic

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Clock used before PLL lock** | garbage/no clock at start | no lock-wait | gate clock until `lock` asserted |
| **Glitchy clock mux** | runt pulse corrupts a domain | combinational clock mux | glitchless (synchronized) mux |
| **Unbalanced clock tree** | hold failures | excess skew | CTS balancing / useful skew |
| **Async-reset removal** | random post-reset state | de-assert near edge | reset synchronizer (sync de-assert) |
| **Reset sequence wrong** | block samples garbage | released before clock/config | ordered reset sequencing |
| **RDC corruption** | spurious value when one domain resets | reset-domain mismatch | RDC analysis + isolation ([Day 13](day13_cdc_techniques.md)) |

---

## Interview Explanation (60–90 sec)

> "Clocking and reset is the infrastructure CDC sits on. A PLL multiplies a clean crystal reference up to the on-chip frequency: a phase detector and charge pump drive a VCO, and a feedback divider sets the ratio — and the two things I never forget are that you can't use the clock until it's locked, and its jitter feeds the timing-uncertainty budget. That one clock is distributed by a synthesized, balanced tree whose job is to minimize skew, because skew trades setup against hold directly. When I need to switch a flop between two running asynchronous clocks I use a glitchless mux that fully stops the old clock at a clean low before starting the new one — never a plain combinational mux, which makes a runt pulse. Multiple IPs run at different rates, so there are multiple clock domains, and every signal between them is a crossing — that's why clocking architecture creates the CDC problem. On reset, the rule is asynchronous assert, synchronous de-assert: assert immediately so it works even before the clock is alive, but release through a two-flop reset synchronizer per domain so the de-assertion can't bring flops out of reset metastable. And reset has to be sequenced — a block released before its clock or configuration samples garbage — which is exactly the kind of bug you hit on the first rung of silicon bring-up, right after confirming rails, PLL lock, and reset release."

---

## Architect Drills (derive, no answer key)

1. Sketch a PLL and explain lock, jitter, and the multiply ratio. What breaks if you use the clock before lock?
2. Why does clock skew trade setup against hold? Tie the direction to [Day 8](day08_timing_sta.md)'s equations.
3. Design a glitchless 2:1 clock mux between two async clocks. Show where the runt pulse comes from in a naive mux.
4. Build an odd-N (÷3, 50% duty) divider. Why does it need both clock edges?
5. Draw the async-assert/sync-de-assert reset synchronizer; explain why assert is async but de-assert is synchronous.
6. Three IPs must leave reset in order (clock → config → enable). Show the failure if enable releases first ([Day 23](day23_cpu_bringup_functional_val.md)).
7. Why is the clock tree a top power consumer, and how does ICG gating fix it without glitching ([Day 28](day28_clock_power_gating_upf.md))?

---

## Coding A — `d11_clock_reset`

Model clocking/reset control in C: a **glitchless clock-mux FSM** (`clk_mux_step`) that switches between two simulated clocks and **asserts no runt pulse** (old clock reaches a clean low before the new one starts); a **÷N divider**; and a **reset sequencer** (`reset_seq`) that releases N domains in a required order through per-domain 2-flop synchronizers, flagging an out-of-order release. Full spec in [coding/EXERCISES.md](../../coding/EXERCISES.md#day-11--clocking--reset-architecture).

**Interview prompts:** Where does the runt pulse come from in a naive clock mux? Why async-assert / sync-de-assert? What breaks if reset releases out of order?

---

## Bug Anchor / cross-links

- **Built from:** [Day 7](day07_sequential_logic.md) (clock tree, skew, CTS), [Day 8](day08_timing_sta.md) (skew/jitter → clock uncertainty), [Day 9](day09_fsm_sequence_detectors.md) (glitchless mux + reset-seq are small FSMs).
- **Feeds:** [Day 12](day12_metastability_synchronizers.md) (domains = where crossings happen; reset-release metastability), [Day 13](day13_cdc_techniques.md) (reset synchronizer + RDC, full treatment), [Day 28](day28_clock_power_gating_upf.md) (clock gating / ICG), [Day 23](day23_cpu_bringup_functional_val.md) (clock+reset = bring-up rung 1).

---

## Exit Check

- [ ] Explain a PLL (PFD → charge pump → VCO → feedback divider), lock, and jitter.
- [ ] Explain CTS and why skew trades setup vs hold.
- [ ] Design a glitchless clock mux and state where a naive mux's runt comes from.
- [ ] Justify async-assert / sync-de-assert and "one reset synchronizer per domain."
- [ ] Show a reset-sequencing failure (block released before clock/config).
- [ ] Define RDC and how it differs from CDC.
- [ ] Place clock+reset as rung 1 of the silicon bring-up ladder.

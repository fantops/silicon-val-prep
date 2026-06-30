# Day 10 — Verilog & SystemVerilog for Validation

## Concept

Verilog/SystemVerilog is the language you **read** to debug RTL and **write** to build testbenches. Master the **synthesis subset** (modules, procedural blocks, `logic`/4-state, blocking-vs-nonblocking) so you can trace a signal in Verdi and spot the classic RTL bugs, and the **verification subset** (classes, randomization, interfaces, clocking blocks, assertions) so you can drive a DUT. This is the language layer under everything in Phase 1, and the direct answer to Broadcom's **"read this Verilog / Verdi waveform"** ask.

## Why this day in the flow

This **caps Phase 1 (Digital Design)**. Days [5](day05_boolean_combinational_logic.md)/[7](day07_sequential_logic.md)/[9](day09_fsm_sequence_detectors.md) used Verilog snippets to express combinational logic, flip-flops, and FSMs; today is the **language itself** — its semantics, its synthesis vs simulation split, and the gotchas (inferred latch, blocking/nonblocking race, X-propagation) that cause real sim/synth mismatches. It is the prerequisite for **Phase 5 (Verification)** — the SystemVerilog testbench, SVA, and UVM of [Day 24](day24_verification_planning_coverage.md)/[Day 25](day25_testbench_assertions_debug.md) are all SV — and for every RTL-reading task at an IP-Val role.

**Why it's new:** the plan was systems-C-heavy; reading and writing HDL is a distinct, essential skill that Broadcom probed ("Verdi/Verilog reading") and Intel IP-Val requires daily.

---

## Deep Study Notes

### 1. Verilog vs SystemVerilog

**Verilog** is the classic HDL (design). **SystemVerilog** is a superset adding both **design** conveniences (`logic`, `always_ff/comb/latch`, `enum`, interfaces) and a full **verification** language (classes/OOP, constrained randomization, assertions, coverage). RTL is written in the SV synthesis subset; testbenches use the SV verification subset.

### 2. Data types — and the 4-state trap

| Type | States | Use |
|---|---|---|
| `wire`/`net` (V) | 4 (0,1,X,Z) | structural connections, continuous `assign` |
| `reg` (V) | 4 | procedural assignment (misnamed — not always a register) |
| `logic` (SV) | 4 | **the default** — replaces `reg`/`wire` for single-driver signals |
| `bit` (SV) | 2 (0,1) | testbench counters, no X/Z |

**`X` = unknown**, **`Z` = high-impedance** (tri-state/floating). `X` is the killer: uninitialized flops, multiple drivers, and incomplete logic produce `X`, which **propagates** through logic (`X & 1 = X`) and shows up as red in the waveform. **X-propagation chasing** — tracing an `X` back to its origin (uninit reg, missing reset, bus contention) — is a core RTL debug skill. `Z` appears on buses; two drivers fighting = `X` (contention).

### 3. Procedural blocks (and SV intent keywords)

```systemverilog
always_comb        y = a & b;            // combinational — full sensitivity, must assign all paths
always_ff @(posedge clk or negedge rst_n) // sequential — registered
  if (!rst_n) q <= '0; else q <= d;
always_latch       if (en) q = d;        // intentional latch (rare)
assign w = sel ? a : b;                  // continuous (combinational net)
initial / final                          // simulation-only (testbench)
```

The SV intent keywords (`always_comb`/`always_ff`/`always_latch`) let the tool **error-check your intent** — `always_comb` warns if you accidentally infer a latch; `always_ff` warns on a bad reset. Use them in RTL.

### 4. Blocking vs nonblocking — THE rule

```systemverilog
always_ff @(posedge clk) begin q1 <= d; q2 <= q1; end  // <= : two flops, shift (CORRECT seq)
always_ff @(posedge clk) begin q1  = d; q2  = q1; end  //  = : ONE flop, q2 gets new d (BUG)
always_comb            y = a & b;                       //  = for combinational (CORRECT)
```

- **`<=` (nonblocking)** for **sequential** logic — all RHS evaluated, then all LHS updated together (models parallel flops).
- **`=` (blocking)** for **combinational** logic — sequential, immediate.
- Mixing them, or using `=` in a clocked block, creates **simulation/synthesis mismatch** and **race conditions** (the order of evaluation becomes undefined). This is the #1 RTL bug after inferred latches.

### 5. Synthesis subset vs simulation-only

Synthesizable RTL is a **subset**: no `#delay`, no `initial` (except ROM init), no `$display`, no unbounded loops, no real-number types. The simulator accepts much more than the synthesizer — **"it simulates" ≠ "it synthesizes."** Validation must distinguish "this works in sim because of a sim-only construct" from "this is real hardware."

### 6. The SystemVerilog verification subset (bridge to UVM)

| Feature | Purpose |
|---|---|
| **classes / OOP** | transactions, components (driver/monitor/scoreboard — [Day 25](day25_testbench_assertions_debug.md)) |
| **`rand` + `constraint`** | constrained-random stimulus ([Day 24](day24_verification_planning_coverage.md)) |
| **mailbox / semaphore** | inter-component communication, resource arbitration |
| **interface + clocking block** | bundle signals; **clocking block** defines *when* the TB samples/drives (avoids races at the DUT boundary) |
| **`covergroup`** | functional coverage ([Day 24](day24_verification_planning_coverage.md)) |
| **assertions (SVA)** | temporal properties ([Day 25](day25_testbench_assertions_debug.md)) |

The **clocking block** is the subtle one: it pins TB drive/sample to a defined edge+skew so the testbench doesn't race the DUT's flops — the SV answer to the same metastability/timing concerns from [Day 7](day07_sequential_logic.md).

### 7. Reading RTL in Verdi (the debug skill)

Given an unfamiliar block and a bad signal:
1. **Trace drivers/loads** — who writes this net, who reads it (Verdi's "drivers/loads" or active annotation).
2. **Walk the cone** back from the bad value to its source (the schematic/`nTrace`).
3. **Find X-origin** — annotate the waveform; `X` traces back to uninit/contention/missing-reset.
4. **FSM view** — Verdi reconstructs the state machine; see which state it's stuck in (the [Day 9](day09_fsm_sequence_detectors.md) / LTSSM debug, [Day 36](day36_pcie_bringup_functional_val.md)).
5. **Cross-probe** RTL ↔ waveform ↔ schematic. Reading RTL fast = knowing these views, not memorizing syntax.

### 8. The RTL bug taxonomy (what you're hunting)

- **Inferred latch** — incomplete `case`/`if` in combinational logic ([Day 5](day05_boolean_combinational_logic.md)). `always_comb` catches it.
- **Blocking/nonblocking race** — `=` in clocked logic → sim/synth mismatch (§4).
- **Multiple drivers** — two `always`/`assign` writing one net → `X` contention.
- **Width mismatch / truncation** — assigning wider to narrower silently drops bits.
- **Sensitivity-list miss** (Verilog `always @(a)` forgetting `b`) — fixed by `always_comb`/`@(*)`.
- **X-propagation** — uninitialized/un-reset state poisons downstream logic.

---

## Bugs in this Topic

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Inferred latch** | sim/synth mismatch, X | incomplete `always_comb` | full case + default; `always_comb` |
| **Blocking in clocked block** | shift collapses to 1 flop, race | `=` where `<=` needed | `<=` for sequential |
| **Multiple drivers** | `X` on the net | two drivers contending | single driver / proper tri-state |
| **Truncation** | high bits silently lost | width mismatch | match widths; lint |
| **Uninit/no-reset reg** | persistent `X` | missing reset / init | reset all state |
| **Sim-only construct in RTL** | "works in sim, not silicon" | `#delay`/`initial` relied on | synthesizable subset only |

---

## Interview Explanation (60–90 sec)

> "Verilog is the design HDL; SystemVerilog is the superset that adds the verification language — classes, randomization, assertions, coverage. When I read RTL the things I watch first are the four-state values: X is unknown and Z is high-impedance, and X propagates, so a red waveform usually traces back to an uninitialized flop, a missing reset, or two drivers contending on one net. The rule I never break is nonblocking assignment for sequential logic and blocking for combinational — using blocking in a clocked block collapses a shift register to one flop and creates a sim-versus-synth mismatch, which is the classic bug right after inferred latches from an incomplete case. I use the intent keywords — always_ff, always_comb — because the tool then flags an accidental latch or a bad reset for me. And I remember that the simulator accepts far more than the synthesizer: delays and initial blocks simulate but aren't hardware, so 'it simulates' doesn't mean 'it synthesizes.' For debug I work in Verdi by tracing drivers and loads back through the logic cone to the X origin, and using the reconstructed FSM view to see which state a machine is wedged in — which is exactly how I'd debug an LTSSM stuck in Polling. On the testbench side, the clocking block is the piece that pins when I sample and drive so the testbench doesn't race the DUT's flops."

---

## Architect Drills (read/derive, no answer key)

1. Here's a 3-line clocked `always` block using `=`; what hardware does it synthesize vs what the author intended? Fix it.
2. A net is `X` from cycle 0. List the four most likely causes and how you'd confirm each in Verdi.
3. Write a 2-block FSM ([Day 9](day09_fsm_sequence_detectors.md)) in SV using `always_ff` + `always_comb`; show why the intent keywords prevent the latch and reset bugs.
4. Why does a blocking-assignment shift register collapse to one flop? Walk the evaluation order.
5. What does a clocking block buy you at the DUT boundary, and which [Day 7](day07_sequential_logic.md) hazard does it dodge?
6. Two `assign`s drive the same wire with different expressions — what value, and how does it show in the waveform?
7. Give three constructs that simulate but don't synthesize, and how each masks a real hardware question.

---

## Coding A — `d10_rtl_semantics`

Model Verilog semantics in C: a **4-state logic type** (`0/1/X/Z`) with AND/OR/NOT/XOR truth tables showing **X-propagation** and **Z-contention → X**; and a tiny **update-scheduler** that demonstrates **blocking vs nonblocking** on a 2-stage shift register (nonblocking shifts correctly, blocking collapses to one flop). Add an `infer_latch()` checker that flags an incomplete case-to-output mapping. Full spec in [coding/EXERCISES.md](../coding/EXERCISES.md#day-10--verilog--systemverilog).

**Interview prompts:** Why does blocking collapse the shift register? What produces X, and how does it propagate? `always_comb` vs `always @(*)`?

---

## Bug Anchor / cross-links

- **Built from:** [Day 5](day05_boolean_combinational_logic.md) (inferred latch, combinational coding), [Day 7](day07_sequential_logic.md) (nonblocking-for-seq, clock boundary), [Day 9](day09_fsm_sequence_detectors.md) (FSM coding styles).
- **Feeds:** [Day 24](day24_verification_planning_coverage.md) (SV covergroups/constraints), [Day 25](day25_testbench_assertions_debug.md) (SV testbench, SVA, UVM), [Day 36](day36_pcie_bringup_functional_val.md) (Verdi FSM debug of a stuck LTSSM).
- **Broadcom anchor:** R1 "Verdi/Verilog reading" maps directly here.

---

## Exit Check

- [ ] Distinguish `logic`/`bit`, 4-state vs 2-state, and explain `X` and `Z`.
- [ ] State the nonblocking-seq / blocking-comb rule and the bug each violation causes.
- [ ] Explain why `always_comb`/`always_ff` intent keywords catch latch/reset bugs.
- [ ] Trace an `X` to its likely origins; describe the Verdi drivers/loads + FSM views.
- [ ] Name three sim-only constructs and why "simulates ≠ synthesizes."
- [ ] Explain what a clocking block does at the DUT boundary.
- [ ] Spot inferred-latch, blocking-in-clocked, and multiple-driver bugs in a snippet.

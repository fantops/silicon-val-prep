# Day 8 — Timing & STA

## Concept

**Static Timing Analysis** is how you prove a chip meets timing on **every path, at every corner, without simulation** — vectorless and exhaustive, where dynamic sim can only check the paths your stimulus happens to hit. [Day 7](day07_sequential_logic.md) gave the per-flop **contract** (setup/hold/Tcq); today is the **chip-wide methodology**: the four path types, **slack**, **clock skew/jitter/OCV**, **timing exceptions** (multicycle/false/CDC), and the **timing-closure loop**. This is the EE-fundamentals depth the Broadcom/Intel-class loops probe.

## Why this day in the flow

[Day 7](day07_sequential_logic.md) defined setup/hold for one launch→capture pair; STA applies that contract across the **whole netlist** under all PVT corners. The **adder/multiplier critical path** from [Day 6](day06_arithmetic_circuits.md) is precisely what STA measures; **pipelining** ([Day 21](day21_pipeline_microarch.md)) is how you fix a setup violation. It also underpins everything physical downstream: **clock-tree/skew** ([Day 11](day11_clocking_reset_architecture.md)), **CDC false paths** ([Day 12](day12_metastability_synchronizers.md)–[Day 14](day14_async_fifo_cdc_verification.md)), and **voltage/frequency corners** in DVFS/PM ([Day 26](day26_dvfs_pstates_pvt.md)). STA sign-off is a core pre-Si gate ([Day 24](day24_verification_planning_coverage.md)/[Day 30](day30_pre_si_pm_validation.md)).

**Why it's new:** "setup vs hold," "what's slack," "false vs multicycle path," and "which corner catches hold" are exactly the EE questions flagged across the interview log (Intel P&P, Broadcom R2). This is the dedicated fix.

---

## Deep Study Notes

### 1. What STA is — and why it beats dynamic timing

| | **STA (static)** | **Dynamic timing sim** |
|---|---|---|
| Coverage | **every path**, exhaustively | only paths the stimulus exercises |
| Vectors | none needed | needs stimulus |
| Speed | fast (graph analysis) | slow |
| Misses | false paths flagged as violations (needs exceptions) | untested paths entirely |

STA models the design as a **timing graph** and, for each path, compares **arrival time** (when the signal actually gets there) against **required time** (when it must). The difference is **slack**. It's the signoff gate because it can't *miss* a path — but it will flag **false paths** as violations unless you tell it they're exceptions (§6).

### 2. The four timing paths

```
   ┌─ in→reg ─┐            ┌─ reg→reg ─┐         ┌─ reg→out ─┐
input ──►[FF]──comb──►[FF]──comb──►[FF]──comb──► output
   └ set_input_delay         (internal)         set_output_delay ┘
   in→out: pure combinational input→output (rare; constrained by max_delay)
```

Every path STA checks is one of: **reg→reg** (internal, the usual `fmax` driver), **in→reg** (constrained by `set_input_delay`), **reg→out** (`set_output_delay`), **in→out** (combinational feed-through). You must constrain the I/O paths or STA can't judge them.

### 3. Slack — arrival vs required (recap + scale)

For a **setup** check on a reg→reg path:
```
arrival  = T_launch_clk + Tcq + Tcomb(max)
required = T_capture_clk + Tclk − Tsu − clock_uncertainty
setup slack = required − arrival   (≥ 0 to pass)
```
For **hold** (same edge): `hold slack = (Tcq + Tcomb(min)) − (Th + skew/uncertainty)` — **`Tclk` absent**, so a hold fail isn't fixed by slowing the clock ([Day 7](day07_sequential_logic.md)). Across the design STA reports **WNS** (worst negative slack) and **TNS** (total negative slack, sum of all violating paths) — WNS tells you the worst path, TNS tells you how much work remains.

### 4. Clock skew, jitter, uncertainty

- **Skew** = the same clock edge reaches launch vs capture flops at different times. Skew that **delays the capture clock helps setup but hurts hold** (and vice versa) — so **useful skew** is a deliberate knob to steal time for a slow path (at the cost of hold margin elsewhere).
- **Jitter** = cycle-to-cycle clock period variation (PLL noise) → eats the setup budget.
- STA lumps skew + jitter + margin into **clock uncertainty**, subtracted from every setup check (and added to hold). Budgeting uncertainty too tight = silicon fails; too loose = you leave frequency on the table.

### 5. OCV / PVT corners and MCMM

Transistors vary with **Process, Voltage, Temperature**. STA signs off across **corners**:

```
slow corner (SS, low V, hot)   → worst SETUP (logic is slow)   → check setup here
fast corner (FF, high V, cold) → worst HOLD  (logic is fast)   → check hold here
```

**On-Chip Variation (OCV)** derates launch vs capture paths differently (one path slow, the other fast) to be conservative; **AOCV/POCV** refine this statistically. **MCMM** (multi-corner multi-mode) signs off every corner × every functional mode (e.g., turbo vs low-power [Day 26](day26_dvfs_pstates_pvt.md)). The rule to recite: **setup is a slow-corner problem, hold is a fast-corner problem.**

### 6. Timing exceptions — don't over-constrain

STA assumes **single-cycle** by default. Real designs have paths that aren't:

| Exception | Meaning | SDC |
|---|---|---|
| **Multicycle path** | data has ≥2 cycles to settle (slow datapath, enable every N) | `set_multicycle_path N` |
| **False path** | logically impossible / never-sampled path | `set_false_path` |
| **CDC path** | crosses async clock domains → synchronized, not timed normally | `set_clock_groups -asynchronous` / `set_false_path` ([Day 12](day12_metastability_synchronizers.md)) |
| **Max/min delay** | point-to-point bound (in→out, I/O) | `set_max_delay` / `set_min_delay` |

**The danger both ways:** *missing* an exception → STA reports false violations and you waste effort or over-design; *wrong* exception (false-pathing a real path) → a true violation escapes to silicon. CDC paths **must** be declared async or STA tries to time an unsynchronizable crossing.

### 7. Constraints (SDC) are as important as the netlist

```tcl
create_clock -period 1.0 [get_ports clk]        ;# 1 GHz
set_input_delay  0.3 -clock clk [all_inputs]
set_output_delay 0.2 -clock clk [all_outputs]
set_clock_uncertainty 0.05 [get_clocks clk]
set_clock_groups -asynchronous -group {clkA} -group {clkB}   ;# CDC
```

A perfect netlist with wrong constraints signs off a broken chip. **Garbage constraints = garbage signoff** — reviewing the SDC is a first-class validation task.

### 8. The timing-closure loop

```
synth/P&R → report_timing → look at WNS path
   ├─ setup fail → shorten Tcomb (restructure logic, resize cells,
   │               pipeline [Day 21]), or steal time with useful skew, or slow clk
   └─ hold fail  → add delay (buffers) on the short path, fix skew  (NOT slower clk)
→ re-run → repeat until WNS ≥ 0 at all corners
```

**Order matters:** close **setup** first (it can shift with placement/clock), then **fix hold** (local buffer insertion) — fixing hold first then breaking it again is wasted work. Sign-off = **WNS ≥ 0 across all MCMM corners**, all exceptions reviewed.

---

## Bugs in this Topic

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Unconstrained CDC path** | false setup violations / or a real crossing timed | no `set_clock_groups`/false-path | declare async ([Day 12](day12_metastability_synchronizers.md)) |
| **False-pathed real path** | silicon fails, STA was clean | over-aggressive exception | remove the wrong exception |
| **Hold fail at fast corner** | works slow, fails fast/cold | hold not checked at FF corner | MCMM hold signoff + buffer fix |
| **Missing multicycle** | wasted area chasing a non-violation | path constrained single-cycle | `set_multicycle_path` |
| **Under-budgeted uncertainty** | passes STA, fails silicon | skew/jitter margin too small | realistic clock uncertainty |
| **Missing I/O delay** | I/O path unchecked | no `set_input/output_delay` | constrain all I/O |

---

## Interview Explanation (60–90 sec)

> "Static timing analysis proves the chip meets timing on every path without vectors — it builds a timing graph and, for each path, compares arrival time against required time; the difference is slack, and the design passes when worst-negative-slack is non-negative at every corner. It checks four path types — register-to-register, input-to-register, register-to-output, and combinational feed-through — so I have to constrain the I/O or STA can't judge them. The setup check has the clock period in it, so a setup failure is fixed by slowing down, restructuring logic, or pipelining; the hold check is same-edge and the period doesn't appear, so hold is a fast-path race you fix with buffer delay or skew, never a slower clock. Because transistors vary, I sign off across PVT corners with the rule that setup is worst at the slow corner and hold is worst at the fast corner, and on-chip variation derates launch and capture paths conservatively. The subtle part is exceptions: multicycle paths get extra cycles, false paths are never sampled, and CDC crossings must be declared asynchronous so STA doesn't try to time an unsynchronizable path — but if I false-path a real path, a true violation escapes to silicon, so the constraints are as safety-critical as the netlist. Closure is a loop: read the worst path, fix setup first by restructuring or pipelining, then fix hold locally, and repeat across all corners."

---

## Architect Drills (derive, no answer key)

1. Write the setup-slack and hold-slack equations with arrival/required and clock uncertainty. Show why the period is absent from hold.
2. A path fails setup by 80 ps at 1 GHz. Give four distinct fixes and the cost of each (tie to [Day 6](day06_arithmetic_circuits.md)/[Day 21](day21_pipeline_microarch.md)).
3. Why is hold checked at the fast corner and setup at the slow corner? What is OCV protecting against?
4. You see thousands of setup violations on paths between two unrelated clocks. Diagnose and write the one SDC line that fixes it ([Day 12](day12_metastability_synchronizers.md)).
5. A datapath is enabled every 4th cycle but STA flags it. Which exception, and what's the risk of getting the multiplier wrong?
6. Useful skew steals 30 ps for a slow path — what did it cost, and where?
7. The netlist is perfect but silicon fails timing. Name three constraint mistakes that cause this.

---

## Coding A — `d08_sta_paths`

Build a **multi-path STA slack calculator in C**: given a list of paths (`launch_clk, capture_clk, period, Tcq, Tcomb_min/max, Tsu, Th, skew, uncertainty`), compute **setup and hold slack** per path, report **WNS/TNS**, classify each violation (setup→"slow/pipeline", hold→"add delay") and **skip paths flagged as false/async** (an exceptions list). Show a CDC path generating false violations until it's added to the async-group exception. Full spec in [coding/EXERCISES.md](../coding/EXERCISES.md#day-8--timing--sta).

**Interview prompts:** Why no `Tclk` in hold? Which corner for hold vs setup? What happens if you false-path a real path?

---

## Bug Anchor / cross-links

- **Built from:** [Day 7](day07_sequential_logic.md) (the per-flop setup/hold/Tcq contract), [Day 6](day06_arithmetic_circuits.md) (the adder/multiplier critical path STA measures), [Day 5](day05_boolean_combinational_logic.md) (`Tcomb` = combinational depth).
- **Feeds:** [Day 11](day11_clocking_reset_architecture.md) (clock tree/skew/CTS), [Day 12](day12_metastability_synchronizers.md)–[Day 14](day14_async_fifo_cdc_verification.md) (CDC false-path exceptions), [Day 21](day21_pipeline_microarch.md) (pipelining to close setup), [Day 26](day26_dvfs_pstates_pvt.md) (per-corner/per-voltage timing), [Day 24](day24_verification_planning_coverage.md)/[Day 30](day30_pre_si_pm_validation.md) (STA signoff gate).

---

## Exit Check

- [ ] State why STA beats dynamic timing (exhaustive, vectorless) and what it can miss (false paths).
- [ ] Name the four path types and which need I/O constraints.
- [ ] Write setup/hold slack from arrival/required; explain WNS vs TNS.
- [ ] Recite "setup = slow corner, hold = fast corner" and what OCV derates.
- [ ] Distinguish multicycle / false / CDC exceptions and the danger of each direction.
- [ ] Explain why the SDC is as safety-critical as the netlist.
- [ ] Walk the closure loop: setup first (restructure/pipeline/skew), then hold (buffers).

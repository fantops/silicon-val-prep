# Day 7 — Sequential Logic + Flip-Flops

## Concept

Sequential logic adds the one thing combinational logic lacks: **memory**. A clocked storage element (flip-flop) captures a value on a clock edge and holds it. The price of memory is a **timing contract** — **setup**, **hold**, **clock-to-Q** — and that contract is what sets a chip's maximum frequency and what *breaks* when a signal crosses clock domains. Master the contract, derive $f_{max}$ from it, and know the sequential building blocks (registers, shift registers, counters, LFSRs).

## Why this day in the flow

[Day 5](day05_boolean_combinational_logic.md) gave memoryless logic. Today adds the **register** that stores state — and together they *are* a finite-state machine: combinational next-state logic (Day 5) + a state register (today) = [Day 9](day09_fsm_sequence_detectors.md). The **timing contract** introduced here is the physical root of [Day 12](day12_metastability_synchronizers.md) (metastability = what a flop does when setup/hold is violated) and therefore of all of [Day 13–14](day13_cdc_techniques.md) (CDC); its full chip-level treatment — slack budgeting, skew, closure — is the very next day, [Day 8 (Timing & STA)](day08_timing_sta.md). The same flops reappear all over the plan: as the pipeline latches in [Day 21](day21_pipeline_microarch.md), the retention flops in [Day 28](day28_clock_power_gating_upf.md), and the FIFO pointers in the Broadcom depth problem.

**Broadcom R2 gap this closes:** setup/hold, latch-vs-flop, "why can't you fix a hold violation by slowing the clock," metastability physics (started here, finished Day 12).

---

## Deep Study Notes

### 1. Latch vs flip-flop — the level/edge distinction

| | **Latch** (level-sensitive) | **Flip-flop** (edge-triggered) |
|---|---|---|
| Captures | while enable is **high** (transparent) | only on the clock **edge** |
| Built from | one gating stage | two latches, master–slave |
| In sync design | **avoid** (inferred latches, time-borrowing, harder STA) | the standard storage element |
| Use on purpose | latch-based time borrowing, low-power | everywhere else |

A **D flip-flop** is two D-latches in series on opposite clock phases (**master–slave**): the master is transparent while clk=0 and samples the input; on the rising edge the master freezes and the slave becomes transparent, presenting the captured value. The net effect: the input is sampled at exactly one instant — the rising edge — and held for a full cycle. *Latches are transparent for half a cycle; flops are transparent for an instant.* That instant is what makes synchronous timing analyzable.

### 2. The timing contract — setup, hold, clock-to-Q

```
            ┌── setup ──┐┌── hold ──┐
 D ─────────┤  stable   ││  stable  ├──────────
            └───────────┘└──────────┘
                         ▲
 clk ___________________/ \________________
                         edge
            └─ Tsu ─┘    └─ Th ─┘
 Q ──────────────────────────╳════════  (changes Tcq after the edge)
                              └ Tcq ┘
```

- **Setup time `Tsu`** — D must be **stable for `Tsu` before** the edge. Violate → captured value undefined.
- **Hold time `Th`** — D must **stay stable for `Th` after** the edge. Violate → captured value undefined.
- **Clock-to-Q `Tcq`** — after the edge, Q takes `Tcq` to present the new value.

Setup + hold define an **aperture window** around the edge during which the data input must not move. Move inside it → the flop may go **metastable** (Day 12).

### 3. Max frequency — the setup equation (derive it)

For a path **launch flop → combinational logic → capture flop**, the data must arrive at the capture flop's D at least `Tsu` before the next edge:

$$T_{clk} \ge T_{cq} + T_{comb}^{max} + T_{su} + T_{skew}$$

$$\boxed{f_{max} = \frac{1}{T_{cq} + T_{comb}^{max} + T_{su} + T_{skew}}}$$

**Setup slack** = `Tclk − (Tcq + Tcomb_max + Tsu + Tskew)`. Negative slack = setup violation = the chip won't run at that frequency. **Fixes:** slow the clock, shorten the longest combinational path (pipeline it, simplify logic), reduce skew. *Setup failures are fixed by slowing down.*

### 4. The hold check — and why slowing the clock does NOTHING

Hold is checked at the **same edge** (data launched on edge *n* must not race through and corrupt the capture flop's *same* edge *n*):

$$T_{cq} + T_{comb}^{min} \ge T_{su}... \text{no} \;\Rightarrow\; T_{cq} + T_{comb}^{min} \ge T_{h} + T_{skew}$$

```
Hold slack = (Tcq + Tcomb_min) − (Th + Tskew)
```

**Notice: `Tclk` does not appear.** A hold violation is a **fast-path / clock-skew** problem — the new data arrives *too soon* and clobbers the value the capture flop was supposed to hold from this edge. **Slowing the clock cannot fix it** (the race is edge-relative, not period-relative). **Fixes:** *add* delay (buffers) on the short path, fix clock-tree skew, balance the clock. This is the single most-asked sequential-timing question — *"setup fails → slow down; hold fails → add delay, the clock period is irrelevant."*

### 5. Clock skew and jitter

- **Skew** = same edge arrives at different flops at different times (clock-tree imbalance). Skew that delays the *capture* clock **helps setup but hurts hold** (and vice versa) — hence **useful skew** as a deliberate tuning knob.
- **Jitter** = edge-to-edge variation in the clock period itself (PLL noise). Eats into the setup budget as guard-band.
- Both are why real STA carries margin and why the clock tree (CTS) is a first-class design problem.

> **This section is the per-path *contract*; the chip-wide *methodology* is the next day.** [Day 8 — Timing & STA](day08_timing_sta.md) takes setup/hold into full static timing analysis: multi-path slack budgeting, on-chip variation (OCV) and PVT corners, multicycle and false paths, CDC timing exceptions, and the timing-closure debug loop. Master the contract here; Day 8 is how you close it across a real netlist.

### 6. Metastability preview (full treatment Day 12)

When D moves inside the setup/hold aperture — unavoidable for a signal from **another clock domain** or a truly **asynchronous input** (a button, a reset release) — the flop's output can hang between 0 and 1 in a **metastable** state for an unbounded (but exponentially decaying) time before resolving to a random value. You can't prevent the *event*; you give it *time to resolve* with a **two-flop synchronizer**. That's the whole bridge into CDC. The lever is the resolution-time term in MTBF — Day 12.

### 7. The sequential building blocks

| Block | What it is | Use |
|---|---|---|
| **Register** | N flops, parallel load + enable | pipeline stage, CSR, datapath |
| **Shift register** | flops in a chain (SISO/SIPO/PISO/PIPO) | serial↔parallel (SerDes), delay line, LFSR core |
| **Binary counter** | register + incrementer | timers, address pointers |
| **Ring counter** | one-hot token circulating (N flops, N states) | simple sequencer, phase generator |
| **Johnson counter** | twisted ring (N flops, **2N** states) | low-glitch phase/clock divider |
| **Gray counter** | counts in Gray code (1 bit changes/step) | **CDC pointers** (Day 14), low-power |
| **LFSR** | shift register + XOR feedback taps | BIST patterns, CRC, scrambler, pseudo-random stim |

**Ripple vs synchronous counter:** a ripple counter clocks each stage off the previous flop's output → cheap but the count **ripples**, producing **decode glitches** and skew (bad for anything sampling the count). A **synchronous** counter clocks all flops together off the same clock → clean, the standard choice.

### 8. LFSR — pseudo-random in a few gates

A linear-feedback shift register XORs selected tap positions back into the input. With a **maximal-length polynomial**, an `n`-bit LFSR cycles through all `2ⁿ − 1` non-zero states before repeating — a cheap pseudo-random generator.

```
4-bit LFSR, taps at bits 4 and 3 (poly x⁴+x³+1):
  feedback = q[3] ^ q[2];          // (0-indexed: bits 3 and 2)
  q <= {q[2:0], feedback};         // shift left, feedback into LSB
  → 15-state max-length sequence (all values except 0000)
```

**The lockup trap:** the all-zeros state is a **fixed point** (XOR of zeros = 0 → stuck forever). Seed it non-zero, or add the extra logic for a "de Bruijn" all-states version. Uses: **BIST** (memory/logic self-test patterns), **CRC** (same polynomial generates and checks; remainder 0 ⇒ clean — Day 15), **scrambling** (DC balance on a SerDes line — [Day 33](day33_pcie_fundamentals_ltssm.md) PCS).

### 9. Reset — synchronous vs asynchronous

| | **Sync reset** | **Async reset** |
|---|---|---|
| Asserted | sampled on clock edge | takes effect immediately |
| Needs clock to reset | **yes** | no (resets even with clock stopped) |
| Risk | none special | **removal (de-assert) near an edge → metastability** |
| Common practice | — | **async assert, sync de-assert** (reset synchronizer, [Day 13](day13_cdc_techniques.md)) |

The async-reset **release** is itself a clock-domain-crossing event — if reset de-asserts inside a flop's setup/hold window, the flop can come out of reset metastable. That's why the **reset synchronizer** exists; today just plants the seed.

### 10. RTL coding discipline — the blocking/nonblocking race

```systemverilog
// Sequential: nonblocking <=, clocked
always_ff @(posedge clk or negedge rst_n)
  if (!rst_n) q <= 1'b0;
  else        q <= d;

// Combinational: blocking =, sensitivity *, full assignment
always_comb
  y = a & b;
```

**The rule:** **`<=` (nonblocking) for clocked/sequential**, **`=` (blocking) for combinational**. Mixing them — or reading a signal you wrote with `=` in the same sequential block — creates **simulation/synthesis mismatch** and read-before-write **races**. This is the sequential sibling of Day 5's inferred-latch bug: both come from sloppy `always` blocks.

---

## Bugs in this Topic

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Hold violation** | fails even at low freq | fast path / skew, data races the edge | add delay, fix skew — *not* slower clock |
| **Setup violation** | fails only at high freq | longest comb path too slow | pipeline / simplify / slow clock |
| **Ripple-counter decode glitch** | spurious one-cycle pulses | stages clock off each other, skew | synchronous counter |
| **LFSR lockup** | sequence stuck at 0 | all-zeros fixed point | non-zero seed / de-Bruijn |
| **Async-reset removal** | random post-reset state | de-assert in setup/hold window | reset synchronizer (Day 13) |
| **Blocking/nonblocking race** | sim≠synth, intermittent | `=` in clocked block / mixed | `<=` seq, `=` comb |

---

## Interview Explanation (60–90 sec)

> "A flip-flop is the unit of memory: it samples D at the clock edge and holds it, where a latch is transparent for a whole level and is best avoided in synchronous design. The cost of that memory is a timing contract — data must be stable a setup time before the edge and a hold time after, and the output appears a clock-to-Q later. Max frequency comes straight from the setup path: the clock period must cover clock-to-Q plus the worst-case combinational delay plus setup plus skew, so `fmax` is one over that sum. The key distinction interviewers probe is hold: a hold violation is a fast-path race, the period doesn't appear in the equation, so slowing the clock can't fix it — you add delay or fix skew. When data comes from another clock domain it *will* sometimes land in the setup/hold window and the flop goes metastable; you don't prevent that, you give it a cycle to resolve with a two-flop synchronizer. The building blocks on top of flops are registers, shift registers, counters — binary, ring, Johnson, Gray — and the LFSR, which is a shift register with XOR feedback that gives a cheap pseudo-random sequence for BIST, CRC, and scrambling, as long as you keep it out of the all-zeros lockup state."

---

## Architect Drills (derive, no answer key)

1. Given `Tcq=80ps, Tcomb_max=600ps, Tsu=50ps, Tskew=40ps`, find `fmax` and the setup slack at 1.2 GHz. Now `Tcomb_min=90ps, Th=60ps` — is hold met? If not, fix it without touching the clock.
2. Derive the D-flop from two D-latches. Why does master–slave make the input "sampled at an instant"?
3. Why does skew that delays the capture clock help setup but hurt hold? Sketch both equations and move `Tskew`.
4. Build a Gray counter; show only one bit changes per step. Why does that property matter for [Day 14](day14_async_fifo_cdc_verification.md)'s FIFO pointers?
5. A 4-bit ripple counter drives a comparator that fires a one-cycle strobe. You see double strobes. Walk the glitch; fix it.
6. Pick a maximal-length tap set for an 8-bit LFSR. Prove all-zeros is a fixed point and say two ways to avoid it.
7. Sync vs async reset: which can reset a clock-stopped block, and what new hazard does async reset introduce on *release*?

---

## Coding A — `d07_seq_timing`

Build a **sequential-primitives model in C**: a max-length `lfsr_step()` (parameterized taps, asserts it visits `2ⁿ−1` states and never 0), `gray_inc()` / `bin_to_gray()` / `gray_to_bin()` (assert single-bit change per step), a `johnson`/`ring` counter, and a **timing calculator** `fmax()` / `setup_slack()` / `hold_slack()` that takes `Tcq, Tcomb_min/max, Tsu, Th, Tskew` and reports pass/fail + which fix applies. Full spec in [coding/EXERCISES.md](../coding/EXERCISES.md#day-7--sequential-logic--timing).

**Interview prompts:** Why doesn't `Tclk` appear in the hold equation? Why does Gray code guarantee a single-bit transition? What makes the LFSR lock up and how do you prevent it?

---

## Bug Anchor / cross-links

- **Feeds:** [Day 9](day09_fsm_sequence_detectors.md) (state register = today's flop + Day 5's next-state logic), [Day 12](day12_metastability_synchronizers.md) (setup/hold violation → metastability, MTBF), [Day 14](day14_async_fifo_cdc_verification.md) (Gray-coded FIFO pointers).
- **Reuses:** [Day 5](day05_boolean_combinational_logic.md) (combinational paths *are* the `Tcomb` in the equations), [Day 21](day21_pipeline_microarch.md) (pipeline registers; pipelining shortens `Tcomb_max` to raise `fmax`), [Day 28](day28_clock_power_gating_upf.md) (retention flops, clock gating, CTS).
- **Hands off:** §6 metastability preview + §9 async-reset release → [Day 12](day12_metastability_synchronizers.md)/[Day 13](day13_cdc_techniques.md).

---

## Exit Check

- [ ] State the latch-vs-flop difference and why latches are avoided in synchronous RTL.
- [ ] Write the `fmax` equation from memory and compute setup slack.
- [ ] Explain in one sentence why slowing the clock cannot fix a hold violation, and give two fixes that do.
- [ ] Explain how skew trades setup against hold.
- [ ] Draw a Gray counter and a Johnson counter; state their state counts (N vs 2N).
- [ ] Build a max-length LFSR; explain the all-zeros lockup and two fixes.
- [ ] State the nonblocking-for-sequential / blocking-for-combinational rule and the race it prevents.

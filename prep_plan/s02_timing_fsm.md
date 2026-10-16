# S02 - Timing and FSMs

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [D8 timing/STA](archive/day08_timing_sta.md), [D9 FSMs](archive/day09_fsm_sequence_detectors.md).
- **Must answer:**
  - The setup-slack equation. Why does lower V mean slower gates, so f must drop with it? (This is the seed of DVFS in S16.)
  - Moore vs Mealy. Draw a REQ/ACK handshake FSM. What do illegal, unreachable, and stuck states look like as hangs?
- **Work link:** device enable and WQ enable are handshake FSMs (request → status). Sketch one as REQ/ACK and mark where it could hang.
- **Handoff →** FSMs need a clock and a known start state. Where do those come from?

## Study notes

### Concept - [day08_timing_sta.md](archive/day08_timing_sta.md)

**Static Timing Analysis** is how you prove a chip meets timing on **every path, at every corner, without simulation** — vectorless and exhaustive, where dynamic sim can only check the paths your stimulus happens to hit. [Day 7](archive/day07_sequential_logic.md) gave the per-flop **contract** (setup/hold/Tcq); today is the **chip-wide methodology**: the four path types, **slack**, **clock skew/jitter/OCV**, **timing exceptions** (multicycle/false/CDC), and the **timing-closure loop**. This is the EE-fundamentals depth the Broadcom/Intel-class loops probe.

### Deep Study Notes - [day08_timing_sta.md](archive/day08_timing_sta.md)

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
For **hold** (same edge): `hold slack = (Tcq + Tcomb(min)) − (Th + skew/uncertainty)` — **`Tclk` absent**, so a hold fail isn't fixed by slowing the clock ([Day 7](archive/day07_sequential_logic.md)). Across the design STA reports **WNS** (worst negative slack) and **TNS** (total negative slack, sum of all violating paths) — WNS tells you the worst path, TNS tells you how much work remains.

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

**On-Chip Variation (OCV)** derates launch vs capture paths differently (one path slow, the other fast) to be conservative; **AOCV/POCV** refine this statistically. **MCMM** (multi-corner multi-mode) signs off every corner × every functional mode (e.g., turbo vs low-power [Day 26](archive/day26_dvfs_pstates_pvt.md)). The rule to recite: **setup is a slow-corner problem, hold is a fast-corner problem.**

### 6. Timing exceptions — don't over-constrain

STA assumes **single-cycle** by default. Real designs have paths that aren't:

| Exception | Meaning | SDC |
|---|---|---|
| **Multicycle path** | data has ≥2 cycles to settle (slow datapath, enable every N) | `set_multicycle_path N` |
| **False path** | logically impossible / never-sampled path | `set_false_path` |
| **CDC path** | crosses async clock domains → synchronized, not timed normally | `set_clock_groups -asynchronous` / `set_false_path` ([Day 12](archive/day12_metastability_synchronizers.md)) |
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

### Bugs in this Topic - [day08_timing_sta.md](archive/day08_timing_sta.md)

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Unconstrained CDC path** | false setup violations / or a real crossing timed | no `set_clock_groups`/false-path | declare async ([Day 12](archive/day12_metastability_synchronizers.md)) |
| **False-pathed real path** | silicon fails, STA was clean | over-aggressive exception | remove the wrong exception |
| **Hold fail at fast corner** | works slow, fails fast/cold | hold not checked at FF corner | MCMM hold signoff + buffer fix |
| **Missing multicycle** | wasted area chasing a non-violation | path constrained single-cycle | `set_multicycle_path` |
| **Under-budgeted uncertainty** | passes STA, fails silicon | skew/jitter margin too small | realistic clock uncertainty |
| **Missing I/O delay** | I/O path unchecked | no `set_input/output_delay` | constrain all I/O |

---

### Interview Explanation (60–90 sec) - [day08_timing_sta.md](archive/day08_timing_sta.md)

> "Static timing analysis proves the chip meets timing on every path without vectors — it builds a timing graph and, for each path, compares arrival time against required time; the difference is slack, and the design passes when worst-negative-slack is non-negative at every corner. It checks four path types — register-to-register, input-to-register, register-to-output, and combinational feed-through — so I have to constrain the I/O or STA can't judge them. The setup check has the clock period in it, so a setup failure is fixed by slowing down, restructuring logic, or pipelining; the hold check is same-edge and the period doesn't appear, so hold is a fast-path race you fix with buffer delay or skew, never a slower clock. Because transistors vary, I sign off across PVT corners with the rule that setup is worst at the slow corner and hold is worst at the fast corner, and on-chip variation derates launch and capture paths conservatively. The subtle part is exceptions: multicycle paths get extra cycles, false paths are never sampled, and CDC crossings must be declared asynchronous so STA doesn't try to time an unsynchronizable path — but if I false-path a real path, a true violation escapes to silicon, so the constraints are as safety-critical as the netlist. Closure is a loop: read the worst path, fix setup first by restructuring or pipelining, then fix hold locally, and repeat across all corners."

---

### Architect Drills (derive, no answer key) - [day08_timing_sta.md](archive/day08_timing_sta.md)

1. Write the setup-slack and hold-slack equations with arrival/required and clock uncertainty. Show why the period is absent from hold.
2. A path fails setup by 80 ps at 1 GHz. Give four distinct fixes and the cost of each (tie to [Day 6](archive/day06_arithmetic_circuits.md)/[Day 21](archive/day21_pipeline_microarch.md)).
3. Why is hold checked at the fast corner and setup at the slow corner? What is OCV protecting against?
4. You see thousands of setup violations on paths between two unrelated clocks. Diagnose and write the one SDC line that fixes it ([Day 12](archive/day12_metastability_synchronizers.md)).
5. A datapath is enabled every 4th cycle but STA flags it. Which exception, and what's the risk of getting the multiplier wrong?
6. Useful skew steals 30 ps for a slow path — what did it cost, and where?
7. The netlist is perfect but silicon fails timing. Name three constraint mistakes that cause this.

---

### Exit Check - [day08_timing_sta.md](archive/day08_timing_sta.md)

- [ ] State why STA beats dynamic timing (exhaustive, vectorless) and what it can miss (false paths).
- [ ] Name the four path types and which need I/O constraints.
- [ ] Write setup/hold slack from arrival/required; explain WNS vs TNS.
- [ ] Recite "setup = slow corner, hold = fast corner" and what OCV derates.
- [ ] Distinguish multicycle / false / CDC exceptions and the danger of each direction.
- [ ] Explain why the SDC is as safety-critical as the netlist.
- [ ] Walk the closure loop: setup first (restructure/pipeline/skew), then hold (buffers).

### Concept - [day09_fsm_sequence_detectors.md](archive/day09_fsm_sequence_detectors.md)

A **finite-state machine** is combinational next-state logic ([Day 5](archive/day05_boolean_combinational_logic.md)) feeding a state register ([Day 7](archive/day07_sequential_logic.md)). It's how hardware *remembers* — the moment a problem depends on **what came before** (a sequence, a mode, a protocol phase), you need an FSM. Master the 7-step derivation so you can **build** a sequence detector (the `101` / `1011` classic) on a whiteboard from first principles, choose Moore vs Mealy and an encoding deliberately, and recognize that a 5-state toy and a 12-state **LTSSM** are the same machine at different scales.

### Deep Study Notes - [day09_fsm_sequence_detectors.md](archive/day09_fsm_sequence_detectors.md)

### 1. The one idea everything hangs on

> **A state is the minimal summary of the past you need to decide the future.** You never store the whole history — only just enough.

Designing an FSM is mostly answering: **"what is the least I must remember?"** For a pattern detector, the answer is almost always *"how much of the target pattern have I matched so far."* Nail that and the states write themselves.

### 2. The five pieces + Moore vs Mealy

| Piece | Meaning | In the bubble diagram |
|---|---|---|
| **States** | the distinct situations | bubbles |
| **Inputs** | what you sample each clock | arrow labels |
| **Transitions** δ(state,input) | from-state on-input → to-state | arrows |
| **Reset state** | where you are after reset | arrow from nowhere |
| **Outputs** | what the machine asserts | inside bubble (Moore) / on arrow (Mealy) |

- **Moore:** output = f(state) — written *inside the bubble*. **Registered, glitch-free**, reacts one cycle later, may need an extra state.
- **Mealy:** output = f(state, input) — written *on the arrow*. **Fewer states, same-cycle reaction**, but the output is combinational on the input so it can **glitch**.

**Interview rule:** draw **Moore first** (safer to reason about), then add "the Mealy version saves a state by asserting on the transition, at the cost of a combinational, glitch-prone output."

### 3. The 7-step recipe (works for ANY sequence-detector question)

1. **Find the trigger** — "when exactly does output go to 1?" (e.g., "stream just ended in `101`").
2. **Decide what to remember** — the longest *prefix of the target matched so far*: `∅ → 1 → 10 → 101`. That list **is** your state set.
3. **Name the reset state** = "matched nothing" (`S0`).
4. **Make every state total** — for *every* state draw *exactly one* arrow for *every* input value. A missing branch becomes an **inferred latch** (Day 5 §10).
5. **Handle partial matches (the subtle step)** — on a "wrong" bit you fall back to the **longest suffix of what you've seen that is still a valid prefix of the target**, *not* always to `S0`. That single rule is the entire trick behind **overlap**.
6. **Place the outputs** — Moore: mark the "matched" state(s). Mealy: mark the completing transition.
7. **Encode & code it** — pick an encoding, then write a combinational next-state block + a clocked state register + the output logic.

### 4. Worked: overlapping `101` detector (Moore)

States = how much of `101` is matched: `S0=∅, S1="1", S2="10", S3="101"(y=1)`.

```
Forward (happy path 1→10→101):
  ──►( S0 )──1──►( S1 )──0──►( S2 )──1──►(( S3 ))  y=1

Back-edges (recipe step 5 — longest valid suffix):
  S0 --0--> S0      still nothing
  S1 --1--> S1      a fresh "1" is still "matched 1"
  S2 --0--> S0      "...100": no usable suffix → restart
  S3 --1--> S1      OVERLAP: trailing 1 begins a new match
  S3 --0--> S2      OVERLAP: trailing 10 is a 2-step head start

Transition table (every state total):
  state  x=0  x=1
  S0  →  S0   S1
  S1  →  S2   S1
  S2  →  S0   S3
  S3  →  S2   S1     y = 1 only in S3
```

`10101` correctly fires **twice** — that's what "overlapping" buys you. **Non-overlapping** would reset to `S0` after a match (`S3 --x--> S0`), so the trailing `1` *cannot* seed the next match. Always **clarify overlapping vs not** out loud — stating the ambiguity is itself a positive signal.

```systemverilog
module seq101 (input clk, rst_n, x, output reg y);
  localparam S0=2'd0, S1=2'd1, S2=2'd2, S3=2'd3;
  reg [1:0] state, next;
  always @(*) begin                     // next-state (combinational)
    case (state)
      S0: next = x ? S1 : S0;
      S1: next = x ? S1 : S2;
      S2: next = x ? S3 : S0;
      S3: next = x ? S1 : S2;           // overlap
      default: next = S0;               // full case → no latch
    endcase
  end
  always @(posedge clk or negedge rst_n)// state register (sequential)
    if (!rst_n) state <= S0; else state <= next;
  always @(*) y = (state == S3);        // Moore output
endmodule
```

**Mealy version** drops `S3`: assert `y` on the `S2 --x=1--> S1` transition (the `1` is also a new prefix). One fewer state; output glitches with the input.

### 5. State encoding — binary vs one-hot vs Gray

| Encoding | Bits for N states | Pros | Use when |
|---|---|---|---|
| **Binary** | `⌈log₂N⌉` | fewest flops, dense | small FSMs, area-critical |
| **One-hot** | `N` (one bit/state) | **fast decode** (state = single bit), trivial "exactly-one" assertion, simple next-state | speed-critical, FPGA (flops are cheap), most RTL FSMs |
| **Gray** | `⌈log₂N⌉` | one bit changes per step → low switching power, **safe across CDC** | counters/pointers, low-power, CDC ([Day 14](archive/day14_async_fifo_cdc_verification.md)) |

One-hot also gives a **free assertion**: `$onehot(state)` must always hold — a cheap, powerful invariant for verification (Day 25). The encoding is a *deliberate* choice; say *why* you picked it.

### 6. State minimization — don't pay for states you don't need

Two states are **equivalent** if, for every possible input sequence, they produce the same output and transition to equivalent states. Merge equivalent states to shrink the machine.

- **By inspection:** states with identical output *and* identical next-state for every input are immediately mergeable.
- **Implication chart (systematic):** tabulate every state pair, cross out pairs with different outputs, then iteratively cross out pairs whose successors are now distinguishable; survivors are equivalent.

Minimization reduces flops, decode logic, and the verification surface. But don't over-minimize past clarity in an interview — a clean, slightly-larger Moore machine you can explain beats a minimal one you can't.

### 7. FSM coding styles (and the bug each prevents)

```
1-block  : everything in one clocked always — compact, but mixes
           combinational + sequential, easy to introduce races. Avoid.
2-block  : (a) clocked state register  +  (b) combinational next-state/output.
           The standard, readable, latch-safe style.
3-block  : separate clocked state reg, comb next-state, comb (or registered)
           output. Clearest separation; preferred for non-trivial FSMs.
```

The **2-block / 3-block** styles enforce Day 5's "full case + default" and Day 7's "nonblocking-seq / blocking-comb" rules — which is exactly how you avoid inferred latches and sim/synth mismatch in an FSM.

### 8. FSMD — control + datapath

Real blocks are a **datapath** (registers, ALUs, muxes, counters — Days 5–7) steered by a **control FSM** that asserts enables/selects each cycle. A memory controller, a DMA engine ([Day 20](archive/day20_dma_doorbells_ordering_barriers.md)), a SerDes link trainer — all are **FSMD**: the FSM walks phases (idle → request → transfer → done) and at each phase drives the datapath's control signals. When you "design a cache controller" or "design a DMA engine" in an interview, you're drawing an FSMD: name the states, then name what the datapath does in each.

### 9. The same machine at scale — LTSSM, MESI, C-states

- **LTSSM** ([Day 33](archive/day33_pcie_fundamentals_ltssm.md)): `Detect → Polling → Config → L0 → Recovery → L1/L2…` — a Moore FSM whose "what to remember" is *link-training phase*. The §6 stuck-state debug ladder ([Day 36](archive/day36_pcie_bringup_functional_val.md)) is literally "which state is it wedged in and which transition's condition never fired."
- **MESI** ([Day 17](archive/day17_coherence_mesi_false_sharing.md)): 4-state FSM per cache line; inputs are local/remote read/write + snoops.
- **C-state / ASPM** ([Day 27](archive/day27_cstates_pkgc_s0ix.md)/[Day 35](archive/day35_pcie_power_management.md)): power FSMs with entry/exit conditions.

Recognizing these as FSMs means a "design/validate the X protocol" question reduces to the 7-step recipe + the verification axes of Day 24 (cover every state, every legal transition, and assert no illegal one).

### 10. Hazards specific to FSMs

- **Mealy output glitch** — output is combinational on the input; if it feeds an async reset/clock/CDC it can be latched as a false event (Day 5 §9). Register the Mealy output (making it effectively Moore-timed) when it crosses a domain.
- **Inferred latch** — incomplete next-state or output `case` (Day 5 §10).
- **Unreachable / deadlock states** — with binary encoding, unused codes (e.g., a 3-state FSM in 2 bits has a 4th code) can be entered via an upset/SEU and **never leave**. Always code `default:` to steer illegal states back to reset (**safe FSM**). For one-hot, assert `$onehot` and recover on violation.

---

### Bugs in this Topic - [day09_fsm_sequence_detectors.md](archive/day09_fsm_sequence_detectors.md)

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Missed overlap** | `10101` fires once, not twice | back-edge went to `S0` not the suffix state | step-5 longest-suffix rule |
| **Inferred latch in FSM** | sim≠synth, X-prop | incomplete next-state/output case | full case + `default` |
| **Deadlock in illegal state** | FSM wedges after glitch/SEU | unused binary code with no exit | `default → reset`, `$onehot` recover |
| **Mealy glitch latched** | spurious event downstream | comb Mealy output crossed a domain | register the output / Moore |
| **Wrong reset state** | first match missed | reset not "matched nothing" | reset = `S0 = ∅` |
| **Over-coded states** | extra flops, hard to verify | skipped minimization | merge equivalent states |

---

### Interview Explanation (60–90 sec) - [day09_fsm_sequence_detectors.md](archive/day09_fsm_sequence_detectors.md)

> "An FSM is combinational next-state logic feeding a state register — it's how hardware remembers. I design one with a fixed recipe: find exactly when the output fires, decide the least I must remember — for a pattern detector that's how much of the pattern I've matched, which becomes my states — pick a reset state meaning 'matched nothing,' then make every state total by drawing one branch per input. The subtle step is partial matches: on a wrong bit I don't always fall back to start, I fall back to the longest suffix that's still a valid prefix of the target — that's what makes `101` overlap so `10101` fires twice. I default to a Moore machine because the output is registered and glitch-free; the Mealy version saves a state by asserting on the transition but its output is combinational and can glitch. Encoding is a deliberate choice — one-hot for fast decode and a free `$onehot` assertion, binary for area, Gray when the state or a pointer crosses a clock domain. And I always code a `default` that steers illegal states back to reset so a glitch can't deadlock the machine. The payoff is that big protocol machines — PCIe's LTSSM, MESI, the C-state controller — are the exact same construction at larger scale."

---

### Architect Drills (derive, no answer key) - [day09_fsm_sequence_detectors.md](archive/day09_fsm_sequence_detectors.md)

1. Build the **`1011`** overlapping detector with the 7-step recipe. Where do the overlap back-edges land, and what's the longest suffix after `1011`+`1`?
2. Convert your `101` Moore machine to Mealy. Which state disappears, and what new hazard appears if the output crosses a clock domain?
3. Non-overlapping vs overlapping `110` detector: show the one transition that differs and why `110110` fires twice vs once.
4. Encode a 5-state FSM three ways (binary, one-hot, Gray). For each, write the "illegal/unreachable state" handling.
5. A binary-encoded 3-state FSM wedges in the field after a particle strike. Diagnose; give the "safe FSM" fix.
6. Reduce this machine: states A and B have identical outputs and both go to C on 0 and D on 1. Are they equivalent? Use the implication-chart idea.
7. Sketch a DMA engine as an FSMD: name 4–5 control states and what the datapath does in each. Map it to [Day 20](archive/day20_dma_doorbells_ordering_barriers.md).

---

### Exit Check - [day09_fsm_sequence_detectors.md](archive/day09_fsm_sequence_detectors.md)

- [ ] Recite the 7-step recipe and use it to build `101` and `1011` from scratch.
- [ ] State the longest-valid-suffix rule and show `10101` firing twice (overlap).
- [ ] Draw both Moore and Mealy versions; state the timing/glitch trade.
- [ ] Choose an encoding and justify it (one-hot fast-decode + `$onehot`; Gray for CDC; binary for area).
- [ ] Show the safe-FSM `default → reset` and explain the deadlock it prevents.
- [ ] Reduce a small machine using state equivalence.
- [ ] Map LTSSM / MESI / C-state to the FSM vocabulary in one sentence each.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S01](s01_signal_logic_state.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S03](s03_clocks_reset.md)

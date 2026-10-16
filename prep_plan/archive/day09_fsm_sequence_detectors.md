# Day 9 — FSM Design + Sequence Detectors

## Concept

A **finite-state machine** is combinational next-state logic ([Day 5](day05_boolean_combinational_logic.md)) feeding a state register ([Day 7](day07_sequential_logic.md)). It's how hardware *remembers* — the moment a problem depends on **what came before** (a sequence, a mode, a protocol phase), you need an FSM. Master the 7-step derivation so you can **build** a sequence detector (the `101` / `1011` classic) on a whiteboard from first principles, choose Moore vs Mealy and an encoding deliberately, and recognize that a 5-state toy and a 12-state **LTSSM** are the same machine at different scales.

## Why this day in the flow

This is where Days 5 and 7 combine (combinational next-state + a state register). It also **pre-loads the protocol state machines you'll meet later**: the PCIe **LTSSM** ([Day 33](day33_pcie_fundamentals_ltssm.md)/[Day 36](day36_pcie_bringup_functional_val.md)), the **MESI** controller ([Day 17](day17_coherence_mesi_false_sharing.md)), the **C-state**/**ASPM** machines ([Day 27](day27_cstates_pkgc_s0ix.md)/[Day 35](day35_pcie_power_management.md)) are all FSMs — master the vocabulary now and those days read as "the same machine at larger scale" instead of new material. It is also the launch point for [Day 12–14](day12_metastability_synchronizers.md): the handshake/pulse synchronizers and async-FIFO control are all small FSMs crossing clock domains.

**Broadcom R2 + R1 gap this closes:** "draw a `101` detector bubble diagram," Moore vs Mealy, state minimization, and the FSM-coding hazards (inferred latch, Mealy glitch). This deepens the condensed §1 in [broadcom_switch_presi_notes.html](../../broadcom_switch_presi_notes.html) into a full derivation.

---

## Deep Study Notes

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
| **Gray** | `⌈log₂N⌉` | one bit changes per step → low switching power, **safe across CDC** | counters/pointers, low-power, CDC ([Day 14](day14_async_fifo_cdc_verification.md)) |

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

Real blocks are a **datapath** (registers, ALUs, muxes, counters — Days 5–7) steered by a **control FSM** that asserts enables/selects each cycle. A memory controller, a DMA engine ([Day 20](day20_dma_doorbells_ordering_barriers.md)), a SerDes link trainer — all are **FSMD**: the FSM walks phases (idle → request → transfer → done) and at each phase drives the datapath's control signals. When you "design a cache controller" or "design a DMA engine" in an interview, you're drawing an FSMD: name the states, then name what the datapath does in each.

### 9. The same machine at scale — LTSSM, MESI, C-states

- **LTSSM** ([Day 33](day33_pcie_fundamentals_ltssm.md)): `Detect → Polling → Config → L0 → Recovery → L1/L2…` — a Moore FSM whose "what to remember" is *link-training phase*. The §6 stuck-state debug ladder ([Day 36](day36_pcie_bringup_functional_val.md)) is literally "which state is it wedged in and which transition's condition never fired."
- **MESI** ([Day 17](day17_coherence_mesi_false_sharing.md)): 4-state FSM per cache line; inputs are local/remote read/write + snoops.
- **C-state / ASPM** ([Day 27](day27_cstates_pkgc_s0ix.md)/[Day 35](day35_pcie_power_management.md)): power FSMs with entry/exit conditions.

Recognizing these as FSMs means a "design/validate the X protocol" question reduces to the 7-step recipe + the verification axes of Day 24 (cover every state, every legal transition, and assert no illegal one).

### 10. Hazards specific to FSMs

- **Mealy output glitch** — output is combinational on the input; if it feeds an async reset/clock/CDC it can be latched as a false event (Day 5 §9). Register the Mealy output (making it effectively Moore-timed) when it crosses a domain.
- **Inferred latch** — incomplete next-state or output `case` (Day 5 §10).
- **Unreachable / deadlock states** — with binary encoding, unused codes (e.g., a 3-state FSM in 2 bits has a 4th code) can be entered via an upset/SEU and **never leave**. Always code `default:` to steer illegal states back to reset (**safe FSM**). For one-hot, assert `$onehot` and recover on violation.

---

## Bugs in this Topic

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Missed overlap** | `10101` fires once, not twice | back-edge went to `S0` not the suffix state | step-5 longest-suffix rule |
| **Inferred latch in FSM** | sim≠synth, X-prop | incomplete next-state/output case | full case + `default` |
| **Deadlock in illegal state** | FSM wedges after glitch/SEU | unused binary code with no exit | `default → reset`, `$onehot` recover |
| **Mealy glitch latched** | spurious event downstream | comb Mealy output crossed a domain | register the output / Moore |
| **Wrong reset state** | first match missed | reset not "matched nothing" | reset = `S0 = ∅` |
| **Over-coded states** | extra flops, hard to verify | skipped minimization | merge equivalent states |

---

## Interview Explanation (60–90 sec)

> "An FSM is combinational next-state logic feeding a state register — it's how hardware remembers. I design one with a fixed recipe: find exactly when the output fires, decide the least I must remember — for a pattern detector that's how much of the pattern I've matched, which becomes my states — pick a reset state meaning 'matched nothing,' then make every state total by drawing one branch per input. The subtle step is partial matches: on a wrong bit I don't always fall back to start, I fall back to the longest suffix that's still a valid prefix of the target — that's what makes `101` overlap so `10101` fires twice. I default to a Moore machine because the output is registered and glitch-free; the Mealy version saves a state by asserting on the transition but its output is combinational and can glitch. Encoding is a deliberate choice — one-hot for fast decode and a free `$onehot` assertion, binary for area, Gray when the state or a pointer crosses a clock domain. And I always code a `default` that steers illegal states back to reset so a glitch can't deadlock the machine. The payoff is that big protocol machines — PCIe's LTSSM, MESI, the C-state controller — are the exact same construction at larger scale."

---

## Architect Drills (derive, no answer key)

1. Build the **`1011`** overlapping detector with the 7-step recipe. Where do the overlap back-edges land, and what's the longest suffix after `1011`+`1`?
2. Convert your `101` Moore machine to Mealy. Which state disappears, and what new hazard appears if the output crosses a clock domain?
3. Non-overlapping vs overlapping `110` detector: show the one transition that differs and why `110110` fires twice vs once.
4. Encode a 5-state FSM three ways (binary, one-hot, Gray). For each, write the "illegal/unreachable state" handling.
5. A binary-encoded 3-state FSM wedges in the field after a particle strike. Diagnose; give the "safe FSM" fix.
6. Reduce this machine: states A and B have identical outputs and both go to C on 0 and D on 1. Are they equivalent? Use the implication-chart idea.
7. Sketch a DMA engine as an FSMD: name 4–5 control states and what the datapath does in each. Map it to [Day 20](day20_dma_doorbells_ordering_barriers.md).

---

## Coding A — `d09_fsm_detect`

Implement two detectors in C and prove them equivalent on random bit streams: a **state-machine `seq_detect()`** (parameterized target pattern, overlapping flag, returns per-bit output) and a **sliding-window matcher** (shift register + comparator, the literal "pattern in the last K bits" reading). Add a **safe-FSM** harness that injects an illegal state code and shows the `default → reset` recovery. Full spec in [coding/EXERCISES.md](../../coding/EXERCISES.md#day-9--fsm-design--sequence-detectors).

**Interview prompts:** Overlapping vs non-overlapping — which transition changes? Moore vs Mealy timing of the output. Why does one-hot give a free assertion?

---

## Bug Anchor / cross-links

- **Built from:** [Day 5](day05_boolean_combinational_logic.md) (next-state = combinational block; inferred-latch rule), [Day 7](day07_sequential_logic.md) (state register, encoding, Gray for CDC).
- **Closes the loop on:** [Day 17](day17_coherence_mesi_false_sharing.md) (MESI FSM), [Day 27](day27_cstates_pkgc_s0ix.md)/[Day 35](day35_pcie_power_management.md) (C-state/ASPM FSMs), [Day 33](day33_pcie_fundamentals_ltssm.md)/[Day 36](day36_pcie_bringup_functional_val.md) (LTSSM — same machine, bigger).
- **Feeds:** [Day 13](day13_cdc_techniques.md) (handshake/pulse synchronizers are tiny FSMs), [Day 14](day14_async_fifo_cdc_verification.md) (FIFO full/empty control), [Day 24–25](day24_verification_planning_coverage.md) (FSM state/transition coverage + `$onehot`/illegal-transition assertions).

---

## Exit Check

- [ ] Recite the 7-step recipe and use it to build `101` and `1011` from scratch.
- [ ] State the longest-valid-suffix rule and show `10101` firing twice (overlap).
- [ ] Draw both Moore and Mealy versions; state the timing/glitch trade.
- [ ] Choose an encoding and justify it (one-hot fast-decode + `$onehot`; Gray for CDC; binary for area).
- [ ] Show the safe-FSM `default → reset` and explain the deadlock it prevents.
- [ ] Reduce a small machine using state equivalence.
- [ ] Map LTSSM / MESI / C-state to the FSM vocabulary in one sentence each.

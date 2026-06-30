# Day 25 — Testbench Architecture + Assertions + Debug

## Concept

The **testbench** is the machine that *executes* the verification plan ([Day 24](day24_verification_planning_coverage.md)): a layered environment that **generates** constrained-random stimulus, **drives** it into the DUT, **monitors** the outputs, and **checks** them against a **reference model** via a **scoreboard** — backed by **assertions** that fire the instant a protocol rule breaks, and a disciplined **debug methodology** that triages from the *first diverging cycle*. Plus the explicit **functional-vs-error/negative** test split, and self-checking discipline so a passing regression actually *means* something.

## Why this day in the flow

[Day 24](day24_verification_planning_coverage.md) was the *plan*; today is the *infrastructure* that realizes it, deepening the UVM/scoreboard sketch from [Day 23](day23_cpu_bringup_functional_val.md) §5–§7. It **caps Phase 5 (Verification Engineering)** and directly answers Broadcom's R2 ask to **"write functional + error/negative tests"** and Nvidia's gap on **debug-as-execution-trace narration**. The DUTs you'd build it around are the Phase 1–7 blocks: the FSM ([Day 9](day09_fsm_sequence_detectors.md)), the async FIFO ([Day 14](day14_async_fifo_cdc_verification.md)), the ECC unit ([Day 15](day15_memory_systems_ecc.md)).

**Broadcom R2 gap this closes:** functional vs error test design; the assertion + scoreboard discipline behind a real verification effort.

---

## Deep Study Notes

### 1. The layered testbench (UVM-shape, but the idea is universal)

```
  ┌──────────────────────────── env ──────────────────────────────┐
  │  config object (knobs, modes, virtual interfaces)              │
  │                                                                │
  │  sequence ─► sequencer ─► DRIVER ─► ┌──────┐ ─► MONITOR ─┐     │
  │   (txn stream)                       │ DUT  │             │     │
  │                          reference ─►│(RTL) │             ▼     │
  │                            MODEL ────┴──────┘        SCOREBOARD │
  │                              │                          ▲       │
  │                              └────── expected ──────────┘       │
  │                                              mismatch → $error  │
  └────────────────────────────────────────────────────────────────┘
```

| Component | Job |
|---|---|
| **Sequence / sequencer** | produce a stream of **transactions** (abstract stimulus items) |
| **Driver** | turn each transaction into **pin wiggles** on the DUT interface (timing, protocol) |
| **Monitor** | passively **observe** the interface, reconstruct transactions from pins |
| **Reference model** | a **golden** implementation (C/SystemC/ISA-sim) computing the expected result |
| **Scoreboard / checker** | **compare** DUT-observed vs reference-expected; flag the first mismatch |
| **Agent** | bundles driver+monitor+sequencer for one interface (**active** drives, **passive** only watches) |
| **Env / config** | composes agents + scoreboard; the config object sets modes/knobs |

The value of the layering is **reuse and abstraction**: tests speak in transactions ("send a 64B unicast frame to port 7"), the driver handles the pin-level protocol. Even a hand-rolled C testbench should keep these *roles* separate — generator, driver, monitor, checker — because mixing them is where testbenches rot.

### 2. Stimulus — constrained random (CRV) + directed

```systemverilog
class frame_txn;
  rand bit [3:0] in_port, out_port;
  rand bit [13:0] len;
  rand pkt_kind_e kind;                 // unicast/multicast/broadcast
  constraint legal { len inside {[64:9000]};
                     kind == MULTICAST -> out_port == '0;   // legal shape only
                     in_port != out_port; }                 // no loopback (per spec)
endclass
```

**CRV** randomizes within **constraints that keep stimulus legal**, generating volume and unexpected interactions to fill the coverage space ([Day 24](day24_verification_planning_coverage.md)); **directed** tests pin specific corners CRV can't reach. Constraints are the safety rail — *unconstrained randomization drives illegal stimulus and produces false failures*. Always **log the seed** so any run is reproducible.

### 3. Reference model + scoreboard — self-checking, first-divergence

The reference model runs the **same** stimulus in software and produces the **expected** result; the scoreboard compares architectural/observable state and **flags the first divergence**.

```
stimulus ─┬─► DUT  ──► monitor ─┐
          └─► ref model ────────┴─► scoreboard ── mismatch → $error @first diverging cycle
```

**Any divergence is a bug — in the RTL, the model, or the test.** Triage *starts at the first diverging cycle* (everything after is noise). This is why **self-checking** matters: a test that doesn't compare against a golden expectation is a test that **passes while verifying nothing** (the Day 24 anti-pattern). *No waveform-staring as the primary check — the scoreboard says pass/fail, the waveform is for root-cause.*

### 4. Assertions (SVA) — the "shall never" net

```systemverilog
// Immediate (procedural): fires inline
always_ff @(posedge clk) if (overflow) $error("FIFO overflow");

// Concurrent (temporal): a property over time. |-> same cycle, |=> next cycle
property req_grant;  @(posedge clk) disable iff(!rst_n)
  req |-> ##[1:8] grant;                         // grant within 1..8 cycles of req
endproperty
assert property (req_grant);

// Useful system functions
assert property (@(posedge clk) $onehot(state));         // one-hot FSM invariant
assert property (@(posedge clk) valid |-> $stable(data));// data held while valid
cover  property (@(posedge clk) qdepth==0 ##1 qdepth==32);// reached empty→full
```

- **Immediate** = procedural, fires on the path; **concurrent** = temporal property sampled each clock.
- `$onehot`, `$stable`, `$rose/$fell`, `$past` express FSM/CDC/protocol invariants compactly.
- **`cover property`** counts that a scenario *happened* (feeds functional coverage) — assertions both **catch illegal** behaviour and **prove scenarios occurred**.
- Assertions live in the design (white-box) or in a **bind file** (no RTL edit) — the **val team owns the bind file**. **Rule:** every spec "shall" → an `assert`; every "should occur" → a `cover`.

### 5. Functional vs error/negative tests — the Broadcom split

| Class | Definition | Asserts |
|---|---|---|
| **Functional** | legal stimulus on the happy path + legal corners | correct output, correct ordering, correct counts |
| **Error / negative** | **illegal** stimulus + **fault injection** + protocol violations | **graceful handling** — drop+count, raise flag, recover, never hang/corrupt |

For a switch ([Day 24](day24_verification_planning_coverage.md)) the *functional* tests forward valid frames; the *error* tests inject **CRC errors** (→ dropped + counter increments), **runt/oversize** frames (→ dropped), **buffer overflow** (→ backpressure, not silent drop), **table miss** (→ defined behaviour). For a FIFO ([Day 14](day14_async_fifo_cdc_verification.md)): force overflow/underflow → flags fire, no data lost/duplicated. **The discriminator between "works" and "robust" is the error suite** — and it's the half candidates skip. Always present both halves.

### 6. Debug & failure-analysis methodology (the execution trace)

A repeatable triage loop — narrate it *as a trace*, the Nvidia-gap fix:

```
1. SYMPTOM        scoreboard mismatch / assertion fire / hang — capture exact signature
2. FIRST DIVERGE  find the FIRST cycle DUT ≠ reference (binary-search in time);
                  everything after is downstream noise
3. NARROW         isolate the stimulus (minimize the failing sequence; same seed);
                  is it RTL, reference model, or test bug?
4. ROOT CAUSE     trace back from the diverging signal through the cone to the cause
                  (waveform/FSDB in Verdi, assertion context, $past)
5. FIX            patch RTL or model or test; add a directed test that reproduces it
6. REGRESS        re-run the minimized test + full regression; close the CR
```

**Rules:** triage from the **first** divergence, not the symptom's surface; **minimize** the failing case before deep-diving; always **lock the seed** for reproducibility; a fixed bug gets a **directed regression test** so it can never silently return. Tools: **Verdi/FSDB** waveform, simulator assertion logs, scoreboard dumps, coverage reports, regression dashboards.

### 7. Reproducibility & triage infrastructure

- **Seeds:** every CRV run logs its seed; a failure is replayed bit-identically.
- **Regression dashboard:** nightly/weekly runs, pass/fail trend, **bug bucketing** (group failures by signature so one root cause ≠ 50 "new" bugs).
- **Minimization:** shrink a failing random test to the smallest reproducer before filing.
- **CR lifecycle:** reproduce → isolate → file → fix → verify-fix → regress.

### 8. Bug-cost ladder — why all this lives pre-Si

| Phase | Cost/bug | Catches cheaply |
|---|---|---|
| RTL sim | ~\$1 | functional, protocol, FSM, register |
| Emulation | ~\$10 | HW×FW, cross-IP, multi-core, boot |
| FPGA proto | ~\$100 | full SW stack at MHz |
| Post-Si | ~\$1000 | analog, PVT, interop, real workload |

**Shift-left:** any bug class catchable in sim *must* be caught there — the testbench + assertions + coverage are how you guarantee it. An escape to post-Si is a **coverage/assertion gap**, not bad luck.

---

## Anti-patterns / Bugs in this Topic

| Anti-pattern | Why dangerous | Fix |
|---|---|---|
| **Non-self-checking test** | passes while verifying nothing | scoreboard/assert every outcome |
| **Assertion never covered** | a "check" that never ran | pair with `cover property`; verify it fires |
| **Unconstrained randomization** | illegal stimulus → false fails | proper constraints; legal-shape rules |
| **Seed not logged** | failure can't be reproduced | log + replay seeds |
| **Debug from symptom** | chases downstream noise | triage from first divergence |
| **Scoreboard ordering bug** | false mismatch on reordered-but-legal output | model allowed reordering / per-flow order |
| **No error suite** | only happy path proven | add fault-injection/negative tests |

---

## Interview Explanation (60–90 sec)

> "A testbench executes the plan with separated roles: a sequencer generates abstract transactions, a driver turns them into pin-level protocol, a monitor observes the interface, a reference model computes the expected result in software, and a scoreboard compares them and flags the first diverging cycle. I keep everything self-checking — a test that doesn't compare against a golden model passes while proving nothing — and I drive volume with constrained-random stimulus whose constraints keep it legal, while directed tests plug the corners random can't reach. On top I write assertions: concurrent SVA properties for every 'shall', like request implies grant within eight cycles, plus invariants like one-hot state and data-stable-while-valid, and cover properties to prove scenarios actually happened. I always present two test classes — functional on the legal happy path, and error or negative tests that inject illegal inputs and faults and assert graceful handling, drop-and-count or flag-and-recover rather than hang or corrupt — because the error suite is what separates 'works' from 'robust.' When something fails I debug as a trace: capture the signature, binary-search to the first cycle where the DUT diverges from the reference, minimize the stimulus, decide whether it's RTL, model, or test, root-cause through the cone in the waveform, fix it, and add a directed regression so it can't come back. And I log every seed so any failure replays bit-for-bit."

---

## Architect Drills (derive, no answer key)

1. Draw the testbench for the [Day 14](day14_async_fifo_cdc_verification.md) async FIFO: what does the reference model compute, and what exactly does the scoreboard compare?
2. Write three SVA properties for the FIFO (no overflow, no underflow, data-stable-while-valid) and one `cover` for empty→full.
3. Split the test list for the ECC unit ([Day 15](day15_memory_systems_ecc.md)) into functional vs error/negative. What does each error test assert?
4. A constrained-random run fails 1-in-500 seeds. Walk your triage from symptom to root cause and to a permanent regression test.
5. Why must the scoreboard model *legal reordering* for a switch with multiple egress queues? What false failure appears if it doesn't?
6. An assertion never fired in a year of regressions. Is the design perfect, or is the assertion dead? How do you tell?
7. Narrate a PCIe link-up failure ([Day 36](day36_pcie_bringup_functional_val.md)) as a first-divergence debug trace (the Nvidia-gap drill).

---

## Coding A — `d25_scoreboard_crv`

Build a **self-checking testbench in C** around a DUT model (reuse the [Day 14](day14_async_fifo_cdc_verification.md) FIFO or a small ALU): a **reference model**, a **constrained-random** stimulus generator (legal-shape constraints + logged seed for replay), a **scoreboard** that flags the **first diverging transaction**, assertion-style invariant checks (`assert_no_overflow`, `assert_inorder`), and an **error-injection** mode that forces illegal inputs and verifies the flags/handling instead of corruption. Full spec in [coding/EXERCISES.md](../coding/EXERCISES.md#day-25--testbench-architecture--assertions--debug).

**Interview prompts:** Why self-checking? What does an assertion catch that a scoreboard doesn't (and vice versa)? Functional vs error tests for this DUT?

---

## Bug Anchor / cross-links

- **Executes:** [Day 24](day24_verification_planning_coverage.md) (the plan/coverage model), [Day 23](day23_cpu_bringup_functional_val.md) (UVM env, scoreboard, reference model — deepened here).
- **DUTs:** [Day 9](day09_fsm_sequence_detectors.md) (FSM + `$onehot` assertion), [Day 14](day14_async_fifo_cdc_verification.md) (FIFO scoreboard + CDC/metastability injection), [Day 15](day15_memory_systems_ecc.md) (ECC/MBIST checking).
- **Debug-trace tie-ins:** [Day 36](day36_pcie_bringup_functional_val.md) (LTSSM stuck-state), [Day 31](day31_post_si_pm_validation.md) (bug stories), [Day 41](day41_final_mock.md) (deliver under time).

---

## Exit Check

- [ ] Draw the layered testbench and name each component's job.
- [ ] Explain self-checking and why a non-checking test is worse than no test.
- [ ] Write immediate + concurrent + cover SVA, and three invariant system functions.
- [ ] Split any DUT's tests into functional vs error/negative and say what each asserts.
- [ ] Narrate the 6-step debug loop as a first-divergence trace.
- [ ] State why seeds are logged and how bug bucketing prevents duplicate triage.
- [ ] Tie every "shall" to an assertion and every "should occur" to a cover.

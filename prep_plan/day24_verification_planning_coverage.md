# Day 24 — Verification Planning + Coverage

## Concept

Verification planning is the **reusable methodology** that turns a **spec** into a **test plan** — not solved from scratch each time, but a repeatable scaffold: **spec → features → stimulus → checks → coverage model → corner/negative cases → performance → pre-Si vs post-Si**. **Coverage** (functional + code) is the metric that answers *"are we done?"*. Master the scaffold so you can stand at a whiteboard and build a verification plan for **any** block — a switch, a FIFO, an ECC unit, a PCIe controller — without freezing.

## Why this day in the flow

[Day 23](day23_cpu_bringup_functional_val.md) named the **deliverables** of a CPU IP Val team (vplan, testbench, coverage, assertions); [Day 30](day30_pre_si_pm_validation.md) applied **coverage axes** to power management. Today extracts the **methodology** underneath both so it generalizes to *any* IP — which is exactly the gap that recurred across **Broadcom** ("verification plan for a 16-port 1.6T switch" — failed), **Cerebras** (PCIe test-plan-from-spec opener), and **Amazon** ("test plan for an IP"). It pairs with [Day 25](day25_testbench_assertions_debug.md) (the testbench that *executes* the plan) and consumes the concrete blocks from Phases 5–7 (FSM, FIFO, memory/ECC) as worked examples.

**Broadcom R2 gap this closes:** verification planning from first principles (self-rated 4/10) — "no structured framework, solved from scratch."

---

## Deep Study Notes

### 1. Why a plan exists — the two arguments

1. **Cost of escape** ([Day 23](day23_cpu_bringup_functional_val.md) §10): a bug caught in RTL sim costs ~\$1; in emulation ~\$10; post-Si ~\$1000. Any bug class that *can* be caught pre-Si *must* be. The plan is how you guarantee coverage of those classes.
2. **The contract with the architect:** the **vplan** maps every spec "shall" → a test → a coverpoint. If a coverpoint isn't covered, the feature **isn't validated**, no matter how many random tests passed. The vplan is the auditable record of "what done means."

### 2. The test-plan-from-spec scaffold (memorize this — it's the centerpiece)

Deliver it as a numbered walk, every time:

```
1. SPEC          Read the spec; list every "shall" / mode / register / interface.
2. FEATURES      Decompose into a feature list (what the block must do).
3. STIMULUS      For each feature: how do I drive it? (directed + constrained-random
                 + real traffic). What are the inputs and their legal ranges?
4. CHECKS        How do I know it's right? (reference model / scoreboard / assertions
                 / protocol checkers). Every feature needs a checker, not just a run.
5. COVERAGE      Define the coverage model: coverpoints, bins, crosses, transitions.
                 This is the "are we done" metric.
6. CORNER/NEG    Corner cases (full/empty, max/min, wrap) + negative/error tests
                 (illegal input, fault injection) — assert graceful handling.
7. PERF/POWER    Throughput, latency, bandwidth under load; power/PM states if relevant.
8. PRE/POST-Si   What's validated where: sim/emulation/formal pre-Si vs lab/ATE post-Si.
```

The mnemonic: **Spec → Features → Stimulus → Checks → Coverage → Corners → Performance → Platform.** Any "design a test plan for X" question is this scaffold instantiated. The interviewer is checking that you have a *method*, not that you guess clever individual tests.

### 3. Worked example — the Broadcom 16-port 1.6T switch (the exact failed question)

Apply the scaffold instead of inventing tests ad hoc:

| Step | For the switch |
|---|---|
| **Features** | ingress parsing, **forwarding/lookup** (MAC/IP table), **switching fabric/crossbar**, per-port **queues/buffering**, **QoS/scheduling** (priorities, WRR), flow control / backpressure, **CDC** (per-port clocks), error handling (CRC, malformed, MTU), counters/stats |
| **Stimulus** | traffic generators: unicast/multicast/broadcast; all port→port pairs; line-rate bursts; mixed packet sizes (min 64B … jumbo); priority mixes; congestion patterns (many→one) |
| **Checks** | scoreboard: every ingress packet egresses **once**, on the **right port**, **in order** within a flow, **uncorrupted**; latency within bound; no drops below congestion threshold; counters match |
| **Coverage** | cross **ingress-port × egress-port** (all 16×16), packet-size bins, priority bins, queue-occupancy bins (empty→full), multicast fan-out, CDC clock-ratio bins |
| **Corner/neg** | all-to-one congestion (**head-of-line blocking**), buffer **full** (must backpressure, not drop wrongly), simultaneous same-egress from all ports, runt/oversize/CRC-error frames → **dropped + counted**, table miss/overflow |
| **Perf** | sustained **1.6 Tbps** line rate, latency vs load curve, fairness under WRR, zero-loss point |
| **Pre/Post** | pre-Si: UVM env + traffic BFMs on sim/emulation; post-Si: hardware traffic generator (IXIA-class), real link partners |

The one-line framing: *"I don't invent tests — I decompose the spec into features, then for each feature define stimulus, a checker, and a coverage model, then layer corner cases, error injection, and performance, and finally split across pre-Si and post-Si."*

### 4. The vplan artifact — a traceable table

Every row maps **one spec requirement → test(s) → coverpoint(s) → status**:

| Spec ref | Feature | Test | Coverpoint | Status |
|---|---|---|---|---|
| §4.2 | unicast forwarding | `fwd_unicast_directed` + `_random` | `cp_in_port × cp_out_port` | 100% |
| §5.1 | buffer backpressure at full | `congest_many_to_one` | `cp_qdepth[full]` | 92% |
| §6.3 | CRC-error drop + count | `err_crc_inject` | `cp_drop_reason[crc]` | 100% |
| §7.x | CDC per-port clock ratios | `cdc_ratio_sweep` | `cp_clk_ratio` | 80% |

The vplan is **the** load-bearing artifact: it's what "coverage closure" closes, and what sign-off audits.

### 5. Coverage — the three flavours and the trap

| Type | Answers | Tool |
|---|---|---|
| **Functional** | did the *intended scenario* fire? (covergroup hits) | simulator covergroups |
| **Code** | did the *RTL* line/branch/toggle/FSM execute? | simulator coverage DB |
| **Assertion** | was each property *exercised*? (`cover property`) | simulator + formal |

**The trap (say it):** *100% code coverage with 40% functional coverage means you ran every line but never under the spec-intended scenario.* Code coverage is necessary (unexecuted RTL is untested) but **not sufficient** (executing a line ≠ checking the right behaviour in the right scenario). Functional coverage is the metric the architect cares about.

### 6. Functional coverage modeling — what to cover

```systemverilog
covergroup cg_switch @(posedge clk);
  ip : coverpoint in_port  { bins p[] = {[0:15]}; }
  op : coverpoint out_port { bins p[] = {[0:15]}; }
  sz : coverpoint pkt_size { bins small={64}; bins mid={[65:1500]}; bins jumbo={[1501:9000]}; }
  q  : coverpoint qdepth   { bins empty={0}; bins mid={[1:30]}; bins near_full={31}; bins full={32}; }
  x_route : cross ip, op;           // all 16×16 port pairs hit?
  x_load  : cross op, q;            // each egress seen at each occupancy?
endgroup
```

Cover **state** (every value/bin), **transition** (every legal state→state, e.g., FSM edges, queue empty→full→empty), and **cross** (combinations that matter — port×port, mode×size). The art is choosing coverpoints that map to *real risk*, not crossing everything (coverage explosion).

### 7. Code coverage — what each metric catches and misses

| Metric | Catches | Misses |
|---|---|---|
| **Line** | unexecuted statements | wrong values, untaken combinations |
| **Branch** | untaken if/else, case arms | data-dependent bugs within a taken branch |
| **Toggle** | nodes that never flip 0↔1 (dead/stuck signals) | functional meaning |
| **FSM** | unvisited states / untaken transitions | illegal-transition *absence* |
| **Condition/expression** | sub-conditions never independently true/false | scenario intent |

Code coverage finds **untested RTL**; functional coverage finds **untested intent**. You need both, plus assertions for the "shall never" cases.

### 8. Coverage closure — the CDV loop

```
   write vplan → write tests → run regression → read coverage report
        ▲                                              │
        └──────── add tests where coverage is low ◄────┘
```

**Closure criteria:** every vplan coverpoint at 100% (or explicitly **excluded/waived** with justification), every assertion `cover` hit ≥ once, every CR resolved or waived, code coverage at target with reviewed exclusions. **Holes** (uncovered bins) drive new directed tests or new constraints. **Exclusions** (legitimately unreachable bins) are reviewed and signed — not silently dropped.

### 9. Directed vs constrained-random vs formal — pick the right engine

| Method | Best for | Coverage role |
|---|---|---|
| **Directed** | known corners, specific bugs, smoke tests | hits a *specific* coverpoint deterministically |
| **Constrained-random (CRV)** | volume, unexpected interactions, closure | fills the coverage space; constraints keep it legal |
| **Formal** | "shall never" properties, small control logic, FSM deadlock, CDC | exhaustive proof, no stimulus needed |

The modern flow is **coverage-driven**: CRV bulk-fills coverage, directed tests plug the holes CRV can't reach, formal proves the absolute invariants. Map each spec requirement to the cheapest engine that closes it.

### 10. Delivering it verbally (the recurring interview opener)

When asked "write a test plan for *X*," **never start listing random tests.** Say: *"Let me work top-down from the spec. First the feature list… for each feature, stimulus and a checker… then a coverage model… then corner and error cases… then performance… and finally what's pre-Si vs post-Si."* Then instantiate. Structuring the answer **is** the signal — it's the difference between "I know some tests" and "I have a verification methodology." This single habit fixes the Broadcom/Cerebras/Amazon pattern.

---

## Anti-patterns / Bugs in this Topic

| Anti-pattern | Why it's dangerous | Fix |
|---|---|---|
| **Tests with no checker** | green regressions that verify nothing | every test asserts/scoreboards an outcome |
| **100% code, low functional** | ran the lines, never the scenario | drive functional coverage to closure |
| **No negative/error tests** | only happy path validated | inject illegal/fault inputs, assert handling |
| **Vplan not traced to spec** | features silently unvalidated | every spec "shall" → a vplan row |
| **Coverage crossing everything** | explosion, never closes | cross only risk-relevant axes |
| **Perf never validated** | functionally correct, misses line rate | throughput/latency-under-load tests |
| **Silent exclusions** | real holes hidden as "excluded" | review + justify every exclusion |

---

## Interview Explanation (60–90 sec)

> "I never write a test plan from scratch — I run a fixed scaffold from the spec. I read the spec and extract a feature list; for each feature I define the stimulus to drive it, a checker to know it's right — a reference model, scoreboard, or assertion — and a coverage model with coverpoints, bins, and the crosses that matter. Then I layer corner cases and negative tests with fault injection, then performance like throughput and latency under load, and finally I split what's validated pre-silicon in sim, emulation, and formal versus post-silicon in the lab. For a 16-port switch, that's: features like forwarding, queuing, QoS, and CDC; stimulus of all port-to-port traffic at mixed sizes and congestion; checks that every packet egresses once on the right port in order; coverage crossing ingress port by egress port by queue occupancy; corner cases like many-to-one head-of-line blocking and buffer-full backpressure; error injection of CRC and malformed frames; and a 1.6-terabit line-rate performance run. On coverage, the trap I always call out is that a hundred percent code coverage with low functional coverage means I executed every line but never under the intended scenario — code coverage is necessary, functional coverage is what says we're actually done. I close coverage with a constrained-random bulk fill, directed tests for the holes, and formal for the 'shall never' properties."

---

## Architect Drills (derive, no answer key)

1. Build a verification plan for the **async FIFO** from [Day 14](day14_async_fifo_cdc_verification.md) using all 8 scaffold steps. What are the corner and error tests?
2. Build a plan for the **ECC unit** from [Day 15](day15_memory_systems_ecc.md): what do you inject, what do you cover, what do you assert?
3. You have 100% code coverage and the chip still has a forwarding bug. Explain how, and what coverage would have caught it.
4. Define a covergroup for a 4-state FSM ([Day 9](day09_fsm_sequence_detectors.md)) that covers every **legal transition** and flags any illegal one.
5. When do you reach for formal instead of constrained-random? Give two switch/FIFO properties formal proves cheaply.
6. A regression is "all green" but verifies nothing. What's the smell, and how do you audit for checker-less tests?
7. Decide directed vs CRV vs formal for: a specific known corner, coverage closure on a big crossbar, and "the FIFO never overflows."

---

## Coding A — `d24_vplan_coverage`

Build a **coverage tracker in C** that models the CDV loop: define coverpoints with **bins** and a **cross**, feed a randomized stimulus stream through it, and report **per-bin hit counts**, overall **coverage %**, and the **uncovered holes** — then show a directed test closing a named hole. Add a tiny **vplan struct** (spec-ref → test → coverpoint → status) and a checker that fails if any "covered" row has 0 hits. Full spec in [coding/EXERCISES.md](../coding/EXERCISES.md#day-24--verification-planning--coverage).

**Interview prompts:** Recite the 8-step scaffold. Why is 100% code coverage insufficient? What's a coverage hole vs a legitimate exclusion?

---

## Bug Anchor / cross-links

- **Generalizes:** [Day 23](day23_cpu_bringup_functional_val.md) (vplan/coverage deliverables), [Day 30](day30_pre_si_pm_validation.md) (PM coverage axes), [Day 36](day36_pcie_bringup_functional_val.md) (PCIe functional val).
- **Applies to:** [Day 9](day09_fsm_sequence_detectors.md) (FSM state/transition coverage), [Day 14](day14_async_fifo_cdc_verification.md) (FIFO/CDC plan), [Day 15](day15_memory_systems_ecc.md) (ECC/MBIST plan).
- **Feeds:** [Day 25](day25_testbench_assertions_debug.md) (the testbench + assertions + debug that execute this plan), [Day 41](day41_final_mock.md) (mock — deliver the scaffold under time).

---

## Exit Check

- [ ] Recite the 8-step scaffold (Spec→Features→Stimulus→Checks→Coverage→Corners→Perf→Platform) cold.
- [ ] Deliver a full verification plan for the 16-port switch in under 3 minutes.
- [ ] Explain functional vs code vs assertion coverage and the 100%-code trap.
- [ ] Write a covergroup with bins, a cross, and a transition coverpoint.
- [ ] State the coverage-closure criteria and the hole-vs-exclusion distinction.
- [ ] Choose directed vs CRV vs formal for three given goals.
- [ ] Catch a checker-less "all green" regression.

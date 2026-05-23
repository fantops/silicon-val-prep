# Day 16 — Pre-Silicon PM Validation

## Concept

Pre-silicon PM validation is the discipline of proving — *before* tape-out — that every defined low-power state can be entered cleanly, held without corruption, and exited correctly on every wake source. The unit of closure is not "does the chip work" but **does every cell in the 3-D coverage matrix get hit at least once, and does every assertion fire and pass on every hit.** The matrix axes are **state × transition × residency-class × wake-source × cross-IP context**. PM validation is harder than functional validation because the bugs are sequencer-level (timing of SAVE/ISO/SLEEP/RESTORE) and they only surface when UPF is in the loop.

## Why this day in the flow

D12 named the states. D13 named the package + idle states. D14 named the cells that physically implement them. **Today is when the val engineer builds the vplan, the LP-sim env, and the coverage that proves those states actually work.** Tomorrow (D17) we move to post-Si — where the same states have to be measured on real silicon with telemetry instead of waveforms.

This is the day that maps most directly to a CPU IP Functional Val JD's bullets on PM coverage, scenario-based testing, and Perspec.

---

## Deep Study Notes

### 1. The 3-axis PM coverage frame

The PM verification plan should always be drawn as a cross-product:

```
                  STATE  ×  TRANSITION  ×  RESIDENCY-CLASS
                    │              │             │
            (C1,C6,C7,    (entry, exit,    (short <1ms,
             S0i1,S0ix,    abort, denial)    typical 1-100ms,
             D3hot,                          long >100ms)
             L1.2,
             PkgC6...)
```

Plus secondary axes:

- **Wake source** — IRQ, timer, IPI, DMA completion, debug, watchdog, PMIC fault.
- **Cross-IP context** — what other IPs are in what state when this transition happens.
- **PVT context** — fast-hot, slow-cold, typical, low-V, high-V, V-droop.
- **Workload context** — idle, periodic, bursty, sustained.

A "covered" PM feature means **every cell in this cross-product has at least one passing test.** A typical CPU IP has thousands of cells. Closure is by automation: random tests stuff cells, directed tests pick off the gaps the report reveals.

### 2. The PM verification plan (PM-vplan)

Each row in the PM-vplan maps **one PM scenario** → **stimulus + scoreboard + coverpoints + assertions**.

| Scenario | Stim | Score | Coverpoints | Assertions |
|---|---|---|---|---|
| C6 entry on idle, wake on timer | MWAIT idle thread; timer 100µs | regs restored bit-exact | `cp_cstate=C6 x cp_wake=TIMER` | `a_save_before_iso`, `a_restore_after_rail_stable` |
| C6 entry aborted by IRQ during entry | MWAIT, fire IRQ 50ns after entry begin | no state loss, FSM returns to C0 | `cp_pm_abort x cp_phase[Q,CG,SAVE,ISO,SLP]` | `a_abort_safe_at_every_phase` |
| PkgC6 with all cores C6 + GPU off + PCIe L1.2 | aligned idle | rail collapses in pkg | `cp_pkgc6 x cp_pcie_l12 x cp_dram_sr` | `a_pkgc_only_when_all_blockers_clear` |
| PM during DMA | start large DMA, request CPU C6 | C6 denied or aborted cleanly | `cp_pm_deny_reason` | `a_no_pd_with_pending_dma` |

A real vplan for a CPU IP is 200-500 rows. The Intel-style metric is **vplan closure %** — coverpoints hit divided by total.

### 3. Three classes of pre-Si PM test

| Class | What it does | Tool |
|---|---|---|
| **Directed PM** | Hand-coded sequence: "enter C6, wake on timer." One per row of vplan. | UVM testcase |
| **Random PM stress** | Constrained-random burst of state requests + wake sources | UVM + coverage feedback |
| **Scenario / Perspec** | Portable Stimulus model that generates SoC-wide PM scenarios reusable in sim, emul, post-Si | Synopsys Perspec / Cadence Perspec |

Perspec is mandatory because the SoC-level PM scenarios (CPU+GPU+PCIe coordinated entry) cannot be expressed at unit-UVM level. Perspec emits the same scenario as a sim testcase, an emulation transactor, and a post-Si C program — **one model, three targets.**

### 4. UPF-aware simulation (LP-sim)

Without UPF, `always_on` and `power_gated` behave identically — the regression passes for the wrong reason. LP-sim corrupts power-gated nets to X and enforces:

- Reads from a powered-down domain return X.
- Iso cells clamp outputs to the UPF-declared value when active.
- Retention flops do not capture unless SAVE asserted; do not output unless RESTORE asserted.
- Level-shifters X-corrupt if missing.

**Closure criterion:** the LP regression must pass at the same rate as the non-LP regression, but with every PM transition cell hit.

### 5. SVA for PM — the must-have assertions

```systemverilog
// SAVE must complete before ISO asserts
property p_save_before_iso(p);
  @(posedge clk) $rose(p.iso_en) |-> p.save_done;
endproperty
assert property (p_save_before_iso(cpu)) else $error("ISO before SAVE: state lost");

// RESTORE must wait for rail-stable
property p_restore_after_rail;
  @(posedge clk) $rose(restore_en) |-> rail_stable;
endproperty
assert property (p_restore_after_rail);

// No traffic into a power-gated domain
property p_no_traffic_when_off;
  @(posedge clk) !rail_on |-> !bus_req_to_block;
endproperty
assert property (p_no_traffic_when_off);

// PkgC6 only when all blockers clear
property p_pkgc_blockers;
  @(posedge clk) pkgc6_entry |-> (&core_in_c6) && pcie_in_l12 && dram_in_sr;
endproperty
assert property (p_pkgc_blockers);

// Wake source must be enabled at PCU before entry
property p_wake_armed;
  @(posedge clk) $rose(c6_entry) |-> |wake_mask;
endproperty
assert property (p_wake_armed) else $error("Entered C6 with no wake source armed - lockup");

// Residency counter must monotonically increase
property p_resid_monotonic;
  @(posedge clk) (state == C6) |-> resid_cnt >= $past(resid_cnt);
endproperty
assert property (p_resid_monotonic);
```

The PM val team owns a **bind file** that hangs these assertions on the PCU and every PM-aware IP. The number of binds × number of instances is the assertion budget.

### 6. PCU opcode tracing on Zebu

The PCU runs a small firmware (microcode + ROM) that implements the PM sequencer. In emulation (Zebu/Palladium) the val team captures a **PCU opcode trace** — per-cycle stream of opcodes executed by the PCU.

```
   cycle    opcode             arg          notes
   1024     PMC_REQ_C6         core=2       PM_OK_CACHES
   1025     PMC_QUIESCE        cluster=0
   1037     PMC_SAVE_REG       bank=GIC_RD
   1041     PMC_ISO_EN         dom=PD_CPU2
   1050     PMC_HEADER_OFF     chain=0
   1052     PMC_HEADER_OFF     chain=1
   ...
   12480    PMC_WAKE_REQ       src=TIMER
   12481    PMC_HEADER_ON      chain=0
   12498    PMC_RAIL_STABLE    dom=PD_CPU2
   12500    PMC_ISO_DIS        dom=PD_CPU2
   12502    PMC_RESTORE        bank=GIC_RD
   12508    PMC_CLK_RELEASE
```

**This trace is the load-bearing artifact for PM debug.** Every PM bug is read off the trace: missing opcode, wrong order, wrong delay, wrong domain.

### 7. Cross-IP PM negotiation — the deadlock farm

Bugs that need two IPs to misbehave together:

| Scenario | Failure mode |
|---|---|
| CPU requests C6; PCIe is in L0 with pending TLP | CPU C6 denied — but PCIe drains and then nothing wakes CPU because wake mask was disarmed during the deny |
| CPU enters PkgC6; PCIe rootport tries to exit L1.2 to deliver MSI | rootport CLKREQ# blocked by PkgC6 → MSI dropped |
| GPU wants D3cold; DMA buffer in DDR is held by DRAM self-refresh exit timer | GPU PD blocked indefinitely |
| Two cores try to enter C6 simultaneously, both vote for PkgC6 in same cycle | race in pkg blockers register; one core gets PkgC6 view, other does not |

**Coverage:** cross-product matrix of `(IP-A-state × IP-B-state × negotiation-event)`. Closure requires deliberate scenarios — random won't find these without bias.

### 8. The 4-phase pre-Si PM val program

| Phase | When | Goal | Exit gate |
|---|---|---|---|
| **P1: Unit LP-sim** | Block-level RTL stable | Every PM state of every IP entered + exited in isolation | Unit PM coverpoints 100%; all unit PM SVAs pass |
| **P2: Subsystem LP-sim** | Subsystem integration | Cross-IP negotiation under directed scenarios | Subsystem PM coverpoints 100%; deadlock scenarios covered |
| **P3: SoC LP-sim + Perspec** | SoC top stable | SoC-wide scenarios from Perspec model | All Perspec PM scenarios pass; coverage closed |
| **P4: Emulation** | Pre-tape-out | Boot Linux on emul, exercise governors, real residency | Real-SW PM works; PCU opcode trace clean |

P4 is the bridge to post-Si. Anything not caught by P4 *will* surface in lab.

### 9. PM regression triage workflow

```
   nightly LP regression
            │
            ▼
   coverage merge + assertion-failure DB
            │
            ▼
   for each new failure:
     ├─ pull waveform window (Verdi)
     ├─ pull PCU opcode trace
     ├─ correlate cycle of assertion fire vs PCU step
     ├─ classify: design / UPF / testbench / FW
     └─ open CR
            │
            ▼
   weekly: coverage gap review
            │
            ▼
   add directed tests for uncovered cells
```

A healthy PM val team has **closure burndown charts** for coverage and CR-by-severity.

### 10. The hard-to-find bug classes

| Class | Why hard | Catch with |
|---|---|---|
| **Race in PM sequencer** | Only fires when wake arrives in the 2-cycle window between SAVE and ISO | Random PM stress with wake-jitter; SVA on every phase |
| **Cross-IP deadlock** | Needs A waiting on B and B waiting on A in same micro-window | Directed cross-IP scenarios + livelock SVA |
| **Residency-counter wraparound** | 32-bit counter overflows in long idle test | Long-soak emul run + counter SVA |
| **Wake-source mask bug** | One source not unmasked in PCU FW; victim is one specific IRQ vector | Coverage on `(state × wake-src × IRQ-vec)` |
| **UPF iso/clamp polarity flip** | Functional sim passes, LP-sim fails only on specific state | LP-sim + LP-formal |
| **Sequenced-wake droop** | Only fires when neighbour IP is also waking | Multi-IP wake-burst stress + rail-monitor SVA |

---

## Bring-up Flow — PM Validation Program

```
Stage A — Spec phase
   ├─ Read PM architecture spec (every state, every transition, every wake)
   ├─ Read UPF intent (every domain, every iso/retention/LS strategy)
   └─ Write PM-vplan rows: 200-500 scenarios

Stage B — Stim + scoreboard
   ├─ Build PCU stimulator (drives entry requests)
   ├─ Build wake-source agents (timer, IRQ, DMA, debug)
   ├─ Build scoreboard: pre-entry state snapshot vs post-exit
   └─ Build PCU opcode tracer

Stage C — Assertion authoring
   ├─ Write bind file with PM SVAs
   ├─ Hang on every PM-aware IP + PCU
   └─ Run JasperGold/VC-Formal on FSM properties (entry-abort safety, deadlock freedom)

Stage D — Directed regression
   ├─ One test per vplan row
   ├─ Run with LP-sim ON
   └─ Coverage closure pass 1

Stage E — Random PM stress
   ├─ Constrained-random burst gen
   ├─ Coverage feedback closes residual cells
   └─ Triage new failures

Stage F — Perspec SoC scenarios
   ├─ Author SoC-wide PM scenarios in Perspec
   ├─ Run as sim testcases (slow but visible) + emul transactors (fast)
   └─ Closure on SoC PM coverpoints

Stage G — Emulation: real SW + governors
   ├─ Boot Linux on Zebu/Palladium
   ├─ Run governor (e.g., menu / intel_idle)
   ├─ Capture residency counters via OS interfaces
   └─ Sanity-match against expected residency under known workload

Stage H — Sign-off
   ├─ All PM coverpoints 100%
   ├─ All PM SVAs covered + passing
   ├─ Perspec PM scenarios all pass
   ├─ Emulation residency matches model within tolerance
   └─ Hand to post-Si team with PM-vplan + scenario library
```

---

## What's Required

- **PM architecture spec** + **UPF intent** + **PCU firmware spec**.
- **PM-vplan** (200-500 rows) maintained in a vplan tool (vManager / vPlanner / homegrown).
- **PCU stimulator** that issues entry requests in directed + random modes.
- **Wake-source library** with all real sources modelled.
- **Scoreboard** that compares pre-entry and post-exit architectural state bit-for-bit.
- **PCU opcode tracer** integrated into the testbench.
- **SVA bind file** with all PM properties.
- **Perspec model** of SoC PM scenarios.
- **LP-sim license** (VCS-NLP / Xcelium-LP) + **LP-formal license** (VC-LP / Conformal LP).
- **Emulation slot** (Zebu / Palladium) reserved for long-soak PM runs.
- **Coverage merge + dashboard** with burndown.

---

## Tools

| Phase | Tool | Use |
|---|---|---|
| **PM-vplan** | Synopsys vManager, Cadence vPlanner, Intel internal | Vplan auth + coverage tracking |
| **LP simulation** | Synopsys VCS-NLP, Cadence Xcelium-LP | UPF-aware functional sim |
| **LP formal** | Synopsys VC-LP, Cadence Conformal LP | Static UPF check; iso/retention proof |
| **FSM formal** | Synopsys JasperGold, Cadence JasperFV | Prove PCU FSM properties (safety, liveness) |
| **Stimulus** | UVM + Perspec | Directed + random + SoC scenarios |
| **Coverage** | Simulator built-in + merge tools | Functional + code + assertion |
| **Waveform** | Synopsys Verdi, Cadence SimVision | Debug, PCU opcode overlay |
| **Emulation** | Synopsys ZeBu, Cadence Palladium, Mentor Veloce | Long-soak; real-SW PM |
| **PCU trace** | Internal; sometimes exposed via DfD port | PCU opcode-level visibility |
| **Regression infra** | LSF / SGE / internal | Compute farm + nightly orchestration |

---

## Bugs in this Topic

- **Wake during SAVE → state lost:** wake event lands in the 1-cycle window after SAVE pulse but before ISO assert; PCU doesn't recognise it as in-progress; entry continues; on wake, restore reads junk. *Catch: random wake-jitter stress + `p_save_before_iso` SVA.*
- **PkgC6 entered with PCIe LTSSM not in L1.2:** PCIe FSM reported "in L1" but extension to L1.2 still pending; PkgC entry collapsed PCIe rootport refclk → link down on wake. *Catch: cross-IP coverage cell `(pkgc=ENTRY x pcie=L1)` + `p_pkgc_blockers` SVA.*
- **Residency counter never increments:** counter clock was itself gated by the PD it was counting. *Catch: counter clock must be AON; SVA `p_resid_monotonic` over long run.*
- **Wake mask zero on entry → infinite lockup:** firmware path failed to set wake mask; entry succeeded; nothing wakes. *Catch: `p_wake_armed`.*
- **Two cores race for PkgC6 vote:** both see all-other-cores-idle simultaneously; both write the pkg blocker register; one's value is lost. *Catch: directed cross-core test with cycle-skew sweep.*
- **Sequenced-wake droops neighbour rail:** all CPU header chains enabled in 4 cycles; ICCmax peaks; neighbour DDR PHY voltage dips below brown-out, DDR-trains-from-scratch on wake. *Catch: rail-monitor model + cross-rail SVA.*
- **Abort during entry leaves IRQ vector masked:** entry aborted at step 4; FW returns to C0 but didn't re-arm vector unmask. *Catch: abort-coverage cells; `(abort-phase × IRQ-vec)` cross.*
- **Coverpoint reaches 100% but is mis-specified:** covergroup defined `cp_cstate = {C0,C1,C6}` and skipped C7; report green; field finds C7-entry bug. *Catch: vplan + coverpoint peer review.*

---

## Interview Explanation (60-90 sec)

> "Pre-Si PM validation is a coverage closure program. You build a 3-D matrix of state × transition × residency, plus secondary axes on wake source, cross-IP context, and PVT, and you write a vplan with one row per scenario — typically 200 to 500 rows for a CPU IP. You stimulate via a PCU stimulator and a library of wake sources, score by snapshotting architectural state before entry and comparing bit-exact after exit, and you bind a set of mandatory SVAs: SAVE before ISO, RESTORE after rail-stable, no traffic into a powered-down domain, wake-mask non-zero before entry, residency monotonic. You run with UPF-aware LP-sim so power-gated nets are X-corrupted — without that the regression passes for the wrong reason. SoC-wide scenarios are authored in Perspec so the same scenario reuses across sim, emulation, and post-Si. The hardest bugs are cross-IP deadlocks and sequencer races that only fire when wake lands in a specific micro-window, so you complement directed tests with random PM stress under coverage feedback. Sign-off is every coverpoint at 100%, every SVA hit and passing, and a clean PCU opcode trace through long-soak emulation."

---

## Architect Drills

*PM val choices are tradeoffs between coverage, runtime, and visibility. Reason without an answer key.*

**D1. Random PM stress covers 90% of cells; the last 10% need directed tests. Reason about how to identify which 10% are *important* vs which are pathological corners not worth closing.**
*Constraints to weigh:* spec-mandated states vs implementation artifacts, customer workload realism, runtime cost of directed-test authoring, risk model.

**D2. Coverage is at 100% but a bug escapes to emulation. Reason about the process changes that prevent the next escape.**
*Constraints to weigh:* coverpoint definition rigor, missing cross-product axes, SVA gaps vs coverpoint gaps, whether the bug was reachable in pre-Si at all.

**D3. The team can run either (a) 10× more random sim cycles or (b) more emulation hours. Reason about which buys more PM-bug-find-rate.**
*Constraints to weigh:* sim depth-of-debug (waveform) vs emul speed (real SW), bug classes findable in each, sequencer races vs SW-PM-policy bugs.

**D4. PCU FSM has 60 states. Reason about whether to prove it correct with formal vs cover it with simulation.**
*Constraints to weigh:* formal explosion on 60-state FSM, what properties are tractable (safety vs liveness), what sim catches that formal misses (interaction with surrounding RTL), team formal skill cost.

**D5. A vplan row says "C6 entry with cross-cluster traffic." Reason about how to turn this into a concrete testcase + coverpoint + scoreboard.**
*Constraints to weigh:* what "cross-cluster traffic" means precisely, how to score correctness (no data loss + no deadlock + residency reached), what coverpoint cells the row implies.

**D6. Perspec scenarios are reusable across sim/emul/post-Si. Reason about what makes a scenario portable vs what locks it to one target.**
*Constraints to weigh:* abstraction level of the model, target-specific timing assumptions, scoring API differences, what "the same scenario" actually means across 3 orders of magnitude in speed.

**D7. The coverage dashboard shows 99% closure but the architect wants 100%. Reason about how to make the call between "close the last 1%" vs "waive with justification."**
*Constraints to weigh:* unreachable-coverpoint analysis, customer exposure of the uncovered scenario, schedule cost of closure, risk if it escapes.

---

## Coding A — Systems C: PM coverage tracker

A coverage merger for PM scenarios — same idea as a vplan dashboard. Reads scenario hits, emits closure %, identifies gaps.

```c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define NSTATE 6
#define NWAKE  5
#define NPHASE 4

static const char *S[NSTATE] = {"C1","C6","C7","S0i1","S0ix","PkgC6"};
static const char *W[NWAKE]  = {"TIMER","IRQ","IPI","DMA","DBG"};
static const char *P[NPHASE] = {"QUIESCE","SAVE","ISO","SLEEP"};

/* coverage matrix: state × wake × phase  (entry-abort scenario) */
static uint32_t cov[NSTATE][NWAKE][NPHASE];

static void hit(int s, int w, int p) { cov[s][w][p]++; }

static void report(void) {
    int total = NSTATE*NWAKE*NPHASE, hit_cells = 0;
    for (int s = 0; s < NSTATE; s++)
        for (int w = 0; w < NWAKE; w++)
            for (int p = 0; p < NPHASE; p++)
                if (cov[s][w][p]) hit_cells++;
    printf("PM coverage: %d / %d  = %.1f%%\n",
           hit_cells, total, 100.0 * hit_cells / total);

    printf("Gaps:\n");
    for (int s = 0; s < NSTATE; s++)
        for (int w = 0; w < NWAKE; w++)
            for (int p = 0; p < NPHASE; p++)
                if (!cov[s][w][p])
                    printf("  %s × wake=%s × abort@%s\n", S[s], W[w], P[p]);
}

int main(void) {
    /* simulated regression hits */
    hit(1,0,0); hit(1,0,1); hit(1,0,2); hit(1,0,3);  /* C6 × TIMER × all phases */
    hit(1,1,1); hit(1,2,2);                          /* a couple more */
    hit(5,0,3); hit(5,3,2);                          /* PkgC6 */
    report();
    return 0;
}
```

---

## Bug Anchor

- **Day 13 (C-states):** every C-state spec'd there must have a PM-vplan row here.
- **Day 14 (UPF):** every iso/retention strategy must have an SVA here.
- **Day 17 (post-Si PM val):** anything escaped here surfaces there as a lab bug.
- **Day 22 (PCIe PM intersection):** PkgC + PCIe L1.2 cross-IP scenarios live in both.
- **Master bug story (ADR opcode ordering):** caught by PCU-opcode-trace + cross-IP coverage cell.

## Exit Check

- [ ] Drew the 3-axis PM coverage matrix.
- [ ] Named 6 mandatory PM SVAs.
- [ ] Stated the difference between directed PM, random PM stress, and Perspec.
- [ ] Explained why LP-sim is mandatory (functional sim passes for the wrong reason).
- [ ] Described a PCU opcode trace and how it is used in PM bug triage.
- [ ] Named two cross-IP PM bug classes and how to cover them.
- [ ] Recited the 8-stage pre-Si PM val program with exit gates.
- [ ] Can in 90 sec explain what "PM coverage closed" means for sign-off.

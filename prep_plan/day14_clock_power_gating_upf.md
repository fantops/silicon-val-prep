# Day 14 — Clock Gating, Power Gating, Retention, Isolation, UPF

## Concept

A modern SoC saves power by **removing activity** (clock gating), **removing leakage** (power gating), **keeping a small subset of state alive across a power-down** (retention), and **preventing X-propagation when a block is off** (isolation + level-shifters). The **Unified Power Format (UPF)** is the language that tells the synthesis, P&R, and verification tools where each of these structures must go. Without UPF, the design works in functional simulation and fails the moment a real chip tries to turn anything off. **UPF is the pre-Si contract for power.**

## Why this day in the flow

D12 introduced DVFS and the P-state knob; D13 introduced C-states and S0ix. Both of those rely on the *physical* mechanisms invented today — clock gating cells, header/footer transistors, retention flops, isolation cells. **You cannot validate a C-state if you don't know what physical structure the C-state turns off.** This day is the bridge from architectural PM (states) to circuit-level PM (gates).

In the Layer map this is the **first concrete day of Phase 3 (CPU PM)** — every following PM day (D16 pre-Si val, D17 post-Si val) refers back to today's cells.

---

## Deep Study Notes

### 1. Clock gating — the cheapest PM lever

**Idea:** if a flop's D-input doesn't change this cycle, don't toggle its clock — save the CV²f on the clock tree and the flop itself.

#### Three forms

| Form | Where | Save |
|---|---|---|
| **RTL-implicit** | `if (en) reg <= d;` — synthesiser infers ICG | Per-flop |
| **Module-level explicit ICG** | Designer instantiates `CLKGATE` cell at module clock root | Per-module subtree |
| **Hierarchical / clock-tree-level** | Inserted by CTS at major clock-tree branches | Per-domain |

#### Integrated Clock Gating cell (ICG)

```
       ┌─────────────┐
en ───►│  latch (L)  │── enL ──┐
       └──────▲──────┘         │
              │                ▼
              clk ─────────► AND ──► gated_clk
```

The latch is **transparent low** to prevent glitches when `en` changes while `clk` is high. Hand-coded `clk & en` AND-gates are forbidden because `en` can glitch and produce a runt clock pulse.

#### Why coverage matters

A flop that's never enabled in a given workload contributes nothing to switching power. **Functional coverage on the enable signal** tells the PM team whether the gating is doing real work or sitting idle.

### 2. Power gating — removing leakage

**Idea:** clock gating only saves dynamic power; leakage continues. To kill leakage, **disconnect the rail entirely** using a series transistor (a *header* or *footer*).

```
   VDD ────[ HEADER PMOS ]────► VDD_virtual ──► logic block
                ▲
                │
            sleep_n (from PCU)

   logic block ──► VSS_virtual ──[ FOOTER NMOS ]──► VSS
                                       ▲
                                       │
                                   sleep
```

| | Header (PMOS) | Footer (NMOS) |
|---|---|---|
| **Where** | Between VDD and block | Between block and VSS |
| **Off-state leakage cut by** | ~100-1000× | ~100-1000× |
| **Area cost** | ~5-10% of cells | ~5-10% |
| **IR-drop during wake** | Worse (PMOS Rds higher) | Better |
| **Typical use** | Most CPU cores | Memories, lower-voltage IPs |

**Sleep-transistor sizing** is a tradeoff: large enough to not droop the virtual rail during peak activity, small enough to not blow area. Sequenced wake is mandatory — turning all headers on at once draws a current spike that can crash neighbouring rails.

### 3. Retention — keeping a tiny island alive

When a block powers down, every flop loses state. Two options:

| Option | Cost | When |
|---|---|---|
| **Save/restore to memory** | SW cost (microseconds), often acceptable for cold blocks | Cores in deep C-states, peripherals |
| **Retention flops** | Area + leakage of always-on cell pair | Hot critical state: SCU, GIC redistributor, MMU regs |

#### Retention flop structure

```
            Power-gated rail (VDD_v)        Always-on rail (VDDA)
                 │                                │
            ┌────▼────┐                    ┌──────▼──────┐
   D ─────►│ main    │── restore on wake ─│ shadow      │
   clk ───►│ flop    │── save on sleep ──►│ latch       │
            └────▲────┘                    └─────────────┘
```

Save signal pulled high before header off; restore pulled high after wake before clk resumes. **Save/restore sequencing is the #1 source of retention bugs.**

### 4. Isolation — preventing X-propagation

When a block is off, its outputs are floating (X in simulation, real silicon = unpredictable Vt). These outputs drive into a block that *is* on — without isolation, the always-on block sees garbage.

#### Isolation cell

```
   block_out ──┐
                ├── AND/OR ──► clamped_out  (to always-on consumer)
   isol_en ────┘
```

- **AND-clamp (clamp-to-0):** output forced low when block off. Use when consumer expects 0 = inactive.
- **OR-clamp (clamp-to-1):** output forced high when block off. Use when consumer expects 1 = inactive (active-low signals).

**Where placed:** on the always-on side of the boundary. Isolation cells themselves must run from the always-on rail.

### 5. Level-shifters — crossing voltage domains

Different power domains run at different voltages (e.g., CPU at 0.8 V, IO at 1.8 V). A signal driven by the low-V domain cannot be reliably interpreted by the high-V domain — gate-source voltage is too low to switch the receiver.

```
                LO-V ──► [ LVL_SHIFTER ] ──► HI-V
```

| Type | Direction | Use |
|---|---|---|
| **Low-to-high shifter** | LV → HV | Most common |
| **High-to-low shifter** | HV → LV | Sometimes just a normal buffer if LV >= V_t |
| **Bidirectional** | Either | IO pads |

Always-on level-shifters that span retention also need isolation behaviour built in.

### 6. UPF — the power-intent file

UPF (IEEE 1801) is a Tcl-style file that declares:

```tcl
# Define power domains
create_power_domain PD_CPU -elements {cpu_top}
create_power_domain PD_AON -elements {aon_top}

# Define supply nets
create_supply_net VDD_CPU
create_supply_net VDD_AON
create_supply_net VSS

# Map domain to primary supply
set_domain_supply_net PD_CPU -primary_power_net VDD_CPU -primary_ground_net VSS

# Switch (power gate)
create_power_switch PSW_CPU \
    -domain PD_CPU \
    -input_supply_port  {in VDD} \
    -output_supply_port {out VDD_CPU} \
    -control_port {sleep_n PCU_SLEEP_N} \
    -on_state  {ON  in {sleep_n}} \
    -off_state {OFF    {!sleep_n}}

# Isolation strategy
set_isolation iso_cpu -domain PD_CPU \
    -isolation_signal PCU_ISO_EN -isolation_sense high \
    -clamp_value 0 -applies_to outputs

# Retention strategy
set_retention ret_cpu -domain PD_CPU \
    -retention_supply_net VDD_AON \
    -save_signal  {PCU_SAVE  high} \
    -restore_signal {PCU_RESTORE high}

# Level-shifter strategy
set_level_shifter ls_cpu_to_aon -domain PD_CPU \
    -applies_to outputs -rule low_to_high -location parent
```

**This file is read by:**
- Synthesis (DC, Genus): inserts ICG / iso / LS cells.
- P&R (ICC2, Innovus): builds power switches, sequences enable chains.
- LP simulation (VCS-NLP, Xcelium-LP): models X-corruption when block is off, validates iso/retention/LS in functional tests.
- Verification (VC-LP, Conformal LP): formally proves iso/LS placement and retention coverage.

**Without UPF, the design simulates fine but is unsynthesisable as a low-power chip.**

### 7. Sequenced power-down — the order matters

```
   power-down request from PCU
            │
            ▼
   1. Quiesce traffic        (drain pending requests; HW handshake)
            │
            ▼
   2. Clock gate             (stop new transitions)
            │
            ▼
   3. Save retention         (assert SAVE; retention flops capture)
            │
            ▼
   4. Assert isolation       (clamps go live)
            │
            ▼
   5. Open header switch     (rail collapses to ~0 V)
            │
            ▼
   PD complete, residency counter ticks
```

**Wake reverses the order:**

```
   wake event
       │
       ▼
   1. Close header           (sequenced wake: cells turned on in chains
                              with delay between each chain to limit di/dt)
       │
       ▼
   2. Wait for rail stable   (V monitor crosses threshold)
       │
       ▼
   3. Deassert isolation     (real signals now reach consumers)
       │
       ▼
   4. Assert restore         (retention flops copy shadow → main)
       │
       ▼
   5. Release clock gate     (clk resumes)
       │
       ▼
   6. Resume traffic
```

**Every bug in PM lives in this sequencer.** Wrong order, wrong delay, missing handshake → corruption, hang, droop.

### 8. Always-on (AON) domain — the survivor

Everything that must observe the SoC while it's mostly off lives in AON:
- **Wake controller** (which events count as wake?)
- **Real-time clock**
- **PMIC interface** (so SW can change rail voltages)
- **PCU/PMC ROM + state machine**
- **GPIO wake pins**
- **Retention rails for the rest of the SoC**

AON is small (kBs of state, tens of kHz clocks), so its own leakage doesn't matter.

### 9. UPF mismatch bugs — the classic class

| Bug | Symptom | Root cause |
|---|---|---|
| **Missing isolation cell** | After wake, always-on block sees Xs in sim, garbage on silicon | UPF didn't list the output net |
| **Wrong clamp polarity** | Block off → consumer sees "active" — kicks off spurious work | clamp_value 1 where consumer is active-high |
| **Retention not asserted** | After wake, regs come up as 0, FW thinks "first boot" | SAVE signal not connected in UPF strategy |
| **Level-shifter missing** | Sporadic functional failure across voltage corner | Cross-domain net not flagged in UPF |
| **Wake sequence too fast** | Rail droops below brown-out, neighbour resets | Sequenced wake chain delays too short |
| **Iso released before rail stable** | Glitch propagates into AON | Sequencer step 2 → 3 with no rail-stable poll |

### 10. Pre-Si vs post-Si visibility

| What | Pre-Si visibility | Post-Si visibility |
|---|---|---|
| ICG enable per module | Full waveform, per-cycle | Indirect (activity counters, dynamic power) |
| Header on/off | Waveform + UPF state | PMIC telemetry, rail voltage scope |
| Retention save/restore | Waveform | Almost none — FW asserts state matches |
| Isolation assertion | Waveform | None — rely on absence of bugs |
| Sequenced-wake delay | Waveform, cycle-accurate | Scope on rail + EN signals |

**Implication:** retention + isolation bugs are *catastrophically* hard to catch post-Si. They must be closed in UPF + LP-sim with assertion coverage.

---

## Bring-up Flow — Power Domain (Pre-Si LP-Sim)

```
Stage A — UPF lint
   ├─ Run UPF lint (VC-LP / Conformal LP)
   ├─ Every cross-domain net has iso + LS rule
   └─ Every retention element has SAVE + RESTORE source

Stage B — Functional sim with NO power-aware
   ├─ Block functional regression passes
   └─ Baseline confidence

Stage C — Enable LP-sim with UPF
   ├─ Simulator corrupts power-gated nets with X when off
   ├─ Re-run regression — failures here are real iso/retention bugs
   └─ Triage every X-propagation

Stage D — Directed PM transition tests
   ├─ Force entry into each low-power state via PCU stimulus
   ├─ Force exit via each wake source
   ├─ Verify ARC: cover entry × exit × residency
   └─ Assert: no traffic between iso-on and rail-collapse

Stage E — Random PM stress
   ├─ Random entry + wake bursts under traffic
   ├─ Find ordering races
   └─ Coverage on transition matrix

Stage F — Cross-IP PM
   ├─ Two domains negotiate (e.g., CPU wants C6, PCIe in L1.2)
   ├─ Verify each side's quiesce handshake
   └─ Catch deadlocks (A waits for B, B waits for A)

Stage G — Sign-off
   ├─ All PM coverpoints at 100%
   ├─ Formal: iso placement + retention save/restore proved
   └─ Hand UPF + LP-sim regression to physical design
```

---

## What's Required

- **UPF 1801 file** with power domains, switches, isolation, retention, level-shifter strategies declared.
- **PCU/PMC reference model** that asserts the SAVE/RESTORE/ISO/SLEEP signals in the correct sequence.
- **Cell library** with ICG, header/footer, isolation, retention, LS cells characterised.
- **LP-aware simulator** (Synopsys VCS-NLP, Cadence Xcelium-LP) that can read UPF and corrupt power-gated nets.
- **LP formal tool** (Synopsys VC-LP, Cadence Conformal LP) for static UPF check.
- **PM verification plan** with one row per power domain × per low-power state × per transition.
- **Always-on logic spec** so the val team knows which signals *must not* glitch during PM transitions.

---

## Tools

| Phase | Tool | Use |
|---|---|---|
| **UPF auth** | Synopsys VC-LP, Cadence Conformal LP | UPF lint + formal proof of iso/retention coverage |
| **LP simulation** | Synopsys VCS-NLP, Cadence Xcelium-LP | Power-aware functional sim; X-corrupt OFF domains |
| **Synthesis** | Synopsys Design Compiler / Fusion, Cadence Genus | Insert ICG/iso/LS/retention cells per UPF |
| **P&R** | Synopsys ICC2, Cadence Innovus | Build power switches, header rings, AON islands |
| **Cell lib** | Foundry std-cell lib + IP-vendor PM lib | Characterised power cells |
| **PM trace** | UPF-aware waveform viewer (Verdi) | Visualise state per domain over time |
| **Emulation** | ZeBu / Palladium with power-aware mode | Boot OS, exercise real PM SW with UPF in the loop |
| **Post-Si** | Scope on rails + EN signals; PMIC i2c trace; SVTools PMC trace | Verify silicon matches UPF intent |

---

## Bugs in this Topic

- **Header on but logic stuck:** wake-rail-stable poll skipped; logic starts before V stabilises → metastability storm. *Fix: insert rail-monitor handshake.*
- **Block wakes with corrupted registers:** retention SAVE asserted *after* clock gate released → save captured already-junked state. *Fix: re-order PCU FSM; add SVA check.*
- **AON block latches X on wake:** isolation released before rail stable → glitch propagates. *Fix: tighten iso-deassert until rail-stable signal high.*
- **Sequenced wake too fast → brown-out:** all header chains enabled in 2 cycles, rail droops 15% → neighbour PMIC trips UV. *Fix: stretch chain delays; add per-chain timer.*
- **Clock gating fights debug:** during halt-mode debug, ICG turns off clk while Lauterbach is mid-read → bus hang. *Fix: debug request overrides ICG enable.*
- **Wrong clamp polarity on IRQ line:** clamp-to-1 on active-high IRQ → AON sees constant interrupt while block is off → fires every cycle, wakes block immediately, no PM. *Fix: clamp-to-0; cover IRQ-while-off in sim.*
- **Level-shifter missing on one net:** sporadic functional fail only at low-V corner. *Fix: UPF strategy must cover every cross-domain net; LP-formal catches this.*
- **Retention flop fails at SS-cold corner:** shadow latch loses state due to leakage in extreme corner. *Fix: characterise retention at all PVT corners; add margin or move state to memory save.*

---

## Interview Explanation (60-90 sec)

> "Clock gating removes dynamic power by stopping the clock to flops that aren't switching — implemented with ICG cells that latch the enable to avoid glitches. Power gating removes leakage by putting a header or footer transistor between the rail and the block — orders of magnitude saving but at the cost of state loss. To preserve a small critical state across a power-down, retention flops have a shadow latch on the always-on rail that captures on SAVE and restores on RESTORE. To prevent X-propagation from an off block into an on block, isolation cells clamp the boundary. Cross-voltage domains need level-shifters. All of this is declared in UPF, which is the IEEE-1801 file that tells synthesis, P&R, and LP-sim where every one of these cells goes. The sequencer that turns these on and off in the right order — quiesce, clock-gate, save, isolate, header-off — is owned by the PCU, and every bug in PM lives in that sequencer. The reason this matters for validation is that retention and isolation bugs are nearly invisible post-Si, so they have to be closed pre-Si with LP-sim and LP-formal coverage."

---

## Architect Drills

*Power-domain structure is full of tradeoffs. Reason about each one — no answer key.*

**D1. A block has 1000 flops. 50 hold critical state needed within 1 µs of wake; 950 hold data that can be reconstructed from memory in 50 µs. Reason about the right mix of retention vs save-to-memory.**
*Constraints to weigh:* retention-flop area + AON-leakage cost, save/restore SW latency, wake-time budget, manufacturing variation on retention at corners.

**D2. Why does clock gating save dynamic power but not leakage, while power gating saves both?**
*Constraints to weigh:* CV²f equation, sub-threshold leakage path, what "the clock is off" actually changes in a flop's power consumption.

**D3. Header (PMOS) vs footer (NMOS) sleep transistors — when do you pick which?**
*Constraints to weigh:* PMOS vs NMOS Rds_on, IR-drop on virtual rail under load, area, body-effect, where the noise-sensitive logic lives.

**D4. Reason about a system that has no UPF — it just simulates functionally with all rails always on. What classes of bug escape?**
*Constraints to weigh:* iso missing, retention sequencing, LS coverage, sequenced wake — which of these surface only in LP-sim?

**D5. The PCU FSM has 8 sequencer steps to enter PD. Reason about why you can't collapse SAVE and ISO-assert into one step.**
*Constraints to weigh:* what SAVE captures (current flop state) vs what ISO does (clamps output), what happens if iso fires before save completes vs after.

**D6. Retention flops cost area and always-on leakage. Reason about a future where they could be eliminated entirely.**
*Constraints to weigh:* fast save-to-SRAM (eDRAM, MRAM), per-block latency budgets, whether some state truly must survive at µs scale.

**D7. A bug shows up only at SS-cold-low-V corner: retention loses state. Reason about whether to fix in design (larger retention cell), in characterisation (tighten corner), or in PM policy (don't enter that PD at that corner).**
*Constraints to weigh:* area cost, yield impact, SW complexity of PVT-aware PM, customer-visible perf hit.

---

## Coding A — Systems C: UPF-style power-domain model

Model a 3-domain SoC (CPU, GPU, AON) with header switches, retention save/restore, isolation, and a sequencer. Asserts catch the classic ordering bugs.

```c
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

typedef struct {
    char     name[16];
    bool     rail_on;
    bool     iso_asserted;
    bool     save_done;
    uint32_t live_state;        /* what main flop holds */
    uint32_t shadow_state;      /* what retention latch holds */
} pd_t;

static void pd_init(pd_t *p, const char *n) {
    snprintf(p->name, sizeof p->name, "%s", n);
    p->rail_on = true; p->iso_asserted = false; p->save_done = false;
    p->live_state = 0; p->shadow_state = 0;
}

/* Correct power-down sequence */
static void pd_powerdown(pd_t *p) {
    /* 1. quiesce + clock gate handled by caller */
    /* 2. SAVE: capture live → shadow */
    p->shadow_state = p->live_state;
    p->save_done = true;
    /* 3. ISO assert */
    p->iso_asserted = true;
    /* 4. rail off */
    p->rail_on = false;
    p->live_state = 0xDEADBEEF;  /* simulate X corruption */
}

/* Correct wake sequence */
static void pd_wake(pd_t *p) {
    /* 1. rail on */
    p->rail_on = true;
    /* 2. wait for rail stable -- modelled implicitly */
    /* 3. RESTORE: shadow → live */
    assert(p->save_done && "RESTORE before SAVE!");
    p->live_state = p->shadow_state;
    /* 4. ISO release */
    p->iso_asserted = false;
    /* 5. clock release */
}

/* Consumer in always-on domain reading p's output */
static uint32_t aon_read(const pd_t *p) {
    if (p->iso_asserted) return 0;          /* clamp-to-0 */
    if (!p->rail_on)    return 0xBADBAD00;  /* iso missing -> garbage leaks */
    return p->live_state;
}

int main(void) {
    pd_t cpu; pd_init(&cpu, "CPU");
    cpu.live_state = 0xCAFEBABE;
    printf("AON reads CPU pre-PD: 0x%X\n", aon_read(&cpu));

    pd_powerdown(&cpu);
    printf("AON reads CPU during PD (iso ON): 0x%X  (expect 0)\n", aon_read(&cpu));

    pd_wake(&cpu);
    printf("AON reads CPU post-wake: 0x%X  (expect 0xCAFEBABE)\n", aon_read(&cpu));

    /* Bug demo: wake without prior save */
    pd_t bad; pd_init(&bad, "GPU");
    bad.live_state = 0x1234;
    /* operator forgot pd_powerdown -> save_done false */
    printf("Triggering RESTORE-before-SAVE assertion...\n");
    pd_wake(&bad);  /* will assert */
    return 0;
}
```

---

## Bug Anchor

- **Day 12 (DVFS):** voltage scaling drives the level-shifter need.
- **Day 13 (C-states):** every C-state above C1 power-gates *something* defined here.
- **Day 16 (pre-Si PM val):** the LP-sim + UPF-formal coverage closure happens against this day's cells.
- **Day 17 (post-Si PM val):** retention/iso bugs that escape pre-Si surface here as "boots fine, crashes on first wake."
- **Day 22 (PCIe PM intersection):** PCIe L1.2 entry uses the same iso/retention pattern at the IP boundary.

## Exit Check

- [ ] Drew an ICG cell and explained why the latch is needed.
- [ ] Stated the difference between header and footer sleep transistors.
- [ ] Stated when to use retention flops vs save-to-memory.
- [ ] Stated isolation clamp polarity rule (active-high consumer → clamp-to-0).
- [ ] Wrote a UPF snippet declaring a power domain with switch + iso + retention + LS.
- [ ] Recited the 5-step power-down sequence and the 6-step wake sequence.
- [ ] Named one bug per sequencer step and how LP-sim catches it.
- [ ] Stated why retention/iso bugs are uniquely dangerous post-Si.

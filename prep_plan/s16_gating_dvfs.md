# S16 - Gating and DVFS

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [D28 clock/power gating](archive/day28_clock_power_gating_upf.md) (concepts; UPF syntax optional), [D26 DVFS/P-states](archive/day26_dvfs_pstates_pvt.md).
- **Must answer:**
  - Clock gating cuts dynamic power; power gating cuts leakage (S1). Gating sequence: **drain → save/retain → isolate → cut**, then reverse on wake.
  - Accelerator view: in-flight work must **drain** before gating (the Drain operation and WQ disable exist for this). What if a job arrives mid-gating?
  - DVFS: going up, **V before f**; going down, **f before V**. HWP/CPPC, Turbo limits, fused V/F curves (S5).
- **Work link:** what software does at work before resetting or disabling a WQ (drain/abort), and why skipping it causes failures.
- **Handoff →** How does the whole package go idle?

## Study notes

### Concept - [day28_clock_power_gating_upf.md](archive/day28_clock_power_gating_upf.md)

A modern SoC saves power by **removing activity** (clock gating), **removing leakage** (power gating), **keeping a small subset of state alive across a power-down** (retention), and **preventing X-propagation when a block is off** (isolation + level-shifters). The **Unified Power Format (UPF)** is the language that tells the synthesis, P&R, and verification tools where each of these structures must go. Without UPF, the design works in functional simulation and fails the moment a real chip tries to turn anything off. **UPF is the pre-Si contract for power.**

### Deep Study Notes - [day28_clock_power_gating_upf.md](archive/day28_clock_power_gating_upf.md)

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

### Bugs in this Topic - [day28_clock_power_gating_upf.md](archive/day28_clock_power_gating_upf.md)

- **Header on but logic stuck:** wake-rail-stable poll skipped; logic starts before V stabilises → metastability storm. *Fix: insert rail-monitor handshake.*
- **Block wakes with corrupted registers:** retention SAVE asserted *after* clock gate released → save captured already-junked state. *Fix: re-order PCU FSM; add SVA check.*
- **AON block latches X on wake:** isolation released before rail stable → glitch propagates. *Fix: tighten iso-deassert until rail-stable signal high.*
- **Sequenced wake too fast → brown-out:** all header chains enabled in 2 cycles, rail droops 15% → neighbour PMIC trips UV. *Fix: stretch chain delays; add per-chain timer.*
- **Clock gating fights debug:** during halt-mode debug, ICG turns off clk while Lauterbach is mid-read → bus hang. *Fix: debug request overrides ICG enable.*
- **Wrong clamp polarity on IRQ line:** clamp-to-1 on active-high IRQ → AON sees constant interrupt while block is off → fires every cycle, wakes block immediately, no PM. *Fix: clamp-to-0; cover IRQ-while-off in sim.*
- **Level-shifter missing on one net:** sporadic functional fail only at low-V corner. *Fix: UPF strategy must cover every cross-domain net; LP-formal catches this.*
- **Retention flop fails at SS-cold corner:** shadow latch loses state due to leakage in extreme corner. *Fix: characterise retention at all PVT corners; add margin or move state to memory save.*

---

### Interview Explanation (60-90 sec) - [day28_clock_power_gating_upf.md](archive/day28_clock_power_gating_upf.md)

> "Clock gating removes dynamic power by stopping the clock to flops that aren't switching — implemented with ICG cells that latch the enable to avoid glitches. Power gating removes leakage by putting a header or footer transistor between the rail and the block — orders of magnitude saving but at the cost of state loss. To preserve a small critical state across a power-down, retention flops have a shadow latch on the always-on rail that captures on SAVE and restores on RESTORE. To prevent X-propagation from an off block into an on block, isolation cells clamp the boundary. Cross-voltage domains need level-shifters. All of this is declared in UPF, which is the IEEE-1801 file that tells synthesis, P&R, and LP-sim where every one of these cells goes. The sequencer that turns these on and off in the right order — quiesce, clock-gate, save, isolate, header-off — is owned by the PCU, and every bug in PM lives in that sequencer. The reason this matters for validation is that retention and isolation bugs are nearly invisible post-Si, so they have to be closed pre-Si with LP-sim and LP-formal coverage."

---

### Architect Drills - [day28_clock_power_gating_upf.md](archive/day28_clock_power_gating_upf.md)

*Power-domain structure is full of tradeoffs. Reason about each one — no answer key.*

**D1. A block has 1000 flops. 50 hold critical state needed within 1 µs of wake; 950 hold data that can be reconstructed from memory in 50 µs. Reason about the right mix of retention vs save-to-memory.**
*Constraints to weigh:* retention-flop area + AON-leakage cost, save/restore SW latency, wake-time budget, manufacturing variation on retention at corners.

**D2. Why does clock gating save dynamic power but not leakage, while power gating saves both?**
*Constraints to weigh:* CV²f equation, sub-threshold leakage path, what "the clock is off" actually changes in a flop's power consumption.

**D3. Header (PMOS) vs footer (NMOS) sleep transistors — when do you pick which?**
*Constraints to weigh:* PMOS vs NMOS Rds_on, IR-drop on virtual rail under load, area, body-effect, where the noise-sensitive logic lives.

**D16. Reason about a system that has no UPF — it just simulates functionally with all rails always on. What classes of bug escape?**
*Constraints to weigh:* iso missing, retention sequencing, LS coverage, sequenced wake — which of these surface only in LP-sim?

**D17. The PCU FSM has 8 sequencer steps to enter PD. Reason about why you can't collapse SAVE and ISO-assert into one step.**
*Constraints to weigh:* what SAVE captures (current flop state) vs what ISO does (clamps output), what happens if iso fires before save completes vs after.

**D18. Retention flops cost area and always-on leakage. Reason about a future where they could be eliminated entirely.**
*Constraints to weigh:* fast save-to-SRAM (eDRAM, MRAM), per-block latency budgets, whether some state truly must survive at µs scale.

**D19. A bug shows up only at SS-cold-low-V corner: retention loses state. Reason about whether to fix in design (larger retention cell), in characterisation (tighten corner), or in PM policy (don't enter that PD at that corner).**
*Constraints to weigh:* area cost, yield impact, SW complexity of PVT-aware PM, customer-visible perf hit.

---

### Exit Check - [day28_clock_power_gating_upf.md](archive/day28_clock_power_gating_upf.md)

- [ ] Drew an ICG cell and explained why the latch is needed.
- [ ] Stated the difference between header and footer sleep transistors.
- [ ] Stated when to use retention flops vs save-to-memory.
- [ ] Stated isolation clamp polarity rule (active-high consumer → clamp-to-0).
- [ ] Wrote a UPF snippet declaring a power domain with switch + iso + retention + LS.
- [ ] Recited the 5-step power-down sequence and the 6-step wake sequence.
- [ ] Named one bug per sequencer step and how LP-sim catches it.
- [ ] Stated why retention/iso bugs are uniquely dangerous post-Si.

### Concept - [day26_dvfs_pstates_pvt.md](archive/day26_dvfs_pstates_pvt.md)

Power has two pieces: **dynamic** $P_{dyn} = C V^2 F$ and **static** (leakage), with leakage **exponential in temperature**. **DVFS** exploits the $V^2$ term by lowering both $V$ and $F$ when full performance isn't needed. **P-states** are the discrete operating points; **Turbo** is the opportunistic above-P0 mode that uses thermal/power headroom from idle cores. Every chip lives at a unique **PVT corner** (Process × Voltage × Temperature); validation must cover the worst cases (FF for power/thermal, SS for timing). After fab, chips are **speed-binned** into SKUs, each with its own DVFS table.

### Deep Study Notes - [day26_dvfs_pstates_pvt.md](archive/day26_dvfs_pstates_pvt.md)

### 1. The power equation

Dynamic:

$$P_{dyn} = \tfrac{1}{2} C V^2 F$$

- **C** — total switched capacitance (gates + wires).
- **V** — supply voltage; *squared*, so a 20% V drop saves 36% dynamic power.
- **F** — switching frequency; linear.

Static:

$$P_{leak} = f(V, T) \quad \text{(roughly exponential in T)}$$

At 7 nm and below, **leakage is 30-40% of total** — and rises fast with $T$. The feedback loop is:

```
more leakage → more heat → even more leakage → thermal runaway
```

PROCHOT and DTS-driven throttling break the loop.

### 2. Why V and F are linked

Transistors need a minimum $V$ to switch reliably at a given $F$. Lower $V$ without lowering $F$ → setup violations → data corruption. Each silicon corner has its own **V-F curve**; the DVFS table is the curve quantised into discrete operating points (OPPs / P-states).

### 3. The DVFS loop and timing

```
Workload demand changes
   │
   ▼
OS power manager picks target P-state
   │
   ▼
Write MSR / CSR (e.g., x86 IA32_PERF_CTL, ARM CPRMAN/RPMh request)
   │
   ▼
HW: PLL relock (~1-10 µs) + VRM ramp (~10-100 µs)
   │
   ▼
New (V, F) operating point stable
```

**Voltage ramp is the bottleneck.** Sequence:

- **Going faster:** raise V *first* (creates timing margin), *then* raise F.
- **Going slower:** drop F *first* (gains timing slack), *then* drop V into the slack.

Reverse either and you get setup violations / data corruption.

### 4. P-states

Discrete (V, F) operating points exposed to the OS:

| P-state | Mode | Example F | Example V |
|---|---|---|---|
| P0 | max perf | 3.6 GHz | 1.15 V |
| P1 | high | 2.8 GHz | 1.05 V |
| P2 | medium | 2.0 GHz | 0.95 V |
| P3 | efficient | 1.2 GHz | 0.85 V |
| P4 | lowest active | 0.8 GHz | 0.75 V |

Linux: `cpufreq` framework + `intel_pstate` / `acpi-cpufreq` / `qcom-cpufreq-hw`. Windows: PPM. ARM/QC: vote into RPMh, which aggregates votes and programs PMIC.

### 5. Turbo — opportunistic above-P0

When **thermal/current headroom exists** AND **some cores are idle**:

- Idle cores burn near-zero dynamic power (clock-gated) and a fraction of leakage.
- Freed budget reallocates to active cores → they exceed P0 frequency.
- Hardware controls (PCU on Intel) — opportunistic, not deterministic.
- Hits a thermal wall (DTS) or current wall (Iccmax) → throttles back.

If throttling is constant, max-Q cooling is undersized or the silicon is in an unfortunate corner.

### 6. PVT — the variation reality

**Process variation:** transistors don't come out identical. Gate-oxide thickness, doping, channel length all vary across a wafer. The corners:

| Corner | NMOS | PMOS | Property |
|---|---|---|---|
| **TT** | Typical | Typical | design target |
| **FF** | Fast | Fast | fastest, hottest, most leakage — **worst for power/thermal** |
| **SS** | Slow | Slow | slowest, lowest leakage, may miss timing — **worst for timing** |
| **FS** | Fast | Slow | skewed pull-up vs pull-down delay |
| **SF** | Slow | Fast | opposite skew |

**Voltage variation:** VRM tolerance ±3-5%; IR drop across die; transient droop on load step. Validate at nominal, +10%, -10%.

**Temperature variation:** consumer 0-100 °C, server 0-105 °C, automotive -40 to 125 °C. Higher T → more leakage → typically slower transistors (temperature inversion can flip this at very low V).

**The corner explosion:**

```
5 process × 3 voltage × 3 temperature × 3 aging × multi-Vt → 200+ corners at 7 nm
```

Pre-silicon picks worst cases. Post-silicon finds what simulation missed (local mismatch, aging, real cooling).

### 7. Speed binning

After fab, every chip is tested. Sorted by max sustainable $F$ at a given $V$/T:

| Bin | SKU | Frequency |
|---|---|---|
| Top | Core i9 / 8cx Premium | 3.8+ GHz |
| Mid | Core i7 | 3.0-3.6 GHz |
| Lower | Core i5 | 2.4-3.0 GHz |
| Defect | discard / disable parts | — |

Each bin gets its own **DVFS table**. An FF chip can sustain higher P0 at lower V than an SS chip. Validation must verify the binned curve is safe for that bin's actual silicon.

### 8. ARM / Qualcomm DVFS — RPMh, PMIC, per-core frequency

On Snapdragon (Day 21 anchor), DVFS is voted into **RPMh**:

- Each requester (Apps cluster, Modem, ADSP, CDSP/NPU) sends `(active, sleep, wake)` votes.
- RPMh aggregates **max** per resource.
- Programs PMIC over SPMI.
- TCS sequences fire on idle entry/exit for fast wake.

**The catch on big.LITTLE / DynamIQ:** cores often share a voltage rail by cluster. Per-core *frequency* is real; per-core *voltage* is not — rail V = max across that rail's cores. Validation: characterise actual V = `max(per-core requests)` and find waste.

### 8a. Multi-SoC / multi-IP DVFS coordination (the AMD design probe)

*"Design the power/perf tradeoff for an NPU + CPU + multi-SoC system" — walk this 6-step scaffold. Steps 1-4 land here; steps 5-6 land in [Day 29 Sec 5a](archive/day29_thermal_power_perf_counters.md).*

```
1. WORKLOAD CHARACTERISATION  — who is hot when?
     CPU-bound bursts vs NPU sustained inference vs concurrent.
     Duty cycle, peak vs avg, latency-sensitive vs throughput-sensitive.
2. POWER BUDGET SPLIT          — TDP carved per agent
     Static split (fixed % per IP) vs dynamic (DTPM — the budget floats to who needs it).
     Per-SoC envelope + system envelope; the smaller wins.
3. RAIL TOPOLOGY               — shared vs independent
     Shared rail = fewer VRMs, cheaper, but rail-V = max(requesters) → leakage waste.
     Independent rail = per-IP DVFS freedom, more VRMs, more board area, more bugs.
     Per-SoC PMIC vs platform PMIC; cross-SoC handshake cost.
4. DVFS COORDINATION           — who arbitrates?
     Single arbiter (one PCU / RPMh / SMU as master, others as voters) — simpler, single bottleneck.
     Distributed (per-SoC arbiter + cross-SoC PM-channel) — scales, ordering bugs.
     Vote aggregation rule: max for performance-floor votes, min for power-cap votes.
     Wake/idle ordering across SoCs (one SoC waking can't be blocked by another in deep sleep).
```

**Reflex one-liner (steps 1-4):** *"Workload → budget split → rail topology → DVFS coordination. The hard tradeoff is rail-sharing: cheap silicon + leakage waste vs per-IP freedom + VRM/bug cost. The hard coordination question is arbiter centralisation — single master is simpler but bottlenecks at scale; distributed scales but introduces cross-SoC ordering bugs."*

Validation matrix that falls out: cross-product of (per-IP P-state) × (per-SoC PM-state) × (rail topology mode), with seam tests at every boundary — see [Day 30 — pre-Si PM validation](archive/day30_pre_si_pm_validation.md).

### 9. Perf counters and IPC (forward to Day 29)

$$\text{IPC} = \frac{\text{instructions retired}}{\text{cycles}}$$

`perf stat` is the entry point on Linux. IPC < 1.0 ≈ memory-bound or branch-bound. Goes up with: better branch prediction, hot-cache access, ILP. Goes down with: cache misses, branch mispredicts, dependency chains, contention.

Useful counters: `instructions`, `cycles`, `cache-misses`, `branches`, `branch-misses`, `LLC-load-misses`, `dTLB-misses`. Validation engineers turn raw counters into ratios + thresholds + alerts.

---

### Bugs in this Topic - [day26_dvfs_pstates_pvt.md](archive/day26_dvfs_pstates_pvt.md)

- **V-F transition reversed (raised F before V):** intermittent data corruption when OS jumps from P3 to P0; symptom looks like memory corruption or random `#GP`. Fix: enforce V-first sequence in microcode / firmware.
- **Turbo never engages on a one-thread workload:** PCU thinks the package is at thermal limit because DTS is reading miscalibrated; fix DTS calibration or the offset MSR.
- **Throttling under sustained load (PROCHOT asserted):** thermal solution undersized or fan curve too lazy; or chip is at an FF corner running hot. Check DTS, fan PWM, ambient.
- **One SKU hangs at the slow corner under cold-boot:** binning DVFS table didn't margin enough for SS at -20 °C; raise V at the lowest P-state for that bin.
- **Per-core DVFS request appears wasted on Snapdragon:** cluster voltage rail = max across cores; one core requesting Vmax pulls the whole rail up. Find the offender.
- **VRM droop on load step causes a transient hang:** missing decoupling, or VRM slew rate too slow; raise V floor or add cap.
- **Aging-related performance regression after 2 years:** NBTI / HCI shifted Vth; the same DVFS table no longer meets timing at the slow OPPs. Retune voltage offsets or accept derating in firmware.

---

### Interview Explanation (60-90 sec) - [day26_dvfs_pstates_pvt.md](archive/day26_dvfs_pstates_pvt.md)

> "Power on a chip is dynamic — C V-squared F — plus leakage, which is exponential in temperature and is now 30 to 40 percent of total at 7 nm and below. DVFS exploits the V-squared term: when full performance isn't needed, lower V and F together along the V-F curve. P-states are the discrete operating points; Turbo is the opportunistic mode that takes thermal and current headroom from idle cores. The transition order matters: raise V before F when speeding up, drop F before V when slowing down — reverse either and you get setup violations. Every chip lives at a different PVT corner — FF is fast and hot, worst for power; SS is slow and cool, worst for timing. After fab, chips are speed-binned into SKUs and each bin gets its own DVFS table. On Snapdragon I'd add that DVFS requests go through RPMh, which aggregates votes from Apps, Modem, ADSP, CDSP and programs the PMIC; per-core frequency is real but voltage is per-cluster on most SKUs because cores share a rail."

---

### Architect Drills - [day26_dvfs_pstates_pvt.md](archive/day26_dvfs_pstates_pvt.md)

*Power is physics, DVFS is policy on top of it. Reason about why every choice (V-before-F, discrete P-states, per-cluster rails, speed binning) lands where it does. No answer key.*

**D1. Why is dynamic power $\frac{1}{2}CV^2F$ — derive from first principles.**
*Constraints to weigh:* energy per switching event ($\frac{1}{2}CV^2$), switches per second ∝ frequency, what charges and discharges (gate capacitance), why V is squared and F is linear. Where does the formula break down (near-threshold voltage, leakage-dominated regime)?

**D2. Raise V before F when speeding up. Imagine a future technology where V and F are decoupled (separate rails per IP). Does the rule still apply?**
*Constraints to weigh:* setup-time slack as a function of V, what "F" even means without V (the PLL still locks), thermal lag vs electrical lag, glitch propagation across rail boundaries. Construct a workload where the rule reverses.

**D3. Why a discrete P-state ladder and not continuous V/F control?**
*Constraints to weigh:* characterisation matrix (each OPP needs validation), VRM transient response, OS predictability, common-OPP libraries reused across SKUs. What does Apple Silicon's near-continuous OPPs cost that Intel's discrete ladder saves — and what does it buy?

**D16. Speed binning sorts post-fab silicon into SKUs. Imagine perfect process control (no spread). What would the industry have to redesign?**
*Constraints to weigh:* yield economics of SKU mixing, datacenter procurement (fast vs slow bin pricing), power-perf curve as ensemble vs per-die. Where does binning create perverse incentives, and what would replace it?

**D17. Leakage is exponential in T and is now 30-40% of total at 7 nm. Reason about what's driving the curve toward 50%+.**
*Constraints to weigh:* shorter channels, lower threshold voltages, more cores idle vs active, on-chip thermal hotspots, power-gating granularity. What architectural choice (chiplets, big.LITTLE, dedicated accelerators) is a *response* to the leakage trend?

**D18. Per-core voltage on DynamIQ shares a rail. Construct the workload where this design loses *more* power than it saves.**
*Constraints to weigh:* one heavy core pulling rail to Vmax for the cluster, idle siblings still at high V (high leakage), heterogeneous workload mix. What does Apple's per-core LDO actually buy in P-core/E-core asymmetric workloads, and at what silicon cost?

**D19. Aging-related Vth shift over 2 years degrades slow-OPP timing margin. Reason about what a "self-tuning" DVFS table would look like.**
*Constraints to weigh:* in-field characterisation (PVT sensors, ring oscillators), telemetry storage in fuses/NVRAM, OS-visible vs FW-only adaptation, security of in-field re-tuning. What stops shipping this today — cost, validation, or trust?

---

### Exit Check - [day26_dvfs_pstates_pvt.md](archive/day26_dvfs_pstates_pvt.md)

- [ ] Wrote $P_{dyn} = \tfrac{1}{2}CV^2F$ and explained each variable.
- [ ] Stated V-first vs F-first rule for ramp up vs ramp down.
- [ ] Listed FF vs SS corners and what each is worst for.
- [ ] Explained Turbo headroom in 30 seconds.
- [ ] Computed IPC from `perf stat` numbers.
- [ ] Mapped Snapdragon DVFS through RPMh + PMIC.

### Concept - [day29_thermal_power_perf_counters.md](archive/day29_thermal_power_perf_counters.md)

The chip operates inside a **junction-temperature** (Tj) envelope: $T_j = T_{ambient} + P \cdot \theta_{JA}$. Cross **Tjmax** and `PROCHOT#` asserts → throttle. **Modern SoCs are multi-IP shared-resource systems** (CPU + GPU + NPU + ISP) where memory bandwidth, LLC, NoC, IOMMU and power budget are contended; performance bugs are usually one of five interference classes. The **PMU** measures it: top-down methodology classifies every pipeline slot into Frontend / Backend-Memory / Backend-Core / Bad-Speculation / Retiring. **RAPL** measures energy. The figure of merit is **perf-per-watt** or **EDP** (energy × delay), not raw throughput.

### Deep Study Notes - [day29_thermal_power_perf_counters.md](archive/day29_thermal_power_perf_counters.md)

### 1. Tj — junction temperature

$$T_j = T_{ambient} + P_{total} \cdot \theta_{JA}$$

| Term | Meaning |
|---|---|
| $T_j$ | silicon junction temperature (hottest point) |
| $T_{ambient}$ | room/case temperature |
| $P_{total}$ | total power dissipation (W) |
| $\theta_{JA}$ | junction-to-ambient thermal resistance (°C/W) |

The thermal stack:

```
silicon die        Tj
  │ θ_JC (junction-to-case)
TIM (paste/pad)
  │
IHS (heat spreader)
  │ θ_CS (case-to-sink)
heatsink
  │ θ_SA (sink-to-ambient)
ambient air        Ta
```

$\theta_{JA} = \theta_{JC} + \theta_{CS} + \theta_{SA}$. Dried TIM doubles $\theta_{CS}$ → Tj jumps; same chip, same workload, throttles where it didn't before.

**Tjmax** is the spec limit (e.g., 100 °C desktop). Above → throttle. Well above → emergency shutdown. Continuous near-Tjmax → reliability degradation (electromigration).

### 2. PROCHOT# and DTS

```
Normal    Tj=75°C  PROCHOT# = high (off)
Hot       Tj=98°C  PROCHOT# = low  (asserted) → CPU drops F/V
Cooled    Tj=90°C  PROCHOT# = high (off)      → resume full speed
```

PROCHOT# can be bidirectional — a hot neighbour chip on a shared signal can throttle the CPU.

**DTS (Digital Thermal Sensor)** — on-die temperature sensor read by the PCU. Calibration fuses programmed at fab; if missing/wrong, DTS reads hot or cold, throttle behaviour misbehaves.

### 3. Hotspots

Different die regions dissipate at very different power densities (CPU AVX core ≥ 50 W/cm², idle PCIe block < 5 W/cm²). Hotspot can be **10-15 °C above die average**. Throttling must respond to the hotspot sensor, not the average — single-sensor throttle leaves the rest of the chip running cool while one corner degrades.

Per-core DVFS (Day 26) lets you throttle the hot core while neighbours stay at P0.

### 4. Voltage droop and AVX license

Sudden current surge (an AVX-512 kernel starts) → V-rail droops faster than VRM can react → if V dips below Vmin for current F → setup violations.

Intel's mitigation: **license levels** — instructions classified into license 0 (scalar), 1 (AVX2-heavy), 2 (AVX-512-heavy). Entering a higher license preemptively cuts F to keep V margin. Recovery to license 0 takes microseconds; mixing a few AVX-512 ops into mostly-scalar code can leave the whole core in a lower license for that window — a real anti-pattern.

### 5. Multi-IP interference — five classes

```
┌────────────────────────────────────────────────┐
│  CPU   GPU   NPU   ISP                         │
│   │     │     │     │                          │
│   ╪═════╪═════╪═════╪══  shared NoC            │
│   │     │     │     │                          │
│   └─────┴─────┴─────┴──→  shared LLC          │
│                          │                     │
│                          └→  Memory ctrl/DDR5 │
└────────────────────────────────────────────────┘
```

| # | Class | Counter to detect | Mitigation |
|---|---|---|---|
| 1 | **Memory BW starvation** | per-agent BW counter at MC | QoS / priority / BW reservation |
| 2 | **LLC thrashing** | per-agent LLC occupancy + miss rate | Cache Allocation Tech (CAT, CLOS) — way partitioning |
| 3 | **NoC congestion** | NoC link utilisation, back-pressure cycles | virtual channels, priority arbitration |
| 4 | **Power-budget conflict** | RAPL package + per-domain | Dynamic power budgeting (DTPM) |
| 5 | **IOTLB / shared IOMMU** | IOTLB miss + page-walk latency | per-agent IOTLB partition, huge pages |

### 6. Multi-IP validation methodology — 4 steps

```
Step 1: Single-IP baseline   — measure each IP alone (BW, latency, IOPS, fps)
Step 2: Pair-wise            — every 2-IP combination; degradation vs baseline
Step 3: Full system          — all IPs active; measure worst case; check QoS
Step 4: QoS validation       — enable QoS; repeat 2 + 3; verify guarantees deliver
```

Skipping step 1 is the #1 mistake — without baseline you can't measure interference.

### 5a. Multi-SoC thermal envelope sharing (the AMD design probe, part 2)

*Continuing the scaffold from [Day 26 Sec 8a](archive/day26_dvfs_pstates_pvt.md). Steps 5-6 of the "design NPU + CPU + multi-SoC PnP tradeoff" answer.*

```
5. THERMAL ENVELOPE SHARING   — one chassis, many hotspots
     Per-SoC Tj limit + system skin-temp limit + ambient envelope; the smaller wins.
     Cross-SoC thermal coupling: SoC-A hot → SoC-B ambient rises → SoC-B headroom shrinks.
     Throttle priority: which agent gives up frequency first under shared thermal pressure?
        - Latency-sensitive (CPU foreground) protected; throughput (NPU batch) yields.
     Sensor topology: per-SoC DTS + skin thermistor + virtual sensor (estimated from RAPL + thermal model).
     Control-loop bandwidth: too fast = oscillation across SoCs; too slow = trip the cap.
6. VALIDATION COVERAGE MATRIX  — what falls out of steps 1-5
     Axes: per-IP P-state × per-SoC PM-state × rail topology × ambient corner × workload mix.
     Seam tests (planned, not hoped-for):
        - SoC-A turbo + SoC-B deep idle → wake-coupling delay
        - NPU sustained + CPU burst → budget reallocation latency
        - Thermal trip on SoC-A → SoC-B throttle response time
        - Single-PMIC failure → fallback rail behaviour
     Done-when: every seam covered, no QoS violation under worst-case mix, perf-per-watt within ±X%.
```

**Reflex one-liner (steps 5-6):** *"Thermal envelope is shared across the chassis — one SoC's hotspot eats the next SoC's headroom, so throttle priority must be workload-aware (latency-sensitive protected, throughput yields). Validation is the cross-product of per-IP P-state × per-SoC PM-state × ambient × workload mix, with explicit seam tests at every boundary — wake-coupling, budget reallocation, cross-SoC throttle response."*

**Full 6-step answer in one breath:** *"Workload → budget split → rail topology → DVFS coordination → thermal envelope → validation matrix. Each step constrains the next; the bugs live at the seams between SoCs, not inside one."*

### 7. PMU — performance monitoring unit

Every modern CPU has hardware counters that increment on architectural events.

- **Fixed-function (3 on Intel):** instructions retired, core cycles, reference cycles. Always available.
- **General-purpose (4-8 per core):** programmable to count any of hundreds of events.
- **Uncore:** LLC, memory controller, NoC, IIO. Chip-scope, not per-core.

Access:

| OS / Vendor | Tool |
|---|---|
| Linux any | `perf stat`, `perf record`, `perf top` |
| Intel | PCM, VTune, EMON (internal) |
| ARM | Streamline, Snapdragon Profiler, Linux `perf` |
| AMD | μProf |

### 8. Top-down microarchitecture analysis (TMA)

Intel's hierarchical methodology. Every pipeline slot lands in one of four buckets:

```
                Pipeline Slot
                /          \
           Frontend       Backend
            Bound          /     \
                       Memory   Core
                        Bound    Bound
                                 \
                          Retiring  ← good
                            \
                       Bad Speculation (mispredict)
```

Workflow:

1. Run workload, collect TMA L1 events.
2. Identify dominant bucket (e.g., "62% Backend Memory Bound").
3. Drill down with L2/L3 events (DRAM bandwidth? LLC miss? Store buffer full?).
4. Now you know **where** to optimise; you're not guessing.

IPC alone is meaningless without TMA. IPC = 0.5 might be fine for a memory-bound kernel and terrible for a compute-bound one.

### 9. RAPL — energy counters

**Running Average Power Limit.** Exposes accumulated joules per domain via MSR:

| Domain | Coverage |
|---|---|
| PKG | whole package |
| PP0 | cores |
| PP1 | client iGPU |
| DRAM | server SKUs |

Read twice, subtract, divide by elapsed → average watts. Same model on AMD; ARM varies (often via SCMI/SCP firmware).

**RAPL ≠ wall power.** RAPL is the VR input estimate; ground truth needs an external power meter (Quarch, etc.). Typical gap: 20-30% (VR loss, DRAM not in client RAPL, platform devices, PSU efficiency).

### 10. Benchmark hygiene

- [ ] Disable Turbo or pin to fixed F (else you measure thermal headroom, not work).
- [ ] Disable HT for compute benchmarks (less variance).
- [ ] Pin to specific CPUs: `taskset -c 4-7`.
- [ ] Set governor `performance`: `cpupower frequency-set -g performance`.
- [ ] Drop file caches: `echo 3 > /proc/sys/vm/drop_caches`.
- [ ] Stop background services.
- [ ] Warm up — discard first N runs (cache fill, JIT, freq ramp).
- [ ] Repeat ≥10 runs; report median + 95% CI, **not single number**.

Aggregation:

- **Geometric mean** for ratios (SPEC scores, normalised speedups).
- **Harmonic mean** for rates (MB/s averaged across mixed sizes).
- **Arithmetic mean** only for absolute durations.

Using arithmetic mean on ratios changes ranking when you change the baseline — invalid.

### 11. Perf-per-watt and EDP

- **perf/W** = throughput / average watts. Higher better. Mobile and DC primary metric.
- **EDP** (Energy-Delay Product) = energy × time = $P \cdot t^2$ for fixed work. Lower better. Penalises both slow-and-efficient and fast-and-power-hungry.
- $ED^2P$ weights latency more.
- Pareto curve: throughput vs power across DVFS points. The "knee" is where $\Delta perf / \Delta watts$ collapses — past it you burn watts for no gain.

### 12. Counter signatures — common bugs

| Symptom | Counter signature | Likely root cause |
|---|---|---|
| Low IPC, high cache-miss | LLC-misses high, DRAM BW high | working set > LLC; missing tile/blocking |
| Low IPC, low cache miss | front-end bound, μop cache miss | I-cache thrash, large code |
| High IPC, slow wallclock | ref cycles ≫ core cycles | freq throttle (thermal/RAPL) — read `MSR_CORE_PERF_LIMIT_REASONS` |
| Single-thread fast, multi-thread doesn't scale | LLC contention high, snoop traffic high | false sharing or coherence storm (Day 17) |
| Latency variance run-to-run | C-state residency high | idle states between bursts; pin governor |

---

### Bugs in this Topic - [day29_thermal_power_perf_counters.md](archive/day29_thermal_power_perf_counters.md)

- **Throttle at Tj=85°C with spec Tjmax=100°C:** DTS calibration fuses missing → sensor reads 15°C high. Verify with external thermocouple.
- **Throttle without thermal cause:** RAPL package-power limit (`PL1`/`PL2`) too aggressive in BIOS. Read `MSR_CORE_PERF_LIMIT_REASONS`.
- **AVX-512 makes scalar code slower:** license-level transition window. Profile transitions; consider `-mprefer-vector-width=256`.
- **Workload "got slower" between two runs, IPC unchanged:** ref cycles ≫ core cycles → frequency dropped. Throttle reason MSR.
- **CPU starves under GPU streaming DMA:** LLC thrash. Apply CAT/CLOS partitioning.
- **NPU EP fallback to CPU per-op:** unsupported op forced fallback; per-fallback tensor copy dominates inference latency. Count fallbacks; quantise/reshape model.
- **HW prefetcher hides a missing software prefetch:** silicon A passes, silicon B (different prefetcher heuristic, or HW PF disabled in BIOS) regresses. Triage: disable HW PF (Intel MSR `0x1A4`) and re-run.
- **RAPL says 45 W, wall says 65 W and you call it a bug:** not a bug — VR loss + DRAM (not on client RAPL) + platform + PSU efficiency. Use RAPL for relative compare, wall for absolute claims.

---

### Interview Explanation (60-90 sec) - [day29_thermal_power_perf_counters.md](archive/day29_thermal_power_perf_counters.md)

> "Thermal in silicon comes back to one equation: Tj equals ambient plus power times theta-JA. The thermal stack — die, TIM, IHS, sink, ambient — is just resistors in series. Cross Tjmax and PROCHOT asserts and the PCU drops F/V. The first thing I check on a thermal complaint is whether DTS is calibrated, because a sensor offset will throttle a chip that's nowhere near hot. On a multi-IP SoC there are five interference classes — memory BW, LLC thrash, NoC congestion, power budget, IOTLB pressure — each with a counter that names it. PMU is how I measure: top-down classifies every pipeline slot into frontend, backend memory, backend core, bad spec, retiring. IPC alone doesn't tell you anything; pair it with TMA. RAPL gives me joules per domain; perf-per-watt and EDP are the figures of merit, not raw throughput. The hygiene that separates real numbers from noise is fixed-frequency runs, dropped warmup, geomean for ratios. Most perf bugs I've chased reduce to: throttle reason MSR, then top-down, then a per-counter ratio with a threshold."

---

### Architect Drills - [day29_thermal_power_perf_counters.md](archive/day29_thermal_power_perf_counters.md)

*Thermal is physics; PMU/RAPL/top-down are abstractions over it. Reason from constraints — what each model captures and where it leaks. No answer key.*

**D1. Why is $T_j = T_a + P\theta_{JA}$ a useful model when in reality the die is non-uniform?**
*Constraints to weigh:* lumped-parameter approximation, thermal RC network of die-TIM-IHS-sink-air, hotspot vs average, when the model breaks (transient PROCHOT). What does a per-tile thermal model buy that justifies its complexity — and where would steady-state lumped still win?

**D2. PROCHOT exists at the package level. Reason about why per-IP thermal throttling isn't more aggressive — what stops a future SoC from per-tile DVFS?**
*Constraints to weigh:* per-IP DTS sensor area, PCU latency to react per tile, voltage-rail granularity, software visibility (one PROCHOT bit vs N), validation matrix. What does Apple's per-block thermal headroom actually cost in silicon and FW?

**D3. RAPL underestimates wall power by 20-30%. Construct the cause stack and reason about which gap is fixable.**
*Constraints to weigh:* VR efficiency (model vs measurement), DRAM not in client RAPL, platform devices, PSU efficiency. What would a "true platform RAPL" need that today's doesn't have — sensors, calibration, or scope?

**D16. Top-down PMU classifies every slot into 4 buckets. Why exactly 4 — could it be 2 or 8?**
*Constraints to weigh:* what each bucket isolates (frontend / backend / bad-spec / retiring), counter budget on the PMU, actionability for a perf engineer, cross-vendor portability of the methodology. What does Intel's TMA L2 add at 6+ buckets, and where does the marginal information die?

**D17. HW prefetchers can hide a missing software prefetch. Reason about why disabling HW PF is a triage step, not a fix.**
*Constraints to weigh:* silicon-A vs silicon-B prefetcher heuristic differences, BIOS-disabled scenarios, perf cost of explicit prefetch, what the bug-report should say (data, not "disable PF"). When does software prefetch genuinely beat HW prefetch?

**D18. AVX-512 license-level cuts F to maintain V margin. Construct a workload where AVX-512 is a net loss vs AVX-256.**
*Constraints to weigh:* fraction of time in vector code, recovery window after license drop, scalar-around-vector latency, license-transition cost. What does Intel's AMX do differently to avoid this trap, and why didn't earlier vector ISAs?

**D19. Five interference classes on a multi-IP SoC (BW, LLC, NoC, power, IOTLB). Construct the diagnostic decision tree from a single "perf is bad" signal.**
*Constraints to weigh:* counter-per-class, what symptoms overlap (BW-starvation looks like LLC-thrash if you only see IPC), order of cheap-to-expensive checks. Build the triage in the right order — which class do you rule in/out first and why?

---

### Exit Check - [day29_thermal_power_perf_counters.md](archive/day29_thermal_power_perf_counters.md)

- [ ] Wrote $T_j = T_a + P\theta_{JA}$ and named each piece of the thermal stack.
- [ ] Stated PROCHOT vs DTS-offset bug.
- [ ] Listed 5 multi-IP interference classes + a counter each.
- [ ] Walked TMA hierarchy and named one event per leaf.
- [ ] Stated RAPL ≠ wall and quantified the gap.
- [ ] Gave the geomean-vs-arithmetic-mean rule.
- [ ] Stated EDP and the Pareto knee idea.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S15](s15_accelerator_job_e2e.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S17](s17_cstates_s0ix_device_pm.md)

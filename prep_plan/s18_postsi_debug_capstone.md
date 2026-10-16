# S18 - Post-silicon debug capstone

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [D29 thermal/power/perf counters](archive/day29_thermal_power_perf_counters.md), [D31 post-silicon PM](archive/day31_post_si_pm_validation.md).
- **Must answer:**
  - **Observables:** residency counters, RAPL, perf-limit reasons, POST codes, LTSSM/AER, IOMMU fault logs, completion status.
  - **Method:** expected flow → first divergence → which layer (fuse, BIOS, OS, firmware, hardware).
- **Capstone (whiteboard, 25 min):** *"The platform is in S0ix. An app wakes and submits a DSA copy job. The result triggers a NIC transmit. Then everything goes idle again."* Trace it on the §1 stack:
  1. wake source and always-on logic
  2. rails and PLLs restored
  3. package C-state, then core C-state exit
  4. links go L1.2 → L0
  5. device leaves its low-power state
  6. ENQCMD → WQ → IOMMU/PASID → DMA → completion → MSI-X
  7. NIC doorbell → DMA → transmit
  8. back to idle

  Give **one failure mode and one observable per step**.
- **Week narration:** the capstone itself, in under 5 minutes.

## Study notes

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

### Concept - [day31_post_si_pm_validation.md](archive/day31_post_si_pm_validation.md)

Post-silicon PM validation is the discipline of proving — on **real silicon, in a real OS, on a real board** — that every PM state the pre-Si team signed off actually works under real workloads, real PVT corners, and real wake patterns. The toolkit shifts from waveforms + UPF to **OS telemetry + lab instruments + microcode patches**. The bug surface shifts from sequencer races to **firmware paths, governor policies, voltage margins, thermal interactions, and silicon variability across units**. Every escaped pre-Si bug surfaces here at 100× the debug cost — which is why the post-Si team's most valuable artifact is the **failure log that feeds back into the next pre-Si vplan.**

### Deep Study Notes - [day31_post_si_pm_validation.md](archive/day31_post_si_pm_validation.md)

### 1. The post-Si PM bring-up flow

```
Stage A — Power-on sequence
   ├─ Apply rails per PMIC sequence
   ├─ Release reset; PCU ROM executes
   ├─ Scope on PCU heartbeat GPIO
   └─ Catch: rail-order bugs, PMIC trim mis-set, brown-out on cold boot

Stage B — PCU FW load + sanity
   ├─ PCU FW DMA'd in from boot ROM / external flash
   ├─ Check FW signature, version
   └─ Catch: FW version mismatch, RAM init wrong

Stage C — First OS boot
   ├─ Linux boots with all PM disabled (cpuidle=poll, intel_pstate=disable)
   ├─ Confirm functional baseline before enabling PM
   └─ Catch: BIOS PM defaults wrong

Stage D — Enable C1 only
   ├─ Allow processor.max_cstate=1
   ├─ Run idle workload; verify residency counters tick
   └─ Catch: C1 entry hang (rare; usually trivial)

Stage E — Enable C6
   ├─ Allow up to C6
   ├─ Idle thread → MWAIT → expect resid_cnt to climb
   ├─ Wake on timer + IRQ + IPI sweep
   └─ Catch: wake-loss, retention loss, stale TLB on wake

Stage F — Enable PkgC6 + S0ix
   ├─ Coordinate all cores idle + PCIe L1.2 + DRAM SR
   ├─ Look for residency in PkgC6 counter
   └─ Catch: cross-IP blocker bugs (ADR opcode story lives here)

Stage G — DVFS sweeps
   ├─ Enable intel_pstate / acpi_cpufreq
   ├─ Run V/F sweep at SS-cold, TT-typical, FF-hot
   └─ Catch: voltage-margin failures, thermal trip ordering

Stage H — Real-workload soak
   ├─ Server load (SPECpower), client load (idle desktop)
   ├─ 24-72 hr soak
   └─ Catch: rare residency-counter wrap, GC-style fragmentation in PCU state
```

### 2. The post-Si PM measurement stack

| Layer | Tool | What it reports |
|---|---|---|
| **OS PM stats** | `cat /sys/devices/system/cpu/cpu*/cpuidle/state*/{name,residency,usage}` | Per-state residency + entry count |
| **Per-core counters** | `turbostat` | Per-core MHz, P-state, C0/C1/C6/C7%, watts, Tj |
| **Package counters** | `socwatch` (Intel) | PkgC residency, S0ix breakdown, blocker attribution |
| **MSRs directly** | `rdmsr 0x3FA` (PKG_C6_RESIDENCY), `0x3FC` (CORE_C6) | Raw counter values |
| **Voltage / current** | `rdmsr` RAPL counters (`0x611` energy) | Energy per package over interval |
| **PMIC telemetry** | i2c trace, vendor tool | Rail voltages + currents in real time |
| **Hardware perf counters** | `perf stat -e ...` | Cycles / instructions / cache / TLB |
| **Lab instruments** | Quarch power module, Monsoon HV, Keysight scope | Bench-grade current + voltage waveforms |
| **In-target probe** | JTAG (Lauterbach), Intel ITP / DCI, SVTools | Halt, MSR dump, PMC trace |

### 3. turbostat — the single most-used PM lab tool

```
$ sudo turbostat --interval 1 --quiet
Package Core CPU Avg_MHz Busy% Bzy_MHz TSC_MHz IPC IRQ POLL C1 C1E C6 POLL% C1% C1E% C6% CPU%c1 CPU%c6 CoreTmp PkgTmp Pkg%pc2 Pkg%pc6 PkgWatt CorWatt
-       -    -   38      1.9   1981    2400    1.2 41   0    1  3   12  0     0.1 0.4  92.1 4.5    93.6   38      40     5.2     86.4    1.42    0.31
```

What to read:
- **Busy%** vs **CPU%c6**: idle time should land in deep C-state, not in C1 (= poll-like).
- **Pkg%pc6**: if cores at C6 but package at PC2 → some IP holds the package up. Hunt the blocker.
- **CorWatt vs PkgWatt**: uncore + memory power; PkgWatt > CorWatt means uncore dominates → look at LLC, mesh, IMC.
- **CoreTmp**: rising temp on an "idle" system → bad C-state entry (clocks still running).

### 4. socwatch / SoCWatch — the package PM oracle

`socwatch` produces a JSON/CSV per-interval breakdown:

```
Package Idle State Summary
  PC0:   12%
  PC2:    3%
  PC6:   85%

S0ix Block Reasons (in % of non-S0ix time)
  Uncore not idle:           8%
  PCIe LTSSM not L1.2:      14%
  DRAM not in self-refresh:  3%
  Display refresh active:   42%
  USB device active:        33%
```

**This is the bug-attribution oracle.** "We're only at 60% PkgC6" → socwatch shows PCIe is the blocker 14% of the time → file CR against PCIe team.

### 5. MSR sweeps — the architect's microscope

Every Intel CPU exposes hundreds of MSRs that control or report PM. The val engineer scripts sweeps:

```
# Walk C-state residency MSRs once per second
while true; do
    sudo rdmsr -a 0x3FC   # CORE_C6_RESIDENCY
    sudo rdmsr -a 0x3FA   # PKG_C6_RESIDENCY
    sudo rdmsr -a 0x611   # ENERGY_STATUS (RAPL)
    sleep 1
done
```

Or change a PM-influencing MSR and observe:

```
# Tighten max C-state demotion threshold and watch C6% fall
sudo wrmsr -a 0xE2 0x1818084  # PKG_CST_CONFIG_CONTROL
turbostat --interval 5
```

The val team builds a library of these scripts. The Intel internal version (SVTools) wraps the same with PMC trace, eventing, and CSV export.

### 6. Lab instruments — the analog ground truth

| Instrument | What it captures | When |
|---|---|---|
| **Quarch power module** | Per-rail V + I at 250 kHz | Always-on for PM characterisation |
| **Monsoon HV** | Battery-side I + V on mobile platforms | S0ix entry / exit |
| **Keysight N6705 + scope** | High-BW transient capture | Sequenced-wake droop, di/dt |
| **PCIe analyser (Lecroy Summit, Teledyne)** | L0/L0s/L1/L1.2 transitions, LTSSM ladder | PCIe PM debug |
| **Logic analyser** | PERST#, CLKREQ#, GPIO wake | Cross-IP PM handshakes |
| **Thermal IR camera** | Surface temp distribution | Thermal-vs-PM regression |

The discipline is to **always have a Quarch capture running during a PM test** — when a residency anomaly shows up, you have the rail trace to correlate.

### 7. The 7-step debug framework

When a PM bug appears, the structured triage:

1. **Reproduce** — minimal stimulus + minimal config that hits it; record seed, FW version, MSRs.
2. **Isolate axis** — which knob, when toggled, makes it go away? (state, wake source, governor, core, voltage)
3. **Telemetry layer pick** — OS counter → MSR → PMC trace → JTAG → scope. Go one layer down per failed isolation.
4. **Cross-IP check** — socwatch blocker breakdown; is another IP the actual root?
5. **Microcode / FW patch test** — if PCU FW change candidate exists, side-load and re-run.
6. **Root cause + spec impact** — design / FW / SW / config / spec ambiguity? Update spec if needed.
7. **Pre-Si feedback** — add a coverpoint + SVA to the pre-Si vplan so the next stepping catches it in sim.

Every PM bug fix touches step 7 — that's how the org gets better stepping over stepping.

### 8. Bug story 1 — ADR opcode-ordering (90 sec)

**Context:** ADR (Asynchronous DRAM Refresh) is the path that, on a sudden power loss, flushes the CPU's WPQ (write-pending queue) into NVDIMM before the rail collapses. It's the highest-priority PM-adjacent flow: it overlaps with PkgC entry, PCIe quiesce, and uncore power-down.

**The bug:** Under a specific timing — ADR-triggered PCU sequence interleaved with a normal PkgC6 entry from the OS governor — the PCU opcode that issued the WPQ flush was reordered with respect to the opcode that fenced PCIe quiesce. Result: WPQ writes raced PCIe back-pressure release; a small window allowed an in-flight CPU write to the NVDIMM to be lost. Symptom: very rare data-loss on platforms that experienced ADR within ~10 µs of a governor-issued PkgC6 entry.

**Detection:** Customer-reported one corruption per 10⁹ ADR events. Reproduction took weeks. PCU opcode trace captured on a side-band SVTools port revealed the reorder. Fix was a PCU microcode patch enforcing a barrier between ADR-WPQ-flush and any uncore power-down opcode.

**Lesson:** the pre-Si vplan was missing the cross-product cell `(ADR-event × PkgC-entry)`. Added that coverpoint + a `p_adr_before_uncore_pd` SVA. Next stepping caught a related but distinct ordering bug in sim, before silicon.

### 9. Bug story 2 — Core C6 wake-loss (90 sec)

**Context:** Core C6 powers down core clocks + rails; wake on IRQ, IPI, or timer. Wake delivery goes through the local APIC and the GIC redistributor (or APIC + LVT on x86).

**The bug:** On a specific stepping, a core in C6 with a tightly-armed deadline timer would occasionally miss the timer wake. Symptom: that core sat in C6 indefinitely; OS scheduler eventually noticed the missed deadline and IPI'd it awake, but the latency was milliseconds, breaking real-time workloads.

**Reproduction:** Required (a) core in C6 for >5 ms, (b) timer programmed for a deadline within 1 µs of the next bus arbitration window, (c) neighbouring core entering its own C6 in the same 100-ns interval. ~1 in 10⁴ entries.

**Telemetry path:** OS counters showed missed wake (`/proc/timer_list` deadline vs actual fire). turbostat showed the affected core stuck at C6. SVTools PMC trace + LAPIC MSR dump showed the timer comparator had armed, but the LAPIC wake-pulse to the PCU was lost during the neighbouring core's PCU-sequencer contention window.

**Root cause:** Arbitration in the PCU between two simultaneous C6-wake events used a fixed priority; if the lower-priority wake arrived in the 2-cycle window during higher-priority service, it was silently dropped. Fix: PCU FW patch changed arbitration to a held-pending queue.

**Lesson:** pre-Si had `cp_wake_src` coverage but no cross-product on **simultaneous wakes from different cores**. Vplan row added; SVA `p_no_wake_drop_on_concurrent_request` written; closed in next stepping.

### 10. Silicon variability — same FW, different unit, different bug

Post-Si engineers learn the painful truth: **two units off the same wafer can behave differently** at the PM corner. A retention flop that holds at typical Vmin might drop at the slow-cold-low-V corner on one die. The disciplines:

- **Sample variety** — characterise across multiple wafers, multiple positions, multiple corners.
- **Per-unit logs** — never report "the bug" without unit serial + corner.
- **Cross-die correlation** — track CR re-tests across units; if a fix works on unit A but not B, the issue is silicon variability, not a missed code path.
- **Vmin/Vmax binning** — characterise the PM-safe voltage range per state per unit.

This is also where the PM team interacts with the **product engineering / characterisation team** — they own per-die binning and PMIC trim tables.

---

### Bugs in this Topic - [day31_post_si_pm_validation.md](archive/day31_post_si_pm_validation.md)

- **ADR ↔ PkgC ordering corruption** (above): cross-event PCU opcode reorder; fix in microcode + add pre-Si cross-coverpoint.
- **Core C6 wake-loss** (above): PCU arbitration drops simultaneous wakes; fix in microcode + add concurrent-wake SVA.
- **PkgC6 never entered, OS thinks all cores idle:** one core's `MWAIT` was returning early due to a stale LVT timer event; OS reported C6 but core was in C1. *Catch: cross-check OS counter against MSR `CORE_C6_RESIDENCY`.*
- **S0ix entered but battery drain unchanged:** display refresh thought "off" by SW but DSI link still active. *Catch: socwatch S0ix block breakdown shows display.*
- **Voltage droop on sequenced wake collapses neighbour:** rail-droop trips PMIC UV, neighbour resets. *Catch: Quarch capture + post-Si SVA-equivalent in PMIC i2c log.*
- **Thermal trip enters wrong state:** thermal-trip handler enters C6 but didn't clear pending IRQ → wake-loop. *Catch: thermal + PM cross test.*
- **DVFS request lost during C6 entry:** DVFS request arrived in the 1-cycle window after PCU committed to C6; was silently dropped. *Catch: directed test + per-core DVFS-on-entry coverpoint.*
- **MSR readback inconsistent across cores:** non-architectural MSR read from different core returns different value due to per-core cache of uncore state. *Catch: cross-core MSR-consistency sweep.*
- **Residency counter wraparound after 4.2 s:** 32-bit counter at 1 GHz wraps; OS sees negative delta → reports 0% residency. *Catch: long-soak + counter-wrap-aware parser.*

---

### Interview Explanation (60-90 sec) - [day31_post_si_pm_validation.md](archive/day31_post_si_pm_validation.md)

> "Post-Si PM val is where the pre-Si coverage meets the lab bench. You bring up PM one knob at a time — C1, then C6, then PkgC6, then S0ix, then DVFS — and at each step you read OS counters with turbostat, package counters with socwatch, raw MSRs directly, and you have a Quarch power module on the rails so any anomaly has analog ground truth. socwatch is the blocker oracle — it tells you *which IP* is preventing PkgC6 entry. The bugs you find are firmware paths, governor policies, and silicon variability — things pre-Si couldn't see. My method is the 7-step framework: reproduce minimally, isolate the axis, descend one telemetry layer per failed isolation, check cross-IP via socwatch, side-load a PCU FW patch if available, root-cause, and feed back into the pre-Si vplan with a new coverpoint and SVA. I've worked through two of these end-to-end — the ADR opcode-ordering corruption that needed a microcode barrier, and a Core C6 wake-loss caused by PCU arbitration dropping simultaneous wakes — and in both cases the actual product fix was in firmware but the *lasting* fix was the pre-Si coverage rule that prevents the next variant."

---

### Architect Drills - [day31_post_si_pm_validation.md](archive/day31_post_si_pm_validation.md)

*Post-Si is where pre-Si assumptions meet silicon reality. Reason without an answer key.*

**D1. socwatch reports PkgC6 = 0%. Cores all report C6 residency >90%. Reason about the next telemetry layer to consult.**
*Constraints to weigh:* what holds the package out (uncore, PCIe, DRAM, display, USB), what each layer reports, what's cheapest to capture first.

**D2. A PM bug reproduces 1 in 10⁴ entries. Reason about whether to chase it or waive.**
*Constraints to weigh:* customer exposure, severity (data loss vs perf hit), cost of reproduction time, risk if it interacts with a future feature.

**D3. The same PCU FW works on unit A but not unit B (same stepping, same board). Reason about the next move.**
*Constraints to weigh:* silicon variability (Vmin, retention margin), board-level differences (PMIC trim, capacitance), wafer position binning, sample size before declaring.

**D16. You add a coverpoint to the pre-Si vplan after finding a post-Si bug. Reason about how to ensure the coverpoint actually exercises the silicon failure mode rather than a sim-only abstraction of it.**
*Constraints to weigh:* what part of the failure is timing-dependent (only emul), what part is FSM-reachable (sim), whether SVA on the property is enough.

**D17. turbostat shows Core%c6 90% but PkgWatt unchanged from "no PM" run. Reason about what's wrong.**
*Constraints to weigh:* uncore power dominance, package not in PC6, sleep transistors not opening (FW bug), telemetry calibration.

**D18. The PCU FW patch fixes the bug in lab but the architect wants to know "is this safe in the wild?" Reason about how to prove generality.**
*Constraints to weigh:* sample diversity, corner sweep, soak runtime, regression scope, what could the patch break that the lab won't exercise.

**D19. Pre-Si reports 100% PM coverage closed; post-Si finds a bug that was theoretically reachable in sim. Reason about whether the failure was a coverpoint gap, an SVA gap, or a stim gap.**
*Constraints to weigh:* what each of the three would have caught, where the abstraction broke between spec and vplan, the difference between "covered" and "exercised under the same constraints as silicon."

---

### Exit Check - [day31_post_si_pm_validation.md](archive/day31_post_si_pm_validation.md)

- [ ] Drew the 8-stage post-Si PM bring-up flow.
- [ ] Named the OS counters, package counters, MSR, and lab instruments for PM measurement.
- [ ] Wrote a turbostat output by hand and explained what each column means for PM.
- [ ] Explained socwatch as the blocker oracle.
- [ ] Recited the 7-step debug framework.
- [ ] Told the ADR opcode-ordering story in 90 sec end-to-end with root cause + pre-Si feedback.
- [ ] Told the Core C6 wake-loss story in 90 sec with the same structure.
- [ ] Stated three classes of silicon-variability-driven post-Si surprise.

### Concept - [day40_intel_role_narrative.md](archive/day40_intel_role_narrative.md)

You are (joining 27 Jul 2026) an **IP Functional Validation engineer at Intel** — CPU subsystem, pre-silicon. The plan's old goal ("cram to leave Microsoft for a silicon role") is **done**: the Intel offer is accepted. This day is the **narrative wrap** — make the whole career arc a set of **reflex traces** (not recited facts), frame **Microsoft as the most-recent past chapter**, keep the **retained bug stories** sharp, and lock the identity shift: *"I continuously build deep expertise; interviews are a by-product."* This directly fixes the **owned-narrative-fluency** gap flagged repeatedly in the [interview log](../INTERVIEW_LOG.md).

### Interview Explanation — the 30-sec career narrative (current = Intel) - [day40_intel_role_narrative.md](archive/day40_intel_role_narrative.md)

> "I'm an IP Functional Validation engineer at Intel, working on CPU-subsystem power-management and PCIe pre-silicon — C-state and ADR flows, transactors, and cross-IP bug hunting on emulation. I started in post-silicon storage-controller bring-up at L&T for SanDisk, moved to Intel for pre-silicon CPU-subsystem validation, then spent two years at Microsoft validating AI models across Intel, AMD, and Qualcomm silicon — capturing performance, accuracy, and memory KPIs and building agentic trace-triage. The common thread is cross-IP, cross-vendor debug and coverage discipline, which is why I came back to the silicon IP boundary — that's where the highest-leverage validation gaps live."

---

### Architect Drills (rehearse aloud, no answer key) - [day40_intel_role_narrative.md](archive/day40_intel_role_narrative.md)

1. Deliver the 30-sec career narrative with **Intel as the current role**, then each role as a one-breath trace.
2. Whiteboard all **5 anchor traces** cold; time yourself to <60 sec each.
3. Tell bug story (A) in 90 sec, ending on the coverage lesson; then map every mechanic to a day-file.
4. Answer "why are you in IP-Val / why silicon" without disparaging any past team (constraints → contribution).
5. Frame Microsoft in ≤30 sec as the bridge, not the headline — measurement, not architecture.
6. "Tell me about a hard bug" → A; "cross-functional work" → C; "an owned bug" → B. Pick instantly.
7. Separate P-states / C-states / D-states in three sentences ([Day 26](archive/day26_dvfs_pstates_pvt.md)/[Day 27](archive/day27_cstates_pkgc_s0ix.md)/[Day 35](archive/day35_pcie_power_management.md)).

---

### Exit Check - [day40_intel_role_narrative.md](archive/day40_intel_role_narrative.md)

- [ ] Deliver the 30-sec career narrative with **Intel current**, Microsoft as the prior chapter.
- [ ] Trace all four roles in one breath each (role → owned → transferable skill).
- [ ] Whiteboard the 5 anchor traces in <60 sec each.
- [ ] Tell 2 bug stories (A + B) in ≤90 sec, each ending on the coverage lesson.
- [ ] Frame Microsoft as measurement/cross-vendor validation without overclaiming architecture.
- [ ] Answer why-silicon / why-IP-Val with the constraints→contribution mindset.
- [ ] State the identity shift and run the plan as a continuous foundation build.

### Concept - [day41_final_mock.md](archive/day41_final_mock.md)

The final day is a **rehearsal day, not a learning day.** You've covered all eight phases — **C foundations (P0), digital design (P1), clocking & CDC (P2), memory (P3), CPU architecture + IP func-val (P4), verification engineering (P5), CPU power management (P6), interconnect & PCIe (P7)** — plus the reasoning + narrative wrap (P8). Today you run **one timed mock interview loop**, sweep the flashcards, and lock the bug stories. The goal is **density and recall**, not new material. The framing throughout is **Intel IP-Val as the current role** ([Day 40](archive/day40_intel_role_narrative.md)).

### Exit Check - [day41_final_mock.md](archive/day41_final_mock.md)

- [ ] One full timed 90-min mock loop completed and self-reviewed.
- [ ] 5 questions for interviewer written down.
- [ ] All 4 bug stories told in 90 sec each, self-recorded.
- [ ] Flashcards run as a 1-minute drill across all sections.
- [ ] One-sentence opener memorised.
- [ ] You can name the right tool for any pre-Si or post-Si question without hesitating.

---

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S17](s17_cstates_s0ix_device_pm.md) | [Foundation plan](FOUNDATION_PLAN.md)

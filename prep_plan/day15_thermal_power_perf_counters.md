# Day 15 — Thermal, Perf Counters, Multi-IP Interference

## Concept

The chip operates inside a **junction-temperature** (Tj) envelope: $T_j = T_{ambient} + P \cdot \theta_{JA}$. Cross **Tjmax** and `PROCHOT#` asserts → throttle. **Modern SoCs are multi-IP shared-resource systems** (CPU + GPU + NPU + ISP) where memory bandwidth, LLC, NoC, IOMMU and power budget are contended; performance bugs are usually one of five interference classes. The **PMU** measures it: top-down methodology classifies every pipeline slot into Frontend / Backend-Memory / Backend-Core / Bad-Speculation / Retiring. **RAPL** measures energy. The figure of merit is **perf-per-watt** or **EDP** (energy × delay), not raw throughput.

## Why this day in the flow

Days 12-14 covered active power, idle power, and the firmware bug stories around them. Day 15 is **how you measure all of it** + **what goes wrong when many IPs share resources**. Forms the bridge into Day 16 (pipeline / SIMD) and Day 17 (concurrency / DSA). In the Layer map this completes **Layer 5** and feeds Layer 6.

---

## Deep Study Notes

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

Per-core DVFS (Day 12) lets you throttle the hot core while neighbours stay at P0.

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

*Continuing the scaffold from [Day 12 Sec 8a](day12_dvfs_pstates_pvt.md). Steps 5-6 of the "design NPU + CPU + multi-SoC PnP tradeoff" answer.*

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
| Single-thread fast, multi-thread doesn't scale | LLC contention high, snoop traffic high | false sharing or coherence storm (Day 5) |
| Latency variance run-to-run | C-state residency high | idle states between bursts; pin governor |

---

## Bugs in this Topic

- **Throttle at Tj=85°C with spec Tjmax=100°C:** DTS calibration fuses missing → sensor reads 15°C high. Verify with external thermocouple.
- **Throttle without thermal cause:** RAPL package-power limit (`PL1`/`PL2`) too aggressive in BIOS. Read `MSR_CORE_PERF_LIMIT_REASONS`.
- **AVX-512 makes scalar code slower:** license-level transition window. Profile transitions; consider `-mprefer-vector-width=256`.
- **Workload "got slower" between two runs, IPC unchanged:** ref cycles ≫ core cycles → frequency dropped. Throttle reason MSR.
- **CPU starves under GPU streaming DMA:** LLC thrash. Apply CAT/CLOS partitioning.
- **NPU EP fallback to CPU per-op:** unsupported op forced fallback; per-fallback tensor copy dominates inference latency. Count fallbacks; quantise/reshape model.
- **HW prefetcher hides a missing software prefetch:** silicon A passes, silicon B (different prefetcher heuristic, or HW PF disabled in BIOS) regresses. Triage: disable HW PF (Intel MSR `0x1A4`) and re-run.
- **RAPL says 45 W, wall says 65 W and you call it a bug:** not a bug — VR loss + DRAM (not on client RAPL) + platform + PSU efficiency. Use RAPL for relative compare, wall for absolute claims.

---

## Interview Explanation (60-90 sec)

> "Thermal in silicon comes back to one equation: Tj equals ambient plus power times theta-JA. The thermal stack — die, TIM, IHS, sink, ambient — is just resistors in series. Cross Tjmax and PROCHOT asserts and the PCU drops F/V. The first thing I check on a thermal complaint is whether DTS is calibrated, because a sensor offset will throttle a chip that's nowhere near hot. On a multi-IP SoC there are five interference classes — memory BW, LLC thrash, NoC congestion, power budget, IOTLB pressure — each with a counter that names it. PMU is how I measure: top-down classifies every pipeline slot into frontend, backend memory, backend core, bad spec, retiring. IPC alone doesn't tell you anything; pair it with TMA. RAPL gives me joules per domain; perf-per-watt and EDP are the figures of merit, not raw throughput. The hygiene that separates real numbers from noise is fixed-frequency runs, dropped warmup, geomean for ratios. Most perf bugs I've chased reduce to: throttle reason MSR, then top-down, then a per-counter ratio with a threshold."

---

## Architect Drills

*Thermal is physics; PMU/RAPL/top-down are abstractions over it. Reason from constraints — what each model captures and where it leaks. No answer key.*

**D1. Why is $T_j = T_a + P\theta_{JA}$ a useful model when in reality the die is non-uniform?**
*Constraints to weigh:* lumped-parameter approximation, thermal RC network of die-TIM-IHS-sink-air, hotspot vs average, when the model breaks (transient PROCHOT). What does a per-tile thermal model buy that justifies its complexity — and where would steady-state lumped still win?

**D2. PROCHOT exists at the package level. Reason about why per-IP thermal throttling isn't more aggressive — what stops a future SoC from per-tile DVFS?**
*Constraints to weigh:* per-IP DTS sensor area, PCU latency to react per tile, voltage-rail granularity, software visibility (one PROCHOT bit vs N), validation matrix. What does Apple's per-block thermal headroom actually cost in silicon and FW?

**D3. RAPL underestimates wall power by 20-30%. Construct the cause stack and reason about which gap is fixable.**
*Constraints to weigh:* VR efficiency (model vs measurement), DRAM not in client RAPL, platform devices, PSU efficiency. What would a "true platform RAPL" need that today's doesn't have — sensors, calibration, or scope?

**D4. Top-down PMU classifies every slot into 4 buckets. Why exactly 4 — could it be 2 or 8?**
*Constraints to weigh:* what each bucket isolates (frontend / backend / bad-spec / retiring), counter budget on the PMU, actionability for a perf engineer, cross-vendor portability of the methodology. What does Intel's TMA L2 add at 6+ buckets, and where does the marginal information die?

**D5. HW prefetchers can hide a missing software prefetch. Reason about why disabling HW PF is a triage step, not a fix.**
*Constraints to weigh:* silicon-A vs silicon-B prefetcher heuristic differences, BIOS-disabled scenarios, perf cost of explicit prefetch, what the bug-report should say (data, not "disable PF"). When does software prefetch genuinely beat HW prefetch?

**D6. AVX-512 license-level cuts F to maintain V margin. Construct a workload where AVX-512 is a net loss vs AVX-256.**
*Constraints to weigh:* fraction of time in vector code, recovery window after license drop, scalar-around-vector latency, license-transition cost. What does Intel's AMX do differently to avoid this trap, and why didn't earlier vector ISAs?

**D7. Five interference classes on a multi-IP SoC (BW, LLC, NoC, power, IOTLB). Construct the diagnostic decision tree from a single "perf is bad" signal.**
*Constraints to weigh:* counter-per-class, what symptoms overlap (BW-starvation looks like LLC-thrash if you only see IPC), order of cheap-to-expensive checks. Build the triage in the right order — which class do you rule in/out first and why?

---

## Coding A — Systems C: Tj model + PROCHOT + RAPL delta + IPC

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    double tj, ta, theta, prochot;
    bool   throttled;
} thermal_t;

static void thermal_step(thermal_t *m, double power, double dt) {
    double tj_ss = m->ta + power * m->theta;
    double tau = 5.0;
    double a = dt / (tau + dt);
    m->tj += a * (tj_ss - m->tj);
    if (!m->throttled && m->tj >= m->prochot) {
        m->throttled = true;
        printf("PROCHOT asserted at Tj=%.1f\n", m->tj);
    } else if (m->throttled && m->tj < m->prochot - 5) {
        m->throttled = false;
        printf("PROCHOT cleared at Tj=%.1f\n", m->tj);
    }
}

/* RAPL delta -> watts */
static double rapl_watts(uint64_t e0, uint64_t e1, uint64_t unit_uj, double dt) {
    return ((double)(e1 - e0) * unit_uj) * 1e-6 / dt;
}

/* Top-down dominant bucket */
typedef struct {
    double frontend_bound, bad_spec, retiring, mem_bound, core_bound;
} tma_t;

static const char *tma_dominant(const tma_t *t) {
    double m = t->frontend_bound; const char *n = "Frontend";
    if (t->bad_spec   > m) { m = t->bad_spec;   n = "Bad Speculation"; }
    if (t->mem_bound  > m) { m = t->mem_bound;  n = "Backend Memory"; }
    if (t->core_bound > m) { m = t->core_bound; n = "Backend Core"; }
    if (t->retiring   > m) { m = t->retiring;   n = "Retiring"; }
    return n;
}

int main(void) {
    thermal_t m = { .tj = 25, .ta = 25, .theta = 0.3, .prochot = 95 };
    for (int s = 0; s < 30; s++)
        thermal_step(&m, m.throttled ? 75.0 : 125.0, 1.0);
    printf("Final Tj=%.1f throttled=%d\n", m.tj, m.throttled);

    printf("RAPL: %.2f W over 1s\n", rapl_watts(0, 65000000, 1, 1.0));

    tma_t t = { .frontend_bound = .15, .bad_spec = .05,
                .retiring = .25, .mem_bound = .45, .core_bound = .10 };
    printf("Top-down dominant: %s\n", tma_dominant(&t));
    return 0;
}
```

---

## System Thinking — Why thermal management is a *control loop*, not a setting

**Frame:** a chip running at full speed dissipates more power than its package can shed without exceeding $T_j$ (junction temperature) limits. You can't fix this by static design — the *workload* determines power, and workloads are unknown ahead of time. The chip must measure temperature in real time and modulate frequency/voltage to stay within thermal limits. That's a feedback control loop running in firmware (PCU/RPMh) on a millisecond timescale. Performance counters exist to give software *visibility* into what that loop and the workload are actually doing — because by the time the user notices throttling, the bug has already happened.

### Q: Why does thermal throttling exist? Can't designers just spec the cooling?

Three reasons static cooling spec is wrong:
1. **Worst-case is too expensive.** Designing the cooler for sustained max-power means a much bigger heat-sink/fan than the *typical* workload needs. Throttling lets you ship smaller cooling for typical loads and gracefully degrade on the rare 100% sustained workload.
2. **Ambient varies.** The same laptop runs in a 20°C office and a 35°C cafe. With $T_j = T_a + P \cdot \theta_{JA}$, ambient is a free 15°C of headroom you'd waste by spec'ing for the cafe.
3. **Leakage feedback.** Hot → more leakage → hotter. Without throttling, this is unstable — the chip would runaway. Throttling is the negative feedback that breaks the loop.

**Throttle types as different control responses:** PROCHOT (cap frequency), VRM-current limit (cap power), $T_j$ critical (forced shutdown). Modern silicon has 5+ throttle inputs feeding the same governor; understanding which one fired is the first debug step in any "why is my CPU slow" investigation.

### Q: Why are performance counters sampled, not always-on summaries?

Because counting every event would cost area and power that's not worth it for events you don't care about. PMU is built around a small number of *programmable* counters (4-8 typical, 32 on big servers) that you configure to count specific events: cache misses, branch mispredicts, TLB misses, retired instructions, FP ops, etc. You sample because:
- **Selection.** You only care about a few events at a time. Programmable beats fixed.
- **Cost.** A counter increments every event — each increment is cheap, but the wires + flops cost area. Fixed-function counters for everything would balloon the PMU.
- **Statistical sufficiency.** For most analysis, sampling at 1 kHz over a 10-second run gives a reliable distribution of where time was spent.

**Tradeoff:** you can miss bursty events between samples. For correctness debugging (find *the* miscompiled load) you use precise events (PEBS / SPE) that capture the exact PC of the sampled event — higher overhead, but no aliasing.

### Q: Why is the $T_j$ feedback loop dangerous?

Leakage power rises exponentially with temperature: $P_{leak} \propto e^{T/T_0}$. Combined with $T_j = T_a + P \cdot \theta_{JA}$:

- Workload runs hot → $T_j$ rises → leakage rises → total $P$ rises → $T_j$ rises faster.

Without intervention, this is a runaway. The control loop must (a) detect $T_j$ rising fast, (b) drop V/F before steady-state is reached, (c) hold there until $T_j$ trends down. **Slow control loops oscillate** (drop F → cools → raise F → hot again → drop F…) — visible as choppy performance under sustained load. Tuning the loop's gain and time constant is a real validation activity.

### One-liner

*"Thermal management is a feedback control loop because workload power isn't predictable and worst-case cooling is too expensive. Throttling is the negative feedback that breaks the leakage-temperature runaway. Perf counters are sampled because programmable beats fixed and statistical sampling is enough for most analysis. When you see weird performance under load, the first question is always: which throttle reason fired, and is the control loop oscillating?"*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **thermal as control loop** — *what:* sense → decide → actuate → repeat / *why:* workload power is unpredictable, worst-case cooling too costly / *how:* DTS sensors → PCU → DVFS throttle / *connects:* throttle is success not failure; oscillation = bad gains
- **RAPL** — *what:* Running Average Power Limit / *why:* enforce TDP without instantaneous-power HW / *how:* MSR counters + sliding window cap / *connects:* turbo budget, PL1 (sustained) / PL2 (burst)
- **perf counters (PMU)** — *what:* HW event counters / *why:* see what CPU actually did, not what you assumed / *how:* PMU MSRs, perf / VTune / pmu-tools / *connects:* cache-misses, branch-mispred, IPC attribution
- **Tj / TjMax** — *what:* junction temperature, max safe junction / *why:* reliability spec / *how:* on-die DTS reports / *connects:* throttle threshold, why Pmax shifts down with rising T
- **leakage-temperature runaway** — *what:* hotter → more leakage → hotter / *why:* $I_{leak}$ exponential in T / *how:* throttle breaks the loop / *connects:* why deep C-states matter, why cooling matters even at idle

## Bug Anchor

- **Day 12 (DVFS):** Turbo / V-droop / license-level interactions live there.
- **Day 13 (PkgC6 / S0ix):** residency counters scored here.
- **Day 19 (NPU runtime bugs):** EP fallback and Paint SR / Camera starvation are multi-IP perf bugs.
- **Day 20 (Debug method):** `perf stat`, RAPL, ETL/WPA, throttle-reason MSR.

## Exit Check

- [ ] Wrote $T_j = T_a + P\theta_{JA}$ and named each piece of the thermal stack.
- [ ] Stated PROCHOT vs DTS-offset bug.
- [ ] Listed 5 multi-IP interference classes + a counter each.
- [ ] Walked TMA hierarchy and named one event per leaf.
- [ ] Stated RAPL ≠ wall and quantified the gap.
- [ ] Gave the geomean-vs-arithmetic-mean rule.
- [ ] Stated EDP and the Pareto knee idea.

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

## Practice Q&A

**Q1. Apply $T_j = T_a + P\theta_{JA}$. $T_a = 35$, $P = 80$, $\theta_{JA} = 0.4$.**
$T_j = 35 + 32 = 67$°C. With $\theta_{JA} = 0.8$ (dried TIM): 99°C — at PROCHOT.

**Q2. PROCHOT asserts at 85°C, spec is 100°C. Top 3 suspects?**
DTS calibration fuses; FW/BIOS-overridden threshold; bidirectional PROCHOT from a neighbour.

**Q3. Why is hotspot temperature higher than die-average and why care?**
Density variation. Throttle on hotspot, not average — single sensor degradation.

**Q4. Voltage droop + AVX-512 — relate them.**
Load step → V droops → if V < Vmin for F → setup violation. Mitigation: license levels preemptively cut F so V margin holds.

**Q5. Five multi-IP interference classes + their counters.**
BW starvation (per-agent BW), LLC thrash (occupancy/miss), NoC congestion (link util), power budget (RAPL), IOTLB (miss + walk latency).

**Q6. Why is geomean correct for SPEC and arithmetic wrong?**
Arithmetic mean of ratios depends on baseline; geomean is invariant under rescaling.

**Q7. RAPL says 45 W, wall says 65 W. Account for the gap.**
VR loss (~5-15%), DRAM not in client RAPL, platform devices, PSU efficiency.

**Q8. IPC unchanged but wallclock 30% slower between runs. First thing to check?**
`MSR_CORE_PERF_LIMIT_REASONS` — frequency dropped (thermal / RAPL / current). Confirm via ref cycles vs core cycles.

**Q9. NPU EP fallback — why is it a perf problem and how do you spot it?**
Each fallback inserts NPU↔host tensor copy + CPU compute. Count `EP_assignment == CPU` ops per inference; mitigate by quantising/reshaping.

**Q10. CAT/CLOS — when does it help, when does it hurt?**
Helps with one noisy neighbour evicting a small hot working set. Hurts when workloads are bursty — static partition under-utilises capacity. Modern Intel allows runtime reconfig.

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

## Coding B — Interview DS&A: Sliding Window Maximum (LC 239) — counter-burst peak detector

```python
from collections import deque
def maxSlidingWindow(nums, k):
    dq, out = deque(), []
    for i, x in enumerate(nums):
        while dq and dq[0] <= i - k: dq.popleft()
        while dq and nums[dq[-1]] < x: dq.pop()
        dq.append(i)
        if i >= k - 1: out.append(nums[dq[0]])
    return out
```

---

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

# Day 12 — DVFS, P-states, PVT Corners, Binning

> **Source merge:** [30day_plan/day04_dvfs_power.md](../30day_plan/day04_dvfs_power.md), [30day_plan/day06_pvt_corners.md](../30day_plan/day06_pvt_corners.md). 30-day files are unchanged.

## Concept

Power has two pieces: **dynamic** $P_{dyn} = C V^2 F$ and **static** (leakage), with leakage **exponential in temperature**. **DVFS** exploits the $V^2$ term by lowering both $V$ and $F$ when full performance isn't needed. **P-states** are the discrete operating points; **Turbo** is the opportunistic above-P0 mode that uses thermal/power headroom from idle cores. Every chip lives at a unique **PVT corner** (Process × Voltage × Temperature); validation must cover the worst cases (FF for power/thermal, SS for timing). After fab, chips are **speed-binned** into SKUs, each with its own DVFS table.

## Why this day in the flow

After Days 1-11 (CPU/memory/PCIe/NVMe — the *what*), Days 12-15 cover **how the platform manages power and thermals around all of it**. DVFS is the active-power lever; Day 13 (C-states) is the idle-power lever; Day 14 (ADR/PCU/Core C6 bugs) is the firmware-side bug story; Day 15 is thermals + perf counters. Together they form **Layer 5 (Power/Thermal)** in the 30-day flow.

---

## Deep Study Notes

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

On Snapdragon (Day 9 anchor), DVFS is voted into **RPMh**:

- Each requester (Apps cluster, Modem, ADSP, CDSP/NPU) sends `(active, sleep, wake)` votes.
- RPMh aggregates **max** per resource.
- Programs PMIC over SPMI.
- TCS sequences fire on idle entry/exit for fast wake.

**The catch on big.LITTLE / DynamIQ:** cores often share a voltage rail by cluster. Per-core *frequency* is real; per-core *voltage* is not — rail V = max across that rail's cores. Validation: characterise actual V = `max(per-core requests)` and find waste.

### 9. Perf counters and IPC (forward to Day 15)

$$\text{IPC} = \frac{\text{instructions retired}}{\text{cycles}}$$

`perf stat` is the entry point on Linux. IPC < 1.0 ≈ memory-bound or branch-bound. Goes up with: better branch prediction, hot-cache access, ILP. Goes down with: cache misses, branch mispredicts, dependency chains, contention.

Useful counters: `instructions`, `cycles`, `cache-misses`, `branches`, `branch-misses`, `LLC-load-misses`, `dTLB-misses`. Validation engineers turn raw counters into ratios + thresholds + alerts.

---

## Bugs in this Topic

- **V-F transition reversed (raised F before V):** intermittent data corruption when OS jumps from P3 to P0; symptom looks like memory corruption or random `#GP`. Fix: enforce V-first sequence in microcode / firmware.
- **Turbo never engages on a one-thread workload:** PCU thinks the package is at thermal limit because DTS is reading miscalibrated; fix DTS calibration or the offset MSR.
- **Throttling under sustained load (PROCHOT asserted):** thermal solution undersized or fan curve too lazy; or chip is at an FF corner running hot. Check DTS, fan PWM, ambient.
- **One SKU hangs at the slow corner under cold-boot:** binning DVFS table didn't margin enough for SS at -20 °C; raise V at the lowest P-state for that bin.
- **Per-core DVFS request appears wasted on Snapdragon:** cluster voltage rail = max across cores; one core requesting Vmax pulls the whole rail up. Find the offender.
- **VRM droop on load step causes a transient hang:** missing decoupling, or VRM slew rate too slow; raise V floor or add cap.
- **Aging-related performance regression after 2 years:** NBTI / HCI shifted Vth; the same DVFS table no longer meets timing at the slow OPPs. Retune voltage offsets or accept derating in firmware.

---

## Interview Explanation (60-90 sec)

> "Power on a chip is dynamic — C V-squared F — plus leakage, which is exponential in temperature and is now 30 to 40 percent of total at 7 nm and below. DVFS exploits the V-squared term: when full performance isn't needed, lower V and F together along the V-F curve. P-states are the discrete operating points; Turbo is the opportunistic mode that takes thermal and current headroom from idle cores. The transition order matters: raise V before F when speeding up, drop F before V when slowing down — reverse either and you get setup violations. Every chip lives at a different PVT corner — FF is fast and hot, worst for power; SS is slow and cool, worst for timing. After fab, chips are speed-binned into SKUs and each bin gets its own DVFS table. On Snapdragon I'd add that DVFS requests go through RPMh, which aggregates votes from Apps, Modem, ADSP, CDSP and programs the PMIC; per-core frequency is real but voltage is per-cluster on most SKUs because cores share a rail."

---

## Practice Q&A

**Q1. Why is V squared in dynamic power but F linear?**
Energy per switch = $\frac{1}{2} C V^2$; switches per second ∝ $F$. Multiply: $P = \frac{1}{2} C V^2 F$.

**Q2. Why raise V before F when speeding up?**
F depends on transistors switching fast enough at supplied V. Raise F first → setup violation → data corruption. Raise V first → timing margin → safe to raise F.

**Q3. Where does Turbo's frequency budget come from?**
Idle cores burn near zero dynamic and a fraction of leakage; freed thermal/current budget reallocates to active cores until DTS or Iccmax says stop.

**Q4. Why does leakage threaten thermal runaway?**
Leakage rises exponentially with T → more heat → more leakage. Without throttle (PROCHOT, DTS), the chip self-heats to fault.

**Q5. FF vs SS — which is worst for what?**
FF: worst for power/thermal (high speed + high leakage). SS: worst for timing (slow → may miss target F at the rated V).

**Q6. Why can't pre-silicon cover all PVT corners?**
200+ at advanced nodes; computationally infeasible. Pre-Si picks worst cases, post-Si finds what local mismatch / aging / real cooling exposes.

**Q7. What is speed binning and how does it shape the DVFS table?**
Sort chips by max sustainable F; assign SKU; per-bin DVFS table is calibrated to the bin's actual silicon (an FF chip can hit P0 at lower V than an SS chip).

**Q8. On big.LITTLE DynamIQ, why is per-core voltage often a lie?**
Cores in a cluster share a PMIC rail; rail V = max across that cluster's cores; per-core *F* is real, per-core *V* is not.

**Q9. Linux: how does the OS request a P-state?**
`cpufreq` governor (e.g., `schedutil`) sets target → driver (`intel_pstate`, `qcom-cpufreq-hw`) writes MSR/CSR → HW relocks PLL and ramps VRM.

**Q10. IPC < 1 — what does that suggest?**
Memory-bound or branch-bound. Investigate cache miss rate, branch mispredict, stalls.

---

## Coding A — Systems C: DVFS table lookup with thermal cap + IPC counter

```c
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef struct { uint32_t freq_mhz; uint32_t volt_mv; } opp_t;

static const opp_t table[] = {
    {  400,  750 }, /* P4 */
    {  800,  850 }, /* P3 */
    { 1200,  950 }, /* P2 */
    { 1800, 1050 }, /* P1 */
    { 2400, 1150 }, /* P0 */
};
static const size_t N = sizeof table / sizeof *table;

/* Pick lowest OPP that meets target_mhz; cap by tj. */
static const opp_t *select_opp(uint32_t target_mhz, int tj_c) {
    uint32_t cap_mhz = (tj_c >= 90) ? 1200 : 0xFFFFFFFFu;
    for (size_t i = 0; i < N; i++) {
        uint32_t f = table[i].freq_mhz;
        if (f > cap_mhz) break;
        if (f >= target_mhz) return &table[i];
    }
    return &table[N-1];
}

/* Apply V-F change with correct sequencing. */
static void apply_opp(const opp_t *cur, const opp_t *next) {
    if (next->freq_mhz > cur->freq_mhz) {
        printf("  RAMP UP:   V %u→%u mV first, then F %u→%u MHz\n",
               cur->volt_mv, next->volt_mv, cur->freq_mhz, next->freq_mhz);
    } else {
        printf("  RAMP DOWN: F %u→%u MHz first, then V %u→%u mV\n",
               cur->freq_mhz, next->freq_mhz, cur->volt_mv, next->volt_mv);
    }
}

/* IPC tracker */
typedef struct { uint64_t insns, cycles; } pmu_t;
static double ipc(const pmu_t *p) { return p->cycles ? (double)p->insns / p->cycles : 0.0; }

int main(void) {
    const opp_t *cur = select_opp(400, 50);
    const opp_t *next = select_opp(2000, 50);
    apply_opp(cur, next);
    cur = next;
    next = select_opp(2400, 95);   /* thermal cap engages */
    apply_opp(cur, next);

    pmu_t p = { .insns = 15000000000ULL, .cycles = 12345678900ULL };
    double i = ipc(&p);
    printf("IPC=%.3f%s\n", i, i < 1.0 ? "  [memory/branch bound?]" : "");
    return 0;
}
```

## Coding B — Interview DS&A: Best Time to Buy and Sell Stock (LC 121)

Single-pass min tracking — same shape as a power-counter rolling baseline.

```python
def maxProfit(prices):
    lo, best = float('inf'), 0
    for p in prices:
        if p < lo: lo = p
        elif p - lo > best: best = p - lo
    return best
```

---

## Bug Anchor

- **Day 13 (C-states, S0ix):** the idle-power side of this story.
- **Day 14 (ADR / PCU / Core C6):** the firmware bug stories that come from PVT + sequence violations.
- **Day 15 (Thermal + perf counters):** DTS, throttle telemetry, IPC scoring.
- **Day 9 (Snapdragon RPMh + PMIC):** the QC name for the same loop.

## Exit Check

- [ ] Wrote $P_{dyn} = \tfrac{1}{2}CV^2F$ and explained each variable.
- [ ] Stated V-first vs F-first rule for ramp up vs ramp down.
- [ ] Listed FF vs SS corners and what each is worst for.
- [ ] Explained Turbo headroom in 30 seconds.
- [ ] Computed IPC from `perf stat` numbers.
- [ ] Mapped Snapdragon DVFS through RPMh + PMIC.

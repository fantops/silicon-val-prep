# Day 12 — DVFS, P-states, PVT Corners, Binning

## Concept

Power has two pieces: **dynamic** $P_{dyn} = C V^2 F$ and **static** (leakage), with leakage **exponential in temperature**. **DVFS** exploits the $V^2$ term by lowering both $V$ and $F$ when full performance isn't needed. **P-states** are the discrete operating points; **Turbo** is the opportunistic above-P0 mode that uses thermal/power headroom from idle cores. Every chip lives at a unique **PVT corner** (Process × Voltage × Temperature); validation must cover the worst cases (FF for power/thermal, SS for timing). After fab, chips are **speed-binned** into SKUs, each with its own DVFS table.

## Why this day in the flow

After Days 1-11 (CPU/memory/PCIe/NVMe — the *what*), Days 12-15 cover **how the platform manages power and thermals around all of it**. DVFS is the active-power lever; Day 13 (C-states) is the idle-power lever; Day 14 (ADR/PCU/Core C6 bugs) is the firmware-side bug story; Day 15 is thermals + perf counters. Together they form **Layer 5 (Power/Thermal)** in the system-flow layers.

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

### 8a. Multi-SoC / multi-IP DVFS coordination (the AMD design probe)

*"Design the power/perf tradeoff for an NPU + CPU + multi-SoC system" — walk this 6-step scaffold. Steps 1-4 land here; steps 5-6 land in [Day 15 Sec 5a](day15_thermal_power_perf_counters.md).*

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

Validation matrix that falls out: cross-product of (per-IP P-state) × (per-SoC PM-state) × (rail topology mode), with seam tests at every boundary — see [Day 20 Sec 2a](day20_debug_method_bug_polish.md).

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

## Architect Drills

*Power is physics, DVFS is policy on top of it. Reason about why every choice (V-before-F, discrete P-states, per-cluster rails, speed binning) lands where it does. No answer key.*

**D1. Why is dynamic power $\frac{1}{2}CV^2F$ — derive from first principles.**
*Constraints to weigh:* energy per switching event ($\frac{1}{2}CV^2$), switches per second ∝ frequency, what charges and discharges (gate capacitance), why V is squared and F is linear. Where does the formula break down (near-threshold voltage, leakage-dominated regime)?

**D2. Raise V before F when speeding up. Imagine a future technology where V and F are decoupled (separate rails per IP). Does the rule still apply?**
*Constraints to weigh:* setup-time slack as a function of V, what "F" even means without V (the PLL still locks), thermal lag vs electrical lag, glitch propagation across rail boundaries. Construct a workload where the rule reverses.

**D3. Why a discrete P-state ladder and not continuous V/F control?**
*Constraints to weigh:* characterisation matrix (each OPP needs validation), VRM transient response, OS predictability, common-OPP libraries reused across SKUs. What does Apple Silicon's near-continuous OPPs cost that Intel's discrete ladder saves — and what does it buy?

**D4. Speed binning sorts post-fab silicon into SKUs. Imagine perfect process control (no spread). What would the industry have to redesign?**
*Constraints to weigh:* yield economics of SKU mixing, datacenter procurement (fast vs slow bin pricing), power-perf curve as ensemble vs per-die. Where does binning create perverse incentives, and what would replace it?

**D5. Leakage is exponential in T and is now 30-40% of total at 7 nm. Reason about what's driving the curve toward 50%+.**
*Constraints to weigh:* shorter channels, lower threshold voltages, more cores idle vs active, on-chip thermal hotspots, power-gating granularity. What architectural choice (chiplets, big.LITTLE, dedicated accelerators) is a *response* to the leakage trend?

**D6. Per-core voltage on DynamIQ shares a rail. Construct the workload where this design loses *more* power than it saves.**
*Constraints to weigh:* one heavy core pulling rail to Vmax for the cluster, idle siblings still at high V (high leakage), heterogeneous workload mix. What does Apple's per-core LDO actually buy in P-core/E-core asymmetric workloads, and at what silicon cost?

**D7. Aging-related Vth shift over 2 years degrades slow-OPP timing margin. Reason about what a "self-tuning" DVFS table would look like.**
*Constraints to weigh:* in-field characterisation (PVT sensors, ring oscillators), telemetry storage in fuses/NVRAM, OS-visible vs FW-only adaptation, security of in-field re-tuning. What stops shipping this today — cost, validation, or trust?

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

---

## System Thinking — Why frequency and voltage are linked

**Frame:** if you could change frequency without changing voltage, DVFS would be trivial — just slow down to save power. You can't. Voltage and frequency are *coupled by physics*: every transistor needs a minimum $V_{dd}$ to switch within a clock period. Drop $V$ too low at the same $F$ and the next pipeline stage doesn't latch in time → silent corruption. P-states exist as a curated list of *safe* (V, F) pairs that the silicon was characterized to run at. PVT sensors close the loop because that characterization drifts with process variation, voltage droop, and temperature.

### Q: Why does dropping V save so much more power than dropping F?

Because $P_{dyn} = \alpha C V^2 F$. Drop F by 2×, save 2× dynamic power. Drop V by 2×, save 4×. *But you usually drop them together*, because lower F lets you run at lower V (more timing slack), so the *effective* savings is closer to $V^3$. **That's why DVFS works — the cubic-ish savings dominate the linear loss in throughput, *if your workload is latency-tolerant*.**

If the workload is latency-bound (interactive, real-time), slowing down hurts user experience more than it saves energy. That's why a *governor* exists — software has to decide what kind of workload is running and pick the operating point.

### Q: Why do governors exist? Can't hardware just pick the right P-state?

Hardware can react to load (utilization, IPC) but it cannot know **intent**. A 50% utilization could be:
- A streaming workload that wants throughput at minimum energy → lower P-state.
- A latency-sensitive workload that's idle waiting for input → stay at high P-state to respond fast.
- A throttled workload that *would* use 100% if allowed → raise P-state.

The OS knows (or guesses): scheduler hints, EAS (Energy-Aware Scheduling), per-task energy models. Modern systems use a hybrid — **HWP (Intel) / RPMh (Qualcomm)** lets HW make fast decisions in microseconds while SW provides hints (window of acceptable P-states, perf bias). This is the modern compromise: HW for speed, SW for intent.

### Q: Why PVT (Process / Voltage / Temperature) sensors?

Because silicon isn't uniform. Two dies from the same wafer can have a 100 mV $V_{min}$ difference at the same frequency — *process variation*. The voltage rail droops under sudden load changes — *voltage transient*. Leakage and timing both vary with temperature. If you set $V_{dd}$ at "worst-case process + worst-case temp + worst-case droop," you waste energy on every nominal die.

PVT sensors let the firmware (PCU on Intel, RPMh on QC, MCC on others) **adapt voltage in real time**: read the temperature sensor every few ms, read voltage droop counters, read process-variation fuses programmed at test — then tune the voltage just-above $V_{min}$ for *this die, this rail, this moment*. **AVS (Adaptive Voltage Scaling) commonly saves 10-20% over static V/F tables.**

### One-liner

*"DVFS works because $P_{dyn} = CV^2F$ and V and F are coupled — lower F enables lower V, giving near-cubic energy savings. P-states are a curated list of safe (V, F) pairs the silicon was characterized at. Governors exist because hardware sees load but not intent. PVT sensors close the loop in real time because every die is different and worst-case static voltage wastes energy on the typical die."*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **why DVFS** — *what:* dynamic voltage + frequency scaling / *why:* $P_{dyn}=αCV^2F$, V dominates / *how:* OS requests P-state, PCU/PMIC enforces / *connects:* race-to-idle vs steady-low strategy
- **V-F curve** — *what:* min V required for given F / *why:* setup timing closes only above Vmin(F) / *how:* characterized per die, per corner / *connects:* Vmin guardband, AVS shrinks it
- **P-states** — *what:* OS-visible (V, F) pairs / *why:* abstraction over silicon-specific points / *how:* PCU + PMIC implement, ACPI _PSS lists them / *connects:* turbo, EPP/HWP hint, why governor matters
- **PVT** — *what:* Process / Voltage / Temperature variation / *why:* every die is different / *how:* speed binning at fab, AVS sensors at runtime / *connects:* why margins exist, why hot chip slows even before thermal trip
- **race-to-idle vs crawl** — *what:* finish fast then sleep deep vs run slow steady / *why:* depends on idle leakage cost / *how:* governor decides via load history / *connects:* why race-to-idle wins on low-leakage workloads

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

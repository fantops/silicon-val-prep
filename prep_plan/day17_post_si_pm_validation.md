# Day 17 — Post-Silicon PM Validation

## Concept

Post-silicon PM validation is the discipline of proving — on **real silicon, in a real OS, on a real board** — that every PM state the pre-Si team signed off actually works under real workloads, real PVT corners, and real wake patterns. The toolkit shifts from waveforms + UPF to **OS telemetry + lab instruments + microcode patches**. The bug surface shifts from sequencer races to **firmware paths, governor policies, voltage margins, thermal interactions, and silicon variability across units**. Every escaped pre-Si bug surfaces here at 100× the debug cost — which is why the post-Si team's most valuable artifact is the **failure log that feeds back into the next pre-Si vplan.**

## Why this day in the flow

D16 closed pre-Si PM. **Today is what happens when the first samples land on a lab bench.** Power-on sequence, first PCU heartbeat, first C-state entry, first residency reading, first turbostat dump — and then the slow grind of finding the bugs that pre-Si missed. This is also the day with the two marquee debug stories you'll get asked about in interview: **ADR opcode-ordering** and **Core C6 wake-loss**.

In the Layer map this is the **closing day of Phase 3 (CPU PM)**, the bridge from "we validated this" to "the product PM works on the customer's silicon."

---

## Deep Study Notes

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

## Bring-up Flow — Lab Setup (Concrete)

```
Day-1 in the lab:
   ├─ Board mounted, rails probed, PMIC i2c bus connected
   ├─ JTAG / Lauterbach connected
   ├─ Serial console up
   ├─ PCIe rootport NVMe attached (for boot)
   ├─ Quarch in series with main rails
   └─ socwatch + turbostat tooling installed on host

First-light checklist:
   ├─ Scope: rail sequence matches PMIC spec
   ├─ Console: PCU heartbeat opcode visible
   ├─ Linux boot to login with all PM disabled
   └─ Capture baseline turbostat for "PM off"

PM enable, one knob at a time:
   ├─ Enable C1; verify Core%c1 climbs, no functional regression
   ├─ Enable C6; verify Core%c6 climbs, residency MSR ticks
   ├─ Enable PkgC6; verify Pkg%pc6 climbs; if 0%, run socwatch and find blocker
   ├─ Enable S0ix; verify display + USB + PCIe align
   └─ Enable DVFS; sweep workload, watch PkgWatt + Tj

Workload soak:
   ├─ SPECpower or steady idle for 24-72 hr
   ├─ Continuous Quarch capture
   ├─ Continuous socwatch CSV
   ├─ Crash / hang dump destination configured
   └─ Daily triage of any anomaly
```

---

## What's Required

- **Eval board** with all rails probable + PMIC i2c accessible.
- **JTAG probe** (Lauterbach TRACE32 or vendor equivalent) + ITP/DCI for Intel parts.
- **Power instrumentation** (Quarch / Monsoon / Keysight) in series with main rails.
- **Scope + logic analyser** for transient + signal-level debug.
- **PCIe protocol analyser** if validating PCIe PM (Lecroy Summit / Teledyne).
- **Host machine** running Linux with kernel + tools matched to silicon.
- **Tool stack** installed: `turbostat`, `socwatch`, `perf`, `msr-tools`, vendor SVTools.
- **PCU FW + side-load mechanism** so FW patches can be tested without re-spin.
- **PM-vplan from pre-Si** so post-Si tests cross-reference the same coverpoints.
- **Bug DB** wired to feedback channel into pre-Si vplan.

---

## Tools

| Layer | Tool | Use |
|---|---|---|
| **OS counters** | `turbostat`, `perf`, `/sys/devices/system/cpu/cpu*/cpuidle/*`, `/proc/timer_list` | Per-core residency, MHz, perf counters, timer deadlines |
| **Package counters** | Intel `socwatch`, ARM equivalent | Package-level breakdown, S0ix blocker attribution |
| **MSR access** | `rdmsr` / `wrmsr` from `msr-tools` | Direct MSR reads/writes |
| **Energy** | RAPL counters (MSR 0x611, 0x614, 0x618, 0x619) | Energy per domain over interval |
| **PMIC** | i2c-tools (`i2cget`/`i2cset`) + vendor tool | Rail-trim + telemetry |
| **JTAG / debug** | Lauterbach TRACE32, Intel ITP/DCI, OpenOCD | Halt, single-step, MSR dump |
| **Intel internal** | SVTools (PMC trace, scenario harness) | Intel-specific post-Si tooling |
| **Power instrumentation** | Quarch PPM, Monsoon HV, Keysight N6705 | Per-rail V/I waveforms |
| **Scope / LA** | Keysight, Tektronix, Saleae | Sequenced-wake droop, transient debug |
| **PCIe analyser** | Lecroy Summit T3, Teledyne PETrainer | PCIe LTSSM + L0s/L1/L1.2 transitions |
| **PCIe OS-level** | `lspci -vvv`, `setpci`, `lspci -PP` | Capability registers, ASPM control |
| **Thermal** | IR camera, on-die DTS via MSR | Thermal regression vs PM |
| **Scenario reuse** | Perspec-emitted post-Si C harness | Run pre-Si scenarios on real silicon |

---

## Bugs in this Topic

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

## Interview Explanation (60-90 sec)

> "Post-Si PM val is where the pre-Si coverage meets the lab bench. You bring up PM one knob at a time — C1, then C6, then PkgC6, then S0ix, then DVFS — and at each step you read OS counters with turbostat, package counters with socwatch, raw MSRs directly, and you have a Quarch power module on the rails so any anomaly has analog ground truth. socwatch is the blocker oracle — it tells you *which IP* is preventing PkgC6 entry. The bugs you find are firmware paths, governor policies, and silicon variability — things pre-Si couldn't see. My method is the 7-step framework: reproduce minimally, isolate the axis, descend one telemetry layer per failed isolation, check cross-IP via socwatch, side-load a PCU FW patch if available, root-cause, and feed back into the pre-Si vplan with a new coverpoint and SVA. I've worked through two of these end-to-end — the ADR opcode-ordering corruption that needed a microcode barrier, and a Core C6 wake-loss caused by PCU arbitration dropping simultaneous wakes — and in both cases the actual product fix was in firmware but the *lasting* fix was the pre-Si coverage rule that prevents the next variant."

---

## Architect Drills

*Post-Si is where pre-Si assumptions meet silicon reality. Reason without an answer key.*

**D1. socwatch reports PkgC6 = 0%. Cores all report C6 residency >90%. Reason about the next telemetry layer to consult.**
*Constraints to weigh:* what holds the package out (uncore, PCIe, DRAM, display, USB), what each layer reports, what's cheapest to capture first.

**D2. A PM bug reproduces 1 in 10⁴ entries. Reason about whether to chase it or waive.**
*Constraints to weigh:* customer exposure, severity (data loss vs perf hit), cost of reproduction time, risk if it interacts with a future feature.

**D3. The same PCU FW works on unit A but not unit B (same stepping, same board). Reason about the next move.**
*Constraints to weigh:* silicon variability (Vmin, retention margin), board-level differences (PMIC trim, capacitance), wafer position binning, sample size before declaring.

**D4. You add a coverpoint to the pre-Si vplan after finding a post-Si bug. Reason about how to ensure the coverpoint actually exercises the silicon failure mode rather than a sim-only abstraction of it.**
*Constraints to weigh:* what part of the failure is timing-dependent (only emul), what part is FSM-reachable (sim), whether SVA on the property is enough.

**D5. turbostat shows Core%c6 90% but PkgWatt unchanged from "no PM" run. Reason about what's wrong.**
*Constraints to weigh:* uncore power dominance, package not in PC6, sleep transistors not opening (FW bug), telemetry calibration.

**D6. The PCU FW patch fixes the bug in lab but the architect wants to know "is this safe in the wild?" Reason about how to prove generality.**
*Constraints to weigh:* sample diversity, corner sweep, soak runtime, regression scope, what could the patch break that the lab won't exercise.

**D7. Pre-Si reports 100% PM coverage closed; post-Si finds a bug that was theoretically reachable in sim. Reason about whether the failure was a coverpoint gap, an SVA gap, or a stim gap.**
*Constraints to weigh:* what each of the three would have caught, where the abstraction broke between spec and vplan, the difference between "covered" and "exercised under the same constraints as silicon."

---

## Coding A — Systems C: residency-counter delta + wrap-safe parser

A tiny utility that turns raw 32-bit residency MSR snapshots into per-interval residency-%, handling wrap. This is *exactly* what turbostat does, simplified.

```c
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

/* Wrap-safe delta on a 32-bit counter */
static uint32_t delta_u32(uint32_t now, uint32_t prev) {
    return now - prev;   /* unsigned subtraction wraps cleanly */
}

/* Convert delta-cycles + interval-ns + freq-MHz → residency % */
static double residency_pct(uint32_t dcyc, uint64_t int_ns, uint32_t mhz) {
    double cyc_avail = (double)int_ns * mhz / 1000.0;
    return 100.0 * dcyc / cyc_avail;
}

int main(void) {
    /* Sample MSR snapshots, interval = 1 s, 2.4 GHz */
    uint32_t snap[] = {
        0x00000000,  /* t=0 */
        0x047DEAB0,  /* t=1s, ~75M cyc in C6 -> ~3% */
        0xF8FA5B00,  /* t=2s wraps */
        0x00FA5B00,  /* t=3s post-wrap */
    };
    uint64_t interval_ns = 1000ULL * 1000 * 1000;
    uint32_t mhz = 2400;

    for (size_t i = 1; i < sizeof(snap)/sizeof(snap[0]); i++) {
        uint32_t d = delta_u32(snap[i], snap[i-1]);
        printf("t=%zus  raw=0x%08X  delta=%10u cyc  C6=%.2f%%\n",
               i, snap[i], d, residency_pct(d, interval_ns, mhz));
    }
    return 0;
}
```

---

## Bug Anchor

- **Day 14 (UPF):** retention/iso bugs from there surface here as "boots, crashes on first wake."
- **Day 15 (thermal/perf counters):** RAPL + DTS telemetry comes from this day.
- **Day 16 (pre-Si PM val):** every escape here generates a new vplan row there.
- **Day 22 (PCIe PM intersection):** PkgC + L1.2 blockers surface in socwatch here.
- **Master bug story 1: ADR opcode ordering** — anchored to this day.
- **Master bug story 2: Core C6 wake-loss** — anchored to this day.

## Exit Check

- [ ] Drew the 8-stage post-Si PM bring-up flow.
- [ ] Named the OS counters, package counters, MSR, and lab instruments for PM measurement.
- [ ] Wrote a turbostat output by hand and explained what each column means for PM.
- [ ] Explained socwatch as the blocker oracle.
- [ ] Recited the 7-step debug framework.
- [ ] Told the ADR opcode-ordering story in 90 sec end-to-end with root cause + pre-Si feedback.
- [ ] Told the Core C6 wake-loss story in 90 sec with the same structure.
- [ ] Stated three classes of silicon-variability-driven post-Si surprise.

# Day 20 — Debug Methodology and Polished Bug Stories

## Concept

Every silicon bug investigation follows the same **7-step framework**: Observe → Reproduce → Isolate → Hypothesise → Test → Root-cause → Fix & Verify. The stages of validation (RTL sim → emulation → FPGA proto → post-Si) each catch a different bug class, and the cost of finding a bug **rises ~10× per stage**, so where you find it matters as much as what you find. In an interview the bug story is delivered in **90 seconds** with a fixed shape: **Symptom → Hypothesis → How I narrowed it → Root cause → Fix → Learning**. The interviewer is not testing the bug; they are testing how your mind works.

## Why this day in the flow

Days 1-19 gave you the technical material. Day 20 is **how you talk about it**. Polished bug stories are what convert technical knowledge into hire-no-hire signal in the loop.

In the Layer map this is the **debug methodology + storytelling layer**.

---

## Deep Study Notes

### 1. Debug framework + triage signals

**7 steps:** Observe → Reproduce → Isolate → Hypothesise → Test → Root-cause → Fix & Verify.

**Repro rate = first triage signal:** 100% → functional (design/FW); intermittent → race/timing; PVT-corner only → V-droop/thermal/process; single-unit only → silicon defect.

**Isolation toolkit:** binary-search the test, JTAG single-step, swap SoC/board/PSU, freq/volt sweep, register-dump diff vs golden, disable prefetch/Turbo.

**Symptom → mechanism cheatsheet:** corruption → DMA/coherence/ECC; hang → deadlock/link-down; hot-only crash → timing margin/V-droop; emul-pass silicon-fail → analog/CDC; `0xFFFFFFFF` reads → PCIe down / BAR unmapped / in-reset; page-fault spike or EP fallback → Day 19 runtime issues.

### 5. Validation stages — bug-cost ladder

```
Design ── Sim (Hz-kHz) ── Emul (MHz) ── FPGA (10-100 MHz) ── Post-Si (GHz)
            │                │              │                    │
            │                │              │                    └── analog, PVT, real workload
            │                │              └── SRAM timing, full SW stack
            │                └── HW/FW integration bugs (transactor-based)
            └── functional + protocol bugs (full visibility)
```

| Stage | Catches | Misses |
|---|---|---|
| **RTL sim** | functional, protocol; full visibility | analog, real timing, real OS |
| **Emulation (Zebu/Palladium)** | HW/FW integration (real FW boots) | analog, process corners, transactor latency |
| **FPGA proto** | full SW stack at MHz | SRAM timing, real silicon analog |
| **Post-Si** | analog, process corner, V-droop, thermal, real interference | nothing — it's reality |

Cost-of-finding rises ~10×/stage. A bug found in sim is "free"; one found post-tape-out can delay a product cycle.

### 5a. Coverage plan — Power Management across pre-Si and post-Si (the one I lived)

> *Why this matrix exists alongside Section 5a:* every role I've owned reduces to a power-management surface — **L&T/SanDisk NVMe SSD** (APST / ASPM / HMB / WCTEMP / throttle), **Intel pre-Si Cambria Xeon** (PCU opcode flow, Core C6, PkgC, ADR, S0ix gating), **Microsoft NPU on Copilot+ PC** (HTP power islands, modem-style duty-cycling, thermal headroom shared with CPU/GPU). When an interviewer asks "how do you build coverage for PM," I answer with this matrix because I've actually filled cells in it.

**The PM coverage frame — three orthogonal axes:**

```
                  STATE SPACE                          TIME / TRANSITION                    ENERGY / RESIDENCY
              (where can we be?)                  (how do we get between states?)        (does it pay back?)
              ────────────────────                 ──────────────────────────────         ──────────────────────
   Pre-Si    enumerate every P/C/D/L state,        every entry/exit FSM,                 modelled wake cost vs
             every PkgC + per-core C combo,        every voltage/freq transition,         residency budget,
             every ASPM/APST level,                handshakes (PCU↔core, PCU↔PMIC,        break-even check is
             every PMIC rail collapse order        RPMh↔CPRSS, host↔NVMe APST)            a coverage cell

   Post-Si   real silicon residency at PVT,        wake latency at -40/+25/+85/+105 °C,   battery-life under
             rail current at every state,          droop at every transition,             real workload mix,
             leakage variance lot-to-lot,          1000× S0ix/S3/S4 cycles,              perf-per-watt vs
             thermal-coupled state shifts          throttle entry/exit timing             previous gen
```

**Coverage matrix — fill every cell, per project:**

| PM axis | Pre-Si (RTL / emul / virtual platform) | Post-Si (A0 on EVM / reference platform) | Gate that says "you're done" | Where I've owned it |
|---|---|---|---|---|
| **CPU P-states (DVFS)** | Directed P0…Pn ladder; every Δf transition; PCU opcode trace; voltage-rail handshake with PMIC model; turbo entry on multi-core load | Real workload P-state residency; turbo headroom at PVT; V-f curve characterisation; PROCHOT# on droop | All P-state transitions covered pre-Si; perf/W within ±5% of model post-Si | Intel Cambria pre-Si (PCU flow); MS NPU DVFS-equivalent (HTP voltage corners) |
| **CPU C-states (per-core)** | C0/C1/C1E/C6/C7/C10 entry/exit FSM; LAPIC ARAT wake; cross-core dependency (last-core-out → PkgC eligible); CFC FIFO clock-domain crossings | C-state wake latency at PVT corners; idle-injection sweeps; C6 wake under back-to-back ARAT (my Story 3); residency at idle vs busy-idle | Every per-core FSM transition covered pre-Si; wake latency in spec at all 4 corners post-Si | Intel Cambria pre-Si (Core C6 / CFC FIFO bug — Story 3) |
| **Package C-states (PkgC)** | PkgC2/C6/C10 entry conditions; PCU last-level coordination; uncore/LLC flush ordering; PCIe L1.2 dependency walk; PCU opcode log per entry | PkgC10 idle current in spec; S0ix entry with PCIe in L1.2 + USB suspended; 1000× cycles; wake from any of N sources | PCU opcode coverage pre-Si; idle current + wake-latency targets post-Si | Intel Cambria pre-Si (PkgC entry, ADR coupling) |
| **System sleep (S0ix / S3 / S4)** | S0ix LPI counters; constraint walk (every block must be in low-power); S3 register save/restore; S4 hibernate page-out path | S0ix residency under realistic idle (browser idle, screen-on idle, modem-connected); 1000× S3/S4 cycles zero data loss; resume time | All entry constraints covered pre-Si; residency + zero-loss post-Si | Intel pre-Si (S0ix gating); MS Copilot+ PC platform validation |
| **Clock gating (CG)** | Per-block CG enable matrix; CG-on / CG-off functional equivalence; glitch-free clock OR; reset-domain interactions | Real CG residency telemetry; perf delta CG-on vs CG-off; no functional regression at PVT | CG matrix 100% covered pre-Si; zero functional delta post-Si | Intel pre-Si (PCU clock-gating opcode sequencing) |
| **Power gating (PG) / power islands** | Per-island PG entry/exit FSM; isolation cell coverage; retention vs cold-start; wake-up sequencer; rail collapse + restore order | Real leakage savings at +85 °C; wake latency from PG; data integrity across PG cycles | PG FSM + isolation 100% pre-Si; leakage targets met at corners post-Si | Intel pre-Si (PG islands); MS NPU (HTP power islands per inference) |
| **Voltage / rail management (PMIC)** | PMIC handshake model (RPMh / SVID / PCU↔PMIC); rail-up/down ordering; droop response model; AVS/adaptive voltage scaling FSM | Real rail current per state; V-droop at transitions; PMIC firmware regression; lot-to-lot variance | Handshake protocol 100% pre-Si; rail current within spec post-Si | Intel pre-Si (PCU↔PMIC opcode sequencing — anchor for ADR Story 1) |
| **Thermal control loop** | **Pre-Si is isothermal — coverage is throttle-FSM only:** every throttle level, hysteresis FSM, sensor-to-throttle path, PROCHOT#/HOT# assertion | Real -40/+25/+85/+105 °C chamber; throttle entry/exit timing; perf-under-throttle correctness; 72 h soak; control-loop oscillation check; per-zone sensor calibration | Throttle FSM 100% pre-Si; control loop stable + zero corruption at all corners post-Si | NVMe SSD WCTEMP/CCTEMP (L&T/SanDisk); Intel PROCHOT path; MS NPU shared thermal budget with CPU/GPU |
| **PCIe link power (ASPM L0s/L1/L1.1/L1.2)** | Every ASPM entry/exit FSM; LTSSM × ASPM cross-product; PkgC↔L1.2 dependency walk; CLKREQ# handshake | ASPM under load × 24 h; L1.2 exit < 200 µs at PVT; per-rail current in each state; PMIC droop on exit; surprise removal in low-power | Every ASPM cap covered pre-Si; residency + wake-latency targets post-Si | Intel pre-Si (PCIe transactor — Story 6 anchor); NVMe SSD ASPM × APST cross-product (L&T/SanDisk) |
| **NVMe device power (APST / PS0…PS4)** | APST autonomous transition FSM; host-driven power-state set; idle-timer coverage; PS-transition mid-IO; latency vs power table sanity | APST residency under real fio mix; PS0↔PS4 wake latency; data integrity across transitions; ASPM × APST interaction; battery-impact under filesystem idle | All PS transitions + APST FSM covered pre-Si; per-state current + zero data loss post-Si | **L&T/SanDisk NVMe SSD validation — owned cell** |
| **NVMe HMB power-coupling** | HMB allocation / release FSM; HMB present vs absent equivalence; L2P writeback ordering when HMB throttled | HMB behaviour above WCTEMP (my Story 7); HMB integrity across S3/S4; HMB after surprise PERST# | FSM coverage pre-Si; thermal-coupled corruption check post-Si | **L&T/SanDisk DRAM-less NVMe SSD (my Story 7)** |
| **ADR / power-loss flow** | PLI assertion → PCU opcode sequence → IO quiesce → WPQ flush → DDR self-refresh; opcode-ordering assertions; ack-handshake check | Real PSU collapse on EVM; persistence verify across N power cycles; capacitor sizing margin | Opcode-sequence FSM + ordering 100% pre-Si; persistence verify post-Si | **Intel Cambria pre-Si — owned cell (my Story 1)** |
| **NPU / accelerator power** | Per-tile PG FSM; voltage corner coverage; DMA-quiesce before island collapse; runtime↔FW power handshake | HTP residency across inference mix; thermal-throttle entry under sustained AI workload; battery impact of always-on NPU features; shared thermal budget with CPU/GPU | FSM coverage pre-Si; residency + thermal-share targets post-Si | **MS Copilot+ PC NPU (QNN/ORT/HTP power) — owned cell** |
| **Wake sources / latency budget** | Every wake source × every sleep state matrix; latency budget per source; wake-coalescing FSM | Real wake latency per source at PVT; wake-storm under modem/Wi-Fi traffic; spurious-wake telemetry | All sources × states pre-Si; latency targets post-Si | Intel pre-Si (wake fabric); MS Copilot+ PC modem-standby |

**Phasing — PM-only, post-Si, in the order I'd actually run it:**

| # | Phase | What runs | What it catches |
|---|---|---|---|
| 0 | Smoke | Boot to OS, all rails up, CPU offlines/onlines clean, basic suspend/resume | Gross PM bringup breaks |
| 1 | Per-state functional | Force every C-state, P-state, ASPM level, APST PS; verify entry + exit + functional integrity | FSM gaps the pre-Si missed |
| 2 | Residency targets | Idle the platform; verify PkgC10, S0ix, ASPM L1.2, APST PS4 residency hits target % | Mis-budgeted constraints (one block keeping the package awake) |
| 3 | Wake latency | Wake from every source × every state; measure latency at -40/+25/+85/+105 °C | Wake-path SI / PMIC droop / clock-relock margin |
| 4 | Cycling endurance | 1000× S0ix, 1000× S3, 1000× S4; `fio --verify` survives all of them | Cumulative state-machine drift, retention bugs, leak-after-cycle |
| 5 | Thermal × power coupling | -40 / +25 / +85 / +105 °C × 72 h with full PM enabled; verify throttle entry/exit + data integrity | Bugs that only exist when state-machine + temperature interact (my Story 7) |
| 6 | Battery / perf-per-watt | Realistic workload mix (browser idle, video, AI inference, modem-connected standby); measure mWh per workload | Regression vs previous gen / vs spec floor |
| 7 | Negative / abuse | Surprise PERST# in L1.2; rail collapse mid-S3; thermal trip during ADR; wake-storm; concurrent S0ix entry + AER inject | Corner bugs at the seam between PM and protocol |

**Pre-Si exit gates (PM):**
- Every P / C / Pkg-C / ASPM / APST / PG FSM 100% state + transition coverage
- PMIC handshake protocol 100% covered (every opcode, every ack path)
- Throttle FSM 100% (sensor → throttle → recovery), even though temperature itself is not modelled
- ADR / power-loss opcode ordering covered with assertions
- Wake-source × sleep-state matrix 100%

**Post-Si exit gates (PM, PRQ-ready):**
- All residency targets met at idle and busy-idle (PkgC10 / S0ix / ASPM L1.2 / APST PS4)
- All wake latencies in spec at -40 / +25 / +85 / +105 °C
- 1000× S0ix / S3 / S4 with zero data loss, zero hang, zero spurious wake outside budget
- 72 h soak at +85 °C with full PM enabled — zero corruption, throttle clean, no oscillation
- Battery / perf-per-watt regression vs previous gen ≤ 0%
- 0 P0/P1 PM escapes; ≤ 3 P2 with documented workaround

**The seam where PM bugs hide (and why this matrix is the right shape):**

PM bugs are almost never in *one* axis — they live at intersections that pre-Si treats as separate cells:

- **C-state × clock domain** → my Story 3 (Core C6 wake lost when CFC FIFO mid-reset)
- **Power-loss × FW opcode order** → my Story 1 (ADR WPQ flush before IO quiesce ack)
- **Thermal × data path** → my Story 7 (HMB silent corruption above WCTEMP)
- **ASPM × PkgC dependency** → classic "S0ix won't enter because one EP refuses L1.2"
- **APST × ASPM cross-product** → device + link both want to sleep, host policy doesn't agree

**The line that lands the PM-flavoured offer:** "PM coverage is three axes — *state space*, *transition / time*, *residency / energy*. Pre-Si owns the first two completely: if a PM state or a PM transition lives in an enumerable FSM, it should never escape into silicon. Post-Si owns the third axis and everything that couples to PVT — real droop at a transition, real residency under a real workload, real thermal feedback into a state machine that pre-Si modelled as isothermal. The hard PM bugs all sit at the seam: C-state × clock-domain, power-loss × opcode-order, thermal × data-path. I've owned cells across the matrix on three different products — Intel pre-Si PCU/ADR/Core-C6, NVMe APST/ASPM/HMB at L&T and SanDisk, NPU power on Copilot+ PC at Microsoft — so when I look at a new PM cell, I know which axis it lives on and which seam to instrument first."

### 6. The 5 + 1 polished bug stories

These are the stories you carry into a loop. Each is a Day 1-19 fundamental put in narrative form.

| # | Class | Layer it tests | Day anchor |
|---|---|---|---|
| 1 | **PCU / ADR opcode-ordering hang** (Intel pre-Si Zebu) | FW sequencing, ordering, ADR power-loss flow | Day 14 |
| 2 | **NVMe CSTS.RDY never asserts (AQA mis-sized)** (L&T/SanDisk) | spec literacy, host bring-up | Day 11 |
| 3 | **Core C6 wake lost on back-to-back ARAT** (Intel pre-Si) | 2-clock-domain race, CFC FIFO mid-reset | Day 14 |
| 4 | **QNN/ORT page-fault + VM regression** (Microsoft + QC, platform) | host runtime memory lifecycle | Day 19 |
| 4b | **Paint SR starves Camera preview** (Microsoft, platform) | single-queue NPU scheduler | Day 19 |
| 5 | (back-pocket) **Silent EP fallback after model rebuild** | QDQ fusion, EP assignment | Day 19 |
| 6 | **PCIe Gen6 L0p re-scaling races a TLP in flight** (transactor enhancement, pre-Si) | new-feature spec literacy + transactor design + ordering at FLIT boundary | Day 10 |
| 7 | **NVMe HMB silent read corruption above WCTEMP** (post-Si only) | thermal × data-path coupling that pre-Si cannot see | Day 11 |

### 7. Story 1 — PCU / ADR opcode-ordering hang (90 sec)

> *Pre-Si, Cambria Xeon on Zebu emulation. ADR = Async DRAM Refresh: when AC power drops, on-board capacitors hold the rails just long enough for PCU firmware to push DRAM into self-refresh so writes survive the outage. A power-management flow under the worst possible deadline.*

> **1. Observe.** Simulated PSU loss, PLI asserted, PCU started its ADR opcode sequence — but DDR never entered self-refresh. Sequence hung mid-flow. iMC write-pending-queue (WPQ) never drained.
>
> **2. Reproduce.** 100 % on every PSU-loss injection. Functional bug, not a race.
>
> **3. Isolate.** Pulled the Zebu PCU opcode trace and diff'd it against the ADR spec sequence. Narrowed the failure window to two adjacent opcodes: `IO_QUIESCE` followed by `WPQ_FLUSH`.
>
> **4. Hypothesise.** PCU was issuing `WPQ_FLUSH` before `IO_QUIESCE` had returned its completion ack — so live IO traffic kept refilling the WPQ, and the self-refresh entry condition was never satisfied.
>
> **5. Test.** Wrote a Perspec scenario that ran just those two opcodes with a controlled, observable handshake. With a forced wait between them: pass. Without: hang. Hypothesis confirmed.
>
> **6. Root cause.** PCU FW sequencer relied on *implicit* ordering between opcodes instead of waiting on the `IO_QUIESCE` ack. Under realistic IO load that implicit gap closed and ordering broke.
>
> **7. Fix & verify.** FW change: explicit wait-for-ack on `IO_QUIESCE` before `WPQ_FLUSH` issues. Added a Perspec assertion gate on the ordering and put it in the nightly regression. Zero recurrence.
>
> **Lesson.** Opcode-level FW sequences in PM flows need explicit handshakes, not implicit ordering. Pre-Si caught it because Zebu shows opcode granularity — post-Si this is a 'random ADR fail' that takes weeks to triage.

### 8. Story 2 — NVMe CSTS.RDY never asserts (90 sec)

> *Polaris NVMe controller bring-up in ROM mode. The boot path that brings the device out of its lowest power state into operational — the very first PM transition the host drives.*

> **1. Observe.** Standard init sequence on Oakgate. Host de-asserts PERST#, waits `CSTS.RDY=0`, programs `AQA/ASQ/ACQ`, sets `CC.EN=1`, then polls `CSTS.RDY=1` within `CAP.TO`. Poll hung on every boot. `CC.EN` read back as 1, `CSTS.CFS` was clear — controller wasn't complaining, it just wasn't coming ready.
>
> **2. Reproduce.** 100 %, every boot. Pure functional.
>
> **3. Isolate.** Diff'd the new host script against the previously-working one. One field different: `AQA`. New script programmed `ASQS=ACQS=0`, treating the field as 0's-based and 'requesting one slot.'
>
> **4. Hypothesise.** `AQA` is *not* 0's-based — every NVMe queue reserves one slot for head/tail disambiguation, so size 0 means zero usable slots, which is illegal. Controller silently refuses to come ready.
>
> **5. Test.** Re-ran with `ASQS=ACQS=3` and a properly contiguous AQ buffer. `CSTS.RDY=1` came up well within `CAP.TO` on every boot.
>
> **6. Root cause.** Host-side spec misuse against a controller that didn't fail loud. NVMe 1.3 Section 4.1 is explicit; the script wasn't.
>
> **7. Fix & verify.** Host script corrected. Filed an FW ask: ROM should set `CSTS.CFS` when it sees `CC.EN=1` with `AQA=0`, so the next person debugs in seconds, not hours. Boot regression added.
>
> **Lesson.** When `CSTS.RDY` hangs on the first PM transition out of reset, suspect the host before the device — `AQA`, `ASQ/ACQ` physical address, contiguous-buffer requirement. Silent-fail FW is a coverage gap, not a feature.

### 9. Story 3 — Core C6 wake lost on back-to-back ARAT (90 sec)

> *Pre-Si Cambria. Core C6 = the deepest per-core sleep state — core is power-gated, PLL off, caches flushed; wake requires a full power-up + PLL relock. The classic PM bug surface: a wake event that arrives *during* the exit transition.*

> **1. Observe.** Idle thread armed with the local APIC ARAT timer firing back-to-back at ~2 µs — designed so wake #2 lands while the core is still mid-C6-exit from wake #1. Sporadic 50–500 ms hangs on the idle thread. Perfmon showed C6 residency continuing past the second timer's expected exit, even though wake #1's IRQ had already been serviced.
>
> **2. Reproduce.** Intermittent — only on back-to-back wakes inside a tight window. Race-condition signature.
>
> **3. Isolate.** Pulled the Core PMA wake-pending CSR from the Zebu trace. Wake #2's latch *did* fire, but `DOMAIN_STATUS.CORE_STATE` never moved C6→C0 for that event. So: wake observed, wake acted on internally, but power-state register never updated.
>
> **4. Hypothesise.** A 2-clock-domain race in the C6 exit path — wake #2's ack edge gets dropped while a clock-crossing FIFO is mid-reset.
>
> **5. Test.** Set a Lauterbach trigger on `CORE_STATE==C6 && wake_in==1 && cfc_init_busy==1`. Every failure landed in exactly that window — the CFC clock-crossing FIFO being re-initialised after PLL relock, *before* the interrupt-buffer drain finished. Hypothesis confirmed.
>
> **6. Root cause.** Wake #2's ack edge was sampled in the old clock domain at the same instant the CFC FIFO was being reset. The edge never made it across the crossing. Net effect: C6→C0 transition silently lost.
>
> **7. Fix & verify.** RTL change: hold the wake latch in the always-on domain until both `init_done` and `drain_done` are high, and gate the `CORE_STATE→C0` write on both. Filed HSD 16015450088. Regression: directed back-to-back-ARAT scenario added; zero recurrence over million-cycle randomised PM stress.
>
> **Lesson.** 2-clock-domain races on power-gated blocks are the classic 'clean in unit sim, fails in full-die emul' class. Unit testbenches don't exercise both domains being transient at the same instant — only a full-die PM scenario does.

### 10. Story 4 — QNN/ORT page-fault + VM regression (90 sec)

> *Platform validation on Copilot+ PC NPU stack. Honest framing: ORT/QNN host-runtime path, not Hexagon RTL. Memory pressure is a power-management surrogate on a battery-powered device — more resident pages = more DRAM refresh = more system power = less PkgC residency.*

> **1. Observe.** PSMU model inference on Qualcomm QNN EP. Known-good EP 2.39 + ORT 1.23.3 → ~7 K page-fault events and ~300 MB virtual memory during inference. New drop EP 2.45 + ORT 1.24.4 → VM grew to ~502 MB, and in one isolated run page faults spiked to ~102 K.
>
> **2. Reproduce.** Deterministic per drop combination. Per-run variance on the page-fault spike, but the VM growth was always present.
>
> **3. Isolate.** Did **not** jump to 'silicon / VTCM defect.' Built a version matrix: old EP / old ORT, new EP / new ORT, and the diagonal — new EP with old ORT. The diagonal showed reduced VM, proving the bug rode the ORT/QNN context-loading path, not the EP binary itself.
>
> **4. Hypothesise.** The newer path was accumulating multiple model context buffers together instead of streaming them through, so touching many host virtual pages at once produced the fault burst, and not releasing them produced the VM growth.
>
> **5. Test.** ETL + WPA correlation: newer path showed bursty `VirtualAlloc` followed by a long 'no-release' tail, lined up exactly with the page-fault timestamps. Source-walked QNN to `qnn_backend_manager.cc::CreateContextFromListAsync()` — confirmed the bulk-load shape. The later mitigation reduced fault count but kept more VM alive, which is exactly what the 300→502 MB shape predicted.
>
> **6. Root cause.** Unbounded context lifetime in `CreateContextFromListAsync` — contexts loaded eagerly and held for the lifetime of the inference session, instead of load → create → release per context.
>
> **7. Fix & verify.** Fix direction: bounded context lifetime (release per-context unless parallel loading is explicitly required). Regression gate added that records EP version, ORT version, page-fault count, `VirtualAlloc` count, peak VM — so the next regression is caught at gate, not in the wild.
>
> **Lesson.** Page faults alone are not the bug. Correlate them with `VirtualAlloc` and peak memory before blaming hardware. On a battery device, host-runtime memory growth is a *power* regression in disguise — more resident pages keep DRAM out of self-refresh.

### 11. Story 4b — Paint SR starves Camera (60 sec)

> *Copilot+ PC NPU, two AI workloads sharing one accelerator. PM-relevant because the only way to get good NPU residency without a real-time tenant starving is to schedule the queue properly — otherwise the framework keeps the NPU at top voltage longer than it needs to be.*

> **1. Observe.** Paint Super-Resolution submits ~36 inference tiles back-to-back; Camera preview submits a fresh inference at 30 fps. When SR runs, Camera drops frames.
>
> **2. Reproduce.** Built a deterministic repro: two semaphore-fired threads, QPC timestamps, both targeting the NPU. Repros every run.
>
> **3. Isolate.** GPUView trace showed Camera frame 0 queued at t = 3 ms but not starting on the NPU until t = 108 ms — ~3.2 frame periods at 30 fps — sitting behind SR's 36 tiles in the queue.
>
> **4. Hypothesise.** The HTP scheduler is single-queue and non-preemptive: once a tile dispatches, it runs to completion. No priority hint means Camera waits behind SR's whole batch.
>
> **5. Test.** Cross-checked with the IHV docs and a second repro with a 1-tile SR vs many-tile SR — head-of-line latency tracked tile count linearly. Confirms single non-preemptive queue.
>
> **6. Root cause.** Structural, not a defect: one HW scheduler, no preemption, no framework-level priority lane to interleave a real-time tenant ahead of a batch tenant.
>
> **7. Fix & verify.** Fix path: framework-side priority lanes so high-priority frames interleave at tile boundaries. Until that lands: workload-policy gate that prevents concurrent NPU workloads when a real-time tenant (Camera, voice) is active.
>
> **Lesson.** A single hardware scheduler is fine until you have two real-time tenants on it. The framework must expose priority — otherwise the only PM-friendly answer is 'don't run them at the same time,' which costs both perf and battery.

### 11a. Story 6 — PCIe Gen6 L0p lane-scaling: a link-level DVFS bug (90 sec)

> *Pre-Si transactor enhancement for a brand-new PCIe PM feature. **The mental model:** L0p is to a PCIe link what DVFS is to a CPU and what tile-power-gating is to an NPU — scale capacity down when traffic is light, scale back up on demand, **without paying the wake-up cost of full sleep**. Same shape as a CPU dropping from P0→P3 instead of going to C6.*

> *Background in plain terms: PCIe already had deep sleep states (L1/L1.1/L1.2) — analogous to CPU C-states — but exiting them costs hundreds of microseconds. **L0p adds a DVFS-like tier:** stay awake, but shed lanes (x16 → x8 → x4) so the unused lanes can drop power. Bug class is exactly the one we know from CPU DVFS — a transition that happens **while a transaction is mid-flight**.*

> **1. Observe.** First pass of L0p support in the transactor passed every directed enter/exit test. A randomised mixed-traffic regression then hit intermittent receive-side error bursts on the DUT at exactly the moments L0p changed lane count. Link stayed up, no fatal errors, but the receiver was getting bad data at the transition boundary.
>
> **2. Reproduce.** Intermittent overall, but **100 % correlated to L0p transitions** — error counter ticked up on every up-shift and down-shift. So: deterministic with respect to the PM event, even if the workload mix made it 'feel' intermittent.
>
> **3. Isolate.** Walked the lane-level trace. The transmit-side data stream changes width at the transition; the receive-side has to know **at exactly which byte** the new width takes effect. The errors all landed at the byte position where the old and new lane widths met.
>
> **4. Hypothesise.** Same mental shape as a CPU DVFS transition that lands mid-instruction: the transactor was switching to the new lane count **as soon as the negotiation completed**, even if the current packet was only half-sent on the old (wider) lane set. The receiver re-aligned to the new width mid-packet, and the second half decoded against the wrong lane map.
>
> **5. Test.** Forced a one-packet quiet window between negotiation-complete and lane-change. Errors went to zero. Restored the original (no-quiet-window) behaviour: errors returned. Hypothesis confirmed.
>
> **6. Root cause.** Transactor was treating the L0p *negotiation* as the commit point. The spec actually requires the lane-count change to be **deferred to the next packet boundary** — exactly like a CPU P-state change has to land on an instruction boundary, not mid-instruction.
>
> **7. Fix & verify.** Added a one-packet drain state to the L0p FSM, gated lane reconfig on `packet_drain_done`, and an assertion `assert always (lane_cfg_change |-> packet_in_flight == 0)`. Receive-side errors went to zero. Million-transition randomised regression clean.
>
> **Lesson.** L0p is link-level DVFS, and its bug class is the same as CPU DVFS: **transitions must align to a transaction boundary.** When you build coverage for any new PM feature — link, CPU, NPU island — the must-have cell is *'what happens if the transition lands mid-traffic.'* Directed tests cover the FSM; the bugs live at the seam between the new PM action and the in-flight payload. Caught pre-Si — post-Si this would have shown as 'random link errors when load is light,' which is weeks of triage.

### 11b. Story 7 — NVMe HMB silent read corruption above WCTEMP (90 sec)

> *Post-Si only — pre-Si cannot see this class because RTL is isothermal. The bug lives at the seam between the **thermal-throttle PM action** and the data path: a textbook 'PM coupling' failure.*

> **1. Observe.** Post-Si characterisation of a Gen4 x4 NVMe SSD on our SoC EVM. `fio --rw=randrw --rwmixread=70 --bs=4k --iodepth=64 --verify=crc32c` running 72 h at ambient 85 °C with the fan off. About once every 6–9 hours, verify failed — and the wrong data was *another LBA's bytes*, not random garbage. NVMe completion `SC=0`, PCIe AER clean, link Gen4 x4 the entire run.
>
> **2. Reproduce.** Corner-only — never at 25 °C, never pure-read or pure-write, never on a DRAM-equipped SSD. Repro window: ambient ≥ 85 °C, mixed RW, DRAM-less HMB part. That repro shape said 'thermal × data-path coupling' before any other hypothesis.
>
> **3. Isolate.** SMART showed Composite Temperature 84 °C, just above WCTEMP 82 °C — i.e. the device was actively throttling. Vendor counter `HMB_L2P_WRITEBACK_DEFERRED` was rising in lockstep with the failures. Two signals on the same axis — temperature.
>
> **4. Hypothesise.** Walked the hypothesis ladder and killed each one cheaply. (a) HMB DRAM bit-flip → ruled out, the wrong data was coherent LBA bytes, not random. (b) Host PRP confusion → ruled out, AER clean and pattern correlated to LBA, not to host address. (c) FTL bug independent of temp → ruled out, doesn't repro at 25 °C. (d) ECC scrub disabled by throttle → ruled out, SMART media-error counter zero. **Surviving hypothesis:** the throttle action defers HMB L2P writebacks, and something downstream isn't honouring that.
>
> **5. Test.** Forced the device into the same throttle state at lower ambient using a vendor debug knob; mixed-RW verify failed in the same shape. Disabled HMB writeback deferral via vendor mode; failures stopped. Two-way confirmation that the deferral is the trigger.
>
> **6. Root cause.** Above WCTEMP, the throttle FSM defers HMB L2P writebacks to save controller power. But the SLC-reuse allocator clears its in-controller 'SLC dirty' bit on **fold completion**, not on **L2P writeback completion**. Sequence: write LBA `L` lands at SLC page `P_slc`, CQE returned. Fold `P_slc` to TLC completes, dirty bit cleared. Throttle engages — L2P writeback for `L → P_tlc` deferred, so HMB still maps `L → P_slc`. SLC allocator sees `P_slc` 'clean' and reuses it for a new write of LBA `L'`. Host reads `L`; controller follows the stale HMB L2P entry, DMAs from `P_slc` which now holds `L'`'s bytes. Two NVMe invariants violated: L2P update + SLC reuse must be atomic w.r.t. subsequent reads, and **Composite-Temperature throttle must affect performance only, never consistency.**
>
> **7. Fix & verify.** SSD FW fix: clear SLC-dirty only after the L2P HMB writeback DMA completes; never defer writeback for a page eligible for reuse. Host-side workaround while we waited for FW: lower HCTM thresholds via `nvme set-feature 0x10` so the host throttles before the SSD's internal defer kicks in — ~10–15 % sustained perf cost, zero corruption. Pre-Si coverage gap closed with a UVM `throttle_during_mixed_rw_l2p_consistency` test that asserts thermal throttle during mixed RW and shadow-checks read-after-write, plus SVA 'SLC dirty clear ⇒ L2P entry written to HMB.'
>
> **Lesson.** Silent data corruption with `SC=0` and clean AER means the link is honest; the lie is in the data path. **PM bugs that couple to data-path correctness are the post-Si-only class** — pre-Si modelled throttle as a perf knob, with no coupling to L2P. Always pull vendor counters before blaming the visible layer.

### 12. Delivery discipline

```
Symptom  →  Hypothesis  →  How I narrowed it  →  Root cause  →  Fix  →  Learning
```

- **90 seconds max.** If you go over, you're telling the story, not making your point.
- **Lead with the symptom.** Skip the project backstory.
- **One hypothesis at a time.** Show the test that proved or killed it.
- **Honest framing.** "I worked at the platform layer" is stronger than overclaiming RTL ownership you didn't have.
- **End with the lesson.** That's what they remember.

### 13. Common debug-tool vocabulary

| Tool | Domain | What you read |
|---|---|---|
| **JTAG / Lauterbach** | post-Si bring-up | core register, single-step, breakpoint |
| **Logic analyser** | board-level signal | parallel bus, protocol decode |
| **Lecroy / Tektronix protocol analyser** | PCIe / USB / NVMe | TLP / packet trace |
| **Oscilloscope** | analog | rail droop, signal integrity |
| **Zebu / Palladium / Veloce** | pre-Si emulation | RTL trace, transaction view |
| **Perspec** | pre-Si test content | scenario-based directed test |
| **VCS / Questa** | pre-Si simulation | waveform, SVA |
| **`perf` / VTune / WPA** | post-Si perf | counters, sampling, top-down |
| **ETL + WPA** | Windows post-Si | system trace, GPUView, memory counters |
| **AER / MCA** | PCIe / CPU error | error log, recoverable / fatal |
| **`smartctl` / nvme-cli** | NVMe field | SMART log, SQ/CQ, identify |

---

## Bugs in this Topic

This day **is** the bugs. The sections above contain the polished 90-sec versions, sized for interview delivery.

---

## Interview Explanation (60-90 sec — the meta-answer)

> "My debug method is observe, reproduce, isolate, hypothesise, test, root-cause, fix and verify. I try to convert a vague failure — a hang, a dropped frame — into one controlled signal: a register bit, a timestamp, a queue depth, a trace edge, an ordering violation. Reproducibility rate tells me what class of bug it is: 100% means functional, intermittent means race or marginal, corner-only means PVT, single-unit means defect. I isolate by binary-searching scope, single-stepping via JTAG, or swapping components, and I always try to take one variable out at a time. The cost of finding a bug rises about 10× per stage from RTL sim through emulation through FPGA to post-Si, so where you catch it is half the value. When I tell a bug story, the shape is symptom → hypothesis → how I narrowed it → root cause → fix → learning, in 90 seconds, leading with the symptom. The honest framing matters: at Microsoft I do platform validation on Copilot+ PC silicon — I correlate ETW counters, ORT/QNN logs, version matrices and source-level pointers; I do not debug Hexagon RTL or claim VTCM defects."

---

## Architect Drills

*Debug method is itself an architecture: each step exists because skipping it produces a predictable failure mode. Reason about which step is load-bearing under which constraints. No answer key.*

**D1. The 7-step framework (observe → reproduce → isolate → hypothesise → test → root-cause → fix). Reason about which step is most often skipped, and why that fails.**
*Constraints to weigh:* "I have a hypothesis" without reproduction, isolation without test, fix without root-cause (band-aid), no regression coverage. Construct a triage where skipping each step in turn predicts a different failure mode.

**D2. Repro rate (100% / intermittent / corner-only / single-unit) classifies the bug. Reason about how rate determines triage tooling.**
*Constraints to weigh:* 100% → single-step debug; intermittent → trigger + capture; corner-only → PVT sweep; single-unit → defect chase. Why does "intermittent" cost the most engineering hours per bug — what's the irreducible work?

**D3. Cost rises 10× per stage from RTL → emul → FPGA → post-Si. Reason about why the curve is so steep.**
*Constraints to weigh:* tool licenses, people involved, integration scope, NRE on physical changes, schedule risk. Where would a future methodology (formal, AI-assisted) flatten the curve, and where is it irreducibly steep?

**D4. "Lead with the symptom" in the 90-second story. Reason about why backstory dilutes signal.**
*Constraints to weigh:* interviewer attention budget, signal-to-noise, what they're testing (thinking pattern, not chronology). Construct the same bug in story-then-symptom order — show the cost in lost signal.

**D5. Honest framing matters ("I don't debug Hexagon RTL"). Reason about why under-claiming wins.**
*Constraints to weigh:* interviewer-side fact-check, internal-credibility downstream, what an over-claimed story does when the follow-up question hits. Construct the question chain that exposes a bluff.

**D6. "Convert vague failure into one controlled signal" is the meta-method. Reason about what makes a signal "controlled."**
*Constraints to weigh:* reproducibility, single-bit clarity, timestamped, attributable to one stage, falsifiable hypothesis. What's the difference between "controlled signal" and "anecdote," and why does the difference predict whether a fix will hold?

**D7. Imagine debug tooling improved 100× (perfect waveform capture, AI-assisted triage). Which steps of the 7-step framework still survive?**
*Constraints to weigh:* observation becomes free, hypothesis still requires understanding, root-cause still requires mechanism, fix still requires designer knowledge. What's the irreducible human work, and what's automatable?

---

## Coding A — Systems C: a debug-log triage helper

A small parser that classifies failures by reproducibility and tags a likely class — the first decision the framework drives.

```c
#include <stdio.h>
#include <string.h>

typedef enum { CLASS_FUNCTIONAL, CLASS_RACE, CLASS_CORNER, CLASS_DEFECT, CLASS_UNKNOWN } class_t;

static const char *class_name(class_t c) {
    switch (c) {
        case CLASS_FUNCTIONAL: return "FUNCTIONAL (design/FW)";
        case CLASS_RACE:       return "RACE / TIMING";
        case CLASS_CORNER:     return "PVT CORNER (V/T/process)";
        case CLASS_DEFECT:     return "SILICON DEFECT (single unit)";
        default:               return "UNKNOWN";
    }
}

static class_t triage(double repro_rate, int unique_units, int corner_dependent) {
    if (corner_dependent && unique_units > 1)        return CLASS_CORNER;
    if (unique_units == 1)                            return CLASS_DEFECT;
    if (repro_rate >= 0.99)                           return CLASS_FUNCTIONAL;
    if (repro_rate < 0.50)                            return CLASS_RACE;
    return CLASS_UNKNOWN;
}

typedef struct {
    const char *name;
    double repro;
    int    units;
    int    corner;
} report_t;

int main(void) {
    report_t reports[] = {
        {"ADR hang on Zebu",          1.00, 5, 0},
        {"C6 ARAT wake loss",         0.05, 4, 0},
        {"V-droop crash @ 105C",      0.40, 6, 1},
        {"Single-unit boot fail",     0.95, 1, 0},
        {"NVMe CSTS.RDY hang",        1.00, 3, 0},
    };
    int n = sizeof reports / sizeof reports[0];
    printf("%-30s  %7s  %5s  %6s  %s\n",
           "Bug", "Repro", "Units", "Corner", "Likely class");
    for (int i = 0; i < n; i++) {
        const report_t *r = &reports[i];
        printf("%-30s  %6.0f%%  %5d  %6s  %s\n",
               r->name, r->repro * 100, r->units,
               r->corner ? "yes" : "no",
               class_name(triage(r->repro, r->units, r->corner)));
    }
    return 0;
}
```

## Coding B — Interview DS&A: Number of Islands (LC 200)

DFS / BFS on a grid — the classic "isolation" pattern.

```python
def numIslands(grid):
    if not grid: return 0
    R, C = len(grid), len(grid[0])
    seen = [[False]*C for _ in range(R)]
    def dfs(i, j):
        if i<0 or j<0 or i>=R or j>=C or seen[i][j] or grid[i][j]=='0': return
        seen[i][j] = True
        dfs(i+1,j); dfs(i-1,j); dfs(i,j+1); dfs(i,j-1)
    n = 0
    for i in range(R):
        for j in range(C):
            if grid[i][j]=='1' and not seen[i][j]:
                dfs(i, j); n += 1
    return n
```

---

## System Thinking — Why debugging is a *method*, not intuition

**Frame:** great debuggers don't "see" the bug — they apply a small set of methods systematically faster than novices apply intuition. Method beats intuition because intuition has bias (you go look where you remember last week's bug, not where this week's bug actually is). Every bug story you tell in an interview is a chance to demonstrate method: how you bounded the problem, how you generated and tested hypotheses, how you knew when you were wrong. The interviewer cares less about the bug than about how you found it.

### Q: Why is bisection the universal debugging tool?

Because it works on any monotonic search space, with no domain knowledge:
- **Time bisection** (`git bisect`): when did this regress? Cuts N commits to log₂(N) builds.
- **Code bisection** (`#if 0` halves of a function): which half is wrong?
- **Input bisection** (binary-chop the failing input): which byte triggers it?
- **Configuration bisection** (toggle features off pairwise): which interaction is broken?

Each step costs one experiment but **halves uncertainty**. After 10 steps you've narrowed 1024 candidates to one. *Most engineers reach for printf or gdb first; mature engineers reach for bisection first because it converges in known time.*

### Q: Why "symptom vs cause" discipline?

Because the symptom and the cause are usually in different places. Examples:
- Symptom: occasional NPU output corruption. Cause: missing cache clean before DMA (Day 8).
- Symptom: NVMe doesn't enumerate. Cause: wrong DSTRD shift in doorbell address (Day 11).
- Symptom: PkgC10 residency drops. Cause: PCIe device firmware refuses L1.2 (Day 10/13).

**The naive fix addresses the symptom and the bug returns under different conditions.** The discipline: keep asking "why" until you reach a *mechanism* that explains the symptom and predicts what other symptoms it would cause. Then verify those other symptoms exist. *That's the difference between "workaround" and "root cause."*

### Q: Why does bug-story polish matter for interviews?

Because the interviewer is testing **how you think under pressure**, not what you know. A bug story is a 3-minute window into your debugging method. Polish four moves into every story:
1. **Frame the symptom precisely.** "Once every ~10 minutes under sustained 70% load, NPU output had a zero-tile." Not "sometimes it broke."
2. **State the hypothesis you tested first and why.** "I suspected a non-coherent DMA write — because the corruption was tile-aligned and the symptom only appeared under sustained throughput where store buffers fill up."
3. **Describe how you bounded it.** "I added a barrier before the doorbell write — if the bug was ordering, this would mask it. Bug disappeared. Then removed barrier, added explicit cache clean instead — also disappeared. Confirmed root cause."
4. **State the fix and the validation hook you added.** "Driver patched to clean+barrier on every descriptor submit. Added a stress test that runs sustained 80% load for 1 hour with output checksum — now in CI."

The story is **method demonstrated through a concrete bug**. Without the method, it's a war story. With method, it's a hiring signal.

### Q: Occam's razor — when does it fail?

When the system has hidden state. Occam says: prefer the simpler explanation. But in HW/firmware:
- The simplest explanation often is *not* the cause because it would have been caught in earlier testing. 
- Multi-day flakiness usually means a *combination* of conditions (load + temperature + frequency + concurrent IO) that none of the individual tests cover.

**Modified rule:** prefer the simplest explanation that *also* explains why the bug evaded existing tests. If your hypothesis would have been caught in CI, it's probably wrong.

### One-liner

*"Debugging is method — bisection, symptom-vs-cause, hypothesis-and-bound — because intuition has bias and method has guaranteed convergence. A polished bug story is method demonstrated through a concrete artifact: precise symptom, first hypothesis, bounding experiment, fix, and the test that prevents regression. The interviewer is hiring the method, not the trivia."*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **debug as method** — *what:* hypothesis → test → narrow / *why:* intuition has bias, method has guaranteed convergence / *how:* eliminate half the search space per step / *connects:* scientific method, why interviewers hire method not trivia
- **bisection** — *what:* `git bisect`, A/B configs, halving the search / *why:* O(log n) to find the bad change / *how:* known-good vs known-bad endpoints / *connects:* finding the introducing commit, finding the breaking config bit
- **layered isolation** — *what:* HW vs FW vs driver vs OS vs app / *why:* the stack is tall and each layer can lie / *how:* swap one layer at a time / *connects:* cross-team handoff, who-owns-the-bug politics
- **capture-and-replay** — *what:* trace + reproduce / *why:* flaky bugs need determinism / *how:* perf, ETW, ftrace, rr / *connects:* heisenbug, racy unit tests, recording the once-in-a-million
- **bug-story shape** — *what:* symptom → hypothesis → bounding experiment → fix → regression test / *why:* shows method through a concrete artifact / *how:* always 5 beats, ≤ 90s spoken / *connects:* CFC, QNN cache, NVMe RDY — same shape, different domain

## Bug Anchor

- **Day 11 (NVMe):** Story 2 (CSTS.RDY) lives here.
- **Day 14 (ADR / Core C6):** Stories 1 and 3 live here.
- **Day 19 (NPU runtime):** Stories 4 and 4b live here.
- **Day 21 (final mock):** these stories are the centrepiece of the loop.

## Exit Check

- [ ] Recited the 7-step framework from memory.
- [ ] Stated reproduction-rate → bug-class mapping.
- [ ] Told stories 1, 2, 3, 4, 4b each in ≤ 90 seconds, leading with the symptom.
- [ ] Stated the bug-cost-per-stage 10× ladder.
- [ ] Have honest framing for the QNN story (platform-validation, not Hexagon RTL).
- [ ] Have the meta-answer ready ("my debug method is …").

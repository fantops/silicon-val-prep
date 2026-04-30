# MCU / SoC Post-Silicon Validation — Revision Notes (generic)

Compact revision sheet for any **MCU or SoC post-silicon / silicon-debug staff role** — Renesas, NXP, TI, ST, Infineon, Microchip, Nordic, Ambiq, Qualcomm IoT, NVIDIA Jetson, Tier-1 automotive suppliers. Focuses on **gaps the existing 21-day plan didn't already cover** (the plan is heavy on x86/ARM SoC platform, PCIe/NVMe, NPU, power). MCU/SoC bring-up roles tilt toward **first-silicon bring-up, low-speed buses, automotive/industrial protocols, lab bench debug, schematics, JTAG**.

The gap material is **folded into the existing 21 days** so you still finish in 21 days. Cross-refs:
- [Day 3 — Extension: low-speed buses (UART/I2C/SPI/QSPI)](day03_bits_mmio_volatile_polling.md#extension--low-speed-buses-uart--i2c--spi--qspi-flash)
- [Day 9 — Extension: POR/reset/clocks/PLL/straps/fuses/boot ROM/secure boot + DDR training](day09_arm_qualcomm_platform_vocabulary.md#extension--por-reset-clocksplls-straps-fuses-boot-rom-secure-boot-mcusoc-bring-up)
- [Day 10 — Extension: automotive buses (LIN/CAN/CAN-FD/Auto Ethernet/TSN) + compliance/corner/stress](day10_pcie_fundamentals_ltssm.md#extension--automotive-buses-lin--can--can-fd--auto-ethernet--tsn--flexray)
- [Day 15 — Extension: ADC, PVT corner sweeps, lab power instruments](day15_thermal_power_perf_counters.md#extension--mixed-signal-adc-pvt-corner-sweeps-lab-power-instruments)
- [Day 20 — Extension: JTAG/CoreSight, lab instruments, schematics, bring-up runbook, Python automation, x86 asm literacy](day20_debug_method_bug_polish.md#extension--jtag--coresight-lab-instruments-schematics-bring-up-runbook)

---

## 1. What this class of JD is really asking for

A staff post-Si engineer who can:

1. **Take first silicon out of the box** and bring it to a running state — POR sequence, clocks alive, boot ROM jumps, UART console up, DDR trained, peripherals enumerated.
2. **Debug at the pin and bus level** — scope, LA, protocol analyzer, schematic in one hand, JTAG in the other.
3. **Drive RCA back to the design team** with a reproducible failing waveform / register dump / scan dump, not a vague "it hangs".
4. **Build the test infrastructure** — Python automation, compliance suites, stress/corner loops, regression on the bench.
5. **Lead** — own a feature/IP across silicon revs, mentor, interface with design, package, SI/PI, FW, validation managers.

Vendor-specific flavour varies: **automotive MCUs** (Renesas RH850/RA/RZ, NXP S32, Infineon AURIX, ST SPC58, TI Hercules), **industrial**, **SoCs with safety + security IP**, mixed-signal (ADC/DAC), heavy use of **CAN/LIN/Ethernet AVB-TSN**, lockstep cores, ASIL-B/D adjacency. Map your story to whichever family the JD targets.

---

## 2. Where the existing 21-day plan already covers you

| JD bullet | Existing day(s) |
|---|---|
| ARM micro-architecture | Day 9 (ARM platform vocab), Day 16 (pipeline / OoO / memory model) |
| C/C++ fluency, MMIO, volatile, RMW | Days 1–3 |
| Cache / coherence / barriers / DMA / IOMMU | Days 4–8 |
| PCIe protocol + LTSSM | Day 10 |
| NVMe queues + bring-up bug story | Day 11 |
| Power-on sequencing context (DVFS, C-states, ADR) | Days 12–14 |
| Stress/corner thermal + perf counters | Day 15 |
| Concurrency / lock-free / DSA round | Day 17 |
| Debug methodology + bug-story polish | Day 20 |

So in the loop you **lean on these days first** for the systems-thinking and protocol questions.

---

## 3. The real gaps to close (now folded into Days 3, 9, 10, 15, 20)

The JD repeatedly emphasises bench / pin / bus / boot / fuse work. The 21-day plan was light there — the material below is now **inlined as `Extension` sections** in the existing days listed at the top of this file.

### 3.1 JTAG, CoreSight, scan, trace
- **JTAG TAP** state machine: TLR → RTI → SelectDR/IR → Capture → Shift → Update. Know the 5 wires (TCK, TMS, TDI, TDO, TRST) and the IR opcodes you actually use: BYPASS, IDCODE, EXTEST, INTEST, SAMPLE/PRELOAD, **boundary scan**.
- **IDCODE** = 32 bits: version[31:28] | part#[27:12] | mfg[11:1] | 1. First sanity check after POR — if IDCODE is wrong/0xFFFFFFFF, you have a power, clock, or strap problem, not a JTAG problem.
- **ARM CoreSight**: DAP (DP+APs) → AHB-AP / APB-AP → memory-mapped debug; ETM/PTM trace → TPIU / ETB / ETR (trace to DDR); CTI (cross-trigger) to halt all cores on one event; ITM for printf-style instrumentation.
- **Scan chains** (DFT side you'll touch as a validator, not designer): stuck-at vs at-speed (transition), scan compression, **scan dump** as the last-resort post-mortem when the chip is hung — design team needs the scan dump + the trigger condition, not just "it hangs".
- **Boundary scan** to wiggle pins when nothing else works (board bring-up, solder-bridge hunting).
- Tools: **Lauterbach TRACE32**, Segger J-Link, ARM DS-5 / Arm Development Studio, OpenOCD; vendor-specific probes (Renesas E2 Lite/E2 + IECUBE for RH850, NXP MCU-Link, TI XDS, ST-LINK, Microchip ICD/PICkit, Intel ITP/XDP/OpenIPC).

→ Full notes: [Day 20 — Extension](day20_debug_method_bug_polish.md#extension--jtag--coresight-lab-instruments-schematics-bring-up-runbook).

### 3.2 Lab instruments (you must speak this fluently)
- **Oscilloscope** — analog BW (≥5× signal fundamental), sample rate (≥2.5× BW), probe loading, ground-spring vs pigtail (impacts ringing), differential probe for LVDS/PCIe, **eye diagram** basics (mask, jitter, BER extrapolation).
- **Logic analyzer** — state vs timing mode, threshold, setup/hold capture, deep memory for long sequences, trigger on bus pattern.
- **Protocol analyzer / bus sniffer** — PCIe (LeCroy Summit / Teledyne, Keysight U4301), USB (Beagle, Ellisys), I2C/SPI (Beagle, Saleae), CAN/LIN (Vector CANoe/CANalyzer, PEAK PCAN), Ethernet (Wireshark + tap, Spirent/Ixia).
- **Power instruments** — DC source/load (Keysight N6705), shunt + DMM, power analyzer, current probe (CT). Know µA-range idle measurements (S0ix-class) need filtered shunt + low-noise sense.
- **Bench discipline** — timestamp every capture, save the .wfm, label channels in the file, store waveform alongside the JIRA. "It happens sometimes" is not data; a `.wfm` is.

### 3.3 Schematics + pin-level signal tracing
- Read a multi-page schematic: power tree, clock tree, reset tree, strap pins, bus topology, decoupling.
- **Net tracing**: from BGA ball → via → trace → series resistor → endpoint. Identify stuffing options (DNI/DNP), test points, strap pull-up/down values.
- Common bring-up checks: **rails in order and within tolerance** (sequencer or PMIC rails, slew, droop), **clocks present** (reference XO, PLL out), **resets de-assert in correct order**, **straps latched** at deassertion.
- Know **what you're allowed to do**: cut a trace, lift a pin, add a 0Ω, blue-wire a strap, pull a fuse — and when to instead push back to layout/EE.

### 3.4 Power-on sequencing, reset, clocks, straps, fuses
- POR sequence: rail ramp order → PGOOD → reference clock stable → de-assert PORESET → boot ROM fetches at reset vector.
- Reset domains: **cold / warm / wake / functional / debug** resets are different — a warm reset must NOT clear sticky debug or some persistent state.
- Clocks: **XO → PLL → dividers → IP clock gating**. PLL lock time, bypass mode, spread-spectrum (SSC) on/off, clock-domain crossings (CDC).
- **Straps** are sampled at reset deassertion; if a strap is wrong (boot mode, PLL multiplier, JTAG mux), the part comes up wrong every time and looks like a bigger bug.
- **Fuses (eFuse / OTP)**: store boot mode, security keys, chicken bits, calibration, die-ID, lifecycle state. Blowing fuses is **one-way** — fuse plans go through review.
- → Full notes: [Day 9 — Extension](day09_arm_qualcomm_platform_vocabulary.md#extension--por-reset-clocksplls-straps-fuses-boot-rom-secure-boot-mcusoc-bring-up).

### 3.5 Boot ROM, boot loaders, secure boot
- **Boot flow**: ROM (immutable, in mask) → loads BL1/BL2 from QSPI/eMMC → measures/verifies → jumps to FW / U-Boot / TF-A → OS.
- **Secure boot chain of trust**: ROM has root pubkey hash in fuses → verifies BL1 signature → BL1 verifies BL2 → … . Break in the chain = boot fail with a specific error code from ROM (memorise that the ROM error code → root cause map exists for every SoC).
- Roles to know: **HSM / Secure IP / RoT** (Renesas SCE/TSIP/RSIP, NXP EdgeLock, ST STSAFE/Cube-Sec, Infineon OPTIGA, ARM CryptoCell/CryptoIsland), key provisioning, lifecycle states (CM → DM → SE → RMA), debug authentication (signed JTAG unlock).
- Validation angle: corrupt the image one bit at a time, confirm ROM rejects; flip lifecycle state; verify JTAG locks; verify rollback counter / anti-rollback.

### 3.6 Low-speed buses you WILL be asked about
- **UART**: framing (start/8/N/1), baud error budget (≤2%), break, flow control (RTS/CTS); first console you bring up.
- **I2C**: open-drain, pull-ups (sized for bus capacitance, typ 2.2k–10k), 7/10-bit addr, START/STOP/RESTART, clock stretching, multi-master arbitration, common bug: **stuck SDA low** → 9-clock recovery.
- **SPI**: CPOL/CPHA (modes 0–3), CS timing, single/dual/quad/octal IO (XIP from QSPI flash), max freq vs trace length.
- **QSPI/OSPI NOR flash**: read/program/erase, status reg, **XIP boot**, mode bit pitfalls (continuous read mode locks the part into a state ROM doesn't expect).
- **DDR / LPDDR**: training (read/write leveling, gate training, DQ/DQS, Vref), MR registers, refresh (tREFI), ZQ cal, ECC. **DDR training failures are the #1 first-silicon hang**; you must know how to read the training log and which knob to turn.
- **SRAM**: tightly-coupled (TCM), backup SRAM (battery-backed), parity/ECC, retention through reset.
- **ADC**: SAR vs sigma-delta, SNR/ENOB, INL/DNL, sample-and-hold, reference, Vref noise → LSB error.
- → Full notes: [Day 3 — Extension](day03_bits_mmio_volatile_polling.md#extension--low-speed-buses-uart--i2c--spi--qspi-flash) (UART/I2C/SPI/QSPI) and [Day 9 — Extension](day09_arm_qualcomm_platform_vocabulary.md#extension--por-reset-clocksplls-straps-fuses-boot-rom-secure-boot-mcusoc-bring-up) (DDR training, SRAM).

### 3.7 Automotive / industrial buses
- **CAN / CAN-FD**: differential, dominant/recessive, arbitration by ID, bit stuffing, error frames, error-active/passive/bus-off; CAN-FD adds higher data-phase rate + larger payload.
- **LIN**: single-wire, master/slave, low cost, body electronics; sync break + sync byte + PID + data + checksum.
- **Automotive Ethernet (100/1000BASE-T1)**: single twisted pair, PAM3 (100M) / PAM4 (1G), PHY link-up, **AVB / TSN** (gPTP time sync, TAS/CBS shapers, frame preemption).
- **FlexRay** (legacy but appears): TDMA, static + dynamic segments.
- **Vendor family map** (worked example — mirror the same shape for whichever vendor's JD you're targeting):
  - **Renesas** — RH850 (auto MCU), RA (general MCU), RZ (Cortex-A MPU), RL78 (8/16-bit), R-Car (auto SoC for ADAS/IVI).
  - **NXP** — S32 (auto MCU/MPU), i.MX (industrial/IoT MPU), Kinetis/LPC (general MCU), Layerscape (networking).
  - **TI** — Hercules/TMS570 (safety MCU), Sitara AM (industrial MPU), Jacinto TDA (auto SoC), MSP430 (low-power MCU).
  - **ST** — STM32 (general MCU/MPU), SPC58 (auto MCU), Stellar (auto MCU/MPU).
  - **Infineon** — AURIX TC3xx/TC4xx (auto MCU), XMC (industrial), PSoC (general).

### 3.8 PLL, clocks, ADC, mixed-signal extras
- **PLL**: PFD → CP → LPF → VCO → divider → /N feedback. Lock detect, jitter, fractional-N spurs, BW vs jitter trade-off.
- Clock distribution: **balanced tree, skew, jitter accumulation**, glitch-free MUX for clock switching.
- Mixed-signal validation: power supply rejection on analog rails, ground bounce, AC-couple test inputs, characterise across **PVT** (process FF/SS, voltage Vmin/Vmax, temp -40/+125 for auto).

### 3.9 Protocol compliance + corner / stress
- **Compliance**: PCIe-CEM (CTS), USB (USB-IF), Ethernet (IEEE 802.3 conformance), CAN (ISO 16845), I2C/SPI (less formal), DDR (JEDEC). Know that compliance = **conformance to spec under specified conditions**, not "it works on my board".
- **Corner cases**: voltage corners (Vmin / Vnom / Vmax), temp (-40/25/85/125 °C for auto), process (FF / TT / SS samples), frequency (Fmin/Fmax), **silicon revision** (A0/A1/B0).
- **Stress**: long-soak (24/72 h), thermal cycling, power cycling, simultaneous-IP load (CPU+GPU+NPU+DDR+PCIe), pattern stress (DDR march tests, cache thrash, walking-1 across pins), error injection.
- **Margining**: **Vmin search** (binary search to first fail), shmoo plots (V vs F), eye margin shrink for SI.

### 3.10 Automation — Python that a validator actually writes
- Instrument control via **pyvisa** (SCPI to scope/PSU/DMM), **pyserial** (UART), **smbus**/**spidev** (Linux host probes), vendor SDKs (Saleae, Beagle, Lauterbach RCL, Vector XL).
- Test framework: **pytest** with fixtures per DUT, parametrize across corners, **Allure / HTML reports**, JUnit XML for CI.
- Result handling: **pandas + matplotlib** for shmoo and trend, **InfluxDB/Grafana** for soak runs.
- Lab orchestration: relay boards for power cycle, **PDU** control (Raritan/APC SNMP), JTAG via OpenOCD scripts, golden-image flashing.
- DUT abstraction pattern: one class per DUT with `power_on()`, `wait_console("U-Boot>")`, `jtag_halt()`, `read_reg(addr)`, `flash(image)`.

### 3.11 x86 assembly (the JD lists it)
- You're not writing kernels. You should be able to **read** disassembly enough to:
  - Recognise calling convention (System V AMD64 vs Microsoft x64): RDI/RSI/RDX/RCX/R8/R9 vs RCX/RDX/R8/R9, return in RAX, callee-saved set.
  - Spot **MMIO access patterns** in disasm (`mov` with `volatile` semantics enforced by compiler, MFENCE/SFENCE/LFENCE around it).
  - Spot **WRMSR/RDMSR**, CPUID, IN/OUT, RDTSC/RDTSCP, PAUSE in spinloops.
  - Read a small ISR prologue/epilogue, recognise stack frame, recognise SIMD spill (XMM/YMM).
- For ARM symmetry, know AAPCS64: X0–X7 args, X0 return, X29 FP, X30 LR, SP; `MRS/MSR` for system regs, `DSB/DMB/ISB` for barriers (already in Day 8/16).

---

## 4. Bench bring-up runbook (the answer to "walk me through first silicon")

A staff-level answer in ~90 sec. Memorise the order; the words wrap around it.

1. **Visual + DC check** — board built per BOM, no shorts on rails, DUT seated.
2. **Power up rails one-by-one** with PSU current-limited, scope each rail for ramp + droop, confirm sequencer/PGOOD chain.
3. **Reference clock present + correct freq** on scope at the XO and at the SoC ball.
4. **Resets** — confirm POR asserts then de-asserts in spec; warm/wake resets isolated.
5. **Straps latched correctly** at reset deassert (scope each strap pin at the de-assertion edge if you suspect).
6. **JTAG IDCODE** — connect probe, scan TAP, IDCODE matches datasheet → silicon is alive.
7. **Boot ROM signs of life** — UART banner, GPIO toggle, ROM error code; if no banner, halt via JTAG and read PC.
8. **Load minimal FW via JTAG** — blink an LED, print "hello", read a couple of CSRs. Now you have a controllable target.
9. **DDR training** — run vendor training, capture pass/fail margin, run a memory test (march, walking-1, random).
10. **Bring up peripherals in dependency order** — UART → I2C (PMIC, sensors) → SPI/QSPI (flash, XIP) → high-speed (PCIe / Ethernet / USB) → accelerators.
11. **Compliance + stress** layered on top once functional bring-up is signed off.

Each step has its own failure modes — the linked Days 22/23/24 carry those.

---

## 5. Three bug-story templates for MCU/SoC bring-up

Use these as moulds; fill with your real Intel/SanDisk/Microsoft material. They map cleanly to any vendor's JD (Renesas, NXP, TI, ST, Infineon).

### 5.1 "Strap latched wrong → boot mode wrong" (POR / strap class)
**Symptom:** new board rev, IDCODE fine, but ROM never prints banner; PC stuck in ROM error path.
**Method:** schematic shows BOOT_MODE[1:0] strap pull-ups changed from 4.7k to 47k for a leakage fix; scope at reset de-assert shows the strap not yet at VIH because of the slow RC + leakage. Strap latched as `00` (JTAG boot) instead of `01` (QSPI boot). Fix: keep 4.7k or move strap latch later.
**Why it matters:** demonstrates schematic + scope + reset-timing + ROM-spec literacy in one story.

### 5.2 "DDR training fails on cold board" (DDR / PVT class)
**Symptom:** training passes at 25 °C, fails at -20 °C cold start, intermittent at 0 °C.
**Method:** capture training log, identify which step (write leveling vs gate training) fails; scope DQS to confirm slew; check Vref-DQ; sweep tWL; correlate with PVT corner samples. Root cause: insufficient Vref margin on slow-cold corner — design team adds Vref offset table per temp band.
**Why it matters:** training, PVT, scope, log analysis, RCA loop with design.

### 5.3 "CAN bus errors under load" (automotive bus class)
**Symptom:** ECU goes error-passive after sustained traffic at 1 Mbps CAN-FD data phase.
**Method:** CAN analyzer (CANoe) shows form errors clustered around specific ID; scope diff pair shows ringing on dominant→recessive edge, exceeding receiver mask. RCA: termination on the new node mis-populated (60Ω instead of 120Ω at each end → 40Ω effective, reflections). Fix BOM, errors clear.
**Why it matters:** protocol analyzer + scope + termination math + automotive context.

---

## 6. Behavioural / leadership angles for staff level

- **Cross-team RCA**: "I drove a weekly triage with design, DV, FW and silicon-debug; standing artefact was a single shared sheet — symptom, hypothesis, owner, next experiment, due date — so RCA had a public clock."
- **Validation plan ownership**: features × test types (functional, compliance, stress, corner, security) × silicon revs × board revs; entry/exit criteria per phase (EVT/DVT/PVT).
- **Mentoring**: pair-debug a junior on first scope capture; review their bug write-ups for "symptom → method → evidence → RCA → fix → regression" structure.
- **Tooling investment**: justify the time spent building Python automation by showing the regression hours saved per silicon spin.

---

## 7. 7-day acceleration path if the loop is close

| Day | Read |
|---|---|
| -7 | This file end-to-end + [Day 20 Extension](day20_debug_method_bug_polish.md#extension--jtag--coresight-lab-instruments-schematics-bring-up-runbook) (JTAG/lab/schematic/runbook) |
| -6 | [Day 9 + Extension](day09_arm_qualcomm_platform_vocabulary.md) (ARM vocab + POR/reset/clocks/straps/fuses/bootROM/DDR training) |
| -5 | [Day 10 Extension](day10_pcie_fundamentals_ltssm.md#extension--automotive-buses-lin--can--can-fd--auto-ethernet--tsn--flexray) (LIN/CAN/Auto-Eth/TSN + compliance/stress) + [Day 3 + Extension](day03_bits_mmio_volatile_polling.md) (C/MMIO + low-speed buses) |
| -4 | Day 8 (DMA/barriers) + Day 16 (pipeline/memory model) |
| -3 | Day 20 (debug method + the new lab/JTAG extension) + the three bug-story templates above |
| -2 | Day 17 (concurrency/DSA round) + [Day 15 Extension](day15_thermal_power_perf_counters.md#extension--mixed-signal-adc-pvt-corner-sweeps-lab-power-instruments) (ADC + PVT + power instruments) + section 3.10 (Python automation) |
| -1 | Section 4 (bring-up runbook) — say it out loud 3× ; Day 21 mock |

---

## 8. One-line anchors per JD theme (for the loop)

- **Bring-up**: "rails → clock → reset → strap → IDCODE → ROM banner → JTAG-load FW → DDR train → peripherals."
- **Debug tools**: "scope for analog truth, LA for digital state, protocol analyzer for spec conformance, JTAG/CoreSight for visibility, scan dump for the autopsy."
- **Schematics**: "I trace BGA → via → series-R → endpoint, check straps at reset deassert, verify rail order against the sequencer."
- **Compliance & stress**: "compliance proves spec conformance under defined conditions; stress proves margin under combined real load; both gate the silicon, neither replaces the other."
- **Secure boot**: "ROM holds the root, fuses hold the key hash, every stage measures and verifies the next, lifecycle locks the door."
- **Automation**: "pyvisa/pyserial + pytest with per-DUT fixtures, results to pandas + InfluxDB, lab orchestration via PDU + OpenOCD."


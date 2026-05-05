# PCIe + NVMe SoC — Test Plan (Pre-Si and Post-Si) + One Hard HW Bug

> Scope: a custom SoC whose flagship I/O is a **PCIe Gen4 x4 root complex** with an attached **NVMe SSD endpoint** (or an internal NVMe controller IP behind a root port). This document is written from a **Post-Silicon Platform Validation Engineer** perspective. It mirrors how an interview answer should be structured: pre-Si scope, post-Si scope, one solid HW bug walked end-to-end.

---

## 0. SoC under test (assumed)

| Block | Detail |
|---|---|
| CPU complex | ARMv8 quad-core, GIC-600, SMMU-500 |
| Interconnect | NoC + AMBA CHI; coherent path CPU↔LLC↔DDR |
| Memory | LPDDR5X-7500, 1 channel (single rank for bring-up) |
| **PCIe** | **Gen4 x4 root complex**, single root port, ECAM at fixed PA |
| **NVMe** | external NVMe Gen4 SSD endpoint behind the RP (also valid: internal NVMe IP) |
| Power | PMIC + RPMh, S2-Idle / SoIx supported |
| Clocks | PLLs for CPU / NoC / PCIe REFCLK (100 MHz, SSC-on) |

The validation owner question is: **"can DDR-resident data move into and out of NVMe at line rate, under all power/clock/IOMMU corners, without data corruption or hang, across PVT?"**

---

## 1. Validation Lifecycle Map

```
   ARCH/RTL ─►  Pre-Si    ─►  Tape-out ─►  A0 in lab ─►  Post-Si    ─►  PRQ
    spec      simulation               first silicon    validation
              + emulation              + bring-up       + content
              + virtual platform                        + soak / corner
                                                        + customer pre-flight
```

| Phase | What exists | What I own |
|---|---|---|
| Pre-Si | RTL, UVM testbench, SystemC/QEMU model, FPGA emulation (Palladium/Veloce/Haps) | Test plan, C model, host-side driver against virtual platform, regression triage |
| Post-Si | A0 silicon on EVM, JTAG, PCIe protocol analyser, lab SSDs, thermal chamber | Bring-up, characterization, soak, corner, escape RCA, fix sign-off |

---

## 2. Pre-Silicon Test Plan (PCIe RP + NVMe path)

### 2.1 Goals

1. **Functional** — every TLP / NVMe SQE class reaches the right destination with right ordering.
2. **Performance modelling** — IOPS / BW envelope predicted within ±10% of post-Si target.
3. **Coverage closure** — exit gates: spec coverage ≥ 95%, FSM (LTSSM, NVMe controller) 100%, error-injection scenarios complete.
4. **Software readiness** — driver + firmware boot a reference workload on the virtual platform.

### 2.2 Environments and what each catches

| Env | Speed | Strength | Catches |
|---|---|---|---|
| **UVM/SV simulation** | slowest (kHz) | bit-exact, every signal, formal hooks | PHY/MAC corner cases, LTSSM transitions, ECRC/LCRC, replay buffer overflow |
| **SystemC TLM model + virtual platform (QEMU)** | MHz | runs Linux + nvme.ko | Driver/firmware bring-up, IOMMU/SMMU stream-table walks, MSI-X plumbing |
| **FPGA emulation (Palladium / Veloce / S2C HAPS)** | ~1 MHz wall, hours-long boots | runs full SW stack at near-real timing | Long bring-up sequences, NVMe Identify → I/O queues → fio, thermal/power state transitions |
| **Formal (JasperGold) on small blocks** | seconds | exhaustive | LTSSM deadlocks, register address-decode aliases, FIFO over/underflow proofs |

### 2.3 Test categories (pre-Si)

#### A. PCIe link / LTSSM (PHY+DLL+TL)

| Test | Method | Pass criterion |
|---|---|---|
| Link train Gen1→Gen4, x1/x2/x4 | UVM + emulation | LTSSM hits L0 in spec time; eq Phase 0–3 completes; lane reversal works |
| Equalisation re-run on Gen4 | UVM | EQ Phase 2/3 converges in < spec window; preset table walked |
| Speed-change downshift on errors | UVM error-inject | Link recovers at lower gen, no data loss |
| Hot reset / warm reset / FLR | UVM + virtual platform | Config space restores; AER errors logged |
| L0s / L1 / L1.1 / L1.2 enter/exit | emulation | Exit latency ≤ spec; no spurious wake |

#### B. PCIe TLP / DLL behaviour

| Test | Method | Pass criterion |
|---|---|---|
| MWr / MRd / CplD ordering | UVM with directed + random | TLP ordering rules (B.2 of base spec) honoured |
| Posted / non-posted / completion deadlock prevention | UVM + formal | No completion timeout; CRS handling correct |
| ECRC / LCRC injection | UVM | Replay triggers; flow control updates |
| Flow control credit underrun | UVM stress | No deadlock; credits returned |
| Max payload size negotiation | UVM | Effective MPS = min(RP, EP); no oversize TLP |
| AER error logging | UVM error-inject | Correct severity bits; root error status updated |

#### C. PCIe → IOMMU/SMMU → DDR path

| Test | Method | Pass criterion |
|---|---|---|
| Stream-table walk per Stream ID | virtual platform | EP DMA hits correct VMID/PASID context |
| 4 KiB / 2 MiB / 1 GiB IOVA mappings | emulation | TLB hit/miss observed; no fault |
| Unmapped IOVA → fault | error-inject | SMMU EVTQ entry generated; AER abort to RP |
| ATS (Address Translation Services) | UVM + emulation | Translation request/response round-trip < threshold |
| PRI (Page Request Interface) | UVM | Page-fault from device, OS handles, resume |

#### D. NVMe controller bring-up (whether internal IP or external SSD model)

| Test | Method | Pass criterion |
|---|---|---|
| CAP read, CC.EN=0 → CSTS.RDY=0 | virtual platform | Within `CAP.TO * 500 ms` |
| Admin queue program (AQA/ASQ/ACQ) | virtual platform | Identify Controller / Identify NS succeed |
| Create I/O CQ then SQ | virtual platform | No fatal status; doorbells at correct DSTRD offsets |
| 4 KiB / 8 KiB / 64 KiB / 128 KiB read/write | emulation | LBA correct, status SC=0 |
| Phase-bit wrap | emulation | Host doesn't lose completions across CQ wrap |
| MSI-X vector per queue | emulation | Vector mapped correctly, no missed ints |
| Abort (opcode 0x08) | virtual platform | In-flight cmd terminates with abort SC |

#### E. NVMe data-integrity and protection

| Test | Method | Pass criterion |
|---|---|---|
| End-to-end Protection Information (PI) — Type 1/2/3 | virtual platform | Guard/AppTag/RefTag verified; bit-flip injection caught |
| Compare command (opcode 0x05) | virtual platform | Mismatch returns SC=Compare Failure |
| Write Zeroes / Dataset Management | virtual platform | Dealloc bitmap honoured |
| Power-loss recovery via SHN (shutdown notification) | emulation | Subsequent boot replays journal cleanly |

#### F. Performance models

- Closed-form Little's law model: `IOPS = QD / latency`. Predict 4 KiB-RR IOPS from per-queue depth × queue count.
- Validate the model against UVM trace counters (TLP/sec on RP egress).
- Identify pre-Si **bottleneck candidates**: NoC arbitration, IOMMU TLB capacity, NVMe completion-coalescing window.

#### G. Power/clock corners (pre-Si flavour)

- Gate REFCLK → PCIe in L1.2; verify PLL lock time on exit.
- DVFS up/down on NoC during sustained NVMe traffic — no TLP loss.
- SSC enabled / disabled — link integrity and EQ stability.

### 2.4 Pre-Si exit checklist

- [ ] LTSSM coverage 100% (every state and transition hit)
- [ ] AER fault classes injected and logged correctly
- [ ] NVMe controller FSM coverage 100%
- [ ] Linux nvme.ko boots on virtual platform; `fio --rw=randread --bs=4k --iodepth=32` runs to completion
- [ ] IOMMU stream-table walk verified for ≥ 3 distinct Stream IDs
- [ ] No deadlock found by formal on RP credit/completion path
- [ ] Performance model within ±10% of post-Si target (spec)

---

## 3. Post-Silicon Test Plan (A0 silicon on EVM)

### 3.1 Setup

| Tool | Use |
|---|---|
| EVM with PCIe slot/CEM | host the SSD |
| **PCIe protocol analyser** (Teledyne LeCroy / Keysight U4301) | TLP-level capture, LTSSM trigger |
| BERT (bit error rate tester) on REFCLK / lanes | margining |
| JTAG (Lauterbach / openocd) | CSR peek/poke, breakpoints in driver |
| Oscilloscope + active probes | REFCLK jitter, AC coupling |
| Thermal chamber | -40 °C / +25 °C / +85 °C / +105 °C |
| Power supply with current logging | rail margining |
| Multiple SSD vendors (Samsung, WD, Micron, Kioxia) | interop |
| `fio`, `nvme-cli`, `lspci -vvv`, `setpci`, in-house traffic generator | software stack |
| `perf`, `ftrace`, kernel `pcie_aer` | RCA telemetry |

### 3.2 Phased plan

#### Phase 0 — Smoke (Day 1 of silicon in hand)

1. PMIC rails up in spec (scope each).
2. CPU boot to ROM → SBL → Linux.
3. `lspci` enumerates the root port. **VID/DID = expected.**
4. `lspci -vvv` shows BAR0 sized correctly, link at expected width/speed.
5. NVMe device appears as `/dev/nvme0n1`; `nvme id-ctrl /dev/nvme0` returns sane data.
6. `dd if=/dev/zero of=/dev/nvme0n1 bs=4k count=1 oflag=direct` succeeds.

#### Phase 1 — Functional / Conformance

| Area | Test | Tool |
|---|---|---|
| Enumeration | sweep slot insert/remove, device reset, FLR | `setpci`, hot-plug rig |
| Link | downshift Gen4→Gen3→Gen2 on `setpci` cap force | analyser |
| AER | inject CRC error via lane ferrite + `setpci` | analyser + `aer_inject` kernel module |
| Config space | read-modify-write all RW capabilities; lock check | `lspci`, `setpci` |
| MSI-X | confirm one vector per queue under load | `cat /proc/interrupts` |
| NVMe Admin | `nvme id-ctrl`, `nvme id-ns`, `nvme list-ns`, `nvme get-feature` | `nvme-cli` |
| NVMe I/O | sequential RW, random RW, mixed, Q-depth sweep 1→1024 | `fio` |
| Data integrity | `fio --verify=crc32c --do_verify=1` for 24 h | `fio` |
| Trim / Dataset Mgmt | DSM dealloc; verify SMART writes-counter | `nvme-cli` |
| Format | `nvme format` to LBA 4 KiB, then re-test | `nvme-cli` |
| Power-loss | yank power mid-write × 10 000 cycles; check FS replay | bench rig |

#### Phase 2 — Performance / Margining

- 4K random read / 4K random write / 128K seq read / 128K seq write across QD = 1, 4, 16, 32, 64, 128, 256.
- Plot **IOPS vs QD**, **BW vs block size**, **latency CDF** (p50 / p99 / p99.99). Compare against pre-Si model.
- **REFCLK jitter sweep**: scope shows REFCLK eye → introduce jitter via signal generator → observe LTSSM recoveries.
- **Voltage margining**: VDD_PCIe ±5% in 1% steps; observe link uptime.
- **Lane margining at receiver** (Gen4 spec feature): run protocol-analyser margining; record voltage/timing margins per lane.
- **IOMMU TLB pressure**: large working-set with many distinct IOVAs; observe TLB miss rate via PMU.

#### Phase 3 — Power / Sx / ASPM

| Test | Setup | Pass |
|---|---|---|
| ASPM L0s/L1 enable+disable | `setpci` | No data loss; latency within spec |
| L1.2 entry/exit under idle | idle 5 s, then traffic | exit < 200 µs; link stays at L0 |
| S0ix (SoC idle) with NVMe present | `pm-suspend`-style | Resume succeeds; FS mounts |
| S3 → resume with NVMe | suspend + resume | NVMe re-enumerates; data intact |
| S4 hibernate | hibernate + cold boot | NVMe namespace consistent |
| Surprise removal | yank slot under load | Driver detaches cleanly; no kernel panic |

#### Phase 4 — Thermal / Endurance / Soak

- 72-hour `fio` mixed RW at QD=64 across temp corners (-40 / +25 / +85 / +105 °C).
- Power-cycle endurance (10k cycles).
- Sustained write to fill SSD overprovisioning, observe **FTL-induced latency cliffs** (this is where post-Si finds bugs pre-Si never catches).
- Concurrent NVMe + Wi-Fi + display + GPU traffic (NoC contention).

#### Phase 5 — Interop & Customer Pre-Flight

- Test with **5+ SSD vendors** at all link speeds.
- Test with **2+ host operating systems** (Linux, Windows, fastboot/UEFI).
- Customer-supplied workload replay.

### 3.3 Coverage / Exit

- All tests in Phase 1 PASS at all temps.
- Phase 2 numbers within 10% of pre-Si model and within spec floor.
- Phase 3 zero data loss across 1000 suspend/resume cycles.
- Phase 4 zero uncorrectable errors, no kernel panic, no fatal AER.
- 0 P0/P1 escapes outstanding; ≤ 3 P2 with documented workaround.

---

## 4. The HW Bug — "Gen4 link drops to Gen2 only on hot SSDs after 2-hour soak; data still flows but at 1/4 BW"

This is the kind of bug you want to be able to walk in an interview. It is **a real class of HW bug** — receiver equalisation drift due to SSD-side temperature-induced amplitude change combined with a root-port equalisation FSM that does **not** re-run Phase 3 on Recovery — and it touches PHY, LTSSM, AER, performance counters, and FW workaround in one story.

### 4.1 The symptom (as filed in the bug tracker)

```
Title: PCIe Gen4 RP downshifts to Gen2 after ~2h fio soak at 85 °C.
        Link recovers but stays at Gen2; full perf only restored by
        warm reset of the RP.
Severity: P1 (perf escape; no data corruption)
Repro:    100% on 3 EVMs; 60% on 2 SSD vendors (A, B); 0% on vendor C.
First seen: A0 silicon, post-Si Phase 4 thermal soak.
```

Telemetry at moment of failure:

- `lspci -vvv` after failure: `LnkSta: Speed 5GT/s, Width x4` (was `16GT/s, x4`).
- `dmesg`: many `pcieport ... AER: Corrected error received` and `Replay Number Rollover` before downshift.
- AER error counter on RP: bumps in **`Bad TLP`** and **`Replay Timer Timeout`**.
- LTSSM trace from analyser: `L0 → Recovery.RcvrLock → Recovery.RcvrCfg → Recovery.Speed → L0 (Gen2)` repeatedly until it sticks at Gen2.
- SSD case temperature at failure: 78 °C (within SSD spec).
- Repeat at 25 °C: never reproduces in 24 h.

### 4.2 Why "data still flows but slow" is a clue

A pure PHY death would lose link entirely. **Recovery succeeded** every time — the FSM brought the link back. The link just didn't come back at the original speed. That isolates the bug to **the speed-change arm of LTSSM Recovery**, not to PHY init.

### 4.3 Hypotheses and how each is ruled in/out

| # | Hypothesis | Test | Result |
|---|---|---|---|
| 1 | Bad solder / connector | swap SSDs and slots; reflow check | rules out — repros across 3 EVMs |
| 2 | REFCLK jitter rising with temp | scope REFCLK at hot | jitter within spec; no rise |
| 3 | SSD-side issue | swap SSDs; vendor C does NOT repro | partial — vendor-C SSD has different EQ presets |
| 4 | Root port speed-change policy aggressive | check `LnkCtl2.Target Link Speed`, RP code that programs it | speed-change *triggered* by Replay Timer Timeout, expected |
| 5 | **EQ Phase 3 not re-run on Recovery** | inspect RP `LnkCtl3` and silicon EQ FSM via JTAG | **CONFIRMED** — RP omits Phase 3 on Recovery-triggered speed change |
| 6 | NoC backpressure starving completions | NoC PMU under load | clean — no starvation |

### 4.4 The actual root cause (HW)

The PCIe Gen4 root-port silicon's LTSSM, on entering `Recovery.Speed` from L0 (triggered by accumulated correctable errors from the receiver as the SSD heats up and its TX amplitude drifts), executes the speed change but **skips equalisation Phase 3** (preset/coefficient request from the partner). Its EQ coefficient register holds the values from the original training at boot — when the SSD was cold. At 78 °C the SSD's TX has drifted such that the boot-time coefficients no longer pass the receiver eye → DLL reports more `Bad TLP` → another Recovery → another Replay Timer Timeout → eventually the link's speed-change policy gives up and pins to Gen2 (where the eye is roomy enough to work).

This is a **classic HW bug shape**:
- *Spec-ambiguous behaviour* — PCIe spec says EQ Phase 3 *may* be re-run on speed change, but most ECNs and vendor IPs require it for Gen4+.
- *Temperature-conditional* — only fires once the SSD has been hot long enough to drift its TX.
- *Vendor-conditional* — vendor C's SSD ships with more conservative TX presets that survive the original coefficients across temperature.
- *Functionally invisible* (no data corruption) — but a 4× perf cliff is unacceptable for a flagship SoC.

### 4.5 Debug method (the picture that lands the offer)

```
  Symptom (perf cliff hot, OK cold)
    │
    ├─ lspci LnkSta             ──►  link speed downshifted
    ├─ AER counters              ──►  Bad TLP + Replay Timer Timeout rising
    ├─ analyser LTSSM trace      ──►  Recovery.Speed loop, lands at Gen2
    ├─ scope REFCLK & lanes hot  ──►  REFCLK clean; LANE eye margin shrunk
    ├─ JTAG read RP EQ regs      ──►  coefficients = boot-time values
    ├─ vendor C SSD does not repro
    │       │
    │       └─ vendor C uses different TX preset table
    │
    └─ Conclusion: RP does not re-run EQ Phase 3 on Recovery speed change
```

### 4.6 Workarounds (immediate) and silicon fix (B-step)

**Immediate FW workaround on A0:**
- Detect first AER `Replay Timer Timeout` on the RP, force an LTSSM Recovery via `LnkCtl.Retrain Link`, and pre-program a thermally-margined EQ preset before allowing the link to return to L0.
- Or: cap link speed to Gen3 above SSD case temperature 70 °C using a thermal sensor + driver hook.
- Cost: ~5–8% perf at high temp; acceptable for A0.

**Silicon fix (B-step ECO):**
- Modify root-port LTSSM to enter EQ Phase 3 unconditionally on any Recovery-triggered speed change.
- Add `EQ_RERUN_ON_RECOVERY` bit in RP control register so FW can disable if it ever regresses.
- Add coverage in pre-Si UVM: a directed test that runs `Recovery → Speed change → expect Phase 3 → expect coefficients update`.

### 4.7 Why this bug class survived pre-Si

- Pre-Si emulation does not model **PHY analog drift with temperature**.
- Pre-Si stimulus didn't produce *enough cumulative correctable errors over time* to drive the LTSSM into `Recovery.Speed` organically — only directed-error-injection hit `Recovery`, and the directed test happened to use a cold-temperature coefficient set.
- This is the **canonical reason post-silicon validation exists**: temperature, time, and tolerance interact with the PHY in ways no RTL simulation captures.

### 4.8 What you put in the bug report (template you should be able to recite)

```
SUMMARY: Gen4 RP downshifts to Gen2 under thermal soak; link does not return
         to Gen4 without warm reset.
SEVERITY: P1 (4× BW loss; no data corruption)
REPRO:    fio 4K randread, QD=32, 2h, ambient 85 °C, vendor A/B SSD.
HW STATE AT FAILURE: AER Bad TLP, Replay Timer Timeout incrementing;
                     LTSSM Recovery.Speed → L0 (Gen2); EQ coeffs = boot values.
ROOT CAUSE: RP LTSSM omits EQ Phase 3 on Recovery-triggered speed change;
            boot-time coefficients fail receiver eye after SSD TX drift at temp.
FIX: B-step RTL change: re-enter Phase 3 on Recovery speed change.
WORKAROUND: FW retrain on first AER Replay Timer Timeout; pre-program
            thermally-margined preset.
COVERAGE GAP: pre-Si lacked thermal-coefficient drift model; new UVM test
              `recovery_speed_change_eq_rerun` added.
```

---

## 5. Interview soundbites for this whole topic

- **"Pre-Si validates intent against the spec; post-Si validates the spec against physics."**
- **"My job ends not when the test passes, but when the failure mode is documented to the point that no future engineer can re-introduce it."**
- **"For PCIe-NVMe, 80% of the bugs that escape RTL are at the boundary — PHY-PMA analog, IOMMU page-walk timing, and FTL latency cliffs."**
- **"AER counters are not just for filing bugs — they are the breadcrumbs that tell you which LTSSM arm to inspect next."**
- **"Every NVMe perf bug has two suspects: the host driver and the FTL. Eliminate one with traffic at constant LBA, and you've found your half."**

---

## 6. Suggested interview narration order (60–90 s)

1. *"On a PCIe+NVMe SoC, my pre-Si plan covers LTSSM and TLP coverage in UVM, IOMMU walks and driver bring-up on a virtual platform, and a performance model tied to UVM counters. Exit gates: 100% LTSSM/NVMe FSM coverage and `fio` running on the virtual platform."*
2. *"Post-Si starts with smoke (lspci, nvme-cli), then conformance (AER, FLR, hot-plug), then performance (IOPS/QD sweep matching the pre-Si model), then power (ASPM, S3, S0ix), then 72-hour thermal soak across temp corners, then interop with multiple SSDs."*
3. *"The bug story I'd walk through is a Gen4 link that quietly downshifts to Gen2 only when the SSD heats up. Symptom: perf cliff, no data loss. Telemetry: AER Replay Timer Timeout, LTSSM stuck in Recovery.Speed. Root cause: the root port skipped EQ Phase 3 on a Recovery-triggered speed change, so boot-time coefficients failed the warm receiver eye. Fix: B-step RTL re-enables Phase 3 on Recovery; FW workaround pre-programs a thermally margined preset. Coverage gap added to pre-Si UVM."*

That's the shape: pre-Si → post-Si → one bug walked from symptom to silicon ECO with the coverage gap that let it escape.

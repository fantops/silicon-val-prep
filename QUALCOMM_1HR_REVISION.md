# Qualcomm Interview — 1-Hour Full Revision Guide

> Read end-to-end ≈ 60 min. Each section ≈ 5–8 min. Skim **section 0** in last 10 min before each interview.

---

## SECTION 0 — Last-10-min Cheat Sheet

**Pitch (60s):** Silicon Validation Engineer, ~8 yrs, full lifecycle. **Microsoft now** — post-Si NPU platform validation on Copilot+ PCs across **Intel LNL, AMD Strix, Qualcomm SD X Elite (Hexagon HTP v79, 8 MB VTCM, ~45 TOPS)**. Built **EdgeOptimizer** (ETW/WPA-based instrumentation). **Intel before** — pre-Si Cambria Xeon Emulation Engineer: C-states, ADR, PCIe Gen6 transactor on Zebu; Star of the Month for CXL FW. **L&T@SanDisk before that** — NVMe post-Si bring-up, 25+ Python power scenarios, SDExpress POC (Spot Award). M.Tech CSE NIT Durgapur 2018.

**Why Qualcomm:** "Snapdragon X Elite + Oryon is where laptop ARM is being defined. CPU system validation — RPMh power coordination, weak memory model, DynamIQ scheduling — is exactly the cross of pre-Si discipline (Intel C-state) and platform validation (MSFT multi-vendor) I want to own end-to-end."

**Why leaving MSFT:** Going *toward* vendor-side ownership, not running. MSFT gave 3-vendor breadth + ETL/EdgeOptimizer rigor. Next: own silicon pre-Si → PV inside a vendor.

**3 bug stories ready (90 sec each):**
1. **Core C6 wake-loss** (Intel Cambria pre-Si) — CFC FIFO 2-clock-domain race
2. **NVMe CSTS.RDY hang** (L&T@SanDisk) — host AQA mis-sized to 0
3. **QNN/ORT page-fault regression** (MSFT) — version-matrix isolation

**The reflexes:**
- "DMB ST between descriptor and doorbell" = #1 ARM driver bug
- "Pre-Si = built it right; Post-Si = built the right thing"
- "Reproducibility rate is the first triage signal"
- "Lead with the symptom; one hypothesis at a time; end with the lesson"

---

## SECTION 1 — Career Narrative (5 min)

### Microsoft (June 2024 → present) — Device Validation Engineer 2, Hyderabad

> "I lead **post-silicon platform validation of NPUs** on Copilot+ PC silicon — Intel Lunar Lake, AMD Strix, Qualcomm Snapdragon X Elite — from EVT through PV. The work answers one question per workload: when a Windows AI app runs, does the NPU actually execute it correctly, fast enough, and within the power budget? I built **EdgeOptimizer**, a platform-level instrumentation tool that profiles NPU vs CPU utilisation, per-inference latency, and energy-per-inference on real devices. RCA uses WPR/WPA hardware traces and ETL logs. I partner with the silicon vendors to triage NPU scheduling stalls, EP-fallback regressions, and suspected silicon issues. I've also deployed **agentic AI workflows with GitHub Copilot CLI** to parse and correlate trace data — what was a 2-hour WPA triage becomes a 15-minute report."

**Anchor numbers:**
- HTP v79 on X Elite: ~45 TOPS, **8 MB VTCM**
- HMX = 4096 MACs/instruction; tile 256×256 INT8 ≈ 2-5 ms; FastRPC wake ~100 µs; DDR ~60 GB/s = real bottleneck
- Windows DLL chain: `onnxruntime_providers_qnn.dll → QnnHtp.dll → QnnHtpV79Stub.dll`; on-device `libQnnHtp.so` is the HTP graph compiler

**Honest framing:** OEM/platform validation, NOT vendor IP validation. I correlate ETW counters, EP/ORT versions, source pointers; I do not debug Hexagon RTL or claim VTCM defects.

### Intel (July 2021 → June 2024) — Emulation Engineer, Bangalore

> "Cambria Xeon SoCs. I owned pre-silicon validation of power-management flows — C-states, idle entry/exit, ADR. I built 10+ PCIe Gen6 transactor enhancements in C++ and developed Perspec scenario test content on Zebu. Debugged 15+ complex cross-IP issues using bare-metal C and system-level analysis. **Star of the Month** for a CXL firmware validation issue."

### L&T @ SanDisk (Sep 2018 → July 2021) — Software Engineer, Bangalore

> "Post-silicon bring-up and debugging of NVMe SoC platforms — Polaris and others. JTAG with Metaware, Lecroy protocol analyser, oscilloscope. Python automation covering 25+ power-management test scenarios. Diagnosed 10+ power-related silicon bugs across firmware and platform layers. Validated PCIe/NVMe protocol compliance and supported the **SDExpress POC** through first power-on — **Spot Award** from SanDisk."

---

## SECTION 2 — Platform Vocabulary (x86 lens, with QC mapping) (10 min)

> **Framing for interview:** "I think in x86 vocabulary because that's where I worked at Intel. Here's how the same concepts map to Snapdragon — the architecture differs but the validation discipline is identical."

### 2.1 Privilege levels

**x86 (what I know):**
- **Ring 3** = user-space apps
- **Ring 0** = OS kernel
- **VMX root** (VT-x) = hypervisor (KVM, Hyper-V)
- **SMM** (System Management Mode) = firmware-only mode entered via SMI; runs PCU/BIOS handlers; opaque to OS

Crossings: `SYSCALL` → ring 0; `VMCALL` → VMX root; SMI → SMM. Each crossing is a security boundary.

**QC mapping (one line):** Ring 3/0/VMX/SMM → **EL0/EL1/EL2/EL3**, with EL3 hosting **ARM Trusted Firmware (ATF)** and TrustZone running S-EL1/S-EL0 alongside Non-Secure. ARMv9 adds Realm world.

### 2.2 MMU / paging

**x86:**
- Page-table base in **CR3** (per-process)
- **PCID** tags TLB entries → no flush on context switch
- 4-level paging: PML4 → PDPT → PD → PT (4KB / 2MB / 1GB pages)
- **MTRRs / PAT** = page-level memory type (WB, WC, UC, WT)

**QC mapping:** CR3 → **TTBR0_EL1** (user) + **TTBR1_EL1** (kernel — two TTBRs, no top-half hack). PCID → **ASID**. MTRR/PAT → **MAIR_EL1 + page AttrIndex**. Granule selectable: 4 KB / 16 KB / 64 KB.

### 2.3 Interrupt controller

**x86:**
- **Local APIC** per core; **I/O APIC** for legacy devices; **MSI/MSI-X** for PCIe (memory-write to APIC address)
- **IPIs** via local APIC ICR
- Interrupt vectors 0-255

**QC mapping:** Local APIC + IOAPIC → **GIC** (Distributor GICD + Redistributor GICR + per-CPU CPU interface). MSI/MSI-X → **ITS** (Interrupt Translation Service) with LPIs.

| ARM range | Class | x86 equivalent |
|---|---|---|
| 0-15 | SGI (Software-Generated) | IPIs |
| 16-31 | PPI (Private Peripheral) | per-CPU APIC timer / watchdog |
| 32+ | SPI (Shared Peripheral) | I/O APIC routed lines |
| LPI via ITS | — | MSI/MSI-X |

### 2.4 IOMMU (DMA address translation + protection)

**x86:**
- Intel **VT-d** / AMD **IOMMU**
- Identifies device by **Requester ID = BDF** (Bus/Device/Function)
- DMAR ACPI table tells OS the IOMMU layout
- Page faults logged in fault recording register; queued invalidation interface for TLB flushes

**QC mapping:** VT-d → **SMMU**. Requester ID → **Stream ID**. Stage 1 = guest VA → IPA, Stage 2 = IPA → PA (virt). **CMDQ** for invalidation, **EVTQ** for faults. Snapdragon puts an SMMU in front of every DMA-capable IP — camera ISP, GPU, modem, NPU, USB, NVMe. **Bring-up bug:** Stream ID not registered → first DMA → EVTQ fault.

### 2.5 Memory consistency (THE big architectural difference)

**x86 (TSO — Total Store Order):**
- Stores can be reordered after **later loads only** (store buffer)
- Store→store, load→load, load→store all preserved
- `MFENCE` = full barrier (rare); `LFENCE` for load fence (Spectre); `SFENCE` for store fence
- For MMIO, `volatile` + WC memory type + occasional `SFENCE` is usually enough
- `LOCK`-prefixed RMW are seq_cst

**QC mapping:** ARM is **weakly ordered** — almost any pair can reorder.

| ARM | x86 analogue |
|---|---|
| `DMB ISH` | similar to `MFENCE` but only inner-shareable domain |
| `DSB SY` | `MFENCE` + drain all outstanding |
| `ISB` | serialises pipeline (after writing system regs); rough x86 cousin: `CPUID` or `IRET` for self-modifying code |
| `LDAR / STLR` | acquire-load / release-store — cheaper one-shot ordering |

**THE #1 ARM-from-x86 port bug:** producer writes `data; ready=1`. On x86 TSO, store→store preserved → consumer always sees data when it sees ready. **On ARM**, `ready=1` can be globally visible **before** `data` → consumer reads stale data. **Fix:** `DMB ST` between data store and flag store; `DMB LD` on consumer; or `STLR/LDAR`. Doorbell pattern needs this between descriptor write and doorbell write.

### 2.6 Power state coordination

**x86 (what I owned at Intel):**
- OS uses **ACPI _PSx** / **MWAIT** with C-state hints; on modern Intel **HWP** (Hardware P-states) lets HW pick the OPP from OS hint
- **PCU / PUnit** is the on-die microcontroller running the power state machine
- PCU communicates with other IPs over **IOSF Sideband (IOSF-SB)**
- **P-channel** = standardised Core PMA ↔ PCU power handshake
- **S3M SRAM** = always-on save/restore SRAM holding arch state during C6
- **CFC** = clock-forwarding/crossing FIFO (always-on ↔ power-gated)
- **PMIC / VR** programmed by PCU over SVID/PMBus
- **PL1/PL2/Tau** = sustained / short-term power limits with averaging window

**QC mapping (one paragraph — say in interview):** OS uses **PSCI** SMCs (`CPU_ON`, `CPU_OFF`, `SYSTEM_SUSPEND`) implemented by ATF at EL3 — that's the standard API. On top of that, Snapdragon has **RPMh** (Resource Power Manager hardened, in always-on) which arbitrates: every subsystem (Apps, Modem, ADSP, CDSP/HTP, LPASS) sends **active/sleep/wake votes**; RPMh picks **MAX per resource** and programs the **PMIC over SPMI** + clock + bus. **TCS** = pre-loaded command sequences fire on idle entry/exit → µs wake (analogue of Intel's PCU sequence ROMs). PMIC is a separate die (PM8xxx). The reason RPMh exists on top of PSCI: Snapdragon has many independent firmwares (Modem, DSPs) each able to vote on shared rails — PSCI alone doesn't aggregate cross-subsystem votes.

**Sleep-state debug ladder (same shape both sides):**

| Step | x86 | QC |
|---|---|---|
| 1. OS requested right state? | MWAIT C-state, ACPI _CST | PSCI CPU_OFF / SYSTEM_SUSPEND |
| 2. PMC/aggregator agreed? | PCU `MSR_PKG_C_STATE_RESIDENCY` | RPMh aggregated vote correctly? |
| 3. Sequence fired? | PCU FW opcode trace | TCS fired? |
| 4. Rail actually dropped? | VR telemetry / scope on rail | PMIC telemetry / SPMI trace |
| 5. Wake clean? | PROCHOT, wake source MSR | RPMh wake votes, PMIC ramp before IP txn |

### 2.7 SoC topology / scheduling

**x86:**
- Hybrid (Alder Lake+): **P-cores** (perf) + **E-cores** (efficiency) under **Thread Director** (HW telemetry feeds OS scheduler hints)
- Per-core L2; shared L3 (LLC)
- Mesh interconnect (server) or ring (client)
- Per-core DVFS via HWP (rails/fuses depending on SKU)

**QC mapping:** P-core/E-core → **big.LITTLE / DynamIQ** (e.g., 1+5+2 prime + perf + efficiency) in **DSU (DynamIQ Shared Unit)**. Cores share L3 + ACE/CHI interconnect. **Per-core frequency real; per-cluster voltage** rail = MAX across cores (PMIC LDO count is finite). **EAS (Energy Aware Scheduling)** = capacity + idle-cost aware, Linux equivalent of Thread Director.

### 2.8 Boot chain

**x86 (briefly):**
```
Reset → Microcode patch → CPU init → CRTM (root of trust) → SEC (Pre-EFI Init)
     → PEI (memory init, MRC trains DDR) → DXE (drivers) → BDS (boot device select)
     → OS loader → kernel
```
Equivalent to Snapdragon flow but UEFI-shaped. MRC ≈ SBL's LPDDR training step. Verified Boot signs each stage.

**QC mapping:**
```
POR → PBL (ROM, EL3, validates SBL signature)
   → SBL/XBL (LPDDR training — long step; loads ATF + TZ + ABL into DRAM)
   → ATF (EL3, secure services, PSCI ready)
   → TZ kernel (S-EL1, TrustZone services)
   → ABL (Android Boot Loader, fastboot, verifies boot.img)
   → Linux start_kernel(), DT probe, init userspace
```
Signature check at every handoff. ROM holds immutable root-of-trust key. Anti-rollback fuse blocks older signed images.

### 2.9 LPDDR5/5X memory subsystem (shared between platforms)

**5 training phases at every cold boot:**
1. **CA training** — command/address eye centering
2. **Read DQ training** — per-DQ delay vs strobe
3. **Write DQ training** — same on write path
4. **ZQ calibration** — termination resistance vs reference
5. **VREF training** — receiver reference voltage centering

Failure of any = no DRAM = hang. Self-refresh in deep sleep (sub-mW); refresh 2× above 85°C (MR4 register tracks). On Intel I called this the **MRC (Memory Reference Code)** step in SEC/PEI; on Snapdragon it's the SBL/XBL stage. Same physics.

### 2.10 Clock / reset gotchas (architecture-agnostic)

- Always poll `PLL_LOCK` before un-gating IP — otherwise flops capture random data
- Glitch-free mux ≠ OR gate (runt pulses → metastable)
- CDC: 2-FF synchroniser for data, async FIFO for multi-bit, handshake for control
- Reset: assert async, **de-assert sync** to destination clock

---

## SECTION 3 — PCIe + NVMe (Days 10-11) (8 min)

### PCIe layers

3-layer point-to-point serial fabric:
- **Physical** — lanes (diff pairs), encoding, equalisation, CDR
- **Data link** — LCRC, ACK/NAK, flow control credits per traffic class
- **Transaction** — TLPs (MWr posted, MRd non-posted, Cpl, Cfg, Msg)

### Generations

| Gen | GT/s | Encoding | x16 BW |
|---|---|---|---|
| 3 | 8 | 128b/130b | 16 GB/s |
| 4 | 16 | 128b/130b | 32 GB/s |
| 5 | 32 | 128b/130b | 64 GB/s |
| **6** | **64** | **PAM4 + FEC + FLIT** | **128 GB/s** |

### TLP types

- **Posted (MWr)** = fire-and-forget — doorbells use this
- **Non-posted (MRd, CfgRd/Wr)** = wait for completion — reads serialise
- Tag matches Cpl to MRd; out-of-order completions normal

### Config space + ECAM

```
addr = ECAM_base | (bus<<20) | (dev<<15) | (func<<12) | (reg & 0xFFF)
```

Vendor ID `0xFFFF` = no device. Walk capability list from offset 0x34. BAR sizing: write all 1s, read back, low cleared bits = size.

### LTSSM states (link-bring-up state machine)

```
Detect → Polling → Config → L0 → Recovery / L0s / L1 / L1.1 / L1.2 / L2
```

### PCIe power states & PkgC6 connection

- L0 = active
- L0s = standby (entered/exited by HW, ns)
- L1 = software-managed sleep
- **L1.2** = deep sleep, PHY off, common-mode off — **REQUIRED for PkgC6**

L1.2 needs: ASPM enabled both sides + L1 PM Substates capability advertised + CLKREQ# routed + LTR values permissive.

### NVMe — why it wins

| | AHCI | NVMe |
|---|---|---|
| Queues | 1 × 32 | 65535 × 65536 |
| Per-IO latency | ~6 µs | ~2.5 µs |
| Locks | global | per-queue lock-free |
| IRQ | shared | MSI-X per queue |

### NVMe register set (BAR0)

| Off | Reg | Purpose |
|---|---|---|
| 0x00 | **CAP** | MQES, DSTRD, MPSMIN/MAX, TO (×500ms) |
| 0x14 | **CC** | EN, CSS, MPS, IOSQES (=6, 64B), IOCQES (=4, 16B) |
| 0x1C | **CSTS** | RDY, CFS (fatal), SHST |
| 0x24 | **AQA** | admin SQ size, admin CQ size |
| 0x28 | **ASQ** | admin SQ base (page-aligned, IOVA) |
| 0x30 | **ACQ** | admin CQ base |
| 0x1000+ | doorbells | `0x1000 + (2y)*(4<<DSTRD)` SQ tail; `+(2y+1)*` CQ head |

### NVMe bring-up dance

1. PCIe enumerate → BAR0 → ioremap
2. Read CAP → MQES, DSTRD, TO
3. CC.EN=0; spin until CSTS.RDY=0
4. Allocate Admin SQ+CQ (page-aligned, contiguous, DMA-mapped)
5. Write AQA, ASQ, ACQ
6. CC.EN=1; spin until CSTS.RDY=1 (or CFS=fatal)
7. Identify Controller (CNS=1), Identify Namespace (CNS=0)
8. Set Features: Number of Queues
9. **Create CQs before SQs** (CQ must exist first)
10. Issue Read/Write

### Command flow

```
Host: write SQE to SQ[tail] → barrier → MWr to SQ Tail Doorbell
Ctrl: DMA-fetch SQE → execute → DMA-write CQE
Host: poll CQ phase bit (or take MSI-X) → read status → MWr to CQ Head DB
```

CQE phase bit avoids host-side memset; flips on each CQ wrap.

---

## SECTION 4 — DVFS, P-states, PVT (Day 12) — DEEP DIVE (12 min)

### 4.1 The power equation (memorize each term)

$$P_{total} = P_{dynamic} + P_{static}$$

$$P_{dyn} = \alpha \cdot C \cdot V^2 \cdot F$$

| Term | Meaning |
|---|---|
| $\alpha$ | activity factor (fraction of nodes switching per cycle) |
| $C$ | total switched capacitance (gates + wires + interconnect) |
| $V$ | supply voltage — **squared**, dominant lever |
| $F$ | switching frequency — linear |

**Why V is squared:** energy stored on a capacitor = $\tfrac{1}{2}CV^2$; charging/discharging once per switch costs that energy. Switches per second ∝ F. Multiply → $\tfrac{1}{2}CV^2F$.

$$P_{leak} \propto V \cdot e^{-V_{th}/(kT/q)}$$

- Leakage is **exponential in T** (sub-threshold conduction worsens as T rises).
- At 7nm/5nm, leakage = **30-40% of total**.
- Feedback loop: more leakage → more heat → more leakage → **thermal runaway**. Throttling (PROCHOT, DTS) breaks the loop.

**Quantitative reflexes:**
- Drop V by 20% → dynamic power drops by **36%** (1 − 0.8²)
- Drop V by 30% → dynamic power drops by **51%** (1 − 0.7²)
- Halve F at same V → halve dynamic power; halve V at same F → quarter dynamic power (but then can't sustain F)

### 4.2 Why V and F are linked (the V-F curve)

Transistor switching delay $t_d \approx \frac{C \cdot V}{(V - V_{th})^\beta}$. Lower V → slower switching → max sustainable F drops. Each silicon corner has its own V-F curve — DVFS table is that curve quantised into discrete OPPs (Operating Performance Points).

```
F |        ────  P0 (turbo)
  |    ────      P1
  |  ──          P2
  | ─            P3
  |─             P4 (Vmin retention)
  +────────── V
       Vmin → Vmax
```

**Vmin** = lowest V where SRAM retains state and logic still meets timing. Going below = data corruption.

### 4.3 DVFS transition sequencing — THE rule

```
Going FASTER (P3 → P0):       1. Raise V first   (creates timing margin)
                              2. Wait Vstable     (~10-100 µs VRM ramp)
                              3. Then raise F     (PLL relock ~1-10 µs)

Going SLOWER (P0 → P3):       1. Lower F first   (frees timing slack)
                              2. Then lower V     (into the new slack)
```

**Reverse either → setup violations → data corruption / random `#GP` / silent flips.** This is a real classic FW bug: someone tries to be efficient by writing both V and F together; the VRM hasn't ramped yet but F already jumped → core clocks faster than V allows → metastable flops.

**Latencies — what's the bottleneck:**
- PLL relock: **1-10 µs**
- VRM voltage ramp: **10-100 µs** ← bottleneck
- Total DVFS transition: ~100 µs typical
- Why this matters: if you DVFS too aggressively (every 100 µs), you spend more time *transitioning* than at the new state. Modern governors batch decisions over ~1-10 ms windows.

### 4.4 P-states (ACPI / ACPI-like)

Discrete (V, F) tuples exposed to the OS. **Lower number = higher perf.**

| P-state | Mode | Example F | Example V |
|---|---|---|---|
| P0 | max active | 3.6 GHz | 1.15 V |
| P1 | high | 2.8 GHz | 1.05 V |
| P2 | medium | 2.0 GHz | 0.95 V |
| P3 | efficient | 1.2 GHz | 0.85 V |
| P4 | Vmin active | 0.8 GHz | 0.75 V |

**OS interfaces:**
- **Linux:** `cpufreq` framework. Drivers: `intel_pstate` (Intel HWP), `acpi-cpufreq`, `qcom-cpufreq-hw`. Governors: `performance`, `powersave`, `ondemand`, `conservative`, **`schedutil`** (modern, scheduler-driven, default on most distros).
- **Windows:** PPM (Power Policy Manager) → ACPI `_PSS` / `_PCT` → MSR write.
- **Intel HWP (Hardware P-states):** OS gives a hint (min/max/EPP — Energy Perf Preference 0=perf, 255=power); HW picks actual OPP. Faster than SW polling.

### 4.5 Turbo / boost — the opportunistic mode above P0

**When can the package go above P0?** When BOTH conditions hold:
1. **Thermal headroom** — Tj well below Tjmax
2. **Current headroom** — sum of core currents below Iccmax
3. **Power budget headroom** — package power below PL1/PL2 (RAPL limits)

**Where the budget comes from:** idle cores in C6 burn near-zero dynamic + small leakage → freed thermal/current → reallocated to active cores → they exceed P0 frequency.

**Intel-specific:**
- **PL1** = sustained power limit (long-term, e.g., 28 W TDP for ~28 sec window)
- **PL2** = short-term boost limit (~56 W for ~28 ms before falling back to PL1)
- **Tau (τ)** = time window over which PL1 is averaged
- Hardware-controlled by **PCU**, not OS
- If throttling is constant under sustained load → max-Q cooling undersized OR silicon at unfortunate FF corner running hot

**Reading limit reasons (post-Si triage):**
```
MSR_CORE_PERF_LIMIT_REASONS (0x64F)  — bits show: thermal? PL1? PL2? Iccmax? VR-therm?
MSR_GRAPHICS_PERF_LIMIT_REASONS      — same for iGPU
```

### 4.6 PVT corners — the variation reality

**Process** (silicon manufacturing variation):

| Corner | NMOS | PMOS | What it means |
|---|---|---|---|
| **TT** | Typical | Typical | Design target — most chips |
| **FF** | Fast | Fast | Fastest switching, **highest leakage**, **hottest** — worst for power/thermal |
| **SS** | Slow | Slow | Slowest switching, lowest leakage — **worst for timing** (may miss F at rated V) |
| FS | Fast | Slow | Skewed pull-down vs pull-up delay |
| SF | Slow | Fast | Opposite skew |

**Voltage:**
- VRM tolerance: ±3-5%
- IR drop across die: edge cores see lower V than rail center
- Transient droop on load step (sudden AVX kernel start): 5-10% dip before VRM reacts → why **license levels** preempt
- Validate at nominal, +10%, −10%

**Temperature:**
- Consumer: 0 to 100 °C
- Server: 0 to 105 °C
- Automotive: −40 to 125 °C
- Higher T → more leakage → typically slower transistors (mobility drops); **temperature inversion** can flip this at very low V (transistors faster at high T because V_th drops faster than mobility)

**Aging:**
- **NBTI** (Negative Bias Temperature Instability) — PMOS V_th drift under negative bias
- **HCI** (Hot Carrier Injection) — degrades over years
- Same DVFS table after 2 years may not meet timing → SKU eventually needs voltage offset

**Corner explosion at 7nm:**
$$5\ \text{process} \times 3\ \text{voltage} \times 3\ \text{temperature} \times 3\ \text{aging} \times \text{multi-Vt} = 200+\ \text{corners}$$

Pre-Si picks worst cases; post-Si finds local mismatch + aging + real cooling that simulation missed.

### 4.7 Speed binning — how SKUs are made

After fab, every chip tested. Sorted by **max sustainable F at given V/T**:

| Bin | SKU example | Max F |
|---|---|---|
| Top | i9 / SD X Elite Premium / 8cx Gen 4 | 3.8+ GHz |
| Mid | i7 / X Plus | 3.0-3.6 GHz |
| Lower | i5 | 2.4-3.0 GHz |
| Defect | discard, or disable defective parts | — |

Each bin gets its **own DVFS table** calibrated to the bin's actual silicon (FF chip can hit P0 at lower V than SS chip). Fuses program per-die voltage offsets.

### 4.8 Intel DVFS path — what I owned at Cambria (the canonical flow)

```
OS power policy (Linux schedutil / Windows PPM)
     │
     │ writes target via MSR_IA32_PERF_CTL  (legacy P-state)
     │   OR sets HWP hints in MSR_IA32_HWP_REQUEST  (min/max/EPP)
     ▼
PCU / PUnit  (on-die power microcontroller, runs PCU FW)
     │
     ├── Aggregates per-core requests + uncore requests
     ├── Checks blockers: PL1/PL2 budget, Iccmax, DTS thermal flag, VR settling timer
     ├── Picks the actual OPP (HWP autonomous mode = HW decides; legacy = OS decides)
     │
     ├── Talks to other IPs over IOSF Sideband (IOSF-SB) using P2P opcodes
     │
     ├── Programs FIVR / external VR over SVID / PMBus → Vcc ramps
     ├── Programs PLLs (relock on F change)
     └── Returns P-channel ACK to Core PMA
            │
            ▼
        Core transitions to new (V,F) operating point
```

**Key Intel-specific vocabulary I used daily on Cambria:**
- **PCU / PUnit** — on-die power microcontroller; client = PUnit, server = PCU
- **PCU FW opcodes** — discrete instructions the FW sequencer issues; opcode-level Zebu trace is how I caught the **ADR opcode-ordering hang**
- **IOSF-SB** — bus PCU uses to talk to other IPs (not on coherent fabric)
- **P-channel** — standardised Core PMA ↔ PCU power handshake protocol
- **HWP (Hardware P-states)** — OS sends hint (min/max/EPP); HW autonomously picks OPP, faster than SW polling
- **EPP (Energy Perf Preference)** — 0=perf, 255=power, scalar tunable
- **PL1 / PL2 / Tau** — sustained limit / short-term boost / averaging window
- **MSR_CORE_PERF_LIMIT_REASONS (0x64F)** — bits show: thermal? PL1? PL2? Iccmax? VR-therm? — first MSR I read on any throttle complaint
- **MSR_PKG_C_STATE_RESIDENCY** — measured cross-IP idle handshake
- **RAPL (Running Average Power Limit)** — joules per domain via MSR
- **FIVR (Fully Integrated Voltage Regulator)** — on-package VR (Haswell+); reduces transient droop
- **AVX license levels (0/1/2)** — preemptive F cut under high-current AVX2/AVX-512 to maintain V margin

**DTPM / RAPL interplay:** Intel's TDP is enforced by RAPL PL1 over Tau window. Going above PL1 briefly is allowed (PL2) until rolling average crosses → throttle. This is how Turbo budget is policed.

### 4.9 QC mapping — reference (one paragraph)

> "The same Intel DVFS flow on Snapdragon: OS doesn't write a P-state MSR — it makes a **PSCI** request or `qcom-cpufreq-hw` writes a vote into **CPRh registers** that get read by **RPMh** (the always-on Resource Power Manager hardened, equivalent of Intel's PCU but with a different scope). RPMh aggregates votes from every subsystem — Apps cluster, Modem, ADSP, CDSP/HTP, LPASS — and takes the MAX per resource (frequency, voltage, bus BW). It then programs the **PMIC** (separate die, e.g., PM8550) over the **SPMI bus** to ramp rails, plus the clock and bus controllers. **TCS** (Trigger Command Set) sequences are pre-loaded so wake takes µs, not ms — analogue of Intel's PCU sequence ROMs. The aggregator-not-MSR difference exists because Snapdragon has many independent firmwares each able to vote on shared rails; PSCI alone wouldn't aggregate cross-subsystem."

**The DynamIQ catch (architecture-shared with Intel hybrid):**
- Cores in same cluster typically share **one voltage rail** (PMIC LDO count is finite)
- Per-core *frequency* is real (independent clock dividers from cluster PLL)
- Per-core *voltage* is **NOT** — rail V = MAX(per-core requests on that rail)
- Same problem on Intel hybrid (P-cores and E-cores often on different rails but cores within share)
- Validation lever: characterise actual rail V vs per-core requests; find waste (one core at 1.0V drags whole cluster up even if others wanted 0.7V)

**CPRh (QC) ≈ AVS (Intel SVID feedback)** — Hardware closed-loop measures critical-path delay via ring oscillator → trims V down to silicon-actual-need (not worst-case-sim margin). Saves 50-150 mV per OPP. Validation: verify CPR converges, doesn't oscillate, doesn't undershoot under thermal/aging.

### 4.10 Adaptive techniques (modern, both architectures)

- **AVS (Adaptive Voltage Scaling)** — closed-loop V adjustment based on silicon actual speed (CPRh on QC)
- **AVFS (Adaptive V & F)** — both adjust adaptively
- **Per-core / per-cluster DVFS** — independent control where rails permit
- **DPM (Dynamic Power Management)** — combines DVFS + clock gating + power gating
- **ATPM / DTPM (Dynamic Thermal Power Management)** — budgets power across IPs (CPU vs GPU vs NPU) under shared thermal cap

### 4.10 DVFS validation — what you actually do

1. **OPP table sanity:** every (V,F) point in the table must run a stress workload (e.g., dhrystone, mprime) for N hours without errors at every PVT corner the SKU ships into.
2. **Transition stress:** rapid P-state hopping (every 100 µs to every 100 ms) — looking for V/F sequencing bugs.
3. **Wake-from-idle latency:** after C6, first instruction should execute at correct V/F (not stale).
4. **Multi-core interactions:** core 0 at P0 + core 1 at P4 — does shared rail V satisfy both? Is per-core F actually independent?
5. **Thermal-coupled DVFS:** ramp ambient → does throttle engage at correct Tj? Does it release cleanly?
6. **Cross-IP power budget:** GPU + NPU + CPU all at max → does package power stay under PL1? Does the right IP throttle first per policy?
7. **Workload characterisation:** measure actual perf-per-watt at each OPP for representative workloads → identify the **knee of the Pareto curve** (past it, more watts buys near-zero perf).

### 4.11 Common DVFS bugs (interview-worthy)

| Bug | Symptom | Root cause |
|---|---|---|
| **V-F transition reversed** | Random `#GP` / data corruption when jumping P3→P0 | FW raised F before V was stable |
| **Turbo never engages on 1-thread workload** | Single-thread perf low | DTS miscalibrated → PCU thinks chip is hot |
| **Constant PROCHOT throttle** | Sustained workload runs at low F | Cooling undersized OR FF corner OR PL1 too tight |
| **Cold-boot SS-corner hang** | Specific SKU hangs at -20°C | Binning DVFS table didn't margin enough V at lowest P-state for SS |
| **Per-core DVFS appears wasted** (Snapdragon) | Big power consumption when only 1 core busy | Cluster rail = MAX → that 1 core pulls whole cluster V up |
| **VRM droop transient hang** | Hang on AVX kernel start | Decoupling missing or VRM slew too slow → V dipped below Vmin |
| **Aging regression after 2 years** | Same workload now slower | NBTI/HCI shifted V_th → DVFS table no longer meets timing → need V offset retune |
| **CPR oscillation** | V flickers, intermittent failures | CPRh closed-loop unstable; gain too high; needs damping |
| **DVFS thrashing under bursty load** | Net power UP, not down | Governor too aggressive → spends time in transitions |

### C-states (idle)

| State | Mechanism | State preserved | Wake | Power saved |
|---|---|---|---|---|
| C0 | active | — | — | none |
| **C1/C1E** | **clock-gated** | flops keep state | ~1-2 µs | dynamic only |
| **C6** | **power-gated** | saved to retention SRAM (S3M); rail collapses | ~100 µs+ | dynamic + leakage |

### PkgC6 — cross-IP idle dance

Triggers when ALL cores in C6 AND every uncore IP asserts idle:
- LLC, memory ctrl, **PCIe ctrl (link in L1.2!)**, display, USB, NIC, IRQ ctrl
- Then PCU flushes LLC→DRAM, gates uncore fabric, collapses package Vcc
- Exit dominated by **LLC reload from DRAM** (~hundreds µs)
- **#1 PkgC6 blocker = PCIe link not in L1.2**

### S0ix / Modern Standby

OS-visible connected standby targeting PkgC10. <100 mW budget. If PkgC6 broken → S0ix over budget. Snapdragon analogue: deep-sleep state RPMh aggregates into via PSCI.

### ADR (Asynchronous DRAM Refresh) — power-loss flow

```
PSU AC loss → PLI asserted → PCH asserts ADR_TRIGGER → PCU FW sequence:
  IO_QUIESCE → FABRIC_QUIESCE → (eADR: LLC writeback) →
  WPQ_FLUSH (IMC drains) → DDR_SELF_REFRESH → (NVDIMM SAVE_n#) → rails off
```

Hold-up budget = 10-30 ms from PSU caps + supercaps. eADR adds LLC writeback so PMem stores persist without explicit `clwb`/`clflushopt`.

### Intel power-mgmt vocabulary (use precisely!)

- **PCU / PUnit / PMC FW** — on-die microcontroller running power state machine
- **IOSF Sideband (IOSF-SB)** — bus PCU uses to talk to other IPs
- **P2P opcodes** — PUnit-to-PUnit message opcodes
- **P-channel** — Core PMA ↔ PCU power handshake
- **S3M SRAM** — always-on save/restore SRAM for C6 state
- **CFC** — clock-forwarding/crossing FIFO (always-on ↔ power-gated)
- **WPQ** — Write Pending Queue inside IMC
- **PLI** — Power Loss Imminent

---

## SECTION 4B — Thermal, RAPL, Perf Counters, Multi-IP (Day 15) (6 min)

### Junction temperature equation

$$T_j = T_{ambient} + P_{total} \cdot \theta_{JA}$$

$$\theta_{JA} = \theta_{JC} + \theta_{CS} + \theta_{SA}$$

| Term | Meaning |
|---|---|
| $T_j$ | silicon junction temp (hottest point) |
| $\theta_{JC}$ | junction-to-case (silicon → IHS through TIM) |
| $\theta_{CS}$ | case-to-sink (heatsink interface) |
| $\theta_{SA}$ | sink-to-ambient (fin to air) |

**Dried TIM** doubles $\theta_{CS}$ → Tj jumps → same workload now throttles. Classic field bug.

**Tjmax** = spec limit (consumer 100°C, server 105°C). Cross → throttle. Continuous near-Tjmax → reliability degradation (electromigration accelerates).

### PROCHOT# and DTS

- **PROCHOT#** — bidirectional throttle signal. Asserted → CPU drops F/V. A hot neighbour chip on the shared signal can throttle CPU.
- **DTS (Digital Thermal Sensor)** — on-die temp sensors read by PCU. Calibration fuses programmed at fab; if missing/wrong → DTS reads hot/cold by 10-15°C → throttle misbehaves. Verify with external thermocouple during validation.
- **Hotspot** vs die-average: AVX-512 core can be **10-15°C above die avg**. Throttling must follow hotspot sensor, not average.

### RAPL (Running Average Power Limit) — energy counters

| Domain | Coverage |
|---|---|
| **PKG** | whole package |
| **PP0** | cores |
| **PP1** | client iGPU |
| **DRAM** | server SKUs only |

Read MSR twice, subtract joules, divide by elapsed → average watts.

**RAPL ≠ wall power.** RAPL = VR input estimate. Ground truth needs external power meter (Quarch). Typical gap **20-30%** (VR loss + DRAM not in client RAPL + platform devices + PSU efficiency). Always say this in interviews — shows measurement literacy.

ARM equivalent: SCMI / SCP firmware exposes per-rail energy. Snapdragon Profiler reads PMIC telemetry directly.

### PMU — performance monitoring unit

- **Fixed-function (3 on Intel):** instructions retired, core cycles, reference cycles
- **General-purpose (4-8 per core):** programmable to count any of hundreds of events
- **Uncore counters:** LLC, memory ctrl, NoC, IIO — chip-scope, not per-core

| OS / Vendor | Tool |
|---|---|
| Linux any | `perf stat`, `perf record`, `perf top` |
| Intel | PCM, VTune, EMON (internal) |
| ARM | Streamline, Snapdragon Profiler, Linux `perf` |
| AMD | μProf |
| Windows | WPR/WPA, ETW, GPUView |

### Top-down microarchitecture analysis (TMA) — Intel methodology

Every pipeline slot lands in one bucket:

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

**Workflow:** collect TMA L1 → identify dominant bucket (e.g., "62% Backend Memory Bound") → drill L2/L3 (DRAM BW? LLC miss? Store buffer full?) → now you know **where** to optimise.

**IPC alone is meaningless** without TMA. IPC=0.5 might be fine for memory-bound or terrible for compute-bound.

### Counter signatures → root cause cheat sheet

| Symptom | Counter signature | Likely cause |
|---|---|---|
| Low IPC, high cache-miss | LLC-miss high, DRAM BW high | working set > LLC; missing tile/blocking |
| Low IPC, low cache miss | front-end bound, μop cache miss | I-cache thrash, large code footprint |
| High IPC, slow wallclock | ref_cycles ≫ core_cycles | freq throttle (read `MSR_CORE_PERF_LIMIT_REASONS` 0x64F) |
| ST fast, MT doesn't scale | LLC contention, snoop traffic high | false sharing or coherence storm |
| Latency variance run-to-run | C-state residency high | idle states between bursts; pin governor `performance` |
| `BR_MISP_RETIRED` high | branch mispredict bound | branchy hot loop; PGO / branchless |
| `DTLB-misses` high | TLB-walk-cycles high | use **huge pages** (2 MB / 1 GB on x86; 2 MB on ARM) |

### Multi-IP interference — five classes (modern SoC reality)

```
CPU   GPU   NPU   ISP
 │     │     │     │
 ╪═════╪═════╪═════╪═══  shared NoC
 │     │     │     │
 └─────┴─────┴─────┴───→ shared LLC → Memory Ctrl → DDR
```

| # | Class | Counter to detect | Mitigation |
|---|---|---|---|
| 1 | **Memory BW starvation** | per-agent BW counter at MC | QoS / priority / BW reservation |
| 2 | **LLC thrashing** | per-agent LLC occupancy + miss | Cache Allocation Tech (CAT/CLOS) — way partitioning |
| 3 | **NoC congestion** | NoC link util, back-pressure cycles | virtual channels, priority arbitration |
| 4 | **Power-budget conflict** | RAPL package + per-domain | Dynamic Thermal Power Mgmt (DTPM) |
| 5 | **IOTLB / shared IOMMU** | IOTLB miss + page-walk latency | per-agent IOTLB partition, huge pages |

### Multi-IP validation methodology — 4 steps

1. **Single-IP baseline** — measure each IP alone (BW, latency, IOPS, fps)
2. **Pair-wise** — every 2-IP combination; degradation vs baseline
3. **Full system** — all IPs active; measure worst case; check QoS
4. **QoS validation** — enable QoS; repeat 2+3; verify guarantees deliver

**Skipping step 1 is the #1 mistake** — without baseline you can't measure interference.

### Benchmark hygiene (ALWAYS do these)

- [ ] Pin freq (disable Turbo OR set `governor=performance`)
- [ ] Disable HT for compute benchmarks
- [ ] Pin to specific CPUs: `taskset -c 4-7`
- [ ] Drop file caches: `echo 3 > /proc/sys/vm/drop_caches`
- [ ] Stop background services
- [ ] Warm up — discard first N runs (cache fill, JIT, freq ramp)
- [ ] Repeat ≥10 runs; report **median + 95% CI**, not single number
- [ ] Aggregate with **geomean** for ratios, **harmonic mean** for rates, **arithmetic** for absolute durations only

### Perf-per-watt + EDP (figures of merit)

- **perf/W** = throughput / avg watts. Higher better. Mobile + DC primary metric.
- **EDP** = Energy × Delay = $P \cdot t^2$ for fixed work. Lower better. Penalises both slow-and-efficient and fast-and-power-hungry.
- **ED²P** weights latency more — DC use case.
- **Pareto curve knee** = where $\Delta perf / \Delta watts$ collapses. Past it = burning watts for no gain. Find the knee per workload.

---

## SECTION 4C — Cache Coherence + False Sharing (Day 5) (3 min)

### MESI protocol

Each cache line in one of 4 states per core:

| State | Meaning |
|---|---|
| **M** odified | this core has only valid copy + dirty (must write back) |
| **E** xclusive | this core has only copy + clean |
| **S** hared | multiple cores have clean copies |
| **I** nvalid | not present |

**Reads:** I → S (or E if no other has it). **Writes:** must reach M → if other cores have it (S), broadcast invalidation first → S → I on others.

**MESIF** (Intel) adds **F**orward — designates one S-line as the responder for new requests (avoids all S-cores responding).
**MOESI** (AMD/ARM) adds **O**wned — dirty + shared without writeback (lazy WB).

### False sharing — THE coherence performance bug

Two threads write to **different** variables that **happen to share the same 64B cache line**:

```c
struct {
    int counter_a;   // thread A writes
    int counter_b;   // thread B writes — same 64B line!
} shared;
```

Each write invalidates the other core's copy → cache line ping-pongs M↔M between cores → BW saturation, IPC tanks, no scaling. Single-thread fast, multi-thread doesn't scale.

**Fix:**
```c
struct {
    int counter_a;
    char pad[60];          // pad to 64B
    int counter_b;
} shared __attribute__((aligned(64)));
```

Or `alignas(64)` in C++. Or per-CPU variables.

**Detect:** `LLC-load-misses`, `cache-references` per-core, snoop counters high. On Intel: `OFFCORE_RESPONSE.*`.

### Coherence storm

Many cores all writing to the same hot line (e.g., shared global counter under contention) → invalidations broadcast → directory/snoop traffic dominates → throughput collapses. Fix: per-CPU counters + sum on read; or atomic with backoff; or RCU for read-mostly.

---

## SECTION 5 — CPU Microarchitecture (Day 16) (5 min)

### Pipeline + OoO

Front-end: I-cache → pre-decode → branch predictor → decoders → μop cache → allocate.
Backend: rename (RAT) → reservation station → execute → ROB → retire.

- **Renaming** kills WAR/WAW (only RAW remains)
- **ROB** retires in-order even though execution is OoO; bounds the in-flight window (~512 on Golden Cove)
- **Mispredict cost:** ≈ pipeline depth × IPC (15-20 cycles wasted)
- **μop cache (DSB)**: x86 needs it (variable length); ARM doesn't (fixed 4B)

### Memory consistency

| Model | Examples | Reorderings |
|---|---|---|
| **TSO** | x86 | only store→later-load via store buffer |
| **Weak** | ARM, RISC-V, POWER | almost any pair can reorder |

x86: `volatile` usually enough for MMIO. ARM: explicit `dmb`, `dsb`, `isb`. Producer-consumer: `dmb st` producer + `dmb ld` consumer (or `STLR/LDAR`).

### C/C++11 memory orders (cross-platform reflexes)

| Order | Use case |
|---|---|
| `relaxed` | counters, statistics — no ordering, only atomicity |
| `acquire` | load that gates subsequent reads (consumer flag) |
| `release` | store that publishes prior writes (producer flag) |
| `acq_rel` | RMW that does both |
| `seq_cst` | default; total order across all threads (most expensive) |

**Producer-consumer pattern done right:**
```c
// Producer
data = compute();
atomic_store_explicit(&ready, 1, memory_order_release);

// Consumer
while (atomic_load_explicit(&ready, memory_order_acquire) == 0) ;
use(data);   // guaranteed to see producer's writes
```

On x86 these are mostly no-ops (TSO does the work). On ARM the compiler emits `STLR` / `LDAR`. **Hand-rolled `volatile` flags are an x86-only bug.**

### SIMD + Intel license levels

| License | Window | F offset |
|---|---|---|
| 0 | scalar | 0% |
| 1 | AVX2-heavy | ~10% |
| 2 | AVX-512-heavy | ~25% |

A few AVX-512 in mostly-scalar code drags the whole core down → net throughput LOWER than AVX2. Use `-mprefer-vector-width=256`.

### Prefetcher triage

HW prefetchers can hide a missing software prefetch — silicon A passes, B with different heuristic regresses. **Standard triage:** disable HW PF (Intel MSR `0x1A4`), re-run.

---

## SECTION 6 — NPU Stack (Day 18) (8 min)

### The 6-layer NPU stack

```
1. Framework (PyTorch / TF) — training
2. ONNX (op-graph IR) — Conv, MatMul, Softmax + QDQ nodes
3. ONNX Runtime — graph partition + EP selection
4. Execution Provider — QNN | OpenVINO | DirectML | CPU (always present, fallback)
5. Vendor runtime + compiler — QnnHtp.dll → libQnnHtp.so; QDQ fusion, tiling, context cache
6. Silicon — Hexagon HTP / Intel NPU / AMD XDNA; HMX + HVX + DMA over VTCM
```

### EP partitioning

ORT asks each EP per-op: "can you run this?" → contiguous subgraphs assigned. **More partitions = more CPU↔NPU handoffs = worse perf.** One unsupported op in middle of clean graph can cut throughput 5-10×.

### EP fallback = THE silent perf cliff

One op unsupported → silently falls back to CPU EP → functional pass, perf collapse. **Detection:**
- `EP_assignment == CPU` per inference
- ETW: QNN DLL loaded but CPU% high during inference window
- ORT verbose: `[VERBOSE] node X assigned to CPUExecutionProvider`

### QDQ fusion

ONNX models for NPUs are **QDQ-format**: `Quantize → Op → Dequantize`. Compiler's most important pass: fuse to single INT8 op. If fusion fails (model rebuilt without `keep_quantized_weights=true`, scale mismatch) → drops to CPU EP.

### Tiling + VTCM

Activations rarely fit. Compiler tiles (e.g., 256×256 + halo) so each fits in VTCM.

| NPU | On-chip SRAM |
|---|---|
| **Hexagon HTP v79** (X Elite) | **8 MB VTCM** |
| Intel NPU (LNL) | ~2 MB |
| AMD XDNA | ~4 MB |

VTCM is **software-managed, not in CPU coherence domain**. Compiler/runtime owns DMA scheduling. Tile-too-big → multiple DMA round-trips → per-tile latency doubles.

### Context cache

```
First launch:  ONNX → partition → lower → fuse → tile → compile → SAVE bin
Second launch: load bin → QnnContext_createFromBinary() → ready in ms
```

Skipping cache (debug build, version mismatch) re-pays compile latency every launch.

### Inference hot loop — the 3-engine pipeline

```
1. CPU prepares input in DDR
2. CPU writes NPU command-queue doorbell (MMIO)
3. NPU DMA: DDR → VTCM
4. Steady state: DMA(N+1) ‖ HMX(N) ‖ HVX(N-1)    ← how you hit ~45 TOPS
5. NPU DMA: VTCM → DDR
6. NPU MSI-X to CPU
7. ORT returns output
```

If any engine stalls (tile cliff, OOM, host starvation) → degrades to serial → latency multiplies.

### Hexagon HTP family (memorize)

| HTP | SoC | TOPS | VTCM |
|---|---|---|---|
| v68 | SD 888 | ~15 | 4 MB |
| v69 | SD 8 Gen 1 | ~18 | 4 MB |
| **v73** | SD 8 Gen 2 | ~26 | **8 MB** ← VTCM doubled |
| v75 | SD 8 Gen 3 | ~36 | 8 MB |
| **v79** | **SD X Elite** | **~45** | **8 MB** |

Other anchors: HMX 4096 MACs/instr; FastRPC wake ~100 µs; **DDR ~60 GB/s shared = real bottleneck**, not TOPS.

### Three NPU compute engines

| Engine | Job |
|---|---|
| **HMX** | INT8 matmul, 4096 MACs/instruction |
| **HVX** | vector ops, requantisation |
| **DMA** | tile fetch / output writeback |

---

## SECTION 7 — Debug Methodology (Day 20) (5 min)

### 7-step framework

1. **Observe** — symptoms, registers, logs, PVT corner
2. **Reproduce** — establish consistency
3. **Isolate** — narrow scope (binary search, swap components)
4. **Hypothesise** — testable theory
5. **Test** — design experiment
6. **Root-cause** — exact RTL/FW/silicon defect
7. **Fix & Verify** — re-test all affected scenarios

### Reproducibility → bug class

| Repro rate | Likely class |
|---|---|
| 100% | functional (design or FW) |
| Intermittent | race / marginal timing |
| Only at PVT corner | V-droop, thermal, process |
| Single unit | silicon defect |

### Bug-cost ladder (10× per stage)

```
RTL Sim (Hz-kHz) → Emul (MHz, Zebu/Palladium) → FPGA (10-100 MHz) → Post-Si (GHz)
   functional      HW/FW integration            full SW stack      analog/PVT/real
```

### Story shape (90 sec each)

**Symptom → Hypothesis → How I narrowed it → Root cause → Fix → Learning.**
- Lead with symptom, not project backstory
- One hypothesis at a time; show test that proved/killed it
- Honest framing > overclaiming
- End with the lesson

### Tool vocab

| Tool | Domain |
|---|---|
| JTAG / Lauterbach | post-Si bring-up; single-step, breakpoint |
| Lecroy / Tektronix | PCIe / USB / NVMe protocol trace |
| Oscilloscope | rail droop, signal integrity |
| Zebu / Palladium / Veloce | pre-Si emulation |
| Perspec | scenario-based directed test |
| ETL + WPA | Windows post-Si trace, GPUView |
| AER / MCA | PCIe / CPU error log |

---

## SECTION 8 — The 3 Bug Stories (rehearse aloud) (5 min)

### STORY 1 — Core C6 wake-loss (Intel Cambria pre-Si) — for QC CPU validation

> "Pre-Si validation of Core C6 entry/exit on Cambria. Workload: idle thread with local APIC ARAT timer firing back-to-back at ~2 µs to stress the wake path. Sporadic 50-500 ms hangs on the idle thread. Perfmon showed C6 residency continuing past when the second wake should have fired, even though the first wake had serviced its IRQ.
>
> I pulled the Core PMA wake-pending CSR in Zebu trace — wake #2 latched in always-on but `DOMAIN_STATUS.CORE_STATE` never went C6→C0 for that event. Set Lauterbach trigger on `CORE_STATE==C6 && wake_in==1 && cfc_init_busy==1` — caught every failure in the window where the CFC FIFO was being re-initialised after PLL relock but before interrupt-buffer drain completed.
>
> Root cause: 2-clock-domain race in C6 exit. Wake #2 sampled in always-on, ack edge into core domain crossed a CFC FIFO that was mid-reset — edge never crossed. Fix: hold wake latch in always-on until both CFC `init_done` and interrupt-buffer `drain_done` asserted; gate CORE_STATE→C0 on both. Added Perspec assertion for regression. Filed HSD 16015450088.
>
> Lesson: anything crossing always-on into a powered-down domain must hold intent until sink clock and reset are both stable — classic 'works in unit-sim, fails in full-die emul' class because unit testbenches don't exercise both transients at the same instant."

**Why for QC:** Same class as RPMh/TCS wake-path bugs.

### STORY 2 — NVMe CSTS.RDY hang (L&T@SanDisk post-Si) — for spec literacy

> "Polaris NVMe controller boot validation in ROM mode. Standard init: PERST# de-assert, wait CSTS.RDY=0, program AQA/ASQ/ACQ, set CC.EN=1, poll CSTS.RDY=1 within CAP.TO. New host script on Oakgate hung at the CSTS.RDY=1 poll on every boot. CC.EN read back as 1, CSTS.CFS clear, but RDY stayed 0 past CAP.TO.
>
> I diff'd the new script against working — only difference was AQA: new script programmed ASQS=ACQS=0, treating it as a 0's-based field and 'wanting one entry.' Walked NVMe 1.3 §4.1: every queue reserves one slot for head/tail empty/full disambiguation, so AQA size 0 means zero usable slots — illegal.
>
> Re-ran with ASQS=ACQS=3 and contiguous AQ buffers — CSTS.RDY came up every boot. Fix on host side, plus FW ask: ROM should set CSTS.CFS when it sees CC.EN=1 with AQA=0 so next person debugs in seconds.
>
> Lesson: when CSTS.RDY hangs on first boot, suspect the host before the device — AQA, ASQ/ACQ physical address, contiguous-buffer requirement (CAP.CQR)."

**Why for QC:** CPU-side spec discipline + bring-up depth.

### STORY 3 — QNN/ORT page-fault + VM regression (MSFT current) — bridge story

> *Honest framing first: platform validation, not Hexagon RTL.*
>
> "On the QNN EP path for a PSMU model on Copilot+ PC X Elite, we saw a memory regression across EP/ORT drops. Known-good EP 2.39 + ORT 1.23.3 had ~7K page-fault events and ~300 MB virtual memory during inference. With EP 2.45 + ORT 1.24.4, VM grew to ~502 MB and in one run page faults spiked to ~102K.
>
> I did NOT treat it as a VTCM or silicon defect first. Built a version matrix: old EP/old ORT, new EP/new ORT, new EP with old ORT. The reduced VM in that combo proved the bug followed the ORT/QNN context-loading path, not the EP itself.
>
> ETL/WPA correlation showed the newer path accumulating multiple model context buffers together — touching many host virtual pages at once caused the fault burst. Source pointer landed on `qnn_backend_manager.cc::CreateContextFromListAsync()`. Fix direction: bounded context lifetime — load, create, release per context unless parallel loading is needed — and a regression gate that records EP version, ORT version, page-fault count, VirtualAlloc count, peak VM.
>
> Lesson: page faults alone are not the bug. Correlate with VirtualAlloc and peak memory, never blame hardware first."

**Why for QC:** Modern AI workload exposure on their silicon + method discipline.

### BACK-POCKET — Paint SR starves Camera (60 sec)

> "Two AI workloads sharing the NPU. Paint SR submits ~36 inference tiles back-to-back; Camera preview submits at 30 fps. When SR runs, Camera drops frames. Built deterministic repro with two semaphore-fired threads + QPC timestamps. GPUView showed Camera frame queued at t=3 ms but not starting on the NPU until t=108 ms — sitting behind SR's 36 tiles. HTP exposes a single hardware scheduler, no preemption; once a tile dispatches, runs to completion. Root cause: structural — single non-preemptive queue + no priority hint from framework. Fix path: framework-side priority lanes. Lesson: a single hardware scheduler is fine until two real-time tenants share it; framework must expose priority."

---

## SECTION 9 — Behavioral / Fluency Q&A (5 min)

**Q: Pre-Si vs post-Si — fundamental difference?**
Pre-Si runs on RTL sim/emulators (Zebu, Palladium) before tape-out — full visibility, infinite breakpoints, slow (kHz). Catches functional/protocol bugs. Post-Si runs on real silicon at full speed (GHz) — limited to JTAG/traces. Catches analog, timing margin, PVT-corner bugs, anything sim model didn't capture.

**Q: Validation vs verification?**
Verification = "did we build the chip the spec said?" (RTL functional checking, DV). Validation = "does the chip do what the customer needs?" (system-level, real workloads, PVT, software stack). Verification = unit-test-of-design; validation = integration-test-of-product.

**Q: Why is multi-SoC validation harder?**
Same workload behaves differently across cache hierarchy, NPU dispatch, frequency governor, thermal limit, driver maturity. Can't blame "the silicon" — must isolate which layer (HW/FW/driver/OS/app) caused the delta.

**Q: Tell me about disagreeing with a designer.**
Situation → your evidence (waveforms, repro, perf counters) → escalation (1:1 first, then data review with both managers, never email war) → outcome (root cause confirmed on either side — both valid endings) → process change after.

**Q: Why leaving Microsoft?**
"Not running from Microsoft — going toward vendor-side ownership. Microsoft gave me 3-vendor breadth and EdgeOptimizer/ETL discipline. Next step: take that platform-validation rigor inside a vendor and own silicon end-to-end — pre-Si through PV — instead of partnering with the vendor on triage."

**Q: Typical day?**
Morning: triage previous night's automated runs (ETW traces from device farms across LNL/Strix/X Elite); flag any regression that crossed a gate (page faults, peak VM, EP-assignment %, S0ix residency). Mid-day: deep-dive one regression — version matrix across EP/ORT/SDK/model, correlate counters, file/update vendor partner ticket. Afternoon: EdgeOptimizer feature work or agentic-CLI workflow extension. End: cross-vendor review with LNL/XDNA/Hexagon partners.

**Q: Strongest project?**
EdgeOptimizer. Problem: multi-vendor NPU perf comparison was manual, 2-hour triage per regression. Solution: platform-level instrumentation tool — ETW provider for ORT/EP events, RAPL/energy collector, EP-assignment tracker, version-matrix dashboard. Impact: regression triage hours → minutes; caught QNN page-fault regression and silent-EP-fallback class.

**Q: First 90 days here?**
Days 1-30: shadow current validation flow, read recent bug postmortems, build my own first-pass profile of silicon and toolchain. Days 31-60: take ownership of one regression class end-to-end; ship one improvement to validation gate. Days 61-90: propose one cross-team workflow improvement (likely agentic-trace pipeline).

**Q: Tough bug?**
For QC CPU system: lead with **Core C6 wake-loss (Story 1)**. For NPU role: lead with **QNN page-fault (Story 3)** then C6 for credibility.

---

## SECTION 10 — Quick C / DSA Reflexes (3 min)

### MMIO doorbell pattern (must say aloud)

```c
// ARM weak model — DMB ST is critical
desc->cmd  = CMD_FOO;
desc->addr = buf_pa;
__asm__ volatile ("dmb st" ::: "memory");   // <-- THIS
*(volatile uint32_t *)doorbell_mmio = 1;
```

### Bit ops

```c
#define SET(x,n)    ((x) |=  (1u << (n)))
#define CLR(x,n)    ((x) &= ~(1u << (n)))
#define TOG(x,n)    ((x) ^=  (1u << (n)))
#define TST(x,n)    (!!((x) & (1u << (n))))
```

### `volatile` for MMIO

```c
volatile uint32_t *reg = (volatile uint32_t *)BAR0;
uint32_t v = reg[STATUS_OFFSET / 4];   // forces actual MMIO read
```

### Reverse linked list

```c
node *rev(node *h) {
    node *p = NULL, *c = h, *n;
    while (c) { n = c->next; c->next = p; p = c; c = n; }
    return p;
}
```

### Valid parentheses (LC 20)

```python
def isValid(s):
    pairs = {')':'(', ']':'[', '}':'{'}
    st = []
    for c in s:
        if c in '([{': st.append(c)
        elif not st or st.pop() != pairs[c]: return False
    return not st
```

### Ring buffer (lock-free SPSC)

- `head` written by producer only; `tail` written by consumer only
- Empty: `head == tail`; Full: `(head+1) % N == tail`
- ARM: `STLR` on head update, `LDAR` on tail read by producer (and vice versa)

---

## SECTION 11 — One-Liner Reflexes (drop these in answers)

- "Pre-Si = built it right. Post-Si = built the right thing."
- "Reproducibility rate is the first triage signal."
- "DMB ST between descriptor and doorbell — most common ARM driver bug."
- "Posted = fire-and-forget. Non-posted = wait. Doorbells are posted."
- "PkgC6's #1 blocker is PCIe not in L1.2."
- "Going faster: V first then F. Going slower: F first then V."
- "FF corner = worst for power. SS corner = worst for timing."
- "Per-core *frequency* is real on DynamIQ; per-core *voltage* is not — rail = MAX across cluster."
- "VRM ramp ~10-100 µs is the DVFS bottleneck, not PLL relock."
- "20% V drop saves 36% dynamic power — that's why V is the dominant lever."
- "Leakage is exponential in T — that's the thermal-runaway feedback loop."
- "Turbo budget comes from idle cores in C6 freeing thermal + current headroom."
- "RAPL is VR-input estimate, not wall power — typical gap 20-30%."
- "IPC alone is meaningless — use TMA to find the bottleneck bucket first."
- "DTS calibration fuses missing → throttles 15°C off — verify with thermocouple."
- "False sharing = two threads, different vars, same 64B line — pad to 64B."
- "RPMh aggregates active/sleep/wake votes, MAX wins, programs PMIC over SPMI."
- "ITS = ARM's MSI-X. Stream ID = ARM's Requester ID."
- "EP fallback to CPU is the silent perf cliff — functional pass, perf collapse."
- "QDQ fusion failure → CPU EP fallback → silent regression."
- "VTCM is software-managed, not in CPU coherence domain."
- "3-engine NPU pipeline: DMA(N+1) ‖ HMX(N) ‖ HVX(N-1) — that's how v79 hits 45 TOPS."
- "Page faults alone aren't the bug. Correlate with VirtualAlloc and peak VM."
- "Lead with the symptom. One hypothesis at a time. End with the lesson."
- "Bug cost rises ~10× per validation stage."
- "Disagreement: lead with data, 1:1 first, never email war."

---

## FINAL 5-MINUTE PRE-INTERVIEW CHECKLIST

- [ ] Said the 60-sec pitch aloud
- [ ] Said one bug story aloud (rotate Story 1/2/3)
- [ ] Recited: "DMB ST between descriptor and doorbell"
- [ ] Recited RPMh: "votes → MAX aggregate → PMIC over SPMI; TCS for fast wake"
- [ ] Recited DVFS rule: "V first then F going up; F first then V going down"
- [ ] Recited HTP v79: "8 MB VTCM, ~45 TOPS, 4096 MACs/instr HMX, 60 GB/s DDR is real bottleneck"
- [ ] Recited PkgC6: "all cores C6 + every uncore IP idle + PCIe L1.2 → flush LLC → collapse rail"
- [ ] Recited TMA: "IPC alone is meaningless — bucket the slot first (Frontend/Memory/Core/BadSpec/Retiring)"
- [ ] Have honest framing ready: "OEM/platform validation, I correlate ETW + version matrices; I don't debug Hexagon RTL"
- [ ] Breathe. You know this.

**Lead with calm. Lead with data. Good luck.**

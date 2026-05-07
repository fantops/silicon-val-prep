# Day 09 — ARM / Qualcomm Platform Vocabulary

## Concept

Qualcomm SoCs are ARM. Most of what you learned in Days 1-8 was named in x86 vocabulary (rings, APIC, VT-d, MTRR). Today translates that vocabulary to AArch64 (EL0/1/2/3, GIC, SMMU, MAIR), then layers Qualcomm-specific power infrastructure (PSCI, RPMh, PMIC, TCS) and SoC bring-up vocabulary (PLL lock, glitch-free mux, LPDDR training, PBL → SBL → ATF → ABL → kernel).

## Why this day in the flow

Days 1-8 are platform-agnostic memory/bus theory. Days 10-13 dive into PCIe, NVMe, DVFS and C-states — material interviewers will probe through ARM/Qualcomm vocabulary if you target QCT. Day 9 inserts the translation table now so every later answer naturally uses the right names. Without today, you will say "ring 0" in a Snapdragon interview.

In the Layer map this is the **Layer 1 → Layer 3 ARM bridge**.

---

## Deep Study Notes

### 1. AArch64 ISA in 60 seconds

- 31 general-purpose 64-bit registers `X0..X30` + `SP`, `PC`, `NZCV` flags. `W0..W30` = lower 32 bits.
- Fixed-width 4-byte instructions → trivial parallel decode, no μop cache.
- **Load/store architecture** — arithmetic only on registers; memory only via `LDR/STR`.
- AAPCS64 calling convention: `X0-X7` args, `X0/X1` return, `X19-X28` callee-saved, `X29=FP`, `X30=LR`.
- SIMD/FP: `V0-V31` (128-bit NEON) and `Z0-Z31` (variable-width SVE/SVE2).

### 2. Exception Levels — the Ring map

ARM replaces x86 ring 0 / ring 3 with **four levels** plus an orthogonal Secure / Non-Secure split (TrustZone).

| EL | Privilege | Typical occupant | x86 analogue |
|---|---|---|---|
| **EL0** | Lowest | User-space apps | Ring 3 |
| **EL1** | OS kernel | Linux/Android kernel | Ring 0 |
| **EL2** | Hypervisor | KVM, Hyper-V | VMX root |
| **EL3** | Secure Monitor | ATF (ARM Trusted Firmware) | SMM-ish |

Plus Secure / Non-Secure: the same EL number can be Secure (S-EL1 = TrustZone kernel, S-EL0 = TA = Trusted Applet) or Non-Secure. ARMv9 adds a Realm world for Confidential Compute.

Crossings: `SVC` → EL1, `HVC` → EL2, `SMC` → EL3. Every crossing is a security boundary; bugs there are CVEs.

### 3. MMU — same idea, different names

| Concept | x86 name | ARM name |
|---|---|---|
| Page table base | `CR3` | `TTBR0_EL1` (user) / `TTBR1_EL1` (kernel) |
| ASID tagging (no flush on context switch) | PCID | ASID |
| Page-level cache attributes | PWT / PCD bits | MAIR_EL1 + page-table AttrIndex |
| Translation control | various MSRs | TCR_EL1 |

ARM has **two TTBRs** so kernel and user halves of the address space have separate page-table roots — no top-half/bottom-half hack. Granule is selectable: 4 KB / 16 KB / 64 KB.

### 4. Memory model — weak and proud

ARM is **weakly ordered**. Barriers you actually see:

| Instruction | Meaning |
|---|---|
| `DMB ISH` | Data Memory Barrier, inner shareable — orders memory ops within inner-shareable domain |
| `DSB SY` | Data Sync Barrier, full system — drains all outstanding ops before continuing |
| `ISB` | Instruction Sync Barrier — flush pipeline; required after writing system registers |
| `LDAR` / `STLR` | Load-acquire / store-release — one-shot ordering, cheaper than full DMB |

The Day 8 doorbell example *requires* `DMB ST` (or stronger) between descriptor write and doorbell write on ARM. This is the most common ARM driver bug and the one interviewers always probe.

### 5. GIC — Generic Interrupt Controller

ARM's APIC. Two pieces:

- **Distributor (GICD):** one per system, routes interrupts.
- **Redistributor + CPU interface (GICR/GICC):** one per core.

Interrupt classes:

| Range | Class | Use |
|---|---|---|
| 0-15 | **SGI** (Software-Generated) | IPIs |
| 16-31 | **PPI** (Private Peripheral) | per-CPU timer, watchdog |
| 32+ | **SPI** (Shared Peripheral) | most device interrupts |

GICv3+ adds **LPI** (Locality-specific Peripheral Interrupt) backed by **ITS** (Interrupt Translation Service) — ARM's MSI/MSI-X equivalent. PCIe endpoints route MSI through ITS → distributor → target CPU. Bring-up bug: ITS table not initialised → interrupts lost.

### 6. SMMU — System MMU (the IOMMU on ARM)

- **Stream ID** ≈ PCIe Requester ID.
- **Stage 1** = guest VA → IPA. **Stage 2** = IPA → PA (used in virtualised systems).
- **CMDQ** for invalidation, **EVTQ** for faults.

Snapdragon puts an SMMU in front of every DMA-capable IP — camera ISP, GPU, modem, NPU, USB, NVMe. Walking the EVTQ is the entry point for any SMMU fault triage.

### 7. big.LITTLE / DynamIQ

Snapdragon configurations like **1+5+2** (1 prime + 5 perf + 2 efficiency) sit in a **DynamIQ Shared Unit (DSU)**:

- All cores share L3, snoop filter, and ACE/CHI interconnect interface.
- Per-core DVFS in principle, but cores often **share a voltage rail** by cluster (cost: fewer PMIC LDOs). "Per-core DVFS" then really means per-core *frequency*, *cluster* voltage = max across cores on that rail.
- **EAS (Energy Aware Scheduling)** uses topology + capacity + idle-state cost to place threads.

### 8. Qualcomm power architecture — PSCI, RPMh, PMIC, TCS

**PSCI (Power State Coordination Interface):** the standard SMC API the OS uses to request CPU on/off, suspend, system reset. `PSCI_CPU_ON`, `PSCI_CPU_OFF`, `PSCI_SYSTEM_SUSPEND`, `PSCI_SYSTEM_RESET`. Implemented by ATF at EL3.

**Power domains:** per-core (collapsible at idle = WFI + retention), cluster, L3/DSU, AON. AON = always-on (RTC, wake sources, debug) — never powered down.

**RPMh (Resource Power Manager — hardened):** an arbiter in the AON domain. Snapdragon has many independent subsystems (Apps cluster, Modem, ADSP, CDSP/NPU, sensor LPASS) each running its own firmware; each can request a clock rate, voltage, or bus bandwidth at any time. RPMh:

- Takes "active", "sleep", and "wake" votes from every requester.
- Aggregates the **maximum** needed across all requesters per resource.
- Programs the PMIC, clock controller, and bus controller.

Without RPMh, subsystems would either over-deliver power (battery drain) or under-deliver (functional fail).

**TCS (Trigger Command Set):** pre-loaded command sequences in RPMh that fire on idle entry/exit so wake latency is microseconds, not milliseconds.

**PMIC (PM8xxx companion die):** separate chip with multiple LDOs/SMPS rails, talks to SoC over **SPMI** bus. Programmable per-rail voltage, sleep/active modes, OCP/OVP protection.

**Snapdragon sleep-state debug flow** (the analogue of the PkgC6 + ADR story coming on Days 13-14):

1. Did the OS request the right PSCI state?
2. Did RPMh aggregate votes correctly?
3. Did the right TCS fire?
4. Did the rail actually drop on PMIC telemetry?
5. On wake, did rails ramp before any IP issued a transaction?

### 9. Clocks, PLLs, glitch-free mux

A PLL multiplies a reference clock:

$$ f_{out} = f_{ref} \times \frac{N}{M \times P} $$

Knobs that bite in validation:

- **Lock time** (µs) — IP clocked before lock asserts → flops capture random data → IP comes out of reset in unknown state. Always poll `PLL_LOCK` before un-gating.
- **Jitter** — period, cycle-to-cycle, accumulated. High jitter on a SerDes refclk kills PCIe / LPDDR eye margin.
- **VCO range** — programming N outside range silently fails.
- **Bypass mode** — route refclk straight through; used during bring-up before PLL params are programmed.

**Glitch-free mux:** switching between two async clock sources requires a handshake (gate current source, wait for it quiet, switch select, gate enable of new source, open). Naive OR-gate mux produces runt pulses → metastable downstream. Use the certified standard-cell mux — never roll your own.

**Clock-domain crossing (CDC):** flops on one clock feeding flops on a different clock can metastable. Mitigations: 2-FF synchroniser (data), async FIFO (multi-bit), handshake (control). CDC bugs are the textbook *"passes simulation, fails silicon"* class.

**CDC techniques in detail:**

| Crossing | Standard solution | Why |
|---|---|---|
| 1-bit level / control | 2-FF synchroniser in destination clock | first flop may go metastable; second flop sees a stable value with very high MTBF |
| 1-bit pulse | toggle-synchroniser (toggle in src, edge-detect in dst) | a short src pulse may be missed by slower dst clock; toggle survives |
| Multi-bit data | **async FIFO with Gray-coded read/write pointers**, OR full **req/ack handshake** | multi-bit can sample mid-transition with multiple bits flipping → resync gives garbage |
| Reset | reset synchroniser (async assert, **sync de-assert**) | otherwise violates flop recovery/removal time |

**Why Gray code in async FIFOs:** binary-coded pointer increments can flip many bits per step (e.g., 0111 → 1000 flips 4 bits). If the destination clock samples mid-transition, each bit may resolve independently → reads a value that never existed → FIFO appears full when empty (or vice-versa). Gray code guarantees exactly **one bit changes per increment**, so worst-case the resync sees either the old or new pointer — both are valid. The pointer is converted Gray↔binary on each side for arithmetic.

**2-FF MTBF math (informal):** MTBF grows exponentially with the time available for metastability to resolve. One flop = barely enough at modern clock rates; two flops = years; three flops are added in safety-critical paths. Combinational logic between sync stages is illegal — the second flop would sample a glitching net.

**Power-domain crossings — isolation cells & retention flops:**

When one rail is power-gated and a neighbouring rail stays alive, every signal that crosses needs an **isolation cell** (clamps the output to a safe value — usually 0 or 1) so the always-on side doesn't see a floating input as the gated rail collapses. Sequencing matters: **assert isolation enable BEFORE power-down; de-assert AFTER power-up.** Wrong order = floating inputs propagate, downstream flops latch garbage.

**Retention flops** keep critical state across a power-down on a separate always-on supply ("balloon latch"). Used for the few bits that *must* survive C6/sleep so the wake sequence has somewhere to restore from. They cost area + leakage on the AON rail — used sparingly.

**Level shifters** sit on signals crossing two different (live) voltage rails — e.g., 0.6 V core ↔ 1.8 V IO. Wrong direction shifter = signal arrives too dim or rails out.

**Validation reflex:** a pre-Si CDC linter (Spyglass-CDC, Conformal-CDC) flags every crossing; the engineer signs off each waiver. A regression that only repros at a specific voltage / temperature / freq combo (not architectural) → suspect a CDC race, isolation-enable timing, or analog margin — not a logic bug.

### 10. Resets

- **POR** (power-on reset) — held until rails stable, released asynchronously.
- **Warm reset** — system-level, preserves AON, drops most domains.
- **Functional reset** — per-IP, software-driven via control register.

Best practice: assert async, **de-assert sync** to the destination clock, otherwise you violate recovery/removal time on the reset pin and the released flop is metastable.

Bring-up bug: IP held in reset by a sticky mask never cleared by FW → reads return all-0 or all-1 → engineer assumes IP is dead. Always check the local reset bit before deeper debug.

### 11. LPDDR5/5X subsystem

```
CPU ↔ LLC ↔ DDRC ↔ DDRPHY ↔ LPDDR5 die(s)
```

- 8533 MT/s (LPDDR5), 9600 MT/s (LPDDR5X).
- Two channels per die, 16-bit each → 32 bits per die.
- WCK runs at 4× CK; relationship trained at boot.
- Burst length 16 default, DBI for power.

**Five training phases at every cold boot:**

1. CA training — command/address eye centering.
2. Read DQ training — per-DQ delay align with strobe.
3. Write DQ training — same on write path.
4. ZQ calibration — termination resistance against reference resistor.
5. VREF training — receiver reference voltage centering.

Failure of any step = no DRAM = system hang. Validation engineers stare at training logs in their sleep. **Self-refresh** during deep sleep keeps content with sub-mW power; refresh rate doubles above 85 °C (DRAM's MR4 register tracks this).

DDRC perf counters worth knowing: BW per channel (read/write split), page hit/miss/conflict ratio, queue full cycles, refresh stall cycles.

### 12. Snapdragon boot chain

```
POR → PBL (ROM, EL3, validates SBL signature, loads to SRAM)
   → SBL/XBL (trains LPDDR — the long step; loads ATF + TZ + ABL into DRAM)
   → ATF (EL3, brings up secure services, PSCI ready)
   → TZ kernel (S-EL1, TrustZone services)
   → ABL (Android Boot Loader, fastboot, verifies boot.img)
   → Linux kernel start_kernel(), DT probe, init userspace
```

**Secure boot in one slide:** ROM holds an immutable public key (or hash) — the *root of trust*. Each stage verifies the next stage's signature before jumping. Anti-rollback fuse counters block loading older signed images with known vulnerabilities. TrustZone establishes secure-world isolation for keys / DRM / biometrics.

### 12a. Where PCIe sits in the boot flow (system-level anchor for Day 10)

PCIe doesn't appear at one point — it's touched in **four distinct phases**, and each phase has its own pre-Si failure class. This is the map every platform validator should be able to draw:

```
POR ─► PBL ─► SBL/XBL ─► ATF ─► ABL ─► Kernel ─► userspace
                │            │       │       │
                │            │       │       └── (D) hot-plug, FLR, runtime PM
                │            │       │
                │            │       └────────── (C) full enumeration, BAR assign,
                │            │                       driver bind, MSI-X, ASPM enable
                │            │
                │            └────────────────── (B) PHY/PLL bring-up, REFCLK,
                │                                    PERST# de-assert, LTSSM → L0,
                │                                    minimal CfgRd to confirm Vendor ID
                │
                └─────────────────────────────── (A) PCIe controller IP out of reset,
                                                     fuses/straps latched (Gen, lane map,
                                                     Vendor/Device ID, class code)
```

| Phase | Who runs it | What PCIe is doing | Pre-Si test vehicle | Pre-Si failure classes |
|---|---|---|---|---|
| **A — IP reset release** | PBL / SBL early | Controller out of reset, fuses & straps latched (max Gen, lane width, BDF roots, Class:Sub:ProgIF), REFCLK gated on | RTL unit-sim of PCIe IP + fuse/strap testbench | Wrong strap value → wrong Class code post-enum; reset de-asserted before REFCLK stable → LTSSM stuck Detect; SerDes calib not run before Polling |
| **B — PHY + LTSSM bring-up** | SBL/XBL (still pre-DRAM-OS) | PERST# released to EP, REFCLK on, LTSSM Detect → Polling → Config → L0; **first CfgRd reads Vendor ID** to confirm link | Full-die emulation (Zebu/Palladium) with EP BFM | LTSSM stuck Polling (refclk wrong / termination wrong); width downgrade in Config (lane map strap mis-routed); `Trhfa` violated → first CfgRd returns `0xFFFF` even though link is up; speed stuck at Gen1 (EQ phase 2/3 fails) |
| **C — Enumeration + BAR + bind** | UEFI / ABL or Linux kernel `pci_scan_root_bus()` | Depth-first walk: probe Vendor ID, read Header Type, recurse bridges, size BARs, write bridge Memory Base/Limit, walk capability list, enable Bus Mastering, allocate MSI-X vectors, enable ASPM if cap present | Virtual platform Linux + KVM + EP model; or emulation with a real `nvme.ko` driver | Bridge Memory Base/Limit too small → EP enumerates but BAR unreachable; Header Type bit[7] strap wrong → fn1+ invisible; Class:Sub:ProgIF wrong fuse → wrong driver binds; cap-list pointer corrupt → MSI-X never enumerated; CfgRd burst dominates boot time |
| **D — Runtime** | Linux kernel + driver | ASPM L0s/L1/L1.x entry/exit, FLR, hot-plug add/remove, AER service, runtime PM (D0↔D3hot↔D3cold), PkgC6 dependency on L1.2 | Virtual platform + fault-injection harness; very limited pre-Si; mostly a **post-Si** discovery surface | L1.2 not advertised → PkgC6 never enters; AER UR storm during enumeration of unimplemented function; FLR doesn't restore BAR → driver re-bind fails; surprise removal during DMA → IOMMU fault on stale ATC entry |

**Pre-Si vs post-Si gap by phase (the line that lands):**
- **A and B** — almost entirely catchable pre-Si (RTL + emulation). If silicon stalls in Detect/Polling, that's a *coverage hole*, not a "post-Si finds it" success.
- **C** — partly pre-Si (virtual-platform Linux can run real PCI core code), but bridge-window math and multi-function corner cases are first found on EVM with real OS + real EP IP.
- **D** — fundamentally post-Si. ASPM exit timing, real PERST# de-bounce, real PMIC droop, real EP firmware races. Pre-Si can model the *FSM*, never the *analog timing*.

**Boot-time PCIe debug cheat:** if the OS log shows no PCIe devices at all, you're in phase B (link never came up). If devices enumerate but the driver doesn't load, you're in phase C (BAR/bridge/class code). If the device works at boot but breaks at idle, you're in phase D (ASPM / runtime PM). This three-way split is how I'd structure first-pass triage on a new SoC.

---

## Bugs in this Topic

- **ARM driver works on x86, hangs on Snapdragon (DMB missing):** weak memory reorders the doorbell ahead of the descriptor; device DMA-reads stale data. Fix: `DMB ST` (or `DSB SY`) between them, or `STLR` for the doorbell.
- **SMMU fault on first NVMe submission (Stream ID not registered):** SMMU EVTQ shows BDF-mapped Stream ID with "no L1". Fix: register endpoint Stream ID in SMMU stream table before probing.
- **Sleep-state regression — rail never drops in S0ix-equivalent:** PSCI request fired; RPMh aggregated correctly; TCS misprogrammed → wrong rail voltage written. Fix: validate TCS contents against expected resource votes.
- **Cold boot fails at -20 °C, works at room temp:** LPDDR retrains differently across PVT; per-bit DQ window collapses at the cold corner. Mitigation: per-corner characterisation, store trained values in NV-RAM for warm boot, run vendor's MR-based retraining.
- **PLL_LOCK polling missing in firmware:** 99% of boots work, 1% (slow PVT corner) hangs because IP came up before lock asserted. Fix: poll `PLL_LOCK` before un-gating.
- **Warm-reset hang:** trained DDR timings clobbered by warm reset and not restored; PHY loses calibration without a re-train. Triage: dump DDRC/PHY registers pre/post warm reset.
- **CDC bug — passes sim, fails silicon at 1-in-10⁶ cycles:** missing 2-FF synchroniser on a control bit crossing clock domains; metastable resolution propagates wrong state. Fix: synchroniser chain, then re-validate corner timings.

---

## Interview Explanation (60-90 sec)

> "On Snapdragon everything I know from x86 has an ARM name. Ring 0 becomes EL1; ring 3 becomes EL0; the hypervisor sits at EL2; the Secure Monitor at EL3 with TrustZone running S-EL1/0 alongside it. CR3 becomes TTBR0/1_EL1 with ASIDs instead of PCIDs and 4 KB or 16 KB or 64 KB granules selectable in TCR_EL1. Memory ordering is weak, so the descriptor-then-doorbell pattern from PCIe needs `DMB ST` between the two stores — that single missing barrier is the most common port-from-x86 bug. The IOMMU is the SMMU, keyed by Stream ID instead of Requester ID. Interrupts route through the GIC: SGIs for IPIs, PPIs for per-CPU timers, SPIs for devices, and PCIe MSIs come through the ITS. Power: the OS uses PSCI SMCs to request states; RPMh in always-on aggregates the votes from Apps, Modem, ADSP, CDSP and programs the PMIC over SPMI; TCS sequences fire on idle entry/exit for fast wake. Boot is PBL → SBL → ATF → ABL → kernel, with LPDDR5 training the long step in SBL and a signature check at every handoff."

---

## Architect Drills

*ARM and Qualcomm's design choices reflect mobile-first power, multi-firmware aggregation, and weak-memory bets. Derive why each landed there — and what would force a redesign. No answer key.*

**D1. Why does ARM expose four exception levels (EL0/1/2/3) when x86 collapsed to two practical rings?**
*Constraints to weigh:* mobile-first hardware-virtualisation story, TrustZone as a peer to the hypervisor, signed-firmware boot chain, attack-surface partitioning. What would x86 have to add to match — and what does the lack of an EL2-equivalent cost in cloud workloads?

**D2. Why does Snapdragon need RPMh on top of standard PSCI when x86 has PCU alone?**
*Constraints to weigh:* multiple independent firmwares (Apps/Modem/ADSP/CDSP) voting on shared rails, no single OS to arbitrate, always-on aggregation point, PMIC over SPMI. What does Intel's PCU do that aggregates implicitly that RPMh has to do explicitly — and why?

**D3. ARM is weakly ordered, x86 is TSO. If ARM moved to TSO tomorrow, derive who wins and who loses.**
*Constraints to weigh:* store-buffer drain energy, retire complexity, snoop traffic on every store-to-shared-line, mobile/server power budgets, software-portability tax (every weak-→-strong port today is a kernel bug). Why won't Apple/ARM make the move even at parity perf?

**D4. PSCI is a spec for OS↔firmware power calls. What would a SoC do without PSCI — what does the spec actually save?**
*Constraints to weigh:* per-vendor SMC ABIs, kernel-side per-SoC code, secure-monitor entry semantics, multi-cluster CPU online/offline. What does Linux's `arm,psci-0.2` DT binding paper over, and where does it leak?

**D5. Snapdragon boots PBL → SBL → ATF → ABL → kernel, with a signature check at every handoff. Reason about why so many stages.**
*Constraints to weigh:* ROM-immutable trust anchor, RAM unavailable until LPDDR train completes (SBL's job), separation of TZ from non-TZ, fastboot/recovery flexibility, A/B slot semantics. What collapses if you merged SBL+ATF — and what attack surface would that open?

**D6. Per-core voltage on DynamIQ is mostly a lie because cores share a rail. What architectural choice would make it real, and at what cost?**
*Constraints to weigh:* per-core LDO area + efficiency, package power-delivery network, per-core PMIC channels, validation matrix explosion. Apple's M-series and Intel's per-core FIVR took different bets — which one is "right" for what workload?

**D7. The GIC routes SGI/PPI/SPI/LPI; the ITS handles MSI. Reason about why MSI was retrofitted via the ITS rather than as a fifth native class.**
*Constraints to weigh:* PCIe-MSI is endpoint-driven and OS-programmable, GIC SPI count is fixed at design time, MSI requires a translator (memory-write → interrupt), backwards compatibility with GICv2. What does GICv4 direct-injection buy for KVM, and what does it cost?

---

## Coding A — Systems C: PSCI dispatcher + RPMh vote aggregator

```c
#include <stdio.h>
#include <stdint.h>

#define PSCI_CPU_ON         0x84000003
#define PSCI_CPU_OFF        0x84000002
#define PSCI_SYSTEM_SUSPEND 0xC400000E
#define PSCI_SYSTEM_RESET   0x84000009

#define MAX_CPUS 8
typedef enum { CPU_OFF, CPU_ON } cpu_state_t;
static cpu_state_t cpus[MAX_CPUS] = { CPU_ON, [1 ... MAX_CPUS-1] = CPU_OFF };

static int psci_call(uint32_t fid, uint64_t a1, uint64_t a2) {
    switch (fid) {
    case PSCI_CPU_ON:
        if (a1 >= MAX_CPUS) return -1;
        if (cpus[a1] == CPU_ON) return -2;
        cpus[a1] = CPU_ON;
        printf("PSCI: CPU %llu ON, entry 0x%llx\n",
               (unsigned long long)a1, (unsigned long long)a2);
        return 0;
    case PSCI_CPU_OFF:        printf("PSCI: caller CPU OFF\n"); return 0;
    case PSCI_SYSTEM_SUSPEND: printf("PSCI: SUSPEND, resume @0x%llx\n",
                                     (unsigned long long)a1); return 0;
    case PSCI_SYSTEM_RESET:   printf("PSCI: SYSTEM_RESET\n"); return 0;
    default: return -3;
    }
}

#define N_REQ 4
typedef struct { uint32_t freq_khz; uint32_t volt_mv; } vote_t;
static vote_t reqs[N_REQ];

static vote_t rpmh_aggregate(void) {
    vote_t agg = { 0, 0 };
    for (int i = 0; i < N_REQ; i++) {
        if (reqs[i].freq_khz > agg.freq_khz) agg.freq_khz = reqs[i].freq_khz;
        if (reqs[i].volt_mv  > agg.volt_mv ) agg.volt_mv  = reqs[i].volt_mv;
    }
    return agg;
}

int main(void) {
    psci_call(PSCI_CPU_ON, 1, 0x80000000);
    psci_call(PSCI_CPU_ON, 1, 0);  /* already on */
    reqs[0] = (vote_t){ 1200000, 800 }; /* Apps */
    reqs[1] = (vote_t){  600000, 700 }; /* Modem */
    reqs[2] = (vote_t){ 1500000, 850 }; /* CDSP */
    reqs[3] = (vote_t){ 0, 0 };
    vote_t a = rpmh_aggregate();
    printf("RPMh aggregate: %u kHz @ %u mV\n", a.freq_khz, a.volt_mv);
    psci_call(PSCI_SYSTEM_SUSPEND, 0x80001000, 0);
    return 0;
}
```

## Coding B — Interview DS&A: Valid Parentheses (LC 20)

Stack walk — same shape as walking nested capability chains in PCIe config space (Day 10).

```python
def isValid(s):
    pairs = {')':'(', ']':'[', '}':'{'}
    st = []
    for c in s:
        if c in '([{': st.append(c)
        elif not st or st.pop() != pairs[c]: return False
    return not st
```

---

---

## Interview Reasoning Pack — System Boot Flow (HW + SW)

This is the question you must be able to walk for **90 seconds without notes**. Two parallel narratives — hardware sequencing and software stages — happening on the same wall clock.

### The vendor-agnostic mental model (memorize this first)

Every modern computer — Intel laptop, ARM phone, RISC-V microcontroller — boots in the **same five conceptual phases**. Only the names differ. Lock these phases in your head and you can answer the question for *any* platform.

```
┌─────────────────────────────────────────────────────────────────────┐
│ Phase 0: POWER         PMIC ramps rails in order, releases reset    │
│ Phase 1: ROM           Tiny immutable code in mask ROM picks medium │
│                        loads next stage to on-die SRAM, verifies it │
│ Phase 2: PLATFORM INIT Train DRAM, init clocks, set up MMU/caches   │
│                        (now we have GBs of memory to work with)     │
│ Phase 3: BOOTLOADER    Find OS image on storage, verify it, load it │
│                        into DRAM, hand off                          │
│ Phase 4: OS + INIT     Kernel takes over, brings up other cores,    │
│                        mounts rootfs, starts userspace              │
└─────────────────────────────────────────────────────────────────────┘
```

**The invariant across vendors:** each phase loads + *signature-verifies* the next phase before jumping. That signature chain, rooted in a key hash burned into one-time fuses, is **secure boot / chain of trust**. Same idea on Intel BootGuard, AMD PSP, ARM TrustZone, Apple Secure Enclave, NXP HABv4 — different names, same pattern.

### Hardware perspective: from "button press" to "ROM running"

The CPU cannot fetch its first instruction until many things have happened in a strict order. **Power and clocks before logic; logic before memory; memory before software.**

1. **Button / wake event** → asserts `PWR_ON` to the **PMIC** (Power Management IC, a separate chip on the board, e.g. Qualcomm PM8550). The SoC core rails are still off.
2. **PMIC rail sequencing.** PMIC brings up rails in a programmed order: always-on (`VDD_AON`) first → I/O (`VDDIO_1V8`) → core (`VDD_CX`) → memory (`VDD_MX`) → DDR PHY (`VDDQ`). Each rail has a **ramp time** and a **good** signal; the next rail can't start until the previous is in regulation. **Wrong sequencing = latch-up or stuck reset.** This is why PMIC firmware is part of the validation matrix.
3. **Reference clock.** A 38.4 MHz crystal (XO) starts oscillating; a small **always-on PLL** locks. The main PLLs are still in bypass.
4. **Reset deassert.** Once rails are good and XO is stable, the PMIC releases the SoC's `PORESETn`. The reset deassertion is **synchronous** to the reference clock (async assert, sync deassert — see Day 9 "Resets") so all flops come out of reset cleanly.
5. **Boot core selection.** A small piece of always-on logic decides which core boots first (typically a single A55/A78 in big.LITTLE, or a dedicated security core). On Snapdragon this is often a small **Cortex-M-class boot core** that runs PBL.
6. **Fuse read.** The boot core reads **eFuses / OTP** (one-time programmable bits): chip ID, root-of-trust public key hash, anti-rollback counters, debug-disable bits, SKU/binning. Fuses gate everything that follows — a "secure boot" fuse forces signature verification; an "anti-rollback" fuse rejects older firmware versions.
7. **Boot ROM execution.** PC jumps to a **mask ROM** (hardwired at fab — cannot be patched). ROM is small (tens of KB), runs from on-die SRAM, and does the minimum: configure boot pins, find the next stage on the boot medium (eMMC / UFS / SPI-NOR — selected by strap pins), DMA it into SRAM, **verify its signature against the fuse-locked root key**, and jump.
8. **DRAM is still off.** Everything so far runs from on-die SRAM. DRAM training has not happened yet — that's the next stage's job.

### Software perspective: the boot chain (generic, then per vendor)

The **generic** chain — what every system does, with role descriptions instead of vendor names:

| # | Generic role | Runs from | What it actually does |
|---|---|---|---|
| 1 | **Boot ROM** | Mask ROM → on-die SRAM | Tiny (~tens of KB), immutable. Picks boot medium from strap pins, reads first sector, signature-verifies it against fuse-locked root key, jumps. |
| 2 | **First-stage bootloader (FSBL)** | SRAM | Has just enough to bring DRAM up: configures PLLs, sequences remaining PMIC rails, **trains DDR**, initializes the memory controller. Then loads the next stage *into DRAM*. |
| 3 | **Second-stage / platform firmware** | DRAM | Sets up platform services: ACPI/Device Tree describing the hardware to the OS, secure-world runtime, power-management firmware (handles future C-state / DVFS requests from the OS). |
| 4 | **OS bootloader** | DRAM | Reads the boot partition off storage (eMMC/NVMe/SATA), verifies the OS image, decompresses the kernel, builds the boot args, jumps to kernel entry. |
| 5 | **OS kernel** | DRAM | Discovers hardware (via DT/ACPI), brings up secondary cores, initializes drivers, mounts root filesystem, hands off to **init / systemd / launchd** which starts userspace. |

**The chain of trust:** stage *N* signature-verifies stage *N+1* against a public key whose hash is locked in eFuses. Break any link — unsigned stage, skipped check — and secure boot collapses. Every vendor implements the same idea (Intel BootGuard, AMD PSP, Apple Secure Boot, ARM TF-A trusted boot, NXP HAB) under different brand names.

#### Vendor mapping (so you can speak any of them)

| Generic role | **x86 PC (Intel/AMD)** | **ARM server / TF-A** | **Snapdragon (QC)** | **Apple Silicon** |
|---|---|---|---|---|
| Boot ROM | Microcode + Intel ME / AMD PSP ROM | BL1 | PBL | iBoot stage 0 (SecureROM) |
| FSBL (DDR train + early init) | FSP (Firmware Support Package) inside UEFI PEI phase | BL2 | SBL / XBL | LLB (Low-Level Bootloader) |
| Platform firmware / secure monitor | UEFI DXE + SMM handlers | BL31 (runtime) + BL32 (TEE) | TZ / QHEE + QSEE | iBoot main + SEPOS |
| OS bootloader | UEFI BDS → GRUB / Windows Boot Manager | U-Boot / GRUB / UEFI | ABL (LK-based) or UEFI | iBoot loads kernelcache directly |
| Kernel + init | Linux + systemd / NT kernel + smss | Linux + systemd | Linux + Android init | XNU + launchd |

**Privilege levels** (where each stage runs):
- **x86:** SMM (most privileged) → Ring 0 (kernel) → Ring 3 (user). Microcode and ME run *below* Ring 0 in their own enclave.
- **ARM:** EL3 (secure monitor / BL31) → EL2 (hypervisor) → EL1 (kernel) → EL0 (user). Secure world (S-EL1, S-EL3) is parallel.

**Why x86 and ARM look so different at first glance:** x86 inherited a 40-year UEFI/BIOS legacy with phase names (SEC → PEI → DXE → BDS) that hide the same five generic roles. ARM started fresh with TF-A and named the stages literally (BL1, BL2, BL31, BL33). They're doing the same things; only the spelling changes.

### The two narratives meeting at DDR training

The pivotal moment in boot is **DDR training**, which happens inside the FSBL (BL2 / FSP / LLB / SBL):

1. PHY calibration (ZQ calibration of on-die termination).
2. CA (command/address) training.
3. Read DQS gate training (find when the DRAM's DQS strobe arrives).
4. Read/write level training (per-byte-lane delay).
5. VREF training (data-eye voltage center).

Until this completes, the SoC has **no general-purpose memory** — it lives in tens of KB of SRAM. After it completes, all subsequent stages (BL31, BL33, kernel) run from DRAM. **A bad LPDDR training tuning is a "boot hangs in SBL" symptom** — exactly the kind of bug a pre-silicon CPU/platform validator chases.

### How to deliver this in an interview (90 seconds)

> "Boot is two parallel stories happening on the same wall clock. **Hardware:** when power comes in, a separate **PMIC chip** ramps the rails in a programmed sequence — always-on, then I/O, then core, then DRAM PHY — each waiting for the previous to be in regulation. Once a reference crystal is stable, the PMIC releases the SoC's power-on reset. The boot core wakes up running from a tiny **mask ROM** — immutable code burned at the foundry — which reads **eFuses** for the root-of-trust key hash, anti-rollback counters, and SKU bits. DRAM is still dark; everything so far runs from tens of KB of on-die SRAM.
>
> **Software:** the ROM loads the **first-stage bootloader** into SRAM, signature-verifies it against the fuse-locked key, and jumps. The first-stage's main job is **DDR training** — five PHY calibration phases that find the right delays and voltages so the memory controller can talk to the DRAM. Once DRAM is alive, it loads the **platform firmware** — secure monitor, power-management firmware — into a protected DRAM region, then loads the **OS bootloader** into normal-world DRAM. Each stage verifies the next; that's the chain of trust.
>
> The OS bootloader finds the kernel image on storage, verifies it, decompresses it, and jumps to kernel entry. The kernel discovers hardware via **device tree or ACPI**, brings up the rest of the cores via firmware calls (PSCI on ARM, INIT-SIPI on x86), initializes drivers, mounts the root filesystem, and hands off to **init** which starts userspace.
>
> The names change per vendor — Intel calls the first stages FSP and UEFI PEI; ARM TF-A calls them BL1, BL2, BL31; Qualcomm calls them PBL, SBL, TZ — but the five roles are identical: **ROM → DDR-training stage → platform firmware → OS loader → kernel + init**, with a signature check at every hop and fuses pinning the root."

### Validation hooks at every stage

This is what makes the answer feel like a *validator's* answer, not a Wikipedia answer:

- **PMIC sequencing:** scope the rails, verify ramp times and order match the PMIC firmware spec. Bug shape: "board boots cold but not warm" → reset of one rail not honored.
- **Reset:** check `PORESETn` is sync-deasserted; async deassert causes random flop init → "boots 9/10 times."
- **Fuses:** boundary scan / JTAG to read fuse shadow; mismatch with expected SKU = wrong binary loaded.
- **ROM → SBL handoff:** UART log from PBL ("PBL: signature OK, jumping to SBL @ 0x...").
- **DDR training:** training logs from SBL, eye margins per byte lane, retry counts. **Tail-bit failures here are silent corruption later.**
- **BL31 PSCI:** test `CPU_ON` / `CPU_OFF` / `SYSTEM_SUSPEND` from kernel — these are the same SMC paths used by Day 13 C-states.
- **AVB / dm-verity:** flip a bit in `boot.img`, confirm BL33 refuses to boot.

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **5-phase boot** — *what:* POWER → ROM → PLATFORM_INIT → BOOTLOADER → OS+INIT / *why:* trust chain + bring up resources progressively / *how:* each stage authenticates and loads the next / *connects:* x86/UEFI vs ARM TF-A vs Snapdragon SBL — same model, different names
- **power-on reset** — *what:* PMIC sequences rails / *why:* HW must be stable before logic toggles / *how:* PMIC FSM + reset-pin deassert / *connects:* PVT corners, PLL lock time, brown-out detect
- **boot ROM** — *what:* masked, on-die, immutable / *why:* root of trust / *how:* authenticates next stage with key in fuses / *connects:* secure boot, eFuse OTP, why ROM bugs are silicon respins
- **bootloader** — *what:* DRAM init, load kernel / *why:* ROM is too small for full bring-up / *how:* trains DDR, sets up MMU, parses DT/ACPI / *connects:* device tree (ARM) vs ACPI (x86), PSCI handoff
- **secure boot chain** — *what:* each stage verifies signature of next / *why:* prevent persistent rootkits / *how:* public key in fuses, hash chain / *connects:* TPM, measured boot, TF-A BL1→BL2→BL31→BL33

## Bug Anchor

- **Day 8 (DMA/doorbells):** the `DMB ST` story lives here.
- **Day 13 (C-states / S0ix):** Snapdragon sleep-state flow is the analogue of PkgC6 + ADR.
- **Day 12 (DVFS):** RPMh + PMIC is the QC name for the Day 12 P-state story on Intel.
- **Day 18-19 (NPU):** Hexagon HTP + CDSP voting fits inside RPMh.

## Exit Check

- [ ] Mapped EL0/1/2/3 (Secure / Non-Secure) to ring 0/3, KVM, ATF, TrustZone.
- [ ] Stated `DMB ISH` / `DSB SY` / `ISB` / `LDAR/STLR` distinctions.
- [ ] Walked GIC SGI/PPI/SPI ranges and ITS for MSI.
- [ ] Stated SMMU Stream ID = Requester ID and walked an EVTQ fault.
- [ ] One-paragraph explanation of RPMh + PSCI + PMIC + TCS.
- [ ] Listed the 5 LPDDR5 training phases.
- [ ] Walked PBL → SBL → ATF → ABL → kernel in 90 seconds.
- [ ] Stated why glitch-free mux can't be an OR gate.

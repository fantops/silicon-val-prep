# Day 09 — ARM / Qualcomm Platform Vocabulary

> **Source merge:** [30day_plan/day06b_arm_qualcomm_power.md](../30day_plan/day06b_arm_qualcomm_power.md), [30day_plan/day19b_clocks_lpddr_boot.md](../30day_plan/day19b_clocks_lpddr_boot.md), [30day_plan/QUALCOMM_CHEATSHEET.md](../30day_plan/QUALCOMM_CHEATSHEET.md). 30-day files are unchanged.

## Concept

Qualcomm SoCs are ARM. Most of what you learned in Days 1-8 was named in x86 vocabulary (rings, APIC, VT-d, MTRR). Today translates that vocabulary to AArch64 (EL0/1/2/3, GIC, SMMU, MAIR), then layers Qualcomm-specific power infrastructure (PSCI, RPMh, PMIC, TCS) and SoC bring-up vocabulary (PLL lock, glitch-free mux, LPDDR training, PBL → SBL → ATF → ABL → kernel).

## Why this day in the flow

Days 1-8 are platform-agnostic memory/bus theory. Days 10-13 dive into PCIe, NVMe, DVFS and C-states — material interviewers will probe through ARM/Qualcomm vocabulary if you target QCT. Day 9 inserts the translation table now so every later answer naturally uses the right names. Without today, you will say "ring 0" in a Snapdragon interview.

In the 30-day Layer map this is the **Layer 1 → Layer 3 ARM bridge**.

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

## Practice Q&A

**Q1. Linux kernel + KVM + a TrustZone TA + an Android app — which EL is each?**
App: NS-EL0. Kernel: NS-EL1. KVM: NS-EL2. ATF / Secure Monitor: EL3. TA: S-EL0 on top of QSEE/Trusty kernel at S-EL1.

**Q2. Linux runs unchanged across x86 and ARM. How?**
Architecture-abstracted barrier macros: `smp_mb`, `smp_rmb`, `smp_wmb`, `READ_ONCE/WRITE_ONCE`, `atomic_*`. On x86 most expand to compiler barriers (TSO does the rest); on ARM they expand to `DMB ISH`, `LDAR`, `STLR`. Hand-rolled accessors are x86-only bugs.

**Q3. SMMU fault during NVMe bring-up — top 3 suspects.**
(1) Stream ID not registered in SMMU stream table. (2) IOVA in SQ/CQ base never DMA-mapped through the SMMU API. (3) Stage-2 enabled but no S2 page tables installed. Read EVTQ — fault syndrome decides.

**Q4. Why does Qualcomm need RPMh on top of standard PSCI?**
PSCI tells one OS instance how to request CPU power states. Snapdragon has many subsystems running independent firmwares, each able to vote on shared rails/clocks/buses at any time. RPMh aggregates the max needed across them and programs the PMIC. Without it, subsystems would fight over shared rails.

**Q5. Per-core DVFS on DynamIQ — what's the catch?**
Cores often share a voltage rail by cluster. Per-core *frequency* is real; per-core *voltage* is not — rail voltage = max across cores on that rail. Validation: characterise voltage = `max(per-core requests)` and find waste.

**Q6. Glitch-free mux — why doesn't an OR gate work?**
ORing two async clocks produces runt pulses near transitions; downstream flops see partial edges → metastable. The right structure is a state machine that gates the current source, waits for it low, switches select, gates the new source's enable, then opens it.

**Q7. Walk Snapdragon boot from POR to Linux init.**
POR releases → PBL (ROM, EL3) validates SBL, loads to SRAM, jumps. SBL trains LPDDR, loads ATF + TZ + ABL into DRAM. ATF brings up secure services, jumps to ABL. ABL fastboot, verifies boot.img, loads kernel + DT + ramdisk. Kernel `start_kernel()`, probes via DT, mounts root, exec init. Each handoff is a signature check.

**Q8. Cold boot hangs only at -20 °C. Path?**
LPDDR per-bit DQ window collapses at slow PVT corner; trained timings from room temp don't fit. Re-train at temperature; or store per-corner trained values in fuses; or use vendor's MR-based retraining.

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

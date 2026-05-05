# PCIe + NVMe SoC — Test Plan (Pre-Si vs Post-Si) + One Hard HW Bug

> **Scope:** custom SoC, PCIe Gen4 x4 root complex, attached NVMe Gen4 SSD (DRAM-less, HMB).
> **Lens:** post-silicon platform validation. Focus is on **what changes between pre-Si and post-Si**, with **power / performance / thermal** as first-class axes.

---

## 1. Pre-Si vs Post-Si — the only table that matters

| Axis | Pre-Si (RTL / emulation / virtual platform) | Post-Si (A0 on EVM) | Why post-Si owns the gap |
|---|---|---|---|
| **Functional** | UVM directed+random TLP, NVMe FSM coverage, virtual-platform Linux + short `fio` | `lspci`, `nvme-cli`, AER inject, FLR, hot-plug, OS races | Real driver/OS races, firmware ordering |
| **Conformance** | PCIe-SIG / NVMe compliance vectors against RTL | UNH-IOL plug-fest vs silicon; SSD interop (5+ vendors) | Tooling vendors only certify silicon |
| **Performance** | Closed-form Little's-law model + UVM TLP/sec counters | IOPS/QD sweep, BW vs blk-size, latency CDF p50/p99/p99.99, **sustained-state perf after SLC fill** | Pre-Si has no NAND, no FTL, no GC cliff |
| **Power — link** | Directed L0s / L1.x entry-exit; modelled PLL lock | ASPM under load × 24 h; per-rail current; PMIC droop | Real REFCLK gating + analog PLL only on Si |
| **Power — Sx** | Virtual-platform `pm-suspend` smoke | S0ix / S3 / S4 × 1000 cycles with NVMe present; surprise removal | Suspend/resume × FTL journal races only on Si |
| **Power — NVMe APST (PS0..PS4)** | Modelled C transitions | Live PS transitions under traffic; idle/exit latency vs spec | Spec-vs-silicon timing only on bench |
| **Thermal** | **None at PHY/analog level** — RTL is isothermal | -40 / +25 / +85 / +105 °C chamber; 72 h soak; **WCTEMP / CCTEMP** crossings; **HCTM** negotiation; throttle entry/exit + data-path correctness under throttle | RTL has no temperature — **biggest single reason post-Si exists** |
| **Data integrity** | UVM E2E PI; ECRC/LCRC | `fio --verify=crc32c` 24 h across temp; power-loss × 10 k cycles | Long-tail corruption is time × temperature |
| **Reliability / endurance** | n/a | Fill + overwrite to surface FTL; SMART deltas; bad-block inject | NAND wear can't be modelled pre-Si |
| **IOMMU / SMMU** | Stream-table walks; PRI/ATS UVM | TLB-pressure stress; PMU TLB-miss telemetry; fault inject | Real DDR + NoC arbitration changes miss profile |

### Pre-Si exit gates
- LTSSM 100%, NVMe controller FSM 100%, AER fault classes covered
- IOMMU walk verified ≥ 3 Stream IDs
- Linux + nvme.ko + 30-min `fio` on virtual platform
- Closed-form perf model within ±10% of spec floor

### Post-Si exit gates (PRQ)
- Functional + Conformance: 100% pass at all 4 temp corners
- **Perf:** within ±10% of pre-Si model AND ≥ spec floor at sustained state (after SLC fill)
- **Power:** S0ix idle current in spec; ASPM L1.2 exit < 200 µs; APST exit in spec; 1000× S3/S4 zero data loss
- **Thermal:** 72 h @ +85 °C zero corruption, zero panic, throttle clean, HCTM correct
- 0 P0/P1 escapes; ≤ 3 P2 with documented workaround

---

## 2. Post-Si phasing (one line each)

| # | Phase | What runs |
|---|---|---|
| 0 | Smoke | Rails up, ROM→Linux, `lspci` enumerates RP, `/dev/nvme0n1` appears, 1-block dd |
| 1 | Functional / conformance | AER inject, FLR, hot-plug, MSI-X, NVMe Admin/IO, DSM, format |
| 2 | **Performance** | IOPS/QD 1→1024, BW vs blk-size, latency CDF, **sustained-state after SLC cache fills** |
| 3 | **Power** | ASPM L0s/L1/L1.1/L1.2; APST PS0..PS4; S0ix / S3 / S4 × 1000; surprise removal |
| 4 | **Thermal / soak** | -40/+25/+85/+105 °C × 72 h; `fio --verify`; throttle entry/exit + HCTM |
| 5 | Interop | 5+ SSD vendors × 2+ host OSes × 3+ link speeds |

---

## 3. The HW bug — silent NVMe read corruption under thermal throttle

### 3.1 SSD architecture refresher (one picture)

```
   HOST                  NVMe controller                 NAND
                                                        ┌────────────┐
   SQ/CQ ─MMIO DB►   ── front-end (SQE fetch, MSI-X)    │ 3D TLC,    │
   HMB region◄──DMA── ── FTL: L2P (in HMB), SLC cache,  │ 16 dies,   │
                         fold (SLC→TLC), GC, wear-level │ 4 planes   │
                      ── Thermal: Composite Temp,       └────────────┘
                         throttle FSM, HCTM
```

**Three things matter for this bug:**
1. **L2P** (LBA → physical) — DRAM-less SSD keeps active L2P in **HMB** (host RAM, accessed by controller via DMA).
2. **SLC write cache** — host writes land at SLC speed; CQE returned; background **fold** later moves them to TLC and updates L2P.
3. **Composite Temperature ≥ WCTEMP** triggers controller's internal throttle FSM.

### 3.2 Symptom

```
fio --rw=randrw --rwmixread=70 --bs=4k --iodepth=64 --verify=crc32c --runtime=72h
   ambient 85 °C, fan off
   → ~1 verify failure per 6–9 h
   → wrong data is ANOTHER LBA's content (not random garbage)
   → NVMe completion SC = 0 (Successful)   ← silent
   → PCIe AER clean; link Gen4 x4 the whole time
   → Composite Temp 84 °C > WCTEMP 82 °C; throttle level 1 active
   → vendor counter HMB_L2P_WRITEBACK_DEFERRED rising
   → never repros at 25 °C; never repros pure-read or pure-write
   → never repros on DRAM-equipped SSD (vendor Y) — only HMB SSD (vendor X)
```

### 3.3 Root cause (in the data path, not the link)

Above WCTEMP, throttle FSM **defers HMB L2P writebacks** to save controller power. But the SLC-reuse allocator clears its in-controller "SLC dirty bit" on **fold completion**, not on writeback completion:

```
   t0  write LBA L     → SLC P_slc; L2P[L]=P_slc queued to HMB; CQE done
   t1  fold P_slc → P_tlc completes; in-controller dirty[P_slc] = 0
   t2  THROTTLE engages → HMB writeback for L2P[L] = P_tlc deferred
   t3  SLC allocator reuses P_slc for a NEW write of LBA L'
   t4  host reads LBA L → L2P[L] from HMB still = P_slc
                       → DMA from P_slc → returns L' 's bytes
                       → CQE SC=0      ← SILENT P0 CORRUPTION
```

Two NVMe invariants violated:
- L2P update + SLC reuse must be **atomic w.r.t. subsequent reads**.
- Composite-Temperature throttle must affect **performance only**, never consistency.

### 3.4 Hypothesis ladder (how clues collapse)

| Hypothesis | Why ruled out |
|---|---|
| HMB DRAM bit-flip | Wrong-LBA data is coherent, not random |
| Host driver PRP confusion | AER clean; pattern is LBA-correlated, not host-buffer-correlated |
| FTL bug independent of temp | Doesn't repro at 25 °C |
| Throttle disables ECC scrub | SMART media-error counter = 0 |
| **Throttle defers HMB writeback while SLC dirty bit cleared on fold** | Matches every clue + vendor counter rising |

### 3.5 Fix and workaround

**SSD FW fix** — clear SLC-dirty bit only **after** L2P HMB writeback DMA actually completes; never defer writeback for a page eligible for reuse.

**Host workaround (no FW change)** — use `nvme set-feature 0x10` (HCTM) to lower thresholds so the host throttles before the SSD's internal defer kicks in. ~10–15% sustained perf cost.

**Pre-Si coverage gap (the line that lands the offer):** add a UVM test that **asserts thermal throttle during mixed RW** and checks read-after-write against a host shadow buffer; add SVA `assert always (SLC_dirty[P]=0 → L2P[*→P] has been written to HMB)`.

### 3.6 Why this bug class survived pre-Si

- RTL is isothermal — Composite Temp doesn't exist in simulation.
- Throttle was modelled as a perf reduction, never coupled to data-path correctness.
- The SLC-reuse-vs-L2P-writeback ordering was an **implicit firmware assumption**, not an SVA.
- Read-after-write in pre-Si used random LBAs; the bug needs LBAs that were **just folded**.

That cell — **thermal × time × data-path** — is exactly where post-Si owns the gap.

### 3.7 Bug report (recite-able)

```
SUMMARY: Silent NVMe read corruption on DRAM-less (HMB) SSD above WCTEMP
         during mixed RW. Read returns another LBA's data with SC=0.
SEVERITY: P0 (silent data corruption)
REPRO:    fio randrw 70/30, 4K, QD=64, --verify=crc32c, ambient 85 °C, 6–9 h
ROOT CAUSE: thermal throttle defers HMB L2P writeback while SLC-reuse
            clears in-controller dirty bit on fold completion → stale L2P
            → DMA from recycled SLC page holding another LBA's bytes
FIX: SSD FW — clear SLC-dirty only after L2P HMB writeback DMA completes
WORKAROUND: lower HCTM thresholds (~10-15% sustained perf cost)
COVERAGE GAP: pre-Si had no test coupling throttle FSM to data-path; added
              UVM 'throttle_during_mixed_rw_l2p_consistency' + SVA on
              SLC-reuse-vs-L2P-writeback ordering invariant
```

---

## 4. 60–90 s interview narration

1. *"Pre-Si covers UVM TLP / NVMe FSM, IOMMU walks, virtual-platform Linux + short fio, and a closed-form perf model. Power gets directed L0s/L1.x and PSCI tests; thermal is essentially uncovered pre-Si because RTL is isothermal."*
2. *"Post-Si is five phases: smoke, functional/conformance, performance with sustained-state after SLC fill, power (ASPM L1.2 + S0ix/S3/S4 + NVMe APST), and 72-hour thermal soak across -40 to +105 °C with `fio --verify` and SSD-vendor interop."*
3. *"The bug I'd walk through is silent read-data corruption on a DRAM-less NVMe SSD when Composite Temp crosses WCTEMP under mixed RW. Reads return another LBA's bytes with `SC=0` — no AER, no NVMe error. Root cause: thermal throttle defers HMB L2P writebacks while SLC-reuse clears its dirty bit on fold completion, so a subsequent read follows stale L2P into a recycled SLC page now holding another LBA. Fix: SSD FW must clear SLC-dirty only after the L2P writeback DMA completes; workaround is HCTM threshold tuning. Pre-Si gap: no test coupled the thermal-throttle FSM to the data-path L2P invariant — added UVM test + SVA."*

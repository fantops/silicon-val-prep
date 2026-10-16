# Foundation Plan — From Electrical Signal to a Working Accelerator (self-contained)

**Start:** Monday 19 Oct 2026 · **Core:** 6 weeks (to 29 Nov) · **Buffer + mock + re-evaluation:** 2 weeks (to 13 Dec) · **Then:** the 8-week CPU-PM depth block in [README.md](README.md#the-next-8-weeks-after-the-foundation).

This document is everything you need. It covers the big picture, the calendar, every session (what to read, what to answer, what to code, how it maps to your DSA work), the buffer weeks, a tracker, and a re-evaluation gate. Priority 0 is office work; this plan is Priority 1.

---

## 0. Before Monday (one-time setup, ~30 min, this weekend)

- [ ] **Get the coding exercises.** The local `coding/` folder is an empty submodule. In the repo root run `git submodule update --init` (or `git clone https://github.com/fantops/silica-val.git coding`). Check that `make sol-d00_my_memcpy` builds inside `coding/`. The prompts and reference solutions are in `coding/EXERCISES.md`.
- [ ] **Create a notes folder:** `prep_plan/notes/`, one file per session (`S01.md` … `S18.md`). Use public-level content only. **Anything Intel-internal from DSA debug goes in `dsa-val/_internal/`, never here.**
- [ ] **Block the calendar:** Mon/Wed/Fri 60 min (study), one weekend slot 45 min (coding). Treat these as appointments you can move, not tasks you can drop.
- [ ] **Print or pin §1 (the big picture).** You will place every session on it.

---

## 1. The big picture — where everything (and DSA) sits

Read this stack bottom-up. The plan climbs it from L1 to L9, then comes back down through power management.

```
 L9  Application / workload        app submits a copy/CRC job; NIC transmits the result
 L8  User-space runtime / tools    accel-config, user libraries, test apps (your dsa_test-type tools)
 L7  OS kernel + drivers           PCI core, idxd driver, IOMMU/SVA, intel_idle, intel_pstate, PM core
 L6  Firmware + tables             microcode, UEFI SEC/PEI/DXE, ACPI (_CST, CPPC, LPIT, DMAR), PCU firmware
 L5  Platform I/O + interconnect   PCIe root complex, RCiEPs, IOMMU, MSI-X, coherent fabric
 L4  SoC IPs                       cores, caches, memory controller, DSA (portals → WQs → groups → engines)
 L3  Clocks, reset, power          PLLs, reset sequencing, rails/power-good, clock/power gating, DVFS
 L2  Digital logic                 flops, timing, FSMs, clock-domain crossing
 L1  Electrical / devices          CMOS switching, logic levels, dynamic + leakage power
```

**Two paths cut through every layer. The capstone (S18) traces both:**

- **Data path (one DSA job):** L9 app → L8 builds a descriptor → **ENQCMD/MOVDIR64B** writes the portal (L5 MMIO in a BAR) → L4 work queue → engine → L5 IOMMU translates with PASID → coherent DMA to memory → completion record → MSI-X or polling → back to L9.
- **Power path (idle and wake):** L7 OS idle governor → L6 firmware policy → L3 gating and DVFS → package C-state / S0ix; devices vote through D-states, link L1.2 and LTR (L5) → a wake event reverses everything.

**Where sessions land on the stack:**

| Layer | Sessions |
|---|---|
| L1–L2 | S1, S2, S4 |
| L3 | S3, S5, S16 |
| L6 | S6 |
| L4 memory side | S7, S8, S9 |
| L5 PCIe | S10, S11, S12 |
| L5/L7 IOMMU + accelerator | S13, S14, S15 |
| L3–L7 power across the stack | S16, S17 |
| All layers, debug | S18 |

**Using your DSA work:** each session has a **"Work link"** line. It is a question you can answer by looking at the DSA issue you are already debugging. When you debug a hang, fault, or bad completion at work, ask: *which layer, and which session, explains this?* That is how the day job and this plan reinforce each other.

---

## 1b. What this plan covers, and where your experience fits

### Topics covered (18 sessions)

| Area | Sessions | Topics |
|---|---|---|
| Electrical + digital | S1–S2 | CMOS, logic levels, noise margin, dynamic + leakage power, latch/flop, setup/hold, STA slack, FSMs and hang states |
| Clocks, reset, CDC | S3–S4 | PLL lock, clock trees/skew, reset sequencing, metastability, synchronizers, pulse/handshake/async FIFO |
| Power-on + boot | S5–S6 | rails, power-good, fuses, PLTRST#/PERST#, microcode/FIT, cache-as-RAM, UEFI SEC/PEI/DXE, MRC, ACPI tables (C/P-states, S0ix, IOMMU), AP wake |
| SW ↔ HW | S7 | MMIO, config space, MSRs, `volatile`, read-modify-write, polling with timeout |
| Memory system | S8–S9 | DRAM/refresh/self-refresh, ECC, caches, AMAT, MESI, I/O coherence, TSO, fences, atomics |
| PCIe | S10–S12 | layers, LTSSM link-up and stuck states, CRS, enumeration, BARs, capabilities, MSI-X, driver probe, DMA, doorbells, ordering |
| Translation | S13 | page walk, TLB, IOMMU, DMA faults, PASID/SVA, ATS/PRI |
| Accelerator (DSA) | S14–S15 | RCiEP bring-up, WQ/group/engine, enable FSMs, ENQCMD/MOVDIR64B, descriptors, completion records, page faults mid-job |
| Power management | S16–S17 | clock/power gating, retention/isolation, drain, DVFS/HWP/Turbo, core C-states, package C-states, S0ix, D-states, ASPM L1.2, LTR |
| Debug + telemetry | S18 | residency counters, RAPL, perf-limit reasons, AER, IOMMU fault logs, first-divergence method, full-stack capstone |

**Coding:** 6 primary + 6 stretch exercises in weeks 1–6, plus 2 in week 7 (bit ops, CDC sync, device init, enumeration, doorbell ring, PkgC arbiter, IOMMU/TLB, FIFO, page walk).

**Deliberately not covered:** pre-silicon verification methodology (D24–D25, parked), RTL coding (D10), pipelines/microarchitecture (D21), AXI/CHI (D32), NVMe (D38), and puzzles (D39). Pull any of these in only if an interview needs them.

### Your bug stories: 4 total, when each one gets re-told

Keep it to **3 past stories + your current DSA work**. The other past bugs in the [README bug bank](README.md#a-note-on-bug-stories) are reference only. The rule: **re-tell the story at the end of its session, in ≤ 90 seconds, using that session's vocabulary.** Write the mechanism-level version into the session note. Week 7 re-tells all four cold.

| # | Story | Explained by | Re-tell in | What the session adds |
|---|---|---|---|---|
| 1 | **QNN page-fault / IOMMU-restore race** (Microsoft, cross-IHV), *your "hard bug" anchor* | IOMMU, DMA faults, wake sequencing | **S13**, again in S17 | The first DMA after wake used an IOVA before the translation tables were restored. It is the same class of failure as an accelerator job submitted before translation is ready |
| 2 | **Intel PM wake/ordering bug: Core C6 wake-loss (CFC FIFO race) + ADR opcode-ordering hang** (one owned Intel story, two angles) | CDC/async FIFO; ordering and drain | **S4** (CDC angle), **S12** (ordering angle) | Why the event was lost or reordered across domains, and which synchronizer/barrier fix closes it |
| 3 | **PkgC6 blocked by PCIe not in L1.2** | PkgC voting, ASPM L1.2, LTR | **S17** | The full vote chain from device link state to the package C-state decision |
| 4 | **Your current DSA bug(s)** (public-level summary only; details stay in `dsa-val/_internal/`) | Whatever layer it sits on | **S15**, refreshed whenever you close a new one | Places your live work on the full-stack diagram. This becomes your freshest interview story |

### Your career traces: where each one is rebuilt

These are the five traces from [D40](archive/day40_intel_role_narrative.md) that were fumbled in past interviews.

| Trace | Rebuilt in |
|---|---|
| Power states: P vs C vs D, kept separate | S16 (P), S17 (C, D) |
| CPU/SoC architecture diagram from memory | S9 (core → caches → LLC → memory), S11 (→ PCIe), §1 stack |
| Intel PCIe transactor + how you debugged it | S10–S12 (TLPs, ordering, first divergence) |
| L&T JTAG / lab debug | S7 (registers), S18 (observables) |
| Microsoft perf-regression flow | S18 (telemetry → baseline → first divergence) |

The **30-second career narrative** from [D40](archive/day40_intel_role_narrative.md) is rehearsed in week 7 and used in the week-8 mock.

---

## 2. Weekly rhythm

| Slot | When | Length | What |
|---|---|---|---|
| Study A | Mon | 60 min | Session 1 of the week |
| Study B | Wed | 60 min | Session 2 |
| Study C | Fri | 60 min | Session 3 + 5-min **week narration** |
| Coding | Sat or Sun | 45 min | One primary exercise (stretch exercise optional) |

**Every study session has the same shape:**

1. **Thread-in (5 min):** re-read your last note and say the handoff question aloud.
2. **Learn (35–40 min):** read the listed sections. Stay on the *Must answer* questions and skip the rest of the day file.
3. **Write (10–15 min)** in `notes/Sxx.md`:
   - the mechanism (sketch it),
   - what can go wrong and what you'd observe,
   - "this matters for PM because …",
   - the work link answer (one public-level line),
   - the handoff question.

**A session is done** when you can explain the note without looking at it.

**Every coding slot has the same shape:**
- Read the prompt in `coding/EXERCISES.md`, write the code in `coding/exercises/`, and build and run it.
- Say *why* it works aloud, then compare with the reference.
- Write 2 lines in that week's last note: what tripped you up, and what pattern you'd reuse.

**Slipping rules:**
- **Heavy office week:** do one study session and slide the calendar by a week. The 2 buffer weeks absorb this.
- **Never skip ahead.** The chain is the point.
- **A missed coding slot moves to the next weekend.** Don't double up.

---

## 3. Calendar

| Week | Dates | Theme | Study sessions (Mon/Wed/Fri) | Coding (weekend) |
|---|---|---|---|---|
| 1 | 19–25 Oct | Voltage → clocked hardware | S1 · S2 · S3 | `d04_bitops` (stretch `d09_fsm_detect`) |
| 2 | 26 Oct–1 Nov | Domains, power-on, CPU boot | S4 · S5 · S6 | `d13_cdc_sync` (stretch `d23_cpu_bringup_ladder`) |
| 3 | 2–8 Nov | How software sees hardware | S7 · S8 · S9 | `d03_dev_init` (stretch `d22_spsc_acqrel`) — *festival weekend, OK to slip* |
| 4 | 9–15 Nov | PCIe: power-on → driver | S10 · S11 · S12 | `d34_enum_walker` (stretch `d33_ltssm_walk`) |
| 5 | 16–22 Nov | Accelerator: DSA as worked example | S13 · S14 · S15 | `d20_sq_doorbell` (stretch `d19_tlb_iommu`) |
| 6 | 23–29 Nov | Power: idle, run, wake, debug | S16 · S17 · S18 | `d37_pkgc_blocker_arbiter` (stretch `d31_residency_delta`) |
| 7 | 30 Nov–6 Dec | **Buffer** | catch-up first; otherwise §5 | 2 slots: §5 list |
| 8 | 7–13 Dec | **Mock + re-evaluation** | §6 | — |

---

## 4. The 18 sessions

### Week 1 — From voltage to clocked hardware
*Story: an electrical signal arrives. How does it become a bit, get stored, get sequenced, and get a clock and reset?*

#### S1 — Signal → bit → stored state · Mon 19 Oct · L1–L2
- **Session file → [S01](s01_signal_logic_state.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D5 Boolean/combinational](archive/day05_boolean_combinational_logic.md), [D7 sequential logic](archive/day07_sequential_logic.md).
- **Must answer:**
  - What are VIL/VIH/VOL/VOH and noise margin? How does a CMOS inverter switch, and where does current flow?
  - Power = **dynamic (α·C·V²·f) + leakage**. Keep this formula: every PM technique later attacks one of these terms.
  - Latch vs flop. What are setup, hold, and clk→Q?
- **Work link:** pick one DSA status/enable bit you poll at work. Physically, it is a flop's output. Which clock domain is that flop in?
- **Handoff →** What limits clock speed, and how does hardware sequence decisions?

#### S2 — Timing and FSMs · Wed 21 Oct · L2
- **Session file → [S02](s02_timing_fsm.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D8 timing/STA](archive/day08_timing_sta.md), [D9 FSMs](archive/day09_fsm_sequence_detectors.md).
- **Must answer:**
  - The setup-slack equation. Why does lower V mean slower gates, so f must drop with it? (This is the seed of DVFS in S16.)
  - Moore vs Mealy. Draw a REQ/ACK handshake FSM. What do illegal, unreachable, and stuck states look like as hangs?
- **Work link:** device enable and WQ enable are handshake FSMs (request → status). Sketch one as REQ/ACK and mark where it could hang.
- **Handoff →** FSMs need a clock and a known start state. Where do those come from?

#### S3 — Clocks and reset · Fri 23 Oct · L3
- **Session file → [S03](s03_clocks_reset.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D11 clocking/reset architecture](archive/day11_clocking_reset_architecture.md).
- **Must answer:**
  - PLL: ref clock → lock → divided clocks. Why must logic wait for lock? Clock trees, skew, and clock domains.
  - Why is reset **asserted asynchronously and deasserted synchronously**? Why do reset *sequences* exist?
  - PCIe has its own refclk and reset (PERST#), which come back in S10.
- **Work link:** which resets can hit DSA (platform reset, FLR, device/WQ reset), and what state does each one clear?
- **Week narration:** voltage → bit → flop → timing → FSM → clock/reset.
- **Handoff →** What happens when signals cross between clock domains?

### Week 2 — Crossing domains, powering on, booting the CPU

#### S4 — Metastability and CDC · Mon 26 Oct · L2
- **Session file → [S04](s04_metastability_cdc.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D12 metastability](archive/day12_metastability_synchronizers.md), [D13 CDC techniques](archive/day13_cdc_techniques.md); skim [D14 async FIFO](archive/day14_async_fifo_cdc_verification.md) for Gray pointers.
- **Must answer:**
  - Why does a 2-flop synchronizer work? Which technique fits each signal type: level, pulse, or multi-bit bus?
  - **Lost-pulse wake:** a fast pulse is sampled by a slow or gated domain. What do you observe? (This is your Core C6 wake-loss story.)
- **Work link:** a doorbell write and a completion interrupt each cross between the CPU fabric and device clock domains. Where would a lost event show up as a hang?
- **Story re-tell (story 2, CDC angle):** **Core C6 wake-loss (CFC FIFO race)**, explained as a CDC structure problem: what was lost, and which synchronizer or FIFO fix closes it.
- **Handoff →** Before any clock ticks, what happens physically at power-on?

#### S5 — Power-on: rails to reset release · Wed 28 Oct · L3 *(inline primer)*
- **Session file → [S05](s05_power_on_sequence.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Primer (generic x86, public-level):**
  1. The **always-on/RTC well** powers first, so wake logic survives every sleep state.
  2. **Standby rails** come up. The platform controller's always-on domain leaves reset (RSMRST#), and management-engine firmware starts from SPI flash.
  3. Power button or wake → SLP_S5#/S4#/S3# deassert → **main rails ramp in order**. Each VR reports **power-good**.
  4. All power-goods → PWROK → **PLLs lock** → **fuses sensed** (configuration, V/F limits) → **PM controller firmware** (PCU class) starts.
  5. **PLTRST# deasserts.** The CPU leaves reset. PCIe devices see **PERST# deassert with a stable refclk**.
- **Must answer:**
  - Why is rail and power-good ordering strict, and what does a late rail cause?
  - Which domains are always-on, and why must wake logic live there?
  - Debug "doesn't power on": what do you check first (power-good chain, resets, POST codes)?
- **Work link:** on your test platform, what is the earliest point where the DSA device could be reached (after which reset, and after which firmware stage)?
- **Handoff →** The CPU is out of reset. What does it run first, and how does it reach the OS?

#### S6 — CPU firmware boot · Fri 30 Oct · L6 *(inline primer)*
- **Session file → [S06](s06_firmware_boot.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Primer (Intel x86, public-level):**
  1. **Before the reset vector:** FIT table → microcode patch → Boot Guard ACM verifies the initial boot block.
  2. **Reset vector 0xFFFFFFF0** with no DRAM yet. Firmware uses **cache-as-RAM** as its first stack.
  3. **SEC → PEI** (MRC memory training, so DRAM comes alive) **→ DXE** (drivers, **PCIe enumeration**, **ACPI tables**) **→ BDS → OS loader**.
  4. BIOS sets PM policy and locks it. ACPI publishes `_CST` (C-states), `_PSS`/CPPC (P-states), LPIT (S0ix), and **DMAR** (IOMMU units).
  5. The BSP wakes the APs with INIT-SIPI-SIPI.
- **Also read:** [D23 CPU bring-up](archive/day23_cpu_bringup_functional_val.md).
- **Must answer:**
  - Why does early firmware need cache-as-RAM?
  - Capability layering is **fuse → BIOS → ACPI → OS driver**. A feature (C-state, IOMMU, device) is "missing": which layer do you check first?
  - Hang at a POST code during memory init: how do you tell DRAM from the CPU?
- **Work link:** what does BIOS have to enable for DSA to work with shared virtual memory (VT-d on, scalable mode, the device visible)? Where would you check each?
- **Week narration:** CDC → rails → power-good → PLL/fuses → reset → reset vector → MRC → DXE → OS.
- **Handoff →** How does software actually talk to hardware?

### Week 3 — How software sees hardware

#### S7 — Registers: MMIO, config space, MSRs, bits · Mon 2 Nov · L4/L5
- **Session file → [S07](s07_registers_mmio.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D3 bits/MMIO/volatile/polling](archive/day03_bits_mmio_volatile_polling.md); skim [D1](archive/day01_c_address_fluency.md)/[D2](archive/day02_pointers_memory_layout_alignment.md).
- **Must answer:**
  - What `volatile` guarantees and what it doesn't. Read-modify-write races.
  - MMIO vs config space vs MSR: who uses which? A device BAR is an MMIO window.
  - Polling with a timeout. Why does every hang debug start with "what was it polling?"
- **Work link:** list the DSA registers you read most while debugging (capabilities, status, error, WQ config). For each one, which BAR or space is it in?
- **Handoff →** How is memory built and cached?

#### S8 — Memory and caches · Wed 4 Nov · L4
- **Session file → [S08](s08_memory_caches.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D15 memory/ECC](archive/day15_memory_systems_ecc.md) (DRAM, refresh, ECC), [D16 cache lines/AMAT](archive/day16_cache_lines_locality_amat.md).
- **Must answer:**
  - DRAM refresh and **self-refresh** (deep package idle needs it).
  - Write-back caches and dirty lines. Why caches must be **flushed before power gating**.
  - Why MMIO is **uncached** while DMA buffers live in **cacheable** memory.
- **Work link:** the cache-flush and memory-move style DSA operations, and how a cache-control hint changes where written data ends up.
- **Handoff →** How does memory stay consistent when many cores and devices touch it?

#### S9 — Coherence and memory ordering · Fri 6 Nov · L4
- **Session file → [S09](s09_coherence_ordering.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D17 coherence/MESI](archive/day17_coherence_mesi_false_sharing.md), [D22 memory consistency/atomics](archive/day22_memory_consistency_atomics.md).
- **Must answer:**
  - MESI states and snoops. What happens to a core's lines on C6 entry?
  - **I/O coherence:** device DMA must snoop CPU caches. Who guarantees that?
  - x86 TSO and fences. Bug signature: the flag is seen before the data.
- **Work link:** the app polls a completion record that the device writes. What ordering guarantee makes the status byte safe to trust before the data?
- **Career trace:** draw the CPU/SoC diagram from memory: core → L1/L2 → LLC → interconnect → memory controller.
- **Week narration:** register access → DRAM/caches → coherence/ordering.
- **Handoff →** A device must come alive on PCIe. What happens from PERST# deassert onward?

### Week 4 — PCIe: from power-on to a working driver
*Story: follow one device from reset release to its driver being ready to move data.*

#### S10 — PCIe boots: link-up · Mon 9 Nov · L5
- **Session file → [S10](s10_pcie_linkup.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D33 PCIe fundamentals/LTSSM](archive/day33_pcie_fundamentals_ltssm.md); link-training part of [D36 PCIe bring-up](archive/day36_pcie_bringup_functional_val.md).
- **Must answer:**
  - The three layers (TL/DLL/PHY), and which one produces TLPs, DLLPs, and ordered sets.
  - The sequence: power-good + refclk → **PERST# deassert** → **Detect → Polling → Configuration → L0** → DL_Active → speed change and equalization.
  - Where links get stuck (Detect, Polling, Recovery loops), and what you read (LTSSM state, link status, AER).
  - The post-reset wait and **CRS** ("not ready yet"). Why does it cause boot hangs?
- **Work link:** DSA is an on-die RCiEP with no external link to train. What *does* it still need from reset and power to be visible? Compare it with a discrete card.
- **Career trace:** how your Intel PCIe transactor drove TLPs, and how you found the first mismatch.
- **Handoff →** The link is in L0. How does firmware find out what the device is and give it addresses?

#### S11 — Enumeration → resources → driver probe · Wed 11 Nov · L5/L7
- **Session file → [S11](s11_pcie_enumeration_driver.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D34 enumeration/config/MSI](archive/day34_pcie_enumeration_config_msi.md); enumeration part of [D36](archive/day36_pcie_bringup_functional_val.md).
- **Must answer:**
  - ECAM config space and the bus/device/function walk. Bridges. **RCiEPs** (how DSA appears).
  - **BAR sizing** (write all-1s, read back) → address assignment → enable Memory Space and **Bus Master**. What breaks if Bus Master is off?
  - Capabilities: MSI-X, PM, PCIe; extended capabilities **PASID, ATS, PRI**.
  - OS handoff: re-scan → match vendor/device ID → **probe()** → map BARs → allocate MSI-X vectors.
- **Work link:** run `lspci -vvv` on the DSA device. Find its BARs, MSI-X table size, PASID/ATS capabilities, and the bound driver. Map each line to this session.
- **Handoff →** The driver is bound. How does the device move data and say "done"?

#### S12 — DMA, doorbells, interrupts, ordering · Fri 13 Nov · L5
- **Session file → [S12](s12_dma_doorbells_interrupts.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D20 DMA/doorbells/ordering](archive/day20_dma_doorbells_ordering_barriers.md).
- **Must answer:**
  - The generic flow: descriptor in memory → **doorbell MMIO write** → device DMA → completion write → **MSI-X** (itself just a memory write).
  - PCIe ordering: posted writes, read-flushes-writes. Why completion data must be visible before the interrupt.
  - **Story re-tell (story 2, ordering angle):** retell the **ADR ordering hang** in these terms.
- **Work link:** for a DSA job, which step is the doorbell, which writes are posted, and what arrives first at memory: the completion record or the MSI-X?
- **Week narration:** PERST# → LTSSM → L0 → enumeration → BARs/MSI-X → probe → doorbell → DMA → interrupt.
- **Handoff →** The device DMAs to "addresses". Whose addresses are they?

### Week 5 — Accelerators: DSA as the worked example
*Story: an accelerator is a PCIe device plus shared virtual memory plus a submission model. This is public-spec level only.*

#### S13 — IOMMU, PASID, ATS · Mon 16 Nov · L5/L7
- **Session file → [S13](s13_iommu_pasid_ats.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D18 virtual memory](archive/day18_virtual_memory_page_faults.md) (page walk and faults), [D19 TLB/IOMMU](archive/day19_tlb_huge_pages_iommu_smmu.md), [DSA fundamentals §6](../dsa-val/fundamentals/dsa_ip_fundamentals.md).
- **Must answer:**
  - CPU page walk + TLB. The IOMMU does the same for devices (requester ID → context → page table). A **DMA remapping fault** is the device's segfault.
  - **PASID** lets a device use a process's virtual addresses (SVA).
  - **ATS** caches translations on the device; **PRI** asks the OS to fault pages in. What happens if an invalidation is slow or missed?
- **Work link:** the last translation or page-fault related failure you saw at work. Which arrow on the translation path broke?
- **Story re-tell (story 1):** the **QNN page-fault / IOMMU-restore race**. On wake, the first DMA went out before the translation tables were restored, causing a fault with no recovery path. Tell it in ≤ 90 s using IOVA, page table, fault, and restore-before-first-DMA.
- **Handoff →** Translation is solved. How does the accelerator get configured and enabled?

#### S14 — Accelerator bring-up · Wed 18 Nov · L4/L7
- **Session file → [S14](s14_accelerator_bringup.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [DSA fundamentals §1–§3, §8](../dsa-val/fundamentals/dsa_ip_fundamentals.md).
- **Must answer:**
  - The chain applied: RCiEP → enumerated (S11) → BARs mapped → **idxd** probes.
  - Architecture: **WQs (dedicated vs shared) → groups → engines**. Configure → enable device → enable WQ. Each enable is a handshake FSM (S2) with status and error registers.
  - Bring-up failures: an enable that never completes, a config rejected with an error code, PASID/ATS not enabled (S11), or the IOMMU not in scalable mode (S6/S13).
- **Work link:** write the configure → enable sequence you use at work as an FSM, and list the observable at each step.
- **Handoff →** The WQ is enabled. What happens when one job is submitted?

#### S15 — One accelerator job, end to end · Fri 20 Nov · L9→L4→L9
- **Session file → [S15](s15_accelerator_job_e2e.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [DSA fundamentals §4–§5, §7, §9](../dsa-val/fundamentals/dsa_ip_fundamentals.md).
- **Must answer:**
  - **Submit:** a 64-byte descriptor → **MOVDIR64B** (dedicated WQ) or **ENQCMD/ENQCMDS** (shared WQ, with retry when full) to the **portal**, an MMIO page in a BAR (S7/S11).
  - **Execute:** the engine reads the descriptor → addresses translated with PASID (S13) → coherent DMA (S9) → completion record → interrupt (S12) or polling.
  - **Page fault mid-job:** block-on-fault vs partial completion, and what the completion status shows.
  - Draw *app → ENQCMD → portal → WQ → engine → IOMMU → memory → completion → MSI-X/poll*, with one failure mode and one observable per arrow.
- **Work link:** put the issue you are currently debugging on this diagram. Which arrow is it?
- **Week narration:** IOMMU/PASID → enable → submit → complete.
- **Handoff →** When everything goes idle, how does the system save power, and how does it wake for the next job?

### Week 6 — Power: idle, run, wake, debug

#### S16 — Gating and DVFS · Mon 23 Nov · L3
- **Session file → [S16](s16_gating_dvfs.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D28 clock/power gating](archive/day28_clock_power_gating_upf.md) (concepts; UPF syntax optional), [D26 DVFS/P-states](archive/day26_dvfs_pstates_pvt.md).
- **Must answer:**
  - Clock gating cuts dynamic power; power gating cuts leakage (S1). Gating sequence: **drain → save/retain → isolate → cut**, then reverse on wake.
  - Accelerator view: in-flight work must **drain** before gating (the Drain operation and WQ disable exist for this). What if a job arrives mid-gating?
  - DVFS: going up, **V before f**; going down, **f before V**. HWP/CPPC, Turbo limits, fused V/F curves (S5).
- **Work link:** what software does at work before resetting or disabling a WQ (drain/abort), and why skipping it causes failures.
- **Handoff →** How does the whole package go idle?

#### S17 — C-states, PkgC, S0ix, device and PCIe PM · Wed 25 Nov · L3–L7
- **Session file → [S17](s17_cstates_s0ix_device_pm.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D27 C-states/PkgC/S0ix](archive/day27_cstates_pkgc_s0ix.md), [D35 PCIe PM](archive/day35_pcie_power_management.md), [D37 PCIe×PM](archive/day37_pcie_pm_intersection.md).
- **Must answer:**
  - Core C1 → C6: clock stop → cache flush (S8/S9) → state save → power gate (S16).
  - Package C-states need all cores idle and all devices agreeing. Devices vote with **D-states, L1/L1.2 (CLKREQ#), and LTR**. Pending DMA or a short LTR blocks deep package C-states. Retell **"PkgC6 blocked by PCIe L1.2"**.
  - S0ix: always-on wake domains (S5), DRAM self-refresh (S8), wake sources.
  - A **"PkgC6 won't enter" ladder:** residency counters → blocker status → device D-states/LTR → link states.
- **Work link:** is DSA a potential package C-state blocker while it has queued or in-flight work? What should happen when it's idle?
- **Story re-tell (story 3):** **PkgC6 blocked by PCIe L1.2** (full vote chain). Then revisit stories 1 and 2 as wake-path failures. Finish by saying P vs C vs D states in 30 s.
- **Handoff →** How do you debug the whole chain on real silicon?

#### S18 — Capstone: post-silicon debug across the chain · Fri 27 Nov · all layers
- **Session file → [S18](s18_postsi_debug_capstone.md)** (self-contained: concept, deep notes, worked examples, bugs, Q&A, exit check). The bullets below are the summary checklist; `Read`/`Also read` links are optional deeper reference.
- **Read:** [D29 thermal/power/perf counters](archive/day29_thermal_power_perf_counters.md), [D31 post-silicon PM](archive/day31_post_si_pm_validation.md).
- **Must answer:**
  - **Observables:** residency counters, RAPL, perf-limit reasons, POST codes, LTSSM/AER, IOMMU fault logs, completion status.
  - **Method:** expected flow → first divergence → which layer (fuse, BIOS, OS, firmware, hardware).
- **Capstone (whiteboard, 25 min):** *"The platform is in S0ix. An app wakes and submits a DSA copy job. The result triggers a NIC transmit. Then everything goes idle again."* Trace it on the §1 stack:
  1. wake source and always-on logic
  2. rails and PLLs restored
  3. package C-state, then core C-state exit
  4. links go L1.2 → L0
  5. device leaves its low-power state
  6. ENQCMD → WQ → IOMMU/PASID → DMA → completion → MSI-X
  7. NIC doorbell → DMA → transmit
  8. back to idle

  Give **one failure mode and one observable per step**.
- **Week narration:** the capstone itself, in under 5 minutes.

---

## 5. Week 7 — Buffer (30 Nov – 6 Dec)

**First,** finish any sessions that slipped. **If you're on track:**
- **Mon:** redo the S15 diagram and the S17 ladder from memory, then compare with your notes.
- **Wed:** re-tell all four stories cold, ≤ 90 s each, out loud: **QNN IOMMU race**, **C6 wake-loss / ADR ordering**, **PkgC6 vs L1.2**, and **your current DSA bug**. Then rehearse the 30-second career narrative from [D40](archive/day40_intel_role_narrative.md).
- **Fri:** sweep [REVISION.md](REVISION.md) Phases 0–4 and 6–7 (Phase 5 is parked). Note every card you couldn't answer.
- **Coding (2 slots):** any stretch exercise you skipped, plus `d14_async_fifo` and `d18_page_walk`.

If you need more time, extend by a week (or two). Extending is part of the plan, not a failure.

---

## 6. Week 8 — Mock + re-evaluation (7 – 13 Dec)

- **Mock (Mon or Wed, 90 min), using [D41](archive/day41_final_mock.md):**
  - the capstone trace, cold;
  - two bug stories;
  - one 30-min coding question (one you haven't done, e.g. `d22_spsc_acqrel` or `d02_aligned_alloc`);
  - 3 random REVISION cards.
- **Re-evaluation (Fri, 30 min).** Answer these in `notes/REVIEW.md`:
  1. Which layers on the §1 stack can I explain cold? Which can't I?
  2. Did the work links help me understand my DSA debugging? Give one concrete example.
  3. How many sessions and coding slots did I actually complete? Is 3+1 per week sustainable alongside the office?
  4. Coding: which patterns were slow?
  5. What is the next block: the CPU-PM depth block (default), more coding, or a gap the mock exposed?
- **From 14 Dec:** start the next block in [README.md](README.md#the-next-8-weeks-after-the-foundation), adjusted by the review.

---

## 7. Tracker

Tick as you go. Write the actual date next to anything that slips.

| # | Session | Planned | Done | Note written | Can explain cold |
|---|---|---|---|---|---|
| S1 | Signal → bit → state | 19 Oct | [ ] | [ ] | [ ] |
| S2 | Timing + FSMs | 21 Oct | [ ] | [ ] | [ ] |
| S3 | Clocks + reset | 23 Oct | [ ] | [ ] | [ ] |
| S4 | Metastability + CDC | 26 Oct | [ ] | [ ] | [ ] |
| S5 | Power-on | 28 Oct | [ ] | [ ] | [ ] |
| S6 | CPU firmware boot | 30 Oct | [ ] | [ ] | [ ] |
| S7 | Registers | 2 Nov | [ ] | [ ] | [ ] |
| S8 | Memory + caches | 4 Nov | [ ] | [ ] | [ ] |
| S9 | Coherence + ordering | 6 Nov | [ ] | [ ] | [ ] |
| S10 | PCIe link-up | 9 Nov | [ ] | [ ] | [ ] |
| S11 | Enumeration → probe | 11 Nov | [ ] | [ ] | [ ] |
| S12 | DMA + interrupts | 13 Nov | [ ] | [ ] | [ ] |
| S13 | IOMMU/PASID/ATS | 16 Nov | [ ] | [ ] | [ ] |
| S14 | Accelerator bring-up | 18 Nov | [ ] | [ ] | [ ] |
| S15 | One job end to end | 20 Nov | [ ] | [ ] | [ ] |
| S16 | Gating + DVFS | 23 Nov | [ ] | [ ] | [ ] |
| S17 | C-states/S0ix + device PM | 25 Nov | [ ] | [ ] | [ ] |
| S18 | Capstone | 27 Nov | [ ] | [ ] | [ ] |

| Week | Coding primary | Done | Stretch | Done |
|---|---|---|---|---|
| 1 | `d04_bitops` | [ ] | `d09_fsm_detect` | [ ] |
| 2 | `d13_cdc_sync` | [ ] | `d23_cpu_bringup_ladder` | [ ] |
| 3 | `d03_dev_init` | [ ] | `d22_spsc_acqrel` | [ ] |
| 4 | `d34_enum_walker` | [ ] | `d33_ltssm_walk` | [ ] |
| 5 | `d20_sq_doorbell` | [ ] | `d19_tlb_iommu` | [ ] |
| 6 | `d37_pkgc_blocker_arbiter` | [ ] | `d31_residency_delta` | [ ] |
| 7 | `d14_async_fifo` | [ ] | `d18_page_walk` | [ ] |

---

## Not in this sequence (backstop only)

[D0 C basics](archive/day00_c_basics.md) · [D6 arithmetic](archive/day06_arithmetic_circuits.md) · [D10 Verilog/SV](archive/day10_verilog_systemverilog.md) · [D21 pipeline](archive/day21_pipeline_microarch.md) · [D30 pre-Si PM](archive/day30_pre_si_pm_validation.md) · [D32 AXI/CHI](archive/day32_amba_axi_chi.md) · [D38 NVMe](archive/day38_nvme_storage_controller_val.md) (natural next device after S15) · [D39 puzzles](archive/day39_logic_puzzles_estimation.md) · [D40 narrative](archive/day40_intel_role_narrative.md). D24–D25 remain parked.

**If an interview gets scheduled mid-plan:** pause, do role-shaped prep + the [D41 mock](archive/day41_final_mock.md), then resume where you stopped.

# S11 - Enumeration, resources, and driver probe

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [D34 enumeration/config/MSI](archive/day34_pcie_enumeration_config_msi.md); enumeration part of [D36](archive/day36_pcie_bringup_functional_val.md).
- **Must answer:**
  - ECAM config space and the bus/device/function walk. Bridges. **RCiEPs** (how DSA appears).
  - **BAR sizing** (write all-1s, read back) → address assignment → enable Memory Space and **Bus Master**. What breaks if Bus Master is off?
  - Capabilities: MSI-X, PM, PCIe; extended capabilities **PASID, ATS, PRI**.
  - OS handoff: re-scan → match vendor/device ID → **probe()** → map BARs → allocate MSI-X vectors.
- **Work link:** run `lspci -vvv` on the DSA device. Find its BARs, MSI-X table size, PASID/ATS capabilities, and the bound driver. Map each line to this session.
- **Handoff →** The driver is bound. How does the device move data and say "done"?

## Study notes

### Concept - [day34_pcie_enumeration_config_msi.md](archive/day34_pcie_enumeration_config_msi.md)

PCIe enumeration is the **depth-first recursive walk** that the root complex (or BIOS) performs across the topology at boot: starting at bus 0, probe every device/function, read its config header, assign bus numbers, size + assign BARs, walk the capability chain, program MSI/MSI-X, and recurse through bridges. **Every PCIe device on the planet, from a graphics card to an NVMe to a 100GbE NIC, is found by the same algorithm.** Config space is the device's self-description; BAR sizing is the protocol by which a device tells SW how much memory window it needs; MSI/MSI-X is how it asks for an interrupt without a wire.

### Deep Study Notes - [day34_pcie_enumeration_config_msi.md](archive/day34_pcie_enumeration_config_msi.md)

### 1. The enumeration algorithm (depth-first)

```
walk(bus):
  for dev in 0..31:
    for fn in 0..7:
      vid = read_cfg(bus, dev, fn, 0x00)
      if vid == 0xFFFF: continue            # no device
      hdr = read_cfg(bus, dev, fn, 0x0E) & 0x7F
      size_and_assign_bars(bus, dev, fn)
      walk_capability_chain(bus, dev, fn)
      program_msi_or_msix(bus, dev, fn)
      if hdr == 0x01:                         # Type-1 (bridge)
        assign secondary_bus = next_bus_num
        write_cfg(bus, dev, fn, 0x19, secondary_bus)
        walk(secondary_bus)                   # recurse
        write_cfg(bus, dev, fn, 0x1A, last_bus_num_used)
      if fn == 0 and !multifunction(hdr): break
```

**Key invariants:**
- Bus 0 is the root complex.
- Bridges (Type-1 headers) have `secondary_bus` and `subordinate_bus` registers — the recursion's stack frame.
- Endpoints (Type-0 headers) terminate the recursion.
- `vid == 0xFFFF` for empty slots — config reads to absent functions return all-1s by spec.

### 2. Config space — the 4 KB self-description

```
   offset
   0x00  Vendor ID (16)              Device ID (16)
   0x04  Command (16)                Status (16)
   0x08  Revision (8)  Class code (24)
   0x0C  CLS (8)  Latency (8)  Header type (8)  BIST (8)
   0x10  BAR0
   0x14  BAR1
   0x18  BAR2
   0x1C  BAR3
   0x20  BAR4
   0x24  BAR5
   0x28  Cardbus CIS
   0x2C  Subsys VID (16)              Subsys ID (16)
   0x30  Expansion ROM BAR
   0x34  Capabilities pointer (8)   reserved
   0x3C  Interrupt line/pin
   ...
   0x100 PCIe extended capability chain
   ...
   0xFFF
```

**Header type byte (0x0E):**
- 0x00 — endpoint (Type-0)
- 0x01 — PCI-to-PCI bridge (Type-1)
- 0x02 — cardbus (legacy, ignored)
- bit 7 — multifunction (probe fns 1-7)

### 3. BAR sizing — the write-all-1s trick

A BAR (Base Address Register) holds the base of a memory window. To find the window size, SW writes all-1s and reads back — the lower bits that are stuck at 0 indicate alignment, the lowest-set bit indicates size.

```
Algorithm:
  orig = read_cfg(bar)
  write_cfg(bar, 0xFFFFFFFF)
  size_mask = read_cfg(bar)
  write_cfg(bar, orig)               # restore
  size_mask &= ~0xF                   # mask out type bits
  size = (~size_mask) + 1             # 2's complement → actual size
```

**BAR low-bit decoding:**
```
bit 0:    0 = memory BAR, 1 = I/O BAR
bits 1-2: memory type (00 = 32-bit, 10 = 64-bit)
bit 3:    prefetchable
```

**64-bit BARs** consume two consecutive BAR slots (BAR0+BAR1, BAR2+BAR3, BAR4+BAR5). The upper BAR holds bits [63:32] of the base.

After sizing, SW picks an address in MMIO space, writes the base to the BAR, and the device's registers are now visible at `base..base+size`. The bridge's **memory base/limit registers** must be programmed to forward this range downstream.

### 4. Capability chain

Capabilities pointer at 0x34 points to first cap in config space (>= 0x40). Each cap is a TLV-style record:

```
   cap_offset:  Cap ID (8) | Next Ptr (8) | cap-specific data...
```

Walk by following Next Ptr until 0. Common Cap IDs:

| ID | Name | Purpose |
|---|---|---|
| 0x01 | PM | D-state control (D0/D1/D2/D3hot) |
| 0x05 | MSI | Message Signaled Interrupts |
| 0x10 | PCIe | Device/Link/Slot capability + control registers |
| 0x11 | MSI-X | MSI-X table + PBA |
| 0x12 | SATA | (legacy) |
| 0x13 | AF | Advanced Features |

**Extended capabilities** (offset >= 0x100) include AER, ACS, ARI, ATS, PRI, PASID, SR-IOV, LTR, L1SS.

### 5. PCIe Capability — Device/Link/Slot

The PCIe cap (ID 0x10) is the most important — it contains the registers that control PM, link speed, link width, and reset:

```
Offset (rel to cap)
  0x00  Cap header
  0x02  PCIe caps
  0x04  Device Capabilities
  0x08  Device Control / Device Status
  0x0C  Link Capabilities
  0x10  Link Control / Link Status      ← ASPM control, target link speed
  0x14  Slot Capabilities
  0x18  Slot Control / Slot Status
  0x1C  Root Control / Root Capabilities
  0x20  Root Status
  0x24  Device Capabilities 2
  0x28  Device Control 2 / Status 2     ← LTR enable, OBFF
  0x2C  Link Capabilities 2
  0x30  Link Control 2 / Status 2       ← negotiated link speed, equalisation
```

**ASPM** is two bits in Link Control: 00=off, 01=L0s, 10=L1, 11=both. SW must read Link Capabilities to find out what the link partner *supports* before enabling.

### 6. MSI — legacy message-signaled interrupts

```
MSI cap layout:
  0x00  Cap header
  0x02  Message Control          [enable, 64-bit, multi-msg cap/en, mask cap]
  0x04  Message Address (low)
  0x08  Message Address (high)   (if 64-bit)
  0x0C  Message Data
  0x10  Mask Bits                (if mask cap)
  0x14  Pending Bits             (if mask cap)
```

**How it fires:** when the device wants to raise an interrupt, it issues a **posted write** of `Message Data` to `Message Address`. The address is typically `0xFEEx_xxxx` on x86 — a magic range that the LAPIC decodes as an interrupt request. No physical IRQ wire; the write *is* the interrupt.

**Multiple vectors:** `multi-msg cap` says "I support up to 2^N vectors." SW grants 2^M (M<=N) by writing `multi-msg en`. The device varies `Message Data[M-1:0]` to indicate which vector it is firing.

**Limitations:** all vectors share one address; only the data field differs; up to 32 vectors max.

### 7. MSI-X — modern message-signaled interrupts

MSI-X scales to 2048 vectors, each with its own independent (address, data) pair, stored in a **table** in device memory.

```
MSI-X cap:
  0x00  Cap header
  0x02  Message Control [enable, function mask, table size]
  0x04  Table BAR / Offset    [bits[2:0]=BAR#, [31:3]=offset in that BAR]
  0x08  PBA BAR / Offset      [Pending Bit Array]
```

The **MSI-X table** is N × 16 bytes (4 dwords each):

```
  +0  Message Address Low
  +4  Message Address High
  +8  Message Data
  +12 Vector Control [bit 0 = mask]
```

**How it fires:** device wants to raise vector V → issues a posted write of `Table[V].Data` to `Table[V].Addr`. SW can move any vector to any address (e.g., one vector per CPU for queue affinity in NVMe / NIC drivers).

**Pending Bit Array (PBA):** one bit per vector, set when the device wants to fire but is masked; cleared after SW unmasks and the interrupt fires.

### 8. Bridge windows — the recursion bookkeeping

A Type-1 bridge has three windows that forward downstream:

```
  Memory Base/Limit       0x20/0x22  (32-bit, 1 MB granularity)
  Prefetchable Mem B/L    0x24/0x26 + 0x28/0x2C (64-bit)
  I/O Base/Limit          0x1C/0x1D
```

For every downstream device behind a bridge, SW assigns BARs *inside* the bridge's window. The bridge forwards any transaction whose address falls inside its base/limit; everything else stays upstream.

This is why a depth-first walk computes **subordinate bus** (the highest bus number behind this bridge) on the way back up — the bridge needs to know its range.

### 9. The classic enumeration bugs

| Bug | Symptom | Root cause |
|---|---|---|
| **BAR mis-sized** | Device works at offset 0..N then aliases | SW wrote partial 1s, didn't check 64-bit pairing |
| **Bridge window too small** | Downstream device unreachable | SW didn't recompute window after assigning all child BARs |
| **Cap chain loop** | Enumerator hangs | A capability's `next` points back into the chain |
| **MSI-X table outside BAR** | Writes go nowhere | SW BAR-assigns smaller than MSI-X table needs |
| **Multifunction probe skipped** | Functions 1-7 invisible | SW didn't read header-type bit 7 |
| **Subordinate bus too low** | Devices behind bridge work, devices behind bridge-of-bridge don't | Subordinate not propagated up |
| **Config write to non-existent function** | UR (unsupported request) error | SW didn't check `vid != 0xFFFF` |
| **64-bit BAR assigned 32-bit address** | Upper 32 bits left as garbage | SW used 32-bit assignment path |

### 10. Validation surface

A PCIe IP val team owns enumeration coverage as:

- **Header type cross:** Type-0 (4 BAR layouts) × Type-1 (bridge windows) × multifunction (1F, 2F, 8F).
- **BAR size cross:** every power-of-2 from 4 KB to 256 MB × 32-bit × 64-bit × prefetchable.
- **Cap chain cross:** every supported cap × at every legal offset × in every legal order.
- **MSI/MSI-X cross:** vector count {1, 2, 4, 8, 16, 32, 64, 256, 2048} × per-vector mask × pending.
- **Bridge depth cross:** 1, 2, 4, 8 levels of bridge nesting.
- **Failure injection:** UR responses, malformed cap chains, BAR-write-during-traffic.

Each of these is a row in the vplan. The IP exposes a config-space agent in the testbench; SW model (or BIOS code under test) walks it; scoreboard verifies the post-walk state matches expectation.

---

### Bugs in this Topic - [day34_pcie_enumeration_config_msi.md](archive/day34_pcie_enumeration_config_msi.md)

- **64-bit BAR sized as 32-bit:** SW wrote 0xFFFFFFFF only to lower BAR; size readback masked off type bits incorrectly; device assigned wrong window. *Fix: pair BAR0+BAR1 read/write; check bit-2 for 64-bit.*
- **MSI-X table BAR too small:** topology declared 64-vector MSI-X (1 KB table) but the BAR holding it was 256 B. *Fix: SW model must round BAR size up to enclose table+PBA.*
- **Cap chain Next Ptr = own offset:** infinite loop on enumerator. *Fix: cap-chain walker bounds check + SVA on RTL.*
- **Multifunction bit ignored:** functions 1-7 of a multifunction device never probed. *Fix: enumerator reads `header_type & 0x80`; covered by vplan multifunction cross.*
- **Subordinate-bus narrow:** bridge-of-bridge devices invisible. *Fix: depth-first walk computes subordinate on return.*
- **BAR write during link-down race:** enumerator started before LTSSM reached L0. *Fix: gate enum start on link-up; SVA `p_no_cfg_before_l0`.*
- **MSI message-data wrong vector encoding:** multi-msg of 4 used bits [1:0] but device hardware decoded [2:1]. *Fix: spec ambiguity → directed test + LAPIC-target scoreboard.*
- **Bridge prefetchable window mis-typed:** non-prefetchable BAR assigned to prefetchable window → MMIO coherence broken. *Fix: enumerator separates pref vs non-pref pools.*

---

### Interview Explanation (60-90 sec) - [day34_pcie_enumeration_config_msi.md](archive/day34_pcie_enumeration_config_msi.md)

> "PCIe enumeration is a depth-first recursive walk: starting at bus 0, you read each function's vendor ID, if it exists you read the header, size the BARs using the write-all-1s trick to discover alignment and size, walk the capability chain by following next-pointers, program MSI or MSI-X by allocating addresses in the LAPIC range and writing them into the device's table, and if it's a bridge you assign a secondary bus number and recurse. On the way back up you write the subordinate bus number so the bridge knows its forwarding range. MSI is up to 32 vectors sharing one address with the data field varying; MSI-X scales to 2048 vectors each with its own address/data pair stored in a table inside one of the device's BARs. Every step here is a validation row: header types, BAR size classes, cap chain orders, MSI-X table-in-BAR sizing, bridge nesting depth, plus failure injections like UR responses and malformed cap chains. The IP val team builds a topology generator, runs the enumerator under test (real BIOS in emul, a model in sim), and scoreboards against an expected post-enum state. The bugs that escape this here are the ones that surface as 'device works in slot 1 but not slot 2' — usually bridge window or sub-bus bookkeeping."

---

### Architect Drills - [day34_pcie_enumeration_config_msi.md](archive/day34_pcie_enumeration_config_msi.md)

*Enumeration is a tree-walk algorithm meeting hardware constraints. Reason without an answer key.*

**D1. Why is enumeration depth-first rather than breadth-first?**
*Constraints to weigh:* bridge bus-number assignment dependency, subordinate-bus propagation, resource (BAR window) accounting, memory cost of the algorithm's stack.

**D2. BAR sizing uses write-all-1s. Reason about what happens if the device is actively servicing traffic during the size probe.**
*Constraints to weigh:* config writes are out-of-band relative to MMIO traffic, transient mis-mapped BAR, what real BIOS code does to protect against this.

**D3. MSI vs MSI-X — when would a modern device choose MSI?**
*Constraints to weigh:* vector count limits, table memory cost, SW affinity flexibility, legacy compatibility, IP complexity.

**D16. Reason about a topology with 8 levels of bridge nesting. What goes wrong before what?**
*Constraints to weigh:* bus-number space (256 buses), BAR window aggregation, latency through bridges, real silicon (almost never sees this).

**D17. The cap chain points back into itself (loop). Reason about whether the SW or HW should be responsible for catching this.**
*Constraints to weigh:* HW assertion vs SW bounds-check, what each catches in real life, escape if HW is "correct" but the chain SW programmed is wrong.

**D18. A 64-vector MSI-X device wants per-CPU queue affinity. Reason about where the val team's coverage must intersect with the host IRQ subsystem.**
*Constraints to weigh:* per-vector address (one per CPU LAPIC), Linux `irq_set_affinity`, NUMA implications, hot-add CPU paths.

**D19. Enumeration coverage is 100% in pre-Si; post-Si finds a bug where a specific endpoint behind a specific bridge fails to enumerate when the link comes up at 2 instead of 3. Reason about what was missing.**
*Constraints to weigh:* link-speed × enum-timing cross, what TLP traffic looks different at different speeds, vplan dimension that wasn't covered.

---

### Exit Check - [day34_pcie_enumeration_config_msi.md](archive/day34_pcie_enumeration_config_msi.md)

- [ ] Wrote the depth-first enumeration pseudo-code.
- [ ] Explained the write-all-1s BAR sizing trick + 64-bit BAR pairing.
- [ ] Walked the capability chain by hand for one example.
- [ ] Stated the difference between MSI and MSI-X.
- [ ] Drew the MSI-X table layout (4 dwords per vector).
- [ ] Stated what bridge `secondary_bus` and `subordinate_bus` are for and when each is written.
- [ ] Named 6 enumeration coverage axes for a PCIe IP vplan.
- [ ] Read an `lspci -vvv` output and identified header type, BARs, caps, MSI/MSI-X state.

### Concept - [day36_pcie_bringup_functional_val.md](archive/day36_pcie_bringup_functional_val.md)

PCIe bring-up is the structured process of taking a designed link from reset to **L0 carrying real traffic**, then proving — through analyser captures, AER monitoring, compliance test patterns, and traffic stress — that the link, the transaction layer, and the SW driver path all match the spec under every realistic scenario. The hardest part is **diagnosing a link that won't reach L0**: PCIe gives you a handful of LTSSM substates and a pile of side-band signals, and the bug could be electrical, polarity, clock, equalisation, side-band, or SW. **A structured LTSSM-stuck-state ladder is the load-bearing diagnostic skill.**

### Deep Study Notes - [day36_pcie_bringup_functional_val.md](archive/day36_pcie_bringup_functional_val.md)

### 1. The PCIe bring-up ladder

```
0. Refclk + power rails       (clock present, rails stable, PERST# asserted)
   │
   ▼
1. PERST# deasserted          (controller exits hot-reset)
   │
   ▼
2. Detect                     (electrical idle, RX detect on each lane)
   │
   ▼
3. Polling                    (TS1/TS2 ordered sets exchanged, bit-lock + symbol-lock)
   │
   ▼
4. Configuration              (link width + lane reversal + polarity negotiated)
   │
   ▼
5. L0 at Gen1                 (link up at lowest speed)
   │
   ▼
6. Equalisation               (Gen3+: phases 0-3 negotiate TX coefficients)
   │
   ▼
7. Recovery → L0 at target    (link retrains at negotiated speed)
   │
   ▼
8. Config space accessible    (host can enumerate)
   │
   ▼
9. TLP traffic                (memory reads/writes flow through)
   │
   ▼
10. Long-soak                 (24-72 hr error-free, no AER, no retrains)
```

Each step has a **stuck-state failure mode** with a structured diagnostic.

### 2. LTSSM stuck-state diagnostic ladder

| Stuck in | What it means | Hunt these |
|---|---|---|
| **Detect** | RX never sees electrical-idle exit on any lane | Refclk dead? PERST# stuck? RX terminations off? Cable/connector? Polarity? |
| **Polling.Active** | TS1s sent, partner's TS1s not seen | TX/RX swap? Polarity reversed on all lanes? PHY PLL not locked? |
| **Polling.Configuration** | Bit-lock OK, can't agree on a common config | Lane count mismatch, partner advertising width > capability |
| **Polling.Compliance** | Stuck in compliance pattern | Compliance bit set in TS1 by partner; check link partner config |
| **Configuration.Linkwidth.Start** | Width negotiation hung | Lane-to-lane skew, lane reversal mis-detected |
| **Configuration.Lanenum.Accept** | Lane numbering not agreed | Lane reversal not handled by IP |
| **Recovery.RcvrLock** | Lost bit-lock during retrain | Equalisation phase failed, SI margin marginal |
| **Recovery.Equalization** | Gen3+ eq did not converge | TX preset/coefficient range insufficient, RX CTLE limits |
| **Recovery.Speed** | Speed change failed | Eq result not applied, PLL re-lock time exceeded |
| **L0 → Recovery loop (L0p)** | Link in L0 briefly then back to Recovery | Marginal SI: random symbol errors trigger retrain; eq drifted |
| **L0s entry → no exit** | TX in L0s, partner doesn't issue FTS | FTS count mismatch, partner's L0s entry timing wrong |

This ladder is **the** PCIe debug skill. You read the LTSSM trace, find the deepest substate the link reached, and the table tells you where to look.

### 3. Equalisation (Gen3+) — the most error-prone bring-up step

Gen3 introduced 8b/10b → 128b/130b encoding and a much more aggressive eye, requiring **TX equalisation**. The eq protocol has 4 phases:

```
Phase 0  — start of eq, baseline; uses preset coefficients
Phase 4  — exchange TX coefficients; receivers measure eye
Phase 6  — receiver requests TX coefficient adjustments
Phase 7  — final convergence; lock in chosen coefficients
```

Eq settings live in **TX FFE coefficients (C-1, C0, C+1)** and **RX CTLE setting**. Mis-converged eq means the link "trains" but every few seconds drops to Recovery for re-training — the classic L0p loop.

Debug: analyser captures eq exchange in Recovery.Equalization; compare requested vs applied coefficients; if RX kept asking for more pre-emphasis than TX could provide, **add an Re-driver** or fix board impedance.

### 4. AER — Advanced Error Reporting

The transaction layer reports errors via the AER extended capability (ID 0x0001). Categories:

| Class | Examples | AER bit |
|---|---|---|
| **Correctable** | Receiver Error (8b/10b decode), Bad TLP, Bad DLLP, Replay Timer Timeout, Replay Number Rollover | Cor Status |
| **Uncorrectable Non-Fatal** | Poisoned TLP, Completion Timeout, Completer Abort, Unexpected Completion | UnCor Status (non-fatal) |
| **Uncorrectable Fatal** | DLP, Surprise Down, Flow Control Protocol Error, Malformed TLP | UnCor Status (fatal) |

```
# Read AER state on Linux
$ sudo setpci -s 01:00.0 ECAP_AER+0x10.L     # Uncor Status
$ sudo setpci -s 01:00.0 ECAP_AER+0x14.L     # Uncor Mask
$ sudo setpci -s 01:00.0 ECAP_AER+0x1C.L     # Cor Status
```

**Validation rule:** AER must be **silent under correctly programmed traffic.** Any AER bit set during a clean test is a bug — usually a TLP that violates ordering, flow-control, or completion-timeout rules.

The val team writes:
- One row per AER error type × per error-injection scenario.
- An assertion that AER `Status` is clear at end of every functional test.
- An error-injection harness (often via dedicated debug regs) that exercises every reporting path.

### 5. Compliance testing — the spec-defined toolbox

PCIe-SIG defines a **compliance pattern** (CPAT) and a set of **compliance test sequences** that the IP must pass to legally claim PCIe compliance.

Test categories:
- **Electrical (PHY):** eye-mask at the spec compliance point, jitter tolerance, common-mode noise.
- **Protocol (DLL/TL):** TS1/TS2 format, DLLP timing, replay buffer behaviour, flow-control credit accounting.
- **Configuration:** all required caps present, sized correctly.

Compliance pattern is requested via TS1's compliance bit; the link goes to `Polling.Compliance` and the TX drives the CPAT continuously, allowing a scope to measure the eye.

**Validation lab uses:**
- PCIe-SIG GoldenFinger / PCIe protocol test card.
- Vendor compliance tool (Lecroy, Teledyne).
- Eye-mask analyser at the compliance point on the channel.

### 6. Traffic stress — the functional regression

After bring-up, the IP val team runs **traffic stress**:

| Pattern | Purpose |
|---|---|
| **Sequential reads/writes** | Baseline throughput |
| **Random TLP sizes** | TLP-buffer + flow-credit stress |
| **Read-completion stress** | Many outstanding NPNP, completion-credit close to zero |
| **Posted vs non-posted mix** | Ordering rule coverage |
| **Cross-traffic between multiple endpoints** | Arbitration fairness |
| **Error injection mid-traffic** | Recovery + replay |
| **PM transitions mid-traffic** | L1 entry with pending TLPs |
| **Link-speed change mid-traffic** | Recovery without TLP loss |
| **Hot-reset / FLR mid-traffic** | Reset semantics |

Each is a coverage row; scoreboard tracks bytes-in == bytes-out, ordering preserved, no AER.

### 7. SI (Signal Integrity) and eye-margin

For a real product, the val team works alongside the SI team:

- **Eye opening at the receiver pad** must clear the spec mask at the **compliance point.**
- **Loss budget** is shared across PCB + connector + cable + Re-driver/Re-timer.
- **Per-lane skew** must be under spec.

Bring-up issues caused by SI usually show as:
- Stuck in `Recovery.RcvrLock` at higher speeds (eq margin gone).
- High `Receiver Error` AER rate.
- L0p loops at sustained traffic.

Fixes are **outside** the IP — board layout, Re-timer addition, connector change. The IP val engineer needs to recognise SI symptoms so the bug is filed against the right team.

### 8. Hot-reset, FLR, Surprise Down

The IP must handle three reset / link-down events cleanly:

| Event | Source | Recovery |
|---|---|---|
| **Hot-reset** | Rootport issues TS1 with hot-reset bit set | Endpoint resets, link re-trains |
| **FLR (Function Level Reset)** | SW writes FLR bit in PCIe Cap | Function resets without link disturbance |
| **Surprise Down** | Link goes electrically idle unexpectedly | Endpoint signals SDU, fatal AER, driver re-binds |

Validation: for each event × every pending traffic class, verify clean recovery + no AER × proper SW notification.

### 9. The PCIe IP val program structure (functional, not PM)

```
P1 — Unit RTL stable
  ├─ LTSSM coverage: every state, every transition
  ├─ Config-space coverage: every register × read/write paths
  ├─ TLP layer: every TLP type, every payload size class
  ├─ DLLP layer: every DLLP, replay buffer, flow control
  └─ AER: every error type, every reporting path

P2 — Subsystem integration
  ├─ Multiple endpoints + bridge
  ├─ Real driver path (or SW model)
  └─ Traffic stress with PM transitions

P3 — SoC integration
  ├─ Rootport + switch + multiple endpoints
  ├─ Linux boot + enumeration + driver init
  └─ Long-soak

P4 — Emulation
  ├─ Boot full OS, run real workloads (NVMe IO, NIC traffic)
  └─ Compliance pattern verification

P5 — Post-Si bring-up
  ├─ Lab board with refclk + PERST# + side-band wired
  ├─ Scope + analyser + lspci
  └─ Long-soak + compliance + interop matrix
```

### 9a. What PCIe emulation covers vs cannot cover

This is the Nvidia/PythonSV-style boundary question. Answer in two columns: what emulation is excellent for, and what must wait for silicon/board.

| Area | Emulation can cover | Not covered / weak in emulation |
|---|---|---|
| **FW + driver path** | Real BIOS/UEFI/Linux enumerating RTL through transactors | Real boot-time board races from straps, PERST#, refclk stability |
| **Config / BAR / MSI** | Enumeration, cap walk, BAR sizing, MSI/MSI-X programming, AER paths | OS timing sensitivity caused by real device init latency / analog reset release |
| **TLP/DLL protocol** | TLP types, ordering, credits, replay/error injection via VIP/transactor | True electrical bit errors, jitter-driven CRC failures, marginal receiver behavior |
| **LTSSM logic** | State transitions, timeouts, directed stuck-state scenarios, illegal transitions | Real PHY training quality, CDR lock margin, lane skew, connector/cable effects |
| **Traffic stress** | Real workloads at MHz speed, long software flows, cross-IP integration | Full-rate bandwidth, thermal/power stress, SI-induced intermittent retrains |
| **Power management** | ASPM/L1/L1SS control sequencing and register programming | Actual L1.2 common-mode voltage off, CLKREQ# board routing, wake latency on real PHY |
| **Interop** | Modelled endpoints/rootports, some speed-bridge setups | Other vendors' silicon quirks and de-facto spec interpretations |

**60-sec answer:**

> "Emulation is strong for HW/FW integration: real BIOS or Linux can enumerate real RTL through a PCIe transactor, program BARs, MSI/MSI-X, AER, run drivers, and exercise LTSSM/control logic at MHz speed. It is weak for what depends on the real PHY and board: eye margin, jitter, CDR lock, equalization quality, lane skew, refclk/PERST# timing on the platform, L1.2 analog behavior, full-rate traffic, thermal/power effects, and interop with other vendors' silicon. So if a bug is protocol, config, ordering, reset sequencing, or FW/driver integration, I expect emulation to catch it. If it is SI/PI, analog PHY, compliance eye, marginal retrains, or vendor interop, I expect post-Si lab tools to catch it."

### 10. Interop matrix — the real-world test

A PCIe IP is exercised against **other vendors' silicon** to catch spec interpretations that differ across the industry:

| Endpoint | Rootport | Typical surprises |
|---|---|---|
| NVMe drive | Intel rootport | Eq settings, MSI-X vector affinity |
| 100GbE NIC | AMD rootport | Flow-control credit accounting |
| GPU | Intel rootport | ASPM L1 negotiation, BAR sizing >4GB |
| FPGA card | ARM rootport | LTR, cap chain ordering |

Each new interop pair finds at least one bug — usually in the *less validated* of the two parties. The IP val team maintains an **interop test list** and runs the matrix at each silicon stepping.

---

### Bugs in this Topic - [day36_pcie_bringup_functional_val.md](archive/day36_pcie_bringup_functional_val.md)

- **Stuck in Detect — refclk dead on one side:** customer board had wrong refclk source-select strap. *Catch: scope on refclk both sides; pre-Si had no equivalent — caught only in lab.*
- **Stuck in Polling — lane polarity reversed:** all 8 lanes had differential polarity swapped on the board; IP didn't auto-detect polarity. *Catch: spec-mandated polarity reversal coverage in pre-Si + board bring-up test card.*
- **Stuck in Recovery.Equalization at Gen4:** TX FFE range insufficient for the channel loss; eq protocol kept requesting more pre-emphasis than TX could provide. *Catch: add re-driver; pre-Si coverage of eq-coefficient-out-of-range.*
- **L0p loop under sustained traffic:** marginal SI at Gen4; receiver errors trigger retrain every ~30 s. *Catch: long-soak AER count; SI fix (board, re-timer); pre-Si SVA `p_no_retrain_during_clean_traffic` over long window.*
- **Completion timeout under heavy NPNP traffic:** completer's response-credit window narrower than master's outstanding-NPNP. *Catch: pre-Si NPNP stress; analyser capture; flow-credit cover.*
- **AER UnCor bit set during clean test:** a Malformed-TLP error fired because checker reacted to a TLP that crossed a Max_Payload boundary; turned out checker was too aggressive. *Catch: AER-must-be-silent assertion + per-error injection coverage.*
- **Hot-reset leaves driver state inconsistent:** endpoint reset cleanly, but driver didn't re-init MSI-X table → no IRQs post-reset. *Catch: hot-reset + IRQ-fire cross-coverage.*
- **FLR doesn't quiesce pending DMA:** FLR completed but a DMA in flight wrote *after* FLR claimed completion. *Catch: FLR + outstanding-DMA scenario; SVA on FLR completion gating.*
- **Lane reversal not handled:** IP advertised lane-reversal support but lane-numbering FSM had off-by-one. *Catch: directed lane-reversal scenarios across all width × lane-rev combos.*
- **Interop: NVMe vendor X fails enum on rootport Y:** vendor X's cap chain had a self-referencing extended cap; rootport Y's enumerator hung. *Catch: interop matrix; cap-chain-loop SVA.*

---

### Interview Explanation (60-90 sec) - [day36_pcie_bringup_functional_val.md](archive/day36_pcie_bringup_functional_val.md)

> "PCIe bring-up is a ladder: refclk on, PERST# released, the LTSSM walks Detect → Polling → Configuration → L0 at Gen1, then equalisation, then Recovery to the target speed. When a link doesn't reach L0 I work the stuck-state ladder — Detect means refclk or RX terminations; Polling means polarity or PLL lock; Recovery.Equalization means eq coefficients out of range, usually a SI fix outside the IP. The validation craft is in the assertions and the traffic stress — AER must be silent under clean traffic, every error class must have an injection scenario, every TLP/DLLP type must be exercised, every reset event (hot, FLR, surprise-down) must leave the IP and the SW in a defined state. We run compliance with the SIG GoldenFinger and eye-mask at the compliance point. We run a long-soak with AER count monitoring. We run an interop matrix against other vendors' silicon, because every new endpoint × rootport pair finds a spec-interpretation difference. The marquee post-Si bug I'd point to is the L0p loop — link in L0 briefly then back to Recovery — usually marginal SI causing receiver errors that trigger retrain; the fix is board-level (re-timer) but the val signature is a slow rise in AER correctable count plus periodic re-train events on the analyser. The pre-Si rule that prevents the next variant is an SVA over a long window: no retrain during a clean traffic burst."

---

### Architect Drills - [day36_pcie_bringup_functional_val.md](archive/day36_pcie_bringup_functional_val.md)

*PCIe bring-up failures have a structured causal graph. Reason without an answer key.*

**D1. Link is stuck in Polling.Active. Reason about how to decide between TX/RX swap, polarity reversal, and PLL lock as the most likely cause.**
*Constraints to weigh:* what each fault makes the analyser show, what board-level evidence (probe RX termination) reveals, ordered cost of each remediation.

**D2. Gen3+ eq doesn't converge — reason about whether the bug is in TX, RX, or the channel.**
*Constraints to weigh:* eq request/response visibility on analyser, eye captured at TX vs RX pad, what changes when a re-driver is added to the channel.

**D3. AER correctable count is climbing at 100/s under traffic. Reason about whether this is acceptable.**
*Constraints to weigh:* spec allows correctable (link still works), what it predicts about future uncorrectable rate, customer SLA, whether SI margin is being eroded.

**D16. The link is in L0 but a `setpci` read returns all-1s. Reason about the next 3 diagnostics.**
*Constraints to weigh:* device cfg vs MMIO access path differences, AER state, link width at L0 (could be 1 instead of 16), driver vs raw access.

**D17. FLR completed but the device is still issuing DMA. Reason about where the FLR completion check failed.**
*Constraints to weigh:* spec says FLR must quiesce inflight transactions before signalling completion, IP FSM may have signaled completion at wrong point, validation gap.

**D18. The interop matrix finds a bug with one vendor's endpoint only. Reason about how to decide if it's our bug or theirs.**
*Constraints to weigh:* spec wording vs implementation interpretation, what other rootports do with the same endpoint, what other endpoints do on our rootport, who has the larger market share (and thus dictates the de-facto interpretation).

**D19. A pre-Si vplan covered every LTSSM state but post-Si saw L0p loops not seen in sim. Reason about what's missing.**
*Constraints to weigh:* long-window properties not expressible at sim runtime, SI/electrical reality absent from sim, what assertion (over many cycles) would catch the symptom in emul.

---

### Exit Check - [day36_pcie_bringup_functional_val.md](archive/day36_pcie_bringup_functional_val.md)

- [ ] Drew the 10-step bring-up ladder.
- [ ] Recited the LTSSM stuck-state diagnostic table.
- [ ] Explained Gen3+ equalisation in 4 phases.
- [ ] Stated when L0p indicates SI vs IP bug.
- [ ] Named 6 AER error classes and stated the "AER silent under clean traffic" rule.
- [ ] Stated 3 reset events (hot, FLR, surprise-down) and recovery semantics.
- [ ] Named the post-Si tools used to capture LTSSM + TLP + eye + AER.
- [ ] Explained why an interop matrix is necessary even after compliance passes.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S10](s10_pcie_linkup.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S12](s12_dma_doorbells_interrupts.md)

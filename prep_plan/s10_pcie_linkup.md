# S10 - PCIe boots: link-up

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [D33 PCIe fundamentals/LTSSM](archive/day33_pcie_fundamentals_ltssm.md); link-training part of [D36 PCIe bring-up](archive/day36_pcie_bringup_functional_val.md).
- **Must answer:**
  - The three layers (TL/DLL/PHY), and which one produces TLPs, DLLPs, and ordered sets.
  - The sequence: power-good + refclk → **PERST# deassert** → **Detect → Polling → Configuration → L0** → DL_Active → speed change and equalization.
  - Where links get stuck (Detect, Polling, Recovery loops), and what you read (LTSSM state, link status, AER).
  - The post-reset wait and **CRS** ("not ready yet"). Why does it cause boot hangs?
- **Work link:** DSA is an on-die RCiEP with no external link to train. What *does* it still need from reset and power to be visible? Compare it with a discrete card.
- **Career trace:** how your Intel PCIe transactor drove TLPs, and how you found the first mismatch.
- **Handoff →** The link is in L0. How does firmware find out what the device is and give it addresses?

## Study notes

### Concept - [day33_pcie_fundamentals_ltssm.md](archive/day33_pcie_fundamentals_ltssm.md)

PCIe is a **3-layer point-to-point serial fabric**: physical (lanes, encoding, equalisation), data link (LCRC, ACK/NAK, flow control credits), transaction (TLPs — posted MWr, non-posted MRd/CfgRd, completions). Every link runs an **LTSSM** state machine: `Detect → Polling → Config → L0 → Recovery / L0s / L1 / L1.1 / L1.2 / L2`. Software talks to the fabric through **config space** at **ECAM** addresses computed from `BDF`, **BARs** that publish MMIO windows, and a **capability list** for power, MSI/MSI-X and PCIe-extended features. Gen6 introduces **PAM4 + FEC + 256 B FLITs + L0p**.

### Deep Study Notes - [day33_pcie_fundamentals_ltssm.md](archive/day33_pcie_fundamentals_ltssm.md)

### 1. Topology and vocabulary

```
        ┌───────────────┐
        │ CPU / Root    │
        │ Complex       │
        └─┬───────┬─────┘
          │       │
       Root    Root
       Port 0  Port 1
          │       │
       NVMe    PCIe Switch
       (EP)   ┌──┬──┐
              │  │  │
             GPU NIC NPU
             (EP) (EP) (EP)
```

- **Root Complex** — CPU side, hosts root ports.
- **Endpoint (EP)** — leaf device.
- **Switch** — packet router (one upstream port, multiple downstream).
- **Link** — two PCIe ports facing each other.
- **Lane** — one differential pair each direction (TX+RX). Links are x1 / x4 / x8 / x16.

Every device is identified by a **BDF** (Bus:Device:Function), 8/5/3 bits → 256 buses × 32 devices × 8 functions.

### 2. Physical layer

| Gen | Rate (GT/s/lane) | Encoding | Per-lane BW | x16 |
|---|---|---|---|---|
| Gen1 | 2.5 | 8b/10b | 250 MB/s | 4 GB/s |
| Gen2 | 5.0 | 8b/10b | 500 MB/s | 8 GB/s |
| Gen3 | 8.0 | 128b/130b | ~1 GB/s | 16 GB/s |
| Gen4 | 16 | 128b/130b | ~2 GB/s | 32 GB/s |
| Gen5 | 32 | 128b/130b | ~4 GB/s | 64 GB/s |
| Gen6 | 64 | PAM4 + FEC + FLIT | ~8 GB/s | 128 GB/s |

- **8b/10b → 128b/130b** at Gen3 dropped encoding overhead from 20% to ~1.5% so doubling throughput at 8 GT/s was achievable.
- **PAM4** at Gen6: 4 voltage levels = 2 bits/symbol, doubles rate at same symbol clock but cuts noise margin to 1/3 — mandates **FEC**.
- **No clock wire** — the receiver runs **CDR** (Clock and Data Recovery); encoding guarantees enough transitions for the PLL to lock.
- **Equalisation (Gen3+):** Tx pre-cursor / cursor / post-cursor coefficients + Rx CTLE / DFE compensate for PCB channel loss. Negotiated in LTSSM Recovery in 4 phases.

### 3. Data link layer — reliability and flow control

- **LCRC** appended to every TLP; receiver checks.
- **Sequence number** per TLP for replay.
- **ACK / NAK** DLLPs — sender's replay buffer holds TLPs until ACK; bad LCRC → NAK → retransmit.
- **Flow Control credits:** receiver advertises buffer credits per traffic class (posted/non-posted/completion × header/data). Sender decrements per TLP, stops at zero. Receiver replenishes via **UpdateFC** DLLPs.

Failure mode: link looks alive but no traffic moves — credit return broken. Symptom: `LinkUp = 1, throughput = 0`.

### 4. Transaction layer — TLP types

| TLP | Direction | Posted? | Latency | Use |
|---|---|---|---|---|
| **MWr** (Memory Write) | Requester → Target | **Posted** | tens of ns | doorbell |
| **MRd** (Memory Read) | Requester → Target | non-posted | ~hundreds ns | poll register |
| **CfgRd / CfgWr** | Root → EP | non-posted | ~µs | config space |
| **Cpl / CplD** | Target → Requester | — | — | response to non-posted |
| **Msg** | various | posted | — | interrupts, power events |

**Posted = fire-and-forget.** **Non-posted = wait for completion.** Doorbells are MWr (posted) so they don't stall the host. Reads serialise — bursts of CfgRd at probe time can dominate boot wall-time.

### 4a. How a PCIe write actually happens

This is the execution trace to say when asked "how does a PCIe write happen?"

```
driver writes *(volatile uint32_t *)(BAR + doorbell_off) = value
    → CPU store enters store buffer
    → page tables / PAT mark the BAR as MMIO / device memory
    → uncore/root complex accepts the transaction
    → RC packetizes it as a posted Memory Write TLP
    → TLP carries address, length, requester ID, byte enables, payload
    → data link adds sequence number + LCRC, consumes posted credits
    → PHY sends it over lanes while link is in L0
    → endpoint decodes BAR hit and updates register / doorbell
```

**Key correctness points:**
- MMIO writes must target an enumerated BAR window; if bridge windows are wrong, the write never reaches the EP.
- A posted Memory Write has no completion; ordering is enforced with device-memory attributes and barriers, not a return value.
- Descriptor writes to memory must be visible before the doorbell MWr. The standard sequence is **write descriptors → memory barrier / flush as needed → MMIO doorbell write**.
- If debugging, look at `lspci` BAR assignment, bridge memory windows, AER, root-port counters, protocol-analyser TLP trace, and endpoint doorbell CSR.

**60-sec answer:**

> "A PCIe write usually starts as a CPU MMIO store to an address inside the endpoint's BAR. Enumeration assigned that BAR and programmed bridge windows so the root complex knows where to route it. The CPU/uncore treats the address as device memory, the root complex converts the store into a posted Memory Write TLP with address, byte enables and payload, the data link layer adds sequence/LCRC and consumes posted credits, and the PHY sends it in L0. The endpoint decodes the BAR hit and updates its register, often a doorbell. Because it is posted there is no completion, so the validation risks are ordering and reachability: descriptors must be visible before the doorbell, and bridge/BAR programming must route the TLP to the device."

### 5. TLP header essentials

A 3- or 4-DW header carries: Type, Requester ID (BDF), Tag (matches Cpl to MRd), Address, Length (in DW), Traffic Class. **Tag** is the key field for matching responses; out-of-order completions are normal.

### 6. Config space and ECAM

```
0x000  Standard Header (64 B) — Type 0 EP or Type 1 bridge
0x040  Capabilities List (linked list, head ptr at 0x34)
0x100  Extended config (AER, SR-IOV, LTR, L1 PM Substates) — only via ECAM
0xFFF
```

Key Type 0 registers:

| Off | Reg | Purpose |
|---|---|---|
| 0x00 | Vendor / Device ID | who, which model |
| 0x04 | Command | enable Mem / IO / Bus Mastering |
| 0x06 | Status | error flags, cap list present |
| 0x10-0x24 | BAR0-BAR5 | MMIO windows |
| 0x34 | Cap Pointer | head of capability list |

**ECAM address** (memory-mapped config space):

```
addr = ECAM_base | (bus << 20) | (dev << 15) | (func << 12) | (reg & 0xFFF)
```

Every PCIe config access uses this formula. Burn it in. Vendor ID == `0xFFFF` on read = no device — that's why `0xFFFF` is reserved as invalid.

### 7. BAR sizing

```
1. Save BAR
2. Write 0xFFFFFFFF
3. Read back → low bits cleared = size (mask off type bits, two's-complement)
4. Restore (or assign new address)
```

BAR encoding: bit 0 = (1 = I/O, 0 = memory). For memory, bits [2:1] = 00 → 32-bit, 10 → 64-bit (next BAR holds upper half). 64-bit lets the OS place big devices anywhere in physical address space.

### 8. Capability list walk

Linked list at offset 0x34: each entry has `[ID, Next, body]`. Walk until `Next == 0`. Common entries: PCIe Capability (link width/speed), Power Management, MSI / MSI-X, vendor-specific. Linked list (vs fixed table) means devices only allocate space for what they implement.

### 8a. Enumeration — how the host discovers the topology

Enumeration is the **depth-first walk of the PCIe tree** that BIOS/UEFI (and again the OS) does at boot. The host owns the bus-number assignment; the fabric is silent until it's asked.

**Algorithm (recursive, root → leaves):**

```
enumerate(bus):
  for dev in 0..31:
    for fn in 0..7:
      vid = cfg_read16(bus, dev, fn, 0x00)
      if vid == 0xFFFF: continue            # no device
      hdr = cfg_read8 (bus, dev, fn, 0x0E)  # Header Type
      if (hdr & 0x7F) == 0x01:              # Type 1 = bridge
         secondary = next_free_bus++
         cfg_write8(bus,dev,fn, 0x19, secondary)   # Secondary Bus #
         cfg_write8(bus,dev,fn, 0x1A, 0xFF)        # Subordinate (temp)
         enumerate(secondary)                       # recurse
         cfg_write8(bus,dev,fn, 0x1A, last_bus)    # tighten Subordinate
      else:                                  # Type 0 = endpoint
         size_and_assign_BARs(bus,dev,fn)
      if fn == 0 and !(hdr & 0x80): break    # not multi-function
```

**Three things the host is doing in one pass:**
1. **Probe** every (bus, dev, fn) by reading Vendor ID. `0xFFFF` = nothing there.
2. **Number the buses** — each bridge gets `Primary / Secondary / Subordinate` written so config TLPs can be routed (a Type 1 forwards a CfgRd whose target bus is in `[Secondary, Subordinate]`).
3. **Size and place BARs** — write all-1s, read back, compute size, allocate from the platform's MMIO pool, write the assigned base.

**OS re-enumeration** repeats the walk but usually **trusts BIOS bus numbers** unless `pci=assign-busses` / `pci=realloc` is set. Hot-plug runs the same code on a single sub-tree.

### 8b. "What's connected?" — the four fields a validator reads first

The host has **no out-of-band knowledge** of what sits behind a port. Everything it knows comes from four config-space fields:

| Off | Field | Width | What it tells you |
|---|---|---|---|
| 0x0E | **Header Type** | 1 B | bit[6:0] = `0x00` Endpoint / `0x01` PCI-PCI Bridge / `0x02` CardBus.  bit[7] = multi-function |
| 0x0B | **Class Code** | 1 B | top-level class (Storage, Network, Display, Bridge, …) |
| 0x0A | **Sub-Class** | 1 B | within-class kind (NVMe vs SATA vs SCSI) |
| 0x09 | **Programming IF** | 1 B | register-level interface (NVMe vs AHCI vs IDE legacy) |
| 0x00 / 0x02 | **Vendor / Device ID** | 2 B + 2 B | exact silicon — looked up in `pci.ids` |

**Worked examples (the ones that come up in interviews):**

| Class : Sub : ProgIF | Means | Driver picks |
|---|---|---|
| `01 : 08 : 02` | **NVMe** controller | `nvme.ko` |
| `01 : 06 : 01` | SATA / AHCI | `ahci.ko` |
| `02 : 00 : 00` | Ethernet NIC | vendor NIC driver (e.g. `ixgbe`) |
| `03 : 00 : 00` | VGA-compatible GPU | `i915` / `amdgpu` / `nouveau` |
| `06 : 04 : 00` | **PCI-to-PCI Bridge** (also Header Type 0x01) | `pcieport` |
| `0C : 03 : 30` | xHCI USB 3 controller | `xhci_hcd` |
| `12 : 00 : 00` | Processing accelerator (NPU class) | vendor EP driver |

**Routing follow-on (validator angle):**
- A **Type 1** header also exposes `Memory Base / Limit` and `Prefetchable Memory Base / Limit` (offsets 0x20–0x2C). Those windows are how a bridge knows which downstream MMIO addresses to forward. **Mis-sized bridge windows = endpoint enumerates fine but its BAR is unreachable** — a classic post-Si bring-up surprise.
- An **Endpoint** with `Multi-Function = 1` may expose unrelated functions (e.g. NIC fn0 + management-controller fn1). The host must walk all 8 functions, not stop at fn0.
- The **PCIe Capability** structure (cap ID 0x10) carries `Device/Port Type` (RC-Endpoint, Root-Port, Upstream-Switch, Downstream-Switch, RC-Integrated-Endpoint). Two devices can both be Class `01:08:02` but one is an **Integrated EP** (no link to train) and the other is a real link — LTSSM bugs apply only to the second.

### 8c. Where enumeration fails (validator's lens)

Things to instrument in a pre-Si or post-Si enumeration test:

| Symptom | Most-likely root cause | First field to inspect |
|---|---|---|
| EP missing from `lspci` | Link not at L0; or LTSSM stuck Polling/Config | LTSSM state CSR; Vendor ID = `0xFFFF` confirms |
| EP visible, BAR reads `0xFFFFFFFF` | BAR not assigned, or bridge window doesn't cover it | Bridge `Memory Base/Limit` vs EP BAR |
| Bridge Subordinate < real depth | Recursive walk truncated; downstream EPs invisible | Bridge `Subordinate Bus Number` (0x1A) |
| Class code reads as `00:00:00` | EP firmware not done initialising at probe time — racey reset/PERST# de-assert | Read again after a known delay; check PERST# timing vs `Trhfa` |
| Multi-function EP only fn0 visible | Header Type bit[7] not set; host short-circuits the fn-loop | `cfg_read8(0x0E)` bit 7 |
| `0xFFFF` on a function the spec says exists | Function-Level Reset still in progress, or strap pin wrong | Status reg + reset-status capability |
| Wrong driver binds | Class/Sub/ProgIF wrong in EP fuse/strap | `lspci -nn` Vendor:Device + class triplet |
| Enumeration takes 30 s | CfgRd burst per probe pass; no caching of capability walk | `dmesg` time-stamps between PCI prints |
| Hot-plug add: EP appears but drivers don't load | uevent fires before BAR assignment finishes | `udevadm monitor` + bridge window programming |
| AER `Unsupported Request` during walk | CfgRd to a function that returns UR instead of all-1s on absent fn (spec violation) | AER log + which BDF |

**The line that lands:** "Enumeration is just a depth-first config-read walk that uses Vendor ID = `0xFFFF` as the absent-marker, Header Type to split bridge vs endpoint, and Class:Sub:ProgIF to pick the driver. Most enumeration bugs are not in the walk — they're in **what the EP exposes too early or too late** relative to PERST#, FLR, or BAR assignment."

### 9. LTSSM — the link-bring-up state machine

```
                 ┌──────────┐
       ┌────────►│ Detect   │◄─────────┐
       │         └────┬─────┘          │
       │              ↓                │
       │         ┌──────────┐          │
       │         │ Polling  │          │
       │         └────┬─────┘          │
       │              ↓                │
       │         ┌──────────┐          │
       │         │ Config   │          │
       │         └────┬─────┘          │
       │              ↓                │
       │         ┌──────────┐    ┌─────┴──────┐
       │    ┌───►│   L0     │◄──►│  Recovery  │
       │    │    └─┬─┬──┬───┘    └────────────┘
       │    │      │ │  │
       │    │      ↓ ↓  ↓
       │    │   L0s L1 L1.1/L1.2
       │    │      │  │
       │    └──────┘  │
       │              ↓
       │         ┌──────────┐
       └─────────│   L2     │  (power off)
                 └──────────┘
```

- **Detect:** Tx checks Rx termination (~100 Ω diff). Stuck here = physical / refclk / termination bug.
- **Polling:** symbol lock + lane polarity + byte alignment (COM symbol) at Gen1. Stuck here = signal-integrity.
- **Config:** negotiate width (x16 vs x4 etc.) and assign lane numbers. Width downgrade here (x16 trained as x4) = bad lane(s).
- **L0:** TLPs flow.
- **Recovery:** re-train without full reset. Triggers: speed change (Gen1 → GenX), bit errors, exit from L0s/L1. Frequent Recovery = signal integrity.
- **L0s:** per-lane idle, ~1 µs autonomous exit (FTS sequences).
- **L1:** whole-link, PLL off, ~2-4 µs exit through Recovery.
- **L1.1:** L1 + CLKREQ# de-asserted → board can gate refclk.
- **L1.2:** L1.1 + Tx common-mode voltage off; only Vaux. ~32-64 µs exit. **PkgC6 requires this.**
- **L2:** power off.

**Speed negotiation:** every link starts at Gen1, advertises max in TS1/TS2, enters Recovery, equalises at higher speed (4 phases), commits if EQ succeeds; otherwise downgrades.

### 10. PCIe Gen6 changes

| Feature | Gen5 | Gen6 |
|---|---|---|
| Encoding | 128b/130b | 256-byte **FLIT** (Fixed Length Information Token) |
| Error handling | LCRC + ACK/NAK | per-FLIT FEC + per-FLIT ACK/NAK |
| TLP framing | variable, start/end markers | TLPs packed into FLITs with padding |
| Lane scaling | fixed at L0 | **L0p** — drop lane count on the fly (tens of ns) |

**Why FLITs?** PAM4's higher BER would collapse throughput under per-TLP retransmit; FEC corrects most errors in-line. Fixed-length FLITs simplify FEC block alignment.

**L0p** solves what L0s/L1 don't: workloads with low-but-constant traffic (background DMA, polling) benefit from keeping the link "on" but at reduced lane count, avoiding L1 entry/exit cost.

### 11. Why PkgC6 needs L1.2 (forward link to Day 27/14)

- **L1** alone: PLL off but common-mode bias still drawn → platform rail can't fully drop.
- **L1.1**: refclk gated, but common-mode still on.
- **L1.2**: common-mode off, only Vaux → platform package idle assertion can succeed → PkgC6 entry.

If your endpoint doesn't advertise L1.2 in the L1 PM Substates capability, PkgC6 cannot enter. **#1 PkgC6 failure root cause in real bring-ups.**

---

### Bugs in this Topic - [day33_pcie_fundamentals_ltssm.md](archive/day33_pcie_fundamentals_ltssm.md)

- **Link stuck in Polling:** refclk wrong / receiver termination wrong / lane polarity not corrected. Polling is bit-level sync — almost always physical/electrical.
- **Width downgrade in Config (x16 trains as x4):** faulty lanes. Look at LTSSM lane status and lane error counters.
- **Frequent Recovery entries during traffic:** signal integrity (eye margin too small). Look at PCIe AER, FEC error counter (Gen6), Recovery counter; check Tx coefficients selected by EQ.
- **Link up but no traffic:** flow-control credit return broken (UpdateFC DLLPs lost). Check FC credit counters per VC.
- **Vendor ID reads `0xFFFFFFFF` after a power-state transition:** link down. Re-read after PHY/LTSSM recovery; if still 0xFF — endpoint didn't survive the PM transition.
- **PkgC6 never entered:** endpoint missing L1.2 in L1 PM Substates; or platform CLKREQ# routing wrong; or LTR setting too tight. Day 27 anchor.
- **`DSTRD` miscomputed on NVMe (forward link to Day 23):** doorbell goes to wrong offset; controller never sees the kick.
- **CfgRd burst dominates boot time:** every probe pass re-reads config; cache it once.
- **Bridge `Memory Base/Limit` mis-sized during enumeration:** EP enumerates fine, but its BAR sits outside the bridge window — every MMIO load from the host returns `0xFFFFFFFF` even though `lspci` shows the device. Validator hook: read bridge window after enum and verify it contains every downstream BAR.
- **Class code reads `00:00:00` at probe time:** EP firmware still initialising when host issues first CfgRd — PERST# de-assert too close to first config access (`Trhfa` violated). Re-probe after spec delay.
- **L0p re-scaling races a TLP in flight (transactor bug story — see Day 35 Story 6):** lane-count change committed mid-FLIT; receiver de-skewed against the old width and dropped the partial FLIT. Symptom: link stays in L0 but FEC error counter spikes coincident with every L0p transition.
- **Multi-function EP — host stops at fn0:** Header Type bit[7] (multi-function) not set in EP straps; fn1…fn7 invisible to OS even though present.

---

### Interview Explanation (60-90 sec) - [day33_pcie_fundamentals_ltssm.md](archive/day33_pcie_fundamentals_ltssm.md)

> "PCIe is a 3-layer point-to-point serial fabric. The physical layer carries differential lanes at 2.5 to 64 GT/s with 8b/10b through Gen2, 128b/130b Gen3-5, PAM4 plus FEC at Gen6, and no clock wire — the receiver does CDR off the data stream. The data link layer adds LCRC, ACK/NAK retry, and flow-control credits — link up but no traffic almost always means credit return is broken. The transaction layer carries TLPs: posted memory writes for doorbells, non-posted memory reads and config reads, completions, messages. Every link runs an LTSSM: Detect, Polling, Config, L0, Recovery for retrain or speed change, L0s/L1/L1.1/L1.2 for power. PkgC6 specifically needs L1.2 because that's where common-mode voltage drops and the platform rail can collapse. Software sees the fabric through config space at ECAM addresses computed from BDF, BARs that publish MMIO windows, and a linked-list capability chain. Gen6 also adds 256-byte FLITs and L0p for dynamic lane scaling — a perf knob L1 entry/exit can't hit at the same latency."

---

### Architect Drills - [day33_pcie_fundamentals_ltssm.md](archive/day33_pcie_fundamentals_ltssm.md)

*PCIe is a layered protocol whose every choice (3 layers, posted writes, 11-state LTSSM, train-up-from-Gen1) is a tradeoff. Derive why — don't recall. No answer key.*

**D1. Why three layers (PHY/DLL/TL) and not two or four? Reason from what each layer isolates.**
*Constraints to weigh:* what fault each layer catches (bit error / packet drop / protocol error), where retry is cheapest, debug-tooling vocabulary (LTSSM vs ACK/NAK vs UR), what would collapse if PHY+DLL merged. Compare to Ethernet's 7-layer OSI — why does PCIe stop at three?

**D2. Why did PCIe move from 8b/10b → 128b/130b at Gen3 instead of further to 256b/258b?**
*Constraints to weigh:* encoding overhead vs DC balance, run-length limits, CDR lock-time, cost of larger framing units at higher rates. What does PAM4+FEC at Gen6 implicitly say about where this trend dead-ends?

**D3. Posted memory writes are fire-and-forget; reads block. Reason from first principles why doorbell-as-MWr won the design.**
*Constraints to weigh:* round-trip latency × IOPS, ordering vs response coupling, bandwidth amortisation, what "completion" would even mean for a doorbell. Construct the workload where non-posted doorbells would be safer, even at the perf cost.

**D16. LTSSM has 11 states. Why so many — could it be expressed in 4?**
*Constraints to weigh:* each state isolates a different failure mode (refclk, EQ, polarity, FC credit, power), debug-protocol exposure, link-power transitions need their own state space. Which states are vestigial vs load-bearing in 2025?

**D17. PCIe trains at Gen1 first then negotiates up. Reason about why no spec ever just "starts at the highest rate."**
*Constraints to weigh:* receiver equalisation needs a known training sequence, cross-vendor interop matrix, downshift recovery on signal-integrity failure, LTSSM Recovery as the universal back-channel. What would a "start-at-Gen6" spec sacrifice?

**D18. Flow-control credits — why aren't they implicit (just stop sending if you don't get an ACK)?**
*Constraints to weigh:* receive-buffer sizing visibility, head-of-line blocking, per-VC isolation, protocol overhead of credit-return DLLPs. What did PCI-X's bus-mastering retry teach the PCIe spec authors that they encoded into the credit scheme?

**D19. Imagine PCIe's successor lets devices share CPU coherence (CXL.cache). Which LTSSM/TLP artefacts survive, and which are obsoleted?**
*Constraints to weigh:* the PHY and DLL stay (they're the wire), TL might evolve for cache-line semantics, LTSSM still needs power states and equalisation, doorbell-as-MWr loses to coherent loads/stores. What new failure mode appears for `.cache` that PCIe never had?

---

### Exit Check - [day33_pcie_fundamentals_ltssm.md](archive/day33_pcie_fundamentals_ltssm.md)

- [ ] Drew the 3-layer stack and named one TLP per type with posted/non-posted.
- [ ] Walked LTSSM Detect → L0 and named one stuck-symptom per state.
- [ ] Stated L1 vs L1.1 vs L1.2 differences and PkgC6 dependency.
- [ ] Computed an ECAM address and a BAR size from readback.
- [ ] Stated 8b/10b → 128b/130b → PAM4 + FEC + FLIT progression.
- [ ] Explained L0p's niche vs L1.

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

Previous: [S09](s09_coherence_ordering.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S11](s11_pcie_enumeration_driver.md)

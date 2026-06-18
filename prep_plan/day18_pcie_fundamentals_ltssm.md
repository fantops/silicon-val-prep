# Day 18 — PCIe Fundamentals + LTSSM + Power States

## Concept

PCIe is a **3-layer point-to-point serial fabric**: physical (lanes, encoding, equalisation), data link (LCRC, ACK/NAK, flow control credits), transaction (TLPs — posted MWr, non-posted MRd/CfgRd, completions). Every link runs an **LTSSM** state machine: `Detect → Polling → Config → L0 → Recovery / L0s / L1 / L1.1 / L1.2 / L2`. Software talks to the fabric through **config space** at **ECAM** addresses computed from `BDF`, **BARs** that publish MMIO windows, and a **capability list** for power, MSI/MSI-X and PCIe-extended features. Gen6 introduces **PAM4 + FEC + 256 B FLITs + L0p**.

## Why this day in the flow

Days 4-8 built the cache/MMU/IOMMU/DMA picture. Day 18 is the **bus that carries all that traffic**. PCIe is the substrate for NVMe (Day 11), thermal events (Day 15), accelerator submission (Day 18-19) and a huge fraction of debug (Day 20). Today is also where the Day 13 PkgC6 story lives — PkgC6 needs PCIe links in **L1.2** specifically.

In the Layer map this is **Layer 3 (Buses)**.

---

## Deep Study Notes

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

### 11. Why PkgC6 needs L1.2 (forward link to Day 13/14)

- **L1** alone: PLL off but common-mode bias still drawn → platform rail can't fully drop.
- **L1.1**: refclk gated, but common-mode still on.
- **L1.2**: common-mode off, only Vaux → platform package idle assertion can succeed → PkgC6 entry.

If your endpoint doesn't advertise L1.2 in the L1 PM Substates capability, PkgC6 cannot enter. **#1 PkgC6 failure root cause in real bring-ups.**

---

## Bugs in this Topic

- **Link stuck in Polling:** refclk wrong / receiver termination wrong / lane polarity not corrected. Polling is bit-level sync — almost always physical/electrical.
- **Width downgrade in Config (x16 trains as x4):** faulty lanes. Look at LTSSM lane status and lane error counters.
- **Frequent Recovery entries during traffic:** signal integrity (eye margin too small). Look at PCIe AER, FEC error counter (Gen6), Recovery counter; check Tx coefficients selected by EQ.
- **Link up but no traffic:** flow-control credit return broken (UpdateFC DLLPs lost). Check FC credit counters per VC.
- **Vendor ID reads `0xFFFFFFFF` after a power-state transition:** link down. Re-read after PHY/LTSSM recovery; if still 0xFF — endpoint didn't survive the PM transition.
- **PkgC6 never entered:** endpoint missing L1.2 in L1 PM Substates; or platform CLKREQ# routing wrong; or LTR setting too tight. Day 13 anchor.
- **`DSTRD` miscomputed on NVMe (forward link to Day 11):** doorbell goes to wrong offset; controller never sees the kick.
- **CfgRd burst dominates boot time:** every probe pass re-reads config; cache it once.
- **Bridge `Memory Base/Limit` mis-sized during enumeration:** EP enumerates fine, but its BAR sits outside the bridge window — every MMIO load from the host returns `0xFFFFFFFF` even though `lspci` shows the device. Validator hook: read bridge window after enum and verify it contains every downstream BAR.
- **Class code reads `00:00:00` at probe time:** EP firmware still initialising when host issues first CfgRd — PERST# de-assert too close to first config access (`Trhfa` violated). Re-probe after spec delay.
- **L0p re-scaling races a TLP in flight (transactor bug story — see Day 20 Story 6):** lane-count change committed mid-FLIT; receiver de-skewed against the old width and dropped the partial FLIT. Symptom: link stays in L0 but FEC error counter spikes coincident with every L0p transition.
- **Multi-function EP — host stops at fn0:** Header Type bit[7] (multi-function) not set in EP straps; fn1…fn7 invisible to OS even though present.

---

## Interview Explanation (60-90 sec)

> "PCIe is a 3-layer point-to-point serial fabric. The physical layer carries differential lanes at 2.5 to 64 GT/s with 8b/10b through Gen2, 128b/130b Gen3-5, PAM4 plus FEC at Gen6, and no clock wire — the receiver does CDR off the data stream. The data link layer adds LCRC, ACK/NAK retry, and flow-control credits — link up but no traffic almost always means credit return is broken. The transaction layer carries TLPs: posted memory writes for doorbells, non-posted memory reads and config reads, completions, messages. Every link runs an LTSSM: Detect, Polling, Config, L0, Recovery for retrain or speed change, L0s/L1/L1.1/L1.2 for power. PkgC6 specifically needs L1.2 because that's where common-mode voltage drops and the platform rail can collapse. Software sees the fabric through config space at ECAM addresses computed from BDF, BARs that publish MMIO windows, and a linked-list capability chain. Gen6 also adds 256-byte FLITs and L0p for dynamic lane scaling — a perf knob L1 entry/exit can't hit at the same latency."

---

## Architect Drills

*PCIe is a layered protocol whose every choice (3 layers, posted writes, 11-state LTSSM, train-up-from-Gen1) is a tradeoff. Derive why — don't recall. No answer key.*

**D1. Why three layers (PHY/DLL/TL) and not two or four? Reason from what each layer isolates.**
*Constraints to weigh:* what fault each layer catches (bit error / packet drop / protocol error), where retry is cheapest, debug-tooling vocabulary (LTSSM vs ACK/NAK vs UR), what would collapse if PHY+DLL merged. Compare to Ethernet's 7-layer OSI — why does PCIe stop at three?

**D2. Why did PCIe move from 8b/10b → 128b/130b at Gen3 instead of further to 256b/258b?**
*Constraints to weigh:* encoding overhead vs DC balance, run-length limits, CDR lock-time, cost of larger framing units at higher rates. What does PAM4+FEC at Gen6 implicitly say about where this trend dead-ends?

**D3. Posted memory writes are fire-and-forget; reads block. Reason from first principles why doorbell-as-MWr won the design.**
*Constraints to weigh:* round-trip latency × IOPS, ordering vs response coupling, bandwidth amortisation, what "completion" would even mean for a doorbell. Construct the workload where non-posted doorbells would be safer, even at the perf cost.

**D4. LTSSM has 11 states. Why so many — could it be expressed in 4?**
*Constraints to weigh:* each state isolates a different failure mode (refclk, EQ, polarity, FC credit, power), debug-protocol exposure, link-power transitions need their own state space. Which states are vestigial vs load-bearing in 2025?

**D5. PCIe trains at Gen1 first then negotiates up. Reason about why no spec ever just "starts at the highest rate."**
*Constraints to weigh:* receiver equalisation needs a known training sequence, cross-vendor interop matrix, downshift recovery on signal-integrity failure, LTSSM Recovery as the universal back-channel. What would a "start-at-Gen6" spec sacrifice?

**D6. Flow-control credits — why aren't they implicit (just stop sending if you don't get an ACK)?**
*Constraints to weigh:* receive-buffer sizing visibility, head-of-line blocking, per-VC isolation, protocol overhead of credit-return DLLPs. What did PCI-X's bus-mastering retry teach the PCIe spec authors that they encoded into the credit scheme?

**D7. Imagine PCIe's successor lets devices share CPU coherence (CXL.cache). Which LTSSM/TLP artefacts survive, and which are obsoleted?**
*Constraints to weigh:* the PHY and DLL stay (they're the wire), TL might evolve for cache-line semantics, LTSSM still needs power states and equalisation, doorbell-as-MWr loses to coherent loads/stores. What new failure mode appears for `.cache` that PCIe never had?

---

## Coding A — Systems C: ECAM walker + LTSSM tracker

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static uint64_t ecam_addr(uint64_t base, uint8_t bus, uint8_t dev,
                          uint8_t fn, uint16_t reg) {
    return base | ((uint64_t)bus<<20) | ((uint64_t)dev<<15)
                | ((uint64_t)fn<<12)  | (reg & 0xFFFu);
}

static uint64_t bar_size_from_readback(uint32_t rb) {
    uint32_t mask = (rb & 1) ? ~0x3u : ~0xFu;
    return (uint64_t)(~(rb & mask)) + 1;
}

typedef enum {
    LTSSM_DETECT, LTSSM_POLLING, LTSSM_CONFIG, LTSSM_L0,
    LTSSM_RECOVERY, LTSSM_L0S, LTSSM_L1, LTSSM_L1_1, LTSSM_L1_2, LTSSM_L2,
    LTSSM_NUM
} ltssm_t;

static const char *names[] = { "Detect","Polling","Config","L0","Recovery",
                                "L0s","L1","L1.1","L1.2","L2" };

/* Allowed-transition table: 1 = legal */
static const uint8_t ok[LTSSM_NUM][LTSSM_NUM] = {
    /*           Det Pol Cfg  L0 Rec L0s L1 L11 L12 L2 */
    /*Det*/    { 1,  1,  0,  0,  0,  0, 0,  0,  0, 0 },
    /*Pol*/    { 1,  0,  1,  0,  0,  0, 0,  0,  0, 0 },
    /*Cfg*/    { 1,  0,  0,  1,  0,  0, 0,  0,  0, 0 },
    /*L0 */    { 0,  0,  0,  0,  1,  1, 1,  0,  0, 0 },
    /*Rec*/    { 1,  0,  0,  1,  0,  0, 0,  0,  0, 0 },
    /*L0s*/    { 0,  0,  0,  1,  1,  0, 0,  0,  0, 0 },
    /*L1 */    { 0,  0,  0,  0,  1,  0, 0,  1,  1, 1 },
    /*L11*/    { 0,  0,  0,  0,  1,  0, 1,  0,  1, 0 },
    /*L12*/    { 0,  0,  0,  0,  1,  0, 0,  0,  0, 0 },
    /*L2 */    { 1,  0,  0,  0,  0,  0, 0,  0,  0, 0 },
};

typedef struct { ltssm_t st; unsigned recoveries; } link_t;

static bool go(link_t *l, ltssm_t to) {
    if (!ok[l->st][to]) {
        printf("ILLEGAL: %s -> %s\n", names[l->st], names[to]); return false;
    }
    printf("  %s -> %s\n", names[l->st], names[to]);
    if (to == LTSSM_RECOVERY) l->recoveries++;
    l->st = to;
    return true;
}

int main(void) {
    /* ECAM example */
    uint64_t a = ecam_addr(0xE0000000, 1, 2, 0, 0x10);
    printf("ECAM 01:02.0 BAR0 -> 0x%012llX\n", (unsigned long long)a);
    printf("BAR readback 0xFFF00004 size=%llu bytes\n",
           (unsigned long long)bar_size_from_readback(0xFFF00004));

    link_t link = { LTSSM_DETECT, 0 };
    go(&link, LTSSM_POLLING);
    go(&link, LTSSM_CONFIG);
    go(&link, LTSSM_L0);
    go(&link, LTSSM_RECOVERY); go(&link, LTSSM_L0); /* speed change */
    go(&link, LTSSM_L1);
    go(&link, LTSSM_L1_2);
    go(&link, LTSSM_RECOVERY); go(&link, LTSSM_L0);
    printf("recoveries=%u\n", link.recoveries);
    return 0;
}
```

---

## System Thinking — Why PCIe looks like a network

**Frame:** PCIe is the universal interconnect because it stopped pretending to be a bus. Old PCI was a parallel bus — shared, slow, doesn't scale past a few devices. PCIe is a **packet-switched, point-to-point, layered serial network** that just happens to fit inside a chassis. Every PCIe concept (TLP, LTSSM, credits, equalization) maps to something in Ethernet/IP — but optimized for a 1-meter wire and microsecond latency.

### Q: Why is PCIe layered (PHY → DLL → TLP)?

Same reason TCP/IP is layered: separation of concerns lets each layer be replaced independently. 
- **PHY** (electrical + 8b/10b or 128b/130b encoding + equalization): hardware bit transport. Replaced going from Gen1 → Gen5 → Gen6 — PHY rate doubled, upper layers unchanged.
- **Data Link Layer (DLL)**: per-hop reliability — sequence numbers, ACK/NAK, replay buffer, CRC. *This is why PCIe can use a non-FEC PHY and still be reliable.*
- **Transaction Layer (TLP)**: end-to-end semantics — memory read/write, config, message, completion. The driver lives at this layer.

Layering is also what enables **PCIe-over-cable, PCIe-over-CXL, PCIe-over-optical** — swap the PHY, keep DLL+TLP.

### Q: Why LTSSM (Link Training and Status State Machine)?

Because a multi-Gbps serial link cannot just turn on and work. The two ends must agree on:
- **Lane count** (x1, x2, x4, x8, x16) and which lanes are usable.
- **Lane reversal / polarity inversion** (the board layout may have flipped pairs).
- **Equalization coefficients** — the receiver tells the transmitter "boost this much, de-emphasize that much" so the eye opens.
- **Speed** (Gen1 → Gen2 → ... up to highest mutually supported).

All of this is a negotiation, and it's stateful with timeouts — hence a state machine. **Most boot-time PCIe bugs are LTSSM bugs:** stuck in Polling, stuck in Recovery, can't reach L0 at the expected speed. Reading the LTSSM state from a config register is the first thing you do when a device doesn't enumerate.

### Q: Why credit-based flow control instead of ACK/NAK pacing?

Because ACK/NAK pacing wastes bandwidth: the sender must wait for permission per-packet (or guess and retransmit). Credit-based flow control inverts it: the receiver advertises buffer space ("I have N credits for posted writes"), the sender consumes credits as it transmits, and the receiver returns credits as it processes. The sender can transmit *up to the credit limit* without waiting — zero idle time on a lossless link.

**Tradeoff:** requires per-VC, per-type credit tracking (huge state). But on a chassis-scale lossless link, that state is worth it. Ethernet doesn't do credits because cross-DC topology makes the credit math impossible — different design point, different choice.

### Q: Why power states (L0, L0s, L1, L1.1, L1.2)?

Because PHY and link-level power dominates idle PCIe energy on laptops. Same ladder logic as CPU C-states (Day 13): each lower state saves more, costs more wake latency. **L1.2** is the deepest — PHY off, refclk gated, ~10 μs wake. PkgC6 / S0ix on the host *requires* every PCIe link to be in L1.2 (Day 13 dependency). One device that won't enter L1.2 (bad ASPM firmware) blocks platform deep idle — visible as missing PkgC10 residency.

### One-liner

*"PCIe is a packet-switched layered serial network that fits in a chassis. PHY/DLL/TLP layering enables generation-over-generation rate doubling without breaking drivers. LTSSM exists because Gbps serial links must train, not just turn on. Credit-based flow control wins on a lossless link because it eliminates idle gaps. Power states mirror CPU C-states — deeper saves more, costs more wake — and the deepest state is gated on every link in the system."*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **PCIe layering** — *what:* PHY / DLL / TL / *why:* network-style serial replaces parallel bus / *how:* lanes + 8b10b (Gen1-2) / 128b130b (Gen3+) / *connects:* why Gen5 = 32 GT/s/lane, why driver above TL is gen-agnostic
- **LTSSM** — *what:* link training FSM / *why:* Gbps SerDes must train, not just turn on / *how:* Detect → Polling → Config → Recovery → L0 → L0s/L1/L2 / *connects:* bring-up debug, why links drop to lower gen under SI issues
- **TLP / DLLP** — *what:* transaction-layer pkt vs link-layer pkt / *why:* layered like a network stack / *how:* headers + LCRC + sequence numbers / *connects:* credit-based flow control via UpdateFC DLLPs
- **credits** — *what:* rx buffer space advertised by receiver / *why:* lossless = no drops, deterministic latency / *how:* InitFC at link-up, UpdateFC during run / *connects:* credit starvation = link hangs, not packet loss
- **enumeration** — *what:* BIOS walks bus, assigns BARs / *why:* devices need address-space windows / *how:* config-space type 0/1 reads, BDF / *connects:* ACS, hot-plug, SR-IOV VFs

## Bug Anchor

- **Day 11 (NVMe `CSTS.RDY` / `DSTRD`):** the canonical bring-up bug on top of PCIe.
- **Day 13 (PkgC6 / S0ix):** L1.2 dependency lives here.
- **Day 14 (ADR/PCU):** the firmware analogue of "doorbell before descriptor."
- **Day 20 (Debug method):** `lspci`, AER, FEC counters, protocol analyser.

## Exit Check

- [ ] Drew the 3-layer stack and named one TLP per type with posted/non-posted.
- [ ] Walked LTSSM Detect → L0 and named one stuck-symptom per state.
- [ ] Stated L1 vs L1.1 vs L1.2 differences and PkgC6 dependency.
- [ ] Computed an ECAM address and a BAR size from readback.
- [ ] Stated 8b/10b → 128b/130b → PAM4 + FEC + FLIT progression.
- [ ] Explained L0p's niche vs L1.

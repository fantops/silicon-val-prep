# Day 19 — PCIe Enumeration, Config Space, BAR Sizing, MSI/MSI-X

## Concept

PCIe enumeration is the **depth-first recursive walk** that the root complex (or BIOS) performs across the topology at boot: starting at bus 0, probe every device/function, read its config header, assign bus numbers, size + assign BARs, walk the capability chain, program MSI/MSI-X, and recurse through bridges. **Every PCIe device on the planet, from a graphics card to an NVMe to a 100GbE NIC, is found by the same algorithm.** Config space is the device's self-description; BAR sizing is the protocol by which a device tells SW how much memory window it needs; MSI/MSI-X is how it asks for an interrupt without a wire.

## Why this day in the flow

D18 gave you the link layer (LTSSM, link-up). **Today is what happens once the link comes up** — the host has to discover the device exists, decide where in memory its registers live, and wire up its interrupts. **Without enumeration, link-up is useless.** Validating enumeration is one of the largest scenario surfaces in PCIe IP val: every header type, every BAR size class, every cap, every cross with another device on the same bus.

In the Layer map this is the **second day of Phase 4 (PCIe)**, building on D18's LTSSM with the SW visibility layer.

---

## Deep Study Notes

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

## Bring-up Flow — PCIe Enumeration (Validation Lens)

```
Stage A — TB harness
   ├─ Instantiate PCIe IP (root complex or endpoint per role)
   ├─ Instantiate AXI/CHI BFM for host side
   ├─ Config-space access agent
   └─ MSI/MSI-X target agent (intercept posted writes to LAPIC range)

Stage B — Topology builder
   ├─ Configurable: # of bridges, # of endpoints per bus
   ├─ Each endpoint parameterised: header, BARs, caps, MSI/MSI-X
   └─ Generate "expected post-enum state" oracle

Stage C — Enumerator under test
   ├─ Option A: real BIOS code (slow but realistic)
   ├─ Option B: SW model of enumerator (fast, deterministic)
   └─ Drive enumerator via config-space agent

Stage D — Scoreboard
   ├─ After enum complete, walk topology and read every cfg reg
   ├─ Compare vs expected oracle
   └─ Flag any divergence

Stage E — Directed scenarios
   ├─ One per row of enumeration vplan
   └─ Including failure injections

Stage F — Random topology stress
   ├─ Constrained-random topology generator
   ├─ Cover BAR sizes, cap chains, bridge depth
   └─ Coverage feedback

Stage G — Real-OS boot
   ├─ Boot Linux with the topology under test
   ├─ `lspci -vvv` should match expected
   └─ All MSI/MSI-X allocated and fired at least once
```

---

## What's Required

- **PCIe IP RTL** (root complex and/or endpoint flavour).
- **Config-space master BFM** to drive cfg reads/writes.
- **MSI/MSI-X sink** that catches the posted-write interrupts.
- **Topology generator** parameterised over bridge depth, endpoint count, BAR profiles, cap chains.
- **Enumerator** (real BIOS code or model).
- **Scoreboard** that knows the expected post-enum state.
- **Vplan** with enumeration cells enumerated (header × BAR × cap × MSI × bridge depth × failure).
- **`lspci`/`setpci`** in the post-Si validation flow for real-OS cross-check.

---

## Tools

| Phase | Tool | Use |
|---|---|---|
| **Pre-Si sim** | VCS / Xcelium with PCIe VIP (Synopsys VC VIP, Cadence Denali) | Drive cfg + TLPs; check protocol |
| **Pre-Si formal** | JasperGold PCIe app | Prove enumeration & error-handling FSMs |
| **Emulation** | ZeBu / Palladium with PCIe transactor | Real BIOS enumerates real RTL |
| **Post-Si** | `lspci -vvv -PP` | Walk topology, dump caps, dump BARs |
| | `setpci -s BB:DD.F CMD=0x06` | Poke command/status, control bits |
| | `dmesg | grep pci` | Boot-time enumeration log |
| | `/sys/bus/pci/devices/*/resource*` | BAR maps post-enumeration |
| | `cat /proc/interrupts` | MSI/MSI-X vector assignment |
| | PCIe analyser (Lecroy Summit, Teledyne) | TLP-level capture of enum walk |

---

## Bugs in this Topic

- **64-bit BAR sized as 32-bit:** SW wrote 0xFFFFFFFF only to lower BAR; size readback masked off type bits incorrectly; device assigned wrong window. *Fix: pair BAR0+BAR1 read/write; check bit-2 for 64-bit.*
- **MSI-X table BAR too small:** topology declared 64-vector MSI-X (1 KB table) but the BAR holding it was 256 B. *Fix: SW model must round BAR size up to enclose table+PBA.*
- **Cap chain Next Ptr = own offset:** infinite loop on enumerator. *Fix: cap-chain walker bounds check + SVA on RTL.*
- **Multifunction bit ignored:** functions 1-7 of a multifunction device never probed. *Fix: enumerator reads `header_type & 0x80`; covered by vplan multifunction cross.*
- **Subordinate-bus narrow:** bridge-of-bridge devices invisible. *Fix: depth-first walk computes subordinate on return.*
- **BAR write during link-down race:** enumerator started before LTSSM reached L0. *Fix: gate enum start on link-up; SVA `p_no_cfg_before_l0`.*
- **MSI message-data wrong vector encoding:** multi-msg of 4 used bits [1:0] but device hardware decoded [2:1]. *Fix: spec ambiguity → directed test + LAPIC-target scoreboard.*
- **Bridge prefetchable window mis-typed:** non-prefetchable BAR assigned to prefetchable window → MMIO coherence broken. *Fix: enumerator separates pref vs non-pref pools.*

---

## Interview Explanation (60-90 sec)

> "PCIe enumeration is a depth-first recursive walk: starting at bus 0, you read each function's vendor ID, if it exists you read the header, size the BARs using the write-all-1s trick to discover alignment and size, walk the capability chain by following next-pointers, program MSI or MSI-X by allocating addresses in the LAPIC range and writing them into the device's table, and if it's a bridge you assign a secondary bus number and recurse. On the way back up you write the subordinate bus number so the bridge knows its forwarding range. MSI is up to 32 vectors sharing one address with the data field varying; MSI-X scales to 2048 vectors each with its own address/data pair stored in a table inside one of the device's BARs. Every step here is a validation row: header types, BAR size classes, cap chain orders, MSI-X table-in-BAR sizing, bridge nesting depth, plus failure injections like UR responses and malformed cap chains. The IP val team builds a topology generator, runs the enumerator under test (real BIOS in emul, a model in sim), and scoreboards against an expected post-enum state. The bugs that escape this here are the ones that surface as 'device works in slot 1 but not slot 2' — usually bridge window or sub-bus bookkeeping."

---

## Architect Drills

*Enumeration is a tree-walk algorithm meeting hardware constraints. Reason without an answer key.*

**D1. Why is enumeration depth-first rather than breadth-first?**
*Constraints to weigh:* bridge bus-number assignment dependency, subordinate-bus propagation, resource (BAR window) accounting, memory cost of the algorithm's stack.

**D2. BAR sizing uses write-all-1s. Reason about what happens if the device is actively servicing traffic during the size probe.**
*Constraints to weigh:* config writes are out-of-band relative to MMIO traffic, transient mis-mapped BAR, what real BIOS code does to protect against this.

**D3. MSI vs MSI-X — when would a modern device choose MSI?**
*Constraints to weigh:* vector count limits, table memory cost, SW affinity flexibility, legacy compatibility, IP complexity.

**D4. Reason about a topology with 8 levels of bridge nesting. What goes wrong before what?**
*Constraints to weigh:* bus-number space (256 buses), BAR window aggregation, latency through bridges, real silicon (almost never sees this).

**D5. The cap chain points back into itself (loop). Reason about whether the SW or HW should be responsible for catching this.**
*Constraints to weigh:* HW assertion vs SW bounds-check, what each catches in real life, escape if HW is "correct" but the chain SW programmed is wrong.

**D6. A 64-vector MSI-X device wants per-CPU queue affinity. Reason about where the val team's coverage must intersect with the host IRQ subsystem.**
*Constraints to weigh:* per-vector address (one per CPU LAPIC), Linux `irq_set_affinity`, NUMA implications, hot-add CPU paths.

**D7. Enumeration coverage is 100% in pre-Si; post-Si finds a bug where a specific endpoint behind a specific bridge fails to enumerate when the link comes up at 2 instead of 3. Reason about what was missing.**
*Constraints to weigh:* link-speed × enum-timing cross, what TLP traffic looks different at different speeds, vplan dimension that wasn't covered.

---

## Coding A — Systems C: PCIe enumeration walker (model)

A topology walker that does the depth-first recursion. Simulates config reads from an in-memory topology and prints the assigned bus numbers + BAR ranges.

```c
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define MAX_DEV 64

typedef enum { EP, BRIDGE } kind_t;

typedef struct dev {
    kind_t   kind;
    uint16_t vid;
    uint8_t  bus, dev, fn;
    uint8_t  sec_bus, sub_bus;             /* bridge only */
    uint32_t bar_size[6];                  /* requested */
    uint32_t bar_base[6];                  /* assigned */
    int      child[MAX_DEV];               /* indices behind a bridge */
    int      nchild;
} dev_t;

static dev_t topo[MAX_DEV];
static int   ntopo;
static uint8_t next_bus = 1;
static uint64_t mmio_cursor = 0x80000000ULL;

static int add(kind_t k, uint16_t vid, uint32_t b0, uint32_t b1) {
    dev_t *d = &topo[ntopo];
    d->kind = k; d->vid = vid;
    d->bar_size[0] = b0; d->bar_size[1] = b1;
    return ntopo++;
}

static void link(int parent, int child) {
    topo[parent].child[topo[parent].nchild++] = child;
}

/* Round up to next power of 2 alignment */
static uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }

static void walk(uint8_t bus, int dev_idx) {
    dev_t *d = &topo[dev_idx];
    d->bus = bus;
    /* assign BARs */
    for (int b = 0; b < 6; b++) {
        if (d->bar_size[b] == 0) continue;
        mmio_cursor = align_up(mmio_cursor, d->bar_size[b]);
        d->bar_base[b] = (uint32_t)mmio_cursor;
        mmio_cursor += d->bar_size[b];
    }
    if (d->kind == BRIDGE) {
        d->sec_bus = next_bus++;
        for (int c = 0; c < d->nchild; c++)
            walk(d->sec_bus, d->child[c]);
        d->sub_bus = next_bus - 1;
    }
}

static void print_dev(const dev_t *d) {
    printf("  %02x:%02x.%x  vid=%04x  kind=%s",
           d->bus, d->dev, d->fn, d->vid, d->kind == EP ? "EP " : "BRG");
    if (d->kind == BRIDGE)
        printf("  sec=%02x sub=%02x", d->sec_bus, d->sub_bus);
    for (int b = 0; b < 6; b++)
        if (d->bar_size[b])
            printf("  BAR%d=0x%08x (%uKB)", b, d->bar_base[b], d->bar_size[b]/1024);
    printf("\n");
}

int main(void) {
    /* Build: bus0 root -> [brg1 -> ep, ep, brg2 -> ep] */
    int brg1 = add(BRIDGE, 0x8086, 0, 0);
    int ep_a = add(EP,     0x10DE, 0x10000, 0);          /* 64 KB */
    int ep_b = add(EP,     0x144D, 0x4000, 0x100000);    /* 16 KB + 1 MB */
    int brg2 = add(BRIDGE, 0x8086, 0, 0);
    int ep_c = add(EP,     0x15B7, 0x40000, 0);          /* 256 KB */

    link(brg1, ep_a);
    link(brg2, ep_c);

    /* Root walks brg1, ep_b, brg2 on bus 0 */
    walk(0, brg1); walk(0, ep_b); walk(0, brg2);

    printf("Post-enumeration topology:\n");
    for (int i = 0; i < ntopo; i++) print_dev(&topo[i]);
    return 0;
}
```

---

## Bug Anchor

- **Day 18 (LTSSM):** link must be L0 before enum can read config.
- **Day 20 (PCIe PM):** PM cap programming happens during enum.
- **Day 21 (PCIe bring-up):** enum failures are the second-most-common bring-up issue after link-train.
- **Day 22 (PkgC ↔ L1.2):** MSI delivery during deep PM stresses the enumerated address path.

## Exit Check

- [ ] Wrote the depth-first enumeration pseudo-code.
- [ ] Explained the write-all-1s BAR sizing trick + 64-bit BAR pairing.
- [ ] Walked the capability chain by hand for one example.
- [ ] Stated the difference between MSI and MSI-X.
- [ ] Drew the MSI-X table layout (4 dwords per vector).
- [ ] Stated what bridge `secondary_bus` and `subordinate_bus` are for and when each is written.
- [ ] Named 6 enumeration coverage axes for a PCIe IP vplan.
- [ ] Read an `lspci -vvv` output and identified header type, BARs, caps, MSI/MSI-X state.

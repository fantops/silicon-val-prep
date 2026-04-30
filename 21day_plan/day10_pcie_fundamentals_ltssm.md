# Day 10 — PCIe Fundamentals + LTSSM + Power States

## Concept

PCIe is a **3-layer point-to-point serial fabric**: physical (lanes, encoding, equalisation), data link (LCRC, ACK/NAK, flow control credits), transaction (TLPs — posted MWr, non-posted MRd/CfgRd, completions). Every link runs an **LTSSM** state machine: `Detect → Polling → Config → L0 → Recovery / L0s / L1 / L1.1 / L1.2 / L2`. Software talks to the fabric through **config space** at **ECAM** addresses computed from `BDF`, **BARs** that publish MMIO windows, and a **capability list** for power, MSI/MSI-X and PCIe-extended features. Gen6 introduces **PAM4 + FEC + 256 B FLITs + L0p**.

## Why this day in the flow

Days 4-8 built the cache/MMU/IOMMU/DMA picture. Day 10 is the **bus that carries all that traffic**. PCIe is the substrate for NVMe (Day 11), thermal events (Day 15), accelerator submission (Day 18-19) and a huge fraction of debug (Day 20). Today is also where the Day 13 PkgC6 story lives — PkgC6 needs PCIe links in **L1.2** specifically.

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

## Coding B — Interview DS&A: Climbing Stairs (LC 70)

DP / Fibonacci — same shape as counting LTSSM substate paths.

```python
def climbStairs(n):
    a, b = 1, 1
    for _ in range(n):
        a, b = b, a + b
    return a
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

---

## Extension — Automotive buses (LIN / CAN / CAN-FD / Auto Ethernet / TSN / FlexRay)

PCIe is the high-speed serial protocol; the same protocol-validation discipline (3 layers, error states, compliance) applies to the automotive buses MCU/SoC bring-up roles (Renesas, NXP, TI, ST, Infineon, Tier-1 auto) centre on.

### LIN
- Single wire (LIN bus) ref to battery; **master/slave**, master-polled. 1–20 kbit/s. Body electronics (mirrors, seats, climate).
- Frame: **break (≥13 dom bits) → sync (0x55) → PID → 0–8 data → checksum**.
- Validate: bit timing across 8–18 V supply, slave wake-up, transceiver dominant-timeout.

### CAN / CAN-FD
- Differential pair (CAN_H / CAN_L), terminated **120 Ω at each bus end** (60 Ω effective DC).
- **Dominant (0)** wins over **recessive (1)** on the wire (wired-AND).
- **Arbitration** by ID — lower ID wins; non-destructive, no retransmit on arbitration loss.
- **Bit stuffing** — insert opposite bit after 5 same-polarity bits.
- **Error states** — error-active → error-passive (TEC ≥ 128) → bus-off (TEC ≥ 256). Bus-off recovery requires controller intervention + 128 × 11 recessive bits.
- **CAN-FD** — same arbitration phase, switches to **higher data-phase rate** (5–8 Mbit/s) and **payload up to 64 B**.
- Validation:
  - CAN analyzer (Vector CANoe, PEAK PCAN) for protocol decode + error-frame counts.
  - Scope diff signal: bit time, slew, ringing on dominant→recessive (most common SI offender).
  - Termination check: 60 Ω measured between H and L with bus powered down.
  - Error injection: corrupt CRC, force error frames, drive bus-off and verify recovery.
  - Bus loading: sustained 80–90 % busload + worst-case ID collisions.
- Bug class: termination wrong (e.g., 60 Ω at one node and a third node missing → reflections → form errors clustered on dominant→recessive edges → error-passive under load).

### Automotive Ethernet
- **100BASE-T1**: single twisted pair, **PAM3**, full-duplex 100 Mbit/s.
- **1000BASE-T1**: **PAM4**, 1 Gbit/s.
- PHY link-up: **master/slave roles configured, NOT auto-negotiated** like consumer Ethernet.
- **AVB / TSN** on top:
  - **gPTP (802.1AS)** — sub-µs time sync.
  - **TAS (802.1Qbv)** — time-aware shaper, gated traffic per cycle.
  - **CBS (802.1Qav)** — credit-based shaper for AVB streams.
  - **Frame preemption (802.1Qbu / 802.3br)**.
- Compliance: IEEE 802.3 conformance for the PHY layer; gPTP/TSN have separate test suites.

### FlexRay (legacy but still appears)
- Dual channel, **TDMA** with static + dynamic segments, deterministic. Historically chassis (brake/steer).

### Vendor MCU/SoC family map (Renesas worked example — mirror the same shape per JD)
| Family | Core | Slot |
|---|---|---|
| **RH850** | Renesas G3/G4 | automotive MCU (powertrain, body) |
| **RA** | ARM Cortex-M | general MCU (consumer/industrial) |
| **RZ** | ARM Cortex-A / R | MPU, HMI, industrial |
| **RL78** | Renesas 16-bit | low-power MCU |
| **R-Car** | ARM Cortex-A + accelerators | ADAS / IVI SoC |
| **RX** | Renesas 32-bit | industrial MCU |

Other vendors land on the same axes (auto MCU / general MCU / low-power MCU / MPU / auto SoC):
- **NXP** — S32 (auto MCU/MPU) / Kinetis / LPC (general MCU) / i.MX (industrial MPU) / Layerscape (networking)
- **TI** — Hercules TMS570 (safety MCU) / MSP430 (low-power) / Sitara AM (industrial MPU) / Jacinto TDA (auto SoC)
- **ST** — SPC58 / Stellar (auto) / STM32 (general MCU+MPU)
- **Infineon** — AURIX TC3xx/TC4xx (auto MCU) / XMC (industrial) / PSoC (general)

### Protocol compliance + corner / stress (applies to PCIe AND automotive)
- **Compliance** = conformance to spec under specified conditions (PCIe-CEM CTS, USB-IF, IEEE 802.3, ISO 16845 CAN, JEDEC DDR). Not "works on my board".
- **Corner cases** = voltage (Vmin/Vnom/Vmax), temp (-40/25/85/125 °C for auto), process (FF/TT/SS samples), frequency (Fmin/Fmax), silicon revision (A0/A1/B0).
- **Stress** = long-soak (24/72 h), thermal cycling, power cycling, simultaneous-IP load, pattern stress (DDR march, walking-1 across pins), error injection.
- **Margining** = Vmin search (binary search to first fail), shmoo plots (V vs F), eye-margin shrink for SI.

---

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

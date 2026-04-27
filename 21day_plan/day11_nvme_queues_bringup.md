# Day 11 — NVMe Queues, Bring-up, CSTS.RDY Bug

> **Source merge:** [30day_plan/day20_nvme_architecture.md](../30day_plan/day20_nvme_architecture.md), [30day_plan/day21_nvme_bringup_coding.md](../30day_plan/day21_nvme_bringup_coding.md). 30-day files are unchanged.

## Concept

NVMe is a **PCIe-native host-controller protocol** built on **paired SQ/CQ ring buffers in host memory** + **MMIO doorbells in the controller's BAR**. Bring-up is a strict register dance: read `CAP`, disable (`CC.EN=0`, poll `CSTS.RDY=0`), program admin queue (`AQA`/`ASQ`/`ACQ`), enable (`CC.EN=1`, poll `CSTS.RDY=1`), Identify, create I/O queues, then issue Reads/Writes. The **`CSTS.RDY` timeout** is the canonical bring-up failure and the bug that exercises every concept from Days 4-10.

## Why this day in the flow

Day 11 is where Days 4-10 stop being theory:

- Day 4-5 cache lines + coherence — SQ entries are 64 B (one cache line) for a reason.
- Day 6-7 VM + IOMMU — every PRP is a physical / IOVA address; mismap = no completion.
- Day 8 DMA + doorbells — exactly the picture, with phase bit on the CQ side.
- Day 9 ARM/Snapdragon — SMMU Stream ID in front of every NVMe DMA on QC platforms.
- Day 10 PCIe — `lspci` for VID/DID, BAR0 = NVMe register block, MSI-X for completion interrupts.

In the 30-day Layer map this is **Layer 4 (Storage on PCIe)**.

---

## Deep Study Notes

### 1. Why NVMe wins over AHCI

| Property | AHCI (SATA) | NVMe |
|---|---|---|
| Queues | 1 × 32 commands | 65535 × 65536 commands |
| Per-IO latency | ~6 µs | ~2.5 µs |
| Locks | global | per-queue, lock-free per-CPU |
| Interrupts | shared IRQ | MSI-X vector per queue |
| Command size | translated SATA FIS | flat 64-byte SQE |

NVMe was designed PCIe-native: per-CPU queues, per-queue MSI-X, simple 64 B commands, posted MMIO doorbell.

### 2. The queue-pair architecture

```
   CPU 0           CPU 1           CPU N
   ┌───┐           ┌───┐           ┌───┐
   │SQ1│ ┌──┐      │SQ2│ ┌──┐      │SQn│ ┌──┐
   │CQ1│ │MSI│     │CQ2│ │MSI│     │CQn│ │MSI│
   └─┬─┘ └─┬┘      └─┬─┘ └─┬┘      └─┬─┘ └─┬┘
     │     │         │     │         │     │
   ══╪═════╪═════════╪═════╪═════════╪═════╪══ PCIe
     │     │         │     │         │     │
     ▼     ▼         ▼     ▼         ▼     ▼
        ┌────────────────────────────────┐
        │       NVMe Controller          │
        │  Admin Q0 (mgmt) + I/O Qs 1..n │
        └────────────────────────────────┘
```

| Object | Lives in | Written by | Entry size |
|---|---|---|---|
| **SQ** | host memory (DMA-mapped) | host (CPU) | 64 B |
| **CQ** | host memory | controller (DMA) | 16 B |
| **SQ Tail Doorbell** | controller BAR0 MMIO | host (posted MWr) | 4 B |
| **CQ Head Doorbell** | controller BAR0 MMIO | host (posted MWr) | 4 B |

### 3. Command flow — 8 steps

```
Host                                          Controller
  1. Write SQE at SQ[tail]   (64 B in host RAM)
  2. (barrier)                                 ─┐
  3. Doorbell → SQ Tail DB   (posted MWr)  ───►│ device fetches SQE via DMA
                                                │ executes (read flash, etc.)
                                                │ writes CQE at CQ[tail] via DMA
  4. (host polls CQ phase or takes MSI-X)  ◄────┘
  5. Read CQE; check phase bit and status code
  6. Doorbell → CQ Head DB   (posted MWr)  ───► free CQ slot
```

### 4. Controller registers (BAR0)

| Off | Reg | Purpose |
|---|---|---|
| 0x00 | **CAP** | MQES (max queue entries), DSTRD, MPSMIN/MAX, TO (timeout × 500 ms) |
| 0x08 | VS | version (e.g., `0x00010400` = NVMe 1.4) |
| 0x0C/0x10 | INTMS/INTMC | int mask set/clear |
| 0x14 | **CC** | EN, CSS (command set), MPS, IOSQES, IOCQES, SHN |
| 0x1C | **CSTS** | RDY, CFS (controller fatal status), SHST |
| 0x24 | **AQA** | admin SQ size, admin CQ size |
| 0x28 | **ASQ** | admin SQ base — physical / IOVA, page-aligned |
| 0x30 | **ACQ** | admin CQ base — physical / IOVA, page-aligned |
| 0x1000+ | doorbells | see formula below |

### 5. Doorbell offset formula

```
SQ y Tail DB = 0x1000 + (2y)     × (4 << CAP.DSTRD)
CQ y Head DB = 0x1000 + (2y + 1) × (4 << CAP.DSTRD)
```

`DSTRD` is in `CAP[35:32]`. `DSTRD=0` is most common; some devices use 1 to leave room for vendor extensions.

| DSTRD | Stride | SQ0 | CQ0 | SQ1 | CQ1 |
|---|---|---|---|---|---|
| 0 | 4 B | 0x1000 | 0x1004 | 0x1008 | 0x100C |
| 1 | 8 B | 0x1000 | 0x1008 | 0x1010 | 0x1018 |

Miscomputing `DSTRD` puts the doorbell at the wrong offset, controller never sees the kick, `CSTS.RDY` never asserts (admin SQ empty forever).

### 6. SQE layout (64 B)

```
opcode | flags | command_id (16) | nsid (32)
reserved (64)
prp1 (64)              ← physical / IOVA of data buffer
prp2 (64)              ← second PRP, or PRP-list if data > 2 pages
cdw10 cdw11 cdw12 cdw13 cdw14 cdw15  (24 B)
```

For **Read (0x02) / Write (0x01)**: `cdw10/11` = LBA (low/high), `cdw12 [15:0]` = number of blocks − 1.

### 7. CQE layout (16 B) and phase bit

```
dw0 (cmd-specific, e.g., bytes copied)
reserved (32)
sq_head (16) | sq_id (16)
command_id (16) | status (16)  ← bit 0 = phase, bits [15:1] = SC/SCT/M/DNR
```

**Phase bit avoids host-side memset.** Controller writes phase=1 on first lap; host expects phase=1; on wrap controller writes phase=0; host's expected phase flips. Free synchronisation, one-bit comparison per poll.

### 8. Bring-up sequence (the canonical dance)

```
1. PCIe enumerate → assign BARs → ioremap BAR0
2. Read CAP → MQES, DSTRD, MPSMIN, TO
3. Disable: CC.EN=0; spin until CSTS.RDY=0 (timeout = TO * 500 ms)
4. Allocate Admin SQ + CQ (page-aligned, contiguous, DMA-mapped)
5. Write AQA (sizes), ASQ (admin SQ base), ACQ (admin CQ base)
6. Enable: CC.EN=1, CSS=0 (NVM), MPS=4 KB, IOSQES=6 (64 B), IOCQES=4 (16 B)
   spin until CSTS.RDY=1; if CSTS.CFS — fatal, dead controller
7. Submit Identify Controller (opcode 0x06, CNS=1)
8. Submit Identify Namespace (opcode 0x06, CNS=0) for each NSID
9. Set Features: Number of Queues (FID=0x07)
10. Create I/O CQs (opcode 0x05) before SQs (opcode 0x01) — CQ must exist first
11. Issue Read / Write on the new I/O queues
```

### 9. The `CSTS.RDY` bug (THE bring-up bug)

**Symptom:** `CC.EN=1` written, `CSTS.RDY` never asserts; driver times out and gives up.

**Top suspects, in order:**

1. **`CSTS.CFS = 1`** (Controller Fatal Status). Hard fault. Get JTAG / firmware crash log; controller is dead. Reset, retry, capture next failure mode.
2. **Wrote AQA/ASQ/ACQ while controller still enabled.** `CC.EN=0` → must poll `CSTS.RDY=0` before touching admin-queue registers; otherwise the controller computes pointers from half-old, half-new values and refuses to come back up.
3. **`AQA` queue sizes wrong** (encoded as `size − 1`; off-by-one writes 0 entries, controller refuses).
4. **`ASQ` / `ACQ` not page-aligned** or not physically contiguous; controller spec violation.
5. **`ASQ` / `ACQ` wrong physical address** (driver wrote a virtual address, or wrote an IOVA without DMA-mapping). Controller DMAs to garbage; on better controllers `CFS` asserts.
6. **`CC` value malformed**: `IOSQES` not 6, `IOCQES` not 4, `MPS` outside `[CAP.MPSMIN, CAP.MPSMAX]`, reserved bits set. Controller silently refuses `RDY`.
7. **Timeout too tight:** some controllers want > `CAP.TO` × 500 ms after `CC.EN=1`. Use double the spec timeout in the driver during bring-up.
8. **PCIe link in lower state:** L1.2 not exited promptly, doorbell write delayed past timeout. Check `lspci -vvv` for current link state.
9. **IOMMU/SMMU fault on first DMA:** controller tried to DMA-fetch the first SQE, IOMMU rejected. Read IOMMU EVTQ.
10. **`DSTRD` miscomputed → doorbells go to the wrong MMIO offset** → admin SQ never receives the first kick → controller never starts processing → `RDY` never sets.

**Debug path:** AER + LTSSM (link alive?) → CSTS.CFS (fatal?) → IOMMU EVTQ (DMA rejected?) → re-read AQA/ASQ/ACQ from BAR0 (did writes stick?) → recompute `DSTRD` → JTAG single-step / protocol analyser.

### 10. The bring-up bug list (from real bring-ups)

| Symptom | Likely Cause |
|---|---|
| `CSTS.RDY` never goes to 1 | one of the 10 above |
| Config reads return `0xFFFFFFFF` | link down (LTSSM not at L0) |
| Completion never arrives | wrong PRP physical address / SMMU/IOMMU fault |
| Phase bit never flips | CQ base wrong → controller writing into nowhere |
| Wrong data | endianness on PRP fields, or `cdw12 = blocks` instead of `blocks - 1` |

---

## Bugs in this Topic

- **`CSTS.RDY` never asserts after `CC.EN=1`:** the headline bug. See #1-#10 above. Most common on first silicon: `DSTRD` mis-decoded or AQA/ASQ written while `CSTS.RDY=1` still.
- **`DSTRD` mis-decoded:** doorbell at wrong offset → controller never sees admin kick → `RDY` never sets. Compute `DSTRD` from `CAP[35:32]` (high DW of CAP), not `CAP[3:0]`.
- **`AQA` off-by-one:** `AQA` encodes `size − 1`; writing the literal size gives 1 entry too many.
- **PRPs hold virtual addresses:** completion never arrives, controller DMAs to nowhere, on systems with IOMMU you get a fault. Use `dma_map_single` (Linux) and program the returned IOVA.
- **CQ/SQ wrong creation order:** create I/O CQ first (opcode 0x05), then I/O SQ that points at it (opcode 0x01). Reversed = `SC = Invalid Completion Queue`.
- **Phase bit never flips:** ACQ base is wrong; controller writes completions into a buffer the host never inspects. Re-read ACQ from BAR0 to confirm the write stuck.
- **MSI-X enabled before queue creation:** controller posts to vector 0 before the host masked it; spurious IRQ at probe time.
- **Wrong endianness on `cdw10/11` (LBA):** controller reads from the wrong block; data corruption silently. Always little-endian on the wire; check on big-endian hosts.

---

## Interview Explanation (60-90 sec)

> "NVMe is a PCIe-native protocol built on paired SQ/CQ rings in host memory and MMIO doorbells in the controller's BAR. Per-CPU queues plus MSI-X per queue let it scale to over a million IOPS where AHCI tops out around 200K. Bring-up is a strict register dance: read `CAP`, disable with `CC.EN=0` and spin on `CSTS.RDY=0`, program `AQA`/`ASQ`/`ACQ` for the admin queue, set `CC.EN=1` with the right `IOSQES`/`IOCQES`, spin on `CSTS.RDY=1`, then Identify Controller, Identify Namespace, create I/O CQ before I/O SQ, then read/write. The classic bring-up bug is `CSTS.RDY` never asserting — first I check `CSTS.CFS` for a fatal, then I verify nothing wrote `AQA`/`ASQ`/`ACQ` while the controller was still enabled, then I recompute `DSTRD` from `CAP[35:32]` because if doorbell offsets are wrong the controller never sees the first admin kick, then I check the IOMMU event queue for a DMA fault on the first SQE fetch. Posted writes for doorbells, phase-bit on completions, and the PCIe-MSI-X plumbing all come together at this layer."

---

## Practice Q&A

**Q1. Why does NVMe scale beyond AHCI?**
Per-CPU lock-free queues, MSI-X per queue, posted doorbells, simple 64 B commands, no SATA translation.

**Q2. Why are doorbells posted writes?**
One-way notify, no response needed; non-posted would round-trip per IO and serialise submissions.

**Q3. Phase bit — what does it save?**
Per-IO `memset` on completions and a per-completion device→host doorbell. One bit comparison per poll.

**Q4. Disable / re-enable sequence in detail.**
`CC.EN=0`; poll `CSTS.RDY=0` (within `CAP.TO × 500 ms`); only **then** write `AQA`/`ASQ`/`ACQ`; `CC.EN=1`; poll `CSTS.RDY=1`. AQA/ASQ/ACQ writeable only while disabled.

**Q5. `CSTS.RDY` never goes to 1 — debug path.**
`CSTS.CFS` first. Then re-read AQA/ASQ/ACQ from BAR0 (writes stuck? alignment?). Then re-derive `DSTRD`. Then IOMMU EVTQ for the first DMA. Then PCIe link state. Last: power-cycle + protocol analyser capture.

**Q6. Why physical (or IOVA) addresses in PRPs?**
Controller is a PCIe DMA master; no concept of OS virtual address space. Driver translates with `dma_map_*` and writes the returned address.

**Q7. Identify Controller vs Identify Namespace — why both?**
Controller-level: vendor, MQES, MDTS, NS count. Namespace-level: capacity, LBA size (512 vs 4096), atomic write size. Driver needs both to issue valid IO.

**Q8. Doorbell offset for SQ tail of queue 1 with `DSTRD=1`?**
`0x1000 + 2*1 * 8 = 0x1010`.

**Q9. `AQA` for a 32-entry admin SQ and 32-entry admin CQ?**
`((32-1) << 16) | (32-1) = 0x001F001F`.

**Q10. Create-order for I/O queues?**
I/O **CQ** first (opcode 0x05), then I/O **SQ** that points at it (opcode 0x01). Reverse → `Invalid Completion Queue`.

**Q11. Completion never arrives. Top 3 suspects?**
PRP holds a virtual address (no DMA mapping); SMMU/IOMMU fault on the data DMA; CQ base wrong (controller wrote completion to nowhere).

---

## Coding A — Systems C: NVMe bring-up + SQ submit + CQ poll

```c
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* Simulated BAR0 */
#define BAR_SZ 0x2000
static uint8_t bar[BAR_SZ];

#define R_CAP 0x00
#define R_VS  0x08
#define R_CC  0x14
#define R_CSTS 0x1C
#define R_AQA 0x24
#define R_ASQ 0x28
#define R_ACQ 0x30
#define R_DB  0x1000

static uint32_t r32(uint32_t off) { uint32_t v; memcpy(&v,bar+off,4); return v; }
static void     w32(uint32_t off, uint32_t v) {
    memcpy(bar+off,&v,4);
    if (off == R_CC) {                  /* model: RDY follows EN */
        uint32_t s = r32(R_CSTS);
        s = (v & 1) ? (s | 1) : (s & ~1u);
        memcpy(bar+R_CSTS,&s,4);
    }
}
static void     w64(uint32_t off, uint64_t v) { memcpy(bar+off,&v,8); }

static void sim_init(void) {
    memset(bar,0,sizeof bar);
    /* CAP: MQES=255 (low 16), TO=10 (bits 31:24), DSTRD=0 (bits 35:32 of full 64) */
    uint64_t cap = 0xFFu | (10ull << 24);
    memcpy(bar+R_CAP,&cap,8);
}

static uint32_t db_off(uint16_t q, bool is_cq, uint8_t dstrd) {
    return R_DB + (2u*q + (is_cq?1:0)) * (4u << dstrd);
}

static bool nvme_bringup(void) {
    /* Step 1: read CAP */
    uint32_t cap_lo = r32(R_CAP), cap_hi = r32(R_CAP+4);
    uint16_t mqes = (cap_lo & 0xFFFF) + 1;
    uint8_t  to   = (cap_lo >> 24) & 0xFF;
    uint8_t  dstrd = cap_hi & 0xF;
    printf("CAP: MQES=%u TO=%u*500ms DSTRD=%u\n", mqes, to, dstrd);

    /* Step 2: disable, poll RDY=0 */
    w32(R_CC, 0);
    if (r32(R_CSTS) & 1) { printf("RDY did not clear\n"); return false; }

    /* Step 3: program admin queue */
    const uint32_t QSZ = 32;
    w32(R_AQA, ((QSZ-1) << 16) | (QSZ-1));
    w64(R_ASQ, 0x100000ull);
    w64(R_ACQ, 0x101000ull);

    /* Step 4: enable */
    /* CC: EN=1, CSS=0, MPS=0, AMS=0, SHN=0, IOSQES=6, IOCQES=4 */
    uint32_t cc = (6u << 16) | (4u << 20) | 1u;
    w32(R_CC, cc);
    if (!(r32(R_CSTS) & 1)) {
        printf("RDY not set; CFS=%u\n", (r32(R_CSTS) >> 1) & 1); return false;
    }
    printf("Controller READY\n");

    printf("Doorbells: SQ0=0x%X CQ0=0x%X SQ1=0x%X CQ1=0x%X\n",
           db_off(0,false,dstrd), db_off(0,true,dstrd),
           db_off(1,false,dstrd), db_off(1,true,dstrd));
    return true;
}

int main(void) {
    sim_init();
    return nvme_bringup() ? 0 : 1;
}
```

## Coding B — Interview DS&A: Group Anagrams (LC 49)

Sort-as-key hash map.

```python
from collections import defaultdict
def groupAnagrams(strs):
    g = defaultdict(list)
    for s in strs:
        g[''.join(sorted(s))].append(s)
    return list(g.values())
```

---

## Bug Anchor

- **Day 8 (DMA / doorbells):** the descriptor + doorbell pattern, with phase bit on the CQ.
- **Day 10 (PCIe / LTSSM):** link state and AER are the first thing to check on `RDY` timeout.
- **Day 7 (IOMMU / SMMU):** every PRP is an IOVA on QC; first DMA fault = bring-up dead.
- **Day 20 (Debug method):** `lspci`, AER, IOMMU EVTQ, JTAG, protocol analyser — the toolkit.

## Exit Check

- [ ] Drew SQ/CQ + doorbell architecture from memory.
- [ ] Listed the 11-step bring-up sequence with register names.
- [ ] Walked the `CSTS.RDY` debug path through 5+ suspects.
- [ ] Computed doorbell offset from `DSTRD`.
- [ ] Stated why CQ must be created before SQ.
- [ ] Stated phase-bit purpose and wrap behaviour.
- [ ] Mapped PRPs to IOMMU mapping.

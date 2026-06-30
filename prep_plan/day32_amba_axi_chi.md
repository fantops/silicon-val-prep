# Day 32 — AMBA AXI & CHI (On-Chip Interconnect)

## Concept

Every IP on an SoC talks over a standard **on-chip bus**. The dominant family is **AMBA**: **APB** (simple peripherals), **AHB** (legacy), **AXI** (the high-performance workhorse — 5 independent channels, bursts, outstanding + out-of-order transactions), and **ACE/CHI** (the **coherent** interconnect for multi-core — cache coherence at interconnect scale). Master AXI's **VALID/READY handshake + ordering model** and CHI's **coherent transactions**, because "validate the IP on the SoC interconnect" is the core of SoC/IP validation, and AXI/CHI is the on-chip cousin of the PCIe link you study next.

## Why this day in the flow

This **opens Phase 7 (Interconnect & PCIe)**. [PCIe](day33_pcie_fundamentals_ltssm.md) is the **off-chip / chip-to-chip** bus; AXI/CHI is the **on-chip** interconnect every IP attaches to — so it belongs first. It reuses [Day 20](day20_dma_doorbells_ordering_barriers.md) (DMA, ordering, barriers — AXI has an explicit ordering model), [Day 17](day17_coherence_mesi_false_sharing.md) (MESI — **CHI is coherence at interconnect scale**), [Day 9](day09_fsm_sequence_detectors.md) (the protocol and its arbiters are FSMs), [Day 5](day05_boolean_combinational_logic.md) (the arbiter is a priority encoder), and [Day 8](day08_timing_sta.md) (the interconnect is a timing-critical block). It feeds the verification methodology ([Day 24](day24_verification_planning_coverage.md)/[Day 25](day25_testbench_assertions_debug.md)) — AXI protocol VIP and CHI coherence checking are textbook IP-Val deliverables.

**Why it's new (Intel IP-Val value):** Arm/Intel SoCs are built on AMBA. "Walk the AXI write path," "what's the AXI ordering rule," "how does CHI keep caches coherent" are standard IP-integration-val questions the plan didn't cover.

---

## Deep Study Notes

### 1. The AMBA family — where each fits

| Protocol | Role | Character |
|---|---|---|
| **APB** | low-bandwidth peripherals (timers, GPIO, CSRs) | simple, single transfer, no bursts/pipeline |
| **AHB** | legacy system bus | pipelined, single outstanding, mostly superseded |
| **AXI4** | high-performance memory-mapped IP | 5 channels, bursts, many outstanding, out-of-order |
| **AXI4-Lite** | simple register access | AXI subset, single beats, no bursts/IDs |
| **AXI-Stream** | data streaming (no addresses) | DMA/accelerator dataflow |
| **ACE / CHI** | cache-coherent interconnect | coherence + snoops; CHI scales to many-core (Arm CMN) |

Pick by need: registers → APB/AXI-Lite; high-throughput memory → AXI; streaming → AXI-Stream; coherent CPU clusters → CHI.

### 2. AXI — five independent channels

```
WRITE:  AW (addr)  ──►   ┐
        W  (data)  ──►   ├─►  slave   ──►  B (write response)
                          ┘
READ:   AR (addr)  ──►   slave   ──►  R (read data + response)
```

Five channels, **each with its own VALID/READY handshake**, and **address is decoupled from data**: **AW** (write address), **W** (write data), **B** (write response), **AR** (read address), **R** (read data+response). Decoupling lets the master issue many addresses before data returns → **outstanding transactions** and pipelining.

### 3. The VALID/READY handshake (and the deadlock rule)

```
transfer happens on the cycle where BOTH VALID and READY are high.
- source asserts VALID when it has data; must HOLD it until READY.
- VALID must NOT wait for READY (a source may assert VALID first);
  READY may wait for VALID. Never make them wait on each other → deadlock.
```

This handshake is the universal backpressure mechanism — a slow consumer simply holds READY low. The **cardinal rule**: VALID, once asserted, cannot be withdrawn before the handshake completes, and **VALID must not depend on READY** — violating it is the classic AXI **deadlock**.

### 4. Bursts, sizes, IDs

- **Burst types:** `INCR` (sequential addresses — the common case), `WRAP` (wraps at a boundary — cache-line fills), `FIXED` (same address — FIFO/peripheral).
- **Burst length/size:** beats per burst × bytes per beat; AXI4 allows up to 256-beat INCR bursts.
- **Transaction IDs (AxID):** tag transactions so multiple can be **outstanding** and **complete out of order** — a key throughput feature.

### 5. The ordering model (the most-asked AXI rule)

```
Same ID    → responses return IN ORDER (the master relies on this).
Different ID → interconnect MAY reorder (independent streams).
Reads vs writes to the same address → ordering enforced by the interconnect.
```

Out-of-order completion across IDs is what hides memory latency; in-order within an ID is what lets software reason about a single stream. The interconnect must **enforce same-ID ordering** and the **read/write hazard ordering** to the same address — this is the [Day 20](day20_dma_doorbells_ordering_barriers.md) ordering story at on-chip scale. Exclusive access (`AxLOCK`) implements atomic load-linked/store-conditional via an exclusive monitor.

### 6. ACE / CHI — coherence at interconnect scale

When multiple CPU clusters each have caches, the interconnect must keep them **coherent** — this is **MESI/MOESI** ([Day 17](day17_coherence_mesi_false_sharing.md)) extended across the SoC:

- **ACE** extends AXI with snoop channels + cache-state signaling.
- **CHI** (Coherent Hub Interface, Arm CMN mesh) is the scalable, packet-based redesign for many-core. Node types: **RN** (Request Node — a CPU/cache), **HN** (Home Node — the point of coherence + snoop filter/directory), **SN** (Slave Node — memory).
- **Coherent transactions:** `ReadShared` (get a shared copy), `ReadUnique` (get exclusive, for a write), `MakeUnique` (invalidate others), `WriteBack`/`WriteClean`, plus **snoops** the HN sends to other RNs. The HN's **snoop filter/directory** tracks who has each line (the [Day 17](day17_coherence_mesi_false_sharing.md) snoop filter, scaled).
- This is how a 64-core SoC keeps every cache consistent without a shared bus — directory-based coherence over a mesh.

### 7. Interconnect topology + arbitration

The fabric between masters and slaves is a **crossbar** or **NoC** (network-on-chip) with **arbiters** at contention points: **round-robin** for fairness, **QoS/priority** for latency-sensitive masters (the [Day 5](day05_boolean_combinational_logic.md) priority encoder + [Day 9](day09_fsm_sequence_detectors.md) FSM). It must avoid **deadlock** (cyclic resource dependency) and **starvation** (a master never granted) — the same arbiter-fairness concerns as the Broadcom switch fabric.

### 8. The validation surface (the IP-Val job)

| Class | What you check |
|---|---|
| **Protocol** | every VALID/READY rule, VALID-not-waiting-on-READY, stable payload while VALID, no illegal transitions (**AXI VIP / assertions**) |
| **Ordering** | same-ID in-order, cross-ID reorder allowed, read/write hazards |
| **Bursts** | INCR/WRAP/FIXED × sizes × lengths; WRAP boundary; 4KB boundary not crossed |
| **Errors** | `OKAY/EXOKAY/SLVERR/DECERR` responses; decode error to unmapped address |
| **Exclusive** | LL/SC exclusive monitor correctness |
| **Coherence (CHI)** | snoop correctness, state transitions, directory accuracy, false sharing |
| **Deadlock/QoS** | no fabric deadlock/livelock, fairness, bandwidth under contention |

AXI/CHI **VIP (verification IP)** + a **scoreboard** that mirrors ordering is the standard setup ([Day 25](day25_testbench_assertions_debug.md)); coverage crosses burst × size × ID × backpressure × response.

---

## Bugs in this Topic

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Handshake deadlock** | bus hangs | VALID waits on READY (or mutual wait) | VALID independent of READY |
| **Payload changes while VALID** | corrupt transfer | source not holding stable | hold payload until handshake |
| **Same-ID reorder** | data returned out of order | interconnect mis-orders an ID | enforce same-ID ordering |
| **4KB boundary cross** | illegal burst | INCR burst spans 4KB | split at boundary |
| **Exclusive monitor bug** | LL/SC livelock or false success | monitor state wrong | correct exclusive tracking |
| **Fabric deadlock** | system-wide hang | cyclic dependency in arbiters | dependency-ordered routing / VC |
| **Snoop miss (CHI)** | stale data, coherence break | directory/snoop-filter error | fix HN tracking; coherence tests |

---

## Interview Explanation (60–90 sec)

> "AMBA is the on-chip bus family. AXI is the workhorse: five independent channels — write address, write data, write response, read address, read data — each with a simple VALID/READY handshake where the transfer happens when both are high. The address channels are decoupled from data, which lets a master have many transactions outstanding, tagged by ID, and those can complete out of order across different IDs while staying in order within the same ID — that ordering rule is the thing software relies on, and the interconnect has to enforce it along with read-write hazards to the same address. The cardinal handshake rule is that VALID must never wait on READY, or you deadlock. Bursts come in INCR, WRAP for cache-line fills, and FIXED, and a burst can't cross a 4KB boundary. When CPUs have caches, AXI extends to ACE or, for many-core, CHI — that's MESI coherence scaled across a mesh, with home nodes holding a snoop-filter directory that tracks which cache owns each line and sends snoops, exactly like the snoop filter from the coherence day but at SoC scale. Validating this is protocol checking with AXI VIP for every handshake and ordering rule, error responses like SLVERR and DECERR, exclusive-access monitors, and for CHI, snoop and directory correctness — plus making sure the fabric arbiters are fair and deadlock-free."

---

## Architect Drills (derive, no answer key)

1. Draw the AXI write path across AW/W/B with VALID/READY. Show exactly where a deadlock appears if VALID waits on READY.
2. A master issues 4 reads with IDs `{A,A,B,B}`. Which responses must stay ordered, which may reorder, and why?
3. Why can't an INCR burst cross a 4KB boundary? What must the master do?
4. Map CHI `ReadUnique` + snoops to the [Day 17](day17_coherence_mesi_false_sharing.md) MESI transitions — what does the Home Node do?
5. Two masters and two slaves with cyclic requests deadlock the fabric. Diagnose and give a routing/virtual-channel fix.
6. Write the AXI coverage model: which axes do you cross, and what are the corner bursts?
7. Implement LL/SC with an exclusive monitor; show a livelock and the fix.

---

## Coding A — `d32_axi_model`

Model AXI in C: a **VALID/READY handshake** (`axi_xfer`) that **detects the VALID-waits-on-READY deadlock**; a **multi-ID outstanding scoreboard** (`axi_order_check`) asserting same-ID responses stay ordered while different-ID may reorder; an **INCR/WRAP burst-address generator** that flags a 4KB-boundary crossing; and a simple **round-robin/QoS arbiter** with a starvation check. Full spec in [coding/EXERCISES.md](../coding/EXERCISES.md#day-32--amba-axi--chi).

**Interview prompts:** Why must VALID not wait on READY? Same-ID vs cross-ID ordering? How is CHI just MESI at interconnect scale?

---

## Bug Anchor / cross-links

- **Built from:** [Day 20](day20_dma_doorbells_ordering_barriers.md) (ordering/barriers — AXI ordering model), [Day 17](day17_coherence_mesi_false_sharing.md) (MESI → CHI coherence + snoop filter), [Day 9](day09_fsm_sequence_detectors.md) (protocol/arbiter FSMs), [Day 5](day05_boolean_combinational_logic.md) (arbiter = priority encoder), [Day 8](day08_timing_sta.md) (interconnect timing).
- **Feeds:** [Day 33](day33_pcie_fundamentals_ltssm.md) (PCIe = the off-chip cousin; TLPs ≈ AXI transactions), [Day 38](day38_nvme_storage_controller_val.md) (NVMe datapath rides the interconnect), [Day 24](day24_verification_planning_coverage.md)/[Day 25](day25_testbench_assertions_debug.md) (AXI/CHI VIP, scoreboard, coverage).

---

## Exit Check

- [ ] Name AXI's five channels and the VALID/READY handshake + the deadlock rule.
- [ ] State the same-ID-ordered / cross-ID-reorderable ordering model.
- [ ] Distinguish INCR/WRAP/FIXED bursts and the 4KB rule.
- [ ] Explain CHI as MESI-at-interconnect-scale: RN/HN/SN, snoop filter, ReadShared/ReadUnique.
- [ ] Describe the AXI validation surface (protocol VIP, ordering, errors, exclusive, coherence).
- [ ] Identify fabric deadlock/starvation and arbiter fairness.
- [ ] Relate AXI transactions to PCIe TLPs ([Day 33](day33_pcie_fundamentals_ltssm.md)).

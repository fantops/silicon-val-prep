# S15 - One accelerator job end to end

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [DSA fundamentals §4–§5, §7, §9](../dsa-val/fundamentals/dsa_ip_fundamentals.md).
- **Must answer:**
  - **Submit:** a 64-byte descriptor → **MOVDIR64B** (dedicated WQ) or **ENQCMD/ENQCMDS** (shared WQ, with retry when full) to the **portal**, an MMIO page in a BAR (S7/S11).
  - **Execute:** the engine reads the descriptor → addresses translated with PASID (S13) → coherent DMA (S9) → completion record → interrupt (S12) or polling.
  - **Page fault mid-job:** block-on-fault vs partial completion, and what the completion status shows.
  - Draw *app → ENQCMD → portal → WQ → engine → IOMMU → memory → completion → MSI-X/poll*, with one failure mode and one observable per arrow.
- **Work link:** put the issue you are currently debugging on this diagram. Which arrow is it?
- **Week narration:** IOMMU/PASID → enable → submit → complete.
- **Handoff →** When everything goes idle, how does the system save power, and how does it wake for the next job?


## Study notes

### One DSA job from submission to completion

Software builds a 64-byte descriptor and submits it to a portal in a BAR. A dedicated WQ commonly uses MOVDIR64B; a shared WQ uses ENQCMD/ENQCMDS and must handle retry/backpressure. An engine fetches the descriptor, translates source/destination addresses using the configured DMA translation path (including PASID where applicable), performs the operation, then writes a completion record in memory. Software polls or receives an MSI-X notification. The exact supported opcodes and status behavior are device/spec-version dependent.

```text
application -> descriptor -> portal -> WQ -> engine -> translation/IOMMU
-> source read + destination write -> completion record -> poll or MSI-X
```

- The completion record is authoritative for operation status; an interrupt is a notification, not proof of success.
- If a page fault occurs, inspect the documented fault/retry mode and completion status; do not assume every operation automatically replays.
- For data correctness, check descriptor fields, address mappings, permissions, cache/coherence assumptions, and completion ordering.

### Worked example: job times out

Trace the path in order: was submission accepted? Did the WQ dequeue? Did the engine fetch the descriptor? Did translation succeed? Did data traffic occur? Was completion written? Was the interrupt delivered or only the wait path broken? Correlate status and logs at every boundary to isolate the first missing event.

### Concept - [day20_dma_doorbells_ordering_barriers.md](archive/day20_dma_doorbells_ordering_barriers.md)

A typical device-submission flow is **descriptor → barrier → MMIO doorbell → device DMA → completion**. Each arrow has a real failure mode: descriptor stuck in a dirty cache line, doorbell reordered ahead of the descriptor write on weak memory, IOMMU mapping missing, MMIO accidentally cached. Today consolidates all four into one mental model and one C harness.

### Deep Study Notes - [day20_dma_doorbells_ordering_barriers.md](archive/day20_dma_doorbells_ordering_barriers.md)

### 1. The canonical descriptor + doorbell flow

```
1. Driver fills a descriptor / queue entry in normal memory.
2. (If non-coherent) Clean / flush the descriptor cache line.
3. Memory barrier — descriptor must be visible before the doorbell.
4. MMIO doorbell write — uncached, side-effecting.
5. Device DMA — sends IOVA → IOMMU/SMMU → PA → reads descriptor.
6. Device performs work and writes completion to host memory.
7. (If non-coherent) Invalidate completion line; CPU reads completion.
```

Every step has a failure mode. The picture below is the one to draw on the whiteboard.

```
   CPU                     |  Device
   ───                     |  ──────
   write descriptor  ──┐   |
   clean cache line  ─►│   |
   memory barrier    ─►│   |
   MMIO doorbell  ─────┼──►│ DMA reads descriptor (via IOMMU)
                       │   │ DMA reads payload
                       │   │ DMA writes completion
   invalidate cache ◄──┼───┤
   read completion  ◄──┘   │
```

### 2. `volatile` vs barrier vs cache maintenance

| Mechanism | What it controls | Where it ends |
|---|---|---|
| `volatile` | compiler load/store emission and ordering w.r.t. other `volatile` | does **not** affect CPU reorder |
| memory barrier (`dmb`, `dsb`, `mfence`, `__sync_synchronize`) | CPU reorder | does **not** flush cache |
| cache maintenance (`dc cvac`, `clflush`, `clwb`) | data movement between cache and memory | does **not** order independent loads/stores |

You sometimes need all three, in that order: `volatile` ensures the store is emitted, the barrier ensures the device sees it after the descriptor, the maintenance ensures the descriptor reaches DRAM if the device is non-coherent.

### 3. Memory models in one paragraph

- **x86 (Total Store Order):** loads do not reorder with loads; stores do not reorder with stores; loads can be reordered before older stores to a different address. For descriptor-then-doorbell on x86, `volatile` plus the implicit `sfence` from the doorbell store-to-uncached is usually enough. Add `mfence` for descriptor-vs-completion ordering.
- **ARM / RISC-V (weakly ordered):** loads and stores can be reordered widely. The descriptor write must be ordered before the doorbell write with **`dmb st`** (Data Memory Barrier, store side) or **`dsb`** (Data Synchronization Barrier — stronger). Use `dsb sy` between stores and a subsequent device-side action.

The single most-asked exam question: *"On ARM, why does the device DMA-read garbage from the descriptor?"* — missing barrier between descriptor write and doorbell write.

### 4. Cache and DMA — coherent vs non-coherent

| System | Behaviour |
|---|---|
| Coherent DMA (Intel server, modern Apple Silicon, ARMv8 with hardware coherence) | Device DMA participates in the coherence protocol; CPU caches stay coherent automatically. |
| Non-coherent DMA (many embedded, some accelerators) | Driver must clean (CPU→DRAM) before the device reads, and invalidate (drop CPU copy) after the device writes. |

A **clean** writes a dirty line to DRAM and keeps it valid in cache. **Invalidate** drops the CPU copy without writing back. **Clean+invalidate** does both; safest after a device write.

DMA buffers should be **cache-line aligned** and sized in multiples of one line so that maintenance ops don't touch neighbour data.

### 5. MMIO accidentally cached

If the page-table cache attribute (`PCD/PWT` on x86, MAIR on ARM) marks the device range as cacheable, register reads can be cached. The CPU re-reads its cached value, status updates from the device side never appear, and the polling loop spins forever even though the device asserted the bit. Always make MMIO ranges **uncached / device-typed** in the PTE; the OS does this for you when you `ioremap` correctly, but bring-up code that maps memory by hand needs to set the attribute explicitly.

### 6. The doorbell write itself

A doorbell is a single store to an MMIO register. Three properties matter:

- **Side-effecting:** writing the doorbell triggers the device to do work; this is **not** memory in the cacheable sense.
- **Non-cacheable:** the store posts on the bus immediately.
- **Posted (PCIe):** the store has no completion the CPU waits for; once on the bus, the CPU moves on.

That last property means: on PCIe, the CPU can issue a doorbell store before the descriptor write has reached system memory. A barrier between them on the CPU side ensures the descriptor is at least globally visible before the device fetches it; the IOMMU/PCIe fabric handles the rest.

### 7. Ring buffers (preview of NVMe SQ/CQ on Day 23)

A submission queue (SQ) is a producer/consumer ring of fixed-size entries:

- Host writes the next entry at `tail`, then **rings the doorbell** (writes the new tail to an MMIO register).
- Device reads from `head`, processes, advances `head`.
- Host reads `head` to know how many entries the device has consumed.

The mirror, **completion queue (CQ)**:

- Device writes a completion at its tail with a **phase bit** that flips each lap.
- Host polls the next CQ entry's phase bit; when it matches the expected phase, the entry is valid.
- Host writes the new CQ head doorbell to tell the device space is free.

The phase bit avoids the host needing a separate "valid" doorbell from the device for each completion — the device's write-with-flipped-phase is self-signalling.

### 8. The five things that go wrong in this picture

1. **Cache stale descriptor:** clean missing on non-coherent path.
2. **Reordered doorbell on weak memory:** missing `dmb st` / `dsb sy`.
3. **MMIO cached:** wrong PTE cache attribute.
4. **IOMMU fault:** OS forgot `dma_map_*`, or device used a stale IOVA.
5. **Wrong doorbell stride / register:** NVMe `DSTRD` (CAP[35:32]) miscomputed; doorbell goes to the wrong offset, device never sees it.

Each maps to a Day on this plan: 1 → Day 16-17, 2 → today, 3 → Day 18, 16 → Day 19, 17 → Day 23.

---

### Bugs in this Topic - [day20_dma_doorbells_ordering_barriers.md](archive/day20_dma_doorbells_ordering_barriers.md)

- **Doorbell visible before descriptor on ARM:** weak-memory reorder; device DMA-reads stale descriptor and either ignores it or executes garbage. Fix: `__asm__ volatile("dmb st" ::: "memory")` between descriptor write and doorbell.
- **Polling loop spins on cached MMIO:** PTE marked cacheable; status reads return the same cached value forever. Fix: ensure PTE is device/uncached; on bare-metal, set MAIR or the right `ioremap` flag.
- **IOMMU fault on first submission:** OS never mapped the descriptor buffer; fault log shows BDF + IOVA + "no entry". Fix: `dma_map_single` (Linux) or equivalent.
- **NVMe `DSTRD` miscomputed (Day 23 anchor):** SQ/CQ doorbells are at `0x1000 + (2*i + (CQ?1:0)) * (4 << CAP.DSTRD)`; getting `DSTRD` wrong puts the doorbell at the wrong offset; device never sees the kick. The `CSTS.RDY` bug surfaces this when the host can't even bring up the admin queue.
- **PCU/ADR `WPQ_FLUSH` before `IO_QUIESCE_ACK` (Day 28 link):** the firmware analogue of issuing the doorbell before the descriptor — same bug class, different domain.

---

### Interview Explanation (60-90 sec) - [day20_dma_doorbells_ordering_barriers.md](archive/day20_dma_doorbells_ordering_barriers.md)

> "Device submission is descriptor, barrier, MMIO doorbell, device DMA via the IOMMU, completion. Each arrow has a failure mode. On ARM the descriptor write can be reordered after the doorbell, so I always put a `dmb st` between them; `volatile` on its own only orders compiler-side, not CPU-side. The way this fails in the wild is intermittent — it works at low throughput because the store buffer drains naturally between submits, then under sustained load the doorbell wins the race and the device fetches a half-written descriptor; you see queue stalls or completion errors that don't reproduce on x86 because TSO masks the ordering. On non-coherent DMA paths I clean the descriptor line so the device reads DRAM, not stale cache; after the device writes the completion I invalidate before reading. MMIO ranges must be uncached in the PTE — the classic bring-up bug is a status register that reads back the same value forever because the cache cached it. The IOMMU is the device-side MMU and its fault register tells me whether the OS forgot to map a buffer or the device sent a stale IOVA. The whole picture is the same physics as MESI on Day 17 — the device is just another coherence agent that has to see the latest copy in the right order at the right physical address."

---

### Architect Drills - [day20_dma_doorbells_ordering_barriers.md](archive/day20_dma_doorbells_ordering_barriers.md)

*Reason from constraints. The right answer is whichever the physics + cost + workload force. No answer key.*

**D1. Why doorbells at all — why doesn't the CPU just stream descriptors directly over a memory-mapped FIFO and skip the MMIO write?**
*Constraints to weigh:* MMIO bandwidth vs DRAM bandwidth, posted-write semantics, batching (one doorbell amortises N descriptors), back-pressure when the device is slower than the bus, what happens to the queue if the CPU stalls mid-stream. What does NVMe SQ tail vs MSI-X completion path actually optimise for?

**D2. Why does ARM expose weak memory ordering when x86 TSO "just works"? What would break if ARM moved to TSO?**
*Constraints to weigh:* store-buffer drain energy, OoO retire complexity, snoop traffic on every store-to-shared-line, mobile/server power budgets, whether weak ordering is a feature for the hardware or a tax on the software. Who pays — the chip designer, the kernel, or the application?

**D3. Why is there a per-device IOMMU translation when the CPU already has an MMU? Why not just give the device the CPU's page table?**
*Constraints to weigh:* devices don't take page faults the way CPUs do, ATC/PRI complexity, isolation between devices (one bad NIC shouldn't see another's buffers), virtualisation (passthrough to a VM with its own VA space), legacy 32-bit DMA. What does ATS buy and what does it cost?

**D16. Why are MMIO writes posted by default? Construct the workload where you'd want them all non-posted.**
*Constraints to weigh:* CPU stall per write vs throughput, ordering guarantees you give up, cost of an explicit read-back fence, error reporting (a posted write that faults can't return SIGBUS to the issuing instruction). When does an FPGA/accelerator team actually demand non-posted MMIO, and what do they give up?

**D17. Three ARM barrier scopes (`ish`/`osh`/`sy`). Why three? What's the cost of always using `sy`?**
*Constraints to weigh:* snoop traffic radius, drain time scales with how far the barrier reaches, energy cost of waking a sleeping interconnect ring, observability of bugs (narrower-than-needed = silent corruption; wider-than-needed = perf only). If the SoC had only one shareability domain, what design simplification appears, and what perf is lost?

**D18. A future SoC promises fully-coherent accelerators (NPU/GPU snoop CPU caches natively). Which artefacts in today's driver stack become obsolete? Which survive?**
*Constraints to weigh:* `dma_map_*` and bounce buffers, cache clean/invalidate, IOVA→PA translation, ordering barriers, doorbell semantics. The interconnect still has finite bandwidth and latency — what stays load-bearing? CXL.cache vs CXL.mem is the real-world version of this question.

**D19. Why is the descriptor → barrier → doorbell → DMA → completion → MSI pipeline *six* stages and not four? What collapses if you merge any pair?**
*Constraints to weigh:* who owns each stage (CPU, interconnect, device, IOMMU), what failure mode each stage isolates, how the on-call engineer would triage if two stages were fused (e.g. doorbell-as-descriptor). The pipeline shape is a debug contract as much as a perf one.

---

### Exit Check - [day20_dma_doorbells_ordering_barriers.md](archive/day20_dma_doorbells_ordering_barriers.md)

- [ ] Drew descriptor → barrier → doorbell → DMA → completion picture from memory.
- [ ] Stated `volatile` vs barrier vs cache maintenance distinctions.
- [ ] Stated x86 TSO vs ARM weak ordering implications for the doorbell pattern.
- [ ] Walked an IOMMU fault to a one-of-two suspect class.
- [ ] Stated why MMIO mappings must be uncached.
- [ ] Computed an NVMe doorbell offset from `DSTRD`.
- [ ] Stated the role of the CQ phase bit.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S14](s14_accelerator_bringup.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S16](s16_gating_dvfs.md)

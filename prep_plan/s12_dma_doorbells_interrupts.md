# S12 - DMA, doorbells, interrupts, and ordering

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [D20 DMA/doorbells/ordering](archive/day20_dma_doorbells_ordering_barriers.md).
- **Must answer:**
  - The generic flow: descriptor in memory → **doorbell MMIO write** → device DMA → completion write → **MSI-X** (itself just a memory write).
  - PCIe ordering: posted writes, read-flushes-writes. Why completion data must be visible before the interrupt.
  - **Story re-tell (story 2, ordering angle):** retell the **ADR ordering hang** in these terms.
- **Work link:** for a DSA job, which step is the doorbell, which writes are posted, and what arrives first at memory: the completion record or the MSI-X?
- **Week narration:** PERST# → LTSSM → L0 → enumeration → BARs/MSI-X → probe → doorbell → DMA → interrupt.
- **Handoff →** The device DMAs to "addresses". Whose addresses are they?

*Story: an accelerator is a PCIe device plus shared virtual memory plus a submission model. This is public-spec level only.*

## Study notes

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

### Concept - [day22_memory_consistency_atomics.md](archive/day22_memory_consistency_atomics.md)

A concurrent primitive offers some combination of three independent guarantees: **atomicity** (no torn read/write), **visibility** (the write becomes observable), **ordering** (relative position vs surrounding ops preserved). `volatile` gives only compiler-side ordering for that variable; **C11 atomics** with explicit memory orders give all three at controllable cost. Pick the **weakest correct** order: relaxed < acquire/release < seq_cst. The canonical lock-free structure is the **SPSC ring** — one writer per pointer + acquire/release, no CAS needed; same shape as NVMe SQ/CQ (Day 23). DSA drills (two-pointer, sliding window, hashmap, stack/queue, BFS/DFS, binary search, bit manipulation, heap) round out a Qualcomm coding round.

### Deep Study Notes - [day22_memory_consistency_atomics.md](archive/day22_memory_consistency_atomics.md)

### 1. Atomicity, visibility, ordering

A primitive can offer:

| Guarantee | Meaning |
|---|---|
| **Atomicity** | operation appears indivisible (no torn read/write) |
| **Visibility** | the write becomes observable to other cores |
| **Ordering** | relative order vs surrounding ops preserved |

`volatile` only gives compiler-side ordering for that variable — *not* CPU ordering, *not* visibility across cores. C11 `<stdatomic.h>` gives all three with explicit `memory_order_*`.

### 2. Memory orders (C11)

| Order | Cost | Use |
|---|---|---|
| `memory_order_relaxed` | cheapest | counter, statistics; no synchronisation needed |
| `memory_order_acquire` | load-acquire | lock acquire, "after this load, all subsequent reads/writes happen-after" |
| `memory_order_release` | store-release | lock release, "before this store, all preceding reads/writes happen-before" |
| `memory_order_acq_rel` | RMW | `compare_exchange` with both ends |
| `memory_order_seq_cst` | strongest, slowest | single global order across all seq_cst ops |

**Pick the weakest correct order.** Default `seq_cst` works but is slow on ARM (full barriers).

### 3. Spinlock — the canonical lock

```c
typedef _Atomic int spinlock_t;

static void spin_lock(spinlock_t *l) {
    int expected;
    do {
        expected = 0;
    } while (!atomic_compare_exchange_weak_explicit(
        l, &expected, 1, memory_order_acquire, memory_order_relaxed));
}

static void spin_unlock(spinlock_t *l) {
    atomic_store_explicit(l, 0, memory_order_release);
}
```

- **Acquire on lock** — subsequent loads/stores cannot reorder above.
- **Release on unlock** — preceding loads/stores cannot reorder below.

**Test-and-test-and-set:** naive CAS in a hot loop hammers the cache line and causes coherence storms (Day 17). Pre-check with a **relaxed load** before CAS to keep the line in Shared state until you really need to acquire:

```c
while (atomic_load_explicit(l, memory_order_relaxed) != 0) cpu_relax();
/* now try the CAS */
```

Real spinlocks back off (`PAUSE` on x86, `YIELD` on ARM) and may sleep after N attempts (hybrid mutex).

**Single-CPU caveat:** spinlocks on a uniprocessor are pointless — the holder can't run while you spin. They make sense in interrupt handlers and SMP kernels where sleeping is forbidden.

### 4. Mutex vs semaphore vs spinlock

| Primitive | Sleep on contention? | When |
|---|---|---|
| **Spinlock** | no — busy-wait | very short critical section, contention rare, cannot sleep (IRQ context) |
| **Mutex** (futex on Linux) | yes | longer critical section, contention possible |
| **Semaphore** | counter; wait blocks if 0 | resource-count semantics, anonymous ownership |
| **Hybrid (adaptive)** | spin briefly, then sleep | best of both for typical workloads |

Always lock/unlock in matched pairs. Never hold a spinlock across a sleep. Prefer RAII / scope-guard patterns where the language allows.

### 5. SPSC lock-free ring (the sweet spot)

Single Producer Single Consumer: one writer per pointer, lock-free with just acquire/release. Memorise this.

```c
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>

#define R 256
typedef struct {
    int buf[R];
    _Atomic uint32_t head;   /* written by consumer */
    _Atomic uint32_t tail;   /* written by producer */
} spsc_t;

static bool spsc_push(spsc_t *r, int v) {
    uint32_t t = atomic_load_explicit(&r->tail, memory_order_relaxed);
    uint32_t h = atomic_load_explicit(&r->head, memory_order_acquire);
    if ((t - h) == R) return false;        /* full */
    r->buf[t & (R - 1)] = v;
    atomic_store_explicit(&r->tail, t + 1, memory_order_release);
    return true;
}

static bool spsc_pop(spsc_t *r, int *v) {
    uint32_t h = atomic_load_explicit(&r->head, memory_order_relaxed);
    uint32_t t = atomic_load_explicit(&r->tail, memory_order_acquire);
    if (h == t) return false;              /* empty */
    *v = r->buf[h & (R - 1)];
    atomic_store_explicit(&r->head, h + 1, memory_order_release);
    return true;
}
```

Same pattern is used by NVMe SQ/CQ (Day 23) and Linux kfifo. **Always pad `head` and `tail` onto separate cache lines** (`alignas(64)`) — otherwise false sharing kills throughput.

### 6. MPSC / MPMC and ABA

- **SPSC** lock-free with just acquire/release.
- **MPSC** producers race on `tail` → need CAS or per-producer slot scheme.
- **MPMC** CAS on both ends, must defend against **ABA**: pop reads ptr A, descheduled, another thread pops A and pushes B and pushes A again (different version), original CAS succeeds with wrong assumption.

ABA mitigations: tagged pointers (high bits = version), hazard pointers, RCU, epoch-based reclamation. You don't need to implement these in an interview; you need to **recognise** the bug class.

### 7. False sharing on the ring

If `head` and `tail` are on the same 64 B cache line, the producer's `tail` write invalidates the consumer's cached `head` (and vice versa) — they ping-pong even though they touch different fields. Throughput collapses. Fix:

```c
typedef struct {
    int buf[R];
    alignas(64) _Atomic uint32_t head;
    alignas(64) _Atomic uint32_t tail;
} spsc_t;
```

### 8. DSA pattern bank (drill these to <10 min each)

| Pattern | Canonical problem |
|---|---|
| Two pointers | Reverse linked list, Floyd cycle detect, middle of list |
| Sliding window | Longest substring no-repeat, max sum window K |
| Hashmap | Two-sum, group anagrams, LRU cache (Day 16 anchor) |
| Stack | Valid parens, next greater element, RPN |
| Queue / BFS | Shortest path in grid, level-order traversal |
| Recursion / DFS | Subsets, permutations, word search |
| Sorting | Quicksort partition, merge two sorted lists |
| Binary search | First/last occurrence, search rotated array |
| Bit manipulation | Single number XOR, count bits, power-of-2 |
| Heap | Top-K, median of stream (two-heap) |

State **time/space complexity unprompted** for every solution: "O(n log n) average from sort, O(n) extra space for the hashmap, in-place would be O(n²) so I traded memory."

### 9. C-specific gotchas in coding rounds

- `malloc` then forget to `free` — interviewer notes it.
- Off-by-one on array bounds — they will run your code mentally.
- `int` overflow on size arithmetic — use `size_t` for indices.
- Returning pointer to local stack — instant fail signal.
- `strcpy` (UB on overlap, no bounds) — use `snprintf` or hand-rolled.

---

### Bugs in this Topic - [day22_memory_consistency_atomics.md](archive/day22_memory_consistency_atomics.md)

- **Counter incremented from two threads with no atomic:** lost updates; final value < expected. Fix: `_Atomic` + `atomic_fetch_add` (relaxed is fine for a pure counter).
- **CAS spin without test-first:** cache-line ping-pong (Day 17 false-sharing / coherence storm). Fix: relaxed pre-check before CAS.
- **`head` and `tail` of SPSC ring on same cache line:** false sharing collapses throughput. Fix: `alignas(64)`.
- **ABA on lock-free pop:** stale CAS succeeds against a recycled pointer. Fix: tagged pointer, hazard pointer, RCU.
- **`volatile` used as substitute for atomic:** no atomicity, no cross-core visibility guarantee. Fix: `_Atomic` with the right memory order.
- **Held a spinlock across a sleeping call (mutex, malloc on a complex allocator, IO):** other CPUs spin forever; on uniproc, deadlock. Fix: never hold a spinlock across anything that could block.
- **Two locks acquired in different order on different threads:** deadlock. Fix: global lock-acquisition order; or use `try_lock` + back-off.

---

### Interview Explanation (60-90 sec) - [day22_memory_consistency_atomics.md](archive/day22_memory_consistency_atomics.md)

> "Concurrency primitives offer three independent guarantees — atomicity, visibility, ordering. `volatile` gives only compiler-side ordering for that variable; for cross-core visibility and CPU ordering you need C11 atomics with explicit memory orders. The rule is pick the weakest correct order: relaxed for counters, acquire/release for lock acquire/release and producer-consumer flags, seq_cst only when you need a single global order. The canonical lock-free structure is the SPSC ring — one writer per pointer means no CAS, just acquire/release; pad `head` and `tail` onto separate cache lines or false sharing kills throughput. The same shape is what NVMe SQ/CQ uses. Spinlocks belong in IRQ context and SMP kernels where you can't sleep; prefer test-and-test-and-set with a relaxed pre-check to avoid coherence storms. MPMC adds the ABA problem — pop reads ptr A, gets descheduled, another thread pops A pushes B pushes A again with different version; the original CAS succeeds against the wrong thing. Defences are tagged pointers, hazard pointers, RCU. For DSA drills: two-pointer, sliding window, hashmap, stack/queue/BFS, DFS/recursion, binary search, bit, heap — each <10 minutes, complexity stated unprompted."

---

### Architect Drills - [day22_memory_consistency_atomics.md](archive/day22_memory_consistency_atomics.md)

*Concurrency primitives are constrained by the memory model, cache-line geometry, and contention shape. Derive which primitive wins under which constraints. No answer key.*

**D1. "Pick the weakest correct memory order" is the rule. What's wrong with always using seq_cst?**
*Constraints to weigh:* seq_cst forces a global total order — full-fence semantics (drain store buffer, snoop, IPI on weak archs), perf cost on every atomic op, lock-free DS throughput drop. Construct the workload where seq_cst is 10× slower than acq/rel.

**D2. SPSC is lock-free; MPSC needs CAS. Reason from first principles why one writer changes the algorithm class.**
*Constraints to weigh:* head/tail single-owner invariant, no race on increment, what breaks at two writers (lost increment, torn pointer). Why is per-producer-slot + epoch reaping a workaround, not a fix?

**D3. ABA defends against reuse-during-CAS. Reason about why hardware doesn't just provide a wider CAS that bakes in a counter.**
*Constraints to weigh:* DCAS (double-word CAS) exists on x86 (cmpxchg16b) and ARM (LDXP/STXP), but most lock-free libs avoid it — generation-counter overflow, portability, why hazard pointers / RCU often win. What would a 128-bit native CAS actually unlock that DCAS doesn't?

**D16. Spinlocks are pointless on uniprocessor. Construct — if you can — a UP workload where spinlocks beat mutexes.**
*Constraints to weigh:* preemption-disabled regions, ISR vs task context, hybrid spin-then-sleep, expected-CS-length distribution. What does Linux actually compile spinlocks to on UP, and why?

**D17. Cache-line padding fixes false sharing but wastes memory. Construct an SPSC ring that is contention-free without padding.**
*Constraints to weigh:* head and tail on different lines, batch updates, separate "shadow" copies in producer/consumer cores. What does kfifo / DPDK rte_ring actually do, and where does the trick fail?

**D18. `volatile` is not a synchronisation primitive. Reason from first principles about why beginners think it is.**
*Constraints to weigh:* compiler-side ordering for that variable vs CPU-side ordering vs cross-core visibility, single-core asymmetry, "it works on x86" trap. What language change would make volatile genuinely sufficient — and why does no language do it?

**D19. Lock-free is not wait-free. Construct a workload where you need wait-free, not just lock-free.**
*Constraints to weigh:* hard real-time deadlines, ISR contexts, attacker-resistant data structures, fairness. What does wait-free buy that lock-free can't, and why isn't it the default?

---

### Exit Check - [day22_memory_consistency_atomics.md](archive/day22_memory_consistency_atomics.md)

- [ ] Wrote spinlock from memory in <5 min.
- [ ] Wrote SPSC ring with correct memory orders + cache-line pad.
- [ ] Articulated false sharing on the ring.
- [ ] Solved 3+ DSA drills back-to-back, complexity stated.
- [ ] Have ABA / hazard pointer answer ready.
- [ ] Reviewed C-specific gotcha list.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S11](s11_pcie_enumeration_driver.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S13](s13_iommu_pasid_ats.md)

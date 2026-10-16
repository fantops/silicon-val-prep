# S09 - Coherence and memory ordering

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [D17 coherence/MESI](archive/day17_coherence_mesi_false_sharing.md), [D22 memory consistency/atomics](archive/day22_memory_consistency_atomics.md).
- **Must answer:**
  - MESI states and snoops. What happens to a core's lines on C6 entry?
  - **I/O coherence:** device DMA must snoop CPU caches. Who guarantees that?
  - x86 TSO and fences. Bug signature: the flag is seen before the data.
- **Work link:** the app polls a completion record that the device writes. What ordering guarantee makes the status byte safe to trust before the data?
- **Career trace:** draw the CPU/SoC diagram from memory: core → L1/L2 → LLC → interconnect → memory controller.
- **Week narration:** register access → DRAM/caches → coherence/ordering.
- **Handoff →** A device must come alive on PCIe. What happens from PERST# deassert onward?

*Story: follow one device from reset release to its driver being ready to move data.*

## Study notes

### Concept - [day17_coherence_mesi_false_sharing.md](archive/day17_coherence_mesi_false_sharing.md)

In a multicore SoC each core has private L1/L2 caches; **MESI** (or its variants MOESI / MESIF) keeps them coherent at cache-line granularity. The performance bug class that follows is **false sharing**: two cores writing different variables that happen to share a 64 B line. The debug counter is **HITM** (Hit-Modified). Today closes Layer 1 and prepares the DMA discussion of Day 20.

### Deep Study Notes - [day17_coherence_mesi_false_sharing.md](archive/day17_coherence_mesi_false_sharing.md)

### 1. Why coherence

```
Core 0 writes X = 7;     /* in Core 0's L1 */
Core 1 reads X;          /* without coherence: reads stale value */
```

A protocol must ensure Core 1's read either (a) sees the new value, or (b) is forced to fetch it. **MESI** is the standard snoop-based protocol used in x86 and many ARM cores.

### 2. The four MESI states

| State | Meaning | Dirty? | Shared? | Snoop response |
|---|---|---|---|---|
| **M** Modified | only copy, written; memory stale | yes | no | supply data, transition I or S |
| **E** Exclusive | only copy, clean (matches memory) | no | no | drop to S on read snoop |
| **S** Shared | multiple caches may have copies, all clean | no | yes | drop to I on invalidate |
| **I** Invalid | not present / stale | — | — | must fetch from memory or peer |

### 3. The four-step walkthrough (memorise)

Two cores, each with L1; address X starts only in memory.

```
1. Core 0 READ X      → miss → fetch from mem
   C0:E   C1:I

2. Core 1 READ X      → miss → snoops C0's E → both go S
   C0:S   C1:S

3. Core 0 WRITE X=42  → invalidate C1 → upgrade to M
   C0:M   C1:I        memory: stale

4. Core 1 READ X      → miss → snoops C0's M → cache-to-cache
   forward, optional write-back
   C0:S   C1:S        memory: now 42 (after write-back)
```

### 4. Why E exists

E lets a core silently upgrade a clean exclusive line to M without sending an invalidate (no other cache has it, so nothing to invalidate). Without E, every write to a clean line would cost a bus transaction. E is a performance optimisation, not correctness.

### 5. Variants

- **MOESI (AMD)** — adds **O** (Owned): one cache holds dirty data and serves snoops without writing back, saving DRAM bandwidth on read-shared dirty lines.
- **MESIF (Intel)** — adds **F** (Forward): one of the S-state copies is designated to respond to snoops, avoiding redundant responses.

### 6. Coherence cost

| Event | Cost |
|---|---|
| Local L1 hit | ~4 cycles |
| Snoop to peer in same socket | ~50-80 cycles |
| Cross-socket NUMA snoop | ~100-300 cycles |
| Modified-line transfer (HITM) | similar to remote-cache snoop |

Each invalidation forces the receiver to refetch on next access. In a tight loop this happens millions of times per second.

### 7. Snoop-based vs directory-based

- **Snoop:** broadcast every coherence event. Bandwidth $O(N)$ in cores. Fine for 4-8 cores; expensive at 16+.
- **Directory:** keep a sharer list per line; send point-to-point messages. Bandwidth $O(\text{sharers})$.
- **Snoop filters** sit between: a directory-like structure that suppresses snoops to cores that demonstrably do not have the line.

### 8. False sharing — the canonical bug

```c
struct counters {
    int core0_count;   /* address X */
    int core1_count;   /* address X+4 — same 64 B line */
};
```

Both ints live on the same 64 B cache line. Each core writes "its own" counter but the protocol works at line granularity:

```
C0 writes core0_count → line M in C0, invalidate C1.
C1 writes core1_count → line M in C1, invalidate C0.
C0 next write → refetch (~50-80 cyc) → write → invalidate C1.
... ping-pong, millions of times per second ...
```

Throughput collapses 5-10× even though the source code looks independent.

### 9. Fix patterns

```c
/* per-line padding */
struct counters {
    int c0; char pad0[60];
    int c1; char pad1[60];
};

/* alignment attribute */
struct counters {
    int c0 __attribute__((aligned(64)));
    int c1 __attribute__((aligned(64)));
};

/* per-CPU array slots */
struct cpu_local {
    long val;
} __attribute__((aligned(64))) per_cpu[NR_CPUS];
```

Padding only works if the field starts at a 64 B boundary. If `malloc` returned 16 B alignment, the struct itself may straddle a line — pair `aligned(64)` with `posix_memalign(&p, 64, sizeof *p)`.

### 10. Cost of padding

- Memory: 60+ wasted bytes per padded field.
- For an array of 1 M such structs: 60 MB of wasted DRAM, plus a worse single-threaded cache footprint.
- Only pad fields you have measured to be contended, or known per-CPU hot spots.

### 11. Detecting false sharing in production

| Counter | What it measures | High value means |
|---|---|---|
| `MEM_LOAD_L3_HIT_RETIRED.XSNP_HITM` (Intel) | loads that hit a peer cache's M line | true or false sharing |
| `OFFCORE_RESPONSE.SNOOP_HITM` | snoops returning M data | sharing hot spot |
| `L2_RQSTS.ALL_DEMAND` | overall L2 pressure | sometimes secondary signal |

Tools:

- **Linux:** `perf stat -e ...`, **`perf c2c`** (cache-to-cache analysis with line-level attribution).
- **Intel:** VTune (Memory Access / Threading).
- **Windows / your Microsoft work:** WPR/WPA can capture ETW PMU counter events.

### 12. Tying back to system bugs

- **DMA visibility (Day 20):** the device is just another agent that must see the latest copy. If the descriptor sits in M in a CPU cache and the platform is non-coherent, the device DMA reads stale DRAM.
- **NUMA cross-socket performance (Day 29):** a hot shared structure pulled across UPI/CXL is the same picture as MESI snoop, just with worse latency.
- **Locks and atomics:** every contended lock is a controlled false-sharing pattern — the cache line containing the lock word ping-pongs. That's why high-contention locks scale badly.

---

### Bugs in this Topic - [day17_coherence_mesi_false_sharing.md](archive/day17_coherence_mesi_false_sharing.md)

- **False sharing on per-thread counters:** 5-10× slowdown that disappears with `__attribute__((aligned(64)))` on each counter. Detected with HITM counter or `perf c2c`. Padding can also fail silently if the struct itself isn't 64 B-aligned.
- **Stale descriptor read by device:** classic DMA bug — descriptor written by CPU sits in M in L1; non-coherent device reads stale DRAM. Prevented by cache-line clean + barrier + doorbell on the DMA path. Picture is exactly MESI's M state without snooping.
- **Coherence storm under directory pressure:** at 32+ cores, one hot shared variable saturates the snoop fabric. Symptom: latency increases with core count even though work doesn't. Fix: per-core sharding, RCU, or use of read-mostly / write-rare patterns.
- **Lock convoy / cache-line ping-pong on a hot mutex:** the mutex's word is the contended line. Symptom looks like CPU saturation with low IPC. Fix: shard the lock, use lock-free data structures, or reduce critical-section size.

---

### Interview Explanation (60-90 sec) - [day17_coherence_mesi_false_sharing.md](archive/day17_coherence_mesi_false_sharing.md)

> "MESI keeps multicore caches coherent at cache-line granularity. Each line is in Modified, Exclusive, Shared, or Invalid; reads and writes drive transitions through snoops or invalidates. Intel's MESIF adds an F state to designate one S copy to respond, AMD's MOESI adds O so a dirty line can be shared without write-back. The performance bug class that follows is false sharing — two cores writing different variables on the same 64 B line invalidate each other on every write, costing 50-80 cycles per refetch. I detect it with the HITM counter or `perf c2c`; I fix it with cache-line padding or `__attribute__((aligned(64)))`, paired with `posix_memalign` so the struct itself starts on a line boundary. The same picture explains DMA-visibility bugs: the device is another coherence agent, and a descriptor still in M in a CPU cache will be read stale on a non-coherent path — fixed by clean + barrier + doorbell."

---

### Architect Drills - [day17_coherence_mesi_false_sharing.md](archive/day17_coherence_mesi_false_sharing.md)

*Coherence is a policy decision under interconnect, area, and workload constraints. Derive why MESI/MOESI/MESIF look the way they do, and what changes when the constraints shift. No answer key.*

**D1. Why MESI specifically — why not just M and I (dirty/invalid) and let everything else miss?**
*Constraints to weigh:* read-shared workloads (mostly-read structures), invalidate-on-every-share traffic on the snoop bus, ordering of upgrades (S→M needs an invalidate), critical-section overhead. Construct the workload where M+I alone catastrophically loses, and the workload where it'd be fine.

**D2. Why did AMD add O (MOESI) and Intel add F (MESIF)? Construct the workload that forces each.**
*Constraints to weigh:* dirty-line forwarding without writeback (O), forwarder selection in a multi-S-copy line (F), interconnect topology (ring vs mesh vs HT), writeback bandwidth pressure on shared LLC. Why didn't both vendors converge — what's structurally different about their fabrics?

**D3. Snoop vs directory coherence. Derive the core count where each crosses over.**
*Constraints to weigh:* snoop traffic = $O(\text{cores})$ per coherence event vs directory state = $O(\text{sharers})$ per line, directory storage cost (1-3% of LLC), latency of an indirected request. Pick AMD's CCX, Intel's mesh, or ARM's CMN and explain its choice.

**D16. Cache-line size sets the false-sharing penalty. If line size dropped to 16 B, what bug classes disappear and which appear?**
*Constraints to weigh:* DRAM burst length mismatch, tag-array overhead 4×, prefetcher reach, lock-word neighbour pollution, per-thread stat counter packing. Why hasn't anyone shipped a 16 B line size — what would it take?

**D17. Coherence is "free" up to the snoop fabric saturating. At 256 cores, derive what changes.**
*Constraints to weigh:* snoop-filter capacity, NUMA traffic, per-line directory fanout, hierarchical coherence (cluster + global). Why does GPU compute use *non*-coherent caches with explicit synchronisation, and where could a CPU borrow that without breaking software?

**D18. Padded per-thread counters "fix" false sharing but waste DRAM. Construct a counter that's both compact and contention-free.**
*Constraints to weigh:* per-CPU storage (sum on read), CAS-free updates, cache-line packing of *unrelated* counters, RCU read-mostly patterns, observability (sum cost). What do Linux's `percpu` counters actually look like, and where do they still cost?

**D19. CXL.cache lets accelerators be peers in the coherence domain. Which Day-5 bug classes follow the accelerator into the new design?**
*Constraints to weigh:* false sharing across CPU and NPU, MESI states held by an NPU L1, snoop-filter sizing for non-CPU peers, who holds M in a heterogeneous workload. What new bug class appears that didn't exist when accelerators were DMA-only?

---

### Exit Check - [day17_coherence_mesi_false_sharing.md](archive/day17_coherence_mesi_false_sharing.md)

- [ ] Walked MESI for the 4-step sequence from memory.
- [ ] Distinguished MOESI and MESIF in one sentence each.
- [ ] Wrote the false-sharing struct + padded fix.
- [ ] Stated the perf counter (HITM) and tool (`perf c2c`) for detection.
- [ ] Stated why padding alone fails if the struct isn't 64 B-aligned.
- [ ] Mapped descriptor-then-doorbell DMA to MESI's M state.

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

Previous: [S08](s08_memory_caches.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S10](s10_pcie_linkup.md)

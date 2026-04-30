# Day 17 — Concurrency Primitives, Lock-free Queues, DSA Drills

## Concept

A concurrent primitive offers some combination of three independent guarantees: **atomicity** (no torn read/write), **visibility** (the write becomes observable), **ordering** (relative position vs surrounding ops preserved). `volatile` gives only compiler-side ordering for that variable; **C11 atomics** with explicit memory orders give all three at controllable cost. Pick the **weakest correct** order: relaxed < acquire/release < seq_cst. The canonical lock-free structure is the **SPSC ring** — one writer per pointer + acquire/release, no CAS needed; same shape as NVMe SQ/CQ (Day 11). DSA drills (two-pointer, sliding window, hashmap, stack/queue, BFS/DFS, binary search, bit manipulation, heap) round out a Qualcomm coding round.

## Why this day in the flow

Days 5 (MESI / false sharing) and 16 (memory model) gave you the *why*; Day 17 gives you the *write it under interview pressure* skill. Coding rounds at Qualcomm/Intel/Microsoft test both systems-C (spinlock, ring, doorbell harness) and classic DSA. Today is the rep day.

In the Layer map this is the **coding-readiness layer that sits across Layers 1, 5, and 6**.

---

## Deep Study Notes

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

**Test-and-test-and-set:** naive CAS in a hot loop hammers the cache line and causes coherence storms (Day 5). Pre-check with a **relaxed load** before CAS to keep the line in Shared state until you really need to acquire:

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

Same pattern is used by NVMe SQ/CQ (Day 11) and Linux kfifo. **Always pad `head` and `tail` onto separate cache lines** (`alignas(64)`) — otherwise false sharing kills throughput.

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
| Hashmap | Two-sum, group anagrams, LRU cache (Day 4 anchor) |
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

## Bugs in this Topic

- **Counter incremented from two threads with no atomic:** lost updates; final value < expected. Fix: `_Atomic` + `atomic_fetch_add` (relaxed is fine for a pure counter).
- **CAS spin without test-first:** cache-line ping-pong (Day 5 false-sharing / coherence storm). Fix: relaxed pre-check before CAS.
- **`head` and `tail` of SPSC ring on same cache line:** false sharing collapses throughput. Fix: `alignas(64)`.
- **ABA on lock-free pop:** stale CAS succeeds against a recycled pointer. Fix: tagged pointer, hazard pointer, RCU.
- **`volatile` used as substitute for atomic:** no atomicity, no cross-core visibility guarantee. Fix: `_Atomic` with the right memory order.
- **Held a spinlock across a sleeping call (mutex, malloc on a complex allocator, IO):** other CPUs spin forever; on uniproc, deadlock. Fix: never hold a spinlock across anything that could block.
- **Two locks acquired in different order on different threads:** deadlock. Fix: global lock-acquisition order; or use `try_lock` + back-off.

---

## Interview Explanation (60-90 sec)

> "Concurrency primitives offer three independent guarantees — atomicity, visibility, ordering. `volatile` gives only compiler-side ordering for that variable; for cross-core visibility and CPU ordering you need C11 atomics with explicit memory orders. The rule is pick the weakest correct order: relaxed for counters, acquire/release for lock acquire/release and producer-consumer flags, seq_cst only when you need a single global order. The canonical lock-free structure is the SPSC ring — one writer per pointer means no CAS, just acquire/release; pad `head` and `tail` onto separate cache lines or false sharing kills throughput. The same shape is what NVMe SQ/CQ uses. Spinlocks belong in IRQ context and SMP kernels where you can't sleep; prefer test-and-test-and-set with a relaxed pre-check to avoid coherence storms. MPMC adds the ABA problem — pop reads ptr A, gets descheduled, another thread pops A pushes B pushes A again with different version; the original CAS succeeds against the wrong thing. Defences are tagged pointers, hazard pointers, RCU. For DSA drills: two-pointer, sliding window, hashmap, stack/queue/BFS, DFS/recursion, binary search, bit, heap — each <10 minutes, complexity stated unprompted."

---

## Practice Q&A

**Q1. Spinlock vs mutex — when each?**
Spinlock: very short CS (<µs), rare contention, cannot sleep (IRQ). Mutex: longer CS, contention possible, blocking thread is acceptable. Hybrid: spin briefly, then sleep.

**Q2. Acquire / release in one minute.**
Acquire on a load: no subsequent reads/writes can reorder before. Release on a store: no preceding reads/writes can reorder after. Pair them: producer release-stores a flag; consumer acquire-loads it; everything before the release is visible after the acquire.

**Q3. Why can SPSC be lock-free but MPSC/MPMC need locks or CAS?**
SPSC has one writer per pointer → no CAS needed for the writer side; only acquire/release for visibility. MPSC: multiple producers race on `tail` → CAS or per-producer slot. MPMC: CAS both ends + ABA defence.

**Q4. False sharing on a ring — describe and fix.**
Producer writes `tail`, consumer reads `head`; if they share a 64 B line, every write invalidates the other side's cached copy → cache-line ping-pong. Fix: `alignas(64)` on each.

**Q5. ABA — what is it?**
Pop reads ptr A → descheduled → another thread pops A, pushes B, pushes A again at a different version → original CAS succeeds against the wrong "A". Fix: tagged pointer / hazard pointer / RCU / epoch GC.

**Q6. Why is `volatile` not enough for cross-core synchronisation?**
`volatile` only constrains the compiler for that variable's accesses; doesn't constrain CPU reordering and doesn't ensure cross-core visibility. Use `_Atomic` with the right memory order.

**Q7. State complexity for two-sum (sorted vs unsorted).**
Unsorted: O(n) time + O(n) space (hashmap). Sorted: O(n) time + O(1) space (two pointers).

**Q8. BFS vs DFS — which to use for shortest path on an unweighted grid?**
BFS — first time you reach the target is guaranteed shortest because all edges cost 1.

**Q9. Top-K largest in a stream — data structure?**
Min-heap of size K. Push every element; if heap size > K, pop. O(n log K).

**Q10. Spinlock on a uniprocessor — what happens?**
Pointless — holder can't run while you spin. Yielding mutex is the right choice.

---

## Coding A — Systems C: spinlock + SPSC ring (full)

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <pthread.h>

/* Spinlock */
typedef _Atomic int spinlock_t;
static void spin_lock(spinlock_t *l) {
    int e;
    do {
        while (atomic_load_explicit(l, memory_order_relaxed) != 0) {}
        e = 0;
    } while (!atomic_compare_exchange_weak_explicit(
        l, &e, 1, memory_order_acquire, memory_order_relaxed));
}
static void spin_unlock(spinlock_t *l) {
    atomic_store_explicit(l, 0, memory_order_release);
}

/* SPSC ring with cache-line padding */
#define R 1024
typedef struct {
    int buf[R];
    alignas(64) _Atomic uint32_t head;
    alignas(64) _Atomic uint32_t tail;
} spsc_t;

static bool spsc_push(spsc_t *r, int v) {
    uint32_t t = atomic_load_explicit(&r->tail, memory_order_relaxed);
    uint32_t h = atomic_load_explicit(&r->head, memory_order_acquire);
    if ((t - h) == R) return false;
    r->buf[t & (R - 1)] = v;
    atomic_store_explicit(&r->tail, t + 1, memory_order_release);
    return true;
}
static bool spsc_pop(spsc_t *r, int *v) {
    uint32_t h = atomic_load_explicit(&r->head, memory_order_relaxed);
    uint32_t t = atomic_load_explicit(&r->tail, memory_order_acquire);
    if (h == t) return false;
    *v = r->buf[h & (R - 1)];
    atomic_store_explicit(&r->head, h + 1, memory_order_release);
    return true;
}

/* Demo: counter under spinlock + SPSC test */
static spinlock_t lock = 0;
static long counter = 0;

static void *bump(void *arg) {
    int n = *(int*)arg;
    for (int i = 0; i < n; i++) {
        spin_lock(&lock);
        counter++;
        spin_unlock(&lock);
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    int n = 100000;
    pthread_create(&t1, NULL, bump, &n);
    pthread_create(&t2, NULL, bump, &n);
    pthread_join(t1, NULL); pthread_join(t2, NULL);
    printf("counter=%ld (expected %d)\n", counter, 2*n);

    spsc_t ring; atomic_init(&ring.head, 0); atomic_init(&ring.tail, 0);
    for (int i = 0; i < 5; i++) spsc_push(&ring, i*i);
    int v;
    while (spsc_pop(&ring, &v)) printf("popped %d\n", v);
    return 0;
}
```

## Coding B — Interview DS&A: Evaluate Reverse Polish Notation (LC 150)

Stack walk over tokens.

```python
def evalRPN(tokens):
    st, ops = [], {'+': lambda a,b:a+b, '-': lambda a,b:a-b,
                   '*': lambda a,b:a*b, '/': lambda a,b:int(a/b)}
    for t in tokens:
        if t in ops:
            b = st.pop(); a = st.pop()
            st.append(ops[t](a, b))
        else:
            st.append(int(t))
    return st[0]
```

---

## System Thinking — Why queues are the universal fabric primitive

**Frame:** every interesting system has multiple producers and consumers running at different rates that need to communicate. **A queue is the single data structure that decouples them in time** — producer doesn't wait for consumer, consumer doesn't wait for producer (within capacity). NIC RX rings, NVMe SQ/CQ, GPU command buffers, kernel work queues, NPU command streams — all queues. Master the lock-free queue and you understand 80% of system-level data movement. Lock-free isn't an optimization; it's *required* whenever the producer and consumer can't share a lock (different cores, different IPs, CPU↔device).

### Q: Why lock-free? Aren't locks fine if uncontended?

Uncontended locks are cheap (~10 ns). Contended locks are catastrophic:
- **Cache line bouncing** — the lock word ping-pongs M→I→M between cores (Day 5).
- **Convoy effect** — a context-switched lock holder blocks every waiter.
- **Priority inversion** — a low-prio holder blocks a high-prio waiter.
- **Cross-domain impossibility** — you cannot share a mutex with a device. Devices don't run mutex code.

Lock-free queues use atomic CAS or producer-consumer index pairs to avoid all of this. The cost is *much* harder reasoning about correctness (memory order, ABA, retry loops). **Use lock-free where you must (cross-IP, hot path, real-time); use locks where you can (cold paths, complex critical sections).**

### Q: SPSC vs MPMC — why so many flavors?

Because the *minimum* synchronization depends on the producer/consumer count:
- **SPSC** (single producer, single consumer): just two indices (`head`, `tail`), no atomics needed for the indices themselves — only memory barriers between data write and index update. **Fastest possible queue.** This is the NIC ring, the NVMe SQ.
- **MPSC / SPMC**: one side needs atomic CAS to claim a slot, the other side is single. Used in work-stealing, telemetry collection.
- **MPMC**: both sides atomic CAS, sequence numbers per slot to avoid ABA, much more state and slower per op. Used in general-purpose work queues.

**You always pick the most-restrictive flavor that fits the workload.** Using MPMC where SPSC suffices is a 5-10× perf loss for nothing.

### Q: What is the ABA problem and why does it haunt lock-free code?

A classic CAS bug:
1. Thread A reads pointer `head = X`.
2. Thread A is preempted.
3. Thread B pops X, pushes Y, pushes X again. `head = X` again, but the list shape has changed.
4. Thread A wakes, does `CAS(&head, X, X->next)` — succeeds! — but `X->next` is stale, pointing into freed memory.

The value matched, but the *world changed and changed back*. CAS only checks the value, not the history. **Fix:** tagged pointers (pack a generation counter into spare pointer bits, increment on every modify), hazard pointers, or RCU. *Most lock-free queue bugs in the wild are unhandled ABA.*

### Q: Why cache-line padding between `head` and `tail` in every ring?

Day 5 false sharing. Producer writes `head` constantly; consumer writes `tail` constantly. If they're on the same cache line, the line bounces between cores on every op — hundreds of cycles per push/pop instead of single cycles. **Pad `head` and `tail` to separate 64-byte cache lines** (`alignas(64)` between them) and throughput jumps 5-10×. *This single change is the difference between a textbook lock-free queue and a production one.*

### One-liner

*"Queues are the universal primitive for decoupling producers and consumers across cores, IPs, and CPU↔device boundaries. Lock-free is required where locks can't reach (devices, real-time, hot paths). Pick the most-restrictive flavor that fits — SPSC is dramatically faster than MPMC. ABA is the value-matched-but-world-changed bug that makes naive CAS wrong; tagged pointers or hazard pointers fix it. Cache-line padding between head and tail is the difference between toy code and shipped code."*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **queues as fabric** — *what:* producer/consumer decouple / *why:* absorb rate mismatch, cross-domain handoff / *how:* ring buffer with head/tail / *connects:* NVMe SQ, NIC rings, work-stealing schedulers
- **lock vs lock-free** — *what:* mutex blocks vs CAS retries / *why:* contention vs progress guarantee / *how:* lock = futex, lock-free = atomic ops + ABA defense / *connects:* RCU, hazard pointers, where you can't take a lock (ISR, device)
- **atomics** — *what:* indivisible RMW / *why:* no torn reads under concurrency / *how:* LL/SC (ARM) or `LOCK` prefix (x86) / *connects:* must be aligned, takes line into M (Day 5)
- **ring buffer (SPSC)** — *what:* fixed-size circular, single producer + single consumer / *why:* O(1) enq/deq, no allocator on hot path / *how:* power-of-2 mask, no CAS needed / *connects:* false sharing if head and tail share a line
- **ABA** — *what:* value matches but the world changed under you / *why:* naive CAS sees same pointer, different object / *how:* tagged pointers (counter in low bits) or hazard pointers / *connects:* lock-free stack/queue correctness

---

## Bug Anchor

- **Day 5 (MESI / false sharing):** ring `head`/`tail` cache-line padding lives there.
- **Day 8 (DMA / doorbells):** the same SPSC pattern for hardware queues.
- **Day 11 (NVMe SQ/CQ):** the production instance of SPSC + phase bit.
- **Day 16 (memory model):** acquire/release map onto ARM `LDAR/STLR` and `DMB`.

## Exit Check

- [ ] Wrote spinlock from memory in <5 min.
- [ ] Wrote SPSC ring with correct memory orders + cache-line pad.
- [ ] Articulated false sharing on the ring.
- [ ] Solved 3+ DSA drills back-to-back, complexity stated.
- [ ] Have ABA / hazard pointer answer ready.
- [ ] Reviewed C-specific gotcha list.

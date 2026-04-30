# Day 05 — Cache Coherence (MESI), False Sharing, Snoop Debug

## Concept

In a multicore SoC each core has private L1/L2 caches; **MESI** (or its variants MOESI / MESIF) keeps them coherent at cache-line granularity. The performance bug class that follows is **false sharing**: two cores writing different variables that happen to share a 64 B line. The debug counter is **HITM** (Hit-Modified). Today closes Layer 1 and prepares the DMA discussion of Day 8.

## Why this day in the flow

Day 4 introduced the cache line and write-back. Day 5 takes the next step: what happens when *two cores* see the same line. This is where the **fourth C of misses (coherence)** lives, where the most common silent-perf bug class lives, and where the snoop traffic that hurts NUMA scaling lives. Day 8 (DMA / doorbells / barriers) reuses the same picture: the device acts like another "core" that needs an up-to-date copy of the line.

In the Layer map this is the second half of **Layer 1**.

---

## Deep Study Notes

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

- **DMA visibility (Day 8):** the device is just another agent that must see the latest copy. If the descriptor sits in M in a CPU cache and the platform is non-coherent, the device DMA reads stale DRAM.
- **NUMA cross-socket performance (Day 15):** a hot shared structure pulled across UPI/CXL is the same picture as MESI snoop, just with worse latency.
- **Locks and atomics:** every contended lock is a controlled false-sharing pattern — the cache line containing the lock word ping-pongs. That's why high-contention locks scale badly.

---

## Bugs in this Topic

- **False sharing on per-thread counters:** 5-10× slowdown that disappears with `__attribute__((aligned(64)))` on each counter. Detected with HITM counter or `perf c2c`. Padding can also fail silently if the struct itself isn't 64 B-aligned.
- **Stale descriptor read by device:** classic DMA bug — descriptor written by CPU sits in M in L1; non-coherent device reads stale DRAM. Prevented by cache-line clean + barrier + doorbell on the DMA path. Picture is exactly MESI's M state without snooping.
- **Coherence storm under directory pressure:** at 32+ cores, one hot shared variable saturates the snoop fabric. Symptom: latency increases with core count even though work doesn't. Fix: per-core sharding, RCU, or use of read-mostly / write-rare patterns.
- **Lock convoy / cache-line ping-pong on a hot mutex:** the mutex's word is the contended line. Symptom looks like CPU saturation with low IPC. Fix: shard the lock, use lock-free data structures, or reduce critical-section size.

---

## Interview Explanation (60-90 sec)

> "MESI keeps multicore caches coherent at cache-line granularity. Each line is in Modified, Exclusive, Shared, or Invalid; reads and writes drive transitions through snoops or invalidates. Intel's MESIF adds an F state to designate one S copy to respond, AMD's MOESI adds O so a dirty line can be shared without write-back. The performance bug class that follows is false sharing — two cores writing different variables on the same 64 B line invalidate each other on every write, costing 50-80 cycles per refetch. I detect it with the HITM counter or `perf c2c`; I fix it with cache-line padding or `__attribute__((aligned(64)))`, paired with `posix_memalign` so the struct itself starts on a line boundary. The same picture explains DMA-visibility bugs: the device is another coherence agent, and a descriptor still in M in a CPU cache will be read stale on a non-coherent path — fixed by clean + barrier + doorbell."

---

## Practice Q&A

**Q1. Walk MESI for: C0 reads X, C1 reads X, C0 writes X, C1 reads X.**
C0:E → both S → C0:M, C1:I → snoop M, cache-to-cache, both S (mem updated).

**Q2. Why does E exist?**
Lets a core silently upgrade clean exclusive to M without an invalidate transaction.

**Q3. Two threads each increment "their own" counter; throughput is 10× too low. Cause and confirmation?**
False sharing. Confirm with HITM perf counter on the line containing both counters, or `perf c2c`. Fix with cache-line padding and aligned allocation.

**Q4. Why does false sharing only show on multicore?**
No coherence traffic with one core; both "shared" variables sit in the same line and stay valid.

**Q5. Padded by 64 bytes and bug didn't go away — what did you miss?**
Struct itself wasn't 64 B-aligned, so the second field still landed on the same line. Use `aligned(64)` on the struct AND on the field, plus `posix_memalign`.

**Q6. MOESI vs MESIF in one sentence each?**
MOESI = "owned dirty line can be shared without write-back." MESIF = "one S copy is designated forwarder for snoops."

**Q7. Snoop-based vs directory-based — which scales?**
Directory scales because cost is $O(\text{sharers})$ not $O(\text{cores})$. Snoop filters bridge the two.

**Q8. Cost trade-off of padding?**
Wasted DRAM and worse single-threaded footprint. Only pad fields you've measured contended.

**Q9. How does the descriptor-then-doorbell DMA bug map to MESI?**
The CPU cache holds the descriptor in M; the non-coherent device reads DRAM (stale). Fix: clean (write back) + barrier + doorbell.

**Q10. Why do contended mutexes scale badly?**
The mutex word is on a single line that ping-pongs between every contender. Same physics as false sharing, intentional this time.

---

## Coding A — Systems C: MESI step-machine + false-sharing layout check

```c
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

typedef enum { I = 0, S, E, M } mesi_t;
static const char *NAME[] = { "I", "S", "E", "M" };

typedef struct { mesi_t st; int v; } cline_t;
static cline_t c0, c1;
static int mem = 100;

static void show(const char *what) {
    printf("%-22s | C0:%s(%d) C1:%s(%d) mem:%d\n",
           what, NAME[c0.st], c0.v, NAME[c1.st], c1.v, mem);
}

static void rd(int who) {
    cline_t *me = who ? &c1 : &c0;
    cline_t *other = who ? &c0 : &c1;
    if (me->st != I) { show(who?"C1 read (hit)":"C0 read (hit)"); return; }
    if (other->st == M) {                     /* HITM: cache-to-cache + WB */
        me->v = other->v; mem = other->v;
        me->st = S; other->st = S;
        show(who?"C1 read (HITM→S)":"C0 read (HITM→S)");
    } else if (other->st == E || other->st == S) {
        me->v = other->v; me->st = S; other->st = S;
        show(who?"C1 read (snoop→S)":"C0 read (snoop→S)");
    } else {                                  /* fetch from mem */
        me->v = mem; me->st = E;
        show(who?"C1 read (mem→E)":"C0 read (mem→E)");
    }
}
static void wr(int who, int v) {
    cline_t *me = who ? &c1 : &c0;
    cline_t *other = who ? &c0 : &c1;
    other->st = I;
    me->v = v; me->st = M;
    show(who?"C1 write":"C0 write");
}

int main(void) {
    show("init");
    rd(0);          /* C0 → E */
    rd(1);          /* both S */
    wr(0, 42);      /* C0 M, C1 I */
    rd(1);          /* HITM, both S */
    wr(1, 99);      /* C1 M, C0 I */

    /* False sharing layout check */
    struct bad  { int a; int b; };
    struct good { int a; char pad[60]; int b; };
    struct bad B; struct good G;
    printf("\nbad:  &a=%p &b=%p sameline=%d\n",
           (void*)&B.a, (void*)&B.b,
           ((uintptr_t)&B.b/64) == ((uintptr_t)&B.a/64));
    printf("good: &a=%p &b=%p sameline=%d\n",
           (void*)&G.a, (void*)&G.b,
           ((uintptr_t)&G.b/64) == ((uintptr_t)&G.a/64));
    return 0;
}
```

## Coding B — Interview DS&A: Linked-List Cycle (LC 141, Floyd)

Two pointers, different speeds — same intuition as snoop races.

```python
def hasCycle(head):
    slow = fast = head
    while fast and fast.next:
        slow = slow.next
        fast = fast.next.next
        if slow is fast: return True
    return False
```

---

## System Thinking — Why coherence is the *price* of multicore

**Frame:** a single core programmer expects one memory: write here, read it back, get the value. Add a second core with its own L1, and that illusion breaks instantly. Coherence is the silicon-and-protocol cost of *preserving the illusion* despite N private caches. Every coherence "bug" is the protocol becoming visible — false sharing, lock contention, atomic-op latency are all the protocol charging you for the abstraction.

### Q: Why do we have a coherence protocol at all? Why not just share L1?

Because an L1 shared across N cores would be slow (more ports, longer wires) and contended (every core competes for it). Per-core private L1s are required for performance. But then the same line can exist in two L1s with different values → broken. Coherence is the answer: a state machine (MESI / MOESI / MESIF) on every cache line that tracks who has it and in what state, and a protocol that broadcasts (snoop) or directs (directory) state changes.

**Tradeoff:** snoop is cheap at low core count (broadcast on a shared bus), expensive at high count (bandwidth scales with N²). Directory is the inverse — adds latency for the lookup but scales to many cores. That's why low-core-count CPUs are snoop and big servers/SoCs use directory-based (or hybrid).

### Q: What is false sharing, *really*?

A fourth class of cache miss (after compulsory, capacity, conflict): a **coherence miss**. Two cores write to *different* variables that happen to share a cache line. The protocol can't see that the writes don't overlap — it tracks lines, not bytes. So the line bounces M→I→M between cores on every write. The data is independent, but the line is shared. Cost: hundreds of cycles per write, instead of single cycles.

**The fix is structural, not algorithmic:** pad the data so each producer's hot field lives on its own cache line. `alignas(64)` per-thread struct, or a 64-byte padding member. *This is why every lock-free queue you'll ever see has cache-line padding between `head` and `tail` — Day 17.*

### Q: Why are atomic operations so expensive?

Because an atomic RMW (`fetch_add`, `compare_exchange`) has to acquire the line in **M** state exclusively, do the operation, and (on weakly-ordered systems) emit a barrier. If another core has the line in S, you must invalidate it first → coherence transaction. Every contended atomic is a cache-line bounce. *That's why uncontended atomics are ~10 cycles and contended atomics are ~100-1000 cycles.*

The lesson: **don't measure atomic cost in isolation.** Measure it under the contention you actually expect. A counter incremented by all 16 cores will absolutely dominate the workload.

### One-liner

*"Coherence is the silicon tax for keeping the illusion of one memory across N private caches. Every line has a state (MESI), every state change is a protocol transaction, and every protocol transaction shows up as latency. False sharing, atomic contention, and lock costs are all the same phenomenon — the protocol charging you because data sharing was implicit in the layout. The fix is structural: pad to a cache line and own the line."*

---

## Bug Anchor

- **Day 8:** descriptor-then-doorbell ordering — same MESI picture, device as the second agent.
- **Day 15:** NUMA snoop traffic dominates cross-socket performance.
- **Locks / atomics anywhere later:** every contended atomic is a controlled false-sharing pattern.

## Exit Check

- [ ] Walked MESI for the 4-step sequence from memory.
- [ ] Distinguished MOESI and MESIF in one sentence each.
- [ ] Wrote the false-sharing struct + padded fix.
- [ ] Stated the perf counter (HITM) and tool (`perf c2c`) for detection.
- [ ] Stated why padding alone fails if the struct isn't 64 B-aligned.
- [ ] Mapped descriptor-then-doorbell DMA to MESI's M state.

# Day 05 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day05_coherence_mesi_false_sharing.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: MESI step-machine + false-sharing layout check

**Problem statement:** Model two cores' L1 lines and one main-memory cell. Implement `rd(who)` and `wr(who, v)` that mutate `(cline_t c0, c1)` according to MESI rules — including HITM (other core in M forwards and both go to S) and write-invalidate. After each step, print the state of `C0`, `C1`, and `mem`.

Also write a layout check: define `struct bad { int a; int b; }` and a padded `struct good { int a; char pad[60]; int b; }`. Print whether `&a` and `&b` land on the same 64-byte line.

**You must implement:**
```c
typedef enum { I = 0, S, E, M } mesi_t;
typedef struct { mesi_t st; int v; } cline_t;
void rd(int who);
void wr(int who, int v);
```

**Driver / tests to write:**
- Sequence: `rd(0)` → C0 in E. `rd(1)` → both S. `wr(0, 42)` → C0 M, C1 I. `rd(1)` → HITM, both S. `wr(1, 99)` → C1 M, C0 I.
- Layout check: print `&a`, `&b`, and `same_line` for both `bad` and `good`.

**Be ready to explain in interview:**
- MESI vs MOESI vs MESIF in one sentence each.
- Why does false sharing cost ~50-80 cycles per write?
- HITM counter and `perf c2c` — what exactly do they observe?
- Why does padding alone fail if the struct itself isn't 64-B aligned?

## E2 — DS&A: Linked-List Cycle (LC 141, Floyd)

**Problem statement:** Detect a cycle in a singly-linked list using the tortoise-and-hare two-pointer technique.
**Signature:** `int has_cycle(struct node *head);` returning 1 if cycle exists, 0 otherwise.
**Constraints / edge cases:** empty list, single node, self-loop, cycle starting at head, cycle starting mid-list.
**Talking points:** O(n) time, O(1) space; analogue to snoop-race detection (two pointers chasing at different speeds).

## E3 — Architect Drills (no code, write your answers)

**D1.** Why MESI specifically — why not just M and I? Construct workload where M+I alone catastrophically loses.

**D2.** Why did AMD add O (MOESI) and Intel add F (MESIF)? Construct the workload that forces each.

**D3.** Snoop vs directory coherence. Derive the core count where each crosses over.

**D4.** Cache-line size sets the false-sharing penalty. If line size dropped to 16B, what bug classes disappear and which appear?

**D5.** Coherence is "free" up to snoop fabric saturating. At 256 cores, what changes?

**D6.** Padded per-thread counters fix false sharing but waste DRAM. Construct a counter that's both compact and contention-free.

**D7.** CXL.cache lets accelerators be peers in the coherence domain. Which Day-5 bug classes follow the accelerator?

## E4 — Bug Anchor

**Symptoms:** Multi-thread per-CPU counter array is 5-10× slower than single-thread. `perf c2c` shows HITM events on the counter array address. Walk the debug tree: how to confirm false sharing, what fix to apply, why `__attribute__((aligned(64)))` on each counter is necessary but not sufficient (struct itself must also be 64-B aligned).

Cross-link to Day 8: stale descriptor read by device is the same MESI picture with the device as the second agent.

## Reference solution

Once you've attempted, see `../day05_coherence_mesi_false_sharing.md` sections "Coding A" and "Coding B" for full solutions and discussion.

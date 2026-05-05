# Day 17 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day17_concurrency_queues_dsa.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: spinlock + SPSC ring (full)

**Problem statement:** Build two lock-free / lock-light primitives end-to-end.

(a) **Spinlock** — test-and-test-and-set. `spin_lock` busy-loads with `memory_order_relaxed`, then tries `compare_exchange_weak` with `memory_order_acquire`. `spin_unlock` does `memory_order_release`. Drive with two pthreads each bumping a shared `counter` 100 000 times; assert final value `== 2 * 100000`.

(b) **SPSC ring** — `R = 1024` slots. `head` and `tail` MUST sit on separate 64-byte cache lines (`alignas(64)`). Push: load tail relaxed, head acquire, write slot, store tail release. Pop: mirror. Drive: push 5 squares, pop them all, print.

**You must implement:**
```c
typedef _Atomic int spinlock_t;
void spin_lock(spinlock_t *l);
void spin_unlock(spinlock_t *l);

typedef struct {
    int buf[R];
    alignas(64) _Atomic uint32_t head;
    alignas(64) _Atomic uint32_t tail;
} spsc_t;
bool spsc_push(spsc_t *r, int v);
bool spsc_pop (spsc_t *r, int *v);
```

**Driver / tests to write:**
- Two-thread counter test → expect `200000`.
- SPSC push 5 squares (`0,1,4,9,16`), drain, print.

**Be ready to explain in interview:**
- 3 atomics guarantees: atomicity, visibility, ordering. `volatile` gives only compiler ordering.
- Pick weakest correct order; relaxed for counters, acq/rel for locks and producer-consumer flags, seq_cst rarely.
- Why SPSC needs no CAS; what changes at MPMC.
- ABA problem; defenses (tagged pointer, hazard pointer, RCU).
- Cache-line padding between `head`/`tail` is the difference between toy and shipped code.

## E2 — DS&A: Evaluate Reverse Polish Notation (LC 150)

**Problem statement:** Evaluate a postfix expression: tokens are integers or `+ - * /` operators. Use a stack.
**Signature:** `int eval_rpn(char **tokens, int n);`
**Constraints / edge cases:** single number, division truncates toward zero (per LC), no validation needed (assume well-formed).
**Talking points:** O(n) stack walk; same shape as walking nested capability chains in PCIe config space (Day 10) or matching `BEGIN`/`END` markers in a debug log.

## E3 — Architect Drills (no code, write your answers)

**D1.** "Pick the weakest correct memory order." What's wrong with always using seq_cst?
**D2.** SPSC is lock-free; MPSC needs CAS. Why does one writer change the algorithm class?
**D3.** Why doesn't HW provide a wider native CAS that bakes in a counter?
**D4.** Construct (if you can) a UP workload where spinlocks beat mutexes.
**D5.** Construct an SPSC ring contention-free without padding.
**D6.** Why do beginners think `volatile` is a synchronisation primitive? What language change would make it sufficient?
**D7.** Construct a workload requiring wait-free, not just lock-free.

## E4 — Bug Anchor

**Symptoms:** Two threads bump a shared counter without atomics; final value < expected. Walk the fix: `_Atomic` + `atomic_fetch_add(memory_order_relaxed)`.

Other anchors: CAS spin without test-first → cache-line ping-pong; SPSC `head`/`tail` on same cache line → false sharing; ABA on lock-free pop; `volatile` mistaken for atomic; spinlock held across a sleeping call → deadlock; lock-acquisition order inversion → deadlock.

## Reference solution

Once you've attempted, see `../day17_concurrency_queues_dsa.md` sections "Coding A" and "Coding B" for full solutions and discussion.

# Day 00 — Exercises (Question First)

> **Read the prompt. Write the code from scratch in `exercises/<name>.c`.**
> Only after you compile, run, and explain *why* it works, peek at `solutions/<name>.c`.
> If your solution differs but passes the asserts, that's fine — diff and learn.

Build your attempt:

```sh
cc -Wall -Wextra -Wpedantic -std=c11 -g -O0 -I. exercises/<file>.c -o /tmp/<file> && /tmp/<file>
```

Build the reference solution to compare output:

```sh
make -C solutions <target>     # if you wire a Makefile inside solutions/
```

---

## E1 — `bit_macros` (file: `exercises/test_macros.c`)

Implement these macros in `macros.h` (you may already have stubs):

```c
BIT(n)
MASK(msb, lsb)
FIELD_GET(reg, msb, lsb)
FIELD_PREP(val, msb, lsb)
FIELD_WRITE(reg, val, msb, lsb)   // RMW
ALIGN_UP(x, a)  ALIGN_DOWN(x, a)  IS_ALIGNED(x, a)
POW2(n)                           // true if n is a power of 2
ARRAY_SIZE(a)
```

**Driver to write in `exercises/test_macros.c`:**

1. Take `r = 0xDEADBEEF`.
2. Print `MASK(15,8)`, `FIELD_GET(r,15,8)` (must print `0xBE`).
3. Use `FIELD_WRITE(r, 0xAA, 15, 8)` and assert result is `0xDEADAAEF`.
4. Show that `BIT(31)` is `0x80000000` *without* triggering UB (explain why `1 << 31` is wrong).
5. Assert `IS_ALIGNED(0x1000, 0x1000)` and `!IS_ALIGNED(0x1004, 8)` is false.

**Gotchas to be able to explain in interview:**
- Why `UINT32_C(1)` and not `1U`?
- Why does `MASK(31, 0)` need the `UINT32_MAX >> ...` form rather than `((1U << 32) - 1)`?
- Why is `FIELD_PREP` masked *after* shifting?

---

## E2 — `mmio_accessors` (file: `exercises/mmio.c`)

Write `volatile`-correct accessors against a software-simulated register block (a `uint32_t` array on the heap stands in for a BAR).

```c
uint32_t reg_read32(volatile uint32_t *reg);
void     reg_write32(volatile uint32_t *reg, uint32_t v);
void     reg_field_write(volatile uint32_t *reg, uint8_t msb, uint8_t lsb, uint32_t v);
```

**Driver:**
1. `mmap`-style allocate 4 KiB; treat as 1024 `uint32_t` registers.
2. Use `reg_field_write(&block[2], 7, 4, 0xC)` and read it back.
3. Print before/after binary; assert neighbour bits are untouched.

**Be ready to answer:**
- What does `volatile` actually prevent the compiler from doing? (3 things.)
- `volatile` is not a memory barrier — what does that mean in practice?
- Why must MMIO writes be sized exactly (no `memcpy` into a register)?

---

## E3 — `wait_for_bit` (file: `exercises/poll.c`)

Implement bounded polling:

```c
int wait_for_bit_set(volatile uint32_t *reg, uint8_t bit,
                     uint32_t timeout_us);
/* return 0 on bit observed set; -ETIMEDOUT on timeout */
```

**Driver:**
1. Spawn a "producer" function called from inside the poll loop that flips the bit after N iterations.
2. Demonstrate the *timeout* path by *not* flipping the bit, with `timeout_us = 1000`.
3. Print number of iterations consumed.

**Interview prompts — write your answers as comments:**
- Why never `while (!(*reg & BIT(x)));` without a timeout?
- Why is `udelay()` between polls usually wrong on MMIO? (Hint: read latency, posted write fence, CSTS.RDY in NVMe Day 11.)
- How would you change this for a sleep-capable context vs. ISR?

---

## E4 — `my_memcpy` (file: `exercises/my_memcpy.c`)

Reimplement `memcpy(void *dst, const void *src, size_t n)` from scratch.

**Constraints:**
- Byte-by-byte first version must compile clean with `-Wall -Wextra -Wpedantic`.
- Unit-test with: empty copy (`n=0`), 1 byte, overlapping (which **must be UB** — show that you know it).
- Print address of `src`, `dst`, and `dst[i]` after copy.

**Stretch:** word-aligned fast path that falls back to bytes for the head/tail. Why does that win on cache lines? (Day 4 prep.)

---

## E5 — `my_memmove` (file: `exercises/my_memmove.c`)

Same prototype as `memcpy` but **must handle overlap**.

**Driver test cases:**
1. `dst < src` overlap (forward copy is safe).
2. `dst > src` overlap (must copy backwards).
3. `dst == src` (no-op).

**Be ready to answer:**
- How do you detect direction?
- Why is the *exact* check `dst < src || dst >= src + n` for safe forward copy?

---

## E6 — `reverse_list` (file: `exercises/reverse_list.c`)

Singly-linked list. Implement:

```c
struct node { int v; struct node *next; };
struct node *reverse(struct node *head);
```

**Driver:** build `1 → 2 → 3 → 4 → 5`, reverse, print.
Then implement **iterative** and **recursive** versions; explain stack-depth cost of the recursive one.

---

## E7 — `cycle_detect` (file: `exercises/cycle_detect.c`)

Floyd's tortoise-and-hare. Return `1` if cycle present, else `0`.
Bonus: return the *node where the cycle starts*.

**Driver:**
- Acyclic list: should return 0.
- Cyclic list (tail->next = node 3): should return 1, and bonus should print value 3.

**Talking points:**
- Why O(1) memory beats a `seen[]` set in firmware contexts.
- Where this pattern shows up in IRQ-storm detection / log replay.

---

## E8 — `struct_sizes` (file: `exercises/struct_sizes.c`)

Print `sizeof` for these structs and explain the padding:

```c
struct A { char a; int b; char c; };
struct B { int b; char a; char c; };
struct C { char a; char c; int b; };
struct D { char a; double b; char c; };
#pragma pack(push,1)
struct E { char a; int b; char c; };
#pragma pack(pop)
```

Compute the expected size **before** running. For each one explain:
- Where is the padding?
- What is the natural alignment of each field on x86-64? On ARMv8?
- Why does `pragma pack(1)` matter for MMIO/PCIe header overlays — and why is it dangerous on ARM (alignment trap)?

---

## E9 — `csr_model` (file: `exercises/csr_model.c`)

Build a tiny "fake silicon" model:
- A struct with `CTRL`, `STATUS`, `DATA` registers.
- A function `step(model)` that, on each call, advances internal state (e.g., bit 0 of STATUS = ready after 3 steps).
- Top-level driver writes CTRL, polls STATUS via your `wait_for_bit_set`, reads DATA.

This is the seed of a **pre-silicon C model** you would write before RTL exists. (Connects directly to Day 21 / pre-Si planning.)

---

## E10 — `csr_init` (file: `exercises/csr_init.c`)

Write a register-init table driver:

```c
struct csr_init { uintptr_t off; uint32_t val; uint32_t mask; };

void apply_csr_init(volatile uint8_t *base,
                    const struct csr_init *tbl, size_t n);
```

- If `mask == 0xFFFFFFFF` → full write.
- Otherwise → RMW with that mask.
- Print every applied step.

**Why this matters:** every SoC bring-up has *thousands* of these init writes; you'll see exactly this pattern in production firmware.

---

## E11 — `bench` (file: `exercises/bench.c`)

Microbenchmark scaffold:

```c
uint64_t now_ns(void);             // CLOCK_MONOTONIC
double   bench(void (*fn)(void), int iters);
```

Use it to time `my_memcpy` vs glibc `memcpy` on 4 KiB, 64 KiB, 4 MiB.
Plot (or just print) the cliffs. Predict where L1/L2/LLC boundaries are on your laptop.
This is the harness you'll reuse on Day 4 (cache lines / locality / AMAT).

---

## How to use this folder going forward

```
day00/
├── EXERCISES.md       ← read this first, every day
├── macros.h           ← shared header you fill in
├── exercises/         ← YOU WRITE CODE HERE
│   └── <file>.c       ← create as you go
└── solutions/         ← reference; only peek after attempting
    └── <file>.c
```

Same convention will be applied to later days as we extend (Day 03 MMIO, Day 08 DMA, Day 11 NVMe bring-up sim, Day 17 lock-free queue).

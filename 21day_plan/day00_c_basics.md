# Day 00 — Embedded-C Basics Ramp

## Concept

Before Day 1's MMIO/register work, spend one day getting comfortable with ordinary embedded C: compile/run discipline, scalar types, arrays, strings, pointers, structs, linked lists, byte copy, overlap handling, and alignment. This day is deliberately basic. It removes syntax friction so the 21-day plan can focus on silicon concepts.

## Why this day in the flow

Day 1 assumes you can type C without spending the whole session remembering pointer syntax. If C itself is shaky, MMIO macros, polling loops, descriptor rings, and lock-free queues become noisy. Day 0 builds the smallest C muscle set required by the rest of the plan.

This is still learn-by-doing:

- type the code from scratch,
- compile with warnings on,
- run it,
- fix every warning you understand,
- explain the pointer movement out loud.

Do this once before starting Day 1. Keep the daily career skim in [INTERVIEW_REVISION_GUIDE.md → Appendix A](../INTERVIEW_REVISION_GUIDE.md#appendix-a--career-narrative-long-form-daily-skim) separate; that file is for interview narrative, not C practice.

---

## Deep Study Notes

### 1. The C loop for this plan

Use the same loop every day:

```sh
cd 21day_plan/day00
make clean
make
make reverse
make cycle
make memcpy
make memmove
make sizes
```

For later days, the exact file changes, but the loop stays the same: compile, run, inspect, explain.

### 2. The minimum C basics to have ready

You should be able to write these cold:

```c
int add(int a, int b);
size_t my_strlen(const char *s);
void *my_memcpy(void *dst, const void *src, size_t n);
void *my_memmove(void *dst, const void *src, size_t n);
struct node *reverse_list(struct node *head);
int has_cycle(struct node *head);
```

The point is not library trivia. These functions teach the exact movements used later in descriptor rings, page tables, MMIO arrays, and queue pointers.

### 3. Pointer rules that must be automatic

- `p + 1` advances by `sizeof(*p)`, not by 1 byte.
- `char *` / `uint8_t *` are byte pointers.
- `const void *src` means "I promise not to write through `src`."
- `void *dst` means "generic destination"; cast to `unsigned char *` for byte copy.
- `sizeof(array) / sizeof(array[0])` only works while the object is still an array, not after it decays to a pointer.

### 4. Structs and alignment

Struct layout is not just a C topic. It is a hardware topic because buses, DMA engines, and cache lines prefer naturally aligned access. Today you only need the instinct:

```c
struct sample {
    char c;
    int  x;
};
```

This struct is usually 8 bytes, not 5, because the compiler inserts padding before `x` and possibly at the end. Day 2 goes deep on why.

### 5. `memcpy` vs `memmove`

`memcpy` assumes no overlap. `memmove` handles overlap by choosing copy direction:

- if `dst < src`, copy forward;
- if `dst > src`, copy backward;
- if equal, do nothing.

This matters in embedded code because buffers, rings, and packet headers often live in the same memory region.

---

## Bugs in this Topic

These are basic C bugs, but they become silicon-validation bugs when they sit inside register pokes, descriptor setup, or stress harnesses:

- **Array-decay bug:** using `sizeof(arr)` inside a function and silently getting pointer size. Later this becomes a descriptor-count or table-walk bug.
- **Overlap-copy bug:** using `memcpy` when source and destination overlap. Later this becomes packet/header corruption or trace-buffer corruption.
- **Lost-list-tail bug:** reversing or splicing a list without preserving `next`. Later this becomes leaked work items in a debug queue.
- **Struct-padding assumption:** assuming C layout equals wire/register layout. Day 2 and Day 3 turn this into the MMIO-struct-overlay bug class.

## Interview Explanation (60 sec)

> "Before I touch MMIO or descriptor rings, I want plain C to be automatic. My Day 0 ramp is compile-run-debug C: arrays and array decay, pointer arithmetic, strings, structs, linked lists, `memcpy` and `memmove`. The key interview point is that C is explicit: an array passed to a function becomes a pointer, so length must travel separately; `memcpy` is fast because it assumes no overlap, while `memmove` pays the direction check to be safe; and struct size is ABI/layout, not just field-size sum. Those basics directly feed silicon work because register tables, DMA descriptors, and queues are all typed memory plus disciplined pointer movement."

---

## Architect Drills

*Minimal hints only. Derive the answer before reading any source day again. No answer key.*

**D0.1. Why does C let arrays decay to pointers instead of carrying length metadata?**
*Constraints to weigh:* zero-cost abstraction, ABI simplicity, old hardware, function-call cost, what the compiler can know at compile time.

**D0.2. Why is `memmove` a separate API instead of making `memcpy` overlap-safe by default?**
*Constraints to weigh:* hot-path performance, branch cost, vectorisation, library contract, programmer intent.

**D0.3. Why does `sizeof(arr)` fail inside `build_list(int *arr, int n)`?**
*Constraints to weigh:* function parameter ABI, pointer width, compile-time vs runtime knowledge, who owns length.

**D0.4. Why do embedded codebases prefer explicit-width types like `uint32_t`?**
*Constraints to weigh:* register width, protocol fields, compiler/ABI portability, trace readability, overflow expectations.

**D0.5. Why are linked lists useful in interviews but less common in hot silicon paths?**
*Constraints to weigh:* allocation cost, cache locality, pointer chasing, deterministic latency, ring buffers.

---

## Coding A — Embedded C basics

Use the reference solutions in [`coding/solutions/`](../coding/) (write your own first in [`coding/exercises/`](../coding/exercises/)):

- `d00_reverse_list.c` — build and reverse a linked list.
- `d00_cycle_detect.c` — Floyd slow/fast pointer cycle detection.
- `d00_my_memcpy.c` — byte copy with no-overlap contract.
- `d00_my_memmove.c` — overlap-safe copy.
- `d00_struct_sizes.c` — inspect padding, alignment, and array decay.

For the Day 0 pass, the canonical exercise set is exactly the five files above. The other Day 0 files (`d00_test_macros`, `d00_mmio`, `d00_poll`, `d00_csr_model`, `d00_csr_init`, `d00_bench`) are MMIO / CSR drills used by later days — ignore them until you reach Day 1.

Type each exercise again from memory after one pass. The second pass is where the learning lands.

## Coding B — Interview DS&A warm-up

Do these in C, not Python:

1. Reverse linked list.
2. Linked-list cycle.
3. Remove duplicates from sorted array.
4. Valid parentheses with a fixed-size stack.

Keep the solutions boring and correct. State time and space complexity out loud.

---

## Practice Q&A

**Q1. Why does `sizeof(arr)` work in `main` but not in `build_list(int *arr, int n)`?**
Because in `main`, `arr` is an actual array object. In a function parameter, it has decayed to `int *`, so `sizeof(arr)` is pointer size.

**Q2. Why copy through `unsigned char *` in `memcpy`?**
Because `unsigned char` is the byte-level view C permits for object representation.

**Q3. What is the linked-list reversal invariant?**
`prev` is the already-reversed prefix, `curr` is the node being moved, and `next` preserves the unreversed suffix.

**Q4. What warning should you fix immediately in interview C?**
Missing prototypes, wrong pointer types, signed/unsigned size mismatches when they hide a real bound bug, and missing return paths.

**Q5. When should you use a ring buffer instead of a linked list?**
When allocation, locality, and deterministic latency matter, which is most embedded hot paths.

---

## Bug Anchor

- Day 1 — C address fluency: macros and accessors depend on pointer/type basics.
- Day 2 — memory layout/alignment: struct padding and array decay become ABI and hardware-layout questions.
- Day 3 — MMIO/volatile: plain pointer movement becomes side-effecting register access.
- Day 8 / Day 11 — DMA and NVMe: descriptor rings reuse the same pointer/count/copy discipline.

---

## Exit Check

- [ ] Can compile and run every Day 0 file from [`coding/`](../coding/) with `make sol-d00_<name>`.
- [ ] Can explain array decay and why `sizeof(arr)` changes in a function.
- [ ] Can write `my_strlen`, `my_memcpy`, and `my_memmove` from memory.
- [ ] Can reverse a linked list and detect a cycle without notes.
- [ ] Can predict basic struct padding for `char + int`.
- [ ] Can state why this Day 0 exists before Day 1 MMIO work.

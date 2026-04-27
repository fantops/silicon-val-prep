# Day 02 — Pointers, Memory Layout, Alignment, container_of

> **Source merge:** [30day_plan/day02_pointers_memory.md](../30day_plan/day02_pointers_memory.md), [30day_plan/day00_c_rampup.md](../30day_plan/day00_c_rampup.md) (Block 3). 30-day files are unchanged.

## Concept

Where data lives (stack / heap / BSS / data / text), how the compiler lays out a struct (natural alignment + tail padding), and three ways to obtain an aligned buffer (DMA / SIMD / page) — plus the kernel idiom `container_of` that recovers the parent struct from a member pointer.

## Why this day in the flow

Day 1 gave you "what is an address." Day 2 gives you "what lives at addresses, in what order, with what padding." Every later bug — DMA buffers crossing a cache line (Day 4), MMIO struct overlay misalignment (Day 8), VTCM tile alignment in NPU runtime (Day 18), false sharing (Day 5) — reduces to a layout-or-alignment question. Doing this badly means re-debugging the same class of bug for the rest of the plan.

In the 30-day Layer map this is **Layer 0 → Layer 1 bridge**: from "addresses" to "addresses inside structures and aligned regions."

---

## Deep Study Notes

### 1. The five segments of a running C program

```
High Address  ┌─────────────────────┐
              │       Stack         │  local variables, call frames
              │    (grows ↓)        │
              ├─────────────────────┤
              │         ↕           │
              ├─────────────────────┤
              │       Heap          │  malloc / calloc / free
              │    (grows ↑)        │
              ├─────────────────────┤
              │       BSS           │  uninitialised globals/statics, zeroed at load
              ├─────────────────────┤
              │       Data          │  initialised globals/statics
              ├─────────────────────┤
              │       Text          │  machine code, read-only
Low Address   └─────────────────────┘
```

**Why this matters in validation:**

- **Stack** is fixed and small in bare-metal firmware (often 4-8 KiB per task). Stack overflow is a corruption bug that masquerades as random data.
- **Heap** is usually forbidden in bare-metal: fragmentation, non-deterministic allocation latency, no MMU recovery, no graceful failure path inside an ISR. Most silicon-firmware codebases statically allocate everything.
- **Text** is read-only; writing to it crashes on systems with an MMU and silently corrupts on those without.
- **BSS vs Data**: BSS is zeroed at startup and costs no flash space; Data carries initial values and pays flash for them. Vendor BSPs often keep large lookup tables in `__attribute__((section(".rodata")))` to put them in flash, not in Data.

### 2. Pointers — the rules

A pointer is a variable that holds an **address**. The pointee type only changes:

- how the compiler interprets the bytes when you dereference,
- how `p + 1` is scaled (`p + 1` advances by `sizeof(*p)` bytes).

```c
uint32_t *base = (uint32_t *)0xFE000000;
base[0] /* 0xFE000000 */    base[1] /* 0xFE000004 */    base[2] /* 0xFE000008 */
```

`sizeof(any pointer) == 8` on a 64-bit system, regardless of pointee. Pointer width is set by the address bus, not the data type.

`void *` is "pointer to unknown type" — assignable from any pointer, but you must cast before dereferencing. `NULL` is the zero pointer; dereferencing it segfaults on a host OS but may hit a valid register on bare-metal where address `0` is mapped — yet another reason to validate inputs.

`const` reads right-to-left:

- `const int *p` — pointer to const int (you can change `p`, not `*p`).
- `int *const p` — const pointer to int (you can change `*p`, not `p`).
- `const int *const p` — both.

### 3. Struct padding — predict before compiling

Every field is aligned to its **own natural alignment**. Total struct size pads up to the **largest member's alignment**.

```c
struct A { char a; int b; char c; };  /* 1 + 3pad + 4 + 1 + 3pad = 12 */
struct B { char a; char c; int b; };  /* 1 + 1 + 2pad + 4         =  8 */
struct C { double d; char a; int b; }; /* 8 + 1 + 3pad + 4         = 16 */
```

**Rule:** order fields **largest-alignment first** to minimise padding. `pahole` (Linux) prints the layout if you ever debug a 200-byte struct that "shouldn't be 240 bytes."

**`__attribute__((packed))`** removes padding but can cause unaligned access faults on ARM/MIPS — never use it as a default; use it only when you must match a wire format byte-for-byte (PCIe TLP header, NVMe SQE).

### 4. Alignment — three ways to get an aligned buffer

```c
/* 1. C11 aligned_alloc — size MUST be a multiple of alignment */
void *p1 = aligned_alloc(64, 4096);

/* 2. POSIX (works pre-C11; returns 0 on success) */
void *p2 = NULL;
posix_memalign(&p2, 64, 4000);

/* 3. Manual wrapper — when you only have malloc() */
void *raw = malloc(size + alignment - 1 + sizeof(void *));
uintptr_t aligned = ((uintptr_t)raw + sizeof(void *) + alignment - 1)
                    & ~(uintptr_t)(alignment - 1);
((void **)aligned)[-1] = raw;        /* stash original for free() */
free(((void **)aligned)[-1]);
```

**Where alignment matters in HW work:**

- **DMA buffers** must be aligned to the device's burst size (often a 64-byte cache line, or 4 KiB page for NVMe PRPs).
- **VTCM / SRAM tiles** in NPUs need page or tile alignment (4 KiB, sometimes 1 MiB).
- **SIMD loads** (HVX, AVX, NEON) trap or perform poorly on misaligned addresses.
- **PCIe TLP payload** alignment affects packet split and latency.

The four macros:

```c
#define ALIGN_UP(x, a)      (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a)    ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a)    (((x) & ((a) - 1)) == 0)
#define IS_POW2(n)          ((n) && (((n) & ((n) - 1)) == 0))
```

`a` must be a power of 2; otherwise the bit-tricks lie.

### 5. `container_of` — the kernel idiom

```c
#include <stddef.h>
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
```

Given a pointer to a member of a struct, recover a pointer to the enclosing struct. Used everywhere in Linux drivers (intrusive lists, work-queue closures, character-device callbacks).

The whole insight is the offset math: `member`'s address = `parent` address + `offsetof(member)`, so subtracting `offsetof` from the member's address gives the parent.

### 6. Stack vs heap vs static — the embedded rule of thumb

| Allocation | Lifetime | Cost | When to use in HW work |
|---|---|---|---|
| static global | program | predictable, fixed | descriptor rings, lookup tables, MMIO base mappings |
| stack local | scope | cheap, bounded | scratch in functions, small temporaries |
| heap | until freed | fragmentation, latency | rare in bare-metal; common only in user-space drivers |

Bare-metal firmware in the Intel/Qualcomm world is usually 100% static. User-space drivers (NVMe-of, vfio, ORT NPU runtime) use heap freely but pay attention to alignment.

---

## Bugs in this Topic

- **Struct-overlay MMIO bug (Day 3 / Day 8 link):** declaring a struct over a register block and relying on `__attribute__((packed))` ordering. Compilers may add padding differently than the hardware register map; struct assignment can read adjacent W1C bits as a side effect; field write order is not guaranteed. **Fix:** explicit `reg_read32`/`reg_write32` per offset.
- **DMA buffer crossing a cache line (Day 4 link):** a 60-byte buffer at a 16-byte aligned address crosses two 64-byte cache lines, causing partial-line writebacks and stale-data races against the device. **Fix:** `aligned_alloc(64, ALIGN_UP(size, 64))`.
- **NPU buffer not page-aligned (Day 19 link):** the QNN/ORT page-fault story has a flavour of this — context buffers touched in non-page-aligned strides spike minor page faults at the start of each layer. **Fix:** allocate context buffers at page granularity and respect tile alignment.
- **Stack overflow in ISR:** a deep call chain inside an interrupt handler corrupts the next task's locals. Symptom is "random" wrong values that move with build changes. **Fix:** keep ISRs flat, size stacks per-task, use a stack-canary build.

---

## Interview Explanation (60-90 sec)

> "Pointers are addresses with a type — the type only changes how the bytes are interpreted and how pointer arithmetic scales. A C program lives in five segments: stack, heap, BSS, data, text; in bare-metal firmware we usually static-allocate everything because heap fragmentation and ISR-context allocation are unsafe. Struct layout follows natural alignment plus tail padding, so you order fields largest-first to minimise padding — `__attribute__((packed))` is a footgun on ARM. For DMA, SIMD, or NPU tile buffers I use `aligned_alloc`, `posix_memalign`, or a manual wrapper that stashes the original pointer one slot before the aligned address. The kernel idiom `container_of` recovers a parent struct from a member pointer using `offsetof` — it's the foundation of Linux driver callbacks. Most layout bugs in silicon firmware come from struct-overlay over MMIO or from a DMA buffer that crosses a cache line; the fixes are explicit register accessors and 64-byte-aligned allocators."

---

## Practice Q&A

**Q1. Why is `sizeof(int *) == 8` regardless of pointee on a 64-bit machine?**
Pointer width is set by the address bus, not the pointee type.

**Q2. Predict `sizeof` for `struct { char a; int b; char c; }` and the reordered version `{ int b; char a; char c; }`.**
12 vs 8. Reorder largest-first to save 4 bytes.

**Q3. Why do bare-metal firmwares ban `malloc`?**
Fragmentation, non-deterministic latency, ISR-unsafe, no MMU recovery.

**Q4. Three ways to allocate a 64-byte aligned 4 KiB buffer?**
`aligned_alloc(64, 4096)`, `posix_memalign(&p, 64, 4096)`, manual wrapper that over-allocates and rounds up.

**Q5. What does `container_of` do?**
Subtracts `offsetof(type, member)` from the member pointer to get the parent struct pointer.

**Q6. Why is `__attribute__((packed))` dangerous on ARM?**
It enables unaligned member accesses; some ARM cores trap, others incur a slow trap-and-emulate path.

**Q7. Why is struct overlay over MMIO a bad idea?**
Padding is compiler-dependent, struct assignment may touch W1C status bits, and field write ordering isn't guaranteed.

**Q8. `const int *const *p` — what is `p`?**
A pointer to a const pointer to a const int. (Right-to-left.)

---

## Coding A — Systems C: aligned allocator + container_of

```c
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#define ALIGN_UP(x, a)    (((x) + (a) - 1) & ~((a) - 1))
#define IS_ALIGNED(x, a)  (((uintptr_t)(x) & ((a) - 1)) == 0)
#define IS_POW2(n)        ((n) && (((n) & ((n) - 1)) == 0))

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* Manual aligned allocator: stash original pointer one slot before. */
void *aligned_malloc(size_t size, size_t align) {
    if (!IS_POW2(align)) return NULL;
    void *raw = malloc(size + align - 1 + sizeof(void *));
    if (!raw) return NULL;
    uintptr_t a = ((uintptr_t)raw + sizeof(void *) + align - 1) & ~(uintptr_t)(align - 1);
    ((void **)a)[-1] = raw;
    return (void *)a;
}
void aligned_free(void *p) {
    if (p) free(((void **)p)[-1]);
}

/* Intrusive list demonstration of container_of. */
struct list_head { struct list_head *next; };
struct work {
    int id;
    struct list_head node;
};

int main(void) {
    /* Aligned alloc */
    void *p = aligned_malloc(4096, 64);
    printf("aligned %p (mod 64 = %lu)\n", p, (unsigned long)((uintptr_t)p % 64));
    aligned_free(p);

    /* container_of */
    struct work w = { .id = 42 };
    struct list_head *n = &w.node;
    struct work *back = container_of(n, struct work, node);
    printf("recovered id = %d\n", back->id);

    /* Struct sizes */
    struct A { char a; int b; char c; };
    struct B { int b; char a; char c; };
    printf("sizeof A=%zu B=%zu\n", sizeof(struct A), sizeof(struct B));
    return 0;
}
```

## Coding B — Interview DS&A: Reverse Linked List (LC 206)

3-pointer iterative; the same shape recurs in interview pointers.

```python
def reverseList(head):
    prev, curr = None, head
    while curr:
        nxt = curr.next
        curr.next = prev
        prev = curr
        curr = nxt
    return prev
```

---

## Bug Anchor

- **Day 4 link:** DMA buffer crossing a cache line — alignment is the fix.
- **Day 8 link:** struct-overlay MMIO — explicit accessors are the fix.
- **Day 19 link:** QNN/ORT page-fault story — page-aligned context buffers are part of the mitigation.

## Exit Check

- [ ] Drew the 5-segment memory map from memory.
- [ ] Predicted struct A/B/C `sizeof` correctly.
- [ ] Wrote three ways to get aligned memory.
- [ ] Wrote `container_of` using `offsetof`.
- [ ] Stated why `__attribute__((packed))` is unsafe on ARM.
- [ ] Stated why struct overlay over MMIO is a bug class, not a style preference.

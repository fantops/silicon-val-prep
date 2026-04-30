# Day 02 — Pointers, Memory Layout, Alignment, container_of

## Concept

Where data lives (stack / heap / BSS / data / text), how the compiler lays out a struct (natural alignment + tail padding), and three ways to obtain an aligned buffer (DMA / SIMD / page) — plus the kernel idiom `container_of` that recovers the parent struct from a member pointer.

## Why this day in the flow

Day 1 gave you "what is an address." Day 2 gives you "what lives at addresses, in what order, with what padding." Every later bug — DMA buffers crossing a cache line (Day 4), MMIO struct overlay misalignment (Day 8), VTCM tile alignment in NPU runtime (Day 18), false sharing (Day 5) — reduces to a layout-or-alignment question. Doing this badly means re-debugging the same class of bug for the rest of the plan.

In the Layer map this is **Layer 0 → Layer 1 bridge**: from "addresses" to "addresses inside structures and aligned regions."

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

### 7. Function pointers (and arrays of them)

C's substitute for virtual dispatch. Lets one piece of code call many implementations chosen at runtime — the foundation of driver `ops` tables, ISR vectors, state machines, and callbacks.

**Declare / assign / call:**

```c
int add(int a, int b) { return a + b; }

int (*fp)(int, int);     /* fp: pointer to fn taking (int,int) returning int */
fp = &add;                /* `&` optional — function name decays to address */
fp = add;                 /* equivalent */

int r = fp(2, 3);         /* implicit call */
int s = (*fp)(2, 3);      /* explicit deref — same effect */
```

**Read inside-out:** `fp` is the identifier; `(*fp)` says it's a pointer; `(int,int)` says the pointee takes two ints; the leading `int` is the return type.

**Use a typedef** — keeps signatures readable, especially in headers:

```c
typedef int (*binop_fn_t)(int, int);
binop_fn_t fp = add;
```

**Array of function pointers — opcode dispatch table:**

```c
typedef void (*op_handler_t)(cpu_t *cpu);

void op_nop (cpu_t *c) { c->pc++; }
void op_add (cpu_t *c) { c->reg[0] += c->reg[1]; c->pc++; }
void op_halt(cpu_t *c) { c->halted = 1; }

static op_handler_t handlers[256] = {
    [0x00] = op_nop,
    [0x01] = op_add,
    [0xFF] = op_halt,
    /* unfilled slots default to NULL — caller must check */
};

void step(cpu_t *cpu) {
    uint8_t opc = cpu->mem[cpu->pc];
    op_handler_t h = handlers[opc];
    if (h) h(cpu);
    else   trap_illegal_opcode(cpu);
}
```

This is **literally how interpreters, NVMe admin command tables, ACPI method dispatch, and packet handlers work.**

**Array of fn-ptrs — state machine:**

```c
typedef enum { S_IDLE, S_RUN, S_DONE, S_NUM } state_t;
typedef state_t (*state_fn_t)(event_t e);

state_t st_idle(event_t e) { return (e == EV_START) ? S_RUN  : S_IDLE; }
state_t st_run (event_t e) { return (e == EV_STOP)  ? S_DONE : S_RUN;  }
state_t st_done(event_t e) { (void)e; return S_DONE; }

state_fn_t fsm[S_NUM] = { st_idle, st_run, st_done };

state_t cur = S_IDLE;
cur = fsm[cur](next_event());
```

Replaces large `switch` blocks; each state is self-contained.

**Real places you've already seen them:**

| Use case | Where |
|---|---|
| Driver ops table | Linux `struct file_operations { ssize_t (*read)(...); int (*open)(...); ... }` |
| ISR vector table | array indexed by IRQ#, each entry an ISR address |
| Callbacks | `qsort(arr, n, sz, cmp_fn)`, libuv async I/O, GUI event handlers |
| Plugin / dynamic load | `dlsym()` returns `void*` cast to fn-ptr |
| Test harness | array of test functions run in order |

**Footguns:**

- `void (*fp)()` is **not** `void (*fp)(void)` in C — the first means "unspecified args", the second means "no args". Always use `(void)` for no-arg fn-pointers.
- Casting between fn-ptrs of different signatures and calling = UB.
- `NULL` check before calling — sparse dispatch tables (like the opcode array above) leave entries zero.

---

## Bugs in this Topic

- **Struct-overlay MMIO bug (Day 3 / Day 8 link):** declaring a struct over a register block and relying on `__attribute__((packed))` ordering. Compilers may add padding differently than the hardware register map; struct assignment can read adjacent W1C bits as a side effect; field write order is not guaranteed. **Fix:** explicit `reg_read32`/`reg_write32` per offset.
- **DMA buffer crossing a cache line (Day 4 link):** a 60-byte buffer at a 16-byte aligned address crosses two 64-byte cache lines, causing partial-line writebacks and stale-data races against the device. **Fix:** `aligned_alloc(64, ALIGN_UP(size, 64))`.
- **NPU buffer not page-aligned (Day 19 link):** the QNN/ORT page-fault story has a flavour of this — context buffers touched in non-page-aligned strides spike minor page faults at the start of each layer. **Fix:** allocate context buffers at page granularity and respect tile alignment.
- **Stack overflow in ISR:** a deep call chain inside an interrupt handler corrupts the next task's locals. Symptom is "random" wrong values that move with build changes. **Fix:** keep ISRs flat, size stacks per-task, use a stack-canary build.

---

## Interview Explanation (60-90 sec)

> "Pointers are addresses with a type — the type only changes how the bytes are interpreted and how pointer arithmetic scales. A C program lives in five segments: stack, heap, BSS, data, text; in bare-metal firmware we usually static-allocate everything because heap fragmentation and ISR-context allocation are unsafe. Struct layout follows natural alignment plus tail padding, so you order fields largest-first to minimise padding — `__attribute__((packed))` is a footgun on ARM. For DMA, SIMD, or NPU tile buffers I use `aligned_alloc`, `posix_memalign`, or a manual wrapper that stashes the original pointer one slot before the aligned address. The kernel idiom `container_of` recovers a parent struct from a member pointer using `offsetof` — it's the foundation of Linux driver callbacks. Most layout bugs in silicon firmware come from struct-overlay over MMIO or from a DMA buffer that crosses a cache line; the canonical tell is a descriptor allocated at offset 56 in a 64-byte line so the device's coherent fetch straddles two lines and the second half arrives stale — fix is a 64-byte-aligned allocator, and for MMIO, explicit `reg_read32`/`reg_write32` instead of struct overlay so the compiler can't pad or reorder the register map."

---

## Architect Drills

*Reason from constraints. The right answer is whichever the hardware bus, compiler ABI, and workload force. No answer key.*

**D1. Why is alignment a hardware requirement at all — why don't CPUs just rotate-and-mask any byte boundary?**
*Constraints to weigh:* memory bus width, cache-line crossing penalty, atomic-access guarantees (lock-free needs naturally aligned), MMU page boundaries, vendor cost of a barrel-shifter in the load/store unit. ARMv7 trapped on unaligned, ARMv8 mostly allows it — why the change, and why does Apple Silicon still penalise it on hot paths?

**D2. Why does a 60-byte DMA buffer at a 16-byte-aligned address corrupt silently while a 64-byte buffer at the same address is fine?**
*Constraints to weigh:* cache-line straddling, partial-line writeback semantics, who owns each line in MESI when the device fetches, the moment of "visible to device." Walk the timing diagram. What does a 4 KB DMA buffer at offset 60 in a page do that's *worse* than the 60-byte case?

**D3. Struct field reordering can save 33% of memory. Why doesn't the C compiler reorder fields automatically?**
*Constraints to weigh:* ABI stability across translation units, ordered initialisers `{a,b,c}`, struct-overlay onto MMIO/protocol headers (where order is a contract), `offsetof` predictability for `container_of`. Rust *does* reorder by default — what gives Rust permission that C doesn't have?

**D4. `container_of` is the foundation of Linux callbacks. What would force a future kernel to drop it?**
*Constraints to weigh:* C lacks generics, fat pointers cost a word, the alternative (parallel hash map from member→parent) costs a lookup per callback, intrusive lists vs allocated wrappers, debuggability under crash dumps. If C added zero-cost interfaces, where would `container_of` survive anyway?

**D5. Bare-metal firmware bans `malloc`. Construct the workload where you'd genuinely need it back, and reason about what you'd accept losing.**
*Constraints to weigh:* fragmentation, ISR context, deterministic latency budgets, slab/pool alternatives, OOM recovery without an MMU. What does Zephyr's `k_heap` actually buy that pure pools don't, and at what cost in worst-case latency?

**D6. `__attribute__((packed))` is a footgun on ARM. What architectural choice would make it safe — and at what cost?**
*Constraints to weigh:* unaligned-access trap-and-emulate cost, per-load alignment check in the LSU, code-size cost (compiler emits byte-by-byte access for packed fields), wire-format pragmatism (network / disk headers want packed). Why is x86 "just fine" with packed and ARM punishes it — what's the silicon trade?

**D7. Five segments (text/data/bss/heap/stack) is the textbook layout. Imagine an MCU with one 64 KB SRAM and no MMU — does the segmentation still earn its keep?**
*Constraints to weigh:* link-time symbol resolution, stack-overflow detection without an MPU, R/W/X permissions, ROM-constant savings (text in flash), zero-init guarantee for BSS. When does a hand-rolled "everything in one .data" win, and what defects does it invite?

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

## System Thinking — Why memory has a *layout*

**Frame:** memory looks flat from the programmer's view (one big array of bytes), but hardware sees regions with very different rules. The layout exists because each region answers a different lifetime + protection + access-pattern question.

### Q: Why three regions (stack, heap, BSS/data) instead of one?

Each solves a different lifetime problem:
- **Stack:** LIFO, automatic, free on return. Cost: must know size at call time. Tradeoff: no fragmentation, no heap calls, but bounded depth → recursion limits, stack-overflow bugs.
- **Heap:** arbitrary lifetime, sized at runtime. Cost: O(log n) allocator work, fragmentation, must `free`. Tradeoff: flexibility for cost.
- **BSS / data:** lifetime = whole program, zero-initialized at load. Cost: image size for `.data`, zero-fill cost for `.bss`. Tradeoff: predictable address (good for symbols) but no per-thread isolation.

A single region cannot serve all three patterns; allocators that try (`alloca`, arena allocators) reinvent one of the three under different names.

### Q: Why does alignment matter? Hardware doesn't care about a few bytes, right?

It does, for three independent reasons:
1. **Atomicity.** A naturally-aligned load/store is a single bus transaction. A misaligned 8-byte load that straddles a cache line is *two* transactions — and the two halves can interleave with another core's store. Atomic ops on misaligned addresses are undefined.
2. **Speed.** Misaligned access either traps to a slow handler (older ARM, MIPS) or splits into two bus cycles (x86). Either way, ~2-10× slower.
3. **DMA.** Devices fetch in cache-line or page chunks. A descriptor that straddles a cache line forces the device to issue two reads, and partial-line writes back from the CPU force the device to invalidate twice. **This is the canonical "works at low rate, fails at line rate" bug.**

### Q: Why does the compiler insert padding?

Because it must respect each member's natural alignment. `struct { char c; int i; }` has 3 bytes of padding so `i` lands on a 4-byte boundary — the alternative is making every load slow or trapping. The fix is to *order members from largest to smallest* (`int` then `char`), which packs naturally. `__attribute__((packed))` removes padding but trades it for misaligned access on every member — useful for wire formats, dangerous for hot paths.

### One-liner

*"Memory has a layout because lifetime, protection, and access pattern are three different questions and one region can't answer all of them. Alignment matters because hardware loads in chunks — misaligned data costs atomicity, speed, and DMA correctness. Compiler padding is the visible cost of natural alignment; packed structs trade space for per-access penalty."*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **segments** — *what:* text/rodata/data/bss/heap/stack / *why:* protection + init policy differ / *how:* linker script + ELF program headers / *connects:* why globals start at 0 (bss zero-fill), why .text is RX
- **alignment** — *what:* `addr % align == 0` / *why:* HW load width + atomicity guarantees / *how:* compiler inserts pad, alignof per type / *connects:* DMA buffer alignment, cache-line alignment, atomic store width
- **struct padding** — *what:* gaps for natural alignment / *why:* fastest field access / *how:* align to largest member, tail-pad to struct align / *connects:* reorder fields large→small to shrink, `__attribute__((packed))` cost
- **endianness** — *what:* byte order in a word / *why:* HW convention varies / *how:* LE = x86/ARM default, BE = network / *connects:* `htonl`, MMIO register byte ordering, file format portability
- **stack vs heap** — *what:* LIFO frame vs explicit alloc / *why:* lifetime model differs / *how:* SP-relative vs malloc / *connects:* stack overflow on recursion, heap fragmentation, escape analysis

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

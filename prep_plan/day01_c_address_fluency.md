# Day 1 — C Address Fluency, Bit/Field Macros, MMIO Primer

## Concept

A pointer is just an address with a type. Hardware registers are just addresses with **side effects** on every load/store. Today you build the macro set (`BIT`, `MASK`, `FIELD_GET`, `FIELD_PREP`, `ARRAY_SIZE`) and the accessor set (`reg_read32`, `reg_write32`, `reg_field_write`, `wait_for_bit_set`) that the next 20 days reuse verbatim.

## Why this day in the flow

Layer 0 of the system flow ("Coding And Address Fluency") starts here because every later layer — caches, virtual memory, DMA, PCIe, NVMe, power management, NPU runtime — is ultimately accessed through a typed address with a side effect. If `reg_field_write` and `wait_for_bit_set` are not muscle memory by the end of today, every later day will spend energy on the wrong problem.

The two real bug stories that hinge on these basics:

- **NVMe `CSTS.RDY` / AQA bug** (Day 23): a polling-with-timeout bug, a register-field-programming bug, and a spec-walking bug at once.
- **PCU/ADR opcode-ordering bug** (Day 28): a register handshake / ack-wait bug expressed as firmware opcodes.

Both bugs reduce to "I read the register wrong, or I wrote the field wrong, or I forgot the timeout." That is why the macros below matter.

---

## Deep Study Notes

### 1. What an address is

A C variable lives at some address; a pointer holds that address. On a 64-bit machine **every** pointer is 8 bytes regardless of pointee type. The pointee type only changes:

- how the compiler interprets the bytes when you dereference (`*p`),
- how `p + 1` is scaled (`p + 1` advances by `sizeof(*p)` bytes).

```c
uint32_t *regs = (uint32_t *)0xFE000000;
regs[0]   /* *(regs + 0) → address 0xFE000000 — 4 bytes */
regs[1]   /* *(regs + 1) → address 0xFE000004 — 4 bytes later */
regs[2]   /* *(regs + 2) → address 0xFE000008 */
```

That scaling is exactly how you walk a PCIe config space, NVMe register block, or any MMIO array. Internalise this picture before any macros; otherwise the macros look like magic.

### 2. The five macros every silicon-firmware codebase has

```c
#include <stdint.h>

#define BIT(n)                  (UINT32_C(1) << (n))
#define MASK(msb, lsb)          ((UINT32_MAX >> (31 - ((msb) - (lsb)))) << (lsb))
#define FIELD_GET(reg, msb, lsb)    (((reg) & MASK(msb, lsb)) >> (lsb))
#define FIELD_PREP(val, msb, lsb)   (((val) << (lsb)) & MASK(msb, lsb))
#define ARRAY_SIZE(a)           (sizeof(a) / sizeof((a)[0]))
```

One sentence each:

- `BIT(n)` — single-bit mask, `UINT32_C(1)` (unsigned 32-bit constant) so bit 31 is well-defined.
- `MASK(msb, lsb)` — width = `msb-lsb+1`; build the low-mask, shift up by `lsb`; the `UINT32_MAX >> ...` shape avoids the `1U << 32` bug for `MASK(31, 0)`.
- `FIELD_GET` — mask the field, shift down to bit 0.
- `FIELD_PREP` — shift the value up, mask to drop overflow bits (defensive).
- `ARRAY_SIZE` — only valid on a real array, not a pointer.

**Why `UINT32_C(1)`, not `1`** — `1` is a signed `int`; `1 << 31` is undefined behaviour because it shifts into the sign bit. `UINT32_C(1)` is an unsigned 32-bit constant and is well-defined modulo $2^{32}$. Interviewers love this trap.

**Why `MASK` shifts left by `lsb` after building the low mask** — that ordering is the whole insight. Build a mask of `width` ones at the bottom, then slide it up to the field's position.

**Self-check:**

```c
uint32_t r = 0xDEADBEEF;
FIELD_GET(r, 15, 8);  /* 0xBE */
(r & ~MASK(15, 8)) | FIELD_PREP(0xAA, 15, 8);  /* 0xDEADAAEF */
```

### 3. MMIO accessors and why `volatile`

MMIO addresses are not regular memory. Every load and store has a hardware side effect (state transition, FIFO pop, interrupt clear). The compiler does not know this, so without `volatile` it may:

- cache the read in a CPU register and never re-read,
- elide a "redundant" store,
- reorder ordinary non-volatile memory around the access.

All three are catastrophic. `volatile` says the access itself is observable and must happen as written; CPU/device ordering still belongs to barriers and memory attributes.

```c
static inline uint32_t reg_read32(volatile uint32_t *reg) { return *reg; }
static inline void     reg_write32(volatile uint32_t *reg, uint32_t v) { *reg = v; }
```

**`volatile` is not a memory barrier.** On weakly ordered ISAs (ARM, RISC-V) you still need `dmb`/`dsb` (or `atomic_thread_fence`) to order MMIO against normal memory, especially the descriptor-then-doorbell pattern (covered Day 20). On x86 the hardware gives you stronger ordering for many common paths, but device mappings and posted writes still need to be reasoned about from the platform rules, not from C `volatile`.

### 4. Read-modify-write a register field

```c
static inline void reg_field_write(volatile uint32_t *reg, int msb, int lsb, uint32_t val) {
    uint32_t r = reg_read32(reg);
    r = (r & ~MASK(msb, lsb)) | FIELD_PREP(val, msb, lsb);
    reg_write32(reg, r);
}
```

Two gotchas to say out loud:

1. RMW is **not atomic across cores or against HW updating the same register**. Vendor specs often require a lock, or guarantee "no HW touches this bit". Read the spec.
2. **W1C (write-1-to-clear) status registers** are corrupted by RMW. Reading then writing the same value clears every set status bit. Always check the field's access type (RW, RO, RW1C, RO/V) before RMW'ing.

### 5. Poll-with-timeout (the most-asked HW interview function)

```c
int wait_for_bit_set(volatile uint32_t *reg, uint32_t mask, int timeout_us) {
    while (timeout_us-- > 0) {
        if ((reg_read32(reg) & mask) == mask) return 0;
        udelay(1);  /* platform-provided 1us delay */
    }
    return -1;     /* timeout */
}
```

This shape recurs everywhere:

- `wait_csts_rdy` in NVMe bring-up (Day 23) — exact same loop on `CSTS.RDY`.
- `wait_link_up` in PCIe LTSSM (Day 22) — same loop on `LinkStatus.DLL_LinkActive`.
- `wait_pll_lock` in clocking — same loop on `PLL_STATUS.LOCK`.
- `IO_QUIESCE_ACK` in the PCU/ADR sequence (Day 28) — same loop in firmware.

**Always handle the timeout case.** The interviewer's red flag is forgetting it. The canonical NVMe `CSTS.RDY` bug (Day 23) was found because the timeout fired and the spec said "wait at most CAP.TO × 500 ms" — without that timeout the host would have hung silently.

**Variants you should be able to write in 60 seconds:**

```c
int wait_for_bit_clear(volatile uint32_t *r, uint32_t m, int us);
int wait_for_value_match(volatile uint32_t *r, uint32_t expected, int us);
```

### 6. Standard C interview one-liners (fluency probes)

```c
/* swap without temp (XOR trick — fails if a == b) */
void swap_xor(int *a, int *b) { *a ^= *b; *b ^= *a; *a ^= *b; }

/* power of two */
int is_pow2(unsigned n) { return n && !(n & (n - 1)); }

/* popcount — Kernighan: each iter clears the lowest set bit */
int popcount(uint32_t n) { int c = 0; while (n) { n &= n - 1; c++; } return c; }

/* log2 floor */
int log2_floor(uint32_t n) { int r = 0; while (n >>= 1) r++; return r; }

/* runtime endianness */
int is_le(void) { uint32_t x = 1; return *(uint8_t *)&x; }

/* byte-swap 32 */
uint32_t swap32(uint32_t x) {
    return ((x & 0xFF000000U) >> 24) | ((x & 0x00FF0000U) >> 8)
         | ((x & 0x0000FF00U) <<  8) | ((x & 0x000000FFU) << 24);
}

size_t my_strlen(const char *s) { const char *p = s; while (*p) p++; return p - s; }
char  *my_strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) ; return r; }
void  *my_memcpy(void *d, const void *s, size_t n) {
    unsigned char *dd = d; const unsigned char *ss = s;
    while (n--) *dd++ = *ss++;
    return d;
}
```

`memcpy` follow-ups asked verbally:

- **Overlap?** Use `memmove`; copy backward when `dst > src`.
- **Faster?** Word-copy in the middle, byte-copy at the unaligned head/tail.

---

## Bugs in this Topic

Today's macros are too foundational to "own" a single bug, but every later bug story passes through them. Two examples to keep in mind:

- **NVMe `CSTS.RDY` AQA bug (Day 23):** the host wrote `AQA` with the wrong field width and `CSTS.RDY` never asserted. Caught by the timeout in `wait_for_bit_set`. Without that timeout the bring-up would have hung forever.
- **PCU/ADR opcode-ordering bug (Day 28):** `WPQ_FLUSH` was issued before `IO_QUIESCE_ACK`. It is the same shape as forgetting to wait for a status bit before issuing the next operation.

If your `reg_field_write` and `wait_for_bit_set` are sloppy, every later bug story is harder to debug because you are fighting your own scaffolding.

---

## Interview Explanation (60-90 sec)

> "MMIO is just memory-mapped registers — every load and store has a hardware side effect, so I always go through `reg_read32`/`reg_write32` accessors with a `volatile uint32_t *` cast. To program a single field I use a `BIT`/`MASK`/`FIELD_GET`/`FIELD_PREP` macro chain and a 5-line `reg_field_write` that does read, mask, OR, write — being careful that read-modify-write is not atomic across cores and corrupts W1C status registers. To wait on a status bit I use a 6-line `wait_for_bit_set(reg, mask, timeout_us)` with an explicit timeout — that exact shape is `wait_csts_rdy` in NVMe, `wait_link_up` in PCIe, and `wait_pll_lock` in clocking. The timeout is what caught my NVMe `CSTS.RDY` bring-up bug; without it the host would have hung silently."

---

## Architect Drills

*Reason from constraints. Don't recall a textbook answer — derive why the design has to look this way given the C language, the bus, and the silicon. No answer key.*

**D1. Why bother with `reg_read32`/`reg_write32` accessors when a `volatile uint32_t *` cast would compile to the same instruction?**
*Constraints to weigh:* what the compiler is allowed to do under `-O3` (load-store hoisting, dead-store elimination, narrowing), where you'd inject a barrier without re-touching every call site, traceability/printf-debug hooks, test-double substitution for unit tests, fault injection. Where would the typed-pointer style fail first when the architecture changes from x86 to ARM?

**D2. Why do silicon designers expose W1C status bits at all instead of plain RW status fields?**
*Constraints to weigh:* the race between hardware setting a bit and software clearing it (lost-event problem on RW), atomic event capture without a software lock, what failure mode RW would create when the IP sets a bit between the SW read and the SW write. Construct the workload where W1S is the right primitive instead.

**D3. Polling vs interrupts — under what residency/wake distribution does polling actually win?**
*Constraints to weigh:* per-poll cost (cycles + cache pollution), interrupt latency + ISR overhead + context switch, wake energy on a sleeping core, jitter, what fraction of polls hit. NVMe completion queues poll on hot paths and interrupt on cold — derive the crossover from first principles.

**D16. `volatile` is necessary but not sufficient for MMIO. Why does the C standard not just define `volatile` to imply a full barrier?**
*Constraints to weigh:* portability (C is multi-arch but barriers are per-arch), per-access cost on TSO vs weak machines, who pays when 99% of `volatile` uses are sig_atomic_t flags not MMIO, who would even own the new semantics. What would a hypothetical `mmio_t` qualifier need to specify?

**D17. RMW on a control register isn't atomic across cores. Why doesn't hardware provide an `atomic_field_write` MMIO opcode?**
*Constraints to weigh:* MMIO bus semantics (single-beat transactions, no per-IP arbiter for SW agents), per-IP mutex hardware area, who arbitrates across CPUs vs DMA agents vs the IP itself, software simplicity vs HW cost. What does the equivalent on coherent CXL.cache space look like, and why is it different?

**D18. `wait_for_bit_set` needs a timeout. Derive the right value for `CSTS.RDY` from first principles — and explain why "1 second" is wrong.**
*Constraints to weigh:* spec'd worst-case ready time (`CAP.TO`), VRM ramp + PLL relock budget, what the host should *do* on timeout (reset, log, escalate), test-vs-production split. What goes wrong system-wide if every driver picks 1 s "to be safe"?

**D19. The same `read/wait/write` accessor pattern shows up in NVMe, PCIe, clocking, and bare-metal probes. What would force a future SoC to abandon it?**
*Constraints to weigh:* uniform mental model for bring-up, debuggability, transactional MMIO with completions (CXL.io), in-band error signalling, virtualised MMIO with paravirt accessors. When the bus is no longer "dumb," what survives and what must change?

---

## Coding A — Systems C: full register-poke harness

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define BIT(n)               (UINT32_C(1) << (n))
#define MASK(msb, lsb)       ((UINT32_MAX >> (31 - ((msb) - (lsb)))) << (lsb))
#define FIELD_GET(r, msb, lsb)  (((r) & MASK(msb, lsb)) >> (lsb))
#define FIELD_PREP(v, msb, lsb) (((v) << (lsb)) & MASK(msb, lsb))

/* Simulated MMIO: just a backing word. Real code casts a base+offset to
 * volatile uint32_t *. */
static volatile uint32_t fake_reg = 0;

static inline uint32_t reg_read32(volatile uint32_t *r)        { return *r; }
static inline void     reg_write32(volatile uint32_t *r, uint32_t v) { *r = v; }

static inline void reg_field_write(volatile uint32_t *r,
                                   int msb, int lsb, uint32_t v) {
    uint32_t cur = reg_read32(r);
    cur = (cur & ~MASK(msb, lsb)) | FIELD_PREP(v, msb, lsb);
    reg_write32(r, cur);
}

/* Stand-in for udelay(); a real platform provides this. */
static void udelay(int us) { (void)us; /* no-op for the harness */ }

static int wait_for_bit_set(volatile uint32_t *r, uint32_t mask, int timeout_us) {
    while (timeout_us-- > 0) {
        if ((reg_read32(r) & mask) == mask) return 0;
        udelay(1);
    }
    return -1;
}

int main(void) {
    /* Program field [15:8] = 0xAA, then sanity check */
    fake_reg = 0xDEADBEEF;
    reg_field_write(&fake_reg, 15, 8, 0xAA);
    printf("after RMW: 0x%08X (expect 0xDEADAAEF)\n", fake_reg);

    /* Show timeout path */
    fake_reg = 0;
    int rc = wait_for_bit_set(&fake_reg, BIT(0), 5);
    printf("timeout case: rc=%d (expect -1)\n", rc);

    /* Show success path */
    fake_reg = BIT(0);
    rc = wait_for_bit_set(&fake_reg, BIT(0), 5);
    printf("success case: rc=%d (expect 0)\n", rc);
    return 0;
}
```

Compile: `gcc -Wall -std=c11 -o reg_poke reg_poke.c && ./reg_poke`.

---

## System Thinking — Why C looks like this

**Frame:** C is the only mainstream language that lets you see the machine. Every other language hides addresses; C makes you say them out loud. That's not a flaw — it's the whole point. Validation, drivers, and firmware *need* the address to be a first-class value, because the bug is usually "the wrong address was loaded."

### Q: Why does C expose addresses at all?

Because hardware is an address space. A peripheral register lives at `0xFE00_2400`. A DMA descriptor lives at a physical address the device's bus master reads. A page table entry lives at PA `(CR3 + index * 8)`. If the language won't let you write "go read this address," you cannot talk to hardware. Every higher-level language gets this capability *by being implemented in C*.

**Tradeoff:** safety vs reach. Java/Rust/Go give you safety by hiding addresses; you can never accidentally cast an int to a pointer. C gives you reach by exposing them; you *can* accidentally cast an int to a pointer. The validator's world lives on the reach side because the bugs live on the reach side.

### Q: Why is pointer arithmetic *type-aware* (`p+1` advances by `sizeof(*p)`)?

Because the compiler knows the element width but the programmer would forget. `int *p; p+1` is `p + 4`, not `p + 1`. This makes array traversal correct by construction (`for (p = arr; p < arr+N; p++)`), but it's *also* why `void *` cannot be dereferenced or incremented — the type system has no width to multiply by. Casting to `uint8_t *` first is the idiom for byte-level walks (e.g., "copy 17 bytes").

### Q: Why does C bother with `const`, `volatile`, `restrict` instead of just types?

These are **promises to the compiler about the world outside the program** — not about the value itself.
- `const`: "I won't write through this pointer" → compiler can hoist loads.
- `volatile`: "the value can change without my writing it" → compiler must not cache it (Day 3, MMIO).
- `restrict`: "no other pointer aliases this region" → compiler can vectorize.

Get these wrong and the compiler optimizes correctly *for the wrong world*. Most "works in debug, breaks in -O2" bugs are missing-volatile or missing-restrict.

### One-liner

*"C exposes addresses because hardware is an address space. Pointer arithmetic is type-aware so array walks are correct by construction. `volatile`, `const`, and `restrict` are not type decorations — they're contracts about the world outside the program, and the compiler optimizes against those contracts. Get them wrong and -O2 breaks code that -O0 ran fine."*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **array→ptr decay** — *what:* arr name = `&arr[0]` in expressions / *why:* C has no value semantics for arrays / *how:* `T[]` decays to `T*` except in `sizeof`, `&`, string literal init / *connects:* `sizeof(arr)` in a function arg = pointer size, not array size
- **sizeof** — *what:* compile-time byte count / *why:* portable layout queries / *how:* not evaluated, type-driven / *connects:* struct padding, `alignof`, VLA exception
- **pointer arithmetic** — *what:* `p+1` advances by `sizeof(*p)` / *why:* typed addressing / *how:* compiler scales index by element size / *connects:* `a[i] == *(a+i)` identity, void* arithmetic UB
- **strings** — *what:* `char*` terminated by `\0` / *why:* no length field / *how:* terminator-driven scan / *connects:* `strlen` is O(n), classic overflow source
- **const / restrict / volatile** — *what:* contracts to the optimizer / *why:* tell compiler what it may assume / *how:* const = caller promise, restrict = no aliasing, volatile = re-read every time / *connects:* `-O2` breaks wrong code that `-O0` hid

---

## Bug Anchor

- Foundational; every later bug uses these accessors.
- **Cross-link to Day 23 (NVMe):** `wait_csts_rdy` is `wait_for_bit_set` on `CSTS.RDY`.
- **Cross-link to Day 28 (PCU/ADR):** the missing wait-for-ack is the firmware analogue of forgetting the timeout in this loop.
- **Cross-link to Day 20 (DMA/doorbells):** `volatile` alone is not enough on weakly ordered ISAs — you'll need barriers.

## Exit Check

- [ ] Wrote `BIT/MASK/FIELD_GET/FIELD_PREP/ARRAY_SIZE` cold.
- [ ] Wrote `reg_read32/reg_write32/reg_field_write` cold with correct `volatile`.
- [ ] Wrote `wait_for_bit_set` cold including the timeout return.
- [ ] Stated the `volatile` vs barrier distinction in 30 seconds.
- [ ] Listed two RMW gotchas (cross-core atomicity, W1C corruption).
- [ ] Wrote 6 of the 8 standard one-liners without notes.
- [ ] Predicted `FIELD_GET(0x12345678, 23, 16) = 0x34` without running it.

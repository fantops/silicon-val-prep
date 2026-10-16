# S07 - Registers: MMIO, config space, MSRs, bits

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [D3 bits/MMIO/volatile/polling](archive/day03_bits_mmio_volatile_polling.md); skim [D1](archive/day01_c_address_fluency.md)/[D2](archive/day02_pointers_memory_layout_alignment.md).
- **Must answer:**
  - What `volatile` guarantees and what it doesn't. Read-modify-write races.
  - MMIO vs config space vs MSR: who uses which? A device BAR is an MMIO window.
  - Polling with a timeout. Why does every hang debug start with "what was it polling?"
- **Work link:** list the DSA registers you read most while debugging (capabilities, status, error, WQ config). For each one, which BAR or space is it in?
- **Handoff →** How is memory built and cached?

## Study notes

### Concept - [day03_bits_mmio_volatile_polling.md](archive/day03_bits_mmio_volatile_polling.md)

Hardware registers are not variables — they are bit-field collections at memory-mapped addresses, and every byte on the wire has an endianness. Today consolidates: register field RMW, why `volatile` matters and where it ends, polling-with-timeout, and parsing little-/big-endian buffers without struct overlay.

### Deep Study Notes - [day03_bits_mmio_volatile_polling.md](archive/day03_bits_mmio_volatile_polling.md)

### 1. Hex ↔ binary (must be instant)

```
0x0=0000 0x1=0001 0x2=0010 0x3=0011  0x4=0100 0x5=0101 0x6=0110 0x7=0111
0x8=1000 0x9=1001 0xA=1010 0xB=1011  0xC=1100 0xD=1101 0xE=1110 0xF=1111
```

`0xA5 = 1010_0101`. Split each hex digit into 4 bits and you can read any register dump.

### 2. The six bitwise operators and the four register patterns

| Op | Effect | Use |
|---|---|---|
| `&` | both 1 → 1 | mask in |
| `\|` | either 1 → 1 | set bits |
| `^` | differ → 1 | toggle |
| `~` | flip all | clear-mask |
| `<<` | × 2^n | shift up |
| `>>` | / 2^n | shift down |

```c
reg |= (1U << bit);                /* set */
reg &= ~(1U << bit);               /* clear */
reg ^= (1U << bit);                /* toggle */
if (reg & (1U << bit)) { ... }     /* test */
```

Always `1U`. `1 << 31` (signed) is undefined behaviour.

### 3. Field extract and insert (the RMW pattern)

```c
uint32_t extract_field(uint32_t reg, int start, int width) {
    return (reg >> start) & ((1U << width) - 1);
}
uint32_t insert_field(uint32_t reg, int start, int width, uint32_t v) {
    uint32_t mask = ((1U << width) - 1) << start;
    return (reg & ~mask) | ((v << start) & mask);
}
```

The `& ~mask` clears the field; the `|` writes the new bits. Forgetting the clear is the #1 bug — old bits remain set, you OR in new bits, the field is wrong.

**RMW gotchas to say out loud:**

- Not atomic across cores.
- Corrupts **W1C** (write-1-to-clear) status registers — reading then writing back the same value clears every set status bit silently.
- Races against HW updates of the same register.

### 4. `volatile` — what it is and isn't

```c
volatile uint32_t *status = (volatile uint32_t *)0xFE000010;
while (!(*status & 0x1)) { }   /* compiler MUST re-read every iteration */
```

`volatile` says: every access to this address really happens, in source order, no caching, no elision, no reordering with respect to other `volatile` accesses.

Three things `volatile` does:

1. Every read goes to memory.
2. Every write goes to memory.
3. The compiler preserves order between `volatile` accesses.

Three things `volatile` does **not** do:

- It is **not** a memory barrier. CPU reordering of accesses to plain memory around the `volatile` access is still allowed.
- It is **not** atomic against other CPUs.
- It does **not** flush a cache.

The classic interview trap: "the compiler hoisted my `*status_reg` out of the polling loop and the loop never terminates." Bug = missing `volatile`. Reproducible only with optimisations enabled, which is why embedded teams often disable inlining of register accessors.

**On weakly ordered ISAs (ARM, RISC-V):** `volatile` orders within `volatile`, but the descriptor-then-doorbell pattern needs a barrier between the (cacheable) descriptor write and the (uncached) doorbell write:

```c
desc->cmd = ...;
__asm__ volatile("dmb st" ::: "memory");   /* publish descriptor first */
*(volatile uint32_t *)doorbell = 1;
```

On x86 (Total Store Order) `volatile` alone is usually enough for single-core MMIO, but a `mfence` is needed against device DMA reading from cacheable memory.

### 5. Why struct overlay on MMIO is a bug class, not a style choice

```c
/* DANGEROUS for MMIO */
typedef struct {
    uint32_t ctrl;       /* offset 0x00 */
    uint32_t status;     /* offset 0x04 — or 0x08 if compiler pads */
    uint16_t config;     /* offset 0x08? alignment varies! */
    uint32_t data;       /* offset?? */
} regs_t;
```

Three failure modes:

1. **Padding mismatch** — compiler may insert padding the hardware register map doesn't have.
2. **Bulk struct copy** — `*regs = defaults;` becomes a `memcpy` that reads adjacent fields, including W1C status bits, as a side effect.
3. **Field write order** — assigning multiple struct fields gives the compiler freedom to reorder, but hardware often requires a specific order (e.g., `DATA` before `TRIGGER`).

**Fix:** explicit `reg_read32(base, offset)` / `reg_write32(base, offset, val)` accessors. Pay the verbosity cost.

### 6. Polling-with-timeout (revisited)

```c
bool poll_ready(volatile void *base, uint32_t off,
                uint32_t mask, uint32_t expected, uint32_t timeout_us) {
    for (uint32_t i = 0; i < timeout_us; i++) {
        if ((reg_read32(base, off) & mask) == expected) return true;
        usleep(1);
    }
    return false;
}
```

Always handle the timeout. The NVMe `CSTS.RDY` bug returned `false` after `CAP.TO × 500 ms`, which forced the host to dump `CSTS.CFS` and the AQA register — that dump is what revealed the field-width mismatch.

### 6a. ISR mechanics — when served, where state goes

*The Intel Bangalore probe: "when an ISR fires, where does the data get stored, by what mechanism?" Answer with the hardware/software split.*

```
   IRQ line asserted (or MSI write to LAPIC / GIC distributor)
        │
        ▼
   Interrupt controller (LAPIC/APIC, GIC) prioritises + selects target CPU
        │
        ▼
   CPU samples at instruction boundary  — if unmasked + priority > current
        │
        ▼
   HARDWARE auto-save (per ISA):
     x86       : push SS, RSP, RFLAGS, CS, RIP, (error code) onto kernel stack;
                 vector → IDT[vec] → handler entry
     ARMv8 AArch64 : SPSR_ELx ← PSTATE, ELR_ELx ← return PC,
                 ESR_ELx ← syndrome; branch to VBAR_ELx + offset
     ARMv7-M (Cortex-M) : HW pushes {R0-R3, R12, LR, PC, xPSR} onto MSP/PSP
                 ("stacking"); vector table at VTOR points to handler
     RISC-V    : mepc ← PC, mcause ← cause, mstatus.MIE→MPIE; jump to mtvec
        │
        ▼
   SOFTWARE prologue (compiler / OS):
     - Save remaining caller-saved GPRs the handler will clobber
     - Save FPU/SIMD lazily (only if handler touches them)
     - Switch to interrupt stack if separate (per-CPU IRQ stack on Linux)
     - Build pt_regs / context frame for the kernel
        │
        ▼
   Handler runs (top half / FLIH — keep it short, mask only what you must)
        │
        ▼
   Acknowledge / EOI to interrupt controller
        │
        ▼
   Epilogue restores in reverse  — IRET / ERET / MRET returns atomically
```

**Storage answer in three layers:**

1. **Vector table** (IDT / VBAR / VTOR / mtvec) — *where to go*. ROM-fixed location; entry holds handler address (and on x86, segment + DPL).
2. **Hardware auto-save** — *minimum needed to return atomically*. Either onto the kernel stack (x86, Cortex-M) or into banked system registers (AArch64 ELR/SPSR/ESR; ARMv7-A banked R13/R14 per mode).
3. **Software context frame** — *the rest*. Handler prologue saves remaining GPRs + (lazily) FPU into a `pt_regs`-shaped frame on the per-CPU IRQ stack.

**Reflex one-liner:** *"Vector table tells the CPU where to jump; hardware auto-saves the minimum needed for atomic return (stack or banked regs depending on ISA); software prologue completes the context frame on the IRQ stack. ISR runs, EOIs the controller, IRET unwinds."*

**Bottom-half / deferred work:** anything that can sleep or take long — softirq, tasklet, workqueue, threaded IRQ. Top half just records + schedules.

### 7. Endianness — the picture you must be able to draw

For `0xDEADBEEF` at address `0x100`:

```
Address:    0x100  0x101  0x102  0x103
Big-Endian:  DE     AD     BE     EF      MSB at lowest address
Little-End:  EF     BE     AD     DE      LSB at lowest address
```

- **LE:** x86, ARM (default), RISC-V, PCIe wire format, NVMe registers.
- **BE:** network byte order (Ethernet, IP, TCP/UDP), some PowerPC and SPARC.
- **ARM:** configurable per-core, but LE dominates in practice.

### 8. Endian-safe parsing (no struct overlay, no casts)

```c
uint16_t read_le16(const uint8_t *b) { return (uint16_t)b[0] | ((uint16_t)b[1] << 8); }
uint32_t read_le32(const uint8_t *b) {
    return (uint32_t)b[0]       | ((uint32_t)b[1] << 8)
         | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
uint16_t read_be16(const uint8_t *b) { return ((uint16_t)b[0] << 8) | (uint16_t)b[1]; }
uint32_t read_be32(const uint8_t *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] << 8)  | (uint32_t)b[3];
}
void write_le32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v>>8); b[2] = (uint8_t)(v>>16); b[3] = (uint8_t)(v>>24);
}
uint32_t swap32(uint32_t x) {
    return ((x>>24)&0xFFu) | ((x>>8)&0xFF00u) | ((x<<8)&0xFF0000u) | ((x<<24)&0xFF000000u);
}
```

Why parse byte-by-byte instead of casting `uint8_t *` to `uint32_t *`?

- **Endianness mismatch** between source format and host byte order silently flips bytes.
- **Unaligned access** on some ARM/MIPS cores traps or emulates slowly.
- **Strict aliasing** rules technically make the cast undefined behaviour.

The byte-by-byte version generates the same code on a modern compiler with optimisation on, costs nothing, and is portable.

### 9. Useful bit tricks

| Trick | Code | Use |
|---|---|---|
| Power of 2 | `n && !(n & (n-1))` | validate alignment / sizes |
| Clear lowest set | `n & (n-1)` | Kernighan popcount |
| Isolate lowest set | `n & -n` | find first set bit |
| Popcount | `while (n) { c++; n &= n-1; }` | bit count |
| Align up | `(addr + a-1) & ~(a-1)` | page/cacheline alignment |

---

### Bugs in this Topic - [day03_bits_mmio_volatile_polling.md](archive/day03_bits_mmio_volatile_polling.md)

- **Compiler hoists status read out of polling loop:** missing `volatile`, loop spins forever. Reproducible only with `-O2`. Fix: declare the pointer `volatile uint32_t *`.
- **W1C status corruption via RMW:** read-modify-write of a status register clears every set status bit. Fix: write-only, never RMW, on W1C registers.
- **Endianness bug on TLP / Ethernet header:** read TCP port `0x0050` (port 80) on x86 as a `uint16_t` cast and get `0x5000`. Fix: `ntohs()` or `read_be16(buf)`.
- **NVMe `CSTS.RDY` bug seen via the polling timeout:** `AQA` programmed with the wrong field width meant the device never asserted RDY; the timeout in `wait_csts_rdy` is what surfaced the failure rather than letting the host hang.
- **Struct-overlay bulk copy hits W1C side effects:** `*regs = defaults;` becomes a `memcpy` that reads `STATUS` and clears every set bit as a side effect. Fix: explicit per-offset writes.

---

### Interview Explanation (60-90 sec) - [day03_bits_mmio_volatile_polling.md](archive/day03_bits_mmio_volatile_polling.md)

> "Hardware registers are bit-field collections at MMIO addresses; I always go through `volatile` accessors and a `BIT/MASK/FIELD_GET/FIELD_PREP` chain. `volatile` prevents compiler caching and reordering of accesses to that address, but it is not a memory barrier or atomic — on ARM I add a `dmb` between the descriptor write and the doorbell. RMW corrupts W1C status registers and is not atomic across cores, so I treat status registers as write-only. For polling I always use a timeout; the NVMe `CSTS.RDY` bug I worked on was surfaced by exactly that timeout. For wire formats and protocol headers I never cast a `uint8_t *` to `uint32_t *` — I parse byte-by-byte with `read_le32`/`read_be32`, which avoids endianness traps, unaligned access, and strict-aliasing UB. Struct overlay over MMIO is a bug class — I use explicit `reg_read32(base, offset)`."

---

### Architect Drills - [day03_bits_mmio_volatile_polling.md](archive/day03_bits_mmio_volatile_polling.md)

*Reason from constraints. Endianness, RMW, polling, and `volatile` are policy decisions of the C standard and the bus, not laws of physics. Derive why each landed where it did. No answer key.*

**D1. Why is endianness a thing at all — why didn't the industry standardise on one?**
*Constraints to weigh:* native carry-propagation direction, human-readable byte order in hex dumps, network protocols predating x86 dominance, swap cost (zero on x86 LE, ~1 cycle on ARM with REV), legacy hardware that exposes the wire format. What concrete things break if you forced one endianness on the world today?

**D2. PCIe defines LE on the wire even though PCI was endian-neutral. Reason about who paid the cost and why.**
*Constraints to weigh:* host CPU population at the time, swap cost in PCIe controller silicon, debug ergonomics with a logic analyser, BE host viability. Network is BE, PCIe is LE — what does this say about who bargains harder, the chip vendor or the protocol committee?

**D3. Why doesn't the compiler insert a memory barrier automatically around every `volatile` access on weak-ordered targets?**
*Constraints to weigh:* most `volatile` uses are flags not MMIO; cost of a universal `dmb` on every load; portability of C; explicit-is-safer philosophy; how a "smart" compiler would distinguish MMIO from flag without losing portability. What would a `volatile_mmio_t` extension need, and why hasn't it landed?

**D16. Polling-loop timeouts are universal. Derive when polling beats interrupts for a given event arrival rate.**
*Constraints to weigh:* per-poll cost (cycles + cache pollution), interrupt latency + ISR overhead, jitter tolerance, what the CPU does between polls (busy-wait vs WFE vs yield), expected wait distribution. NVMe goes from interrupts to polling on hot completions — derive the crossover.

**D17. W1C status bits exist because of a hardware/software race. Why isn't "read-to-clear" used everywhere instead?**
*Constraints to weigh:* re-entrant ISR safety (a second handler must see the bit), debug printk reading status without side effects, RC requires every handler to remember not to read twice. What are the failure modes of each, and which one a multi-CPU OS prefers — and why?

**D18. Struct-overlay over an MMIO block is forbidden but ergonomic. What would the language need to add to make it safe?**
*Constraints to weigh:* per-field load/store semantics (no bulk memcpy), explicit no-padding, explicit field width, no implicit RMW. Rust's `bitfield` crate and Linux's `regmap` solve this differently — compare. What survives in a hypothetical `mmio struct` keyword?

**D19. Imagine MMIO is replaced by CXL.io with full request/response semantics. Which Day-3 disciplines stay load-bearing, and which become legacy?**
*Constraints to weigh:* completion semantics replace posted-write fire-and-forget, error reporting becomes synchronous, ordering becomes per-transaction, but `volatile` + barrier still constrains the *compiler*. What does a "modern" register-poke look like in CXL-land?

---

### Exit Check - [day03_bits_mmio_volatile_polling.md](archive/day03_bits_mmio_volatile_polling.md)

- [ ] Stated `volatile` vs memory barrier in 30 seconds.
- [ ] Listed the three failure modes of struct-overlay-on-MMIO.
- [ ] Wrote `extract_field` / `insert_field` cold.
- [ ] Wrote `read_le32` and `read_be32` cold.
- [ ] Drew `0x12345678` in LE and BE memory at `0x100`.
- [ ] Stated why W1C registers must be write-only.
- [ ] Wrote `poll_ready` with timeout cold.
- [ ] Predicted the result of `reg_rmw` on a sample value without running it.

### Concept - [day01_c_address_fluency.md](archive/day01_c_address_fluency.md)

A pointer is just an address with a type. Hardware registers are just addresses with **side effects** on every load/store. Today you build the macro set (`BIT`, `MASK`, `FIELD_GET`, `FIELD_PREP`, `ARRAY_SIZE`) and the accessor set (`reg_read32`, `reg_write32`, `reg_field_write`, `wait_for_bit_set`) that the next 20 days reuse verbatim.

### Deep Study Notes - [day01_c_address_fluency.md](archive/day01_c_address_fluency.md)

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

### Bugs in this Topic - [day01_c_address_fluency.md](archive/day01_c_address_fluency.md)

Today's macros are too foundational to "own" a single bug, but every later bug story passes through them. Two examples to keep in mind:

- **NVMe `CSTS.RDY` AQA bug (Day 23):** the host wrote `AQA` with the wrong field width and `CSTS.RDY` never asserted. Caught by the timeout in `wait_for_bit_set`. Without that timeout the bring-up would have hung forever.
- **PCU/ADR opcode-ordering bug (Day 28):** `WPQ_FLUSH` was issued before `IO_QUIESCE_ACK`. It is the same shape as forgetting to wait for a status bit before issuing the next operation.

If your `reg_field_write` and `wait_for_bit_set` are sloppy, every later bug story is harder to debug because you are fighting your own scaffolding.

---

### Interview Explanation (60-90 sec) - [day01_c_address_fluency.md](archive/day01_c_address_fluency.md)

> "MMIO is just memory-mapped registers — every load and store has a hardware side effect, so I always go through `reg_read32`/`reg_write32` accessors with a `volatile uint32_t *` cast. To program a single field I use a `BIT`/`MASK`/`FIELD_GET`/`FIELD_PREP` macro chain and a 5-line `reg_field_write` that does read, mask, OR, write — being careful that read-modify-write is not atomic across cores and corrupts W1C status registers. To wait on a status bit I use a 6-line `wait_for_bit_set(reg, mask, timeout_us)` with an explicit timeout — that exact shape is `wait_csts_rdy` in NVMe, `wait_link_up` in PCIe, and `wait_pll_lock` in clocking. The timeout is what caught my NVMe `CSTS.RDY` bring-up bug; without it the host would have hung silently."

---

### Architect Drills - [day01_c_address_fluency.md](archive/day01_c_address_fluency.md)

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

### Exit Check - [day01_c_address_fluency.md](archive/day01_c_address_fluency.md)

- [ ] Wrote `BIT/MASK/FIELD_GET/FIELD_PREP/ARRAY_SIZE` cold.
- [ ] Wrote `reg_read32/reg_write32/reg_field_write` cold with correct `volatile`.
- [ ] Wrote `wait_for_bit_set` cold including the timeout return.
- [ ] Stated the `volatile` vs barrier distinction in 30 seconds.
- [ ] Listed two RMW gotchas (cross-core atomicity, W1C corruption).
- [ ] Wrote 6 of the 8 standard one-liners without notes.
- [ ] Predicted `FIELD_GET(0x12345678, 23, 16) = 0x34` without running it.

### Concept - [day02_pointers_memory_layout_alignment.md](archive/day02_pointers_memory_layout_alignment.md)

Where data lives (stack / heap / BSS / data / text), how the compiler lays out a struct (natural alignment + tail padding), and three ways to obtain an aligned buffer (DMA / SIMD / page) — plus the kernel idiom `container_of` that recovers the parent struct from a member pointer.

### Deep Study Notes - [day02_pointers_memory_layout_alignment.md](archive/day02_pointers_memory_layout_alignment.md)

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

### Bugs in this Topic - [day02_pointers_memory_layout_alignment.md](archive/day02_pointers_memory_layout_alignment.md)

- **Struct-overlay MMIO bug (Day 3 / Day 20 link):** declaring a struct over a register block and relying on `__attribute__((packed))` ordering. Compilers may add padding differently than the hardware register map; struct assignment can read adjacent W1C bits as a side effect; field write order is not guaranteed. **Fix:** explicit `reg_read32`/`reg_write32` per offset.
- **DMA buffer crossing a cache line (Day 16 link):** a 60-byte buffer at a 16-byte aligned address crosses two 64-byte cache lines, causing partial-line writebacks and stale-data races against the device. **Fix:** `aligned_alloc(64, ALIGN_UP(size, 64))`.
- **NPU buffer not page-aligned (Day 34 link):** the QNN/ORT page-fault story has a flavour of this — context buffers touched in non-page-aligned strides spike minor page faults at the start of each layer. **Fix:** allocate context buffers at page granularity and respect tile alignment.
- **Stack overflow in ISR:** a deep call chain inside an interrupt handler corrupts the next task's locals. Symptom is "random" wrong values that move with build changes. **Fix:** keep ISRs flat, size stacks per-task, use a stack-canary build.

---

### Interview Explanation (60-90 sec) - [day02_pointers_memory_layout_alignment.md](archive/day02_pointers_memory_layout_alignment.md)

> "Pointers are addresses with a type — the type only changes how the bytes are interpreted and how pointer arithmetic scales. A C program lives in five segments: stack, heap, BSS, data, text; in bare-metal firmware we usually static-allocate everything because heap fragmentation and ISR-context allocation are unsafe. Struct layout follows natural alignment plus tail padding, so you order fields largest-first to minimise padding — `__attribute__((packed))` is a footgun on ARM. For DMA, SIMD, or NPU tile buffers I use `aligned_alloc`, `posix_memalign`, or a manual wrapper that stashes the original pointer one slot before the aligned address. The kernel idiom `container_of` recovers a parent struct from a member pointer using `offsetof` — it's the foundation of Linux driver callbacks. Most layout bugs in silicon firmware come from struct-overlay over MMIO or from a DMA buffer that crosses a cache line; the canonical tell is a descriptor allocated at offset 56 in a 64-byte line so the device's coherent fetch straddles two lines and the second half arrives stale — fix is a 64-byte-aligned allocator, and for MMIO, explicit `reg_read32`/`reg_write32` instead of struct overlay so the compiler can't pad or reorder the register map."

---

### Architect Drills - [day02_pointers_memory_layout_alignment.md](archive/day02_pointers_memory_layout_alignment.md)

*Reason from constraints. The right answer is whichever the hardware bus, compiler ABI, and workload force. No answer key.*

**D1. Why is alignment a hardware requirement at all — why don't CPUs just rotate-and-mask any byte boundary?**
*Constraints to weigh:* memory bus width, cache-line crossing penalty, atomic-access guarantees (lock-free needs naturally aligned), MMU page boundaries, vendor cost of a barrel-shifter in the load/store unit. ARMv7 trapped on unaligned, ARMv8 mostly allows it — why the change, and why does Apple Silicon still penalise it on hot paths?

**D2. Why does a 60-byte DMA buffer at a 16-byte-aligned address corrupt silently while a 64-byte buffer at the same address is fine?**
*Constraints to weigh:* cache-line straddling, partial-line writeback semantics, who owns each line in MESI when the device fetches, the moment of "visible to device." Walk the timing diagram. What does a 4 KB DMA buffer at offset 60 in a page do that's *worse* than the 60-byte case?

**D3. Struct field reordering can save 33% of memory. Why doesn't the C compiler reorder fields automatically?**
*Constraints to weigh:* ABI stability across translation units, ordered initialisers `{a,b,c}`, struct-overlay onto MMIO/protocol headers (where order is a contract), `offsetof` predictability for `container_of`. Rust *does* reorder by default — what gives Rust permission that C doesn't have?

**D16. `container_of` is the foundation of Linux callbacks. What would force a future kernel to drop it?**
*Constraints to weigh:* C lacks generics, fat pointers cost a word, the alternative (parallel hash map from member→parent) costs a lookup per callback, intrusive lists vs allocated wrappers, debuggability under crash dumps. If C added zero-cost interfaces, where would `container_of` survive anyway?

**D17. Bare-metal firmware bans `malloc`. Construct the workload where you'd genuinely need it back, and reason about what you'd accept losing.**
*Constraints to weigh:* fragmentation, ISR context, deterministic latency budgets, slab/pool alternatives, OOM recovery without an MMU. What does Zephyr's `k_heap` actually buy that pure pools don't, and at what cost in worst-case latency?

**D18. `__attribute__((packed))` is a footgun on ARM. What architectural choice would make it safe — and at what cost?**
*Constraints to weigh:* unaligned-access trap-and-emulate cost, per-load alignment check in the LSU, code-size cost (compiler emits byte-by-byte access for packed fields), wire-format pragmatism (network / disk headers want packed). Why is x86 "just fine" with packed and ARM punishes it — what's the silicon trade?

**D19. Five segments (text/data/bss/heap/stack) is the textbook layout. Imagine an MCU with one 64 KB SRAM and no MMU — does the segmentation still earn its keep?**
*Constraints to weigh:* link-time symbol resolution, stack-overflow detection without an MPU, R/W/X permissions, ROM-constant savings (text in flash), zero-init guarantee for BSS. When does a hand-rolled "everything in one .data" win, and what defects does it invite?

---

### Exit Check - [day02_pointers_memory_layout_alignment.md](archive/day02_pointers_memory_layout_alignment.md)

- [ ] Drew the 5-segment memory map from memory.
- [ ] Predicted struct A/B/C `sizeof` correctly.
- [ ] Wrote three ways to get aligned memory.
- [ ] Wrote `container_of` using `offsetof`.
- [ ] Stated why `__attribute__((packed))` is unsafe on ARM.
- [ ] Stated why struct overlay over MMIO is a bug class, not a style preference.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S06](s06_firmware_boot.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S08](s08_memory_caches.md)

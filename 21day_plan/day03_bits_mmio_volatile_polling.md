# Day 03 — Bits, MMIO, volatile, Polling, Endianness

## Concept

Hardware registers are not variables — they are bit-field collections at memory-mapped addresses, and every byte on the wire has an endianness. Today consolidates: register field RMW, why `volatile` matters and where it ends, polling-with-timeout, and parsing little-/big-endian buffers without struct overlay.

## Why this day in the flow

Day 1 gave you the macros, Day 2 gave you layout. Day 3 puts them on a real register block: program a field, poll a status bit, parse a TLP/Ethernet/NVMe header. The next layer (caches, VM, DMA, PCIe, NVMe) is all *register programming + buffer parsing*. If today is solid, Days 8 (DMA), 10 (PCIe), 11 (NVMe) are scaffolding work, not new concepts.

The two bug stories that anchor today:

- **NVMe `CSTS.RDY` / AQA bug** (Day 11): wrong field width on `AQA`, polling on `CSTS.RDY` with a timeout caught the failure.
- **PCU/ADR opcode-ordering bug** (Day 14): missing wait-for-ack between firmware opcodes — the polling-with-timeout discipline expressed in firmware.

---

## Deep Study Notes

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

## Bugs in this Topic

- **Compiler hoists status read out of polling loop:** missing `volatile`, loop spins forever. Reproducible only with `-O2`. Fix: declare the pointer `volatile uint32_t *`.
- **W1C status corruption via RMW:** read-modify-write of a status register clears every set status bit. Fix: write-only, never RMW, on W1C registers.
- **Endianness bug on TLP / Ethernet header:** read TCP port `0x0050` (port 80) on x86 as a `uint16_t` cast and get `0x5000`. Fix: `ntohs()` or `read_be16(buf)`.
- **NVMe `CSTS.RDY` bug seen via the polling timeout:** `AQA` programmed with the wrong field width meant the device never asserted RDY; the timeout in `wait_csts_rdy` is what surfaced the failure rather than letting the host hang.
- **Struct-overlay bulk copy hits W1C side effects:** `*regs = defaults;` becomes a `memcpy` that reads `STATUS` and clears every set bit as a side effect. Fix: explicit per-offset writes.

---

## Interview Explanation (60-90 sec)

> "Hardware registers are bit-field collections at MMIO addresses; I always go through `volatile` accessors and a `BIT/MASK/FIELD_GET/FIELD_PREP` chain. `volatile` prevents compiler caching and reordering of accesses to that address, but it is not a memory barrier or atomic — on ARM I add a `dmb` between the descriptor write and the doorbell. RMW corrupts W1C status registers and is not atomic across cores, so I treat status registers as write-only. For polling I always use a timeout; the NVMe `CSTS.RDY` bug I worked on was surfaced by exactly that timeout. For wire formats and protocol headers I never cast a `uint8_t *` to `uint32_t *` — I parse byte-by-byte with `read_le32`/`read_be32`, which avoids endianness traps, unaligned access, and strict-aliasing UB. Struct overlay over MMIO is a bug class — I use explicit `reg_read32(base, offset)`."

---

## Architect Drills

*Reason from constraints. Endianness, RMW, polling, and `volatile` are policy decisions of the C standard and the bus, not laws of physics. Derive why each landed where it did. No answer key.*

**D1. Why is endianness a thing at all — why didn't the industry standardise on one?**
*Constraints to weigh:* native carry-propagation direction, human-readable byte order in hex dumps, network protocols predating x86 dominance, swap cost (zero on x86 LE, ~1 cycle on ARM with REV), legacy hardware that exposes the wire format. What concrete things break if you forced one endianness on the world today?

**D2. PCIe defines LE on the wire even though PCI was endian-neutral. Reason about who paid the cost and why.**
*Constraints to weigh:* host CPU population at the time, swap cost in PCIe controller silicon, debug ergonomics with a logic analyser, BE host viability. Network is BE, PCIe is LE — what does this say about who bargains harder, the chip vendor or the protocol committee?

**D3. Why doesn't the compiler insert a memory barrier automatically around every `volatile` access on weak-ordered targets?**
*Constraints to weigh:* most `volatile` uses are flags not MMIO; cost of a universal `dmb` on every load; portability of C; explicit-is-safer philosophy; how a "smart" compiler would distinguish MMIO from flag without losing portability. What would a `volatile_mmio_t` extension need, and why hasn't it landed?

**D4. Polling-loop timeouts are universal. Derive when polling beats interrupts for a given event arrival rate.**
*Constraints to weigh:* per-poll cost (cycles + cache pollution), interrupt latency + ISR overhead, jitter tolerance, what the CPU does between polls (busy-wait vs WFE vs yield), expected wait distribution. NVMe goes from interrupts to polling on hot completions — derive the crossover.

**D5. W1C status bits exist because of a hardware/software race. Why isn't "read-to-clear" used everywhere instead?**
*Constraints to weigh:* re-entrant ISR safety (a second handler must see the bit), debug printk reading status without side effects, RC requires every handler to remember not to read twice. What are the failure modes of each, and which one a multi-CPU OS prefers — and why?

**D6. Struct-overlay over an MMIO block is forbidden but ergonomic. What would the language need to add to make it safe?**
*Constraints to weigh:* per-field load/store semantics (no bulk memcpy), explicit no-padding, explicit field width, no implicit RMW. Rust's `bitfield` crate and Linux's `regmap` solve this differently — compare. What survives in a hypothetical `mmio struct` keyword?

**D7. Imagine MMIO is replaced by CXL.io with full request/response semantics. Which Day-3 disciplines stay load-bearing, and which become legacy?**
*Constraints to weigh:* completion semantics replace posted-write fire-and-forget, error reporting becomes synchronous, ordering becomes per-transaction, but `volatile` + barrier still constrains the *compiler*. What does a "modern" register-poke look like in CXL-land?

---

## Coding A — Systems C: device init harness with poll + RMW

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define BIT(n)               (1U << (n))
#define MASK(msb, lsb)       (((1U << ((msb)-(lsb)+1)) - 1) << (lsb))

static uint8_t mmio[256];

static inline uint32_t reg_read(uint32_t off) {
    uint32_t v; memcpy(&v, &mmio[off], 4); return v;
}
static inline void reg_write(uint32_t off, uint32_t v) {
    memcpy(&mmio[off], &v, 4);
}
static inline void reg_rmw(uint32_t off, uint32_t mask, uint32_t v) {
    uint32_t t = reg_read(off);
    t = (t & ~mask) | (v & mask);
    reg_write(off, t);
}

#define CTRL    0x00
#define STATUS  0x04
#define DATA    0x08
#define CTRL_EN     BIT(0)
#define CTRL_START  BIT(1)
#define CTRL_RESET  BIT(31)
#define STATUS_RDY  BIT(0)

static bool poll_ready(uint32_t off, uint32_t mask, uint32_t exp, uint32_t tries) {
    for (uint32_t i = 0; i < tries; i++) {
        if ((reg_read(off) & mask) == exp) return true;
        if (i == 2) reg_write(off, exp);  /* simulate HW asserting RDY */
    }
    return false;
}

int main(void) {
    memset(mmio, 0, sizeof(mmio));

    /* Reset */
    reg_write(CTRL, CTRL_RESET);
    reg_write(CTRL, 0);

    /* Wait ready */
    if (!poll_ready(STATUS, STATUS_RDY, STATUS_RDY, 5)) {
        puts("ERR: device never ready"); return 1;
    }

    /* Enable + start (RMW preserves other bits) */
    reg_rmw(CTRL, CTRL_EN, CTRL_EN);
    reg_rmw(CTRL, CTRL_START, CTRL_START);

    reg_write(DATA, 0xDEADBEEF);
    printf("DATA readback = 0x%08X\n", reg_read(DATA));
    return 0;
}
```

## Coding B — Interview DS&A: Number of 1 Bits (LC 191)

Kernighan's `n & (n-1)` — exactly the popcount in your bit-trick table.

```python
def hammingWeight(n):
    c = 0
    while n:
        n &= n - 1
        c += 1
    return c
```

---

## System Thinking — Why hardware access is *not* normal C

**Frame:** writing `*reg = 1` looks like writing memory, but it isn't. The compiler, the cache, and the CPU's reordering machinery were all built on the assumption that loads/stores are *idempotent* and *side-effect-free*. Hardware registers violate every one of those assumptions. Every keyword on this day exists to tell one of those layers "this address is different."

### Q: Why is `volatile` needed? Doesn't the compiler just do what I write?

No — the compiler is *built* to not do what you write, when it knows better. If you write:

```c
while (*status_reg & BUSY) ;
```

the compiler sees a load with no side effects in a loop and hoists it out: "the value can't change, so check it once." Result: infinite loop. `volatile` tells the compiler **"this address has side effects you cannot see — issue exactly the loads/stores I wrote, in order, and don't cache the value in a register."** Without `volatile`, every poll loop and every MMIO write is potentially miscompiled at -O2.

**`volatile` does NOT give you:** atomicity, ordering between *different* volatile addresses, or memory barriers. For those you need explicit barriers (Day 8).

### Q: Why are MMIO regions mapped uncached / device-type?

Because the cache breaks device semantics in three ways:
1. **Write coalescing:** the cache might combine 4 byte-writes into one 4-byte write — but the device may interpret each byte-write as a separate command (e.g., a UART TX-data write per byte).
2. **Write reordering:** the cache may delay a write indefinitely (write-back). The device sees stale state.
3. **Read squashing:** a read from a status register the cache thinks it already has may return the old cached value forever.

Mapping the region as Device or Strongly-Ordered (ARM) / UC or WC (x86) tells the MMU to skip the cache entirely — every load/store goes straight to the bus. This costs latency but *that's the entire point*: you needed to talk to the device, not to a stale snapshot of it.

### Q: Polling vs interrupts — when do I use which?

A pure tradeoff between **latency, CPU cost, and event rate**:
- **Polling wins** when the event is *expected soon* (microseconds) or *frequent* (every few μs). Interrupt entry/exit costs ~hundreds of cycles plus cache pollution; if events arrive faster than that, IRQs collapse.
- **Interrupts win** when the event is *rare* or *latency-tolerant*. CPU sleeps; one wake handles the work.
- **Hybrid (NAPI / interrupt-coalescing / poll-after-IRQ):** trigger via IRQ for the first event, then poll while events keep coming. Used by every modern NIC and NVMe driver.

The NVMe completion queue with phase bit (Day 11) is *poll-style* even though MSI-X is set up — because at high IOPS, polling beats IRQs.

### One-liner

*"Writing a hardware register isn't writing memory. `volatile` stops the compiler from caching loads in registers. Uncached/device mappings stop the CPU cache from coalescing or reordering writes. Barriers (Day 8) stop the CPU's reorder buffer. And polling beats interrupts when events arrive faster than the IRQ entry/exit cost. Each keyword exists to opt one layer of optimization back out — because hardware doesn't follow C's rules."*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **volatile** — *what:* compiler must emit every load/store / *why:* HW value changes outside the program / *how:* disables register caching + reorder across the access / *connects:* MMIO, ISR-shared vars; **NOT** atomicity, **NOT** a memory barrier
- **MMIO vs DRAM** — *what:* device regs in CPU address space / *why:* uniform load/store ISA / *how:* UC/WC mapping, no cache fill / *connects:* posted writes, BAR setup, write combining
- **RMW on bit fields** — *what:* read | mask | write / *why:* preserve other bits / *how:* `*reg = (*reg & ~MASK) | val` / *connects:* race if HW also writes the reg, W1C semantics
- **polling vs interrupt** — *what:* CPU spin vs HW notify / *why:* latency vs CPU-cycle tradeoff / *how:* poll for sub-µs events, IRQ for long waits / *connects:* NVMe SQ doorbell + CQ phase bit, NAPI hybrid
- **W1C / W1S registers** — *what:* write-1-to-clear / write-1-to-set / *why:* avoid RMW races on status bits / *how:* HW clears bit on write of 1 / *connects:* IRQ status regs, why `reg |= bit` is wrong on W1C

---

## Extension — Low-speed buses (UART / I2C / SPI / QSPI flash)

The MMIO + polling fluency above is the same pattern you use to bring up every low-speed peripheral. MCU/SoC bring-up roles (Renesas, NXP, TI, ST, Infineon, Microchip) ask these by name.

### UART
- Frame: start (0) + data LSB-first + optional parity + stop. Common 8N1.
- Baud = source clock / divider; **end-to-end error budget ≤ ~2%** per frame (TX clock + RX clock combined). Wrong PLL ⇒ wrong divisor ⇒ garbled console — first thing to check when banner is gibberish.
- Flow control: RTS/CTS HW; XON/XOFF SW. Break = TX held low > 1 frame.
- Bring-up checks: scope TX (idle high, frame shape, baud measured = 1/bit-width); loopback TX→RX; start at 9600, then target.
- Bug class: pin-mux strap routes UART to alt pins → no banner; level-shifter direction wrong on probe.

### I2C
- **Open-drain** both lines; pull-up sized for bus capacitance: $R_p \le \frac{t_r}{0.8473 \cdot C_b}$. Typ 2.2k–10k for 100–400 kHz; lower for 1 MHz Fast-Mode+.
- Frame: START → 7-bit addr + R/W → ACK → data + ACK → STOP. RESTART (Sr) for combined R/W. 10-bit addr prefixed by `1111 0xxR/W`.
- **Clock stretching**: slave holds SCL low; master must tolerate.
- **Multi-master** arbitration on SDA — first to drive 0 while peer drives 1 wins.
- **Stuck-SDA-low recovery** (memorise): master toggles SCL up to **9 cycles** until SDA released, then issues STOP.
- Bug class: wrong pull value → slow rise → false 0 at high speed; two devices at same address.

### SPI
- 4-wire SCLK / MOSI / MISO / CSn, full-duplex, master clock.
- **CPOL/CPHA** ⇒ 4 modes (0–3). CPOL = idle clock polarity; CPHA = sample edge (0 = first, 1 = second). Mode-mismatch on the wire = all zeros or shifted-by-one bytes.
- CS setup before first SCLK + hold after last SCLK per slave datasheet.
- Multi-IO: single / dual / quad / octal — used by NOR flash for throughput.

### QSPI / OSPI NOR flash + XIP boot
- Standard ops: **READ 0x03, Fast Read, SFDP, WREN 0x06, Page Program 0x02, Sector Erase 0x20/0xD8, Read Status 0x05, Write Status 0x01, JEDEC ID 0x9F**.
- **SFDP** is the discoverable parameter table — read it, don't hardcode part IDs.
- **XIP**: flash mapped into address space, CPU fetches directly. Boot ROM XIPs from QSPI for BL1.
- **Continuous-read-mode bit trap**: skips the opcode on subsequent reads (faster) but is **stateful**; if a warm reset doesn't clear it, ROM's first opcode is interpreted as data → boot dies. Classic post-warm-reset hang.

### Bring-up dependency order (after a UART console exists)
```
UART → I2C (PMIC, sensors, EEPROM) → SPI/QSPI (flash, FRAM) → DDR train → high-speed (PCIe/USB/Eth) → app IPs (CAN/LIN, ADC, accel)
```

---

## Bug Anchor

- **Day 11 (NVMe):** `wait_csts_rdy` and AQA field-width bug.
- **Day 14 (PCU/ADR):** missing wait-for-ack — polling discipline in firmware.
- **Day 8 (DMA/doorbells):** descriptor-then-doorbell needs a barrier; `volatile` alone won't do it on ARM.

## Exit Check

- [ ] Stated `volatile` vs memory barrier in 30 seconds.
- [ ] Listed the three failure modes of struct-overlay-on-MMIO.
- [ ] Wrote `extract_field` / `insert_field` cold.
- [ ] Wrote `read_le32` and `read_be32` cold.
- [ ] Drew `0x12345678` in LE and BE memory at `0x100`.
- [ ] Stated why W1C registers must be write-only.
- [ ] Wrote `poll_ready` with timeout cold.
- [ ] Predicted the result of `reg_rmw` on a sample value without running it.

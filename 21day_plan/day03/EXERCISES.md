# Day 03 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day03_bits_mmio_volatile_polling.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: device init harness with poll + RMW

**Problem statement:** Build a fake-device init harness using a small `uint8_t mmio[256]` backing array. Implement typed accessors (`reg_read`, `reg_write`, `reg_rmw`), a bounded `poll_ready` that simulates HW asserting ready after N iterations, and a top-level main that resets the device, polls for ready, enables and starts it, then writes/reads `DATA`.

**You must implement:**
```c
static inline uint32_t reg_read(uint32_t off);
static inline void     reg_write(uint32_t off, uint32_t v);
static inline void     reg_rmw(uint32_t off, uint32_t mask, uint32_t v);
static bool            poll_ready(uint32_t off, uint32_t mask, uint32_t exp, uint32_t tries);
```

Register map: `CTRL=0x00, STATUS=0x04, DATA=0x08`. Bits: `CTRL_EN=BIT(0), CTRL_START=BIT(1), CTRL_RESET=BIT(31), STATUS_RDY=BIT(0)`.

**Driver / tests to write:**
- Reset (write `CTRL_RESET` then 0).
- `poll_ready(STATUS, STATUS_RDY, STATUS_RDY, 5)` — must return true; fail = "device never ready".
- RMW set `CTRL_EN`, then `CTRL_START` (preserve other bits).
- Write `0xDEADBEEF` to `DATA` and assert readback.

**Be ready to explain in interview:**
- 3 ways the compiler miscompiles missing-`volatile` poll loops at -O2.
- Why is RMW on a W1C status register catastrophic?
- Why must status registers be written, not read-modify-written?
- Why does `volatile` not imply a memory barrier on ARM?

## E2 — DS&A: Number of 1 Bits (LC 191)

**Problem statement:** Return the number of set bits in a 32-bit unsigned integer (Hamming weight / popcount).
**Signature:** `int hamming_weight(uint32_t n);`
**Constraints / edge cases:** 0, `UINT32_MAX`, single-bit values.
**Talking points:** Kernighan's `n &= n - 1` is O(set-bits); contrast with naive O(32) and the SWAR/`__builtin_popcount` paths. Where popcount shows up: error-counting, mask-density checks, perf counters.

## E3 — Architect Drills (no code, write your answers)

**D1.** Why is endianness a thing at all — why didn't industry standardise on one? What concrete things break if you forced one today?

**D2.** PCIe is LE on the wire even though PCI was endian-neutral. Who paid the cost and why?

**D3.** Why doesn't the compiler insert a memory barrier automatically around every `volatile` access on weak-ordered targets?

**D4.** Polling-loop timeouts are universal. Derive when polling beats interrupts for a given event arrival rate.

**D5.** W1C status bits exist because of a HW/SW race. Why isn't read-to-clear used everywhere instead?

**D6.** Struct-overlay over an MMIO block is forbidden but ergonomic. What would the language need to add to make it safe?

**D7.** Imagine MMIO is replaced by CXL.io with full request/response semantics. Which Day-3 disciplines stay load-bearing?

## E4 — Bug Anchor

**Symptoms:** Polling loop spins forever at -O2 but works at -O0. Walk the debug tree: what's missing, why does -O0 mask it, what compiler pass is responsible, how does `objdump -d` confirm it?

Also: a `*regs = defaults;` bulk struct write to an MMIO block silently clears every set W1C status bit. Explain the read-side-effect that triggers it.

## Reference solution

Once you've attempted, see `../day03_bits_mmio_volatile_polling.md` sections "Coding A" and "Coding B" for full solutions and discussion.

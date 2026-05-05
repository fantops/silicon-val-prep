# Day 01 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day01_c_address_fluency.md` ("Coding A" and "Coding B" sections).

Build:
```sh
cc -Wall -Wextra -Wpedantic -std=c11 -g -O0 exercises/<file>.c -o /tmp/<file> && /tmp/<file>
```

---

## E1 — Systems C: full register-poke harness

**Problem statement:** Build a simulated MMIO harness from scratch. Back a register with a single `volatile uint32_t` (stand-in for a real BAR offset). Implement bit/field macros, typed accessors, an RMW field-write, and a bounded poll loop. Drive the harness end-to-end: program a field, demonstrate the timeout path, demonstrate the success path.

**You must implement:**
```c
#define BIT(n)               /* ... */
#define MASK(msb, lsb)       /* ... */
#define FIELD_GET(r, msb, lsb)  /* ... */
#define FIELD_PREP(v, msb, lsb) /* ... */

static inline uint32_t reg_read32(volatile uint32_t *r);
static inline void     reg_write32(volatile uint32_t *r, uint32_t v);
static inline void     reg_field_write(volatile uint32_t *r, int msb, int lsb, uint32_t v);
static int             wait_for_bit_set(volatile uint32_t *r, uint32_t mask, int timeout_us);
```

**Driver / tests to write:**
- Start with `fake_reg = 0xDEADBEEF`, then `reg_field_write(&fake_reg, 15, 8, 0xAA)` and assert result `0xDEADAAEF`.
- Show timeout path: `fake_reg = 0`, call `wait_for_bit_set(&fake_reg, BIT(0), 5)` and assert `rc == -1`.
- Show success path: `fake_reg = BIT(0)`, call again and assert `rc == 0`.

**Be ready to explain in interview:**
- What does `volatile` actually prevent the compiler from doing? (3 things.)
- Why is `volatile` not a memory barrier?
- Why must `wait_for_bit_set` carry a timeout? Where does the timeout value come from for `CSTS.RDY`?
- Why is RMW on a W1C status register a bug?

## E2 — DS&A: Two Sum (LC 1)

**Problem statement:** Given an array `nums` and an integer `target`, return indices `i, j` such that `nums[i] + nums[j] == target`. Single-pass with a hash map.
**Signature:** `int *twoSum(int *nums, int n, int target, int *out_i, int *out_j);`
**Constraints / edge cases:** exactly one solution; same element cannot be used twice; negative numbers allowed.
**Talking points:** O(n) time, O(n) memory; firmware analogue is "have I seen this descriptor index before?" lookup tables.

## E3 — Architect Drills (no code, write your answers)

**D1.** Why bother with `reg_read32`/`reg_write32` accessors when a `volatile uint32_t *` cast would compile to the same instruction? Reason from `-O3`, barrier-injection points, traceability hooks, test doubles.

**D2.** Why do silicon designers expose W1C status bits at all instead of plain RW status fields? Reason about the lost-event race and the workload where W1S is preferred.

**D3.** Polling vs interrupts — under what residency/wake distribution does polling actually win? Derive the NVMe completion-queue crossover from first principles.

**D4.** `volatile` is necessary but not sufficient for MMIO. Why doesn't the C standard make `volatile` imply a full barrier?

**D5.** RMW on a control register isn't atomic across cores. Why doesn't hardware provide an `atomic_field_write` MMIO opcode? Compare to coherent CXL.cache.

**D6.** Derive the right `wait_for_bit_set` timeout for `CSTS.RDY` from first principles. Why is "1 second" wrong?

**D7.** The same `read/wait/write` accessor pattern shows up in NVMe, PCIe, clocking, bare-metal probes. What would force a future SoC to abandon it?

## E4 — Bug Anchor

**Scenario:** NVMe `CSTS.RDY` never asserts during bring-up. The host wrote `AQA` with the wrong field width. Walk the debug tree: how does the timeout in `wait_for_bit_set` surface this? What logs would you add? What's the symptom if the timeout were missing?

Cross-link: PCU/ADR opcode-ordering bug (Day 14) — `WPQ_FLUSH` issued before `IO_QUIESCE_ACK` is the same shape as forgetting to wait for a status bit.

## Reference solution

Once you've attempted, see `../day01_c_address_fluency.md` sections "Coding A" and "Coding B" for full solutions and discussion.

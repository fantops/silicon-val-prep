# Day 10 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day10_pcie_fundamentals_ltssm.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: ECAM walker + LTSSM tracker

**Problem statement:** Build two helpers.

(a) **ECAM address compute.** Given `(base, bus, dev, fn, reg)`, return the 64-bit ECAM address where `bus<<20 | dev<<15 | fn<<12 | (reg & 0xFFF)`. Also provide `bar_size_from_readback(uint32_t rb)`: write `0xFFFFFFFF` to the BAR, read back, mask the type bits, invert and add 1 to get the size.

(b) **LTSSM tracker.** Define the 10 LTSSM states (Detect, Polling, Config, L0, Recovery, L0s, L1, L1.1, L1.2, L2). Build an allowed-transition table. `go(link, to)` enforces legality, prints transitions, counts `recoveries` whenever entering Recovery.

**You must implement:**
```c
uint64_t ecam_addr(uint64_t base, uint8_t bus, uint8_t dev, uint8_t fn, uint16_t reg);
uint64_t bar_size_from_readback(uint32_t rb);

typedef enum { LTSSM_DETECT, LTSSM_POLLING, LTSSM_CONFIG, LTSSM_L0,
               LTSSM_RECOVERY, LTSSM_L0S, LTSSM_L1, LTSSM_L1_1,
               LTSSM_L1_2, LTSSM_L2, LTSSM_NUM } ltssm_t;
typedef struct { ltssm_t st; unsigned recoveries; } link_t;
bool go(link_t *l, ltssm_t to);
```

**Driver / tests to write:**
- ECAM `01:02.0` BAR0 against `base=0xE0000000` → print address.
- `bar_size_from_readback(0xFFF00004)` → 0x100000.
- Walk: Detect → Polling → Config → L0 → Recovery → L0 → L1 → L1.2 → Recovery → L0; print transitions and `recoveries` count.
- Try one illegal transition (e.g. Detect → L0) → expect ILLEGAL message.

**Be ready to explain in interview:**
- 3 PCIe layers (PHY/DLL/TL); what each isolates.
- Posted vs non-posted; why doorbell-as-MWr won.
- L1 vs L1.1 vs L1.2; PkgC6 dependency on L1.2.
- Credit-based flow control vs ACK/NAK pacing.

## E2 — DS&A: Climbing Stairs (LC 70)

**Problem statement:** You can climb 1 or 2 steps. How many distinct ways to reach step `n`? Fibonacci recurrence.
**Signature:** `long long climb_stairs(int n);`
**Constraints / edge cases:** `n=0` (1 way), `n=1`, `n=45` (overflow on `int`).
**Talking points:** O(n) DP, O(1) space; same recurrence shape as counting LTSSM substate paths or C-state transition paths (Day 13).

## E3 — Architect Drills (no code, write your answers)

**D1.** Why three layers (PHY/DLL/TL) and not two or four?
**D2.** Why did PCIe move from 8b/10b → 128b/130b at Gen3 instead of 256b/258b?
**D3.** Posted writes are fire-and-forget; reads block. Why doorbell-as-MWr won.
**D4.** LTSSM has 11 states. Why so many — could it be 4? Which are vestigial in 2025?
**D5.** PCIe trains at Gen1 first, then negotiates up. Why doesn't any spec start at the highest rate?
**D6.** Flow-control credits — why aren't they implicit?
**D7.** With CXL.cache, which LTSSM/TLP artefacts survive? What new failure mode appears?

## E4 — Bug Anchor

**Symptoms:** Link stuck in Polling. Walk the debug tree: refclk, receiver termination, lane polarity. Why is Polling almost always physical/electrical?

Other anchors: width downgrade (x16 trains as x4) → faulty lanes; frequent Recovery during traffic → SI / EQ Tx coefficients; link up but no traffic → flow-control credit return broken; `0xFFFFFFFF` Vendor ID after PM transition → link did not survive.

## Reference solution

Once you've attempted, see `../day10_pcie_fundamentals_ltssm.md` sections "Coding A" and "Coding B" for full solutions and discussion.

# Day 12 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day12_dvfs_pstates_pvt.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: DVFS table lookup with thermal cap + IPC counter

**Problem statement:** Build a DVFS controller. Given a 5-entry P-state table (P4..P0 from `(400 MHz, 750 mV)` to `(2400 MHz, 1150 mV)`), implement `select_opp(target_mhz, tj_c)` that picks the lowest OPP meeting the target frequency, **capped to ≤1200 MHz when `tj_c >= 90`** (thermal throttle). Then implement `apply_opp(cur, next)` that prints the correct sequencing — **V before F when ramping up, F before V when ramping down**.

Also a tiny `pmu_t { insns, cycles }` and `ipc()` helper.

**You must implement:**
```c
typedef struct { uint32_t freq_mhz; uint32_t volt_mv; } opp_t;
const opp_t *select_opp(uint32_t target_mhz, int tj_c);
void         apply_opp(const opp_t *cur, const opp_t *next);

typedef struct { uint64_t insns, cycles; } pmu_t;
double ipc(const pmu_t *p);
```

**Driver / tests to write:**
- Pick OPP for `(400, 50)`, then `(2000, 50)` — expect ramp-up sequence printed.
- Pick OPP for `(2400, 95)` — expect thermal cap to `1200 MHz`, ramp-down sequence printed.
- Compute IPC for `insns=15e9, cycles=12.34e9` and flag if `< 1.0`.

**Be ready to explain in interview:**
- Derive `P_dyn = (1/2) C V^2 F`; explain V² dominance.
- Why V-before-F up, F-before-V down? Setup-time slack argument.
- Why discrete P-states vs continuous? Validation-matrix argument.
- Snapdragon DVFS through RPMh + PMIC (per-cluster rail vs per-core).

## E2 — DS&A: Best Time to Buy and Sell Stock (LC 121)

**Problem statement:** Given `prices[n]`, return the max profit from one buy-then-sell. O(n) single pass tracking running min.
**Signature:** `int max_profit(const int *prices, int n);`
**Constraints / edge cases:** empty array, monotone-decreasing (profit 0), single element.
**Talking points:** Same shape as a power-counter rolling baseline (track min and best-delta in one pass).

## E3 — Architect Drills (no code, write your answers)

**D1.** Derive $P_{dyn} = \tfrac{1}{2}CV^2F$ from first principles. Where does it break (NTV, leakage-dominated)?
**D2.** With per-IP V/F rails, does V-before-F still apply? Construct a workload where it reverses.
**D3.** Why discrete P-states vs continuous? What does Apple's near-continuous OPP cost vs Intel's ladder?
**D4.** Speed binning sorts post-fab silicon. With perfect process control, what would the industry redesign?
**D5.** Leakage = 30-40% at 7nm. What's pushing toward 50%? Which architectural choices are responses?
**D6.** DynamIQ shares a cluster rail. Construct a workload where this loses *more* than it saves.
**D7.** Aging-related Vth shift over 2 years. What does a self-tuning DVFS table look like? What stops shipping today?

## E4 — Bug Anchor

**Symptoms:** Intermittent data corruption when OS jumps from P3 to P0. Walk the debug tree: V-F transition reversed (raised F before V) → setup-timing violation. Where in microcode/firmware does the sequence enforcement live?

Other anchors: turbo never engages on single-thread (DTS calibration); throttling under sustained load (PROCHOT, fan curve); cold-boot hang at SS corner (DVFS bin margin); per-core DVFS appears wasted on Snapdragon (cluster rail = max).

## Reference solution

Once you've attempted, see `../day12_dvfs_pstates_pvt.md` sections "Coding A" and "Coding B" for full solutions and discussion.

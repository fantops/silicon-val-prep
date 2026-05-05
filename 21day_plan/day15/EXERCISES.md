# Day 15 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day15_thermal_power_perf_counters.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: Tj model + PROCHOT + RAPL delta + IPC

**Problem statement:** Build three small models that compose into a perf/thermal validation harness.

(a) **Thermal step.** $T_j = T_a + P \cdot \theta_{JA}$ with first-order RC smoothing (tau=5s). On each `dt`, push `tj` toward steady state. Assert PROCHOT when `tj >= prochot`; clear when `tj < prochot - 5` (5°C hysteresis). When throttled, the step uses 75 W instead of 125 W (caller decides, model just observes).

(b) **RAPL delta → watts.** `rapl_watts(e0, e1, unit_uj, dt)` converts a counter delta + LSB-µJ + duration to watts.

(c) **Top-down dominant bucket.** Given fractions `frontend_bound, bad_spec, retiring, mem_bound, core_bound`, return the name of the largest.

**You must implement:**
```c
typedef struct { double tj, ta, theta, prochot; bool throttled; } thermal_t;
void thermal_step(thermal_t *m, double power, double dt);

double rapl_watts(uint64_t e0, uint64_t e1, uint64_t unit_uj, double dt);

typedef struct { double frontend_bound, bad_spec, retiring, mem_bound, core_bound; } tma_t;
const char *tma_dominant(const tma_t *t);
```

**Driver / tests to write:**
- 30-second sim with ambient 25, theta 0.3, prochot 95, power 125 W (throttled → 75) — print PROCHOT enter/clear events, final `tj` and `throttled`.
- `rapl_watts(0, 65000000, 1, 1.0)` → 65.0 W.
- TMA fractions `(0.15, 0.05, 0.25, 0.45, 0.10)` → "Backend Memory".

**Be ready to explain in interview:**
- $T_j = T_a + P\theta_{JA}$ as a lumped RC; when the model breaks (transient PROCHOT, hotspot).
- Why RAPL underestimates wall by 20-30%; what closes each gap.
- Top-down's 4 buckets; what they isolate.
- HW prefetcher hides missing software prefetch — why disabling HW PF is triage, not fix.

## E2 — DS&A: Sliding Window Maximum (LC 239) — counter-burst peak detector

**Problem statement:** Given `nums[n]` and window size `k`, return `max` for each window of length `k` as it slides. O(n) using a monotonic deque of indices.
**Signature:** `int *sliding_max(const int *nums, int n, int k, int *out_len);`
**Constraints / edge cases:** `k = 1`, `k = n`, all-equal, strictly decreasing.
**Talking points:** Monotonic deque keeps O(n); same shape as a perf-counter peak-detector over a rolling window.

## E3 — Architect Drills (no code, write your answers)

**D1.** Why is $T_j = T_a + P\theta_{JA}$ useful when the die is non-uniform?
**D2.** Why isn't per-IP thermal throttling more aggressive? What stops a future SoC from per-tile DVFS?
**D3.** RAPL underestimates wall by 20-30%. Construct the cause stack; which gap is fixable?
**D4.** Why exactly 4 top-down buckets (not 2 or 8)?
**D5.** HW prefetcher can hide a missing SW prefetch. Why is disabling HW PF triage, not a fix?
**D6.** AVX-512 license-level cuts F. Construct workload where AVX-512 loses to AVX-256.
**D7.** Five multi-IP interference classes (BW, LLC, NoC, power, IOTLB). Build the diagnostic decision tree.

## E4 — Bug Anchor

**Symptoms:** Throttle at Tj=85°C with spec Tjmax=100°C. Walk the debug tree: DTS calibration fuses missing → sensor reads 15°C high → external thermocouple confirms.

Other anchors: throttle without thermal cause (RAPL PL1/PL2 too aggressive in BIOS — `MSR_CORE_PERF_LIMIT_REASONS`); IPC unchanged but ref cycles ≫ core cycles (frequency dropped — throttle reason MSR); LLC thrash from GPU streaming DMA (CAT/CLOS); NPU EP fallback dominating inference (count fallbacks); RAPL=45W vs wall=65W is not a bug (VR loss + DRAM + platform + PSU).

## Reference solution

Once you've attempted, see `../day15_thermal_power_perf_counters.md` sections "Coding A" and "Coding B" for full solutions and discussion.

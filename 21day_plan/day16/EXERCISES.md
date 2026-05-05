# Day 16 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day16_pipeline_memory_consistency_simd.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: pipeline simulator + 2-bit branch predictor + license-level model

**Problem statement:** Build three small models that you can talk through in 60 seconds.

(a) **2-bit saturating branch predictor.** State `bp ∈ [0,3]`. Predict NT if `bp < 2`, T if `bp ≥ 2`. Update saturates: `bp++` on taken, `bp--` on not-taken. Score against a fixed branch stream and report accuracy.

(b) **License-level effective frequency.** Given fractions of time spent in scalar / AVX2 / AVX-512 plus a base GHz, return effective frequency where AVX2 = 0.90×, AVX-512 = 0.75× base.

(c) **5-stage pipeline diagram.** Print `IF/ID/EX/ME/WB` for 4 instructions with one load-use stall after the first instruction (a 1-cycle bubble before instruction 2).

**You must implement:**
```c
bool   predict(void);
void   update(bool taken);
double effective_freq_ghz(double scalar_pct, double avx2_pct, double avx512_pct, double base_ghz);
void   pipeline_demo(void);
```

**Driver / tests to write:**
- Stream `{1,1,1,0,1,1,0,1,1,1,0,0,1}` → print `BP accuracy hits/total = pct`.
- `(1.0, 0, 0, 4.0)` → 4.00 GHz; `(0.9, 0, 0.1, 4.0)` → ~3.90 (note net loss vs pure scalar).
- `pipeline_demo()` prints the 4-row diagram.

**Be ready to explain in interview:**
- OoO vs in-order; what extracts ILP.
- TSO vs weak; producer-consumer ring race; `dmb st` between data and flag.
- AVX-512 license-level pessimisation; when sprinkling AVX-512 is a net loss.
- HW prefetcher disable (`MSR 0x1A4`) as triage when silicon A passes / B regresses.

## E2 — DS&A: Counting Bits (LC 338)

**Problem statement:** Given `n`, return an array `bits[0..n]` where `bits[i]` is popcount(i). DP recurrence `bits[i] = bits[i >> 1] + (i & 1)`.
**Signature:** `int *count_bits(int n, int *out_len);`
**Constraints / edge cases:** `n = 0`, `n = 1`, large `n` (cap to a reasonable size in driver).
**Talking points:** O(n), beats per-element popcount; pattern shows up in mask-density precomputation for perf counters.

## E3 — Architect Drills (no code, write your answers)

**D1.** Why do deep OoO pipelines beat in-order at the same clock? When does in-order win?
**D2.** x86 needs a μop cache; ARM doesn't. Why? What does RISC-V compressed mode threaten to bring back?
**D3.** TSO vs weak. If ARM moved to TSO tomorrow, who wins/loses? Why didn't ARM v8 make the change?
**D4.** Branch mispredict cost ≈ depth × IPC. Where does a perfect predictor still lose to branchless?
**D5.** SIMD width doubling slowed at 512. Why? What comes after?
**D6.** Why is the producer-consumer ring race the canonical TSO-vs-weak bug? Construct the trace and the minimal fix.
**D7.** If a future ISA mandated seq_cst as default, who wins, what dies?

## E4 — Bug Anchor

**Symptoms:** Producer-consumer ring on ARM: consumer occasionally sees `ready=1` with stale payload. 24-hour x86 stress passes; ARM dev-kit fails in minutes. Walk the debug tree: missing `dmb st` between data store and flag store on producer; consumer needs `dmb ld` (or use `STLR/LDAR`).

Other anchors: branchy hot loop (`BR_MISP_RETIRED.ANY`); AVX-512 net loss (`CORE_POWER.LVL2_TURBO_LICENSE`); HW PF hides bug; I-cache thrash from huge code; Spectre-class side channels.

## Reference solution

Once you've attempted, see `../day16_pipeline_memory_consistency_simd.md` sections "Coding A" and "Coding B" for full solutions and discussion.

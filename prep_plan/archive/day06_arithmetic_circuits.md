# Day 6 — Arithmetic Circuits

## Concept

Arithmetic is the **heaviest combinational logic** on a chip — the adder is the critical path of nearly every datapath, the multiplier is the biggest block in a DSP/AI engine, and the number **format** (two's complement, fixed-point, floating-point) decides range, precision, and how comparisons behave. Master the **speed/area trade in adders** (ripple → carry-lookahead → prefix), the **structure of multipliers** (Booth + Wallace tree), and the **floating-point gotchas** — because this is the logic that sets $f_{max}$ ([Day 8](day08_timing_sta.md)) and fills the [Day 21](day21_pipeline_microarch.md) execute stage.

## Why this day in the flow

This sits between [Day 5](day05_boolean_combinational_logic.md) (combinational logic, where the half/full adder and the ripple-vs-CLA trade were introduced) and [Day 7](day07_sequential_logic.md) (sequential). It deepens the adder/ALU of Day 5 §7 into the full arithmetic toolkit, and it **feeds forward**: the adder/multiplier is usually the **longest combinational path**, so it's exactly what [Day 8 (Timing & STA)](day08_timing_sta.md) measures and what pipelining ([Day 21](day21_pipeline_microarch.md)) breaks up to raise frequency. Fixed/floating-point also underpins the AI-model KPI work — quantization is a fixed-point story.

**Why it's new (Intel IP-Val value):** datapath validation, ALU/FPU corner cases, and the speed/area reasoning behind "make the adder faster" are bread-and-butter for CPU/accelerator IP val.

---

## Deep Study Notes

### 1. Number formats (the foundation)

- **Unsigned:** `n` bits → `0 … 2ⁿ−1`. Carry-out of MSB = unsigned overflow.
- **Two's complement (signed):** negate = invert + 1; range `−2ⁿ⁻¹ … 2ⁿ⁻¹−1`; **signed overflow = carry-in ≠ carry-out of the MSB** (two same-sign operands → opposite-sign result). One representation of zero, and `A−B = A + ~B + 1` so one adder does both.
- **Fixed-point `Qm.n`:** an integer with an implied binary point; value = `raw / 2ⁿ`. `m` integer bits set range, `n` fraction bits set precision. Multiply two `Qm.n` → `Q(2m).(2n)`, so you **shift right by `n` and round** to stay in format (watch overflow + rounding bias). This is how quantized NN inference avoids floating-point.

### 2. Floating-point (IEEE-754) — and why it bites

```
 [ sign | exponent (biased) | mantissa (implicit leading 1) ]
 fp32:    1      8 (bias 127)     23      → ~7 decimal digits
 fp16:    1      5 (bias 15)      10      → ML inference
 bf16:    1      8 (bias 127)      7      → ML training (fp32 range, less precision)
```

- **Value** = `(−1)^sign × 1.mantissa × 2^(exp−bias)` for normals; **denormals** (exp=0) fill the gap near zero; **specials**: exp all-ones → `±Inf` (mantissa 0) or `NaN` (mantissa ≠ 0).
- **Gotchas to say out loud:** floating-point is **not associative** (`(a+b)+c ≠ a+(b+c)`); **never compare FP with `==`** (compare `|a−b| < ε`); rounding mode matters (round-to-nearest-even is default); `NaN != NaN`. These are recurring validation bugs (golden-vs-DUT mismatch is often a rounding/order difference, not a real bug — [Day 25](day25_testbench_assertions_debug.md)).
- **bf16 vs fp16:** bf16 keeps fp32's 8-bit exponent (same range, fewer mantissa bits) — why training prefers it; fp16 has more precision but less range (overflow risk).

### 3. Adders — the speed/area ladder

| Adder | Delay | Area | Idea |
|---|---|---|---|
| **Ripple-carry** | `∝ N` | smallest | carry walks every full-adder stage |
| **Carry-lookahead (CLA)** | `∝ log N` | more | precompute generate `gᵢ=AᵢBᵢ`, propagate `pᵢ=Aᵢ^Bᵢ`; `Cᵢ₊₁ = gᵢ + pᵢCᵢ` expanded so carries resolve in parallel |
| **Carry-select** | `∝ √N` | ~2× | compute both carry-0 and carry-1 results, mux when the real carry arrives |
| **Prefix (Kogge-Stone, Brent-Kung)** | `∝ log N`, low depth | large (KS) / smaller (BK) | parallel-prefix tree over the G/P operator; KS = fastest/most wires, BK = fewer wires/more depth |
| **Carry-save (CSA)** | O(1) per add | small | for **multi-operand** sums: keep carry separate, collapse once at the end |

The one-liner: **ripple is small but slow because the carry is serial; everything faster spends area to compute carries in parallel.** Carry-save is special — it defers the carry to sum *many* numbers cheaply (the heart of multipliers).

### 4. Multipliers — why multiply is expensive

`A × B` generates `N` **partial products** (shifted copies of `A`), which must be summed.

- **Shift-and-add** (sequential): `N` cycles, tiny area.
- **Array multiplier**: `N²` full adders in a grid — combinational but big and slow (long carry paths).
- **Booth encoding (radix-4)**: recodes the multiplier to **halve the number of partial products** (handles runs of 1s), and handles signed operands naturally.
- **Wallace / Dadda tree**: reduce the partial-product stack with **carry-save adders** (3:2 compressors) in `O(log N)` depth, then **one final fast adder** (CLA/prefix) collapses the last two rows.

The canonical fast multiplier = **Booth (fewer PPs) + Wallace tree of CSAs (fast reduction) + a prefix adder (final sum)**. Pipelining it (registers between reduction stages) trades latency for throughput and raises `fmax`.

### 5. Division (the expensive outlier) + the rest

- **Division** is iterative and slow: restoring / non-restoring / **SRT** (radix-4+) produce a few quotient bits per cycle. It's the longest-latency basic op — often not pipelined, sometimes done in software. Just know it's costly and why.
- **Comparator**: subtract and look at sign/zero, or a dedicated tree.
- **Saturating arithmetic**: clamp to max/min instead of wrapping (DSP, counters — [Day 4](day04_bit_manipulation.md)).
- **Rounding**: round-to-nearest-even avoids the bias of round-half-up — important for fixed-point datapaths and FP.

### 6. Where the critical path lives (the bridge to STA)

In a datapath, the **adder or multiplier is almost always the longest combinational path** → it sets the setup constraint and therefore `fmax` ([Day 8](day08_timing_sta.md)). Two levers: pick a **faster adder topology** (area for speed), or **pipeline** the arithmetic ([Day 21](day21_pipeline_microarch.md)) so each stage's `Tcomb` shrinks. "How do you make this datapath run faster?" = "faster adder or more pipeline stages" — say both and the trade.

---

## Bugs in this Topic

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Unflagged overflow** | wrong result, no flag | overflow not detected | check carry-in ≠ carry-out (signed) / carry-out (unsigned) |
| **Fixed-point overflow** | wraps after multiply | didn't shift/saturate the `Q(2m).(2n)` product | shift-right by `n` + saturate + round |
| **FP `==` compare** | spurious mismatch | exact equality on rounded values | `|a−b| < ε`; canonicalize NaN |
| **Non-associative FP** | golden≠DUT, both "right" | summation order differs | match reduction order or allow tolerance |
| **Booth recoding error** | wrong high bits | sign / radix-4 table slip | re-derive Booth table; test signed corners |
| **Rounding bias** | slow drift in accumulation | round-half-up bias | round-to-nearest-even |

---

## Interview Explanation (60–90 sec)

> "Arithmetic is the heaviest combinational logic, so it usually owns the critical path. For addition, ripple-carry is smallest but its delay grows linearly because the carry is serial; carry-lookahead precomputes generate and propagate so carries resolve in log-N time, and parallel-prefix adders like Kogge-Stone push that further by spending wires and area. Subtraction is free — add the two's complement. Multiplication is expensive because you sum N partial products: the fast recipe is Booth encoding to halve the partial products, a Wallace tree of carry-save adders to reduce them in log depth, and one prefix adder to collapse the final two rows; pipelining it trades latency for throughput. Division is the slow outlier — iterative SRT, a few quotient bits per cycle. On formats, I'm careful with floating-point: it's not associative, NaN isn't equal to itself, and you never compare with equals — you use a tolerance — which is why a golden-versus-DUT mismatch is often just a rounding or summation-order difference, not a real bug. And fixed-point is the quantization story: after multiplying two Q-m-dot-n numbers you shift right by n and round, watching for overflow. When asked to speed up a datapath, the answer is a faster adder topology or more pipeline stages, and I'd name the area and latency cost of each."

---

## Architect Drills (derive, no answer key)

1. Build the carry-lookahead `Cᵢ₊₁ = gᵢ + pᵢCᵢ` for a 4-bit adder; expand all four carries to show they compute in parallel.
2. Encode `−6 × 5` with radix-4 Booth; show the partial products and the sign handling.
3. Why does a Wallace tree reduce partial products in `O(log N)` while an array multiplier is `O(N)`? What does the final prefix adder do?
4. Multiply two `Q4.4` numbers; show the `Q8.8` product and how you round+saturate back to `Q4.4`.
5. Give three values where `(a+b)+c ≠ a+(b+c)` in fp32. How does this surface as a "bug" in golden-vs-DUT ([Day 25](day25_testbench_assertions_debug.md))?
6. A datapath fails timing on its multiplier at the slow corner. Two distinct fixes; name the cost of each (tie to [Day 8](day08_timing_sta.md)/[Day 21](day21_pipeline_microarch.md)).
7. Why bf16 for training but fp16 for some inference? Reason from the exponent/mantissa split.

---

## Coding A — `d06_arith`

Build an **arithmetic library in C** validated against native ops: `add_ripple`/`add_cla` (expose carry + signed-overflow flags), a **Booth radix-4** `mul_booth` and a shift-add `mul_shiftadd` (assert both == native over random signed inputs), a **fixed-point** `qmul(Qm.n)` with shift+round+saturate, and `fp_eq(a,b,eps)` plus a NaN/Inf classifier showing why `==` fails. Full spec in [coding/EXERCISES.md](../../coding/EXERCISES.md#day-6--arithmetic-circuits).

**Interview prompts:** Ripple vs CLA — where's the delay? Why is Booth+Wallace the fast multiplier? Why never `==` on floats?

---

## Bug Anchor / cross-links

- **Built from:** [Day 5](day05_boolean_combinational_logic.md) (half/full adder, ripple-vs-CLA, two's complement, hazards), [Day 4](day04_bit_manipulation.md) (saturating arithmetic, widen-before-shift).
- **Feeds:** [Day 8](day08_timing_sta.md) (the adder/multiplier is the critical path STA measures), [Day 21](day21_pipeline_microarch.md) (pipelined execute), [Day 15](day15_memory_systems_ecc.md) (ECC is XOR arithmetic; CRC), [Day 25](day25_testbench_assertions_debug.md) (FP rounding = golden-vs-DUT tolerance).
- **AI tie-in:** fixed-point quantization + bf16/fp16 underpin the Microsoft AI-model KPI work ([Day 40](day40_intel_role_narrative.md)).

---

## Exit Check

- [ ] Explain two's-complement overflow as carry-in ≠ carry-out of the MSB.
- [ ] Rank ripple / CLA / carry-select / prefix by delay and area; state the trade.
- [ ] Describe the fast multiplier: Booth → Wallace CSA tree → final prefix adder.
- [ ] State why division is the slow outlier.
- [ ] List three FP gotchas (non-associative, NaN≠NaN, never `==`) and the bf16/fp16 trade.
- [ ] Multiply and re-format two fixed-point `Qm.n` values with round+saturate.
- [ ] Give two ways to speed a datapath and the cost of each.

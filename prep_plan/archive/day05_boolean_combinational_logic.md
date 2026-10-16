# Day 5 — Boolean Algebra + Combinational Logic

## Concept

A **combinational** circuit's output is a pure function of its inputs *right now* — no clock, no memory. Master the three layers: **Boolean algebra** (the math), **minimization** (K-maps / theorems → the smallest gate network), and the **standard datapath blocks** (mux, decoder, encoder, priority encoder, adder, comparator, ALU). The goal is to *derive any block from its truth table on a whiteboard* — not recall a gate picture.

## Why this day in the flow

This opens **Phase 1 — Digital Design**, the RTL floor the rest of the plan is built on. Combinational logic shows up *everywhere downstream* — you'll keep meeting it: the BAR address decode in [Day 34](day34_pcie_enumeration_config_msi.md) is a decoder; the field-extract masks in [Day 3](day03_bits_mmio_volatile_polling.md) are AND/shift networks; the ALU in the [Day 21](day21_pipeline_microarch.md) pipeline is a combinational adder/logic slice; the **next-state logic** of every FSM ([Day 9](day09_fsm_sequence_detectors.md)) is a combinational block feeding a register. Building this floor first means the rest of Phase 1 (sequential logic → timing → FSM → Verilog) and everything above it has solid ground.

**Broadcom R2 gap this closes:** "digital logic — combinational vs sequential," "build XOR from NANDs," "reason from a truth table." Self-rated weak; this is the fix.

---

## Deep Study Notes

### 1. The dividing line — combinational vs sequential

| | Combinational | Sequential |
|---|---|---|
| Output depends on | current inputs **only** | current inputs **+ stored state** |
| Memory | none | yes (latches / flip-flops) |
| Clock | no | yes (synchronous) |
| Examples | adder, mux, decoder, ALU, comparator | counter, FSM, register, FIFO |
| Failure mode | glitch / hazard, inferred latch | metastability, setup/hold, race |

**The one test:** *"Does the answer depend on what came before?"* If yes → you need state → it's sequential (Day 7–9). If the output is fully determined by the inputs on the wires this instant → combinational. That single question is the first thing to say out loud on any "design X" question.

### 2. Boolean algebra — the laws you must wield, not recite

```
Identity     A·1 = A        A+0 = A
Null         A·0 = 0        A+1 = 1
Idempotent   A·A = A        A+A = A
Complement   A·A' = 0       A+A' = 1
Involution   (A')' = A
Commutative  A·B = B·A      A+B = B+A
Associative  A(BC)=(AB)C    A+(B+C)=(A+B)+C
Distributive A(B+C)=AB+AC   A+BC=(A+B)(A+C)
Absorption   A+AB = A       A(A+B) = A
Consensus    AB + A'C + BC = AB + A'C   (BC is redundant)
```

**De Morgan (the bubble-pushing rule — the most-used one):**

$$\overline{A \cdot B} = \overline{A} + \overline{B} \qquad \overline{A + B} = \overline{A} \cdot \overline{B}$$

Pushing a bubble through a gate flips AND↔OR and inverts every input/output. This is *why* NAND and NOR are universal, and it's how you turn an AND-OR network into an all-NAND network for synthesis.

### 3. Canonical forms — SOP, POS, minterms, maxterms

Every truth table has two exact algebraic forms:

- **SOP (sum of products)** — OR of the **minterms** (rows where output = 1). `F = Σm(1,3,5)`.
- **POS (product of sums)** — AND of the **maxterms** (rows where output = 0). `F = ΠM(0,2,4,6,7)`.

```
A B | F     minterm
0 0 | 0
0 1 | 1     m1 = A'B
1 0 | 0
1 1 | 1     m3 = AB
F = A'B + AB = B   (after minimization)
```

Pick SOP when 1s are sparse, POS when 0s are sparse. **Minimization** is then "merge adjacent terms that differ in one variable" — which is exactly what a K-map automates.

### 4. K-map minimization (≤ 4 vars by hand)

A K-map is a truth table reordered in **Gray-code** row/column order so that **physically adjacent cells differ in exactly one variable** — so any group of adjacent 1s collapses that variable away.

```
4-var map (AB rows in Gray order, CD cols in Gray order):
          CD=00  01  11  10
   AB=00 |  .    1   1   .
   AB=01 |  .    1   1   .
   AB=11 |  .    .   .   .
   AB=10 |  .    .   .   .
   → the 2x2 block of 1s = A'C  (B and D vanish; only A'=0 and C=1 survive)
```

**Rules:** group **powers of two** (1,2,4,8…), make groups **as large as possible** (fewer literals), groups may **wrap** edges and **overlap**, cover every 1 at least once. **Don't-cares (X)** join whichever group makes a bigger block — free minimization. Beyond 4 vars, hand K-maps break down → **Quine–McCluskey** (tabular, algorithmic) or just let the synthesizer do it; know the *name* and that it's what `synthesize` runs.

### 5. Universal gates + building XOR (the Broadcom ask)

NAND alone (or NOR alone) can build every function:

```
NOT   = NAND(A,A)
AND   = NOT(NAND(A,B))                    = NAND then invert
OR    = NAND(A',B')                       = De Morgan
XOR   = NAND(NAND(A,NAND(A,B)),           4 NANDs
             NAND(B,NAND(A,B)))
```

`XOR = A'B + AB'` = "inputs differ." Say it as: *"XOR is a 1-bit difference detector and a controllable inverter — `A^1 = A'`, `A^0 = A`. From NANDs it's four gates; the trick is the shared inner `NAND(A,B)` feeding both halves."*

### 6. The standard combinational blocks (one-liner + where it appears)

| Block | Function | Shows up as |
|---|---|---|
| **MUX** (n:1) | select 1 of n inputs via `log2(n)` select lines | bus steering, register read port, FSM output select |
| **Decoder** (n→2ⁿ) | one-hot the address | **register/address decode** ([Day 3](day03_bits_mmio_volatile_polling.md) MMIO, [Day 15](day15_memory_systems_ecc.md) row/col select) |
| **Encoder** (2ⁿ→n) | binary-encode a one-hot | collapse one-hot back to index |
| **Priority encoder** | index of the **highest-priority** set input | **interrupt arbitration**, find-first-set ([Day 4](day04_bit_manipulation.md) `ffs`) |
| **Demux** (1→n) | route one input to one of n outputs | write-enable steering |
| **Comparator** | `<`, `=`, `>` | bounds checks, FIFO full/empty ([Day 14](day14_async_fifo_cdc_verification.md)) |
| **Adder / ALU** | arithmetic + logic | the [Day 21](day21_pipeline_microarch.md) execute stage |

**MUX is secretly universal** (Shannon expansion): any function `F(A,B,…)` = `A'·F(0,…) + A·F(1,…)`, i.e. a 2:1 mux selected by `A`. That's why LUTs in FPGAs are just muxes. Good thing to say.

**Priority encoder vs plain encoder** is a classic trap: a plain encoder assumes **exactly one** input is hot (multiple hot → garbage); a **priority** encoder is defined for *any* input pattern and returns the highest index set. Interrupt controllers need priority encoders.

### 7. Arithmetic — adders and the carry problem

**Half adder:** `sum = A^B`, `carry = A·B`.
**Full adder:** `sum = A^B^Cin`, `Cout = A·B + Cin·(A^B)`.

```
Ripple-carry: chain N full adders, carry flows A→B→C...
  + simple, small
  − slow: carry ripples through all N stages → delay ∝ N  (the critical path)

Carry-lookahead (CLA): precompute Generate gᵢ=AᵢBᵢ and Propagate pᵢ=Aᵢ^Bᵢ,
  then Cᵢ₊₁ = gᵢ + pᵢ·Cᵢ expanded so all carries compute in parallel.
  + fast: delay ∝ log N
  − more gates/area
```

The **ripple vs CLA trade** (speed vs area, the classic "how do you make an adder faster") is a favourite. Subtraction = add the **two's complement**: `A − B = A + (~B) + 1` (set carry-in = 1, invert B). One adder does both.

### 8. Two's complement + overflow (ties to number systems)

- Negate: **invert all bits, add 1**. Range of `n` bits: $-2^{n-1} \dots 2^{n-1}-1$.
- **Signed overflow** = two same-sign operands produce the opposite sign, equivalently **carry-in ≠ carry-out of the MSB**. `120 + 50` in int8 → `170 > 127` → wraps negative.
- **Unsigned overflow** = carry-out of the MSB = 1.
- This is the exact bug class behind [Day 3](day03_bits_mmio_volatile_polling.md)'s "widen before you shift" and the two-32-bit-regs-into-64 trap from the QC post-Si round — promotion/width before arithmetic.

### 9. Hazards and glitches — and why the FSM lesson starts here

A **hazard** is a momentary wrong output during input transitions, caused by **unequal path delays** reconverging:

- **Static-1 hazard:** output should stay 1 but dips to 0 (fix: add the redundant consensus term that K-maps drop).
- **Static-0 hazard:** should stay 0, blips 1.
- **Dynamic hazard:** output meant to change once, bounces 0→1→0→1.

In **synchronous** design glitches are harmless — they settle before the clock edge samples them. They become **fatal** when a combinational signal feeds **(a) an asynchronous reset, (b) a clock, or (c) a clock-domain crossing** — there the glitch can be latched as a real edge. That is the seam where today's material hands off to [Day 12–13](day12_metastability_synchronizers.md) (CDC/metastability). **Never gate a clock or build a reset out of glitchy combinational logic** — use a clean ICG cell ([Day 28](day28_clock_power_gating_upf.md)) or a synchronizer.

### 10. The RTL pitfall that bridges to sequential logic

In RTL, an **incomplete combinational specification infers a latch** — the synthesizer adds memory you didn't ask for because some input combination doesn't assign the output:

```systemverilog
// BUG: no default / missing else → 'y' must "remember" → inferred latch
always @(*)
  case (sel)
    2'b00: y = a;
    2'b01: y = b;
    // 2'b10, 2'b11 unassigned → latch on y
  endcase

// FIX: full case — every path assigns y
always @(*)
  case (sel)
    2'b00:   y = a;
    2'b01:   y = b;
    default: y = 1'b0;   // or assign y a default before the case
  endcase
```

This is the single most common combinational RTL bug, and it's *why* the FSM recipe ([Day 9](day09_fsm_sequence_detectors.md)) insists "make every state total — one branch per input." Latches inferred on a datapath then become timing and CDC nightmares downstream.

---

## Bugs in this Topic

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Inferred latch** | sim/synth mismatch, timing fails, X-prop | incomplete `case`/`if` in `always @(*)` | full case + default / pre-assign |
| **Priority vs plain encoder** | wrong index when >1 input hot | used a plain encoder where multiple inputs can be set | priority encoder |
| **Glitch on async path** | spurious reset / double-clock | combinational glitch fed a clock or async reset | register it / ICG / synchronizer |
| **Static-1 hazard** | momentary 0 on a steady-1 net | K-map dropped the consensus term | add redundant cover term |
| **Signed/unsigned compare** | off-by-huge comparator result | mixed signedness in `<` | match widths + signedness explicitly |
| **CLA carry expansion bug** | wrong high bits only | dropped a `pᵢ·Cᵢ` term | re-derive G/P recurrence |

---

## Interview Explanation (60–90 sec)

> "Combinational logic is any circuit whose output is a pure function of its current inputs — no clock, no memory. The test I apply is: *does the answer depend on history?* If not, it's combinational and I can write it as a truth table, then minimize. For minimization I use Boolean laws — absorption, consensus, De Morgan — or a K-map for up to four variables, which is just the truth table in Gray-code order so adjacent 1s merge and cancel a variable. The standard blocks all fall out of truth tables: a decoder one-hots an address, a priority encoder returns the highest set index for interrupts, a mux steers a bus — and a mux is actually universal by Shannon expansion. For arithmetic, ripple-carry is small but slow because the carry walks every stage; carry-lookahead precomputes generate/propagate so carries resolve in parallel — speed for area. The trap I watch is glitches: harmless inside synchronous logic because they settle before the edge, but never let a glitchy combinational net drive a clock, a reset, or a clock-domain crossing — that's where a glitch becomes a real, latched edge."

---

## Architect Drills (derive, no answer key)

1. Minimize `F(A,B,C,D) = Σm(0,1,2,3,8,9,10,11)` with a K-map. Which variables vanish, and why physically?
2. Build a 4:1 mux from 2:1 muxes. Now build *any* 2-input function from a single 4:1 mux — what do you wire to the data inputs?
3. You have a plain 4→2 encoder but two inputs can be hot at once. Show the failure, then convert it to a priority encoder. Where in a real SoC does this exact bug bite?
4. Derive `Cout` of a full adder two ways: from the truth table, and as `G + P·Cin`. Show they're equal.
5. A combinational net feeds an asynchronous reset and you see random spurious resets. Walk the hazard. Three fixes, ranked.
6. Prove NOR is universal by building NOT, AND, OR from it. How many NORs for XOR?
7. Why does an incomplete `case` infer a latch but a complete one doesn't? Tie it to the "every state total" FSM rule.

---

## Coding A — `d05_comb_logic`

Build a small **combinational-block library in C** that models the gates/blocks from truth tables: `priority_encode()` (find-first-set, highest index), a 4-bit ripple-carry `add4()` exposing carry-out, a 2:1/4:1 `mux()`, a 4-bit `alu()` (ADD/SUB/AND/OR/XOR/SLT) with carry + overflow flags, and `parity()`/`popcount()`. Driver proves each against a brute-force truth table. Full spec + signatures in [coding/EXERCISES.md](../../coding/EXERCISES.md#day-5--boolean--combinational-logic).

**Interview prompts:** Why is a mux universal? Show signed overflow as carry-in ≠ carry-out of the MSB. Where does priority encoding vs plain encoding change the answer?

---

## Bug Anchor / cross-links

- **Feeds:** [Day 7](day07_sequential_logic.md) (register the combinational next-state), [Day 9](day09_fsm_sequence_detectors.md) (next-state logic *is* a combinational block), [Day 15](day15_memory_systems_ecc.md) (decoders for row/column select, XOR trees for ECC).
- **Reuses:** [Day 3](day03_bits_mmio_volatile_polling.md) (masks/shifts = AND/OR networks; widen-before-shift = overflow), [Day 21](day21_pipeline_microarch.md) (ALU in execute), [Day 34](day34_pcie_enumeration_config_msi.md) (BAR decode), [Day 4](day04_bit_manipulation.md) (find-first-set = priority encoder in software).
- **Hands off to CDC:** §9 glitch-on-async-path → [Day 12](day12_metastability_synchronizers.md)/[Day 13](day13_cdc_techniques.md).

---

## Exit Check

- [ ] State the combinational-vs-sequential test in one sentence and classify 5 blocks instantly.
- [ ] Minimize a 4-variable function with a K-map, using don't-cares, and name Quine–McCluskey as the >4-var tool.
- [ ] Build NOT/AND/OR/XOR from NAND **and** from NOR.
- [ ] Draw a full adder; explain ripple vs carry-lookahead as a speed/area trade.
- [ ] Explain signed overflow as carry-in ≠ carry-out of the MSB.
- [ ] Explain why an incomplete `case` infers a latch and how that connects to the FSM "every branch total" rule.
- [ ] Name the three places a combinational glitch becomes fatal (clock, reset, CDC).

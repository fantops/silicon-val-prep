# Day 16 — CPU Pipeline, OoO, Memory Consistency, SIMD

## Concept

Modern cores are **deeply pipelined out-of-order machines**: front-end (fetch / branch-predict / decode / μop cache) feeds a backend (rename → reservation stations → functional units → ROB → retire). Three hazards block the simple pipeline (structural / data / control). The **memory consistency model** decides what reorderings the hardware is allowed to expose to other cores: x86 is **TSO** (mostly safe), ARM/RISC-V/POWER are **weak** (need explicit barriers). **SIMD** widens each instruction (SSE/AVX/AVX-512 on x86, NEON/SVE on ARM); on Intel the wider you go, the more current you draw, and the **license level** drops core frequency to keep V margin.

## Why this day in the flow

Days 4-15 covered the platform — caches, MMU, IOMMU, DMA, PCIe, NVMe, power, thermal, perf counters. Day 16 is the **CPU itself** as a microarchitecture, the layer where IPC actually happens. You need this to talk credibly about pipeline-stall counters from Day 15, and to write the concurrency primitives on Day 17 with the right memory orders.

In the Layer map this is **Layer 6 (CPU microarch)**.

---

## Deep Study Notes

### 1. The classic 5-stage pipeline

```
IF  ─►  ID  ─►  EX  ─►  MEM  ─►  WB
fetch    decode  execute  memory   write-back
```

| Stage | What happens | HW units |
|---|---|---|
| **IF** | PC → I-cache fetch; branch-predict next PC | I$, BPU |
| **ID** | decode opcode; read source regs from regfile | decoder, regfile read ports |
| **EX** | ALU op, branch decide, AGU computes load/store address | ALU, AGU, branch unit |
| **MEM** | access D-cache for load/store (only if needed) | D$, LSQ |
| **WB** | write result back to architectural register | regfile write port |

Three hazard classes:

| Hazard | Cause | Mitigation |
|---|---|---|
| **Structural** | two stages need same resource (e.g., single memory port) | split I-cache / D-cache (Harvard) |
| **Data** (RAW / WAR / WAW) | instruction needs result not yet ready | forwarding/bypass, stalls, **register renaming** kills WAR/WAW |
| **Control** | branch target unknown until EX | branch prediction + speculative fetch |

Modern Intel/ARM cores are **14-20 stages deep**. Deeper = higher F (per-stage delay smaller) but **bigger mispredict penalty** because the whole pipeline must be flushed.

### 1b. Walking a single instruction through the stages

Interviewers love this concrete drill. Three flavours of `MOV` cover the picture:

**`MOV A, B` — register-to-register copy (no ALU, no MEM):**

| Stage | What happens |
|---|---|
| IF | fetch `MOV A,B` opcode bytes from I$ |
| ID | decode → "copy reg B into reg A"; read B from regfile |
| EX | **no ALU op needed** — value passes through (or `0 + B` in some impls) |
| MEM | **skipped** — no memory access |
| WB | write B's value into reg A |

**Modern OoO twist — *move elimination*:** the rename stage just remaps A's logical-reg pointer to B's physical reg in the RAT. **Zero EX latency, zero ALU usage, zero issue slot.** Intel has done this since Ivy Bridge; ARM since Cortex-A57. This is why "how many cycles is `mov rax, rbx`?" can legitimately answer **zero**.

**`MOV A, [B]` — load from memory (uses MEM stage):**

| Stage | What happens |
|---|---|
| IF | fetch |
| ID | decode; read B (the address) |
| EX | AGU computes effective address = B (no ALU on data) |
| MEM | **D-cache read at addr;** L1 miss → L2 → L3 → DRAM |
| WB | result written into reg A |

Latency depends on where it hits: L1 ≈ 4 cyc, L2 ≈ 12, L3 ≈ 40, DRAM ≈ 200-300. ROB fills while waiting on DRAM = backend stall.

**`MOV [B], A` — store to memory (uses MEM stage, no WB):**

| Stage | What happens |
|---|---|
| IF | fetch |
| ID | decode; read A (data) and B (address) |
| EX | AGU computes addr = B |
| MEM | enqueue into store buffer; eventual D-cache write |
| WB | nothing to write back |

The store buffer decouples store latency from issue — TSO allows store→later-load reordering precisely because the store sits in the buffer while the load fires.

**Hazards on `MOV A, B` specifically:**

- RAW: a later instruction reading A — handled by EX→EX forwarding or rename.
- WAR / WAW on A: killed by register renaming.
- Structural: never — uses no specialized FU.
- Control: never — not a branch.

For the load form `MOV A, [B]` the extra hazards are **load-use hazard** (next instruction needs A immediately → 1-cycle stall even with forwarding) and **load latency** (TLB miss + cache miss can stretch the in-flight window for 100+ cycles).

### 2. The front end

```
[I-Cache] → [Pre-decode] → [Branch Predictor] → [Decoders] → [μop Cache] → [Allocate]
```

- **Branch predictor:** BTB (target), direction predictor (TAGE / perceptron), RAS (return-address stack). State of the art ~97-99% on typical code.
- **Mispredict penalty:** ~pipeline depth × IPC ≈ 15-20 cycles of wasted work. Branchy unpredictable code (hash-table probes, interpreter dispatch) lives in this penalty.
- **μop cache (DSB on Intel):** caches post-decode μops; hit → skip decoders → 6+ μops/cycle vs ~4 from legacy decode.
- **x86 specific:** variable-length 1-15-byte instructions force serial pre-decode; complex instructions hit MSROM and emit multi-μop sequences. ARM's fixed 4-byte format trivially decodes in parallel — no μop cache needed.

### 3. The OoO backend

```
Allocate → Rename (RAT) → Reservation Station → Execute (FUs) → ROB → Retire
```

- **Register renaming:** maps architectural names to a much larger physical register file (e.g., 280+ entries on Golden Cove). Kills WAR and WAW; only RAW remains.
- **ROB (ReOrder Buffer):** holds in-flight μops in program order; retires in order even though execution is OoO. ROB size caps the instruction window (e.g., 512 on Golden Cove).
- **Reservation stations / scheduler:** wakes μops when operands are ready, dispatches to a free functional unit.
- **Memory ordering buffer (LSQ — load + store queues):** enforces the consistency model.

### 4. Memory consistency models

| Model | Examples | What can reorder? |
|---|---|---|
| **SC** (sequential consistency) | theoretical | nothing |
| **TSO** (total store order) | x86, SPARC | stores can be reordered after later loads (store buffer); store→store and load→load preserved |
| **Weak / RC** | ARM, RISC-V, POWER | almost any pair can reorder |

Implications:

- **x86:** `volatile` is usually enough for MMIO; `mfence` only needed for store-then-load across cores.
- **ARM:** explicit `dmb`, `dsb`, `isb`. Producer-consumer flag pattern needs `dmb st` on the producer (data store, then ready store) and `dmb ld` on the consumer (read flag, then read data). Day 8 doorbell story is exactly this.

**The classic memory-model bug:** on ARM, producer writes `data; ready=1`; consumer polls `ready==1; reads data`. Consumer can see `ready=1` with **stale data** because the two stores can become globally visible out of order. Fix: barriers on both sides (or `STLR`/`LDAR`).

### 5. Prefetchers

Hardware prefetchers fetch lines before demand:

| Type | What it spots |
|---|---|
| Next-line / streamer | sequential |
| Stride | fixed delta (e.g., `array[i].field`) |
| Spatial / region | neighbours |
| NLP | L1 → L2 hint |

Validation lever: HW prefetchers can **hide** a missing software prefetch (silicon A passes; silicon B with different heuristic regresses). Always re-run with HW prefetch off (Intel MSR `0x1A4`) as a triage step. Aggressive prefetch can cause BW contention in multi-IP systems (Day 15).

### 6. SIMD and Intel license levels

Vector widths:

| ISA | Widths |
|---|---|
| x86 | SSE 128 → AVX/AVX2 256 → AVX-512 512 |
| ARM | NEON 128 → SVE/SVE2 (variable 128-2048) |
| RISC-V | V extension (variable) |

**Intel license levels:** AVX-512 instructions draw enough current that the core preemptively drops F to keep Vmin margin.

| License | Window | F offset (SKU-specific) |
|---|---|---|
| 0 | scalar | 0% |
| 1 | AVX2-heavy | ~10% |
| 2 | AVX-512-heavy | ~25% |

A few AVX-512 instructions in mostly-scalar code drag the whole core to license 2 for microseconds → scalar code surrounding them runs slower → net throughput **lower** than just using AVX2. Compiler flag `-mprefer-vector-width=256` exists for this reason.

### 6b. Floating-point arithmetic — IEEE 754 and what goes wrong

FP isn't "real numbers in a computer." It's a **finite, log-spaced grid of representable values** with hard rules about rounding and exceptions. Most "weird FP results" are not bugs in the FPU — they're the spec working as designed and software not respecting it.

**IEEE 754 layout (the only formats you need cold):**

| Format | Bits | Sign | Exponent | Mantissa | Approx range | Approx precision |
|---|---|---|---|---|---|---|
| **FP32** (single) | 32 | 1 | 8 (bias 127) | 23 + 1 implicit | ±10^±38 | ~7 decimal digits |
| **FP64** (double) | 64 | 1 | 11 (bias 1023) | 52 + 1 implicit | ±10^±308 | ~15-16 decimal digits |
| **FP16** (half) | 16 | 1 | 5 | 10 | ±65504 | ~3-4 digits |
| **BF16** (bfloat16) | 16 | 1 | **8** (FP32 range) | 7 | ±10^±38 | ~2-3 digits |
| **FP8** (E4M3 / E5M2) | 8 | 1 | 4 or 5 | 3 or 2 | small | ~1-2 digits |

Value = `(-1)^sign × 1.mantissa × 2^(exp - bias)` (normal numbers).

**Special values:** `+0`, `-0`, `±Inf` (exp all-ones, mantissa zero), `NaN` (exp all-ones, mantissa non-zero — quiet vs signalling), **denormals/subnormals** (exp zero, gradual underflow).

**Rounding modes (IEEE 754):** round-to-nearest-even (default), round-toward-zero, round-up, round-down. The default is the only one most software ever sees, but DSP / NPU pipelines may pick differently per op.

**The five FP exceptions:** invalid (0/0, sqrt(-1)), divide-by-zero, overflow, underflow, inexact. By default they set sticky flags; trap-on is opt-in.

**What goes wrong (the actual interview/bug surface):**

1. **Non-associativity.** `(a + b) + c ≠ a + (b + c)` in general. Re-ordering by the compiler (`-ffast-math`), by SIMD reduction trees, or by parallel-sum across cores changes the bit-exact result. Classic bug: "my unit test passes single-threaded, fails parallel" — it's not a race, it's reduction order.
2. **Catastrophic cancellation.** Subtracting two near-equal numbers cancels the high bits and amplifies low-bit noise. Numerically stable algorithms (Kahan summation, log-sum-exp, two-pass variance) exist exactly for this.
3. **Denormals tank performance.** Some hardware traps to microcode on denormal inputs/outputs → 100× slowdown. Audio/DSP code sets **FTZ** (flush-to-zero) and **DAZ** (denormals-are-zero) bits in MXCSR / FPCR. NPU / DSP pipelines often disable denormals entirely.
4. **Overflow / underflow silently produces ±Inf or 0.** Then a downstream `Inf - Inf = NaN`, and now your whole tensor is NaN. Tracking down "first NaN" is the #1 ML-stack debug skill (`tf.debugging.check_numerics`, ORT `--enable_nan_check`).
5. **NaN propagation + ordering.** Any arithmetic with a NaN yields NaN. Any comparison with NaN is **false** (including `NaN == NaN`) — use `isnan()`. Sort comparators that don't handle NaN go infinite.
6. **`-0.0` vs `+0.0`.** Equal under `==` but distinguishable: `1/+0 = +Inf`, `1/-0 = -Inf`. Bites you in numerical kernels.
7. **FP32 → FP16 / BF16 / FP8 quantisation (NPU world).** Different formats trade range vs precision: BF16 keeps FP32's range (great for training gradients), FP16 has more mantissa bits (better precision in a narrow range). Wrong choice → silent overflow to Inf in attention scores, or precision collapse in small activations.
8. **FMA changes results.** Fused multiply-add `a*b + c` rounds **once**, not twice → different last-bit result vs separate mul + add. Compilers contract MUL+ADD into FMA opportunistically; turn off with `-ffp-contract=off` if bit-exact required.
9. **x87 80-bit extended precision.** Legacy x87 stack uses 80-bit internally; spilling to memory rounds to 64-bit. Same expression yields different results depending on register pressure. Modern code uses SSE2/AVX scalar FP to avoid this entirely.
10. **Cross-vendor non-bit-exactness.** Intel, AMD, ARM, NVIDIA, and a Hexagon NPU may all be IEEE-754-compliant and still produce different last-bit results for the same model — different op orderings, different transcendental approximations (sin/cos/exp are not specified to ULP), different FMA usage. Your post-Si NPU validation lives here: pick a tolerance (relative + absolute), don't bit-compare.

**Validation reflexes:**
- For numeric regressions, log: were FTZ/DAZ set? Was FMA contraction on? Was reduction order changed by a thread-count delta? Did a denormal slip in?
- For NPU silent-fallback / accuracy bugs: compare INT8 quantised result vs FP32 reference under a defined tolerance (e.g. cosine similarity > 0.999, or per-element relative err < 1e-2). Bit-exact is the wrong goal.
- For "first NaN" hunts: enable per-op NaN check, bisect to the layer, then look at the inputs for overflow-to-Inf or underflow-to-zero one stage upstream.

**One-liner for the interview:** *"Floating point isn't real arithmetic — it's a finite grid with rounding rules. Non-associativity, denormal slowdowns, NaN propagation, and reduction-order changes are the four bug shapes you actually hit in validation; bit-exact across vendors is the wrong goal, defined tolerance is the right one."*

### 7. ISA at interview depth

You don't need to write assembly. You need to articulate trade-offs:

| ISA | Property | Trade-off |
|---|---|---|
| **x86 (CISC)** | variable length, complex addressing, μop translation | strong single-thread, mature; needs μop cache to pay for ISA overhead |
| **ARM (RISC)** | fixed length, load/store, weak memory | power-efficient; mobile/Apple/AWS Graviton; explicit barriers |
| **RISC-V (RISC, open)** | fixed, modular extensions (M/A/F/D/C/V), no royalties | rising in embedded and DC accelerators |

Calling conventions: x86-64 SysV vs Microsoft x64 vs AArch64 AAPCS — relevant when debugging across toolchains.

---

## Bugs in this Topic

- **Producer-consumer on ARM with no barrier:** consumer sees `ready=1` with stale data. Fix: `dmb st` on producer between data and flag; `dmb ld` on consumer; or use `STLR/LDAR`.
- **Branchy hot loop with high mispredict rate:** wasted-work cost ≈ 15-20 cycles per mispredict × pipeline depth × IPC. Counter: `BR_MISP_RETIRED.ANY`. Mitigation: branchless (cmov, table lookup), profile-guided opt, restructure.
- **AVX-512 pessimisation:** sprinkling AVX-512 in scalar code lowers F via license level. Counter: `CORE_POWER.LVL2_TURBO_LICENSE`. Mitigate with `-mprefer-vector-width=256` or restrict AVX-512 to long-running kernels.
- **HW prefetcher hides a software bug:** silicon A passes, B regresses. Triage: disable HW PF, re-run.
- **I-cache thrash from huge code:** front-end bound in TMA, μop cache miss high. Mitigate: PGO, hot/cold code separation, smaller object format.
- **Speculative side-channel anti-patterns** (Spectre-class): the same speculative execution that hides branch latency leaks data via cache state. Mitigations: `lfence` on x86 around bounds checks, ARM `CSDB`, retpoline, kernel hardening. Useful interview context, not validation work.

---

## Interview Explanation (60-90 sec)

> "Modern cores are deeply pipelined out-of-order machines. The front end fetches, predicts, decodes, and feeds μops; on x86 a μop cache bypasses the variable-length decoder bottleneck. The backend renames registers into a large physical file to kill WAR and WAW, schedules μops to functional units when operands are ready, and retires in order through the ROB. Three hazards block the pipeline: structural, data, control. The memory consistency model decides what reorderings the hardware exposes — x86 is TSO so most code "just works," ARM is weak so you have to put `dmb st` between data and flag in any producer-consumer pattern. SIMD widens each instruction; on Intel each wider class draws more current, so the license level preemptively cuts core frequency to maintain V margin — sprinkling AVX-512 into mostly-scalar code can be a net loss. Prefetchers hide latency, but they also hide bugs — disabling them is a standard triage step when silicon A passes and silicon B regresses."

---

## Practice Q&A

**Q1. Branch mispredict on a 16-stage pipeline at IPC=3 — wasted work?**
~16 × 3 = ~48 μops squashed plus front-end refill (4-8 cycles).

**Q2. Why does x86 need a μop cache and ARM doesn't?**
x86 instructions are variable length and decode is serial/expensive; μop cache delivers post-decode μops at higher bandwidth. ARM's fixed 4-byte format decodes trivially in parallel.

**Q3. TSO vs weak — give one bug that occurs on ARM but not x86.**
Producer writes data then `ready=1`; consumer polls flag then reads data. On TSO stores propagate in order; on weak ARM, `ready=1` can become visible before data → consumer reads stale data.

**Q4. HW prefetcher hides a bug — describe.**
Software's missing manual prefetch is masked by HW stride PF. Silicon B (different heuristic / PF off in BIOS) regresses. Triage: disable HW PF (`MSR 0x1A4`).

**Q5. Why does AVX-512 sometimes make code slower?**
License level cuts F when AVX-512 is active; if it's a small fraction of the code, scalar-around it runs slower for the recovery window — net loss.

**Q6. RAT, ROB, RS — one sentence each.**
RAT: rename table, maps arch reg → physical reg, kills WAR/WAW. ROB: in-order retirement buffer that bounds the instruction window. RS: wakes μops with ready operands and dispatches to FUs.

**Q7. Forwarding / bypass — what does it eliminate?**
Pipeline stall on RAW dependency that would otherwise wait for write-back; result is forwarded directly from EX/MEM to dependent EX.

**Q8. RAS (return address stack) — purpose?**
Predicts the target of `RET` from the matching `CALL`. Mispredicted RAS = function-call hot path tanks.

**Q9. Why is branch mispredict worse on a deeper pipeline?**
More in-flight work to squash; more cycles to refill the front end after re-steer.

---

## Coding A — Systems C: pipeline simulator + 2-bit branch predictor + license-level model

```c
#include <stdio.h>
#include <stdbool.h>

/* 2-bit saturating counter: 0,1 = predict NT; 2,3 = predict T */
static int bp = 1;
static unsigned hits, total;

static bool predict(void) { return bp >= 2; }
static void update(bool taken) {
    if (taken && bp < 3) bp++;
    if (!taken && bp > 0) bp--;
}

/* Effective frequency under license-level mix. */
static double effective_freq_ghz(double scalar_pct, double avx2_pct,
                                 double avx512_pct, double base_ghz) {
    double f0 = base_ghz;
    double f1 = base_ghz * 0.90;
    double f2 = base_ghz * 0.75;
    return scalar_pct * f0 + avx2_pct * f1 + avx512_pct * f2;
}

/* 5-stage pipeline with one load-use hazard demo */
static void pipeline_demo(void) {
    const char *insn[] = { "ld  r1,[r0]", "add r2,r1,r3", "sub r4,r5,r6", "st  r2,[r0]" };
    int n = 4, stall = 0;
    printf("\nCycle: 1  2  3  4  5  6  7  8  9 10\n");
    for (int i = 0; i < n; i++) {
        printf("%-12s ", insn[i]);
        for (int c = 0; c < 10; c++) {
            int p = c - i - stall;
            const char *s = "  .";
            if (p == 0) s = " IF";
            else if (p == 1) s = " ID";
            else if (p == 2) s = " EX";
            else if (p == 3) s = " ME";
            else if (p == 4) s = " WB";
            printf("%s", s);
        }
        printf("\n");
        if (i == 0) stall = 1;  /* load-use stall before next insn */
    }
}

int main(void) {
    /* Predictor on a "take, take, take, NT, T, T, NT" stream */
    int stream[] = { 1,1,1,0,1,1,0,1,1,1,0,0,1 };
    int n = sizeof stream / sizeof *stream;
    for (int i = 0; i < n; i++) {
        bool t = stream[i];
        if (predict() == t) hits++;
        update(t); total++;
    }
    printf("BP accuracy %u/%u = %.1f%%\n", hits, total, 100.0*hits/total);

    printf("F (100%% scalar): %.2f GHz\n",
           effective_freq_ghz(1.0, 0.0, 0.0, 4.0));
    printf("F (90%% scalar + 10%% AVX-512): %.2f GHz   <-- often a net loss\n",
           effective_freq_ghz(0.9, 0.0, 0.1, 4.0));

    pipeline_demo();
    return 0;
}
```

## Coding B — Interview DS&A: Counting Bits (LC 338)

DP with bit trick.

```python
def countBits(n):
    bits = [0] * (n + 1)
    for i in range(1, n + 1):
        bits[i] = bits[i >> 1] + (i & 1)
    return bits
```

---

## Bug Anchor

- **Day 5 (false sharing):** the same coherence story drives many concurrency bugs.
- **Day 8 (DMA / doorbells):** ARM weak ordering needs `dmb st` between descriptor and doorbell.
- **Day 15 (perf counters):** TMA, mispredict, μop cache miss, license-level counters live here.
- **Day 17 (concurrency):** acquire/release semantics bind to today's memory model.

## Exit Check

- [ ] Drew 5-stage pipeline + 3 hazard classes from memory.
- [ ] Named front-end (BTB, μop cache, RAS) and backend (RAT, ROB, RS, LSQ) blocks.
- [ ] Articulated TSO vs weak with the producer-consumer bug.
- [ ] Stated HW prefetcher disable as triage step.
- [ ] Stated AVX-512 license-level pessimisation.
- [ ] Stated branch mispredict cost = depth × IPC.

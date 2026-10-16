# Day 12 — Metastability + Synchronizers

## Concept

When a flip-flop's **setup/hold** window ([Day 7](day07_sequential_logic.md)) is violated — unavoidable for any signal arriving from a **different clock domain** or a truly **asynchronous** source — its output can hang **between 0 and 1** in a **metastable** state for an unbounded (but exponentially decaying) time before snapping to a random value. You cannot prevent the *event*; you make the probability of it causing a *failure* vanishingly small by giving the metastable node **time to resolve** — the **two-flop synchronizer**. The math behind "how much time is enough" is **MTBF**.

## Why this day in the flow

[Day 7](day07_sequential_logic.md) defined the setup/hold contract; today is **what physically happens when that contract is broken by an asynchronous edge** — the root cause of every CDC bug. This is the foundation for [Day 13](day13_cdc_techniques.md) (the full crossing taxonomy: pulse, bus, reset) and [Day 14](day14_async_fifo_cdc_verification.md) (async FIFO + CDC verification). Your retained **Intel back-to-back-wake** bug ([Day 31](day31_post_si_pm_validation.md)) is a CDC failure — an *event* crossed a domain as a one-cycle pulse and was lost — so this chapter also sharpens a real bug story.

**Broadcom R2 gap this closes:** *the* major weakness — "metastability physics," "design methods to avoid metastability," "setup/hold," "two-flop synchronizer," MTBF. This is the deep version of §3–§4 in [broadcom_switch_presi_notes.html](../../broadcom_switch_presi_notes.html).

---

## Deep Study Notes

### 1. What metastability *is* — the physics

A flip-flop stores a bit using **regenerative feedback** — two cross-coupled inverters with two stable points (0 and 1) and **one unstable equilibrium** exactly between them.

```
energy
  \                         /
   \        metastable      /     ← unstable: a ball balanced on the hilltop
    \      (the hilltop)   /
     \_____      .   _____/
       0   \____/ \_/   1          ← two stable valleys
```

When D meets the clock edge **outside** the setup/hold window, the flop settles cleanly into 0 or 1. When D transitions **inside** that aperture, the flop can be pushed to the **hilltop** — neither 0 nor 1. Like a ball balanced on a peak, it *will* eventually fall to one side, but **how long it balances is unbounded** and the side it falls to is **random**. While balanced, Q sits at an intermediate voltage that downstream gates may interpret inconsistently. The resolution is **exponential**: the probability the node is *still* metastable decays as $e^{-t/\tau}$.

### 2. When it happens — and why you can't avoid the event

Metastability is triggered by a setup/hold violation, which is **guaranteed to occur eventually** whenever:

- a signal crosses from **clock domain A to clock domain B** (the edges have no fixed phase → sometimes D moves inside B's aperture), or
- a **truly asynchronous** input is sampled (a button, a sensor, an async reset *release* — Day 7 §9).

Within one synchronous domain, STA guarantees setup/hold are met, so flops never go metastable. **CDC is the entire problem.** You can't stop the event; you engineer so it (almost) never propagates as a *failure*.

### 3. Resolution time and MTBF — the equation that runs the chapter

The mean time between failures caused by metastability:

$$\text{MTBF} = \frac{e^{\,t_r / \tau}}{T_0 \cdot f_{clk} \cdot f_{data}}$$

| Term | Meaning | You control? |
|---|---|---|
| $t_r$ | **resolution time allowed** before the next stage samples (≈ one clock period per added sync flop) | **YES — the lever** |
| $\tau$ | flop's metastability time-constant (regeneration speed) | no (technology) |
| $T_0$ | flop's metastability susceptibility window | no (technology) |
| $f_{clk}$ | sampling clock frequency | partly |
| $f_{data}$ | rate of asynchronous data changes | partly |

The exponential $e^{t_r/\tau}$ is everything: **a little more resolution time buys an enormous MTBF increase.** Faster clocks and faster data toggling *reduce* MTBF (more chances to sample in the aperture, less time to resolve), which is why CDC gets harder at high frequency.

### 4. The two-flop synchronizer — the canonical fix

```
   async / domain-A signal
        │
        ▼  (may go metastable here)
   ┌────────┐      ┌────────┐
   │  FF1   │─────▶│  FF2   │──────▶  clean, synchronized signal
   └────────┘      └────────┘         (domain B)
        ▲               ▲
       clkB            clkB
   └──────── no logic between them ────────┘
```

**How it works:** FF1 samples the asynchronous signal and may go metastable. It then gets **a full clock cycle** (`t_r ≈ T_clkB`) to resolve before **FF2** samples it. By the next edge, FF1 has resolved to a clean 0/1 with overwhelming probability, so FF2 captures a stable value. **Two flops** turn `t_r` from "nearly zero" into "~one cycle," pushing MTBF from **seconds to millions of years**. At very high clock rates (short `t_r` per stage) you add a **third** flop to recover the exponential margin.

### 5. MTBF worked intuition

Each added flop multiplies `t_r` by roughly one clock period, and MTBF grows as $e^{t_r/\tau}$:

```
1 flop  : t_r ≈ 0           → MTBF ≈ failures per second        (unusable)
2 flops : t_r ≈ 1 cycle     → MTBF ≈ years → millennia          (standard)
3 flops : t_r ≈ 2 cycles    → MTBF ≈ age-of-universe scale      (very high freq)
```

The exact numbers need `τ`, `T0` from the cell library, but the **shape** — exponential in flop count — is the point. Sign-off is "MTBF ≫ product lifetime with margin," not "metastability impossible."

### 6. What a synchronizer does and does **NOT** solve

A 2-FF synchronizer **only** safely transfers **one bit of stable, slowly-changing control/level information** (a flag that sits at 0 or 1). It does **not**:

- **Carry a multi-bit value safely.** Each bit resolves independently with its own random delay → you can latch a word that **never existed** (bits skew). → Day 13 (handshake / Gray / FIFO).
- **Guarantee a one-cycle pulse survives.** If the source pulse is narrower than the destination period it can be **missed**; if it straddles edges it can be **stretched**. → Day 13 (toggle + edge-detect / handshake).
- **Preserve correlation between separate synchronized signals.** Two flags synced independently can arrive on different cycles. → sync once, distribute (avoid re-convergence).

**Knowing the limits is the senior signal.** "I'd 2-FF a level, but for that bus I'd use a handshake or async FIFO" is the answer that lands.

### 7. Synchronizer design rules (the checklist)

1. **No combinational logic between FF1 and FF2** — logic eats the resolution time `t_r` and re-exposes the metastable value. The two flops must be back-to-back.
2. **Place them physically close**, on the destination clock; minimize routing so the full period is available to resolve.
3. **No fanout from FF1** — its possibly-metastable output must feed *only* FF2. Fanning it out spreads metastability to multiple cones.
4. **Use the library's dedicated synchronizer cells** if available (higher `τ`/lower `T0`, sometimes back-to-back in one cell).
5. **Constrain it in STA/CDC** — mark the crossing a **false/async path** (`set_false_path` or a max-delay/`set_clock_groups`) so the tool doesn't try to time it normally, and so CDC tools recognize the structure.
6. **Synchronize a signal once, then distribute** — never let a synchronized net re-converge with another path (re-convergence corrupts the assumption that it's stable).

### 8. Classifying the crossing — choose the structure by *what* crosses (preview of Day 13)

| What's crossing | Correct structure | Why a plain 2-FF fails |
|---|---|---|
| **Single control level** (stable 1/0) | **2-FF synchronizer** | — (this is its job) |
| **A pulse / event** | toggle + 2-FF + edge-detect, or req/ack | pulse can be missed or stretched |
| **Multi-bit data bus** | handshake (req/ack + recirc-mux) or async FIFO | bits skew → value that never existed |
| **Multi-bit counter/pointer** | **Gray code** + 2-FF | only 1 bit changes/step → off-by-≤1, always legal |
| **Reset de-assert** | reset synchronizer (async assert, sync de-assert) | release near edge → metastable wake |

Day 13 builds each of these; today you must be able to **say which one** for a given signal and **why the level-only synchronizer is insufficient** for the rest.

### 9. Why simulation hides metastability (verification reality)

In RTL simulation a flop resolves **instantly** to the sampled value — there is no hilltop, so a naive sim **never shows** a metastability bug. That is exactly why CDC bugs escape functional sim and bite in silicon. Catching them needs:

- **Structural CDC tools** (Spyglass-CDC, VC-SpyGlass, Conformal-CDC, Questa-CDC) — *static* analysis that finds every crossing and checks each has a correct synchronizer.
- **Metastability injection** — sim models that randomly **delay/invert** a synchronized signal by a cycle to mimic real resolution, stress-testing that the downstream logic is robust to it.
- **Assertions** that a synchronized control changes no faster than the destination can sample.

This is the seam into [Day 14](day14_async_fifo_cdc_verification.md)'s CDC verification methodology — and it's why "it passed functional sim" is **not** evidence a crossing is safe.

---

## Bugs in this Topic

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Missing synchronizer** | rare, un-reproducible field hang/corruption | async signal sampled by one flop | 2-FF synchronizer |
| **Logic between sync flops** | metastability still propagates | gate eats `t_r` | back-to-back flops, no logic |
| **Fanout from FF1** | multiple cones corrupted | possibly-metastable net fanned out | FF1 → FF2 only |
| **One-flop "sync"** | works in sim, fails in silicon | `t_r ≈ 0`, tiny MTBF | add second (third) flop |
| **Bus synced bit-by-bit** | latches a value that never existed | independent per-bit resolution | handshake / Gray / FIFO (Day 13–14) |
| **Lost one-cycle pulse** | intermittent missed event (the back-to-back-wake bug) | pulse synchronized as a level | toggle + edge-detect / handshake |
| **Timed as a real path** | false STA violations or unrecognized crossing | no false-path/CDC constraint | `set_false_path` + CDC setup |

---

## Interview Explanation (60–90 sec)

> "A flip-flop stores a bit with regenerative feedback that has two stable points and one unstable point exactly between them. If the data input changes inside the setup/hold window — which is unavoidable when a signal comes from another clock domain — the flop can be pushed to that unstable point and hang between 0 and 1. Like a ball balanced on a peak, it resolves eventually, but the time is unbounded and the result is random; the probability it's still metastable decays exponentially. You can't prevent the event, so you give it time to resolve: a two-flop synchronizer lets the first flop take a full clock cycle to settle before the second samples it, and because MTBF goes as e-to-the-resolution-time-over-tau, one extra flop pushes the mean time to failure from seconds to millennia. The critical caveat is that this only safely moves a single, slowly-changing control bit — for a multi-bit bus the bits skew and you'd latch a value that never existed, for a one-cycle pulse you can miss it, and for a counter you Gray-code it. So my rule is: classify the crossing first — level, pulse, bus, counter, reset — then pick the matching structure, and never put logic between the two synchronizer flops or fan out the first flop. And I never trust functional sim to prove a crossing is safe, because RTL flops resolve instantly — I rely on a structural CDC tool plus metastability injection."

---

## Architect Drills (derive, no answer key)

1. Write the MTBF equation from memory. If you double `f_clk`, what happens to MTBF, and how do you recover it?
2. Why exactly two flops and not one? Sketch where metastability lives and how `t_r` changes from 1 to 2 flops.
3. Someone puts an AND gate between FF1 and FF2 "to qualify the signal." Explain precisely why MTBF collapses.
4. You must move an 8-bit status bus across domains. Argue why eight parallel 2-FF synchronizers is wrong; name two correct structures.
5. A wake event is a single source-clock pulse and it's intermittently lost in the destination domain. Diagnose using the level-vs-pulse classification; give the fix (map it to the Intel back-to-back-wake bug).
6. Why does functional RTL sim pass a design with a missing synchronizer? What two tools/techniques would catch it?
7. At 3 GHz a 2-FF synchronizer's MTBF is marginal. Two independent ways to raise it without lowering the clock.

---

## Coding A — `d12_metastability`

Build an **MTBF + synchronizer model in C**: `mtbf(t_r, tau, T0, f_clk, f_data)` (assert it grows exponentially in `t_r` and shrinks with `f_clk`/`f_data`), a **2-FF synchronizer model** that delays a level by two destination cycles, and a **metastability-injection harness** that, on a simulated near-edge transition, randomly stalls the first stage by a cycle and shows the 2-FF structure still delivers a clean level while a 1-FF "sync" can pass a corrupted one. Full spec in [coding/EXERCISES.md](../../coding/EXERCISES.md#day-12--metastability--synchronizers).

**Interview prompts:** Which MTBF term is the design lever? Why no logic between the flops? What can't a 2-FF synchronizer carry?

---

## Bug Anchor / cross-links

- **Built from:** [Day 7](day07_sequential_logic.md) (setup/hold = the violated contract; async-reset release).
- **Feeds:** [Day 13](day13_cdc_techniques.md) (pulse/bus/reset crossings built on this), [Day 14](day14_async_fifo_cdc_verification.md) (async FIFO Gray pointers + CDC verification), [Day 25](day25_testbench_assertions_debug.md) (metastability-injection + CDC assertions in the testbench).
- **Bug story:** [Day 31](day31_post_si_pm_validation.md) back-to-back-wake (event-vs-level crossing) is *this* failure mode — narrate it as "wrong CDC structure for a pulse."

---

## Exit Check

- [ ] Explain metastability with the ball-on-hilltop / regenerative-feedback picture.
- [ ] Write the MTBF equation and name the one term you control.
- [ ] Explain why a 2-FF synchronizer works and when you'd add a 3rd flop.
- [ ] List what a 2-FF synchronizer **cannot** carry (bus, pulse, correlated signals).
- [ ] Recite the synchronizer design rules (no logic between flops, no FF1 fanout, false-path, sync-once).
- [ ] Classify a given signal (level/pulse/bus/counter/reset) and name its correct structure.
- [ ] Explain why functional sim misses CDC bugs and what catches them.

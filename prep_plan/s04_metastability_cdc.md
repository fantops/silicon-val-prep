# S04 - Metastability and CDC

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [D12 metastability](archive/day12_metastability_synchronizers.md), [D13 CDC techniques](archive/day13_cdc_techniques.md); skim [D14 async FIFO](archive/day14_async_fifo_cdc_verification.md) for Gray pointers.
- **Must answer:**
  - Why does a 2-flop synchronizer work? Which technique fits each signal type: level, pulse, or multi-bit bus?
  - **Lost-pulse wake:** a fast pulse is sampled by a slow or gated domain. What do you observe? (This is your Core C6 wake-loss story.)
- **Work link:** a doorbell write and a completion interrupt each cross between the CPU fabric and device clock domains. Where would a lost event show up as a hang?
- **Story re-tell (story 2, CDC angle):** **Core C6 wake-loss (CFC FIFO race)**, explained as a CDC structure problem: what was lost, and which synchronizer or FIFO fix closes it.
- **Handoff →** Before any clock ticks, what happens physically at power-on?

## Study notes

### Concept - [day12_metastability_synchronizers.md](archive/day12_metastability_synchronizers.md)

When a flip-flop's **setup/hold** window ([Day 7](archive/day07_sequential_logic.md)) is violated — unavoidable for any signal arriving from a **different clock domain** or a truly **asynchronous** source — its output can hang **between 0 and 1** in a **metastable** state for an unbounded (but exponentially decaying) time before snapping to a random value. You cannot prevent the *event*; you make the probability of it causing a *failure* vanishingly small by giving the metastable node **time to resolve** — the **two-flop synchronizer**. The math behind "how much time is enough" is **MTBF**.

### Deep Study Notes - [day12_metastability_synchronizers.md](archive/day12_metastability_synchronizers.md)

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

This is the seam into [Day 14](archive/day14_async_fifo_cdc_verification.md)'s CDC verification methodology — and it's why "it passed functional sim" is **not** evidence a crossing is safe.

---

### Bugs in this Topic - [day12_metastability_synchronizers.md](archive/day12_metastability_synchronizers.md)

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

### Interview Explanation (60–90 sec) - [day12_metastability_synchronizers.md](archive/day12_metastability_synchronizers.md)

> "A flip-flop stores a bit with regenerative feedback that has two stable points and one unstable point exactly between them. If the data input changes inside the setup/hold window — which is unavoidable when a signal comes from another clock domain — the flop can be pushed to that unstable point and hang between 0 and 1. Like a ball balanced on a peak, it resolves eventually, but the time is unbounded and the result is random; the probability it's still metastable decays exponentially. You can't prevent the event, so you give it time to resolve: a two-flop synchronizer lets the first flop take a full clock cycle to settle before the second samples it, and because MTBF goes as e-to-the-resolution-time-over-tau, one extra flop pushes the mean time to failure from seconds to millennia. The critical caveat is that this only safely moves a single, slowly-changing control bit — for a multi-bit bus the bits skew and you'd latch a value that never existed, for a one-cycle pulse you can miss it, and for a counter you Gray-code it. So my rule is: classify the crossing first — level, pulse, bus, counter, reset — then pick the matching structure, and never put logic between the two synchronizer flops or fan out the first flop. And I never trust functional sim to prove a crossing is safe, because RTL flops resolve instantly — I rely on a structural CDC tool plus metastability injection."

---

### Architect Drills (derive, no answer key) - [day12_metastability_synchronizers.md](archive/day12_metastability_synchronizers.md)

1. Write the MTBF equation from memory. If you double `f_clk`, what happens to MTBF, and how do you recover it?
2. Why exactly two flops and not one? Sketch where metastability lives and how `t_r` changes from 1 to 2 flops.
3. Someone puts an AND gate between FF1 and FF2 "to qualify the signal." Explain precisely why MTBF collapses.
4. You must move an 8-bit status bus across domains. Argue why eight parallel 2-FF synchronizers is wrong; name two correct structures.
5. A wake event is a single source-clock pulse and it's intermittently lost in the destination domain. Diagnose using the level-vs-pulse classification; give the fix (map it to the Intel back-to-back-wake bug).
6. Why does functional RTL sim pass a design with a missing synchronizer? What two tools/techniques would catch it?
7. At 3 GHz a 2-FF synchronizer's MTBF is marginal. Two independent ways to raise it without lowering the clock.

---

### Exit Check - [day12_metastability_synchronizers.md](archive/day12_metastability_synchronizers.md)

- [ ] Explain metastability with the ball-on-hilltop / regenerative-feedback picture.
- [ ] Write the MTBF equation and name the one term you control.
- [ ] Explain why a 2-FF synchronizer works and when you'd add a 3rd flop.
- [ ] List what a 2-FF synchronizer **cannot** carry (bus, pulse, correlated signals).
- [ ] Recite the synchronizer design rules (no logic between flops, no FF1 fanout, false-path, sync-once).
- [ ] Classify a given signal (level/pulse/bus/counter/reset) and name its correct structure.
- [ ] Explain why functional sim misses CDC bugs and what catches them.

### Concept - [day13_cdc_techniques.md](archive/day13_cdc_techniques.md)

A 2-flop synchronizer ([Day 12](archive/day12_metastability_synchronizers.md)) safely moves **one slowly-changing level**. Everything else needs a structure matched to **what** is crossing: a **pulse/event** (toggle synchronizer or handshake), a **multi-bit bus** (req/ack handshake or MUX-recirculation, or an async FIFO — [Day 14](archive/day14_async_fifo_cdc_verification.md)), a **multi-bit counter/pointer** (Gray code), and **reset** (reset synchronizer: async-assert / sync-de-assert). The senior skill is **classify the crossing, then pick the structure** — and to know **reset-domain crossing (RDC)** is a separate hazard from CDC.

### Deep Study Notes - [day13_cdc_techniques.md](archive/day13_cdc_techniques.md)

### 1. The crossing taxonomy — pick by what crosses

| What's crossing | Structure | Core idea |
|---|---|---|
| **Single level** (stable 1/0) | **2-FF synchronizer** | give metastability a cycle to resolve (Day 12) |
| **Pulse / event** (1-cycle strobe) | **toggle synchronizer** (toggle + 2-FF + edge-detect) or **handshake** | turn the fragile pulse into a durable level, recover the edge |
| **Multi-bit bus** | **req/ack handshake** or **MUX-recirculation**; or **async FIFO** | hold data stable; capture only when a synced "valid" says it's safe |
| **Multi-bit counter/pointer** | **Gray code** + 2-FF | one bit changes/step → mid-flight sample off by ≤1, always legal |
| **Reset de-assert** | **reset synchronizer** (async assert, sync de-assert) | avoid a metastable wake on reset *release* |

Two universal rules under all of it: **synchronize once, then distribute** (never let a synced net re-converge), and **tell STA it's a false/async path** so it isn't timed normally.

### 2. Pulse / event crossing — the toggle synchronizer

A one-cycle pulse cannot be 2-FF'd directly: in a **fast→slow** crossing the destination may **never sample** the pulse (it's gone before an edge); near an edge it can be **stretched** to two. Convert the *event* into a *level change*:

```
SOURCE domain                         DEST domain
  pulse ─► toggle FF (flip on pulse)   level ─► [2-FF sync] ─► edge-detect ─► pulse_out
            (level toggles 0↔1                  (FF_a, FF_b)    (FF_b ^ FF_c)
             once per event)

  pulse_out = synced_prev ^ synced_now   // XOR of last two = "the level moved"
```

The **toggle** turns each event into a durable 0→1 or 1→0 transition; the 2-FF carries the *level* safely; the **edge-detector** (XOR of two adjacent synced samples) regenerates one clean destination-domain pulse per event.

**The hard limit — closed vs open loop:** this open-loop toggle works only if events are **spaced far enough apart** that the destination samples each transition before the next (events slower than ~2–3 destination cycles). For **back-to-back** events you need a **closed-loop handshake** (next section) so the source *waits for acknowledgment* before sending again. **This is exactly the Intel back-to-back-wake bug** ([Day 31](archive/day31_post_si_pm_validation.md)): an event crossed as a pulse and a tight retrigger was lost because there was no ack — fix = level/handshake with a proper synchronizer.

### 3. Handshake (req/ack) — the safe multi-bit crossing

A **4-phase (return-to-zero) handshake** moves a whole word with no FIFO:

```
SOURCE                                  DEST
1. place DATA on the bus, raise REQ ───────────►  (REQ synced via 2-FF)
2.                                       latch DATA (now provably stable), raise ACK
3. (ACK synced back via 2-FF) ◄─────────────────
   drop REQ
4.                                       drop ACK   → ready for next word
```

Why it's safe for a **bus**: the data is **held stable** the entire time REQ is propagating, so when DEST finally sees REQ (after its synchronizer) and latches, the bits are guaranteed settled — **no skew hazard**, because you never synchronize the data bits themselves, only the 1-bit REQ/ACK *control*. Cost: **latency** — a full round-trip of synchronizers per word (low throughput). **2-phase (non-return-to-zero)** handshake uses edges instead of levels to halve the transitions for higher throughput. For sustained high-bandwidth streaming, use an **async FIFO** instead ([Day 14](archive/day14_async_fifo_cdc_verification.md)).

### 4. MUX-recirculation (data-hold) — the lightweight bus method

When the source can *hold* the data, sync just a **load-enable**:

```
DEST capture register feeds back on itself (recirculates) until a
synchronized "load" pulse arrives; on load, it captures the bus once.
  reg <= load_sync ? data_bus : reg;   // hold until told to grab
```

The synchronized `load` (a 2-FF'd level→pulse) guarantees the bus is sampled only when stable. Cheaper than a full handshake when the producer naturally holds the value.

### 5. Gray code — the counter/pointer crossing

A **binary** counter changes **multiple bits at once** (`0111 → 1000`): sample it mid-flight across a domain and you can read a value that **never existed** (e.g., `1111` or `0000`) → false compare → data loss/duplication. **Gray code** changes **exactly one bit per step**, so a mid-increment sample is **off by at most one position — always a legal value**.

```
binary→gray:  g = b ^ (b >> 1);
gray→binary:  b = g; b ^= b>>1; b ^= b>>2; b ^= b>>4; ...   (XOR-scan)
```

This is *the* reason async-FIFO read/write pointers are Gray-coded before crossing for the full/empty compare ([Day 14](archive/day14_async_fifo_cdc_verification.md)). Any free-running counter that must be observed in another domain → Gray + 2-FF.

### 6. Reset synchronizer — async assert, sync de-assert

The async-reset **release** is itself a crossing (Day 7 §9): if reset de-asserts inside a flop's setup/hold window, flops can come out of reset **metastable**, landing in random/illegal states. The standard structure:

```
   rst_n_async ───────────────┐ (async — asserts immediately, even with clk stopped)
                              ▼
   1 ──►[ FF ]──►[ FF ]──► rst_n_sync   (both flops' async-clear = rst_n_async)
          ▲        ▲                      de-assertion walks through 2 FFs on dest clk
         clk      clk
```

- **Assertion is asynchronous** → reset takes effect instantly (even if the clock is dead — important for safe power-up/gating).
- **De-assertion is synchronous** → the release is registered through two flops, so it leaves reset cleanly aligned to the clock, with metastability given a cycle to resolve.
- One reset synchronizer **per clock domain**; distribute its output as that domain's clean reset.

### 7. Reset Domain Crossing (RDC) — a *different* hazard

CDC is about *clock* domains; **RDC** is about *reset* domains. A signal launched from a flop in **reset domain A** and captured by a flop in **reset domain B** is an RDC. The danger: if **A is being reset while B is active** (independent resets, common with power gating — [Day 28](archive/day28_clock_power_gating_upf.md)), the launch flop is forced to its reset value *asynchronously*, which can violate the capture flop's setup/hold → **metastability or capturing a spurious value**, even if the two share a clock (so CDC analysis alone misses it).

- **Detection:** dedicated **RDC analysis** (the CDC tool's RDC mode) — enumerate flop pairs whose reset domains differ.
- **Mitigation:** isolation/qualification (gate the crossing signal during the asserting reset), or a synchronizer that tolerates the reset, or shared/ordered reset sequencing.
- **Why it matters now:** multi-power-domain SoCs ([Day 28](archive/day28_clock_power_gating_upf.md)) and PM flows ([Day 30–31](archive/day30_pre_si_pm_validation.md)) create many reset domains — RDC is a top escape source in power-managed designs.

### 8. The do/don't rules (sign-off checklist)

- **Do** synchronize **every** crossing; pick the structure by *what* crosses (the §1 table).
- **Don't** sync a multi-bit bus bit-by-bit with parallel 2-FFs — latch a value that never existed.
- **Don't** let a synchronized signal **fan out and re-converge** — sync once, then distribute.
- **Don't** put logic between synchronizer flops (Day 12 §7).
- **Do** tell STA each crossing is a synchronized **false/async path** (`set_false_path` / `set_clock_groups`).
- **Do** run **CDC + RDC** structural analysis; functional sim won't find these (Day 12 §9, Day 14).

---

### Bugs in this Topic - [day13_cdc_techniques.md](archive/day13_cdc_techniques.md)

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Lost pulse (back-to-back)** | intermittent missed event/wake | open-loop toggle outrun by events | closed-loop handshake (req/ack) |
| **Stretched/double pulse** | one event seen as two | pulse synced as a level near an edge | toggle + edge-detect, qualify spacing |
| **Bus value that never existed** | corrupt multi-bit transfer | bits synced independently | handshake / recirc-mux / async FIFO |
| **Binary pointer mis-sampled** | false FIFO full/empty, data loss | multiple bits change at once | Gray-code the pointer |
| **Metastable reset release** | random post-reset state | async de-assert near edge | reset synchronizer (sync de-assert) |
| **RDC corruption** | spurious value when one domain resets | reset-domain mismatch on a crossing | RDC analysis + isolation/qualify |
| **Re-convergence** | logically impossible combinations | synced net fanned out then recombined | sync once, then distribute |

---

### Interview Explanation (60–90 sec) - [day13_cdc_techniques.md](archive/day13_cdc_techniques.md)

> "After the single-bit level case, I classify every crossing by what's moving. A one-cycle pulse can't go through a plain synchronizer — fast-to-slow it can be missed, near an edge it can be stretched — so I convert the event into a level with a toggle flop, synchronize the level with two flops, and regenerate one clean pulse with an edge detector; but that open-loop trick only survives if events are spaced out, so for back-to-back events I use a closed-loop req/ack handshake where the source waits for acknowledgment. For a multi-bit bus I never synchronize the data bits — the bits skew and I'd latch a word that never existed — instead I hold the data stable and synchronize a single req/ack so the destination captures only when it's provably settled, or I use an async FIFO for streaming. For a counter or pointer crossing I Gray-code it, because only one bit changes per step so a mid-flight sample is off by at most one and always legal — that's exactly how async-FIFO pointers work. And reset gets its own structure: assert asynchronously so it works even with the clock stopped, but de-assert synchronously through two flops so the release doesn't bring flops out of reset metastable. The one people forget is reset-domain crossing — even within one clock domain, if the launch flop's reset asserts while the capture flop is live, you get corruption, so I run RDC analysis separately from CDC."

---

### Architect Drills (derive, no answer key) - [day13_cdc_techniques.md](archive/day13_cdc_techniques.md)

1. Draw the toggle synchronizer end to end (toggle → 2-FF → XOR edge-detect). Show why a fast→slow pulse is lost without it, and when even the toggle version fails.
2. Walk a 4-phase req/ack handshake moving a 16-bit bus. Exactly when is the data guaranteed stable, and why don't you synchronize the data bits?
3. Compute the throughput cost of a handshake vs an async FIFO for a streaming source. When is each right?
4. Show `0111 → 1000` in binary and in Gray. Sample each mid-transition; which gives a legal value and why?
5. Build a reset synchronizer. Why async assert *and* sync de-assert — what breaks if you make assert synchronous? What breaks if you make de-assert asynchronous?
6. Define RDC and give a power-gating scenario ([Day 28](archive/day28_clock_power_gating_upf.md)) where it bites even with a shared clock. How do you detect it?
7. Map the Intel back-to-back-wake bug to §2: which structure was used, which was needed, what's the regression test?

---

### Exit Check - [day13_cdc_techniques.md](archive/day13_cdc_techniques.md)

- [ ] Recite the crossing taxonomy and name the structure for level / pulse / bus / counter / reset.
- [ ] Draw a toggle synchronizer and state its open-loop limit; give the closed-loop fix.
- [ ] Walk a req/ack handshake and explain why the bus bits are never synchronized.
- [ ] Show why Gray code makes a mid-flight pointer sample always legal.
- [ ] Build a reset synchronizer and justify async-assert / sync-de-assert.
- [ ] Define RDC, distinguish it from CDC, and name how it's detected.
- [ ] State the "sync once, no re-convergence, false-path it" rules.

### Concept - [day14_async_fifo_cdc_verification.md](archive/day14_async_fifo_cdc_verification.md)

The **async (dual-clock) FIFO** is the canonical structure for streaming **multi-bit data** across clock domains: a dual-port RAM with a write pointer in one domain and a read pointer in another, where the pointers cross as **Gray code** so the full/empty compare is always safe ([Day 13](archive/day13_cdc_techniques.md)). Two skills sit on top: **depth sizing** (rate vs burst — the Broadcom `80/100` vs `8/10` → **32** problem), and **CDC verification** — the *methodology* that proves every crossing in a chip is correctly synchronized, since functional sim can't (Day 12 §9).

### Deep Study Notes - [day14_async_fifo_cdc_verification.md](archive/day14_async_fifo_cdc_verification.md)

### 1. Async FIFO architecture

```
        write domain (wclk)                 read domain (rclk)
   ┌───────────────────────┐          ┌───────────────────────┐
   │  wptr (bin+gray)       │          │  rptr (bin+gray)       │
   │     │                  │          │     │                  │
   │     ▼   ┌───────────┐  │  data    │     ▼                  │
   │  write─►│ dual-port │──┼─────────►│  read                  │
   │         │   RAM     │  │          │                        │
   │  full ◄─┤  (2^n)    │  │          │  empty ◄──             │
   │     ▲   └───────────┘  │          │     ▲                  │
   │  wgray ──► [2-FF] ─────┼──────────┼──► to read full/empty  │
   │  rgray ◄── [2-FF] ◄────┼──────────┼──◄ from write          │
   └───────────────────────┘          └───────────────────────┘
```

- The **RAM** is written on `wclk`, read on `rclk` — no metastability on the *data* (the writer never reads the same cell the reader is reading; the pointers keep them apart).
- **Each pointer is Gray-coded and 2-FF-synchronized into the opposite domain** for the full/empty compare. That's the *only* place a crossing happens, and it's safe because Gray = one-bit-change (Day 13 §5).

### 2. Why Gray pointers (the whole trick)

A **binary** pointer changes multiple bits at once (`0111 → 1000`). Synchronized into the other domain mid-increment, it can resolve to a value that **never existed** → a false full/empty → **data loss or duplication**. A **Gray** pointer changes exactly one bit per increment, so a mid-flight sample is **off by at most one position — always a legal pointer value**. The compare may be momentarily *pessimistic* (you might see "almost full" one cycle late) but **never wrong** in a dangerous direction. Conservative is exactly what you want for full/empty.

### 3. Full / empty generation — the extra-bit trick

To distinguish **full** from **empty** when the pointers are equal, the pointers carry **one extra MSB** (an `n`-deep FIFO uses an `(n+1)`-bit pointer, i.e. `log2(DEPTH)+1` bits):

```
empty : read pointer == synchronized write pointer        (all bits equal)
        → nothing new to read

full  : write pointer == synchronized read pointer with
        the two MSBs inverted (Gray) / top bit differs, lower bits equal
        → writer has wrapped exactly one lap ahead of reader
```

The extra bit records **how many laps** ahead the writer is: equal pointers *including* the wrap bit = empty; equal lower bits but **opposite wrap bit** = full. Because each side compares against a **synchronized** (slightly stale) copy of the other pointer, **full asserts a touch early and empty a touch late** — conservative, never overflowing/underflowing. This pessimism is a feature.

### 4. FIFO depth sizing — rate vs burst (the Broadcom problem)

**The literal R1 question:** writes arrive at **80 per 100 clocks**, reads drain at **8 per 10 clocks**, all random — minimum depth for **no data loss**? Answer: **32**.

**Step 1 — compare average rates.**
```
write avg = 80/100 = 0.8 word/clk
read  avg =  8/10  = 0.8 word/clk      → EQUAL average rates
```
The trap is "rates match → no FIFO needed." Equal *average* rates means the FIFO never overflows **in steady state**, but it must still absorb the **burst backlog** — writes bunching while reads are in a slow phase.

**Step 2 — size for worst-case backlog.**
$$\text{depth} = \max\big(\text{cumulative writes} - \text{cumulative reads}\big)\ \text{over the burst window}$$
With writes able to bunch (up to 80 in a 100-clk window) while reads trickle (8 per 10-clk sub-window), the worst-case backlog builds to **≈ 32** before the reader catches up.

**The general rule:**
```
write_rate > read_rate (sustained)  → FIFO overflows eventually — needs BACKPRESSURE, not depth
write_rate = read_rate (this case)  → size for burst backlog only          → 32
write_rate < read_rate              → depth set by the largest single burst
```

**One-line answer:** *"Average rates are equal, so it's not a rate-mismatch problem — depth is set by the worst-case burst backlog of writes bunching while reads are slow, ≈ 32. If writes exceeded reads I couldn't fix it with depth at all — I'd need backpressure/flow control."* Don't oversize: area/power + latency, and a too-big FIFO hides a real rate problem.

### 5. Sync vs async FIFO — when each

| | **Synchronous FIFO** | **Asynchronous FIFO** |
|---|---|---|
| Clocks | one (read = write clk) | two independent |
| Pointer sync | none needed | Gray + 2-FF each way |
| Use | rate-matching / buffering in one domain | **clock-domain crossing** of a data stream |
| Full/empty | direct binary compare | conservative, synced-pointer compare |

Use a **sync FIFO** for buffering within a domain (Day 7-era), an **async FIFO** specifically to cross domains. A handshake (Day 13) is for *occasional* words; the async FIFO is for *sustained throughput*.

### 6. CDC verification methodology — proving it, since sim can't

Functional RTL sim resolves flops instantly, so it **cannot** show a missing or wrong synchronizer (Day 12 §9). CDC sign-off is a **separate, mostly-static flow**:

**(a) Structural / static CDC** (Spyglass-CDC, VC SpyGlass, Conformal-CDC, Questa-CDC):
- **Enumerate every crossing** — every signal whose launch clock ≠ capture clock.
- **Check each has a correct synchronizer** for its type (2-FF for control, recirc/handshake/FIFO for buses, Gray for pointers).
- **No combinational logic between synchronizer flops**; **single** synchronizer per crossing (no double-sync of the same source).
- **Convergence/re-convergence** checks — synced signals that recombine.
- **Data-stability** checks — bus held stable across the capture window.

**(b) RDC analysis** — reset-domain crossings (Day 13 §7), a separate pass.

**(c) Functional / dynamic CDC** — assertions + **metastability injection**:
```
- assert: synchronized control changes no faster than dest can sample
- assert: $onehot / Gray one-bit-change on pointers
- assert: data bus stable while "valid" is high (handshake)
- inject:  randomly delay/invert each synced signal by a cycle (model real
           resolution) and confirm downstream logic still behaves
```

**(d) CDC setup** — declare **clock groups** (`set_clock_groups -asynchronous`), **false-path** the crossings, and **waive** reviewed exceptions with justification. Sign-off = **lint-clean + RDC-clean + assertions pass under injection + waivers reviewed**.

### 7. Writing the FIFO test plan — functional + error (the Broadcom R2 ask)

For an async FIFO, "functional + error/negative tests" looks like:

| Class | Tests |
|---|---|
| **Functional** | single write→read; fill to full; drain to empty; simultaneous read+write; pointer wrap (lap the FIFO); data integrity (written == read, in order) |
| **Corner** | write at full (must block, not drop); read at empty (must block, not duplicate); near-full/near-empty oscillation; max clock-ratio skew (very fast wclk vs slow rclk and vice versa) |
| **Error / negative** | force overflow → assert no data loss / flag raised; force underflow → assert no garbage read; reset one domain mid-traffic → assert clean recovery (RDC); inject pointer metastability → assert no false full/empty |
| **Coverage** | both clock-ratio directions; full and empty both reached; wrap covered; every full/empty transition; back-pressure exercised |

This is the **test-plan-from-spec scaffold** ([Day 24](archive/day24_verification_planning_coverage.md)) applied to a concrete block — exactly what Broadcom probes.

---

### Bugs in this Topic - [day14_async_fifo_cdc_verification.md](archive/day14_async_fifo_cdc_verification.md)

| Bug | Symptom | Root cause | Fix |
|---|---|---|---|
| **Binary pointer sync** | random false full/empty, data loss | multi-bit pointer sampled mid-change | Gray-code the pointer |
| **Missing extra MSB** | can't tell full from empty | pointer too narrow | `log2(DEPTH)+1`-bit pointers |
| **Under-depth FIFO** | overflow under burst | sized for average, not burst backlog | size to peak backlog (the 32) |
| **Sustained wr > rd** | overflows no matter the depth | rate mismatch, not burst | backpressure / flow control |
| **No combo-logic rule broken** | metastability leaks past sync | gate between Gray sync flops | back-to-back flops |
| **RDC on FIFO reset** | corruption when one domain resets | reset-domain mismatch | reset both / RDC-safe sequencing |
| **"Passed functional sim"** | CDC bug escapes to silicon | sim resolves flops instantly | structural CDC + injection |

---

### Interview Explanation (60–90 sec) - [day14_async_fifo_cdc_verification.md](archive/day14_async_fifo_cdc_verification.md)

> "An async FIFO is how you stream multi-bit data across two clocks: a dual-port RAM written on one clock and read on the other, with the write and read pointers Gray-coded and two-flop-synchronized into the opposite domain to compute full and empty. Gray is the key — only one bit changes per increment, so a pointer sampled mid-flight is off by at most one and always legal; a binary pointer could resolve to a value that never existed and give a false full or empty. I carry one extra pointer MSB so I can tell full from empty when the lower bits match — same wrap bit means empty, opposite wrap bit means full — and because each side compares against a synchronized, slightly-stale copy, full asserts a little early and empty a little late, which is conservatively safe. Sizing the depth is a separate question: if write and read average rates are equal you still need depth for the worst-case burst backlog — that's the 80-per-100 versus 8-per-10 problem that works out to 32 — but if writes sustainably exceed reads, no depth saves you and you need backpressure. And I never trust functional sim to prove any of this, because RTL flops resolve instantly; CDC sign-off is a static tool that enumerates every crossing and checks the synchronizer, plus reset-domain-crossing analysis and metastability injection in simulation."

---

### Architect Drills (derive, no answer key) - [day14_async_fifo_cdc_verification.md](archive/day14_async_fifo_cdc_verification.md)

1. Re-derive the **32** from the `80/100` vs `8/10` problem as a cumulative-backlog argument. What changes if reads drop to `7/10`?
2. Why does an `n`-deep async FIFO need `log2(n)+1` pointer bits? Show full vs empty when the lower bits are equal.
3. Walk a `0111 → 1000` pointer increment in binary and Gray; sample each mid-transition in the other domain and show which is dangerous.
4. Your async FIFO passes every functional sim but corrupts data 1-in-10⁹ in silicon. List the structural CDC checks that would have caught it.
5. Write the **error/negative** test list for the FIFO (overflow, underflow, reset-mid-traffic, pointer metastability). What does each assert?
6. When do you choose a handshake over an async FIFO and vice versa? Tie it to throughput vs occasional transfer ([Day 13](archive/day13_cdc_techniques.md)).
7. Reset one domain while the other streams. Which hazard class is this (CDC or RDC) and how do you make the FIFO safe?

---

### Exit Check - [day14_async_fifo_cdc_verification.md](archive/day14_async_fifo_cdc_verification.md)

- [ ] Draw the async FIFO and explain where the only crossing is and why it's safe.
- [ ] Explain Gray pointers and the extra-MSB full/empty trick.
- [ ] Re-derive depth = 32 as a burst-backlog argument and state the three rate cases.
- [ ] Explain why full asserts early / empty asserts late, and why that's safe.
- [ ] List the CDC sign-off flow (structural + RDC + injection + waivers) and why sim alone fails.
- [ ] Write the functional + error/negative test list for an async FIFO.
- [ ] Choose async FIFO vs handshake for a given traffic pattern.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S03](s03_clocks_reset.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S05](s05_power_on_sequence.md)

# Day 13 — CDC Techniques: Pulse, Handshake, Gray, Reset

## Concept

A 2-flop synchronizer ([Day 12](day12_metastability_synchronizers.md)) safely moves **one slowly-changing level**. Everything else needs a structure matched to **what** is crossing: a **pulse/event** (toggle synchronizer or handshake), a **multi-bit bus** (req/ack handshake or MUX-recirculation, or an async FIFO — [Day 14](day14_async_fifo_cdc_verification.md)), a **multi-bit counter/pointer** (Gray code), and **reset** (reset synchronizer: async-assert / sync-de-assert). The senior skill is **classify the crossing, then pick the structure** — and to know **reset-domain crossing (RDC)** is a separate hazard from CDC.

## Why this day in the flow

[Day 12](day12_metastability_synchronizers.md) established *why* crossings are dangerous and fixed the single-bit level case. Today completes the toolbox for the cases a 2-FF can't handle, and **feeds directly into [Day 14](day14_async_fifo_cdc_verification.md)** (the async FIFO is the canonical multi-bit data crossing, and CDC verification checks all of these structures exist where they should). The handshake and pulse synchronizers here are small **FSMs** ([Day 9](day09_fsm_sequence_detectors.md)) crossing domains; the Gray pointers reuse [Day 7](day07_sequential_logic.md).

**Broadcom R2 gap this closes:** "multi-bit CDC, gray code, async FIFO, handshake synchronizers, reset synchronization" — the full §3 list named in the R2 ask.

---

## Deep Study Notes

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

**The hard limit — closed vs open loop:** this open-loop toggle works only if events are **spaced far enough apart** that the destination samples each transition before the next (events slower than ~2–3 destination cycles). For **back-to-back** events you need a **closed-loop handshake** (next section) so the source *waits for acknowledgment* before sending again. **This is exactly the Intel back-to-back-wake bug** ([Day 31](day31_post_si_pm_validation.md)): an event crossed as a pulse and a tight retrigger was lost because there was no ack — fix = level/handshake with a proper synchronizer.

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

Why it's safe for a **bus**: the data is **held stable** the entire time REQ is propagating, so when DEST finally sees REQ (after its synchronizer) and latches, the bits are guaranteed settled — **no skew hazard**, because you never synchronize the data bits themselves, only the 1-bit REQ/ACK *control*. Cost: **latency** — a full round-trip of synchronizers per word (low throughput). **2-phase (non-return-to-zero)** handshake uses edges instead of levels to halve the transitions for higher throughput. For sustained high-bandwidth streaming, use an **async FIFO** instead ([Day 14](day14_async_fifo_cdc_verification.md)).

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

This is *the* reason async-FIFO read/write pointers are Gray-coded before crossing for the full/empty compare ([Day 14](day14_async_fifo_cdc_verification.md)). Any free-running counter that must be observed in another domain → Gray + 2-FF.

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

CDC is about *clock* domains; **RDC** is about *reset* domains. A signal launched from a flop in **reset domain A** and captured by a flop in **reset domain B** is an RDC. The danger: if **A is being reset while B is active** (independent resets, common with power gating — [Day 28](day28_clock_power_gating_upf.md)), the launch flop is forced to its reset value *asynchronously*, which can violate the capture flop's setup/hold → **metastability or capturing a spurious value**, even if the two share a clock (so CDC analysis alone misses it).

- **Detection:** dedicated **RDC analysis** (the CDC tool's RDC mode) — enumerate flop pairs whose reset domains differ.
- **Mitigation:** isolation/qualification (gate the crossing signal during the asserting reset), or a synchronizer that tolerates the reset, or shared/ordered reset sequencing.
- **Why it matters now:** multi-power-domain SoCs ([Day 28](day28_clock_power_gating_upf.md)) and PM flows ([Day 30–31](day30_pre_si_pm_validation.md)) create many reset domains — RDC is a top escape source in power-managed designs.

### 8. The do/don't rules (sign-off checklist)

- **Do** synchronize **every** crossing; pick the structure by *what* crosses (the §1 table).
- **Don't** sync a multi-bit bus bit-by-bit with parallel 2-FFs — latch a value that never existed.
- **Don't** let a synchronized signal **fan out and re-converge** — sync once, then distribute.
- **Don't** put logic between synchronizer flops (Day 12 §7).
- **Do** tell STA each crossing is a synchronized **false/async path** (`set_false_path` / `set_clock_groups`).
- **Do** run **CDC + RDC** structural analysis; functional sim won't find these (Day 12 §9, Day 14).

---

## Bugs in this Topic

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

## Interview Explanation (60–90 sec)

> "After the single-bit level case, I classify every crossing by what's moving. A one-cycle pulse can't go through a plain synchronizer — fast-to-slow it can be missed, near an edge it can be stretched — so I convert the event into a level with a toggle flop, synchronize the level with two flops, and regenerate one clean pulse with an edge detector; but that open-loop trick only survives if events are spaced out, so for back-to-back events I use a closed-loop req/ack handshake where the source waits for acknowledgment. For a multi-bit bus I never synchronize the data bits — the bits skew and I'd latch a word that never existed — instead I hold the data stable and synchronize a single req/ack so the destination captures only when it's provably settled, or I use an async FIFO for streaming. For a counter or pointer crossing I Gray-code it, because only one bit changes per step so a mid-flight sample is off by at most one and always legal — that's exactly how async-FIFO pointers work. And reset gets its own structure: assert asynchronously so it works even with the clock stopped, but de-assert synchronously through two flops so the release doesn't bring flops out of reset metastable. The one people forget is reset-domain crossing — even within one clock domain, if the launch flop's reset asserts while the capture flop is live, you get corruption, so I run RDC analysis separately from CDC."

---

## Architect Drills (derive, no answer key)

1. Draw the toggle synchronizer end to end (toggle → 2-FF → XOR edge-detect). Show why a fast→slow pulse is lost without it, and when even the toggle version fails.
2. Walk a 4-phase req/ack handshake moving a 16-bit bus. Exactly when is the data guaranteed stable, and why don't you synchronize the data bits?
3. Compute the throughput cost of a handshake vs an async FIFO for a streaming source. When is each right?
4. Show `0111 → 1000` in binary and in Gray. Sample each mid-transition; which gives a legal value and why?
5. Build a reset synchronizer. Why async assert *and* sync de-assert — what breaks if you make assert synchronous? What breaks if you make de-assert asynchronous?
6. Define RDC and give a power-gating scenario ([Day 28](day28_clock_power_gating_upf.md)) where it bites even with a shared clock. How do you detect it?
7. Map the Intel back-to-back-wake bug to §2: which structure was used, which was needed, what's the regression test?

---

## Coding A — `d13_cdc_sync`

Model the crossings in C: a **pulse (toggle) synchronizer** (`toggle_sync`) that converts a source-pulse stream to dest-pulses and **demonstrates a lost back-to-back pulse** when events are too close, then a **req/ack handshake FSM** (`handshake_xfer`) that never loses a word, plus `bin_to_gray`/`gray_to_bin` reused from [Day 7](day07_sequential_logic.md) showing single-bit transitions. Full spec in [coding/EXERCISES.md](../../coding/EXERCISES.md#day-13--cdc-techniques-pulse-handshake-gray-reset).

**Interview prompts:** Why does the open-loop toggle lose back-to-back events? Why is a handshake safe for a bus without syncing the data? Why Gray for pointers?

---

## Bug Anchor / cross-links

- **Built from:** [Day 12](day12_metastability_synchronizers.md) (2-FF level sync + metastability), [Day 9](day09_fsm_sequence_detectors.md) (handshake/pulse synchronizers are small FSMs), [Day 7](day07_sequential_logic.md) (Gray code, async reset).
- **Feeds:** [Day 14](day14_async_fifo_cdc_verification.md) (async FIFO = the multi-bit data crossing + CDC/RDC verification), [Day 25](day25_testbench_assertions_debug.md) (CDC assertions, metastability injection).
- **Power tie-in:** RDC connects to [Day 28](day28_clock_power_gating_upf.md) (power/reset domains) and [Day 30–31](day30_pre_si_pm_validation.md) (PM validation escapes).
- **Bug story:** [Day 31](day31_post_si_pm_validation.md) back-to-back-wake = open-loop pulse where a handshake was needed.

---

## Exit Check

- [ ] Recite the crossing taxonomy and name the structure for level / pulse / bus / counter / reset.
- [ ] Draw a toggle synchronizer and state its open-loop limit; give the closed-loop fix.
- [ ] Walk a req/ack handshake and explain why the bus bits are never synchronized.
- [ ] Show why Gray code makes a mid-flight pointer sample always legal.
- [ ] Build a reset synchronizer and justify async-assert / sync-de-assert.
- [ ] Define RDC, distinguish it from CDC, and name how it's detected.
- [ ] State the "sync once, no re-convergence, false-path it" rules.

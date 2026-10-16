# Day 14 — Async FIFO + CDC Verification

## Concept

The **async (dual-clock) FIFO** is the canonical structure for streaming **multi-bit data** across clock domains: a dual-port RAM with a write pointer in one domain and a read pointer in another, where the pointers cross as **Gray code** so the full/empty compare is always safe ([Day 13](day13_cdc_techniques.md)). Two skills sit on top: **depth sizing** (rate vs burst — the Broadcom `80/100` vs `8/10` → **32** problem), and **CDC verification** — the *methodology* that proves every crossing in a chip is correctly synchronized, since functional sim can't (Day 12 §9).

## Why this day in the flow

This **caps Phase 2 (Clocking & CDC)**: [Day 12](day12_metastability_synchronizers.md) gave metastability + the level synchronizer, [Day 13](day13_cdc_techniques.md) gave pulse/handshake/Gray/reset, and today assembles them into the full data-streaming crossing **and** turns to *how you verify it* — which is the hinge into **Phase 5 (Verification Engineering, [Day 24–25](day24_verification_planning_coverage.md))**. It directly answers two things Broadcom asked: the **FIFO depth** problem (R1, depth=32) and **"write functional + error tests"** (R2) for a CDC structure. This deepens §1.4 and §4.3 of [broadcom_switch_presi_notes.html](../../broadcom_switch_presi_notes.html).

**Broadcom gaps closed:** FIFO depth reasoning (R1), async FIFO + CDC verification (R2), functional-vs-error test design on a real structure.

---

## Deep Study Notes

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

This is the **test-plan-from-spec scaffold** ([Day 24](day24_verification_planning_coverage.md)) applied to a concrete block — exactly what Broadcom probes.

---

## Bugs in this Topic

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

## Interview Explanation (60–90 sec)

> "An async FIFO is how you stream multi-bit data across two clocks: a dual-port RAM written on one clock and read on the other, with the write and read pointers Gray-coded and two-flop-synchronized into the opposite domain to compute full and empty. Gray is the key — only one bit changes per increment, so a pointer sampled mid-flight is off by at most one and always legal; a binary pointer could resolve to a value that never existed and give a false full or empty. I carry one extra pointer MSB so I can tell full from empty when the lower bits match — same wrap bit means empty, opposite wrap bit means full — and because each side compares against a synchronized, slightly-stale copy, full asserts a little early and empty a little late, which is conservatively safe. Sizing the depth is a separate question: if write and read average rates are equal you still need depth for the worst-case burst backlog — that's the 80-per-100 versus 8-per-10 problem that works out to 32 — but if writes sustainably exceed reads, no depth saves you and you need backpressure. And I never trust functional sim to prove any of this, because RTL flops resolve instantly; CDC sign-off is a static tool that enumerates every crossing and checks the synchronizer, plus reset-domain-crossing analysis and metastability injection in simulation."

---

## Architect Drills (derive, no answer key)

1. Re-derive the **32** from the `80/100` vs `8/10` problem as a cumulative-backlog argument. What changes if reads drop to `7/10`?
2. Why does an `n`-deep async FIFO need `log2(n)+1` pointer bits? Show full vs empty when the lower bits are equal.
3. Walk a `0111 → 1000` pointer increment in binary and Gray; sample each mid-transition in the other domain and show which is dangerous.
4. Your async FIFO passes every functional sim but corrupts data 1-in-10⁹ in silicon. List the structural CDC checks that would have caught it.
5. Write the **error/negative** test list for the FIFO (overflow, underflow, reset-mid-traffic, pointer metastability). What does each assert?
6. When do you choose a handshake over an async FIFO and vice versa? Tie it to throughput vs occasional transfer ([Day 13](day13_cdc_techniques.md)).
7. Reset one domain while the other streams. Which hazard class is this (CDC or RDC) and how do you make the FIFO safe?

---

## Coding A — `d14_async_fifo`

Build an **async FIFO model + depth calculator in C**: `fifo_write`/`fifo_read` over a `2^n` array with **Gray-coded** pointers and the `+1` MSB full/empty logic (assert: never overflow, never underflow, FIFO order + data integrity preserved); a **dual-rate driver** that randomizes write/read bursts and finds the **peak backlog**, reproducing **32** for the `80/100` vs `8/10` case; and an **error harness** that forces overflow/underflow and shows the flags fire instead of losing/duplicating data. Full spec in [coding/EXERCISES.md](../../coding/EXERCISES.md#day-14--async-fifo--cdc-verification).

**Interview prompts:** Why Gray pointers? Why the extra MSB? Why is depth a burst problem, not a rate problem, when averages match?

---

## Bug Anchor / cross-links

- **Built from:** [Day 12](day12_metastability_synchronizers.md) (pointer synchronizers), [Day 13](day13_cdc_techniques.md) (Gray crossing, RDC, handshake alternative), [Day 7](day07_sequential_logic.md) (Gray counters), [Day 5](day05_boolean_combinational_logic.md) (comparators for full/empty).
- **Feeds:** [Day 24](day24_verification_planning_coverage.md) (the FIFO test plan = the coverage/vplan scaffold), [Day 25](day25_testbench_assertions_debug.md) (FIFO scoreboard, CDC assertions, metastability injection).
- **Anchors the Broadcom problems:** R1 FIFO depth=32 (§1.4), R2 async FIFO + functional/error tests.

---

## Exit Check

- [ ] Draw the async FIFO and explain where the only crossing is and why it's safe.
- [ ] Explain Gray pointers and the extra-MSB full/empty trick.
- [ ] Re-derive depth = 32 as a burst-backlog argument and state the three rate cases.
- [ ] Explain why full asserts early / empty asserts late, and why that's safe.
- [ ] List the CDC sign-off flow (structural + RDC + injection + waivers) and why sim alone fails.
- [ ] Write the functional + error/negative test list for an async FIFO.
- [ ] Choose async FIFO vs handshake for a given traffic pattern.

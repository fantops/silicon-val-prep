# Day 39 — Logic Puzzles + Estimation + Reasoning

## Concept

Puzzles test **method, not trivia**. A repeatable approach beats memorizing answers: **restate the constraints → name a strategy → do the math → sanity-check units and bounds**, narrating the whole way. Master a small **catalog of strategies** (invariant, pigeonhole, binary search, work-backwards, extremal, parity, information-theoretic bound, linearity of expectation), a **ready puzzle pack** (including the **100-coins** family that was missed), and **Fermi estimation** — and the "didn't solve" outcome turns into "structured attempt the interviewer can follow."

## Why this day in the flow

This **caps Phase 8 (Reasoning)** and the entire foundation track. Broadcom missed the **100-coin puzzle** and rated logical thinking 5/10; the diagnosis was "improve logical reasoning and interview problem solving." This day deepens §5 of [broadcom_switch_presi_notes.html](../broadcom_switch_presi_notes.html) into a *method* plus the missing 100-coin variants, and doubles as a **behavioral** fix — **thinking aloud, stating assumptions, sanity-checking** is the same owned-narrative fluency that the QC/Nvidia rounds wanted ([INTERVIEW_LOG](../INTERVIEW_LOG.md) gaps). The hardware-flavored puzzles route straight back to [Day 5](day05_boolean_combinational_logic.md)/[Day 9](day09_fsm_sequence_detectors.md).

**Broadcom R2 gap this closes:** 100-coin puzzle (didn't solve), logical reasoning (5/10), thinking-aloud under pressure.

---

## Deep Study Notes

### 1. The method — 4 steps, spoken out loud

```
1. RESTATE   Repeat the constraints in your words. Surface hidden assumptions,
             ask the one clarifying question. (Half of "didn't solve" is mis-read.)
2. STRATEGY  Name the tool before computing: "this looks like an invariant /
             pigeonhole / binary-search / work-backwards / parity problem."
3. MATH      Execute the chosen strategy. Show the steps; keep a running check.
4. SANITY    Check units, bounds, and a tiny case. "n=1 gives… which is right."
```

Interviewers **watch the approach more than the number.** Narrating steps 1–2 is the signal even if you stumble on the math — and it's exactly the "explain assumptions / think aloud" behavior that lands in every round.

### 2. The strategy catalog (one-liner + trigger)

| Strategy | Trigger phrase | Core move |
|---|---|---|
| **Invariant** | "no matter what you do…" | find a quantity that never changes (parity, sum, color) |
| **Pigeonhole** | "must there exist…" | N+1 items in N boxes → two share a box |
| **Binary search / halving** | "minimize worst case", "find among N" | each step halves the search space → `log N` |
| **Work backwards** | "end state given, find start" | reverse the operations from the goal |
| **Extremal** | "always / can you guarantee" | consider the largest/smallest element |
| **Parity / coloring** | tiling, switches, flips | 2-color the board; track odd/even |
| **Information-theoretic bound** | "fewest weighings/questions" | `k` ternary weighings distinguish `3^k`; `k` yes/no → `2^k` |
| **Linearity of expectation** | "expected number of…" | sum per-item probabilities; independence not required |
| **Symmetry** | "fair split", "who wins" | mirror the opponent / exploit a symmetric strategy |

Pick the tool in step 2 *by name* — that's the difference between flailing and a methodical attempt.

### 3. The 100-coins family (the missed problem — know all three)

**(a) Blindfolded coins — split into equal heads** *(the most literal "100-coin puzzle")*
> 100 coins on a table; exactly **K are heads** (you're told K, e.g. 10), rest tails. Blindfolded — you can't see or feel which is which. Make **two piles with the same number of heads.**

**Solution (invariant + flip):** Separate **any K coins** into pile A (the remaining 100−K form pile B). **Flip every coin in pile A.** Done.
*Why:* if pile A happened to contain `h` heads, it had `K−h` tails, and pile B holds the other `K−h` heads. Flipping pile A turns its `h` heads into tails and its `K−h` tails into heads → **pile A now has `K−h` heads = pile B.** The count you don't know (`h`) cancels. *Strategy named: "I'll look for an invariant — the unknown head-count in the separated pile cancels when I flip it."*

**(b) Coin-take game — last to take wins** *(Nim-style)*
> A pile of **100** coins; players alternate taking **1…K** coins; whoever takes the last coin **wins.**

**Solution (modular invariant):** losing positions are **multiples of `K+1`**. Strategy: always leave a multiple of `K+1` for the opponent. With `K=10` → `K+1=11`; `100 mod 11 = 1`, so the **first player takes 1** (leaving 99 = 9×11) and thereafter takes `11 − (opponent's take)` → always wins. If `N` is already a multiple of `K+1`, the **second** player wins. *Strategy: invariant on `N mod (K+1)`.*

**(c) Take-from-ends game — maximize your total** *(optimal play / DP)*
> 100 coins of various values in a **row**; players alternately take from **either end**; maximize your own sum against an optimal opponent.

**Solution (minimax DP):** `best(i,j)` = max coins you can guarantee from subarray `i..j` = `max( v[i] + min(best(i+2,j), best(i+1,j-1)), v[j] + min(best(i+1,j-1), best(i,j-2)) )` — you take an end, opponent then plays optimally (the `min`). Greedy "take the bigger end" is **not** optimal; the DP is. *Strategy: work the recursion from small subarrays up.*

**(d) 100 prisoners & boxes** *(the famous "100" puzzle — often phrased with coins/slips)*
> 100 prisoners, numbers 1–100 on slips inside 100 boxes in random order. Each prisoner opens **50** boxes; **all** must find their own number or all lose. No communication after starting.

**Solution (cycle-following):** each prisoner opens the box with **their own number**, reads the slip inside, opens **that** box next, and follows the chain. This traces a **permutation cycle**; a prisoner succeeds iff their cycle length ≤ 50. **All** succeed iff the permutation's **longest cycle ≤ 50**. Probability ≈ **1 − ln 2 ≈ 31%** (vs `(1/2)^100` for random opening — astronomically small). *Strategy: the structure is a permutation; convert "find my number" into "is the longest cycle ≤ half."*

### 4. The classic pack (have the one-liners; full set in the Broadcom notes)

| Puzzle | Answer + key move |
|---|---|
| **2 eggs, 100 floors** | minimize worst-case drops: first egg at steps 14,13,12…; **14 drops** (smallest `n` with `n(n+1)/2 ≥ 100`) |
| **Burning ropes → 45 min** | rope A both ends + B one end → A done at 30, light B's other end → **45** |
| **12 balls, 1 odd, 3 weighings** | `3³=27 ≥ 24` (12 balls × heavy/light) → find ball **and** which way in **3** |
| **Bridge crossing (1,2,7,10)** | send the two fastest as ferries → **17 min** |
| **Clock angle** | `|30H − 5.5M|°`; 3:15 → **7.5°** |
| **Blue-eyed islanders** | induction on `n`: `n` blue-eyed people leave on night `n` |

These reuse the catalog: eggs = minimize-worst-case (balance), ropes = work-with-rates, 12 balls = information-theoretic `3^k`, bridge = extremal/greedy-with-ferry, islanders = induction.

### 5. Weighing puzzles — the general method

Each balance weighing has **three outcomes** (left-heavy / right-heavy / balanced) → `k` weighings distinguish up to **`3^k`** cases. First compute the **bound** (how many distinguishable outcomes you need), confirm `3^k ≥ cases`, then **design weighings so each outcome partitions the suspects roughly into thirds.** For "find a counterfeit among `N` and say heavier/lighter," you need to separate `2N` cases → `3^k ≥ 2N`. Stating the bound first *is* the senior move.

### 6. Fermi estimation — decompose, bound, multiply, sanity

> "How many X fit in / happen in Y?" → break into factors you can each bound, multiply, then sanity-check the magnitude.

```
e.g. "how many DRAM refreshes per second for a chip?"
  rows per bank × banks × (1 / retention_time)
  ≈ 2^16 rows × 16 banks × (1 / 64ms) → order-of-magnitude answer
```

State each assumption (so a wrong factor is visible and correctable), keep one significant figure, and **end with a sanity check** ("that's ~millions/sec, which feels right for a memory"). Estimation questions grade **decomposition + assumption-stating**, not precision.

### 7. Hardware-flavored aptitude (route back to Phase 1)

Broadcom may dress aptitude as logic: **gate-count minimization** (→ K-maps, [Day 5](day05_boolean_combinational_logic.md)), **"build XOR from NANDs"** (→ [Day 5](day05_boolean_combinational_logic.md) §5, four NANDs), **"detect divisible-by-3 with an FSM"** (→ [Day 9](day09_fsm_sequence_detectors.md): states = remainder mod 3, input = next bit, `next = (2·rem + bit) mod 3`), **ripple vs carry-lookahead trade** (→ [Day 5](day05_boolean_combinational_logic.md) §7). When a "puzzle" is really digital logic, fall back to the truth-table / FSM toolkit and reason from first principles.

### 8. Thinking-aloud discipline (the behavioral half)

The same habit fixes puzzles *and* the owned-narrative gaps from the log:

- **Restate** before solving (catches misreads, buys thinking time).
- **State assumptions explicitly** ("assuming independent draws…") — visible, correctable reasoning.
- **Narrate the strategy** ("I'll try an invariant") before the math.
- **Sanity-check aloud** (units, bounds, `n=1`).
- If stuck: **simplify** (smaller N), **look for symmetry**, **work backwards**, or **try the extreme case** — and *say* you're doing it. A visible, methodical attempt scores even without the final number.

---

## Reasoning traps (the "bugs")

| Trap | Failure | Fix |
|---|---|---|
| **Jump straight to math** | solve the wrong problem | restate constraints first |
| **No strategy named** | flail, no progress to show | pick a tool in step 2 |
| **Anchoring on first idea** | miss the elegant invariant | list 2 strategies before committing |
| **Skip the bound** | brute-force a weighing/search | compute `3^k`/`2^k` bound first |
| **No sanity check** | ship an off-by-magnitude answer | check units, bounds, tiny case |
| **Silent solving** | interviewer can't follow / help | narrate every step |

---

## Interview Explanation (how to approach any puzzle, 60–90 sec)

> "I run a fixed method out loud. First I restate the constraints in my own words and ask the one clarifying question, because a lot of 'didn't solve' is really misread. Then — before any arithmetic — I name the strategy: this smells like an invariant, or pigeonhole, or binary search, or work-backwards, or an information-theoretic bound. For the hundred-coins-blindfolded problem, the move is an invariant: I separate exactly as many coins as there are heads and flip that pile, and the unknown head-count cancels so both piles end equal. For a coin-take game it's a modular invariant — leave a multiple of K-plus-one. For weighing problems I state the bound first: each balance weighing has three outcomes so k weighings distinguish three-to-the-k cases, and I design each weighing to split the suspects into thirds. For estimation I decompose into factors I can bound, multiply, and sanity-check the magnitude. Throughout I narrate assumptions and check a tiny case, because the interviewer is grading the method and a visible, structured attempt scores even when I don't land the exact number."

---

## Architect Drills (attempt cold, narrate aloud, no answer key)

1. 100 coins, **17** heads, blindfolded — make two equal-heads piles. Then explain *why* the unknown cancels.
2. Pile of 100, take 1–10 per turn, last to take **loses** (misère) — does the strategy change? Find the first move.
3. Row of coins `[3,9,1,2]`, take-from-ends, you go first — compute the DP and your guaranteed total.
4. 100 prisoners/boxes — explain cycle-following and why the success condition is "longest cycle ≤ 50." Estimate the probability.
5. 1000 coins, one bag of all-counterfeit (each 1g light), **one** weighing on a scale (not balance) — find the bag.
6. Estimate the number of 64-byte cache lines touched by a `memcpy` of 1 MiB ([Day 16](day16_cache_lines_locality_amat.md)) — decompose and sanity-check.
7. Build a "divisible-by-3" FSM for a serial bit stream ([Day 9](day09_fsm_sequence_detectors.md)) — states, transitions, output.

---

## Coding A — `d39_puzzle_sims` *(optional, light)*

Optionally **verify two puzzles by simulation in C**: Monte-Carlo the **100-prisoners** strategy — random opening (≈0% all-success) vs **cycle-following** (≈31%) — to *see* the `1 − ln 2` result; and a **coin-game** verifier that plays the `N mod (K+1)` optimal strategy against a random opponent and confirms it always wins from a winning position. Full spec in [coding/EXERCISES.md](../coding/EXERCISES.md#day-39--logic-puzzles--estimation-optional). *(Like [Day 40–41](day41_final_mock.md), this day is reasoning-first; coding is optional.)*

**Interview prompts:** Name the strategy before solving. What's the information-theoretic bound for this weighing? What tiny case sanity-checks your answer?

---

## Bug Anchor / cross-links

- **Deepens:** [broadcom_switch_presi_notes.html](../broadcom_switch_presi_notes.html) §5 (aptitude pack) into a method + the 100-coin variants.
- **Routes to Phase 1 for HW-flavored puzzles:** [Day 5](day05_boolean_combinational_logic.md) (XOR-from-NANDs, gate minimization), [Day 9](day09_fsm_sequence_detectors.md) (divisible-by-N FSM).
- **Behavioral tie-in:** the thinking-aloud discipline is the same owned-narrative fix flagged in [INTERVIEW_LOG](../INTERVIEW_LOG.md) (QC CPU Core R2, SysPerf); rehearse with [Day 41](day41_final_mock.md).

---

## Exit Check

- [ ] Recite the 4-step method and apply it to a fresh puzzle, narrating aloud.
- [ ] Solve all four 100-coin variants (blindfolded-flip, take-game, ends-DP, prisoners-cycle) and name each strategy.
- [ ] State the information-theoretic bound for a weighing puzzle before designing it.
- [ ] Do a Fermi estimate with explicit, correctable assumptions and a magnitude sanity-check.
- [ ] Convert a "puzzle" that's really digital logic into a K-map or FSM ([Day 5](day05_boolean_combinational_logic.md)/[Day 9](day09_fsm_sequence_detectors.md)).
- [ ] Demonstrate thinking-aloud: restate → name strategy → math → sanity, even when stuck.

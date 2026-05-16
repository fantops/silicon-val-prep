# Career Decision Matrix — Silicon Validation Cycle (Apr–May 2026)

_Last refreshed: 17 May 2026. Synced with [INTERVIEW_LOG.md](INTERVIEW_LOG.md) (15 rounds / 10 loops) and [offers/](offers/) JD text._

> **Framing rule:** **staying at MSFT is OFF the table.** Current role is workload/OS-level abstraction. Every option below is a HW-depth upgrade. The decision is which depth + which timing + which tradeoffs.
>
> **Status rule:** offers in hand outrank theoretical fit. Active loops outrank cold applications.

---

## Pipeline state (from interview log, 16 May 2026)

| Loop | Role | Status | Next signal |
|---|---|---|---|
| **Intel Bangalore — IP Functional Validation** | post-Si IP val + PCIe + PM/reset/boot | ✅ **SELECTED — offer in progress** | Negotiate TC, get team scope letter |
| **AMD — Core PnP Post-Si** | Zen CPU power & perf post-Si | 🟢 R1 cleared positive | Awaiting next round |
| **Qualcomm — CPU Core (Oryon μarch)** | core pipeline / μarch val | 🟢 R1+R2+HM done | Combined-loop decision pending |
| **Qualcomm — PnP / Thermal (Hyderabad)** | power/perf/thermal sys debug | 🟢 both rounds same day | Feedback pending |
| **Intel Ireland — NPU** | pre-Si + post-Si NPU validation | 🟢 R1 done (rapid-fire) | Feedback pending |
| **Cerebras — WSE** | wafer-scale bringup + perf opt | 🟢 R1 cleared cautiously+ | Next round pending |
| **Amazon Lab126 — AZ1 ML accel SoC** | SoC val pre-Si + post-Si | 🟡 **Recruiter call done — fit-screen in progress** | Awaiting recruiter to confirm match → schedule R1 |
| Intel Power & Perf (Apr) | different Intel team | ❌ rejected R2 | — |
| Renesas | automotive | ❌ R1 — role-fit miss | — |
| Arm Verification | pure DV | 🟡 mismatch | — |
| QC Pre-Si Verification | pure DV | 🟡 mismatch | — |

**Real shape of the decision:** 1 offer in hand + 5 active loops + 1 recruiter-screen pending. The matrix below is now scoring **which active loop to push hardest on**, with Intel Bangalore as the locked-in floor.

---

## Baseline & profile (recap)

| Component | Value |
|---|---|
| Base | ₹35.0 LPA · Bonus 15% · RSU ~₹17L/yr · **All-in ~₹57L** |
| Unvested MSFT RSU | ~$48K (~₹40L) — any offer MUST backfill |

**Skills shipped:** PCIe Gen6 pre-Si (Cambria) · NVMe post-Si bringup · C-states · ADR · Zebu emulation · Perspec · C++ transactors (10+) · RAPL+ETW+WPA · energy-per-inference · EDP · PL1/PL2/Tau · EdgeOptimizer profiler · cross-vendor NPU debug (Intel LNL / AMD XDNA / QC HTP v79) · QNN page-fault RCA · SRD/HMX tile escalation · C/C++/Python · Lauterbach/Metaware/Lecroy.

**Skills NOT shipped:** SystemVerilog/UVM · x86 assembly authoring · core-μarch RTL-level val · Verilog RTL coding · RIS at block level.

**JD-gap reality-check from interview log:**
- QC Oryon CPU Core JD says **12+ yrs** → in practice **you cleared R1+R2+HM with 8 yrs**, so the years requirement is *much* softer than feared. Likely outcome: Senior level (not Staff) with combined-loop decision pending.
- AMD Zen P&P R1 went positive without x86-asm being a blocker. The "x86 assembly programming" preferred-qual is a real but not fatal gap.
- Intel Bangalore IP Functional Validation reads like Cambria → confirmed by selection.

---

## Per-loop deep analysis (status-aware)

### 1. Intel Bangalore — IP Functional Validation · ✅ OFFER IN PROGRESS
**Source:** [offers/intel.md](offers/intel.md) · **Log:** [12 May F2F R1+R2](INTERVIEW_LOG.md)

| Axis | Read |
|---|---|
| JD-fit | **9.5** — JD reads like Cambria (PCIe on Zebu, PM/reset/boot, Lauterbach, 6–10 yrs). They selected you. |
| Depth upgrade vs MSFT | 7.5 — IP-level platform val (HW debug), but lateral to your prior Intel role |
| TC expectation | 6.5 — ₹60–70L band (need to push to ₹72L+ using other offers as leverage) |
| Stability | 6.0 — Intel cutting heads (78.5K vs 97.6K YoY); IP functional val org is core enough to survive |
| Growth | 7.0 — turnaround real (DCAI +22%, NVIDIA picked Xeon for Rubin), but stock vol |
| Brand | 9.0 |
| Round risk now | **0** — already selected |

**Tradeoffs you're accepting if you sign:**
- Lateral on depth (you've done this work at Cambria already).
- Lowest TC of the contenders unless you use AMD/QC as leverage to push it.
- Re-entering an org you left in 2024 — answer "why returning" honestly.
- Smallest skill-step toward what the 21day plan is preparing you for (core-μarch / post-Si CPU).

**What this offer is GREAT for:**
- Zero round-risk floor.
- Negotiation leverage on every other active loop ("Intel has me on offer at X — beat it").
- Hyderabad/Bangalore family-locality.
- Day-1 productive, no ramp tax.

**Action:** **accept the offer letter (don't sign yet), set a 2-week decision window**, use as leverage on AMD/QC/Cerebras/Intel-Ireland in parallel.

---

### 2. AMD — Zen Cores System Validation, Power & Perf · 🟢 R1 POSITIVE
**Source:** [offers/amd.md](offers/amd.md) · **Log:** [14 May R1](INTERVIEW_LOG.md)

| Axis | Read |
|---|---|
| JD-fit | **9.0** — JD = power/perf methodology (RAPL/ETW/WPA, EDP, PL/Tau) you already ship. Multi-SoC PnP design scaffold answer landed in R1. |
| Depth upgrade vs MSFT | 8.5 — HW power/perf on flagship x86 core |
| TC expectation | 7.0 — ₹65–75L |
| Stability | 8.5 — blowout exec (rev +38% YoY, DCAI +57%) |
| Growth | **9.5** — co-leader of AI cycle |
| Brand | 9.0 |
| Round risk now | Medium — likely 2–3 more rounds; x86-asm depth probe is the residual gap |

**R1 evidence (from log):** interviewer framed the role as **"core PnP post-Si — heavy execution + heavy debug after data lands; needs architecture exposure + system understanding"** — this is *exactly* your CV arc (Intel PnP → MS NPU). Multi-SoC PnP design scaffold answer (workload → budget split → rail topology → DVFS coordination → thermal envelope → validation matrix) landed cleanly.

**Residual risks for next rounds:**
- x86 assembly content authoring will come up. Mitigation: rehearse 1 simple test snippet hitting a pipeline corner ("uop fusion didn't happen, here's the rep stos sequence + perf counter").
- L7 vs L6 title — confirm in offer letter.
- TCS Helios India deal makes Bengaluru AMD = strategic, not just back-office.

**What this offer is GREAT for:**
- Lowest ramp tax of any depth-upgrade offer.
- Methodology continuation = day-1 contribution on P&P.
- AI cycle tailwind for 3+ yrs visibility.
- Highest 5-yr trajectory if execution holds.

**Action:** **prep next round around x86-asm authoring + multi-SoC PnP follow-up**. Treat as the leading contender behind Intel-offer-in-hand.

---

### 3. Qualcomm — CPU Core (Oryon μarch) · 🟢 HM DONE, COMBINED PENDING
**Source:** [offers/qualcomm.md](offers/qualcomm.md) · **Log:** [28 Apr R1 + 30 Apr R2 + 12 May HM](INTERVIEW_LOG.md)

| Axis | Read |
|---|---|
| JD-fit | **8.0** — μarch depth is aspirational, but HM cleared with bug-story ownership + system breadth narrative. JD years gap (12+) didn't kill you. |
| Depth upgrade vs MSFT | **9.5** — only seat with true core-μarch RTL/pipeline depth |
| TC expectation | 9.0 — ₹85–91L at Staff; ₹70–80L if downleveled to Senior |
| Stability | 8.5 — $20B buyback, $150M India AI fund |
| Growth | 7.5 — diversification real (Auto/IoT/XR) |
| Brand | 9.0 |
| Round risk now | Combined decision — the *level* is the open question, not the offer |

**HM evidence (from log):** HM pushed *"no CPU-core experience — why hire you?"* and *"if switching for money, don't come to QC"*. You answered with **scope + validation rigor + cross-vendor system view** — exactly the right framing. Money-led answer is an instant red flag at QC; you didn't fall into it.

**Most likely outcome:**
- Offer at **Senior** (not Staff) with combined-loop weighting.
- TC band: ₹70–80L (vs ₹85–91L if Staff).
- Promo path to Staff: 12–18 mo, possibly negotiable in writing.

**Hard rule trigger:** if Senior is offered, **demand written Staff-promo timeline ≤18 mo**. Without it, the years-shipped gap eats your TC AND your title for 2+ yrs.

**What this offer is GREAT for:**
- Highest depth ceiling — core-μarch RTL is the rarest validation skill.
- 21day plan is literally preparing you for this seat.
- Snapdragon vocab from MSFT compounds.

**Tradeoffs:**
- 12–18 mo at bottom of team's experience curve.
- HR push on "why hire you" means you're being weighed against more-experienced candidates — you may be a marginal hire, not a star hire.
- If downleveled, internal promo politics matter.

**Action:** **wait for combined-loop decision. If Staff offered → strong contender against AMD. If Senior offered without promo path → leverage with Intel-in-hand to push to Staff or walk.**

---

### 4. Qualcomm — PnP / Thermal Hyderabad · 🟢 PENDING
**Source:** Not in [offers/](offers/) yet · **Log:** [28 Apr 2 rounds same day](INTERVIEW_LOG.md)

| Axis | Read |
|---|---|
| JD-fit | **8.5** — power/perf/thermal sys debug = your MSFT methodology + L&T thermal scenarios. Closest QC role to your actual shipped work. |
| Depth upgrade vs MSFT | 8.0 — sys-level PnP at vendor side (vs OEM side at MSFT) |
| TC expectation | 8.5 — ₹80–88L (PnP role band, slightly below CPU Core Staff) |
| Stability | 8.5 (same QC) |
| Growth | 7.5 (same QC) |
| Brand | 9.0 |
| Round risk now | Low — both rounds done, just feedback pending |

**R1+R2 evidence (from log):** domain alignment + cross-vendor NPU breadth landed; **thermal control-loop depth and leadership/ownership framing slightly soft**.

**Note:** this is the **sleeper option in the QC stack**. Lower depth ceiling than Oryon μarch, but much higher day-1 fit (your MSFT energy-per-inference work is *literally* this role at vendor side). If both QC loops come back with offers, this is a real choice — not just a fallback.

**Tradeoff vs QC Oryon:**
- PnP Hyderabad: lower depth ceiling, but day-1 productive + likely Staff level not Senior + Hyderabad locality.
- Oryon μarch: highest ceiling, but downlevel risk + Bangalore + ramp tax.

**Action:** if both QC offers land, treat PnP Hyderabad as a serious contender — don't auto-pick Oryon just for prestige.

---

### 5. Intel Ireland — NPU Validation · 🟢 PENDING
**Source:** Day 21 pitch (day21_final_mock_company_tailoring.md) · **Log:** [29 Apr rapid-fire](INTERVIEW_LOG.md)

| Axis | Read |
|---|---|
| JD-fit | **9.0** — pre-Si + post-Si NPU val = literally your day21 pitch. Cross-vendor NPU + EdgeOptimizer story strong; EE fundamentals weak under pace. |
| Depth upgrade vs MSFT | 8.5 — vendor-side NPU IP val (vs OEM-side platform val) |
| TC expectation | Hard to peg — Intel Ireland relocation makes it a different decision (housing/COL/visa) |
| Stability | 6.0 — same Intel cuts risk, but Ireland NPU charter is funded |
| Growth | 7.0 — Intel turnaround |
| Brand | 9.0 |
| Round risk now | Medium — EE fundamentals weak in R1; STAR-strict bug stories needed for next |

**R1 evidence (from log):** ~70–80% landed. Bug story tangled under pace — needs 90-sec timer rehearsal.

**Key tradeoff: RELOCATION.** Bangalore → Leixlip/Shannon is a life decision, not just a job decision. Family, schools, COL (Ireland tax + housing brutal), visa, return-to-India optionality (Ireland Intel cuts could leave you stranded).

**If relocation is OFF the table → Intel Ireland is moot.**

**If relocation is ON the table → it's the closest "close-the-loop" role (pre-Si NPU at design house, after years of post-Si NPU at OEM).**

**Action:** **resolve relocation question first.** If yes, prep EE fundamentals + STAR-bugs for next round. If no, politely close the loop.

---

### 6. Cerebras — Wafer Scale Engine · 🟢 NEXT ROUND PENDING
**Source:** [offers/cerebras.md](offers/cerebras.md) · **Log:** [13 May R1](INTERVIEW_LOG.md)

| Axis | Read |
|---|---|
| JD-fit | **7.5** — test-plan-from-spec scaffold + 2-language coding landed. Verilog/SV partial gap; 100+ core fabric exp not shipped. |
| Depth upgrade vs MSFT | 8.5 — wafer-scale-specific concepts, niche |
| TC expectation | 7.5 — ₹70–80L (post-IPO RSU value uncertain) |
| Stability | **4.0** — post-IPO crash (−28% from May 14 peak), $60B cap on $510M rev, MBZUAI/G42 concentration |
| Growth | 9.0 — highest if survives |
| Brand | 7.0 (rising) |
| Round risk now | Low — R1 cleared cautiously positive |

**R1 evidence (from log):** test-plan scaffold landed; grep-in-Python + moving-avg-in-C edge cases recovered. Closing chat on team-size expansion = positive signal.

**Major hazard:** **the RSU portion of any Cerebras offer is now risky.** Pre-IPO offers were lottery-ticket equity. Post-IPO is a public stock with brutal first-week drawdown. Negotiation MUST anchor to base + cash sign-on, NOT to RSU notional value.

**Action:** continue interview, but go in eyes-open about equity volatility. **Hard rule: 100% sign-on for unvested + ≥1.5× equity refresh trigger if stock −30% in yr 1.** Don't let the recruiter pitch you on the May 14 peak price.

---

### 7. Amazon Lab126 — AZ1 ML Accel SoC · 🟡 RECRUITER FIT-SCREEN IN PROGRESS
**Source:** [offers/amazon.md](offers/amazon.md) · **Log:** recruiter call received 17 May, fit-match assessment in progress

| Axis | Read |
|---|---|
| JD-fit | **8.0** — ML accelerator SoC + Zebu + pre→post lifecycle. **UVM is BASIC qual (4+ yrs), you have 0** — but recruiter already called, so the resume-keyword filter is already past. |
| Depth upgrade vs MSFT | 8.0 |
| TC expectation | 8.0 — ₹78–85L |
| Stability | 9.0 |
| Growth | 9.0 (AWS) |
| Brand | 9.5 |
| Round risk now | **Medium** — recruiter screen passed; HM fit-match next. UVM gap will surface there. |

**Status update (17 May):** recruiter reached out, evaluating fit-match → will schedule R1 if positive. The keyword-screen wall I previously flagged is **partially behind you** — they already have your profile and want to talk. Referral hunt no longer required for entry.

**The remaining wall:** the fit-screen and HM round will probe **SystemVerilog/UVM 4+ yrs**. Your real story:
- Cambria = **C++ transactors on Zebu** (same role-shape, different language — UVM-equivalent IP-level pre-Si).
- MSFT = post-Si NPU val across 3 vendors (closest commercial analog to AZ1).
- Pitch: *"I've shipped pre-Si IP val via C++ transactors on Zebu and post-Si ML-accel val across Intel/AMD/QC NPUs. UVM is the methodology I haven't authored in, but the IP-val problem-shape is identical and I can ramp."*

**Cost/benefit now (recruiter is engaged):**
- Marginal cost dropped sharply — no referral hunt needed, just take the call.
- Marginal value unchanged — AWS brand + Lab126 ML accel + TC bump + stability.
- Real risk: time-sink if HM-screen drops you on UVM. Mitigate by being **upfront about UVM gap in the recruiter fit-call** — let them filter before you invest interview prep cycles.

**Action:** 
1. **Take the recruiter call.** Be explicit: *"I've done IP-level pre-Si val with C++ on Zebu, not UVM. If that's a hard requirement, let's not waste each other's time."* Forces the gate early.
2. If they proceed → request **2-week scheduling buffer** so it doesn't collide with AMD R2 / QC decision window.
3. Prep AZ1-specific story: ML accel pipelines + DMA + Zebu emulation flow.

---

## Per-axis matrix (status-weighted)

| Axis | Intel BLR (✅) | AMD Zen P&P | QC Oryon μarch | QC PnP Hyd | Intel Ireland NPU | Cerebras | Amazon |
|---|---:|---:|---:|---:|---:|---:|---:|
| TC | 6.5 | 7.0 | 9.0/7.5* | 8.5 | varies (reloc) | 7.5 | 8.0 |
| JD-fit | 9.5 | 9.0 | 8.0 | 8.5 | 9.0 | 7.5 | 8.0 |
| Depth upgrade | 7.5 | 8.5 | 9.5 | 8.0 | 8.5 | 8.5 | 8.0 |
| Stability | 6.0 | 8.5 | 8.5 | 8.5 | 6.0 | 4.0 | 9.0 |
| Growth | 7.0 | 9.5 | 7.5 | 7.5 | 7.0 | 9.0 | 9.0 |
| Brand | 9.0 | 9.0 | 9.0 | 9.0 | 9.0 | 7.0 | 9.5 |
| Round-risk now | **0** (offer) | low-med | low (decision pending) | low (decision pending) | medium | low | medium (recruiter engaged) |

*QC Oryon: 9.0 if Staff, 7.5 if Senior without promo path.

---

## Weighted scores (TC .20 · JD .20 · Depth .15 · Stab .15 · Growth .15 · Brand .10 · Round-risk inverted .05)

| Rank | Loop | Score | Status note |
|---:|---|---:|---|
| 1 | **AMD Zen P&P** | **8.40** | R1 positive, fits like a glove, AI tailwind |
| 2 | **QC PnP Hyd** | **8.30** | Sleeper — day-1 fit + Staff level likely |
| 3 | **QC Oryon (if Staff)** | **8.25** | Highest ceiling — IF Staff. Drops to 7.50 if Senior w/o promo path |
| 4 | **Intel Bangalore** | **7.55** | Offer in hand — the floor |
| 5 | **Amazon Lab126** | **7.65** | Recruiter engaged; UVM gap still real at HM screen |
| 6 | **Intel Ireland NPU** | **7.20** | Reloc decision dominates |
| 7 | **Cerebras** | **6.95** | Post-IPO chart penalty |

---

## Decision logic (now, not theoretical)

### Step 1 — Lock the floor (this week)
- **Receive Intel Bangalore offer letter.** Confirm: title, base, sign-on, RSU, team scope, manager, hire date.
- **Hard rule:** require ≥₹40L sign-on or yr-1 accelerated equity to backfill MSFT RSU.
- Do NOT sign. Set verbal "2-week consideration window."
- This becomes your **leverage floor** for every other negotiation.

### Step 2 — Run the active loops to closure (next 2–3 weeks)
Priority order based on score × momentum × time-to-decision:

1. **AMD next round** — highest score + already positive. Prep: x86-asm test snippet, multi-SoC PnP follow-up scaffold, L7 confirmation question.
2. **QC combined decision (both loops)** — already in. Just wait. When offer(s) land, decide level + locality.
3. **Cerebras next round** — anchor negotiation to base + cash, not RSU.
4. **Intel Ireland next round** — **only if relocation is yes.** Otherwise close politely.

### Step 3 — Amazon recruiter gate
- **Take recruiter call this week.** Disclose UVM gap upfront — let them filter early.
- If they schedule R1 → **request 2-week buffer** so it doesn't crash into AMD R2 / QC decision window.
- If they pass → no loss; concentrate on AMD/QC.

### Step 4 — Final compare (when 2+ offers in hand)

Decision tree:

```
Do you have AMD offer in hand?
├── YES → AMD vs Intel-BLR vs QC-whichever-level
│   ├── AMD > Intel-BLR on TC + growth + depth → AMD
│   ├── QC at Staff (with promo path) > AMD on TC + depth ceiling → QC Oryon
│   └── QC at Senior w/o promo path → REJECT, take AMD
├── NO, but QC Staff offer → vs Intel-BLR
│   ├── QC Staff with team scope OK → QC Oryon (highest ceiling)
│   └── Senior w/o promo path → Intel-BLR floor
└── NO from AMD AND QC → Intel-BLR
    ├── Negotiate TC to ₹72L+ using Cerebras/Intel-Ireland as leverage
    └── Sign with team scope letter + re-org protection
```

---

## Hard rules (gates before signing anything)

1. **Backfill ≥₹40L unvested MSFT RSU** — sign-on cash or yr-1 accelerated equity. Non-negotiable.
2. **QC Oryon:** reject Senior without **written Staff-promo path ≤18 mo**.
3. **AMD:** confirm **MTS L7 (not L6)** in offer letter + team scope naming Zen5/Zen6.
4. **Intel Bangalore:** verify team is NOT in ongoing 78.5K→? headcount reduction. Get manager to name FY26–27 programs.
5. **Cerebras:** 100% sign-on for unvested + **≥1.5× equity refresh trigger on −30% stock drop in yr 1**.
6. **Intel Ireland:** resolve relocation/family decision BEFORE next round, not after offer.

---

## Negotiation leverage matrix

| Offer arrives | Use as leverage on | Floor anchor |
|---|---|---|
| AMD ≥₹70L | Intel BLR (push to ₹75L), QC PnP (push to ₹85L) | Intel BLR ₹65L+ |
| QC PnP ₹80–85L | AMD (push title to L7+), Intel BLR (push sign-on) | Intel BLR ₹65L+ |
| QC Oryon Staff ₹85L+ | AMD (TC + growth equity), Intel BLR (sign-on +50%) | Intel BLR ₹65L+ |
| QC Oryon Senior | AMD (use as floor), or walk if no promo path | Intel BLR |
| Cerebras ₹70L+ cash | AMD (use only for cash component, not RSU) | Intel BLR |

---

## What changed from prior matrix

| Prior assumption | Actual (from log) | Impact |
|---|---|---|
| QC 12+ yr basic qual will reject you | Cleared R1+R2+HM with 8 yrs | QC Oryon back in play, downlevel risk replaces reject risk |
| Single QC role tracked | Two QC loops active (CPU Core + PnP Hyderabad) | PnP Hyderabad = sleeper |
| Intel = aspirational | Intel = offer in hand | Floor locked, leverage active |
| Amazon = needs referral, not started | Amazon recruiter called 17 May, fit-screen in progress | Round risk Medium not High; no referral hunt needed |
| AMD x86-asm gap = high risk | R1 cleared positive without it being a blocker | Lower risk; still rehearse |
| Intel Ireland = separate option | Confirmed active; reloc decision dominates | Gate on family decision |
| Cerebras = pre-IPO lottery | Post-IPO crash | RSU value penalized; cash matters more |

---

## Final position (17 May 2026)

**You are in a strong position:**
- 1 offer in hand at a JD-perfect match (Intel BLR)
- 2 high-scoring loops near decision (AMD R1+, QC combined pending)
- 3 secondary loops active (QC PnP Hyd, Cerebras, Intel Ireland)
- 1 recruiter screen in progress (Amazon Lab126)
- Cycle conversion: 6/10 loops landed + 1 new inbound = strong

**The decision is no longer "where to apply" — it's "which active offer to accept and how to negotiate."**

**Most likely best outcome (probability-weighted):**
- AMD lands with ₹70–75L → AMD.
- AMD doesn't land + QC offers Staff → QC Oryon.
- AMD doesn't land + QC offers Senior w/o promo path → Intel BLR with TC negotiated up using QC PnP / Cerebras as leverage.

**Don't:**
- Sign Intel BLR before AMD and QC decisions land (you're throwing away leverage).
- Invest deep Amazon prep before disclosing UVM gap to their recruiter (let them filter first).
- Optimize for QC Oryon prestige if they downlevel without a promo path (the math doesn't work).
- Take Intel Ireland for the role alone if relocation is family-no.
- Anchor Cerebras negotiation on RSU notional value.

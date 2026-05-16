# Career Decision Matrix — Silicon Validation Offers

_Last refreshed: 17 May 2026 with live 2026 market data (Q1'26 earnings, IPO prints, stock action through 16 May 2026)._

## Baseline (current role — MSFT NPU, Bengaluru, 8 yrs exp)

| Component | Value |
|---|---|
| Base | ₹35.0 LPA |
| Target bonus | 15% (~₹5.25L) |
| RSU vest/yr (avg) | ~₹17L |
| **All-in cash + equity** | **~₹57L/yr** |
| Unvested RSU pool | ~$48K (~₹40L) — must be backfilled by any new offer |

---

## Weighting model (per request: TC 25 / JD-fit 25 / Stability 20 / Growth 20 / Brand 10)

Shifted from the prior TC-heavy 30/25/20/15/10 toward JD-fit because:
- All five offers clear the survival TC bar (≥₹60L)
- The 2026 cycle rewards being on the right silicon team, not just the highest-paying
- Day-to-day satisfaction over 3 yrs > one-time signing-bonus delta

## Actual role scope (per user clarification, 17 May 2026)

| Co. | Role | Domain |
|---|---|---|
| Qualcomm | Staff Engineer — **Oryon CPU core instruction-pipeline validation** | Custom Arm CPU (ex-Nuvia), Snapdragon X / Compute / Auto |
| AMD | MTS L7 — **Zen CPU core validation** | Flagship x86 core, EPYC + Ryzen |
| Intel | G7/G8 — **server validation (Xeon / DCAI)** | Server-class CPU + platform |
| Amazon | Lab126 L5 — devices / SoC integration | Echo / Kindle / Fire silicon integration |
| Cerebras | Wafer-scale validation | WSE / CS-3 systems |

**Important context for JD-fit scoring:** your 8 yrs is NPU validation. QC Oryon / AMD Zen / Intel Xeon are all **CPU pipeline pivots**, not NPU continuations. Skills transfer well (pipeline hazards, OoO, branch pred, perf counters, memory ordering) but it is genuinely a level-up in core-μarch depth, not a lateral move.

---

## 2026 Market Sentiment Snapshot (IR / CNBC, May 2026)

| Co. | Q1 2026 Headline | Stock / Capital Signal | Direction |
|---|---|---|---|
| **AMD** | Rev **$10.3B (+38% YoY)**; DCAI **+57%**; FCF $2.57B; Q2 guide $11.2B (+46%) | Co-leader of AI cycle | ▲▲▲ |
| **Qualcomm** | Q2 FY26 reported Apr 29; **$20B buyback + dividend hike** Mar 17; **$150M India AI Venture Fund** Feb 18 | Confident capital return; diversification real (Auto/IoT/XR) | ▲▲ |
| **Amazon (AWS)** | AWS AI run-rate **>$15B**; Bedrock spend **+170% QoQ**; Trainium chips **+40% QoQ**; Anthropic adds 5 GW of Trainium | Hyperscaler at peak form | ▲▲▲ (but Lab126 ≠ AI story) |
| **Intel** | Rev **$13.6B (+7%)**; **DCAI +22%**; non-GAAP EPS $0.29 (+123%); 6th straight beat under Lip-Bu Tan; NVIDIA picked Xeon for DGX Rubin NVL8; Google multiyear + custom IPU | INTC $108.77 (−6.2% on 16 May, but >5× US govt's Aug '25 entry at $20.47). Headcount **78.5K vs 97.6K YoY** — still cutting | ▲ Improving but volatile |
| **Cerebras** | IPO'd **13 May 2026 @ $185** → spiked **$386** → **$279 (−10%) on 15 May**; mkt cap **$60B on ~$510M rev (118× sales)**; Cramer publicly flagged the spike "wrong" | Post-IPO bubble unwind in progress; MBZUAI 62% + G42 24% customer concentration unchanged | ▼ |

---

## Per-axis scores (0–10)

| Axis | Qualcomm Staff | AMD MTS L7 | Amazon Lab126 L5 | Cerebras | Intel G7/G8 |
|---|---:|---:|---:|---:|---:|
| **TC** (₹ all-in vs ₹57L base) | 9.0 (₹85–91L) | 7.0 (₹65–75L) | 8.0 (₹78–85L) | 7.5 (₹70–80L) | 6.5 (₹60–70L) |
| **JD-fit** (CPU pipeline val pivot from NPU bg) | 8.5 (Oryon custom Arm core, ex-Nuvia, hot team but young) | 9.0 (Zen flagship x86, biggest scope, mature org) | 6.5 (Lab126 = devices/SoC integration, weakest fit) | 7.5 (wafer-scale; world-class but niche) | 8.5 (Xeon server val; broad scope but less core-μarch than Zen/Oryon) |
| **Stability** (2026-refreshed) | 8.5 ($20B buyback) | 8.5 (blowout exec) | 9.0 (AWS funds everything) | 4.0 (post-IPO crash + 2-customer concentration) | 6.0 (turnaround real, layoffs ongoing) |
| **Growth** (2026 momentum) | 7.5 (Auto/IoT diversification) | 9.5 (+38% rev, +57% DCAI) | 9.0 (AWS AI $15B run-rate) | 9.0 (highest upside if survives) | 7.0 (6 beats but bumpy) |
| **Brand** | 9.0 | 9.0 | 9.5 | 7.0 | 9.0 |

---

## Weighted scores (TC .25 · JD .25 · Stab .20 · Growth .20 · Brand .10)

| Rank | Offer | Weighted | Notes |
|---:|---|---:|---|
| 1 | **AMD MTS L7 (Zen)** | **8.50** | Flagship x86 core, maximum scope, +38% rev tailwind, TCS Helios India deal |
| 2 | **Qualcomm Staff (Oryon)** | **8.48** | Custom Arm core, ex-Nuvia prestige; lower growth axis than AMD pulls it down 0.02 |
| 3 | **Amazon Lab126 L5** | **8.18** | High TC + Brand, but JD-fit weak (devices ≠ CPU core) |
| 4 | **Intel G7/G8 (Xeon server)** | **7.25** | Server scope is broad; TC + headcount cuts still drag |
| 5 | **Cerebras** | **7.05** | Highest growth axis; stability collapsed post-IPO |

**#1 → #2 margin is 0.02 — effectively a tie.** Decide on team / manager / scope, not the score.

---

## Hard rules (unchanged)

1. **Reject any offer that does not backfill ≥₹40L unvested MSFT RSU** (sign-on or accelerated yr-1 vest).
2. **Reject Qualcomm "Senior" w/o written Staff-promo ≤18 mo** — matrix above assumes Staff.
3. **Reject Intel/AMD offer in a team scheduled for FY26 re-org** — confirm team scope letter before signing.
4. **Cerebras: require liquid sign-on covering 100% of unvested AND ≥1.5× equity refresh trigger if stock −30% in yr 1** — the chart says you need this.

---

## Scenarios

### A) Maximize lifetime EV + craft depth (risk-tolerant)
**AMD MTS L7 (Zen)** — flagship x86 core val is the most prestigious CPU-μarch seat in the industry right now. TCS Helios India deal makes local investment explicit. ₹15–20L TC gap vs QC is recoverable in 2 promo cycles given +38% rev momentum.

### B) Maximize TC + brand + capital safety (risk-averse)
**Qualcomm Staff (Oryon)** — virtual tie with AMD on score, wins on TC by ₹15–20L. Oryon is ex-Nuvia talent; pipeline-val seat on a young custom Arm core is rare and high-leverage. $20B buyback + India AI fund = strong local commitment.

### C) Best resume + horizontal optionality
**Amazon Lab126 L5** — AWS halo + Amazon L5 filter. Accept the JD-fit hit; pays off if you ever pivot to AWS Silicon (Trainium/Inferentia) internally. Weakest pure-CPU fit of the five.

### D) Server-class breadth
**Intel G7/G8 (Xeon server)** — broadest validation scope (platform + core + memory + IO). Only sign if (i) team is on Xeon 6+ / Rubin-host roadmap, (ii) TC negotiated to ≥₹72L, (iii) you can tolerate restructuring noise.

### E) Lottery ticket
**Cerebras** — Only if sign-on fully de-risks unvested AND you genuinely want wafer-scale exposure. Equity is no longer the moonshot it was 60 days ago.

---

## Negotiation checklist

- [ ] Base ≥ **₹50L** for QC/Amazon; ≥ **₹45L** for AMD/Intel/Cerebras
- [ ] Sign-on ≥ **₹40L cash** OR equivalent yr-1 accelerated equity
- [ ] RSU refresh year-on-year (not one-shot grant)
- [ ] Title locked in offer letter (Staff / MTS / L5 — not "to be confirmed")
- [ ] Team + manager + product roadmap named in offer addendum
- [ ] Re-org protection: 12-mo severance trigger if team disbanded
- [ ] Relocation / WFH clause if applicable
- [ ] **QC:** written Staff-promo timeline if extended at Senior
- [ ] **Cerebras:** equity refresh trigger on −30% stock drawdown
- [ ] **Intel:** confirm role is NOT in a unit slated for further headcount reduction

---

## Verdict

- **Primary: AMD MTS L7 — Zen core val (8.50)** — best JD scope; AI cycle puts DCAI/EPYC in the spotlight for 3+ yrs.
- **Virtual tie: Qualcomm Staff — Oryon pipeline val (8.48)** — wins on TC by ₹15–20L; choose this if you weight cash > scope, or if QC team/manager interview impresses more.
- **Tiebreaker between AMD vs QC:** ask each team to walk you through a recent core-pipeline bug they shipped a fix for. Whoever gives the more honest, technical answer is your team.
- **Walk-away floor: Intel Xeon server (7.25)** — only sign if TC AND team are both negotiated up.

## Pivot risk note (NPU → CPU core)

QC Oryon, AMD Zen, and Intel Xeon all move you off the NPU track. That is fine — pipeline-val skills transfer — but be deliberate:
- After 2–3 yrs in CPU-core val, returning to NPU val is easy; the reverse pivot is harder.
- If long-term you want to stay in AI accelerator silicon (NPU/GPU/wafer-scale), **AMD** keeps that door open (Instinct adjacency) better than **QC Oryon** (CPU-only) or **Intel server** (CPU-only).
- Cerebras is the only offer that is purely AI-accelerator. If AI-accel is the long-term identity, Cerebras + a hard sign-on becomes more interesting than its raw score suggests.

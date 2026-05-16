# Career Decision Matrix — 5 Active Roles

_17 May 2026. Scope: **only the 5 roles you're actively evaluating.** QC PnP Hyderabad and Intel Ireland parked until feedback._

**Rule:** MSFT is out (workload abstraction, no HW depth). Decision = which of these 5 to push hardest on.

---

## 1. Status snapshot

| # | Role | Status | Next |
|---|---|---|---|
| 1 | **Intel Bangalore** — IP Functional Val | ✅ **OFFER IN PROGRESS** | Get letter, don't sign |
| 2 | **AMD** — Zen Core P&P Post-Si | 🟢 R1 positive | Next round |
| 3 | **QC CPU Core** (Oryon μarch) | 🟢 R1+R2+HM done | Combined-loop decision pending |
| 4 | **Amazon Lab126** — AZ1 ML SoC | 🟡 Recruiter fit-screen | Awaiting schedule |
| 5 | **Cerebras** — WSE | 🟢 R1 cleared | Next round |

---

## 2. Master matrix (all axes side-by-side)

Scoring 1–10. **Bold = best in row.**

| Axis | Intel BLR | AMD | QC CPU Core | Amazon | Cerebras |
|---|---:|---:|---:|---:|---:|
| TC (₹ all-in expected) | 65–70L · 6.5 | 70–75L · 7.0 | 85L Staff / 75L Sr · **9.0 / 7.5** | 78–85L · 8.0 | 70–80L · 7.5 |
| JD-fit (CV vs JD) | **9.5** | 9.0 | 8.0 | 8.0 | 7.5 |
| Depth upgrade vs MSFT | 7.5 | 8.5 | **9.5** | 8.0 | 8.5 |
| Stability (12-mo layoff risk) | 6.0 | 8.5 | 8.5 | **9.0** | 4.0 |
| Growth (3-yr trajectory) | 7.0 | **9.5** | 7.5 | 9.0 | 9.0 |
| Brand | 9.0 | 9.0 | 9.0 | **9.5** | 7.0 |
| Round-risk remaining | **0** (offer) | low-med | low (decision pending) | medium (UVM gap) | low |
| Time-to-offer | **now** | 3–4 wks | 1–2 wks | 4–6 wks | 3–4 wks |
| Locality | BLR | BLR | BLR | BLR | BLR |
| Family/reloc disruption | none | none | none | none | none |

---

## 3. Gap matrix (what's missing in your CV per role)

| Skill | Intel BLR | AMD | QC CPU Core | Amazon | Cerebras |
|---|:-:|:-:|:-:|:-:|:-:|
| SystemVerilog / UVM | — | — | — | **❌ hard** | ⚠ partial |
| x86 assembly authoring | — | ⚠ soft | — | — | — |
| ARM/Oryon μarch depth | — | — | ⚠ soft | — | — |
| Core-μarch RTL val | — | — | ⚠ soft | — | — |
| Verilog RTL coding | — | — | — | — | ⚠ soft |
| Years (vs JD floor) | 8 vs 6+ ✅ | 8 vs 7+ ✅ | 8 vs **12+** ⚠ (soft in practice) | 8 vs 4+ ✅ | 8 vs 7+ ✅ |
| PCIe / Zebu / PnP / NPU | ✅ | ✅ | partial | ✅ | partial |

**Read:** Amazon's UVM gap is the only **hard** miss. QC's 12+yr requirement proved soft (HM cleared). AMD's x86-asm is rehearsable.

---

## 4. Weighted score

Weights: TC .20 · JD-fit .20 · Depth .15 · Stability .15 · Growth .15 · Brand .10 · Round-risk-inverse .05

| Rank | Role | Score | Note |
|---:|---|---:|---|
| 1 | **AMD** | **8.40** | Best 3-yr trajectory, day-1 fit, AI tailwind |
| 2 | **QC CPU Core** (if Staff) | **8.25** | Drops to **7.50 if Senior w/o promo path** |
| 3 | **Amazon** | **7.65** | High brand+stability, UVM gap at HM is the gate |
| 4 | **Intel Bangalore** | **7.55** | Floor. Zero risk. Lateral depth. |
| 5 | **Cerebras** | **6.95** | Post-IPO stock crash penalizes RSU TC |

---

## 5. Tradeoff matrix

| Role | You GAIN | You GIVE UP / RISK |
|---|---|---|
| Intel BLR | Zero risk, day-1 productive, ≥₹40L sign-on possible | Lateral depth, same employer you left, smallest skill-step |
| AMD | Highest 3-yr trajectory, P&P methodology fit, AI tailwind | x86-asm gap probe, L7-vs-L6 unclear, more rounds |
| QC CPU Core | Highest depth ceiling, μarch RTL, Snapdragon vocab | Downlevel → Senior risk, 12–18mo ramp, marginal-hire optics |
| Amazon | AWS brand, ML accel charter, stability | UVM gap at HM (real reject risk), longest timeline |
| Cerebras | Niche depth (wafer-scale), highest upside if survives | Post-IPO crash, RSU risky, concentration risk |

---

## 6. Hard rules (gates before signing)

| Role | Non-negotiable |
|---|---|
| **Any offer** | ≥₹40L sign-on to backfill MSFT unvested RSU |
| Intel BLR | Team scope letter naming FY26–27 programs + manager confirms NOT in HC-reduction cohort |
| AMD | **MTS L7** confirmed in writing + Zen5/Zen6 team scope |
| QC CPU Core | If Senior → **written Staff-promo path ≤18 mo**, else walk |
| Amazon | Disclose UVM gap in recruiter call; let them filter before deep prep |
| Cerebras | 100% sign-on for unvested + ≥1.5× equity refresh trigger if stock −30% yr-1 |

---

## 7. Decision logic

```
Step 1: Lock Intel BLR offer letter (do NOT sign). 2-week window.
Step 2: Run AMD + QC + Amazon + Cerebras to closure.
Step 3: When 2+ offers in hand, compare:

  AMD ≥ ₹70L + L7              →  AMD
  QC Staff + team scope OK     →  QC CPU Core
  QC Senior w/o promo path     →  REJECT
  Amazon clears HM             →  compare TC vs AMD
  Cerebras                     →  cash component only as leverage

  Default if AMD + QC both fail →  Intel BLR with TC negotiated up
```

---

## 8. Leverage flow

| When offer X lands | Use to push |
|---|---|
| Intel BLR ✅ | All other 4 — anchors "beat this floor" |
| AMD ₹70L+ | QC (push to Staff), Amazon (push TC) |
| QC Staff ₹85L+ | AMD (push L7 + RSU), Intel (push sign-on) |
| Amazon offer | AMD (TC), QC (cash component) |
| Cerebras cash sign-on | AMD/QC (sign-on only) |

---

## 9. Push priority

1. **AMD** — push hardest on next round.
2. **QC CPU Core** — wait for combined decision; gate on level.
3. **Amazon** — take recruiter call; disclose UVM upfront.
4. **Intel BLR** — get letter, hold as floor.
5. **Cerebras** — continue, anchor on cash not RSU.

# Career Decision Matrix — Silicon Validation Offers (May 2026)

> Source of original analysis: a Claude conversation (15–16 May 2026). Numbers below have been **independently cross-checked** against Levels.fyi and AmbitionBox (India) for May 2026. Corrections to the original conversation are flagged inline.

---

## 1. Candidate Context (baseline)

| Item | Value |
| --- | --- |
| Current employer | Microsoft (NPU validation) |
| Experience | ~8 years |
| Base | ₹35 LPA |
| Bonus target | 15% (~₹5.25 LPA) |
| Stock value | ~$20–21K/yr at MSFT $455 (~₹17 LPA) |
| **Current all-in TC** | **~₹57 LPA** (rising to ~₹56–60 LPA via sign-on for next ~2 yrs) |
| Unvested at MSFT | ~$48K (~₹40 LPA at switch) |
| Floor target on new role | Match Intel discussion (~₹50–55 base + bonus + RSU top-up of $48K) |

Companies still in play (Broadcom removed — role filled):
**Qualcomm · AMD · Amazon Lab126 · Cerebras · Intel (floor / reference)**

---

## 2. Verified Comp Data (India, 8 yrs experience, May 2026)

All figures are total compensation (TC) per year, ₹ LPA. Stock annualised across vest schedule. Sign-on shown separately.

| Company | Level (target) | Base | Stock/yr | Bonus | **TC (ongoing)** | Sign-on — **realistic India** | Source |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Qualcomm | Staff HW Engr | 55–60 | 22–28 | 5–8 | **~85–91** | **3–8** (up to 10–12 with strong counter) | [Levels.fyi avg ₹91.3L][q1]; AmbitionBox Staff 9–12y ₹50–56L base[q2] |
| AMD | MTS L7 (Post-Si CPU P&P) | 40–45 | 18–22 | 5–6 | **~65–75** | **5–10** | [Levels.fyi MTS L7 avg ₹68L][a1]; recent 8y MTS ₹70L |
| Amazon Lab126 | HW Engr II L5 (AZ1 NPU) | 48–55 | 25–30 (backloaded) | 2–3 | **~78–85 steady-state** | **20–30 (Y1) + 15–22 (Y2)** typical India L5 hw; total ₹35–50L over 2 yrs | [Levels.fyi HW II L5 avg ₹79.9L][am1]; vest 5/15/40/40 |
| Cerebras | HW / Silicon Engr (India) | 45–55 | 20–25 | 5–7 | **~70–80** (est.) | **Unknown** — treat as 0 until LOI | Unverified — limited India data points; treat ±15% |
| Intel (floor) | Grade 7 / Sr Val Engr | 38–45 | 8–12 | 4–6 | **~50–65** + RSU top-up | **3–8 cash + explicit ~$48K unvested-RSU match** (standard Intel India practice) | [Levels.fyi Grade 7 avg ₹49.7L][i1]; Grade 8 Staff ₹77.7L |

> **Sign-on reality check:** the original chat numbers (Qualcomm ₹8–10L, AMD ₹15L, Amazon ₹32L+₹27L = ₹59L total) skewed to the upper band of what's negotiable with strong competing offers — closer to US patterns than India defaults. The ranges above are **typical India sign-on for 8-yr Staff/MTS hires**. Anchor your expectations here; treat anything higher as a successful negotiation outcome, not a baseline.

[q1]: https://www.levels.fyi/companies/qualcomm/salaries/hardware-engineer/locations/india
[q2]: https://www.ambitionbox.com/salaries/qualcomm-salaries/staff-engineer
[a1]: https://www.levels.fyi/companies/amd/salaries/hardware-engineer/locations/india
[am1]: https://www.levels.fyi/companies/amazon/salaries/hardware-engineer/locations/india
[i1]: https://www.levels.fyi/companies/intel/salaries/hardware-engineer/locations/india

### Corrections to original Claude analysis

| Claim in original chat | Verified reality | Impact |
| --- | --- | --- |
| "Qualcomm Staff ~₹72 LPA, barely above Intel" | Levels.fyi median Staff HW = **₹91.3L** (range ₹85–95L); ₹72L is likely a **Senior** offer, not Staff | Qualcomm's financial rank is higher than the chat suggested; downgrade-to-Senior concern is more material than the Staff number itself |
| "Amazon Y3 dips to ₹64–92L (cliff)" | Amazon vesting is **5 / 15 / 40 / 40** (backloaded). Y3 is the **peak** stock year, not a cliff. Sign-on tapering is offset by ramp-up in stock vest | Amazon's 4-year TC trajectory **rises monotonically**, not dips. Negotiation focus should still be on RSU grant size, but no cliff risk |
| "AMD MTS at 8 yrs ≈ ₹82 LPA" | Verified MTS L7 8-yr submissions: ₹65–75L. ₹82L is achievable only with strong refresh + bonus or Sr MTS L8 (which is more like 11–13 yrs) | AMD's finance rank slips slightly below Amazon and Qualcomm on ongoing TC, but is still #2–3 |
| "Intel Sr Val Engr India ₹11–25 LPA" | AmbitionBox role-specific page shows ₹18–27L but **excludes RSU & sign-on**. Levels.fyi Grade 7 actual TC ₹49.7L, Grade 8 Staff ₹77.7L | Intel is materially better than the AmbitionBox slice suggests; ₹60–70L all-in is realistic at Grade 7/8 |
| "Cerebras Bengaluru ~₹70 LPA" | Not independently verifiable from public sources in this check | Treat as estimate; insist on a written offer breakdown before relying on it |

---

## 3. Multi-axis Ranking (recalibrated)

Scoring 1–10; weighted: Tech 25%, Learning 20%, AI-defensibility 15%, Stability 15%, Value-adder 10%, Finance 15%.

| Company | Tech depth | Learning curve | AI defensibility | Stability | Value-adder | Finance (ongoing TC) | **Weighted score** | **Rank** |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Qualcomm (Staff, CPU pipeline) | 9 | 9 | 8 | 7 | 8 | 7 (₹85–91L) | **8.1** | **#1** |
| AMD (MTS, CPU core P&P) | 8 | 8 | 9 (perf/W → AI infer) | 7 | 9 (unique skill) | 7 (₹65–75L) | **7.9** | **#2** |
| Amazon Lab126 (AZ1 NPU, full cycle) | 8 | 7 | 8 | 9 | 7 | 8 (Y1 ₹100L+ w/ sign-on, steady ~₹80L) | **7.7** | **#3** |
| Intel (Grade 7/8, IPU/network accel) | 7 → 8 (after JD) | 6 | 6 → 7 | 6 (layoffs) | 6 | 6 (₹60–70L all-in) | **6.7** | **#4** |
| Cerebras (HW, India) | 7 | 8 | 8 | 4 (startup) | 7 | 7 (₹70–80L, est.) | **6.8** | **#4 tied** |

**Key:** Qualcomm wins on the technical axes that match your stated goals; AMD is the closest substitute and is **not** a consolation prize — CPU power/perf is a distinct, AI-relevant specialisation. Amazon wins on Y1 cashflow and full-cycle ASIC breadth.

---

## 4. Switching Economics (vs MSFT today) — corrected sign-on

Y1 **net** TC after writing off the ~₹40L unvested forfeiture, using realistic India sign-on ranges. Stock contribution in Y1 uses the actual vest fraction (Amazon 5%; others 25–33%).

| Scenario | Base + bonus | Y1 stock vest | Sign-on (typ.) | Y1 gross | – Forfeit | **Y1 net** | vs MSFT (₹57L) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Qualcomm Staff (Levels.fyi median) | 60 + 7 | ~8 (33%) | 5 | **~80** | –40 | **~40** | **–₹17L** |
| Qualcomm **Senior** (downgrade) | 45 + 5 | ~6 | 4 | **~60** | –40 | **~20** | **–₹37L (bad)** |
| AMD MTS L7 | 42 + 5 | ~5 (25%) | 7 | **~59** | –40 | **~19** | **–₹38L (worst)** |
| Amazon Lab126 (typical India L5 hw) | 52 + 3 | ~1.5 (5% Y1) | 25 + 18 next yr | **~82** Y1 | –40 | **~42** | **–₹15L** |
| Cerebras (assume zero sign-on) | 50 + 6 | ~6 (25%) | 0 | **~62** | –40 | **~22** | **–₹35L** |
| Intel Grade 7/8 (base + sign-on + RSU match) | 45 + 5 | ~4 | 5 cash + ~40 RSU match grant (vests over 3 yr, ~13 in Y1) | **~67** | –40 | **~27** | **–₹30L** |

> **Corrected takeaway:** with realistic India sign-on, **every option takes a meaningful Y1 hit** (₹15–38L vs current MSFT). The only structures that come close to covering the forfeiture are **Amazon's split Y1+Y2 sign-on** and **Intel's RSU-match grant** (because the match itself is a multi-year RSU, not cash). Amazon is the **least painful Y1**, not a Y1 win.
>
> The decision is therefore **almost entirely a Y2+ trajectory and learning bet**, not a Y1 cash bet. Anyone selling a switch on Y1 numbers is misframing it.

---

## 4a. The Real Tradeoff — All Four Are 7+/10 Technically. So What Decides?

Given Qualcomm 9/10, AMD/Amazon/Intel/Cerebras all 7–8/10, the technical gap is **smaller than the variance on the other axes**. A 1-point technical edge does **not** automatically win — it has to clear the cost of the other axes it drags.

### Decision rubric — six axes, ranked by how much they actually move the needle for *you*

| Axis | Why it matters | Best | Worst | Spread |
| --- | --- | --- | --- | --- |
| **1. Level (Staff vs Senior)** | A downgrade costs 2–3 yrs of negotiating leverage on the next move. Bigger than any pay axis. | QC Staff, AMD MTS L7, Intel Grade 8 | QC Senior (if downgraded), Intel Grade 7 | **Huge** |
| **2. Skill compounding (5-yr resume value)** | What can you say at the *next* interview? CPU pipeline depth, perf/W, custom-NPU bringup are all distinct currencies. | QC (compute arch), AMD (perf/W → AI infer), Amazon (full-cycle custom NPU) | Intel (process overhead), Cerebras (single-product risk) | **Large** |
| **3. Y2+ TC trajectory** | Y1 is sunk; what matters is the slope. Refresher RSUs + hike cycle. | Amazon (peak vest Y3+Y4), QC Staff, AMD | Intel (slow hikes), QC Senior | Medium |
| **4. Stability / layoff risk** | A 12-month layoff erases any TC delta. | Amazon (9), QC (7), AMD (7) | Cerebras (4), Intel (6, recent layoffs) | Large for bottom two |
| **5. AI defensibility** | If 2027–28 is more AI-shaped, which skill ages best? | AMD (perf/W), QC (NPU on Snapdragon), Amazon (NPU), Cerebras | Intel (network IPU, indirect) | Medium |
| **6. Y1 cash** | Real but recoverable. Don't let it dominate. | Amazon (split sign-on) | AMD, Cerebras (low sign-on) | Smaller than it feels |

### How the picks change as you re-weight axes

| If you weight most heavily… | Pick | Why |
| --- | --- | --- |
| Pure technical (axis 2 + axis 1) | **Qualcomm Staff** | Only option where the 9/10 isn't dragged by any sub-7. **Reject if downgraded to Senior** — that drops it to 6/10 effective. |
| AI-skill defensibility (axis 5 + 2) | **AMD MTS** | Power/perf on CPU core is the scarcest, most AI-transferable skill on the table. The 1-point gap to QC is the smallest possible cost for the largest defensibility gain. |
| Y1 cash + breadth (axis 6 + 3) | **Amazon Lab126** | Only package that survives the ₹40L forfeiture reasonably. Full-cycle ASIC validation is a unique breadth bet. |
| Stability + floor protection (axis 4 + 6) | **Intel** | Only as a floor / leverage tool. The RSU match makes the Y1 number competitive; the role doesn't justify it as a primary pick. |
| Upside / venture (axis 5 + 3) | **Cerebras** | Only if you can absorb the stability discount and have written equity terms. |

### The honest tradeoff equation

For each non-Qualcomm option, the question is:

> **Is the (Qualcomm advantage on axis 2) bigger than (this company's advantage on axes 3–6)?**

- **AMD vs QC:** AMD gives up ~1 point on architecture depth and ~₹15–20L ongoing TC, but gains a *more scarce* AI-perf/W skill and a more distinct resume line. **Tradeoff is roughly even** — defensible either way, decide on which technical world you want to live in.
- **Amazon vs QC:** Amazon gives up ~1 point on pipeline depth and learning ceiling, but gains the **best Y1 cash, the best stability, and the only full-cycle custom-NPU exposure** in the set. **Tradeoff favours Amazon if Y1 cash matters or if QC offers Senior.**
- **Intel vs QC:** Intel gives up ~2 points on technical and ~₹25L on ongoing TC, gaining only a moderate RSU-match cash bridge and PCIe/IPU systems depth. **Tradeoff favours QC clearly** unless Intel counters above ₹75L all-in.
- **Cerebras vs QC:** Cerebras gives up ~2 points on stability for ±0 on technical and unknown on cash. **Tradeoff favours QC** unless Cerebras puts written equity terms on the table.

### One-line decision

> **Qualcomm Staff if not downgraded. AMD MTS if QC stalls or downgrades. Amazon if Y1 cash dominates or you want full-cycle ASIC breadth. Intel only as leverage. Cerebras only with written equity proof.**

---

## 5. Tradeoff Limits (how much pay can you absorb for tech?)

| Scenario | Min acceptable TC | Condition |
| --- | --- | --- |
| Qualcomm **Staff** | ₹78L | Acceptable as-is; this is the case for Qualcomm |
| Qualcomm **Senior** (downgrade) | ₹75L + **written Staff promo within 18 mo** | Otherwise reject; the level reset costs 2–3 yrs of negotiating leverage |
| AMD MTS | ₹70L | Acceptable; learning + AI-perf/W specialisation justifies parity with current MSFT |
| Amazon Lab126 | ₹85L steady-state (not just Y1 with sign-on) | Push for RSU grant size, not sign-on top-up |
| Cerebras | ₹85L + clarity on IPO/funding runway | Stability discount of ~15% required |
| Intel | ₹65L all-in | Only viable as a floor / leverage tool; not a primary career bet at this stage |

---

## 6. Role Content — Why Each Job Builds Different Muscle

- **Qualcomm Staff (CPU pipeline / corner cases):** deepest architecture exposure, pipeline trace, working directly with CPU team. Long-term compounding on compute-architecture intuition.
- **AMD MTS (CPU core P&P post-Si):** generates frequency-voltage curves, debugs power-delivery failures, feeds back to power arch. **Most directly transferable to AI inference perf/watt** — a genuinely scarce skill.
- **Amazon Lab126 (AZ1 Gen-2 NPU):** full-cycle ASIC validation on a *custom* ML accelerator (Zebu emulation + HAPS + post-Si bringup + baremetal drivers). Breadth is unusual; product-driven so depth is bounded by ship cadence.
- **Intel (IPU / server network accelerator IP):** IP-level functional validation, PCIe Gen5/6 + power management + boot/reset on server accelerator silicon. Strong on system/interconnect depth; capped on learning by Intel's process overhead.
- **Cerebras:** large wafer-scale silicon — different problem class entirely (training, not edge). High variance on stability and team scaling.

---

## 7. Negotiation Notes

1. **Lead Intel discussions with a competing offer in hand.** Headcount approval ≠ ceiling. AMD ₹85L on paper unlocks Intel refresh-RSU / retention bonus.
2. **Push Amazon on RSU grant size**, not sign-on top-up. Sign-on covers the MSFT forfeiture; grant size compounds for 4+ years.
3. **For Qualcomm**, surface the level question early. If they signal Senior, ask for the written Staff-promo path before counter-offer drops.
4. **PCIe gap for Intel JD** — your post-Si power management depth (C-states, ADR/eADR, PkgC6, boot flow) maps cleanly; emphasise it. Read up on PCIe Gen5 link training, equalisation, signal-integrity basics before the loop.
5. **The 48K unvested is leverage**, not just a cost. Every recruiter should hear "I'm walking away from $48K — show me how the package recognises that."

---

## 8. Decision Hierarchy

1. **Qualcomm Staff** (if not downgraded and TC ≥ ₹78L) → take it.
2. **AMD MTS** (if Qualcomm stalls or downgrades) → equally defensible, different but adjacent skill.
3. **Amazon Lab126** (if Y1 cash matters or the AZ1 custom-NPU pull is strong) → strongest financial switch in Y1.
4. **Intel Grade 7/8** (only with strong counter-offer leverage) → floor, not target.
5. **Cerebras** — only if a written offer beats AMD by ≥ ₹15L AND you accept startup variance.

---

## 9. Data Quality Notes

- **Levels.fyi** India samples for hardware engineers are smaller than US samples; expect ±10% variance on the median.
- **AmbitionBox** role-specific pages frequently **exclude RSU/sign-on** (especially for Indian payslip-only contributors), so they understate FAANG/semi TC by 30–50%. Use Levels.fyi as the primary anchor for TC; use AmbitionBox for base-only sanity check.
- **Cerebras India** — sparse public data; numbers above are estimates only. Insist on a written offer before relying on them.
- **All numbers are May 2026 snapshots** and will drift; refresh before any final negotiation.

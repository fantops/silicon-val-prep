# Career Decision Matrix — Silicon Validation Offers (May 2026)

**Baseline:** MSFT, NPU val, 8y, ~₹57L all-in, ~$48K (~₹40L) unvested.
**In play:** Qualcomm · AMD · Amazon Lab126 · Cerebras · Intel.

---

## Offers (India, 8y, all-in TC, ₹ LPA)

| Company | Target level | Ongoing TC | Sign-on / unvested buyout |
| --- | --- | --- | --- |
| Qualcomm | Staff HW | **85–91** | 3–8 cash; RSU buyout on request |
| AMD | MTS L7 | **65–75** | 5–10 cash; RSU buyout on request |
| Amazon Lab126 | HW Engr II L5 | **78–85** steady | Split sign-on 25 (Y1) + 18 (Y2) absorbs MSFT unvested |
| Cerebras | HW (India) | **70–80** (est.) | Folded into larger 4-yr grant |
| Intel | Grade 7/8 | **60–70** | 3–8 cash + standard $48K RSU match |

Sources: Levels.fyi India (QC Staff ₹91L, AMD MTS ₹68L, Amazon HW II ₹80L, Intel G7 ₹50L / G8 ₹78L). Cerebras India is sparse — treat ±15%.

**Unvested ₹40L is recoverable** at all five via RSU buyout / split sign-on / larger grant — *if negotiated*. Treat any offer without buyout language as incomplete.

---

## Ranking (weighted 1–10)

Weights: Tech 25, Learning 20, AI-defensibility 15, Stability 15, Value-adder 10, Finance 15.

| Company | Score | One-line |
| --- | --- | --- |
| **Qualcomm Staff** | **8.1** | Best technical fit; only risk is level downgrade |
| **AMD MTS** | **7.9** | CPU perf/W = scarcest AI-transferable skill |
| **Amazon Lab126** | **7.7** | Best stability + full-cycle custom NPU breadth |
| Cerebras | 6.8 | High variance; just IPO'd, customer-concentrated |
| Intel | 6.7 | Floor only; ongoing layoffs |

---

## Role content (what you actually build)

| Company | Role | Skill being built |
| --- | --- | --- |
| Qualcomm | CPU pipeline / corner-case validation | Compute-arch intuition, pipeline trace, direct CPU-team exposure |
| AMD | Post-Si CPU core power & perf | Freq-V curves, power delivery debug → **maps directly to AI inference perf/W** |
| Amazon Lab126 | AZ1 Gen-2 NPU full cycle (Zebu + HAPS + post-Si + baremetal) | Full-cycle custom-ASIC breadth on a shipping ML accelerator |
| Cerebras | Wafer-scale silicon | Training (not edge); different problem class entirely |
| Intel | Server accelerator IP (PCIe Gen5/6 + power mgmt) | Systems/interconnect depth; capped by Intel process overhead |

---

## Scenario decision matrix

| Scenario | Action |
| --- | --- |
| QC Staff lands at ≥ ₹78L | **Take it.** Stop optimising. |
| QC offers Senior, no written promo path | **Reject.** Level reset = 2–3y wait + lost negotiating leverage. |
| QC offers Senior + written Staff promo ≤ 18 mo + ≥ ₹75L | Take only if AMD/Amazon haven't closed. |
| QC stalls; AMD ≥ ₹70L lands | **Take AMD.** Adjacent skill, not consolation. |
| Both QC and AMD stall; Amazon lands | **Take Amazon** if AZ1 NPU pull is real or stability matters most. |
| Only Intel lands | Counter with AMD/Amazon number; take only if ≥ ₹75L all-in with RSU match. |
| Cerebras alone, no written equity | **Pass.** Stability + customer-concentration risk too high. |
| Cerebras beats AMD by ≥ ₹15L, written equity | Take only if you can absorb startup variance. |

---

## Tradeoff limits — how much salary can you absorb for tech?

| Option | Min acceptable all-in | Required condition |
| --- | --- | --- |
| QC Staff | ₹78L | None — just don't accept Senior |
| QC Senior | ₹75L | **Written** Staff promo ≤ 18 mo |
| AMD MTS | ₹70L | None — perf/W specialisation justifies parity |
| Amazon L5 | ₹85L steady-state (not Y1-only) | Push grant size, not sign-on |
| Cerebras | ₹85L | IPO/runway clarity in writing |
| Intel G7/8 | ₹65L | Only as floor / leverage |

---

## What you lose if you don't go to QC (4-yr lens)

| Skipping QC for… | Tech delta | Ongoing TC delta | 4-yr expertise cost | Verdict |
| --- | --- | --- | --- | --- |
| AMD | –1 pt on arch depth | –₹15–20L/yr | Roughly **even** — different specialisation, equally defensible | Take it |
| Amazon | –1 pt on depth, +1 pt on breadth | –₹5–10L/yr | Lose pipeline depth, gain full-cycle ASIC + best stability | Take if stability/breadth wins |
| Intel | –2 pts | –₹20–25L/yr | Significant skill compounding loss | Only as floor |
| Cerebras | –1 pt + stability hit | ±0 | Concentration + execution risk | Only with written terms |

**Hard rules:**
- Reject QC Senior unless written Staff-promo path within 18 mo.
- Reject any offer that doesn't address the ₹40L unvested explicitly.
- Y1 cash isn't the differentiator once buyout is in — decide on level, skill, stability.

---

## Market context (May 2026)

- **Cerebras** — IPO'd May 2026 ($185/share, $5.55B raised). 4 customers ≈ 86% of revenue. Newly public, customer-concentrated.
- **Intel** — 24K layoffs in 2025; new CEO Lip-Bu Tan (Mar 2025); US govt 9.9% stake. Restructuring ongoing.
- **AMD / Qualcomm / Amazon** — stable, hiring in India.

---

## Negotiation checklist

- Lead Intel with a competing offer in hand.
- Push Amazon on **RSU grant size**, not sign-on top-up.
- Show MSFT vest screenshot to unlock QC/AMD buyout.
- For Intel JD: lead with C-states / PkgC6 / boot-flow depth; brush up on PCIe Gen5 link training.

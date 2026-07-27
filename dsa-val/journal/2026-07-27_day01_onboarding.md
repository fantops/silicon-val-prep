# Day 01 — Onboarding: DSA scope + validation flow

**Date:** 2026-07-27 · **Stage:** onboarding

## What I did

- Joined Intel · **NXNE** group · assigned to **DSA IP validation** for server (Xeon) platforms.
- Mapped the validation flow: **HAPS (pre-Si, IP coverage + power-on on some platforms) → full-SoC emulation (concurrency) → post-Si regression (volume)**.
- Confirmed **Maestro content** carries over from previous work; **bare-metal OS boots** are a test vehicle.
- Set up this `dsa-val/` workspace + DSA fundamentals + validation-flow docs.

## What I learned

- DSA = on-die Xeon **data-mover accelerator**: 64B descriptors → work queues → engines; `MOVDIR64B` (dedicated WQ) vs `ENQCMD` (shared WQ, accept/retry).
- Address translation via **IOMMU/VT-d + PASID + SVM**, with **PRS** page-faulting — a big functional surface.
- Concurrency (fairness/ordering/deadlock) is why the **full-SoC emulation** stage exists on top of HAPS breadth.

## Open questions

- [ ] Which Xeon gen(s) / platform(s) are in scope?
- [ ] Priority opcodes for my IP? SWQ vs DWQ focus? SIOV in scope?
- [ ] What does Maestro give me vs. what I author? (notes → `_internal/`)
- [ ] Which platforms get the **power-on** pass on HAPS?
- [ ] Bare-metal boot target + handoff to regression?

## Next

- Read the [DSA IP fundamentals](../fundamentals/dsa_ip_fundamentals.md) end-to-end; mark anything unclear.
- Start filling the [coverage checklist](../validation/validation_flow.md) against my actual IP scope.
- Capture Maestro/platform specifics in [`../_internal/`](../_internal/).

---
*Promote matured concepts → `../fundamentals/` or `../validation/`. Internal specifics → `../_internal/`.*

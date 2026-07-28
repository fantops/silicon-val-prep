# Day 02 — DSA spec 3.0 + ADR & NTB ramp-up

**Date:** 2026-07-28 · **Stage:** onboarding / ramp-up

## What I did

- Got the **public** Intel DSA Architecture Spec **Rev 3.0** (341204-006US, Jun 2025); kept a local copy in `dsa-val/` (now git-ignored via `dsa-val/*.pdf`).
- Updated [DSA fundamentals](../fundamentals/dsa_ip_fundamentals.md) to Rev 3.0: revision history, full op set (scatter-gather, type-convert/reduce), inter-domain, Event Log, Translation Fetch, durable writes, NTB reach, expanded coverage §9.
- Wrote a [Fast Ramp-Up](../ramp_up.md) one-pager: mental model, spec reading order, register + instruction cheat-sheets, first flows, glossary.
- Added flow primers for my extra scope: [ADR / durable writes](../fundamentals/adr_flow.md) and [NTB / cross-node](../fundamentals/ntb_flow.md).

## What I learned

- **Rev 3.0 delta:** Scatter-Gather; Type Conversion + Reduce (compute); per-group Read/Write Bandwidth Limit (`GRPCFG`); single-entry Batch; `DSACAP0/1/2`. (Inter-domain / Event Log / Translation Fetch landed in Rev 2.0.)
- **ADR intersection (§3.11):** durable writes via cache-control flags, gated by `GENCAP` Durable Write Support — DWS=0 → CC1=0, CC2=1 (+ readback); DWS=1 → CC3=1. The **completion record is always cached → never persistent**. Strict Ordering ≠ persist-ordering.
- **NTB intersection (§2, §3.9):** DSA moves to/from remote-node memory across an NTB; **destination readback** (a zero-length read at the final dst) confirms the write crossed the NTB link before the CR is written. Drain issues implicit per-TC readbacks (Root Complex) + up to 2 explicit readbacks for peer / NTB targets.
- **Inter-domain (§3.14):** access PASID vs descriptor PASID; IDPT entry types SASS / SAMS + memory windows (Address vs Offset mode); Update Window = atomic change + implicit drain.

## Open questions

- [ ] ADR vs eADR domain scope on the target platform? Which durable-write path (`GENCAP` DWS 0/1)?
- [ ] NTB topology in scope; inter-domain (same-node) vs cross-node NTB flows — which are priority?
- [ ] Which Rev 3.0 ops (scatter-gather, reduce) are in my coverage priority?

## Next

- Read Ch 8 for the exact cache-control-flag encodings (§8.1.3.1) and inter-domain descriptor fields.
- Turn the ADR / NTB coverage bullets into a concrete vplan slice against my IP scope.
- Capture platform / Maestro specifics for ADR power-fail + NTB topology in [`../_internal/`](../_internal/).

---
*Promote matured concepts → `../fundamentals/` or `../validation/`. Internal specifics → `../_internal/`.*

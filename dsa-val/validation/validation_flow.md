# DSA Validation — Flow & Platforms

> How DSA IP gets validated across pre-Si and post-Si. Public methodology + your described flow.
> **Internal specifics (Maestro details, platform configs, IP internals) do NOT belong here** — put them in [`../_internal/`](../_internal/) (git-ignored).

---

## The three stages

```mermaid
flowchart LR
  A["1 · Pre-Si HAPS<br/>FPGA prototype"] --> B["2 · Full-SoC emulation"]
  B --> C["3 · Post-Si regression"]
```

### 1. Pre-Si — HAPS (Synopsys FPGA prototype)

- **Goal:** extensive **per-IP functional coverage** — drive the DSA opcode × size × alignment × boundary matrix (see fundamentals §9) with real speed.
- **Power-on** pass for some platforms (bring the IP up from reset, verify power/clock-on sequencing before functional traffic).
- **Why HAPS:** near-real clock speeds → long/high-volume functional runs that are too slow on pure RTL sim; earliest place to shake out real firmware/driver + IP interaction.
- Trade-off: great throughput and IP breadth, but limited internal visibility vs. emulation, and an IP-block (not full-SoC) view.

> TODO (internal → `_internal/`): which HAPS platform(s), power-on-enabled platforms, capacity/partitioning, bring-up checklist.

### 2. Full-SoC emulation model

- **Goal:** **concurrency + cross-IP** behavior — many engines/WQs/clients + IOMMU + mesh + memory + PCIe + PM interacting at once (fundamentals §10).
- Targets fairness/QoS, ordering, head-of-line blocking, deadlock freedom, translation-cache churn, and DSA↔other-IP interactions a HAPS IP view can't fully see.
- Trade-off: highest visibility/debuggability, lowest speed → focused concurrency scenarios, not brute-force volume.

> TODO (internal → `_internal/`): emulation platform, which concurrency scenarios, debug/waveform flow.

### 3. Post-Si — regression (volume validation)

- **Goal:** high-volume regression on **real silicon** — confirm the IP holds up at scale, across parts/platforms, over time.
- Content promoted from pre-Si; failures triaged back to IP/firmware/platform.

> TODO (internal → `_internal/`): regression harness, platforms, pass/fail triage path, escape-tracking.

---

## Content & vehicles

### Maestro content (reused from previous work)

- Same **content framework** as your prior work → faster ramp; re-target existing patterns at DSA.
- **Public-safe notes only here.** Keep Maestro-specific structure, APIs, and content details in [`../_internal/`](../_internal/).

> TODO (internal → `_internal/`): what Maestro provides vs. what you author; test/content layout; reuse map from previous IP → DSA.

### Bare-metal OS boots

- **Bare-metal** (no full OS scheduler/driver stack) gives **deterministic, low-noise** control of the IP — you own MMIO, descriptors, and completion polling directly (ties to [Day 3 MMIO](../../prep_plan/day03_bits_mmio_volatile_polling.md), [Day 20 DMA/ordering](../../prep_plan/day20_dma_doorbells_ordering_barriers.md)).
- Typical bare-metal DSA smoke path (generic):
  1. Enumerate device, map BAR / portal.
  2. Configure + enable a WQ (dedicated is simplest for bring-up).
  3. Build a 64B **Memory-Move** descriptor (small, aligned).
  4. Submit (`MOVDIR64B`), **poll the completion record** status.
  5. Verify destination bytes; then widen size/alignment/opcode coverage.
- Great for **power-on** and first-functional on HAPS before layering OS/driver runs.

> TODO (internal → `_internal/`): actual boot target(s), image build/load flow, handoff to regression.

---

## Coverage checklist (derive the vplan from fundamentals §9)

- [ ] Every **opcode** exercised (move, fill, compare, compare-pattern, delta create/apply, dualcast, CRC, copy-with-CRC, DIF insert/strip/update/check, cache-flush, drain, batch, noop).
- [ ] **Sizes:** 0, 1B, sub-line, cache-line, page (4K), huge (2M), max descriptor size.
- [ ] **Alignment / boundaries:** aligned, unaligned, **cache-line-crossing**, **page-crossing**, overlapping src/dst.
- [ ] **Submission:** DWQ (`MOVDIR64B`) and SWQ (`ENQCMD`); **retry/backpressure** at queue-full; batch descriptors; drain/fence.
- [ ] **Translation:** PASID, SVM, **PRS page-fault + replay**, DevTLB/IOTLB invalidation, permission faults.
- [ ] **Completion/interrupts:** poll vs `UMWAIT` vs **MSI-X / IMS**; completion ordering; status/error codes.
- [ ] **Error injection:** bad addr, misalign, poison/UC memory, DIF/CRC mismatch, WQ overflow, PASID misconfig.
- [ ] **Concurrency (emulation):** multi-engine/WQ/client, fairness/QoS, ordering, deadlock freedom.
- [ ] **Power:** idle clock/power gating; **power-on** sequence (target platforms).
- [ ] **Virtualization:** SIOV, VF/mdev to guest, isolation.

---

## Debug ladder (when a DSA test fails)

1. **Completion record status** — did the engine report success, an error code, or nothing (hang)?
2. **Submission** — for SWQ, did `ENQCMD` get accepted (ZF)? For DWQ, did we overflow?
3. **Descriptor** — opcode/size/addr/PASID/flags correct? alignment/boundary as intended?
4. **Translation** — PASID valid? page present? PRS fault expected/handled? IOMMU fault log?
5. **Ordering/coherence** — barrier before submit? completion vs data visibility?
6. **Isolate** — reproduce single-descriptor (HAPS) before blaming concurrency (emulation).

> Symptom → hypothesis → narrowed-how → root cause → fix → what-I'd-do-differently (your 90-sec bug-story shape).

---

### Cross-links
- [DSA IP fundamentals](../fundamentals/dsa_ip_fundamentals.md) · [Journal](../journal/) · [Internal (git-ignored)](../_internal/)
- Foundation: [Day 20 DMA/ordering](../../prep_plan/day20_dma_doorbells_ordering_barriers.md) · [Day 19 IOMMU](../../prep_plan/day19_tlb_huge_pages_iommu_smmu.md) · [Day 24 verification planning](../../prep_plan/day24_verification_planning_coverage.md)

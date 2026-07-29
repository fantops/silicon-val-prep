# Proposal (DRAFT) — Dec 2026 IEEE: DSA-offloaded AI inference data plane (Intel server: AMX + DSA)

> **Tracked in the private repo** (`fantops/silicon-val-prep`, private + 2FA). Public-source only — **no Intel-internal data**.
> **Status: DRAFT** — the long-paper thesis; still gathering data and refining. Near-term (Aug 7) work lives in
> [`proposal_npu_dsa_mover.md`](proposal_npu_dsa_mover.md) + [`experiment_plan_npu_dataplane.md`](experiment_plan_npu_dataplane.md).
> Confidentiality: genericize internal tool names, use public benchmarks/models for any numbers, keep Intel internals in git-ignored `_internal/`, and route through Intel publication clearance before submission.

---

## Scope

Full **experimental** paper. Compute engine = **AMX** on a DSA-capable Xeon; **DSA** offloads the inference
data plane on the **same** die. The near-term client/NPU proposal measures the *opportunity*; this paper
measures the *mechanism* end-to-end on **Intel** silicon. **This is a draft — data still to be gathered.**

---

## The one-paragraph pitch

On **Intel** platforms — a **Xeon** server (AMX compute) and the **Core Ultra** client (NPU) — a large and
growing share of inference latency, energy, and memory traffic is **not** matrix compute; it is the **data
plane**: quantize/dequantize, tensor layout & type conversion, embedding gather, KV-cache movement, and
copies across execution-provider (EP) boundaries including CPU fallback. We characterize this **"data-plane
tax,"** argue it should be **offloaded to a data-streaming accelerator**, and show that **Intel DSA Rev 3.0**
added the exact primitives — **Type Conversion, Reduce, Scatter-Gather** (plus Memory Move / Dualcast / Fill) —
that map onto these operations, freeing CPU cycles for compute. We propose an offload + validation framework
and a research agenda, evaluated end-to-end on **Intel silicon** (DSA + AMX on one Xeon).

---

## Platform reality — AMX as the compute engine (get this right)

On Intel, **DSA and the NPU do not co-exist on one die**: DSA ships on **server (Xeon)**, the NPU on
**client (Core Ultra)**. So a *measured* "DSA accelerates the NPU" result is not possible on one box.

**Fix — use AMX as the compute engine on the same Xeon:** a Sapphire-Rapids-class Xeon has **both AMX**
(in-core matrix/GEMM — the *server analog of an NPU*) **and DSA**. Single platform, fully reproducible:
**AMX runs the matmuls; DSA offloads the data plane that feeds them**. The **Intel Core Ultra** NPU stays in the
story as the **generalization** (the same offload frees the host that feeds any compute engine), argued analytically.

---

## Title candidates

1. Offloading the Inference Data-Plane Tax: A Case for Data-Streaming Accelerators on Intel Platforms
2. Beyond the Matrix Engine: Freeing CPU Cycles by Offloading the AI Data Plane to DSA
3. The Hidden Data-Movement Tax in AI Inference — and an Accelerator-Offload Agenda

---

## Why this is credible (leverages prior + current work)

| Paper section | Sourced from |
|---|---|
| KPI method + data-plane-tax intuition (perf / accuracy / memory) | Microsoft experience — method/knowledge only, **no MS data**; re-measure with public tools |
| Framing context (ONNX Runtime, EPs, NPU/CPU fallback) | Microsoft experience (concept only) |
| Compute + offload on one Xeon: **AMX** (matmul) + **DSA** (data plane) | Public: AMX ISA + DSA spec 341204-006US |
| Offload op mapping (DSA Rev 3.0 type-convert / reduce / scatter-gather) | Current Intel DSA role + public spec |
| Correctness / validation angle (numerics, ordering, fault handling) | Current Intel DSA validation |

---

## AI data-plane op → data-streaming-accelerator op (the core table)

| AI-inference data-plane op | DSA operation | New in |
|---|---|---|
| Quantize / dequantize, dtype cast | **Type Conversion** | Rev 3.0 |
| All-reduce, pooling, reductions | **Reduce** | Rev 3.0 |
| Embedding lookup, KV gather, MoE routing | **Scatter-Gather** | Rev 3.0 |
| KV-cache movement, weight streaming, double-buffering | Memory Move / Dualcast / Fill | base |
| Cross-node KV / model-state transfer | Inter-Domain + NTB | Rev 2.0 |
| Persistent KV-cache / checkpoint | Durable writes (ADR) | base |

---

## Analytical framing

- CPU-side copy time:   `t_cpu  = B / BW_cpu`
- DSA-side time:        `t_dsa  = B / BW_dsa + t_submit`
- CPU cycles freed:     `~ f_core * t_cpu`   (core released; async overlap)
- Energy freed:         `~ P_core * t_cpu`   (minus DSA active energy)

Worth offloading when `t_submit` is amortized (large / batched B) and the core has other work to overlap.
Type-Conversion / Scatter-Gather add *compute* offload on top of movement — raising the benefit for AI data
planes vs classic memcpy-only DSA studies.

---

## Evidence plan — reproducible PoC (AMX + DSA on one Xeon)

**Hardware:** a **DSA-capable Xeon** (SPR/EMR/GNR), DSA enabled in BIOS, IOMMU on, `accel-config` installed.
(A generic "server PC" will not have DSA; cloud-guest exposure is unreliable — plan on a lab/Intel box.)

**Software:** `accel-config` + Intel **DML** / `idxd` (`MOVDIR64B`/`ENQCMD`) for DSA; **llama.cpp** (AMX/oneDNN)
or ONNX Runtime / OpenVINO for a small open model (Phi / Qwen / Llama-3.x 1–3B, int8/bf16); `perf` + RAPL.

**Offload 1–2 DSA-native ops:** dequant (int8/int4→bf16) → **Type Conversion**; embedding gather →
**Scatter-Gather**; KV-cache move → **Memory Move**.

**Two tiers:** (1) microbenchmark at inference-realistic sizes, CPU vs DSA → cycles freed / latency / bandwidth /
energy; (2) stretch: wire one op through DSA inside the runtime → end-to-end tokens/sec + CPU-util delta.

---

## Contributions

1. **Characterize** the inference data-plane tax (public benchmarks + open models).
2. **Taxonomy** mapping AI data-plane ops → data-streaming-accelerator ops, DSA Rev 3.0 as the concrete instance.
3. **Framework:** offload + validation — correctness KPIs (type-conversion numerics, ordering, faults) *and*
   system KPIs (latency / energy / memory).
4. **Measurements** on AMX+DSA; **research agenda** for accelerator-offloaded inference data planes.

---

## Prior art to differentiate (verify before submitting)

- Datacenter / "microservice tax" — data movement dominates (Kanev et al., ISCA 2015).
- DSA characterization on 4th-gen Xeon — memcpy/dedup/DPDK/SPDK, Rev 1/2 (Kuper et al., ~2023).
- Data-movement-is-energy results (general).
- EP / runtime-overhead and CPU-fallback cost in on-device inference.
- KV-cache offload / prefill-decode disaggregation in LLM serving (Mooncake / DistServe-style).
- **Gap to claim:** AI data plane + DSA **Rev 3.0 compute ops** (type-convert / reduce / scatter-gather) on Intel platforms is under-covered.

---

## Future directions (park here; decide later)

- **DSA + NTB for KV-cache disaggregation** (cross-node model state) — hot topic; needs a multi-node setup.
- **Persistent KV-cache / fault-tolerant inference** via DSA durable-writes (ADR/eADR) — reliability angle.
- **Validation-methodology** track: coverage model for a data-streaming accelerator (op × size × fault ×
  concurrency + inter-domain / NTB / durable-write).

---

## Confidentiality checklist

- [ ] Genericize any internal tool names (e.g., "a KPI-capture harness", not the product name).
- [ ] Use only public benchmarks / open models for any numbers; no proprietary MS or Intel data.
- [ ] DSA content limited to the public spec (341204-006US, Rev 3.0).
- [ ] Route through Intel publication clearance before submission.
- [ ] Co-author / affiliation attribution agreed with the team.

---

## Open items / next (draft)

- [ ] Secure a **DSA-capable Xeon** (SPR/EMR/GNR); confirm the **AMX** compute path in the chosen runtime.
- [ ] Related-work scan to pin the exact gap + citations.
- [ ] Gather per-op data points (CPU vs DSA) → feed the analytical model + figures.
- [ ] Decide co-author split and who owns the experiments.

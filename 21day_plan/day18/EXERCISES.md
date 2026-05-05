# Day 18 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day18_npu_ort_ep_hexagon_htp.md` ("Coding A" and "Coding B" sections).

> Note: Coding A is a Systems C model of the host-side NPU dispatch path; the production stack uses Python/ORT for measurement, but the exercise is C.

---

## E1 — Systems C: NPU host harness model

**Problem statement:** Build a tiny simulator of the host side of NPU dispatch. Each `op_t` says whether it is `npu_supported`. `run_graph(graph, n, &stats)` walks the ops, assigns each to NPU or CPU EP, charges latency per op (NPU = 0.2 µs, CPU = 5.0 µs), and adds a 50 µs "tensor copy" cost at every EP boundary (transition from one EP to the other). Track `total_ops, npu_ops, cpu_ops, partition_boundaries, sim_latency_us`.

Use it to demonstrate the **silent EP fallback cliff**: a clean 6-op graph runs on NPU; inserting a single unsupported `CustomOp` adds two EP boundaries and ~100 µs of pure copy cost.

**You must implement:**
```c
typedef enum { EP_NPU, EP_CPU } ep_t;
typedef struct { const char *name; bool npu_supported; int flops; } op_t;
typedef struct { int total_ops, npu_ops, cpu_ops, partition_boundaries; double sim_latency_us; } stats_t;
void run_graph(const op_t *graph, int n, stats_t *s);
```

**Driver / tests to write:**
- Clean graph: `Conv1, Relu1, Conv2, Add, Conv3, Softmax` (all NPU-supported) → expect 0 boundaries.
- Fallback graph: insert `{"CustomOp", false, 1}` after `Relu1` → expect 2 boundaries, ~100 µs added.
- Print `[CLEAN]` and `[FALLBACK]` summaries.

**Be ready to explain in interview:**
- 6-layer stack (Framework → ONNX → ORT → EP → vendor runtime → silicon).
- QDQ fusion; what happens when QDQ doesn't fuse.
- VTCM tile-size cliff; why it's sharp not smooth.
- Context cache; why first-frame cost is seconds without it.
- Why NPU at all (energy/MAC table vs CPU AVX-512 / GPU SIMT).

## E2 — DS&A: Top-K Frequent Elements (LC 347)

**Problem statement:** Given an integer array `nums` and `k`, return the `k` most frequent elements.
**Signature:** `int *top_k_frequent(const int *nums, int n, int k, int *out_len);`
**Constraints / edge cases:** `k = 1`, all unique, all same, ties broken arbitrarily.
**Talking points:** Bucket sort by frequency in O(n); same shape as bucketing kernel-launch counts when triaging an EP-fallback log.

## E3 — Architect Drills (no code, write your answers)

**D1.** Why six layers? What collapses if Framework→silicon directly?
**D2.** Why doesn't ORT refuse to run if any op falls back? What would a strict mode look like?
**D3.** Why doesn't the framework emit fused INT8 directly? What does ONNX QOperator buy?
**D4.** Why is VTCM software-managed (not coherent)? When CXL.cache makes coherent NPUs viable, what dies first?
**D5.** Tile-size cliff: barely overflow → 2× DMA → 2× latency. What makes the cliff sharp?
**D6.** Where does compile cost live (IR opt vs scheduling vs codegen)? Which is amortisable?
**D7.** NPU vs GPU vs CPU for inference — which converges first?

## E4 — Bug Anchor

**Symptoms:** Functional pass, perf 10× off. Walk the debug tree: count `EP_assignment == CPU` ops in the partition log; ETW shows QNN DLL loaded but CPU% high. Cause: silent EP fallback from one unsupported op.

Other anchors: VTCM tile-size cliff (per-tile latency from QNN profile); quant-accuracy regression after rebuild without `keep_quantized_weights=true`; missing context cache (re-compile per launch); ORT × QNN EP × QNN SDK version-matrix regression; DLL load failure (`QnnHtpV79Stub.dll` missing → unload → silent CPU fallback).

## Reference solution

Once you've attempted, see `../day18_npu_ort_ep_hexagon_htp.md` sections "Coding A" and "Coding B" for full solutions and discussion.

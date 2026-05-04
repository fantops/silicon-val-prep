# Day 18 — NPU Stack: ORT, Execution Providers, QNN, Hexagon HTP

## Concept

A trained model runs on an NPU through a **6-layer software stack**: Model framework → ONNX export → ONNX Runtime → Execution Provider (QNN / OpenVINO / DirectML / CPU) → vendor runtime/compiler → silicon (Hexagon HTP, Intel NPU, AMD XDNA). ORT **partitions** the graph: contiguous subgraphs go to the chosen EP; unsupported ops fall back to **CPU EP**. The vendor compiler runs **QDQ fusion** (Quantize → Conv → Dequantize collapses to a single INT8 op), tiles the activation to fit on-chip SRAM / local-store memory, and serialises the artifact as a **context cache**. At inference the CPU writes a doorbell; the NPU **DMAs** input from DDR into local SRAM; **HMX** runs INT8 matmul, **HVX** does vector/requant, and **DMA** streams the next tile — multiple engines pipelined.

## Why this day in the flow

You spent Days 1-17 learning the platform — pointers, MMU, IOMMU, DMA, PCIe, NVMe, power, thermal, perf counters, pipeline, concurrency. Day 18 is **the workload that runs on top of it all** in your current Microsoft role and in any Qualcomm/Intel NPU role. Day 19 is the bug stories on this stack.

In the Layer map this is **Layer 7 (NPU runtime)**.

---

## Deep Study Notes

### 1. The 6-layer NPU stack

```
┌──────────────────────────────────────────────┐
│  Layer 1: Model framework (PyTorch / TF)     │
│           training / fine-tune                │
└──────────────────────────────────────────────┘
                 │ export
┌──────────────────────────────────────────────┐
│  Layer 2: ONNX (op-graph IR)                 │
│           Conv, MatMul, Softmax, …; QDQ nodes│
└──────────────────────────────────────────────┘
                 │ load
┌──────────────────────────────────────────────┐
│  Layer 3: ONNX Runtime (the dispatcher)      │
│           graph partition + EP selection     │
└──────────────────────────────────────────────┘
                 │ assign per op
┌──────────────────────────────────────────────┐
│  Layer 4: Execution Provider                 │
│   QNN EP  | OpenVINO EP | DirectML EP | CPU  │
└──────────────────────────────────────────────┘
                 │ lower / compile
┌──────────────────────────────────────────────┐
│  Layer 5: Vendor runtime + compiler          │
│   QnnHtp.dll → libQnnHtp.so HTP graph compile│
│   QDQ fusion, tiling, context cache (binary) │
└──────────────────────────────────────────────┘
                 │ execute
┌──────────────────────────────────────────────┐
│  Layer 6: Silicon                            │
│   Hexagon HTP v79 / Intel NPU / AMD XDNA     │
│   HMX + HVX + DMA  ← VTCM (on-chip SRAM)    │
└──────────────────────────────────────────────┘
```

### 2. ORT and EP partitioning

ORT loads the ONNX graph and asks each registered EP, **per operator**, "can you run this?" The result is a **partition**: contiguous subgraphs assigned to the NPU EP, the unsupported gaps assigned to CPU EP.

Common EPs:

| EP | Backend |
|---|---|
| **QNN EP** | Qualcomm Hexagon HTP (NPU) |
| **OpenVINO EP** | Intel NPU / GPU / CPU |
| **DirectML EP** | any DX12 device (generic GPU/NPU) |
| **CPU EP** | always present, the safety net |

**More partitions = more CPU↔NPU handoffs = worse perf.** Each handoff means a tensor copy out of NPU memory back to host. One unsupported op in the middle of a clean graph can cut throughput 5-10×.

### 3. EP fallback — the silent perf cliff

If a single op isn't supported by the target EP (custom op, unsupported dtype, shape mismatch, NPU OOM), ORT silently falls back to CPU EP for that op. Functional pass; perf collapse. **Detection:**

- Enumerate `EP_assignment == CPU` ops per inference (`session.get_session_options()` + log).
- ETW: `onnxruntime_providers_qnn.dll` loaded but CPU% high during inference window.
- ORT verbose log: `[VERBOSE] node X assigned to CPUExecutionProvider`.

Mitigation: quantise / reshape the model so all ops fit; or accept the partition cost; or ship a custom op for that EP.

### 4. QDQ fusion and quantisation

ONNX models for NPUs are usually **QDQ-format**: each compute op is wrapped in `Quantize → Op → Dequantize` boundaries. The vendor compiler's most important pass is **QDQ fusion**: `Quantize → Conv → Dequantize` collapses to a single INT8 Conv that the NPU can execute natively.

If QDQ fusion fails (e.g., model rebuilt without `keep_quantized_weights=true`, mismatched scales, unrecognised pattern), each Quantize/Dequantize stays as a standalone op → drops off NPU → ends up on CPU EP → silent fallback (see above).

### 5. Tiling and VTCM

Activations rarely fit in NPU SRAM. The compiler **tiles** them (e.g., 256×256 with halo padding) so each tile fits in VTCM.

| NPU | On-chip SRAM / local store |
|---|---|
| Qualcomm Hexagon HTP v79 (Snapdragon X Elite) | VTCM / local SRAM; exact size should be quoted only from internal platform docs |
| Intel NPU (LNL) | local SRAM / scratchpad; quote exact size only from the relevant platform doc |
| AMD XDNA | local SRAM / scratchpad; quote exact size only from the relevant platform doc |

VTCM is **software-managed, not in the CPU coherence domain**. The compiler/runtime owns DMA scheduling. Trade-off: deterministic latency vs more code complexity.

**VTCM tile-size cliff:** if the compiler picks a tile that doesn't fit, multiple DMA round-trips per tile happen and per-tile latency can jump sharply instead of degrading smoothly.

### 6. Context cache

The compiled artifact (after lowering, QDQ fusion, tiling) is serialised as a **context binary** (QNN: `*.bin` context cache).

```
First launch:  load ONNX → partition → lower → fuse → tile → compile → SAVE bin
Second launch: load bin → QnnContext_createFromBinary() → ready in ms, not seconds
```

Skipping the cache (debug build, missing file, version mismatch) means you re-pay the compile latency on every launch — a real bring-up bug.

### 7. The execute hot loop

```
1. CPU prepares input tensor in DDR
2. CPU writes NPU command-queue doorbell (MMIO)        ← Day 8
3. NPU DMAs input DDR → VTCM
4. NPU runs the graph:
     HMX  — INT8 matmul, 4096 MACs/instr        ┐
     HVX  — vector ops, requantisation         │  3 engines pipelined
     DMA  — fetch next tile / write output      ┘
   So at steady state: DMA(N+1) + HMX(N) + HVX(N-1) all in flight
5. NPU DMAs output VTCM → DDR
6. NPU raises MSI-X to CPU                            ← Day 7 / 11
7. ORT returns the output tensor
```

The multi-engine pipeline is how Snapdragon X Elite-class NPUs reach their publicly advertised peak TOPS. If any engine stalls (tile cliff, OOM, host starvation), the pipeline degrades toward serial execution and latency multiplies.

### 8. Hexagon HTP family — interview numbers

| HTP version | SoC | Peak TOPS | VTCM / local-store note |
|---|---|---|---|
| v68 | Snapdragon 888 | ~15 | quote exact VTCM only from platform docs |
| v69 | SD 8 Gen 1 | ~18 | quote exact VTCM only from platform docs |
| **v73** | SD 8 Gen 2 | ~26 | quote exact VTCM only from platform docs |
| v75 | SD 8 Gen 3 | ~36 | quote exact VTCM only from platform docs |
| **v79** | Snapdragon X Elite (laptop) | **~45** | quote exact VTCM only from platform docs |

Other anchors to verify against internal docs before quoting: HMX instruction shape, FastRPC wake latency, and effective shared-DDR bandwidth.

DLL chain on Windows (Copilot+ PC):

```
onnxruntime_providers_qnn.dll  → QnnHtp.dll  → QnnHtpV79Stub.dll
                                                    │
                                  on-device: libQnnHtp.so (HTP graph compiler)
```

### 9. The three NPU compute engines

| Engine | Job | Throughput |
|---|---|---|
| **HMX** (Matrix eXtensions) | INT8 matmul / convolution | 4096 MACs / instruction |
| **HVX** (Vector eXtensions) | vector ops, activations, requantisation | 1024-bit vectors |
| **DMA** | tile shuffles, in/out, halo padding | parallel to compute |

The three run as a software-scheduled pipeline; the compiler interleaves their work so all three are active per cycle steady-state.

### 10. ETW and platform observability (Windows)

On Copilot+ PC, the canonical observation tool is **ETW** (Event Tracing for Windows) → captured to `.etl` → analysed in **WPA** (Windows Performance Analyzer). Useful providers:

| Provider | What it shows |
|---|---|
| ORT EP events | which EP got which op |
| QNN EP DLL load | confirms NPU path actually attempted |
| GPUView | NPU command-queue timeline |
| Memory counters | VirtualAlloc, page faults, peak committed |
| CPU sampling | did inference land on CPU EP by accident |

This is what you reach for during NPU validation, the same way you'd reach for `perf stat` on Linux.

---

## Bugs in this Topic

> Bugs that fit *here* (the stack itself); the deep memory + scheduling stories are on Day 19.

- **Silent EP fallback** — one unsupported op forces a per-op CPU partition; tensor copies dominate. Functional pass, perf 10× off. Detect: count `EP_assignment == CPU` ops; ETW shows QNN DLL loaded but CPU% high.
- **VTCM tile-size cliff** — compiler chose tile larger than VTCM; multiple DMA trips per tile; latency doubles. Detect: per-tile latency from QNN profile; check tile-fits-VTCM in compile log.
- **Quant-accuracy regression after rebuild** — model rebuilt without `keep_quantized_weights=true` → QDQ doesn't fuse → Quantize/Dequantize stay → ops drop to CPU. Detect: compile log shows split QDQ; INT8 ops missing.
- **Missing context cache** — debug build re-runs compile every launch; first-frame latency is seconds. Fix: ensure `QnnContext_createFromBinary()` path with valid `.bin`.
- **Version-matrix regression** — ORT version × QNN EP version × QNN SDK version forms a 3D space. A "good" combo is identified by the matrix, not by changing one variable. Bug 4 on Day 19 is this story.
- **DLL load failure on the QNN path** — `QnnHtpV79Stub.dll` not found / wrong arch (x64 vs ARM64) → QNN EP unloads → silent fall to CPU EP → looks like an NPU perf bug.

---

## Interview Explanation (60-90 sec)

> "An AI workload on an NPU runs through six layers — framework, ONNX, ONNX Runtime, Execution Provider, vendor runtime/compiler, silicon. ORT partitions the graph: contiguous subgraphs go to QNN EP for Qualcomm Hexagon, OpenVINO EP for Intel, DirectML for any DX12 device, and CPU EP is always present as the fallback. The fallback is the silent perf cliff — one unsupported op forces a per-op partition and you spend most of your time copying tensors between NPU and host memory. The vendor compiler does QDQ fusion to collapse Quantize-Conv-Dequantize triples into a single INT8 op, then tiles activations so each tile fits in software-managed local SRAM — Qualcomm calls that VTCM. The compiled artifact is serialised as a context cache so the second launch skips compile. At inference the CPU writes a doorbell, the NPU DMAs input from DDR to local SRAM, and compute/vector/DMA engines run as a pipeline — that's how Snapdragon X Elite-class NPUs reach their publicly advertised peak TOPS. On Windows I observe all of this with ETW and WPA, the equivalent of `perf` on Linux."

---

## Architect Drills

*The NPU stack is six layers because each layer is a tradeoff (portability vs performance, flexibility vs determinism). Reason about why each landed there — and what would force a redesign. No answer key.*

**D1. Why does the AI stack have six layers (Framework → ONNX → ORT → EP → vendor runtime → silicon)?**
*Constraints to weigh:* model-author flexibility, vendor-portability, IR stabilisation point (ONNX), runtime hot-swap, vendor-specific codegen. What collapses if Framework→silicon directly (PyTorch's MPS does this) — and what's the cost in portability?

**D2. EP fallback is the silent perf cliff. Reason about why ORT doesn't just refuse to run if any op falls back.**
*Constraints to weigh:* model-author convenience (functional first), partial-acceleration as a feature, debug-build vs prod-build expectations. What would a "strict" mode look like, and who would actually enable it in production?

**D3. QDQ fusion collapses Q-Op-DQ into INT8. Reason about why the framework doesn't just emit fused INT8 ops directly.**
*Constraints to weigh:* model interoperability across vendors with different fused-op support, training-time vs inference-time graph, opset stability, calibration metadata location. What does ONNX's QOperator format buy that QDQ doesn't?

**D4. VTCM is software-managed (not coherent with CPU). Reason about why NPUs trade coherence for area/power.**
*Constraints to weigh:* coherence interconnect cost, prefetch/tile predictability, deterministic latency, INT8 dataflow doesn't need fine-grain sharing. When CXL.cache makes coherent NPUs viable, what dies first — VTCM or DMA?

**D5. The tile-size cliff: barely overflow VTCM → 2× DMA trips → 2× latency. Reason about what makes this cliff sharp.**
*Constraints to weigh:* on-chip vs off-chip latency ratio, tile-DMA double-buffering, compiler-heuristic conservatism, weight-vs-activation tile split. What does a future "VTCM = 16 MB" buy and what does it not change?

**D6. Context cache saves seconds on launch. Reason about why the compile cost is so high in the first place.**
*Constraints to weigh:* graph optimisation, op fusion, tile sizing, codegen for HVX/HMX, register allocation. Where does the cost actually live — IR optimisation, scheduling, or codegen — and which is amortisable across model versions?

**D7. NPU vs GPU vs CPU for inference. Imagine a future where one architecture wins. Reason about which and why.**
*Constraints to weigh:* TOPS/W, programmability, model-shape flexibility, batch size, memory bandwidth. Construct the workload where each genuinely wins today, and project which converges first.

---

## Coding A — Systems C: NPU host harness model

A small simulator of the host side of NPU dispatch — doorbell + completion + EP fallback counter.

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef enum { EP_NPU, EP_CPU } ep_t;

typedef struct {
    const char *name;
    bool        npu_supported;   /* compile-time op support */
    int         flops;           /* synthetic cost */
} op_t;

typedef struct {
    int total_ops, npu_ops, cpu_ops;
    int partition_boundaries;
    double sim_latency_us;
} stats_t;

static double partition_cost_us = 50.0;  /* tensor copy per boundary */
static double npu_op_cost_us    = 0.2;
static double cpu_op_cost_us    = 5.0;   /* CPU EP fallback is expensive */

static void run_graph(const op_t *graph, int n, stats_t *s) {
    ep_t prev = EP_NPU;
    memset(s, 0, sizeof *s);
    for (int i = 0; i < n; i++) {
        ep_t cur = graph[i].npu_supported ? EP_NPU : EP_CPU;
        if (i > 0 && cur != prev) {
            s->partition_boundaries++;
            s->sim_latency_us += partition_cost_us;
        }
        if (cur == EP_NPU) { s->npu_ops++; s->sim_latency_us += npu_op_cost_us; }
        else               { s->cpu_ops++; s->sim_latency_us += cpu_op_cost_us; }
        prev = cur;
    }
    s->total_ops = n;
}

static void report(const char *tag, const stats_t *s) {
    printf("[%s] ops=%d npu=%d cpu=%d boundaries=%d latency=%.1f us\n",
           tag, s->total_ops, s->npu_ops, s->cpu_ops,
           s->partition_boundaries, s->sim_latency_us);
}

int main(void) {
    /* Clean graph: all NPU. */
    op_t clean[] = {
        {"Conv1", true, 1}, {"Relu1", true, 1}, {"Conv2", true, 1},
        {"Add",   true, 1}, {"Conv3", true, 1}, {"Softmax", true, 1},
    };
    /* Same graph but one custom op forces CPU fallback in the middle. */
    op_t fallback[] = {
        {"Conv1",     true, 1}, {"Relu1", true, 1}, {"CustomOp", false, 1},
        {"Conv2",     true, 1}, {"Add",   true, 1}, {"Softmax", true, 1},
    };

    stats_t s;
    run_graph(clean,    6, &s); report("CLEAN   ", &s);
    run_graph(fallback, 6, &s); report("FALLBACK", &s);
    return 0;
}
```

Demonstrates the cliff: one fallback op turns a 6-op clean run into a 2-boundary partition with 100 µs of pure copy cost.

## Coding B — Interview DS&A: Top-K Frequent Elements (LC 347)

Bucket sort by frequency.

```python
from collections import Counter
def topKFrequent(nums, k):
    cnt = Counter(nums)
    buckets = [[] for _ in range(len(nums) + 1)]
    for x, c in cnt.items():
        buckets[c].append(x)
    out = []
    for b in reversed(buckets):
        out.extend(b)
        if len(out) >= k:
            return out[:k]
    return out
```

---

## System Thinking — Why NPUs exist as separate compute domains

**Frame:** CPUs and GPUs can run neural-network inference, but they spend most of their energy on the *wrong* things — instruction fetch, scheduling, cache coherence, register renaming. NPUs (Hexagon HTP, Apple ANE, Intel NPU, Google TPU) strip all that out and dedicate silicon to **one operation at extreme energy-efficiency: int8 matmul + activation**. The cost is a separate execution domain with its own ISA, its own memory, its own driver, and its own validation surface. The Execution Provider (EP) abstraction in ORT/TFLite exists so a single model graph can target any of these backends without rewriting.

### Q: Why a separate NPU instead of just AVX/SVE on the CPU?

**Energy per op.** A CPU spends ~70% of energy on overhead (decode, dispatch, OoO machinery) and ~30% on the actual ALU. SIMD widens the ALU but doesn't reduce overhead per *instruction*. NPUs amortize overhead over thousands of MACs per instruction:

| Compute domain | Energy per int8 MAC | Throughput |
|---|---|---|
| CPU scalar | ~50 pJ | 1 op / cycle |
| CPU AVX-512 | ~5 pJ | 64 ops / cycle |
| GPU SIMT | ~1 pJ | 1000s ops / cycle |
| **NPU (HTP / TPU)** | **~0.2 pJ** | **10K-100K ops / cycle** |

For a 1B-parameter model, the energy difference is 100-1000× — the difference between 30 minutes and 5 hours of laptop battery on continuous inference. **That's why NPUs ship in every phone, laptop, and edge SoC now.**

### Q: Why an Execution Provider abstraction (EP) instead of one runtime per backend?

Because models outlive backends. A model trained in PyTorch should run on:
- CPU EP for development.
- CUDA EP for desktop training/inference.
- DirectML EP on Windows.
- CoreML EP on Apple.
- QNN EP on Snapdragon.
- TensorRT EP on NVIDIA edge.

Each EP is a plugin that takes the same ONNX graph, partitions it into subgraphs the backend can run natively, and falls back to CPU for the rest. **The graph is portable; the kernel implementations are not.** Validation per EP focuses on (a) correctness vs CPU reference (`atol`/`rtol` per op), (b) op coverage (which ops fall back to CPU?), and (c) partition boundaries (each fallback is a CPU↔NPU memory copy = latency).

### Q: Why is HTP a separate domain, not just "part of the SoC"?

Because it has its own:
- **ISA** — Hexagon VLIW + HVX/HMX vector extensions, not ARM.
- **Memory** — VTCM (Vector Tightly-Coupled Memory), separately allocated, often non-coherent with CPU caches (Day 8 / Day 19).
- **Address space** — a separate IOMMU/SMMU context (Day 7) for DMA.
- **Power domain** — powered up/down independently (RPMh vote).
- **Firmware** — loaded by the CPU at runtime, runs the kernel scheduler on HTP itself.

This isolation is *what makes it efficient* (no coherence overhead, no shared L1 contention) but *also* what makes it hard to debug — a CPU stack trace cannot see HTP state; you need PD (Process Domain) signals, FastRPC traces, and HTP firmware logs. *Day 19 covers the bug classes this isolation enables.*

### One-liner

*"NPUs exist because dedicated matmul silicon is 100-1000× more energy-efficient than CPU SIMD — the cost is a separate ISA, memory, address space, and power domain. The EP abstraction lets one model target many backends; each EP is a partitioner + a kernel library. HTP is a separate compute domain with its own everything; that isolation is its efficiency advantage and its debug pain."*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **why NPUs exist** — *what:* dedicated DL accelerator / *why:* CPU/GPU power-inefficient for MAC-heavy graphs / *how:* systolic arrays + on-die SRAM + low-precision math / *connects:* TOPS/W metric, quantization, why a separate domain
- **EP (Execution Provider)** — *what:* ORT plugin per backend / *why:* abstract the model over many HWs / *how:* partition graph by op support, fall back unsupported ops / *connects:* silent CPU fallback bug, why partition logs matter
- **Hexagon HTP** — *what:* Qualcomm NPU subsystem / *why:* shared DSP heritage + tensor engine / *how:* HVX vector + HMX matrix units / *connects:* context cache, QNN driver, FastRPC marshalling
- **quantization** — *what:* FP32 → INT8 / INT4 / *why:* 4–8× bandwidth + power / *how:* per-tensor or per-channel scale + zero-point / *connects:* accuracy loss, calibration set, PTQ vs QAT
- **graph partition** — *what:* split model across EPs by op support / *why:* maximize NPU coverage / *how:* EP returns supported-op list, ORT splits / *connects:* CPU↔NPU transitions are the perf cliffs

---

## Bug Anchor

- **Day 6 (virtual memory):** page-fault counters that explode in the QNN regression on Day 19.
- **Day 7 (IOMMU/SMMU):** the NPU's DMA path goes through SMMU; mistranslated stream IDs are an NPU bring-up failure.
- **Day 8 (DMA / doorbells):** the doorbell-then-DMA-then-MSI sequence is exactly the NPU execute loop.
- **Day 15 (multi-IP / perf):** EP fallback shows up as an interference pattern.
- **Day 19 (NPU bugs):** QNN page-fault story, Paint SR vs Camera, silent EP fallback in detail.

## Exit Check

- [ ] Drew the 6-layer stack from model to silicon.
- [ ] Stated EP fallback as the silent perf cliff.
- [ ] Defined VTCM, its size on v79, and the tile-size cliff.
- [ ] Defined QDQ fusion and what fails when it doesn't.
- [ ] Walked the NPU execute hot loop with HMX/HVX/DMA pipelining.
- [ ] Stated only public HTP TOPS numbers publicly; quoted VTCM/local-SRAM sizes only when backed by internal platform docs.
- [ ] Named ETW + WPA as the Windows observation tool chain.

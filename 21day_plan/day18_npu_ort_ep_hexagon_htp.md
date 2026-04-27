# Day 18 — NPU Stack: ORT, Execution Providers, QNN, Hexagon HTP

## Concept

A trained model runs on an NPU through a **6-layer software stack**: Model framework → ONNX export → ONNX Runtime → Execution Provider (QNN / OpenVINO / DirectML / CPU) → vendor runtime/compiler → silicon (Hexagon HTP, Intel NPU, AMD XDNA). ORT **partitions** the graph: contiguous subgraphs go to the chosen EP; unsupported ops fall back to **CPU EP**. The vendor compiler runs **QDQ fusion** (Quantize → Conv → Dequantize collapses to a single INT8 op), tiles the activation to fit on-chip SRAM (Qualcomm **VTCM** 8 MB on v79; Intel ~2 MB; AMD ~4 MB), and serialises the artifact as a **context cache**. At inference the CPU writes a doorbell; the NPU **DMAs** input from DDR into VTCM; **HMX** runs INT8 matmul (4096 MACs/instr), **HVX** does vector/requant, **DMA** streams the next tile — three engines pipelined.

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

| NPU | On-chip SRAM |
|---|---|
| Qualcomm Hexagon HTP v79 (Snapdragon X Elite) | **8 MB VTCM** |
| Intel NPU (LNL) | ~2 MB |
| AMD XDNA | ~4 MB |

VTCM is **software-managed, not in the CPU coherence domain**. The compiler/runtime owns DMA scheduling. Trade-off: deterministic latency vs more code complexity.

**VTCM tile-size cliff:** if the compiler picks a tile that doesn't fit, multiple DMA round-trips per tile happen and per-tile latency doubles. Real numbers on v79: 256×256 burst-mode tile ≈ 2-5 ms.

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

The 3-engine pipeline is **how you hit ~45 TOPS on v79**. If any engine stalls (tile cliff, OOM, host starvation), the pipeline degrades to serial and latency multiplies.

### 8. Hexagon HTP family — interview numbers

| HTP version | SoC | Peak TOPS | VTCM |
|---|---|---|---|
| v68 | Snapdragon 888 | ~15 | 4 MB |
| v69 | SD 8 Gen 1 | ~18 | 4 MB |
| **v73** | SD 8 Gen 2 | ~26 | **8 MB** ← VTCM doubling |
| v75 | SD 8 Gen 3 | ~36 | 8 MB |
| **v79** | Snapdragon X Elite (laptop) | **~45** | **8 MB** |

Other anchors: HMX = 4096 MACs/instruction; FastRPC wake ~100 µs; real bottleneck is shared DDR (~60 GB/s on X Elite).

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

> "An AI workload on an NPU runs through six layers — framework, ONNX, ONNX Runtime, Execution Provider, vendor runtime/compiler, silicon. ORT partitions the graph: contiguous subgraphs go to QNN EP for Qualcomm Hexagon, OpenVINO EP for Intel, DirectML for any DX12 device, and CPU EP is always present as the fallback. The fallback is the silent perf cliff — one unsupported op forces a per-op partition and you spend most of your time copying tensors between NPU and host memory. The vendor compiler does QDQ fusion to collapse Quantize-Conv-Dequantize triples into a single INT8 op, then tiles activations so each tile fits in on-chip SRAM — Qualcomm calls it VTCM, 8 MB on HTP v79, vs Intel about 2 MB and AMD about 4. The compiled artifact is serialised as a context cache so the second launch skips compile. At inference the CPU writes a doorbell, the NPU DMAs input from DDR to VTCM, and HMX, HVX, DMA run as a 3-engine pipeline — that's how you hit roughly 45 TOPS on v79. On Windows I observe all of this with ETW and WPA, the equivalent of `perf` on Linux."

---

## Practice Q&A

**Q1. Six layers from model to silicon — name each.**
Framework (PyTorch/TF) → ONNX → ORT → EP (QNN / OpenVINO / DirectML / CPU) → vendor runtime + compiler → NPU silicon.

**Q2. Why is EP fallback the silent perf cliff?**
One unsupported op → partition splits → per-op tensor copy NPU↔host. Functional pass; throughput collapses. Detect by counting `EP_assignment == CPU` ops.

**Q3. What is QDQ fusion and what happens when it fails?**
Compiler pass collapsing `Quantize → Op → Dequantize` into a single INT8 op. If it fails (rebuild without `keep_quantized_weights`, scale mismatch, unrecognised pattern), Q/D stay as standalone ops and the op drops to CPU EP.

**Q4. What is VTCM and what's the tile-size cliff?**
Hexagon HTP's on-chip SRAM (8 MB on v79), software-managed, not in CPU coherence. Tile-size cliff: compiler picks a tile too big → multiple DMA trips per tile → per-tile latency doubles.

**Q5. Walk the NPU execute hot loop.**
CPU writes doorbell → NPU DMA DDR→VTCM → HMX matmul + HVX requant + DMA next tile (3-engine pipeline) → DMA VTCM→DDR → MSI-X → ORT returns tensor.

**Q6. What is a context cache and why does it matter?**
Serialised compiled artifact (`*.bin`); second launch loads it via `QnnContext_createFromBinary()` and skips compile. Missing cache = seconds of first-frame latency every launch.

**Q7. NPU vs GPU at interview depth.**
NPU: fixed-function INT8 dataflow accelerator, software-managed SRAM, deterministic latency, very high TOPS/W on quantised models. GPU: programmable shader cores, hardware-coherent caches, FP throughput, more flexible but lower perf/W on quantised inference. NPUs trade flexibility for efficiency.

**Q8. Why is "more partitions = worse perf"?**
Each partition boundary is a tensor copy out of NPU memory back to host. Cost is proportional to tensor size × boundary count.

**Q9. Hexagon HTP family — which generation doubled VTCM?**
v73 on SD 8 Gen 2: 4 MB → 8 MB. v75 (SD 8 Gen 3) and v79 (X Elite) both kept 8 MB.

**Q10. What does ETW give you that `perf stat` doesn't?**
Windows kernel + provider-specific events with timestamps in one trace: ORT EP events, QNN DLL loads, GPUView NPU queue timeline, memory counters (VirtualAlloc, page faults), CPU sampling — correlated in WPA.

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
- [ ] Stated the HTP family numbers (v73 doubled VTCM; v79 ≈ 45 TOPS / 8 MB).
- [ ] Named ETW + WPA as the Windows observation tool chain.

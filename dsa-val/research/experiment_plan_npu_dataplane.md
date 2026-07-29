# Experiment plan — proving the NPU "data-plane tax" (client) + mover benefit

> **Tracked in the private repo** (private + 2FA). Public-source only — no Intel-internal data. Companion to [`proposal_npu_dsa_mover.md`](proposal_npu_dsa_mover.md) (current proposal).
> Goal: a repeatable experiment **you can run on your own NPU laptop** that proves the near-term paper's
> claim, plus a **brief** server (AMX+DSA) sketch to detail later.

---

## 0. What we are proving (hypotheses)

- **H1 — the tax exists:** a non-trivial fraction of end-to-end NPU inference time/energy is **host-side
  data plane** (pre/post-processing, quant/dequant, layout conversion, host↔NPU staging copies), not NPU compute.
- **H2 — it grows for GenAI:** the fraction is larger for **dynamic-shape / LLM** workloads (tokenization,
  KV handling, dynamic-shape padding, weight dequant, CPU/GPU fallback) than for a static CNN.
- **H3 — offloading helps:** moving that data plane to a **DSA-type mover** (overlapped with NPU compute)
  would free CPU cycles and cut latency/energy. **Estimated** on the client here; **measured** on the server later.

**Success artifact:** a bar chart "host data-plane tax vs NPU compute" (CNN vs LLM) + a freed-cycles / ideal-overlap estimate.

---

## 1. What you need (your NPU laptop)

**Confirm the NPU is live**
- Windows: Task Manager → Performance → **NPU**; or Device Manager → *Neural processors*. Install the Intel NPU driver.
- Linux (Ubuntu 22.04+, kernel 6.6+): `intel_vpu` module + `/dev/accel/accel0`; install the [linux-npu-driver](https://github.com/intel/linux-npu-driver).

**Software**
```bash
pip install openvino openvino-genai "optimum[openvino]" nncf psutil numpy
# benchmark_app ships with the openvino wheel (console script)
```
**Measurement tools**
- Windows: **Intel PCM** (`pcm.exe`) or **HWiNFO64** (logging) for CPU util + package power; PresentMon optional.
- Linux: `perf stat`, `turbostat`, and RAPL sysfs `/sys/class/powercap/intel-rapl:0/energy_uj`; `psutil` for per-process CPU time.

**Models (download locally)**
| Class | Model | Why |
|---|---|---|
| CNN (static, NPU-friendly) | ResNet-50 or MobileNetV2, **INT8** | Baseline: little data-plane tax expected |
| Speech | whisper-base | Real pre/post-processing (audio features, decode) |
| LLM (dynamic) | TinyLlama-1.1B / Phi-3-mini / Qwen2.5-1.5B, **INT4** | Where the tax + fallback should grow |

Export an LLM to OpenVINO IR:
```bash
optimum-cli export openvino -m TinyLlama/TinyLlama-1.1B-Chat-v1.0 --weight-format int4 tinyllama-ov
```

---

## 2. Method / measurement hygiene (do this or numbers are noise)

- **Async inference** (`start_async` + `wait`) when measuring CPU load, so you can show the CPU is *free*
  while the NPU computes; **per-stage sync timing** for the breakdown.
- **Warm-up:** discard the first ~20 iterations (first-inference includes compile/load — FEIL/FIL).
- **Repeat:** N ≥ 200 iterations; report **median + p95**, not mean.
- Fix the input; set the **performance power plan** (plugged in); close background load; log wall-clock timestamps per phase.

---

## 3. Protocol (phase by phase)

### P1 — Device baseline
```bash
benchmark_app -m resnet50_int8.xml -d NPU  -hint latency -pc -niter 200
benchmark_app -m resnet50_int8.xml -d CPU  -hint latency -niter 200
benchmark_app -m resnet50_int8.xml -d AUTO:NPU,CPU -hint latency -pc -niter 200
```
`-pc` dumps per-op exec type + **which device each op ran on** → for AUTO/HETERO this reveals **fallback** (ops that did NOT run on the NPU). Record NPU latency, and the fallback op count/time.

### P2 — Instrument the pipeline (the core measurement)
Time each stage separately with `perf_counter_ns`, plus per-op profiling:
```python
import time, numpy as np, openvino as ov
core = ov.Core()
cm  = core.compile_model(core.read_model("resnet50_int8.xml"), "NPU",
                         {"PERF_COUNT": True})   # enable profiling
req = cm.create_infer_request()

def ms(t0): return (time.perf_counter_ns() - t0) / 1e6
rows = []
for i in range(220):
    t = time.perf_counter_ns(); x = preprocess(img);              t_pre = ms(t)   # decode/resize/normalize (or tokenize)
    t = time.perf_counter_ns(); req.set_input_tensor(ov.Tensor(x)); t_h2d = ms(t)  # host->NPU staging copy
    t = time.perf_counter_ns(); req.infer();                       t_inf = ms(t)   # NPU compute
    t = time.perf_counter_ns(); y = req.get_output_tensor().data.copy(); t_d2h = ms(t)  # NPU->host copy
    t = time.perf_counter_ns(); post = postprocess(y);             t_post = ms(t)  # argmax / detokenize / NMS
    if i >= 20: rows.append((t_pre, t_h2d, t_inf, t_d2h, t_post))

a = np.array(rows)
print("median ms [pre, h2d, infer, d2h, post] =", np.median(a, 0))
dp = a[:, [0,1,3,4]].sum(1); tot = a.sum(1)
print("DATA-PLANE FRACTION (median) =", float(np.median(dp / tot)))

# per-op: which ops actually executed on the NPU vs optimized-out / fallback
for p in req.get_profiling_info():
    print(p.node_name, p.exec_type, p.status, p.real_time)
```
> NPU per-op counters are coarser than CPU — the **reliable signal is your own per-stage timing** (pre/h2d/d2h/post). Treat `get_profiling_info()` as corroboration for fallback.

### P3 — CPU load + energy per phase
- **Linux:** wrap each phase (or the whole run) in `perf stat -e task-clock,cycles,instructions -p <pid>`; read RAPL `energy_uj` before/after; `psutil.Process().cpu_times()` deltas per phase.
- **Windows:** run `pcm.exe 0.1` (or HWiNFO logging) during the run; align to your timestamp log; use `psutil` for per-process CPU time per phase.
- **Metric:** CPU busy-ms / cycles / energy spent in **data-plane phases** vs the **infer** phase. During async `infer`, CPU should be low → confirms those cycles are the offloadable target.

### P4 — Contrast CNN vs LLM
Repeat P2–P3 for the **LLM** (via `openvino-genai` `LLMPipeline(model, "NPU")`, or HETERO). Expect the data-plane
+ fallback fraction to be **larger** (tokenization, dynamic-shape padding, weight dequant, KV handling, CPU fallback).
This contrast is the paper's punchline.

### P5 — Estimate the mover benefit (no DSA on client)
1. **Analytical (primary):** with an async mover overlapping data movement and NPU compute,
   `latency_ideal ≈ max(t_infer, t_dataplane)` instead of the current **sum**; freed CPU cycles = measured
   data-plane cycles from P3. Report the ideal speedup and cycles/energy freed.
2. **Proxy (optional, label clearly as proxy ≠ DSA):** offload the copy/convert stage to a **separate CPU
   thread or the iGPU** to demonstrate the overlap is real and measure a lower-bound latency reduction.

---

## 4. Outputs (what goes in the paper/slide)

- **Table:** median ms per stage [pre, h2d, infer, d2h, post] for CNN vs LLM.
- **Figure (the money shot):** stacked/side-by-side bar — **data-plane tax vs NPU compute**, CNN vs LLM.
- **Numbers:** data-plane fraction (%), CPU cycles/energy in the data plane, ideal-overlap speedup.
- **Per-op data points** (feed the proposal's optimization table): for each offloadable op (dequant, embedding/KV gather, KV move, layout, host↔NPU staging) capture **bytes/token, baseline CPU cycles, energy, and its share of the data plane** → fills the `<TBD>` cells in [`proposal_npu_dsa_mover.md`](proposal_npu_dsa_mover.md).

---

## 5. Interpreting results

| Result | Meaning |
|---|---|
| Data-plane fraction small for CNN, large for LLM | ✅ H1+H2 — mover matters most for on-device GenAI |
| High CPU busy-ms during pre/h2d/d2h/post, low during infer | ✅ those cycles are the offload target |
| Many ops fall back to CPU in AUTO/HETERO for LLM | ✅ static-shape limit → host data-plane tax (extra copies) |
| Data-plane fraction tiny everywhere | ⚠️ pivot: emphasize energy, or pick pre/post-heavy pipelines (vision + audio), or larger models that spill SRAM↔DRAM |

---

## 6. Risks / caveats (state them in the paper)

- NPU per-op profiling is coarse — lean on stage-level host timers.
- The client benefit is **estimated** (no DSA on client); the proxy is a proxy, not a DSA measurement.
- Energy tooling varies (Intel Power Gadget is EOL → use PCM / HWiNFO / RAPL).
- Ensure the model actually runs on the NPU (static shapes); document any fallback.

---

## 7. Server sketch (AMX + DSA) — BRIEF, detail later

> This is the **measured** counterpart to the client's estimate: same "offload the data plane" idea, but on
> a Xeon where a real DSA exists and **AMX** plays the NPU's compute role. Extends the current experiment.

- **Platform:** DSA-capable **Xeon** (SPR/EMR/GNR) with **AMX**; DSA enabled in BIOS + `accel-config`.
- **Runtime:** llama.cpp (AMX/oneDNN) or OpenVINO CPU plugin (uses AMX) running the **same local models**.
- **Offload one data-plane op to DSA** via `accel-config` + Intel **DML**/`idxd`:
  - dequant (INT8/INT4→bf16) → **Type Conversion**; embedding gather → **Scatter-Gather**; KV-cache move → **Memory Move**.
- **Measure:** CPU core-cycles freed (`perf`), tokens/sec, latency, energy (RAPL) — CPU-memcpy/convert baseline vs DSA-offload.
- **Two-tier:** (1) microbenchmark of the op at inference-realistic sizes; (2) stretch: wire it into the runtime end-to-end.
- **Story link:** client paper measures the **opportunity**; server paper measures the **mechanism** → they cite each other.

---

## 8. Next actions

- [ ] Pick the first model (suggest ResNet-50 INT8 for a clean P1–P3 dry run, then TinyLlama INT4 for the contrast).
- [ ] Confirm energy tool available on your laptop (PCM / HWiNFO / RAPL).
- [ ] I can generate the full runnable timing script (vision + LLM variants) into `research/` when you're ready.
- [ ] After data: build the bar figure + fill the results table; then draft the abstract.

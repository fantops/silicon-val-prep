# Day 19 — NPU Memory, Runtime & Scheduling Bugs

## Concept

Once the NPU stack of Day 18 is in your head, **the bugs you actually see in production** fall into four buckets: (1) **memory-lifecycle regressions** in the host runtime — page-fault spike, virtual-memory bloat — that **look like silicon defects but aren't**; (2) **scheduler bugs** — single-queue HTP, no preemption, one workload starves another; (3) **silent EP fallback** — looks fine functionally, perf 10× off; (4) **power / Modern Standby** — NPU stays in active power state when it should idle. The discipline that keeps you out of trouble: **never blame silicon first**. Build a version matrix, separate variables, correlate counters together (page faults *and* virtual memory, not either alone).

## Why this day in the flow

Day 18 was the architecture; Day 19 is the **stories an interviewer will ask about**. These are the bugs you actually own at Microsoft and the ones a Qualcomm/Intel NPU role wants to hear you reason through. Every bug on this page traces back to a Day 1-17 fundamental.

In the Layer map this is the **debug-storytelling layer for NPU work**.

---

## Deep Study Notes

### 1. The four NPU bug families

| Family | Symptom | Where it lives in the stack |
|---|---|---|
| **Memory-lifecycle regression** | page-fault spike, VM bloat | host runtime / EP context-loading path |
| **Scheduler / concurrency** | one workload starves another, frame drops | NPU command queue, HTP single-queue + no preempt |
| **Silent EP fallback** | functional pass, perf 10× off | EP partition decision; one unsupported op |
| **Power / Modern Standby** | NPU stays active when it should idle | EP/runtime keep-alive, MS S0ix residency |

### 2. Bug 1 — QNN/ORT context-loading page-fault and virtual-memory regression (Microsoft + Qualcomm)

**Honest framing:** post-silicon **platform** validation on Copilot+ PC; you correlated OS counters and EP/ORT versions. **You did not debug Hexagon RTL or claim a VTCM silicon defect.** Host-runtime / EP memory-lifecycle regression exposed by an NPU workload.

```
SITUATION   PSMU AI workload, Qualcomm Copilot+ PC, ORT QNN EP. Workload uses
            QNN context binaries and HTP/VTCM planning, so startup and inference
            memory behaviour matters: multiple model contexts loaded together
            can spike host VM and page faults before NPU compute is even the
            bottleneck.

BUG         Drop EP 2.39 / ORT 1.23.3 stable: ~7 045 page-fault events,
            ~300 MB virtual memory during inference.
            Drop EP 2.45 / ORT 1.24.4: VM during inference grew to ~502 MB.
            One isolated cross-run: page faults jumped to ~102 445 when
            multiple model context buffers were loaded together.

APPROACH    1. Treated it as a version-regression problem first, not silicon.
            2. Built a version matrix: known-good EP 2.39 + ORT 1.23.3 vs
               EP 2.45 + ORT 1.24.4. Then EP 2.45 + ORT 1.23.3 to separate
               variables. The reduced VM in that combo proved it wasn't
               "new EP is bad" or "VTCM too small."
            3. Correlated ETL/WPA counters: page-fault count, VirtualAlloc
               activity, peak committed/virtual memory.
            4. Source-level pointer: ORT QNN backend context-loading path,
               around qnn_backend_manager.cc::CreateContextFromListAsync().
               Newer path lets multiple context binaries/buffers accumulate
               together instead of load → create → release per context.

ROOT CAUSE  Host-runtime memory-lifecycle regression in ORT/QNN EP context
            loading. Loading/accumulating contexts together touches many
            virtual pages in a short window → demand-zero / minor page-fault
            burst. Later mitigation reduced the burst by holding more
            virtual memory alive → explains the 300 MB → 502 MB increase.

FIX         Recommend bounded context lifetime: load/map one context, create
            QNN context, release temp host buffer, then the next — unless
            parallel loading is truly needed. For product, hold on the
            known-good EP/ORT combo or take the fix only after validating
            both counters together (faults stay controlled AND VM does not
            regress). Added a regression gate recording EP version, ORT
            version, page-fault count, VirtualAlloc count, peak VM.

IMPACT     Avoided misclassification as a Qualcomm silicon / VTCM bug.
           Narrowed the regression to host runtime; gave the owning team
           a precise version matrix and source area.

LEARNING   Page faults and virtual memory must be interpreted together.
           A page-fault spike can mean the runtime is touching many newly
           allocated virtual pages — not that NPU compute is failing.
           Reducing page faults by keeping larger mappings alive can hide
           one symptom while creating another (higher steady-state VM).
           Always correlate EP version, ORT version, context-cache
           behaviour, page faults, VirtualAlloc, peak memory before
           blaming hardware.
```

**90-second spoken version:**

> On the Qualcomm QNN EP path for a PSMU model, we saw a memory regression across EP/ORT drops. Known-good EP 2.39 with ORT 1.23.3 had roughly 7 000 page-fault events and around 300 MB virtual memory during inference. With EP 2.45 and ORT 1.24.4 virtual memory grew to about 502 MB, and in one isolated run page faults spiked to ~102 K. I did not treat that as a VTCM or silicon defect first. I built a version matrix: old EP/old ORT, new EP/new ORT, then new EP with old ORT. That showed the behaviour followed the ORT/QNN context-loading path. Trace and source correlation pointed to the newer path accumulating multiple model context buffers together before creating contexts. Touching many host virtual pages at once caused the page-fault spike; the later mitigation reduced that symptom by keeping more virtual memory alive — that explained the 300→502 MB increase. The fix direction was bounded context lifetime: load, create, release per context unless parallel loading is required, and gate both counters together in regression. The key learning was: page faults alone are not the bug. You correlate them with VirtualAlloc, committed memory, ORT version, EP version and context-cache lifetime before blaming NPU hardware.

### 3. Bug 2 — Paint SR starves Camera preview (Microsoft, post-Si platform)

```
SITUATION   Two AI workloads share the NPU on Copilot+ PC (Snapdragon X Elite,
            HTP v79, 8 MB VTCM). Paint Super-Resolution submits ~36 inference
            tiles back-to-back to upscale a still; Camera preview submits a
            fresh inference at 30 fps (~33 ms budget). Both run via QNN EP
            on the same NPU.

BUG         When Paint SR runs, Camera preview drops frames. GPU-only path
            clean. Reproduced intermittently in user complaints.

APPROACH    1. Built a deterministic repro: tiny C exe fires both workloads
               on the same instant — CreateSemaphore, ReleaseSemaphore(2),
               QueryPerformanceCounter to flag any frame > 33 ms as DROPPED.
               100% reproducible.
            2. Captured GPUView timeline. Camera frame 0 queued at t=3 ms but
               did not start executing on the NPU until t=108 ms — about
               3.2 frame periods at 30 fps. SR's 36 tiles ran back-to-back
               on the NPU command queue ahead of it.
            3. Cross-checked with the IHV: HTP exposes a single hardware
               scheduler with no preemption. Once a tile is dispatched it
               runs to completion; FIFO ordering at the queue.

ROOT CAUSE  Single-queue, non-preemptive NPU scheduler + no priority hint
            from the framework. SR's burst of tiles fills the queue; Camera's
            single tile waits behind all of them.

FIX         Two complementary directions: (a) framework / app side — submit
            SR tiles in smaller batches and yield the queue; tag Camera as
            high-priority so the runtime interleaves; (b) IHV-side — request
            a future HTP scheduler with priority lanes or preemption between
            tiles. Until then, gate concurrent NPU workloads with a priority
            hint.

LEARNING    A single hardware scheduler + no preemption is a structural
            limitation, not a bug; but the framework must expose priority
            so apps don't accidentally starve each other. Useful concurrency
            archetype: "one big batch behind one small frame-budget request."
```

### 4. Bug 3 — Silent EP fallback after model rebuild

```
SITUATION   ResNet-class model running on QNN EP, perf good. Team rebuilt
            the model with an updated quantisation toolchain and dropped
            it into the same harness.

BUG         Same workload, same harness, same NPU — inference 8× slower.
            CPU% during the inference window is high; QNN EP DLL is loaded
            (we checked).

APPROACH    1. Counted EP_assignment per op via ORT verbose log: most ops
               had silently moved to CPUExecutionProvider.
            2. Compared ONNX graphs old vs new: new build had standalone
               Quantize/Dequantize nodes, not the QDQ-wrapped pattern the
               compiler expects.
            3. Found the rebuild had dropped keep_quantized_weights=true.

ROOT CAUSE  Without keep_quantized_weights, QDQ fusion didn't recognise
            the pattern; ops dropped off NPU; per-op tensor copies ran the
            inference on CPU EP.

FIX         Restore the quantisation export option; re-validate the QDQ
            fusion in the compile log; gate the build on
            "EP_assignment == NPU for >= N% of ops."

LEARNING    Silent fallback presents as a silicon perf issue; it is almost
            always a model-build or EP-version issue. Always check the EP
            assignment log before opening a silicon ticket.
```

### 5. Bug 4 — VTCM tile-size cliff

```
SITUATION   New larger model variant deployed on HTP v79 (8 MB VTCM).

BUG         Per-tile latency doubled from ~3 ms to ~6 ms; throughput
            collapsed by ~2×.

APPROACH    1. QNN profile dump showed each tile now took 2 DMA round-trips
               instead of 1.
            2. Tile size from the compile report was just over the VTCM
               budget for this layer's working set; previous variant fit.

ROOT CAUSE  Compiler picked tile geometry that overflowed VTCM by a small
            margin; activations + weights + halo no longer fit in 8 MB.

FIX         Hint the compiler with a smaller tile shape; or split the layer.

LEARNING    "Almost fits" in VTCM is a 2× cliff, not a smooth degradation.
```

### 6. Bug 5 — NPU keeps platform out of Modern Standby

```
SITUATION   Modern Standby (S0ix) target residency at idle; NPU recently
            enabled via QNN EP background warm-up.

BUG         Battery drain at idle 30% above target; PkgC10 residency low.

APPROACH    1. Read S0ix residency and PkgC10 counters: low when NPU was
               loaded, normal otherwise.
            2. ORT/QNN held NPU power state high to keep first-frame
               latency low.

ROOT CAUSE  Runtime keep-alive policy held the NPU in active power state
            during idle; blocked package idle entry (Day 13).

FIX         Switch to a release-on-idle policy with a short re-warm-up
            window; accept slightly higher first-frame latency for the
            S0ix residency target.

LEARNING    Power and perf are negotiated, not stacked. NPU keep-alive is
            a perf optimisation that violates a power KPI; both must
            appear in the regression gate.
```

### 7. The triage discipline (interview pattern)

For any "NPU bug" report:

1. **Reproduce deterministically.** Tiny harness, semaphore-fired threads, QPC timestamps, fail criterion as a number.
2. **Build a version matrix.** EP version × ORT version × QNN SDK × model build. Change one variable at a time.
3. **Correlate counters together.** Page faults *and* VirtualAlloc *and* peak VM. Not one alone.
4. **Read the EP-assignment log.** Are ops actually on NPU?
5. **Check the compile / context-cache log.** Tile size? VTCM fit? QDQ fusion happened?
6. **Cross-check with IHV** (Qualcomm / Intel / AMD) before claiming a silicon defect.
7. **Add a regression gate** that records every variable in the matrix.

---

## Bugs in this Topic (recap table)

| # | Bug | Root cause | Tell |
|---|---|---|---|
| 1 | QNN/ORT page-fault spike | host runtime context-loading regression | matrix isolates ORT version |
| 2 | Paint SR starves Camera | single HTP queue, no preempt | GPUView shows 108 ms wait |
| 3 | Silent EP fallback | QDQ fusion broke after rebuild | EP-assignment log shows CPU |
| 4 | VTCM tile-size cliff | tile barely overflowed 8 MB | per-tile latency doubled |
| 5 | NPU blocks S0ix | runtime keep-alive policy | PkgC10 residency low |

---

## Interview Explanation (60-90 sec)

> "The NPU bugs I see in platform validation fall into four families. First, memory-lifecycle regressions — the canonical one I worked on was a QNN/ORT context-loading change where page faults spiked to ~100 K and virtual memory grew from 300 to 500 MB. I did not treat it as a silicon defect; I built a version matrix across EP and ORT, separated variables, and the new EP with the old ORT gave a different footprint. That isolated the bug to the host runtime context-loading path, not VTCM. The lesson: page faults alone are not the bug — correlate them with virtual-alloc and peak memory. Second, scheduler bugs: HTP is a single hardware queue with no preemption, so a Paint Super-Resolution job submitting 36 tiles starves a Camera preview behind it; GPUView timeline showed Camera waiting 108 ms behind SR. Fix is priority lanes from the framework. Third, silent EP fallback — one unsupported op or a broken QDQ pattern after a rebuild and ops drop to CPU EP; functional pass, perf 10× off; you spot it in the EP-assignment log. Fourth, power: an NPU keep-alive policy held the device active and blocked Modern Standby residency; you can't optimise first-frame latency at the cost of S0ix. The triage discipline is always reproduce, version-matrix, correlate counters together, and never blame silicon first."

---

## Practice Q&A

**Q1. Page-fault count jumped 14×, virtual memory grew 67%. Silicon bug?**
No — almost certainly host runtime memory lifecycle. Build version matrix; correlate page faults with VirtualAlloc and peak VM together.

**Q2. Camera preview drops frames whenever Paint SR runs — root cause path?**
Single non-preemptive NPU command queue. SR's 36 tiles run back-to-back ahead of Camera's tile. GPUView shows the wait.

**Q3. ORT verbose log says half your ops landed on CPUExecutionProvider. What broke?**
Silent EP fallback. Likely cause: QDQ pattern not recognised after rebuild; check `keep_quantized_weights`. Or EP version mismatch; or unsupported op variant.

**Q4. Per-tile latency on a model doubled with no code change. First check?**
QNN profile + compile log. Suspect tile-size cliff: tile no longer fits in VTCM; one compute tile = two DMA trips.

**Q5. PkgC10 residency dropped after enabling NPU EP. Why?**
NPU runtime keep-alive holds device in active power state; blocks package idle entry (Day 13).

**Q6. EP fallback vs scheduler bug — how do you tell them apart?**
EP fallback: CPU% high during inference; EP-assignment log shows CPU ops. Scheduler bug: NPU command queue is busy; latency is wait-time, not compute-time; GPUView shows waiting at queue head.

**Q7. Why is "never blame silicon first" the rule?**
Most NPU bug reports are runtime / EP / model / build issues. A wrong silicon claim costs IHV-side time and can derail a tape-out plan. Cross-check IHV only after you've isolated to a layer.

**Q8. What goes in an NPU regression gate?**
EP version, ORT version, QNN SDK version, model build hash, EP-assignment ratio (%NPU), per-tile latency, page-fault count, peak VM, S0ix residency.

**Q9. The "version matrix" — what is it and why?**
Cartesian product of independently-versioned components (EP × ORT × SDK × model build). Vary one at a time to isolate which version step caused the regression. Without the matrix you guess.

**Q10. Honest framing on the QNN page-fault story — what did you NOT do?**
Did not debug Hexagon RTL, did not claim a VTCM hardware defect. Worked at the platform layer with OS counters, ORT/QNN logs, and source-code pointers.

---

## Coding A — Systems C: NPU regression-gate harness

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    const char *ep_version;
    const char *ort_version;
    const char *model_build;
    int     npu_op_pct;          /* % of ops assigned to NPU EP */
    int     page_faults;
    int     peak_vm_mb;
    double  per_tile_ms;
    int     s0ix_residency_pct;
} run_t;

typedef struct {
    int min_npu_op_pct;
    int max_page_faults;
    int max_peak_vm_mb;
    double max_per_tile_ms;
    int min_s0ix_residency_pct;
} gate_t;

static bool gate_check(const gate_t *g, const run_t *r) {
    bool ok = true;
    if (r->npu_op_pct        < g->min_npu_op_pct)        { printf("  FAIL npu_op_pct %d < %d\n", r->npu_op_pct, g->min_npu_op_pct); ok = false; }
    if (r->page_faults       > g->max_page_faults)       { printf("  FAIL page_faults %d > %d\n", r->page_faults, g->max_page_faults); ok = false; }
    if (r->peak_vm_mb        > g->max_peak_vm_mb)        { printf("  FAIL peak_vm_mb %d > %d\n", r->peak_vm_mb, g->max_peak_vm_mb); ok = false; }
    if (r->per_tile_ms       > g->max_per_tile_ms)       { printf("  FAIL per_tile_ms %.2f > %.2f\n", r->per_tile_ms, g->max_per_tile_ms); ok = false; }
    if (r->s0ix_residency_pct < g->min_s0ix_residency_pct){ printf("  FAIL s0ix %d < %d\n", r->s0ix_residency_pct, g->min_s0ix_residency_pct); ok = false; }
    return ok;
}

int main(void) {
    gate_t gate = {
        .min_npu_op_pct = 90,
        .max_page_faults = 10000,
        .max_peak_vm_mb = 350,
        .max_per_tile_ms = 4.0,
        .min_s0ix_residency_pct = 80,
    };

    run_t known_good = {"EP 2.39", "ORT 1.23.3", "psmu_v1", 95, 7045,  300, 3.0, 85};
    run_t regressed  = {"EP 2.45", "ORT 1.24.4", "psmu_v1", 92, 102445, 502, 3.1, 84};
    run_t fallback   = {"EP 2.45", "ORT 1.24.4", "psmu_v2", 40, 8000,  310, 5.0, 84};

    printf("Known-good run [%s + %s]\n", known_good.ep_version, known_good.ort_version);
    printf("  result: %s\n\n", gate_check(&gate, &known_good) ? "PASS" : "FAIL");

    printf("Regressed run  [%s + %s]\n", regressed.ep_version, regressed.ort_version);
    printf("  result: %s\n\n", gate_check(&gate, &regressed) ? "PASS" : "FAIL");

    printf("Fallback run   [%s + %s]\n", fallback.ep_version, fallback.ort_version);
    printf("  result: %s\n", gate_check(&gate, &fallback) ? "PASS" : "FAIL");
    return 0;
}
```

## Coding B — Interview DS&A: LRU Cache (LC 146)

Hashmap + doubly linked list — same data structure that backs an EP context cache.

```python
class Node:
    __slots__ = ('k', 'v', 'p', 'n')
    def __init__(self, k=0, v=0): self.k, self.v, self.p, self.n = k, v, None, None

class LRUCache:
    def __init__(self, cap):
        self.cap = cap
        self.m = {}
        self.h = Node(); self.t = Node()
        self.h.n = self.t; self.t.p = self.h

    def _remove(self, x):
        x.p.n = x.n; x.n.p = x.p

    def _add_front(self, x):
        x.n = self.h.n; x.p = self.h
        self.h.n.p = x; self.h.n = x

    def get(self, k):
        if k not in self.m: return -1
        x = self.m[k]; self._remove(x); self._add_front(x); return x.v

    def put(self, k, v):
        if k in self.m:
            x = self.m[k]; x.v = v; self._remove(x); self._add_front(x); return
        if len(self.m) == self.cap:
            lru = self.t.p; self._remove(lru); del self.m[lru.k]
        x = Node(k, v); self.m[k] = x; self._add_front(x)
```

---

## System Thinking — Why NPU bugs look like model bugs (but aren't)

**Frame:** when an inference engine on an NPU produces wrong results, hangs, or runs 10× slower than expected, the first instinct is "the model is wrong" or "the quantization is off." Usually it isn't. The bug is almost always at the **boundary** between two domains — CPU↔NPU memory, host VA↔device IOVA, coherent↔non-coherent fabric, runtime↔driver — because each domain works correctly in isolation but the contract between them is fragile. *This is the highest-value validation skill for AI accelerators.*

### Q: Why is the CPU↔NPU memory contract so error-prone?

Because the two domains have different assumptions:

| Property | CPU side | NPU/HTP side |
|---|---|---|
| Addressing | Virtual (per-process) | IOVA (per-IOMMU context) or physical |
| Cache | Coherent within CPU cluster | Often non-coherent with CPU caches |
| Page size | 4 KB / huge pages | Often only 4 KB or device-specific |
| Pinning | Pages may be swapped | Must be pinned for DMA |
| Memory model | TSO / weak (with barriers) | Usually weak, vendor-specific |

A buffer the CPU thinks it wrote is, from the NPU's view: in the wrong address space (no IOMMU map), behind a dirty cache line (need clean), in a swappable page (need pin), or visible only after a barrier the driver forgot. **All four bug classes look identical from the model's perspective: "output is wrong / NaN / zero / stale."**

### Q: How do the three classic miss types converge here?

Day 4-7 distinguished cache miss / TLB miss / page fault. On a heterogeneous NPU path *all three exist on both sides*:
- **Page fault on CPU side**: model weights mmap'd from disk, first touch faults. **Cold-start latency anomaly** the user blames on the NPU.
- **TLB miss on CPU side**: huge weight tensor, walks 4-level page table per access. Fix: huge pages.
- **IOMMU TLB miss on NPU side**: similar to CPU TLB miss but for the device. SMMU does a page walk on every cold IOVA. Fix: pinned + huge IOMMU mappings.
- **Cache coherence miss across CPU↔NPU**: the bug Day 8 warned about.

*Diagnostic discipline:* never say "the NPU is slow" — say "there's a 3 ms anomaly; let me check whether it correlates with major faults, TLB MPKI, IOMMU faults, or cache management calls." That's the difference between an L3 engineer and an L5.

### Q: Why do FastRPC / EP runtime layers exist between the model and the NPU?

Because the NPU is a separate process domain (PD) on a separate processor (Hexagon). You cannot just call into it like a function. **FastRPC** (Qualcomm's RPC over shared memory) and equivalent runtimes (Apple's IOSurface + neural-engine, Intel's NPU UMD) are the marshalling layer:
1. CPU prepares input tensor in shared memory.
2. RPC packs handles, IOMMU maps, calls into the kernel driver.
3. Driver wakes the NPU PD, hands off the work descriptor.
4. NPU executes, writes output to shared memory.
5. RPC returns; CPU sees output.

**Each step is a place a bug can hide:** wrong handle, wrong size, wrong offset, missed cache flush, missed IOMMU map. The bug usually fires only on long sequences or under load, because the early calls happen to leak handles slowly until exhaustion.

### One-liner

*"NPU bugs almost always live at the boundary — CPU↔NPU memory, VA↔IOVA, coherent↔non-coherent, runtime↔driver — because each domain is correct in isolation but the contract is fragile. The first diagnostic is to attribute slowdowns to a specific miss class (page fault, TLB, IOMMU, coherence), not to blame the model. FastRPC / EP runtimes are the marshalling layer where most leak/handle bugs hide."*

---

## Bug Anchor

- **Day 6 (virtual memory):** the page-fault counter that Bug 1 hangs on.
- **Day 8 (DMA / doorbells):** Paint SR's tiles all go through the doorbell-DMA-MSI sequence on the single HTP queue.
- **Day 13 (PkgC / S0ix):** Bug 5 — NPU keep-alive blocks package idle entry.
- **Day 15 (multi-IP / perf):** EP fallback shows up as a 5-class interference pattern (CPU↔NPU copy bandwidth).
- **Day 17 (concurrency):** the producer-consumer / single-queue pattern in Bug 2.
- **Day 18 (NPU stack):** every term used on this page is defined there.

## Exit Check

- [ ] Told the QNN page-fault story in 90 seconds with version-matrix as the centrepiece.
- [ ] Told the Paint SR vs Camera story with GPUView wait time as the evidence.
- [ ] Stated the silent EP fallback detection (EP-assignment log).
- [ ] Stated the VTCM tile cliff as a 2× discontinuity, not smooth.
- [ ] Stated NPU keep-alive vs Modern Standby tension.
- [ ] Have the 7-step triage discipline ready to recite.
- [ ] Have honest framing ready: platform-validation, not Hexagon-RTL.

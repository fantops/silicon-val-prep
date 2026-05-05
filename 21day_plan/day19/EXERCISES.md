# Day 19 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day19_npu_memory_runtime_bugs.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: NPU regression-gate harness

**Problem statement:** Build a `gate_check(g, r)` that takes a `gate_t` of thresholds and a `run_t` of measured values from one inference run, prints a `FAIL` line for each violated metric, and returns overall pass/fail.

Metrics tracked: `npu_op_pct (≥)`, `page_faults (≤)`, `peak_vm_mb (≤)`, `per_tile_ms (≤)`, `s0ix_residency_pct (≥)`. Use these to score three runs (known-good, regressed, fallback-only) and prove the gate catches each different failure mode.

**You must implement:**
```c
typedef struct { const char *ep_version, *ort_version, *model_build;
                 int npu_op_pct, page_faults, peak_vm_mb;
                 double per_tile_ms; int s0ix_residency_pct; } run_t;
typedef struct { int min_npu_op_pct, max_page_faults, max_peak_vm_mb;
                 double max_per_tile_ms; int min_s0ix_residency_pct; } gate_t;
bool gate_check(const gate_t *g, const run_t *r);
```

**Driver / tests to write:**
- Gate: `min_npu_op_pct=90, max_page_faults=10000, max_peak_vm_mb=350, max_per_tile_ms=4.0, min_s0ix=80`.
- Run A (known-good): `(95, 7045, 300, 3.0, 85)` → PASS.
- Run B (regressed): `(92, 102445, 502, 3.1, 84)` → FAIL on page faults + VM.
- Run C (fallback): `(40, 8000, 310, 5.0, 84)` → FAIL on npu_op_pct + per_tile_ms.

**Be ready to explain in interview:**
- The QNN/ORT page-fault story (~100K minor faults, VM 300→500 MB) — version-matrix isolation, "page faults alone are not the bug, correlate with virtual-alloc."
- Paint SR vs Camera (single non-preemptive HTP queue, 108 ms wait in GPUView). Fix is framework priority lanes, not silicon preempt.
- Silent EP fallback after rebuild without `keep_quantized_weights=true`.
- NPU keep-alive blocking PkgC10/S0ix.
- "Never blame silicon first." Why is this load-bearing?

## E2 — DS&A: LRU Cache (LC 146)

**Problem statement:** Same as Day 4 — implement an O(1) LRU cache (hash map + doubly-linked list). Reused here as the data structure that backs an EP context cache.
**Signature (C):**
```c
typedef struct LRU LRU;
LRU *lru_create(int cap);
int  lru_get(LRU *l, int key);
void lru_put(LRU *l, int key, int val);
void lru_free(LRU *l);
```
**Talking points:** Same DS as the EP context cache; eviction policy choice; why `move-to-front` on `get` matters for production NPU runtimes that re-use compiled artifacts.

## E3 — Architect Drills (no code, write your answers)

**D1.** "Page faults spike" is almost never silicon. What single signal *would* point at silicon?
**D2.** Should an NPU implement preemption like a GPU? Why might priority lanes from the framework be the right fix instead?
**D3.** EP fallback is silent. Construct the gate that catches it without false positives.
**D4.** Why does the runtime have a keep-alive policy at all? Who decides — runtime, framework, or OS?
**D5.** What's the smallest version matrix (EP × ORT × SDK × model build) that catches 90% of regressions? Which axis is most often the culprit?
**D6.** "Never blame silicon first." When does the rule reverse? What evidence is sufficient to escalate?
**D7.** With a coherent (CXL.cache peer) NPU, which Day-19 bug classes survive and which die? What new class appears?

## E4 — Bug Anchor

**Story 1 — QNN/ORT page-fault spike.** Walk the triage: NOT silicon; build version matrix across EP × ORT; new EP + old ORT gives different footprint → isolates to host runtime context-loading path. Page faults alone are not the bug — correlate with virtual-alloc and peak memory.

**Story 2 — Paint SR starves Camera.** GPUView shows Camera waiting 108 ms behind SR's 36 tiles on a single non-preemptive HTP queue. Fix: framework priority lanes.

**Story 3 — Silent EP fallback.** Rebuilt without `keep_quantized_weights=true` → QDQ doesn't fuse → ops drop to CPU. Detect: EP-assignment log + INT8 op count.

**Story 4 — Version-matrix regression.** ORT × QNN EP × QNN SDK forms a 3D space; "good combo" identified by the matrix.

**Story 5 — NPU blocks S0ix.** Runtime keep-alive policy holds device active; PkgC10 residency tanks.

## Reference solution

Once you've attempted, see `../day19_npu_memory_runtime_bugs.md` sections "Coding A" and "Coding B" for full solutions and discussion.

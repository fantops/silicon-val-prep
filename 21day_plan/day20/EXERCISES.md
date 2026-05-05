# Day 20 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day20_debug_method_bug_polish.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: debug-log triage helper

**Problem statement:** Implement a `triage(repro_rate, unique_units, corner_dependent)` classifier that maps a bug report to one of:

- `CLASS_FUNCTIONAL` — design/FW (`repro_rate ≥ 0.99`)
- `CLASS_RACE` — race / timing (`repro_rate < 0.50`)
- `CLASS_CORNER` — PVT (V/T/process) when corner_dependent and seen on multiple units
- `CLASS_DEFECT` — silicon defect (single unit)
- `CLASS_UNKNOWN` — otherwise

Print a small table over a fixed list of reports.

**You must implement:**
```c
typedef enum { CLASS_FUNCTIONAL, CLASS_RACE, CLASS_CORNER, CLASS_DEFECT, CLASS_UNKNOWN } class_t;
const char *class_name(class_t c);
class_t triage(double repro_rate, int unique_units, int corner_dependent);

typedef struct { const char *name; double repro; int units; int corner; } report_t;
```

**Driver / tests to write:**
- `{"ADR hang on Zebu",       1.00, 5, 0}` → FUNCTIONAL
- `{"C6 ARAT wake loss",      0.05, 4, 0}` → RACE
- `{"V-droop crash @105C",    0.40, 6, 1}` → CORNER
- `{"Single-unit boot fail",  0.95, 1, 0}` → DEFECT
- `{"NVMe CSTS.RDY hang",     1.00, 3, 0}` → FUNCTIONAL

**Be ready to explain in interview:**
- 7-step framework: observe → reproduce → isolate → hypothesise → test → root-cause → fix.
- Reproducibility rate maps to bug class (100% / intermittent / corner / single-unit).
- Cost rises 10× per stage RTL → emul → FPGA → post-Si.
- Bug-story shape: symptom → hypothesis → bounding experiment → fix → regression test, ≤90 sec.
- Honest framing ("platform validation, not Hexagon RTL").

## E2 — DS&A: Number of Islands (LC 200) — reused

**Problem statement:** Same as Day 14 — count 4-connected `'1'` regions in a grid. Reused as the canonical "isolation" pattern (also models clustering of fault sites in scan dump).
**Signature:** `int num_islands(char **grid, int rows, int cols);`
**Talking points:** O(R·C) flood-fill; same shape as connected-component analysis on a die thermal map or a scan-cell fault-cluster localisation.

## E3 — Architect Drills (no code, write your answers)

**D1.** Which step of the 7-step framework is most often skipped, and what failure does each skip predict?
**D2.** How does repro rate determine triage tooling? Why does "intermittent" cost the most engineering hours per bug?
**D3.** Cost rises 10× per stage. Where can future methodology (formal, AI-assisted) flatten the curve?
**D4.** Why "lead with the symptom" in the 90-sec story? Construct symptom-first vs story-first to show the cost.
**D5.** Why does honest framing ("I don't debug Hexagon RTL") win? Construct the question chain that exposes a bluff.
**D6.** What makes a signal "controlled" vs an anecdote? Why does the difference predict whether a fix holds?
**D7.** With 100× better debug tooling, which steps of the framework still survive? What's the irreducible human work?

## E4 — Bug Anchor (the 90-second story shape)

This whole day is bug stories. Practice each in ≤90 sec leading with the symptom:

- **Story 1 — ADR opcode-ordering hang (Day 14).** Symptom → Zebu opcode log → root cause (`WPQ_FLUSH` before `IO_QUIESCE_ACK`) → fix (explicit handshake + Perspec assertion) → lesson.
- **Story 2 — NVMe CSTS.RDY never asserts (Day 11).** Symptom → CSR snapshot → root cause (CC.EN write missed because BAR was prefetched cached) → fix (uncacheable mapping) → lesson.
- **Story 3 — Core C6 wake-loss CFC race (Day 14).** Symptom → Lauterbach trigger → root cause (FIFO mid-reset on cross-clock-domain wake) → fix (hold latch until `init_done && drain_done`) → lesson.
- **Story 4 — QNN/ORT page-fault spike (Day 19).** Symptom → version matrix → root cause (host runtime context-loading regression) → fix (rev pin) → lesson ("page faults alone are not the bug").
- **Story 4b — Paint SR vs Camera (Day 19).** Symptom → GPUView 108 ms wait → root cause (single non-preemptive HTP queue) → fix (framework priority lanes) → lesson.

## Reference solution

Once you've attempted, see `../day20_debug_method_bug_polish.md` sections "Coding A" and "Coding B" for full solutions and discussion (also: JTAG / CoreSight / lab-instrument extension at the bottom).

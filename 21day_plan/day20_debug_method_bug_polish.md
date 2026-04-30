# Day 20 — Debug Methodology and Polished Bug Stories

## Concept

Every silicon bug investigation follows the same **7-step framework**: Observe → Reproduce → Isolate → Hypothesise → Test → Root-cause → Fix & Verify. The stages of validation (RTL sim → emulation → FPGA proto → post-Si) each catch a different bug class, and the cost of finding a bug **rises ~10× per stage**, so where you find it matters as much as what you find. In an interview the bug story is delivered in **90 seconds** with a fixed shape: **Symptom → Hypothesis → How I narrowed it → Root cause → Fix → Learning**. The interviewer is not testing the bug; they are testing how your mind works.

## Why this day in the flow

Days 1-19 gave you the technical material. Day 20 is **how you talk about it**. Polished bug stories are what convert technical knowledge into hire-no-hire signal in the loop.

In the Layer map this is the **debug methodology + storytelling layer**.

---

## Deep Study Notes

### 1. The 7-step debug framework

```
1. OBSERVE      collect symptoms — exact failure, registers, logs, PVT corner
2. REPRODUCE    establish consistency — always? intermittent? specific corner?
3. ISOLATE      narrow scope — binary search, single-step, swap components
4. HYPOTHESISE  form a testable theory mapping symptom → mechanism
5. TEST         design experiment that confirms or denies one hypothesis
6. ROOT-CAUSE   identify the exact RTL / FW / silicon defect
7. FIX & VERIFY implement; re-test across all affected scenarios
```

### 2. Reproducibility as a diagnostic signal

| Reproduction rate | What it usually means |
|---|---|
| 100% | functional bug — design or FW |
| intermittent | timing issue, marginal silicon, race condition |
| only at PVT corner | voltage droop, thermal margin, process sensitivity |
| only on a specific unit | silicon defect, process outlier |

Reproduction rate is the **first** triage signal. Always quantify it before chasing the bug.

### 3. Isolation techniques

| Technique | When |
|---|---|
| Binary-search test scope | shrink failing test until minimal repro |
| Single-step via JTAG | pre-OS or low-level FW |
| Swap components | isolate to SoC vs board vs PSU |
| Frequency / voltage sweep | timing-margin vs functional |
| Register-dump diff vs golden | "what changed?" |
| Disable HW prefetch / Turbo | does the symptom move? |

### 4. Hypothesis catalog (symptom → mechanism)

| Symptom | Likely mechanism |
|---|---|
| Data corruption in memory | DMA address wrong, cache coherence, ECC failure |
| System hang | deadlock, infinite poll, link down |
| Intermittent crash at high temp | timing-margin erosion, V-droop |
| Works in emul, fails in silicon | analog, timing closure, CDC race |
| Reads return `0xFFFFFFFF` | PCIe link down, BAR not mapped, device in reset |
| Page-fault count spike | host runtime memory lifecycle (Day 19) |
| EP fallback / CPU% high | model build / EP version (Day 19) |

### 5. Validation stages — bug-cost ladder

```
Design ── Sim (Hz-kHz) ── Emul (MHz) ── FPGA (10-100 MHz) ── Post-Si (GHz)
            │                │              │                    │
            │                │              │                    └── analog, PVT, real workload
            │                │              └── SRAM timing, full SW stack
            │                └── HW/FW integration bugs (transactor-based)
            └── functional + protocol bugs (full visibility)
```

| Stage | Catches | Misses |
|---|---|---|
| **RTL sim** | functional, protocol; full visibility | analog, real timing, real OS |
| **Emulation (Zebu/Palladium)** | HW/FW integration (real FW boots) | analog, process corners, transactor latency |
| **FPGA proto** | full SW stack at MHz | SRAM timing, real silicon analog |
| **Post-Si** | analog, process corner, V-droop, thermal, real interference | nothing — it's reality |

Cost-of-finding rises ~10×/stage. A bug found in sim is "free"; one found post-tape-out can delay a product cycle.

### 6. The 5 + 1 polished bug stories

These are the stories you carry into a loop. Each is a Day 1-19 fundamental put in narrative form.

| # | Class | Layer it tests | Day anchor |
|---|---|---|---|
| 1 | **PCU / ADR opcode-ordering hang** (Intel pre-Si Zebu) | FW sequencing, ordering, ADR power-loss flow | Day 14 |
| 2 | **NVMe CSTS.RDY never asserts (AQA mis-sized)** (L&T/SanDisk) | spec literacy, host bring-up | Day 11 |
| 3 | **Core C6 wake lost on back-to-back ARAT** (Intel pre-Si) | 2-clock-domain race, CFC FIFO mid-reset | Day 14 |
| 4 | **QNN/ORT page-fault + VM regression** (Microsoft + QC, platform) | host runtime memory lifecycle | Day 19 |
| 4b | **Paint SR starves Camera preview** (Microsoft, platform) | single-queue NPU scheduler | Day 19 |
| 5 | (back-pocket) **Silent EP fallback after model rebuild** | QDQ fusion, EP assignment | Day 19 |

### 7. Story 1 — PCU / ADR opcode-ordering hang (90 sec)

> "Pre-silicon validation of the ADR power-loss flow on a Cambria Xeon SoC running on Zebu emulation. ADR flushes the iMC write-pending queue into persistent memory before rails collapse. On Zebu the sequence hung mid-flow — PSU loss simulated, PLI asserted, PCU started its FW opcode sequence but DDR never entered self-refresh. I pulled the Zebu trace and walked the PCU opcode log against the ADR spec. The ordering was wrong: PCU issued the WPQ-flush opcode before the IO-quiesce opcode had acknowledged completion. I wrote a Perspec test that re-ran just those two opcodes with a controlled handshake — reproduced 100%. Root cause was the FW sequencer not waiting for the IO-quiesce ack; IO traffic still in flight kept refilling the WPQ, so the self-refresh entry condition never met. Fix was an explicit wait-for-ack and a Perspec assertion gate on the ordering for regression. The lesson: opcode-level FW sequences need explicit handshakes, not implicit ordering. Pre-Si caught it because Zebu lets you trace at opcode granularity — post-Si would have shown 'random ADR fail' and taken weeks."

### 8. Story 2 — NVMe CSTS.RDY never asserts (90 sec)

> "Polaris NVMe controller boot validation in ROM mode. Standard init: PERST# de-assert, wait CSTS.RDY=0, program AQA/ASQ/ACQ, set CC.EN=1, poll CSTS.RDY=1 within CAP.TO. New host script on Oakgate hung at the CSTS.RDY=1 poll on every boot. CC.EN read back as 1, CSTS.CFS clear, but RDY stayed 0 past CAP.TO. I diff'd the new script against the working one — only difference was AQA: new script programmed ASQS=ACQS=0, treating it as a 0's-based field and 'wanting one entry.' Walked NVMe 1.3 §4.1: every queue reserves one slot for head/tail empty/full disambiguation, so AQA size 0 means zero usable slots — illegal. Re-ran with ASQS=ACQS=3 and contiguous AQ buffers and CSTS.RDY came up every boot. Root cause was host-side spec misuse against a controller that didn't fail loud. Fix on the host side, plus an FW ask: ROM should set CSTS.CFS when it sees CC.EN=1 with AQA=0 so the next person debugs in seconds. Lesson: when CSTS.RDY hangs on first boot, suspect the host before the device — AQA, ASQ/ACQ physical address, contiguous-buffer requirement."

### 9. Story 3 — Core C6 wake lost on back-to-back ARAT (90 sec)

> "Pre-Si validation of Core C6 entry/exit on Cambria. Workload: idle thread armed with the local APIC ARAT timer firing back-to-back at ~2 µs, designed to stress the C6 wake path with two wakes landing close enough that the second arrives mid-exit. Sporadic 50-500 ms hangs on the idle thread. Perfmon showed C6 residency continuing past the second timer's expected exit, even though the first wake had already serviced its IRQ. I pulled the Core PMA wake-pending CSR in Zebu trace — second wake's latch fired, but DOMAIN_STATUS.CORE_STATE never went C6→C0 for that event. Triggered Lauterbach on `CORE_STATE==C6 && wake_in==1 && cfc_init_busy==1`; every failure landed in the window where the CFC clock-crossing FIFO was being re-initialised after PLL relock but before interrupt-buffer drain completed. Wake #2's ack edge was sampled in the old domain just as the FIFO got reset — the edge never made it across. Root cause: 2-clock-domain race in C6 exit; the wake latch crossed a CFC FIFO that was mid-reset. Fix held the wake latch in the always-on domain until both `init_done` and `drain_done` were high; CORE_STATE→C0 write gated on both. Filed under HSD 16015450088. Lesson: 2-clock-domain races on power-gated blocks are the classic 'works in unit sim, fails in full-die emul' class — unit testbenches don't exercise both domains being transient at the same instant."

### 10. Story 4 — QNN/ORT page-fault + VM regression (90 sec)

> *Honest framing: platform validation, not Hexagon RTL.*

> "On the Qualcomm QNN EP path for a PSMU model on Copilot+ PC, we saw a memory regression across EP/ORT drops. Known-good EP 2.39 + ORT 1.23.3 had ~7K page-fault events and ~300 MB virtual memory during inference. With EP 2.45 + ORT 1.24.4, VM grew to ~502 MB and in one isolated run page faults spiked to ~102K. I did not treat it as a VTCM or silicon defect first. Built a version matrix: old EP/old ORT, new EP/new ORT, then new EP with old ORT. The reduced VM in that combo proved the bug followed the ORT/QNN context-loading path, not the EP itself. ETL/WPA correlation showed the newer path accumulating multiple model context buffers together; touching many host virtual pages at once caused the fault burst. The later mitigation reduced faults but kept more VM alive, which explained the 300→502 MB increase. Source pointer landed on `qnn_backend_manager.cc::CreateContextFromListAsync()`. Fix direction was bounded context lifetime — load, create, release per context unless parallel loading is needed — and a regression gate that records EP version, ORT version, page-fault count, VirtualAlloc count, peak VM. Lesson: page faults alone are not the bug. You correlate them with VirtualAlloc and peak memory before blaming hardware."

### 11. Story 4b — Paint SR starves Camera (60 sec)

> "Two AI workloads sharing the NPU on Copilot+ PC. Paint Super-Resolution submits ~36 inference tiles back-to-back; Camera preview submits a fresh inference at 30 fps. When SR runs, Camera drops frames. Built a deterministic repro with two semaphore-fired threads and QPC timestamps. GPUView showed Camera frame 0 queued at t=3 ms but not starting on the NPU until t=108 ms — about 3.2 frame periods at 30 fps — sitting behind SR's 36 tiles in the queue. Cross-checked with the IHV: HTP exposes a single hardware scheduler with no preemption; once a tile dispatches it runs to completion. Root cause was a structural limitation, not a defect: single non-preemptive queue plus no priority hint from the framework. Fix path: framework-side priority lanes so high-priority frames interleave; until then, gate concurrent NPU workloads. Lesson: a single hardware scheduler is fine until you have two real-time tenants on it; the framework must expose priority."

### 12. Delivery discipline

```
Symptom  →  Hypothesis  →  How I narrowed it  →  Root cause  →  Fix  →  Learning
```

- **90 seconds max.** If you go over, you're telling the story, not making your point.
- **Lead with the symptom.** Skip the project backstory.
- **One hypothesis at a time.** Show the test that proved or killed it.
- **Honest framing.** "I worked at the platform layer" is stronger than overclaiming RTL ownership you didn't have.
- **End with the lesson.** That's what they remember.

### 13. Common debug-tool vocabulary

| Tool | Domain | What you read |
|---|---|---|
| **JTAG / Lauterbach** | post-Si bring-up | core register, single-step, breakpoint |
| **Logic analyser** | board-level signal | parallel bus, protocol decode |
| **Lecroy / Tektronix protocol analyser** | PCIe / USB / NVMe | TLP / packet trace |
| **Oscilloscope** | analog | rail droop, signal integrity |
| **Zebu / Palladium / Veloce** | pre-Si emulation | RTL trace, transaction view |
| **Perspec** | pre-Si test content | scenario-based directed test |
| **VCS / Questa** | pre-Si simulation | waveform, SVA |
| **`perf` / VTune / WPA** | post-Si perf | counters, sampling, top-down |
| **ETL + WPA** | Windows post-Si | system trace, GPUView, memory counters |
| **AER / MCA** | PCIe / CPU error | error log, recoverable / fatal |
| **`smartctl` / nvme-cli** | NVMe field | SMART log, SQ/CQ, identify |

---

## Bugs in this Topic

This day **is** the bugs. They are sized to interview delivery. The full template entries with raw evidence are under `day29_debug_bugs.md`; the polished 90-sec versions are above.

---

## Interview Explanation (60-90 sec — the meta-answer)

> "My debug method is observe, reproduce, isolate, hypothesise, test, root-cause, fix and verify. I try to convert a vague failure — a hang, a dropped frame — into one controlled signal: a register bit, a timestamp, a queue depth, a trace edge, an ordering violation. Reproducibility rate tells me what class of bug it is: 100% means functional, intermittent means race or marginal, corner-only means PVT, single-unit means defect. I isolate by binary-searching scope, single-stepping via JTAG, or swapping components, and I always try to take one variable out at a time. The cost of finding a bug rises about 10× per stage from RTL sim through emulation through FPGA to post-Si, so where you catch it is half the value. When I tell a bug story, the shape is symptom → hypothesis → how I narrowed it → root cause → fix → learning, in 90 seconds, leading with the symptom. The honest framing matters: at Microsoft I do platform validation on Copilot+ PC silicon — I correlate ETW counters, ORT/QNN logs, version matrices and source-level pointers; I do not debug Hexagon RTL or claim VTCM defects."

---

## Practice Q&A

**Q1. State the 7-step framework.**
Observe, reproduce, isolate, hypothesise, test, root-cause, fix & verify.

**Q2. Repro rate of 0.1% — what class of bug?**
Race condition, marginal timing, voltage droop or process outlier. Need stress amplification before chasing.

**Q3. "Works in emulation, fails in silicon" — usual mechanisms?**
Analog issue, timing closure, CDC race, real-world interference, process corner. Emulation is cycle-inaccurate and no analog.

**Q4. Why does bug-finding cost rise 10× per stage?**
Each stage adds people, tools and integration; post-tape-out adds NRE and schedule risk.

**Q5. Lead with the symptom — why?**
Interviewer is testing how you think. Backstory dilutes signal in the 90-second budget.

**Q6. Honest framing on the QNN page-fault story — why does it matter?**
Overclaiming RTL ownership reads as exaggeration; precise platform-validation framing reads as competence and integrity.

**Q7. "Tell me about a time you disagreed with a designer." Shape?**
Situation → your evidence (waveform, repro, counters) → escalation path (1:1 first, then data review, no email war) → outcome (root-cause confirmed on either side — both are valid) → process change afterwards.

**Q8. ADR story — one-line takeaway?**
Opcode-level FW sequences need explicit handshakes, not implicit ordering.

**Q9. NVMe CSTS.RDY hang — first 3 things to suspect on host?**
AQA size, ASQ/ACQ physical addresses + contiguity (CAP.CQR), CC.MPS / CC.CSS configuration.

**Q10. Core C6 wake-loss story — class of bug?**
Two-clock-domain race on a power-gated block. "Works in unit sim, fails in full-die emul" pattern.

**Q11. QNN page-fault — one-line takeaway?**
Page faults alone are not the bug; correlate with VirtualAlloc and peak VM and version matrix.

**Q12. Paint SR / Camera — one-line takeaway?**
Single non-preemptive hardware scheduler is fine until two real-time tenants share it; the framework must expose priority.

---

## Coding A — Systems C: a debug-log triage helper

A small parser that classifies failures by reproducibility and tags a likely class — the first decision the framework drives.

```c
#include <stdio.h>
#include <string.h>

typedef enum { CLASS_FUNCTIONAL, CLASS_RACE, CLASS_CORNER, CLASS_DEFECT, CLASS_UNKNOWN } class_t;

static const char *class_name(class_t c) {
    switch (c) {
        case CLASS_FUNCTIONAL: return "FUNCTIONAL (design/FW)";
        case CLASS_RACE:       return "RACE / TIMING";
        case CLASS_CORNER:     return "PVT CORNER (V/T/process)";
        case CLASS_DEFECT:     return "SILICON DEFECT (single unit)";
        default:               return "UNKNOWN";
    }
}

static class_t triage(double repro_rate, int unique_units, int corner_dependent) {
    if (corner_dependent && unique_units > 1)        return CLASS_CORNER;
    if (unique_units == 1)                            return CLASS_DEFECT;
    if (repro_rate >= 0.99)                           return CLASS_FUNCTIONAL;
    if (repro_rate < 0.50)                            return CLASS_RACE;
    return CLASS_UNKNOWN;
}

typedef struct {
    const char *name;
    double repro;
    int    units;
    int    corner;
} report_t;

int main(void) {
    report_t reports[] = {
        {"ADR hang on Zebu",          1.00, 5, 0},
        {"C6 ARAT wake loss",         0.05, 4, 0},
        {"V-droop crash @ 105C",      0.40, 6, 1},
        {"Single-unit boot fail",     0.95, 1, 0},
        {"NVMe CSTS.RDY hang",        1.00, 3, 0},
    };
    int n = sizeof reports / sizeof reports[0];
    printf("%-30s  %7s  %5s  %6s  %s\n",
           "Bug", "Repro", "Units", "Corner", "Likely class");
    for (int i = 0; i < n; i++) {
        const report_t *r = &reports[i];
        printf("%-30s  %6.0f%%  %5d  %6s  %s\n",
               r->name, r->repro * 100, r->units,
               r->corner ? "yes" : "no",
               class_name(triage(r->repro, r->units, r->corner)));
    }
    return 0;
}
```

## Coding B — Interview DS&A: Number of Islands (LC 200)

DFS / BFS on a grid — the classic "isolation" pattern.

```python
def numIslands(grid):
    if not grid: return 0
    R, C = len(grid), len(grid[0])
    seen = [[False]*C for _ in range(R)]
    def dfs(i, j):
        if i<0 or j<0 or i>=R or j>=C or seen[i][j] or grid[i][j]=='0': return
        seen[i][j] = True
        dfs(i+1,j); dfs(i-1,j); dfs(i,j+1); dfs(i,j-1)
    n = 0
    for i in range(R):
        for j in range(C):
            if grid[i][j]=='1' and not seen[i][j]:
                dfs(i, j); n += 1
    return n
```

---

## System Thinking — Why debugging is a *method*, not intuition

**Frame:** great debuggers don't "see" the bug — they apply a small set of methods systematically faster than novices apply intuition. Method beats intuition because intuition has bias (you go look where you remember last week's bug, not where this week's bug actually is). Every bug story you tell in an interview is a chance to demonstrate method: how you bounded the problem, how you generated and tested hypotheses, how you knew when you were wrong. The interviewer cares less about the bug than about how you found it.

### Q: Why is bisection the universal debugging tool?

Because it works on any monotonic search space, with no domain knowledge:
- **Time bisection** (`git bisect`): when did this regress? Cuts N commits to log₂(N) builds.
- **Code bisection** (`#if 0` halves of a function): which half is wrong?
- **Input bisection** (binary-chop the failing input): which byte triggers it?
- **Configuration bisection** (toggle features off pairwise): which interaction is broken?

Each step costs one experiment but **halves uncertainty**. After 10 steps you've narrowed 1024 candidates to one. *Most engineers reach for printf or gdb first; mature engineers reach for bisection first because it converges in known time.*

### Q: Why "symptom vs cause" discipline?

Because the symptom and the cause are usually in different places. Examples:
- Symptom: occasional NPU output corruption. Cause: missing cache clean before DMA (Day 8).
- Symptom: NVMe doesn't enumerate. Cause: wrong DSTRD shift in doorbell address (Day 11).
- Symptom: PkgC10 residency drops. Cause: PCIe device firmware refuses L1.2 (Day 10/13).

**The naive fix addresses the symptom and the bug returns under different conditions.** The discipline: keep asking "why" until you reach a *mechanism* that explains the symptom and predicts what other symptoms it would cause. Then verify those other symptoms exist. *That's the difference between "workaround" and "root cause."*

### Q: Why does bug-story polish matter for interviews?

Because the interviewer is testing **how you think under pressure**, not what you know. A bug story is a 3-minute window into your debugging method. Polish four moves into every story:
1. **Frame the symptom precisely.** "Once every ~10 minutes under sustained 70% load, NPU output had a zero-tile." Not "sometimes it broke."
2. **State the hypothesis you tested first and why.** "I suspected a non-coherent DMA write — because the corruption was tile-aligned and the symptom only appeared under sustained throughput where store buffers fill up."
3. **Describe how you bounded it.** "I added a barrier before the doorbell write — if the bug was ordering, this would mask it. Bug disappeared. Then removed barrier, added explicit cache clean instead — also disappeared. Confirmed root cause."
4. **State the fix and the validation hook you added.** "Driver patched to clean+barrier on every descriptor submit. Added a stress test that runs sustained 80% load for 1 hour with output checksum — now in CI."

The story is **method demonstrated through a concrete bug**. Without the method, it's a war story. With method, it's a hiring signal.

### Q: Occam's razor — when does it fail?

When the system has hidden state. Occam says: prefer the simpler explanation. But in HW/firmware:
- The simplest explanation often is *not* the cause because it would have been caught in earlier testing. 
- Multi-day flakiness usually means a *combination* of conditions (load + temperature + frequency + concurrent IO) that none of the individual tests cover.

**Modified rule:** prefer the simplest explanation that *also* explains why the bug evaded existing tests. If your hypothesis would have been caught in CI, it's probably wrong.

### One-liner

*"Debugging is method — bisection, symptom-vs-cause, hypothesis-and-bound — because intuition has bias and method has guaranteed convergence. A polished bug story is method demonstrated through a concrete artifact: precise symptom, first hypothesis, bounding experiment, fix, and the test that prevents regression. The interviewer is hiring the method, not the trivia."*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **debug as method** — *what:* hypothesis → test → narrow / *why:* intuition has bias, method has guaranteed convergence / *how:* eliminate half the search space per step / *connects:* scientific method, why interviewers hire method not trivia
- **bisection** — *what:* `git bisect`, A/B configs, halving the search / *why:* O(log n) to find the bad change / *how:* known-good vs known-bad endpoints / *connects:* finding the introducing commit, finding the breaking config bit
- **layered isolation** — *what:* HW vs FW vs driver vs OS vs app / *why:* the stack is tall and each layer can lie / *how:* swap one layer at a time / *connects:* cross-team handoff, who-owns-the-bug politics
- **capture-and-replay** — *what:* trace + reproduce / *why:* flaky bugs need determinism / *how:* perf, ETW, ftrace, rr / *connects:* heisenbug, racy unit tests, recording the once-in-a-million
- **bug-story shape** — *what:* symptom → hypothesis → bounding experiment → fix → regression test / *why:* shows method through a concrete artifact / *how:* always 5 beats, ≤ 90s spoken / *connects:* CFC, QNN cache, NVMe RDY — same shape, different domain

---

## Bug Anchor

- **Day 11 (NVMe):** Story 2 (CSTS.RDY) lives here.
- **Day 14 (ADR / Core C6):** Stories 1 and 3 live here.
- **Day 19 (NPU runtime):** Stories 4 and 4b live here.
- **Day 21 (final mock):** these stories are the centrepiece of the loop.

## Exit Check

- [ ] Recited the 7-step framework from memory.
- [ ] Stated reproduction-rate → bug-class mapping.
- [ ] Told stories 1, 2, 3, 4, 4b each in ≤ 90 seconds, leading with the symptom.
- [ ] Stated the bug-cost-per-stage 10× ladder.
- [ ] Have honest framing for the QNN story (platform-validation, not Hexagon RTL).
- [ ] Have the meta-answer ready ("my debug method is …").

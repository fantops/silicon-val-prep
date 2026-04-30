# Day 14 — ADR, PCU/PUnit Firmware, Core C6 Wake Bugs

## Concept

ADR (Asynchronous DRAM Refresh) is a platform-level **power-loss ordering** sequence run by PCU/PUnit firmware: PLI asserts → IO quiesces → WPQ drains → DDR enters self-refresh → optional NVDIMM SAVE → rails collapse. Core C6 is a **per-core power-gating** sequence: cache flush → state save to S3M SRAM → P-channel handshake → power-FET opens. Both fail in the same shape: a handshake across always-on logic, firmware, and a power-gated domain that races a clock or reset.

This day exists to **convert the C-states material from Day 13 into bug stories you can speak in 90 seconds** — these are your headline differentiators.

## Why this day in the flow

Day 12 gave you DVFS and PVT (the active-power lever). Day 13 gave you the C-state hierarchy and PkgC6 blockers (the idle-power lever). Day 14 takes the two real Intel-shaped bugs that live at the seam between firmware, always-on logic, and power-gated cores, drills them deep, and turns them into rehearsed stories. Day 15 then steps back out into thermal and perf counters, which is where these bugs were **observed** before they were **isolated**.

In the Layer map this is **Layer 4 (Power Management)**, specifically the firmware/sequence side: `PCU FW → IOSF Sideband → Core PMA → S3M SRAM`.

---

## Deep Study Notes

### 1. The Intel power-management hierarchy (mandatory mental model)

```
   OS idle driver  (intel_idle on Linux, PPM on Windows)
        │  MWAIT(target_cstate)
        ▼
   Core PMA (Power Management Agent) — per-core
        │  P-channel REQ to PCU
        ▼
   PUnit / PCU (Package Control Unit) — package-level firmware
        │
        ├─→ Aggregates requests from all cores + uncore IPs
        ├─→ Checks blockers (PCIe LTSSM != L1.x, IIO outstanding txns,
        │   IMC refresh, thermal flag, etc.)
        ├─→ Talks to VRM (drop Vcc), to PLLs (gate refclk),
        │   to LLC (flush), to IMC (drain WPQ)
        └─→ Returns P-channel ACK → cores transition
```

Key vocabulary you must use correctly in interviews:

- **PCU / PUnit / PMC FW** — the on-die microcontroller running the power state machine. Intel calls it PUnit on client and PCU on server; same idea.
- **IOSF Sideband (IOSF-SB)** — the on-die bus the PCU uses to talk to other IPs. PCU does **not** sit on the coherent fabric for these messages.
- **P2P opcodes (PUnit-to-PUnit)** — message opcodes the PCU FW issues to other PCU/PUnit blocks (e.g., across socket, or to satellite controllers). A *stale opcode mapping* after a stepping change is a classic FW bug.
- **P-channel** — the standardised power handshake protocol between Core PMA and PCU.
- **S3M SRAM** — the always-on save/restore SRAM that holds architectural state during C6.
- **CFC** — clock-forwarding/crossing FIFO between always-on and power-gated domains.
- **WPQ** — Write Pending Queue inside the IMC (Integrated Memory Controller).
- **PLI** — Power Loss Imminent, the PSU-driven signal that starts ADR.

### 2. ADR end-to-end (the diagram you should be able to draw cold)

```
   PSU detects AC loss
        │
        │ asserts PLI (Power Loss Imminent)
        ▼
   PCH / Platform controller receives PLI
        │
        │ asserts ADR_TRIGGER (sideband to PCU)
        ▼
   PCU runs the ADR FW sequence
        │
        ├─→ IO_QUIESCE       : stop new PCIe/IIO txns, drain in-flight
        ├─→ FABRIC_QUIESCE   : mesh/coherent fabric stops accepting
        ├─→ (eADR only)      : LLC writeback to IMC (WBINVD-equivalent)
        ├─→ WPQ_FLUSH        : IMC drains write-pending queue to DDR
        ├─→ DDR_SELF_REFRESH : all channels enter self-refresh
        └─→ NVDIMM SAVE_n#   : if NVDIMM-N populated, FW copies DRAM→flash
        │
        ▼
   Platform powers off
```

**Hold-up budget** is the validation pain point: PSU bulk caps + supercaps give you 10-30 ms before rails sag. The whole sequence above must finish in that window. Every IP added to the platform eats into the budget. Your validation deliverable at Intel was: characterise end-to-end ADR latency, find the long-pole IP, work with that IP team to shave latency.

**eADR vs ADR**: classic ADR drains only IMC WPQ; eADR adds LLC writeback so application-direct PMem stores are also persisted without explicit `clwb`/`clflushopt`. eADR costs more hold-up time but removes a software-side ordering trap.

**Cross-socket ADR**: in 2-socket systems the UPI link teardown ordering matters. Socket 0 must not power off while Socket 1 is still draining over UPI. Bug shape: socket-1 hang waiting on a UPI completion that will never come.

### 3. Core C6 entry sequence (Intel-specific, block-by-block)

```
   OS  ─── MWAIT(C6) ───►  Core PMA
                              │
                              ├─ Flush L1/L2 to LLC
                              ├─ Save GPRs, MSRs, FPU/AVX to S3M SRAM
                              ├─ Save TSC delta
                              ├─ Send P-channel C6_REQ to PCU
                              │
                              ▼
                           PCU
                              │
   ┌── Check: any LTSSM not in L1.x?         (PCIe IIO blocker)
   ├── Check: VR settling timer running?      (DVFS in flight)
   ├── Check: IIO outstanding txns?
   ├── Check: thermal flag clear?
   │
   │ All clear:
   ├── Request VR Vcc collapse on this core domain
   ├── Power-gate FET opens
   ├── Core voltage rail sinks to 0 V
   └── Send P-channel C6_ACK back
```

Each `Check` is a real bug surface — that is why Day 13's PkgC6 blocker list and this entry diagram are not duplicates: the PkgC6 list tells you **what to check first** when residency is low; this diagram tells you **where in the sequence the check sits** when you have to root-cause the firmware.

### 4. Core C6 exit sequence (where the wake-lost bug lives)

```
   Wake event  (interrupt, ARAT timer, NMI, IPI)
        │  sampled by always-on logic
        ▼
   Always-on latch raises wake_pending
        │
        ▼
   PCU: request VR ramp-up on the core domain
        │
        ▼
   PLL relock on core clock
        │
        ▼
   CFC FIFO between always-on and core domain re-initialises
        │  cfc_init_busy = 1 during this window  ◄── RACE WINDOW
        ▼
   Interrupt buffer drains across the CFC
        │
        ▼
   PCU sends C0_ACK to Core PMA
        │
        ▼
   Core PMA restores GPRs/MSRs from S3M SRAM
        │
        ▼
   DOMAIN_STATUS.CORE_STATE := C0  → core executes wake handler
```

**The race window** is the small interval where the CFC FIFO is mid-reset after PLL relock but before the interrupt buffer has drained. A second wake that arrives during `cfc_init_busy` can be sampled in always-on logic but its acknowledgement edge into the core domain never crosses, so the core stays in C6. This is the Core C6 wake-lost bug below.

### 5. Save/Restore SRAM (S3M)

- Dedicated always-on SRAM holding architectural state during C6.
- Powered from a separate AON rail.
- ECC protected; you validated retention across PVT and restore latency.
- Restore on exit: PCU restores Vcc, PLLs lock, then state copies back to core registers.

The "Why isn't S3M defeating the purpose?" interview question: S3M is a few KB and almost never accessed; its leakage is tiny compared to keeping multi-MB cache + execution units powered. Net win is large.

### 6. Validation tools you used (call them out by name)

- **Logic analyzer / scope** on PLI, ADR_TRIGGER, SAVE_n#, VR rail probes.
- **PCU FW traces** (P2P opcode log; you cross-checked against the opcode workbook).
- **IOSF Sideband sniffer** for PCU↔IP messages.
- **BIOS-side ADR injection tool** for repeatable software-triggered ADR.
- **Zebu emulation** for opcode-level visibility before silicon.
- **Lauterbach / ITP / DCI / SVTools** for in-target probing on first silicon.
- **Perspec** for assertion-driven directed regressions on the offending sequence.
- **Perfmon / RAPL counters** for residency vs telemetry cross-checks.

---

## Bugs in this Topic (the two stories you must own)

### Bug 1 — PCU/ADR opcode-ordering hang

| Field | Content |
|---|---|
| Project | Cambria Xeon SoC, pre-silicon Zebu emulation |
| Feature | ADR power-loss flow flushing IMC WPQ to persistent memory |
| Symptom | ADR sequence hung mid-flow. PSU loss simulated, PLI asserted, PCU FW started its opcode sequence, but DDR never entered self-refresh. |
| Initial hypothesis | Memory controller or DDR self-refresh side problem. |
| Isolation | Pulled Zebu trace and walked the PCU opcode log against the ADR spec sequence. Failure showed `WPQ_FLUSH` issued **before** `IO_QUIESCE` acknowledgement was complete. A targeted Perspec test reran only the offending opcode pair with controlled handshake and reproduced 100%. |
| Root cause | PCU FW sequencer did not wait for IO-quiesce ack before issuing WPQ flush. IO traffic was still in flight, so the WPQ could refill during drain. DDR self-refresh entry condition was never met → hang. |
| Fix / verify | PCU FW added explicit wait-for-ack on IO quiesce before WPQ flush. A Perspec assertion was added to gate the ordering for regression. Stress and power-loss sequences passed after the fix. |
| Learning | Opcode-level firmware sequences need **explicit handshakes**, not implicit ordering assumptions. Zebu visibility caught this at opcode granularity; post-silicon would have looked like a vague ADR failure. |

**90-second telling:**

> "On Cambria pre-Si, ADR was hanging mid-sequence — PSU loss was simulated, PLI asserted, PCU FW started its opcodes, but DDR never reached self-refresh. The early hypothesis was an IMC issue. I pulled the Zebu PCU opcode trace and walked it against the ADR spec, and saw `WPQ_FLUSH` was being issued before `IO_QUIESCE_ACK` came back. So IO was still in flight, the WPQ kept refilling during drain, and self-refresh entry condition was never met. I wrote a Perspec test that reran just that opcode pair and reproduced 100%. The fix was an explicit wait-for-ack in the PCU FW sequencer; we added a Perspec assertion on the ordering to catch regressions. The takeaway was that firmware sequences across IPs need explicit handshakes, not implicit ordering assumptions."

### Bug 2 — Core C6 wake lost on back-to-back ARAT timer

| Field | Content |
|---|---|
| Project | Cambria, pre-silicon Core C6 entry/exit validation |
| Feature | Core C6 wake from local APIC ARAT timer |
| Workload | Armed back-to-back ARAT wakes ~2 µs apart to stress a second wake during C6 exit. |
| Symptom | Sporadic 50-500 ms hangs on the idle thread. Perfmon showed C6 residency continuing past the second timer's expected exit, even though the first wake had serviced its IRQ. |
| Initial hypothesis | Timer or interrupt delivery issue. |
| Isolation | Pulled Core PMA / CCP wake-pending CSR in Zebu trace. Wake #2 latched in always-on but `DOMAIN_STATUS.CORE_STATE` never went C6→C0 for that event. Lauterbach trigger on `CORE_STATE==C6 && wake_in==1 && cfc_init_busy==1` caught every failure in the window where the CFC FIFO was being reinitialised after PLL relock but before interrupt-buffer drain completed. |
| Root cause | Two-clock-domain race in the C6 exit sequence. Wake #2 was sampled in the always-on domain, but the acknowledgement edge into the core domain crossed a CFC FIFO that was mid-reset. The edge never made it across, so CCP held the core in C6. |
| Fix / verify | Held the wake latch in always-on logic until the CFC FIFO reported `init_done` AND the interrupt buffer reported `drain_done`. Gated `DOMAIN_STATUS.CORE_STATE → C0` on both signals. Added a Perspec assertion for wake pulses arriving during `cfc_init_busy`. Directed ARAT/wake stress passed. |
| Learning | Anything crossing always-on into a powered-down domain must hold intent until the sink clock and reset are stable. This is the canonical "works in unit-sim, fails in full-die emulation" class — unit-sim never modelled the CFC reset window. |

**90-second telling:**

> "On Cambria, Core C6 was occasionally losing a wake when two ARAT timers fired about 2 µs apart. Perfmon showed C6 residency continuing past when the second wake should have woken the core. I pulled the wake-pending CSR in Zebu and saw wake #2 was latched in the always-on domain but `CORE_STATE` never transitioned to C0 for that event. I set a Lauterbach trigger on `wake_in==1 && cfc_init_busy==1` and it hit every failure. Root cause was a two-domain race: the wake was sampled in always-on but its ack edge into the core domain crossed a CFC FIFO that was mid-reset after PLL relock. The fix was to hold the wake latch in always-on until both the CFC FIFO `init_done` and the interrupt-buffer `drain_done` were asserted. The lesson was that any always-on to power-gated handoff must hold intent until the sink clock and reset are both stable."

### Why these are good interview stories

- They use real Intel vocabulary (PCU, P2P, IOSF-SB, CFC, S3M, ARAT, Perspec, Zebu).
- They show the full debug arc: symptom → hypothesis → trace evidence → root cause → fix → regression coverage → learning.
- They show **mechanism**, not just outcome: opcode handshake, two-domain race.
- They are reproducible in directed test, which proves you isolated the cause, not just observed correlation.

---

## Interview Explanation (60-90 sec)

> "ADR and Core C6 are both sequencing problems across firmware and power-gated logic. ADR is the platform power-loss flow: PLI from the PSU triggers PCU FW to quiesce IO, drain the IMC write-pending queue, put DDR into self-refresh, and signal NVDIMM SAVE before rails collapse — all inside a 10-30 ms hold-up budget. Core C6 is per-core power gating: cache flush, arch state save to S3M SRAM, P-channel handshake to PCU, then the power-FET opens. The bug surface in both is handshakes that cross firmware, always-on logic, clocks, and resets. My headline ADR bug was a PCU opcode-ordering issue — `WPQ_FLUSH` issued before `IO_QUIESCE_ACK`, caught on Zebu opcode trace. My headline C6 bug was a wake lost across a CFC FIFO that was mid-reset after PLL relock, caught with a Lauterbach trigger on `wake_in && cfc_init_busy`. Both were fixed by explicit hold-and-handshake and covered with Perspec assertions for regressions."

---

## Practice Q&A

**Q1. What is PLI and why does it matter for the validation budget?**
Power Loss Imminent. It is the PSU-driven signal that says rails will sag in 10-30 ms. Every IP that has to quiesce eats into that budget. The validation deliverable is end-to-end latency characterisation against the budget.

**Q2. Why must IO be quiesced before WPQ flush?**
If IO continues, new writes can enter the IMC write-pending queue while it is being drained, so self-refresh entry condition is never met. This is the PCU/ADR bug above.

**Q3. Difference between ADR and eADR?**
ADR drains IMC WPQ. eADR also writes back the LLC, so app-direct PMem stores are persisted without explicit `clwb`. eADR costs more hold-up time.

**Q4. What is the P-channel?**
The handshake protocol Core PMA uses to request a C-state from PCU. PCU evaluates blockers and acks/declines.

**Q5. What is S3M SRAM?**
Always-on SRAM that holds the core's architectural state during C6. Powered from the AON rail with ECC.

**Q6. Why can a Core C6 wake be lost?**
The wake crosses from always-on logic into a domain whose clock and reset are still transitioning after PLL relock. If the ack edge crosses a CFC FIFO mid-reset, it never makes it across, and the core stays in C6.

**Q7. Why is "works in unit-sim, fails in full-die emulation" the C6-wake-bug archetype?**
Unit sims usually do not model the CFC reset window or AON-to-core domain crossing realistically. You only see the race when you have the full clock/reset infrastructure modelled, i.e. on emulation or first silicon.

**Q8. PkgC6 vs Core C6 in 30 seconds?**
Core C6: one core power-gates, others can stay active. PkgC6: ALL cores in C6, LLC flushed, package Vcc collapsed, PLLs off. PkgC6 has a long blocker list (PCIe L1.x, IIO outstanding txns, IMC refresh, etc.).

**Q9. You said "PCU FW had a stale opcode mapping after a silicon stepping change" elsewhere — what does that mean?**
PCU FW carries an opcode table addressing other IPs over IOSF-SB. A new stepping changed an opcode value; the FW table wasn't updated, so the message went to the wrong target (or no target) and was dropped silently. Caught by cross-checking PCU FW trace against the IOSF-SB log.

**Q10. What evidence is sufficient to claim root cause for a C6 wake bug?**
A trigger condition that catches every failure (`wake_in && cfc_init_busy`), a directed test that reproduces 100%, and a fix that closes the race in the always-on holding logic. Bonus: a Perspec assertion in regression.

---

## Coding A — Systems C: ADR opcode-sequence simulator

A skeleton you can talk through in an interview to show you can model the ordering bug. Compile with `gcc -Wall -std=c11 -o adr adr.c && ./adr`.

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    OP_IO_QUIESCE,
    OP_IO_QUIESCE_ACK,
    OP_FABRIC_QUIESCE,
    OP_LLC_WRITEBACK,    /* eADR only */
    OP_WPQ_FLUSH,
    OP_WPQ_FLUSH_ACK,
    OP_DDR_SELF_REFRESH,
    OP_NVDIMM_SAVE,
    OP_DONE,
} adr_op_t;

typedef struct {
    bool io_quiesced;
    bool fabric_quiesced;
    bool wpq_drained;
    bool in_self_refresh;
    bool eadr;
} adr_state_t;

/* The buggy version: issues WPQ_FLUSH without waiting for IO_QUIESCE_ACK. */
static int adr_run_buggy(adr_state_t *s) {
    /* ... issue OP_IO_QUIESCE ... */
    /* MISSING wait-for-ack */
    /* issue WPQ_FLUSH while IO still in flight */
    if (!s->io_quiesced) {
        printf("BUG: WPQ_FLUSH issued before IO_QUIESCE_ACK -> WPQ refills, hang\n");
        return -1;
    }
    return 0;
}

/* The fixed version: explicit wait-for-ack. */
static int adr_run_fixed(adr_state_t *s) {
    /* 1. IO quiesce + wait ack */
    s->io_quiesced = true;             /* ack arrived */

    /* 2. Fabric quiesce */
    s->fabric_quiesced = true;

    /* 3. Optional eADR LLC writeback */
    if (s->eadr) { /* WBINVD-equivalent, wait done */ }

    /* 4. WPQ flush + wait ack */
    if (!s->io_quiesced || !s->fabric_quiesced) return -1;
    s->wpq_drained = true;

    /* 5. DDR self-refresh */
    s->in_self_refresh = true;

    /* 6. NVDIMM SAVE_n# (if present) */
    return 0;
}

int main(void) {
    adr_state_t s = {0};
    printf("--- buggy run ---\n");
    adr_run_buggy(&s);

    printf("--- fixed run ---\n");
    s = (adr_state_t){0};
    int rc = adr_run_fixed(&s);
    printf("fixed: rc=%d in_self_refresh=%d\n", rc, s.in_self_refresh);
    return 0;
}
```

Talking points while you write: "I'm modelling the opcode sequence as an enum, the visible side effects as a struct, and the bug as a missing barrier between OP_IO_QUIESCE and OP_WPQ_FLUSH — that is exactly the shape of the Cambria PCU FW bug."

## Coding B — Interview DS&A: Number of Islands (LC 200)

The grid-traversal pattern that comes up at QC/MS phone screens. Pick BFS or DFS; both are fine. Key insight: every unvisited `'1'` starts a new island; flood-fill from there marks the whole connected component.

```python
from collections import deque

def numIslands(grid):
    if not grid: return 0
    R, C = len(grid), len(grid[0])
    seen = [[False]*C for _ in range(R)]
    islands = 0
    for i in range(R):
        for j in range(C):
            if grid[i][j] == '1' and not seen[i][j]:
                islands += 1
                q = deque([(i, j)])
                seen[i][j] = True
                while q:
                    r, c = q.popleft()
                    for dr, dc in ((1,0),(-1,0),(0,1),(0,-1)):
                        nr, nc = r+dr, c+dc
                        if 0 <= nr < R and 0 <= nc < C and grid[nr][nc] == '1' and not seen[nr][nc]:
                            seen[nr][nc] = True
                            q.append((nr, nc))
    return islands
```

---

## System Thinking — Why platform power is *firmware*, not just hardware

**Frame:** by the time you're managing C-states, ADR, and core C6 across a multi-core multi-IP SoC, the coordination problem has more state than any hardware FSM can encode. The **PCU (Power Control Unit)** — a small embedded processor inside the SoC running its own firmware — exists because the policy is too complex and too release-cycle-sensitive to bake into RTL. ADR exists because power *can fail*, and software writes that the application thought were committed must survive. Core C6 bugs are silent because the state save/restore surface is huge and a single missed bit is invisible until the wake-up returns.

### Q: Why is the PCU a microcontroller running firmware, not just RTL?

Three reasons:
1. **Policy complexity.** Deciding when to enter PkgC6, what voltage to drop to, how to coordinate with the PCH, when to reject a deep state because PCIe isn't in L1.2 — this is dozens of conditions and timeouts. Encoding it in RTL is possible but un-patchable. A buggy policy needs a silicon respin.
2. **Per-SKU tuning.** Same silicon ships as 4 SKUs with different TDPs, Turbo limits, voltage tables. PCU firmware is per-SKU; RTL is one design.
3. **Field updates.** New workloads (AI inference, games) reveal new patterns. PCU firmware can be updated via microcode/BIOS; RTL cannot.

**The cost:** PCU firmware bugs look like silicon bugs to the user ("my CPU randomly hangs in idle"), but they're software bugs that ship as microcode patches. Most of the "errata fixed in microcode" entries in CPU release notes are PCU/PCODE fixes.

### Q: Why ADR (Asynchronous DRAM Refresh)? RAM is volatile anyway.

Because **persistent memory** (NVDIMM-N, CXL persistent memory, battery-backed DRAM regions) commits writes only when they reach the storage media. In flight, those writes sit in the CPU's **WPQ (Write Pending Queue)** inside the memory controller. On a sudden power loss:
- Without ADR: WPQ contents lost → application thinks the write committed (it returned from `pmem_persist`), but it didn't → silent data loss / torn structures on reboot.
- With ADR: PMIC asserts `ADR_TRIGGER` → PCU drains WPQ to DRAM → puts DRAM into self-refresh → backup capacitor or battery sustains DRAM long enough to flush. On reboot, persistent regions are recovered.

**ADR is a real-time correctness property — the WPQ drain must complete in the time the hold-up cap can sustain power.** ADR opcode-ordering bugs (sending the drain command before the persistent commit, or vice versa) cause silent corruption that surfaces only after a power-fail event — days or weeks later. *Hardest class of validation bug.*

### Q: Why are Core C6 bugs so hard to catch?

Because C6 saves the entire architectural state of the core (registers, MSRs, PMU counters, debug state, performance event configs) to an SRAM region, then power-gates the core. On wake, every bit must be restored exactly. Bugs:
- A new MSR added in a step but missed in the C6 save list → reads as 0 after first C6 wake → application breaks subtly.
- Save/restore order wrong (restore X2APIC before LAPIC enable bit) → lost interrupts after wake.
- C6 entry races with an interrupt arrival → interrupt lost or duplicated.

These pass functional tests (the CPU works fine *between* C6 cycles). They surface as **flakiness under stress** — a perf counter drifts, a TSC skews, an interrupt occasionally arrives twice. *Coverage of C6 entry-exit at every interrupt boundary is the validation methodology.*

### One-liner

*"PCU firmware exists because power policy is too complex, too SKU-specific, and too release-sensitive for RTL. ADR exists because persistent memory commits are in-flight in the WPQ on power loss — the PCU has microseconds to drain it. Core C6 bugs are silent because the save/restore surface is wide and the bug only shows after a wake under specific timing — this is what pre-silicon validators chase with C6-stress + perf-counter delta tests."*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **PCU** — *what:* on-die power microcontroller / *why:* realtime power decisions faster than OS / *how:* firmware in ROM/patchable, talks PMIC and clock tree / *connects:* P-state arbiter, why power bugs are firmware bugs
- **Core C6** — *what:* flush state, power-gate the core / *why:* deepest core idle, kills both dynamic and leakage / *how:* save arch state to SRAM, drop Vdd to 0 / *connects:* wake = restore from SRAM, retention flop bugs, save/restore surface
- **ADR** — *what:* async DRAM refresh on power loss / *why:* persistent memory durability, drain WPQ / *how:* PSU hold-up + battery-backed CKE / *connects:* CXL.mem, NVDIMM, why store ordering matters near power events
- **CFC FIFO bug pattern** — *what:* wake event lost in cross-clock FIFO / *why:* CDC + power gating overlap / *how:* FIFO reset before wake propagated through synchronizers / *connects:* every async power bug looks like this; pre-Si test = wake-under-stress + counter delta
- **why power = firmware** — *what:* policy lives in PCU firmware not RTL / *why:* SKU-specific, release-sensitive, too complex for hard logic / *how:* firmware patched per stepping / *connects:* why a power bug becomes a CR not a respin

---

## Bug Anchor

- **Primary:** PCU/ADR opcode ordering (Bug 1 above).
- **Primary:** Core C6 wake lost (Bug 2 above).
- **Cross-link:** Day 13 (PkgC6 blockers) — those are the *static* checks; today's ADR/C6 bugs are the *dynamic* sequence failures.
- **Cross-link:** Day 8 (DMA/doorbells/ordering) — same handshake-and-barrier discipline, different domain.
- **Cross-link:** Day 20 (debug method polish) — both stories follow observe → reproduce → isolate → root-cause → fix → regression.

## Exit Check

- [ ] Drew the ADR flow diagram from memory: PSU → PLI → PCU → IO_QUIESCE → WPQ_FLUSH → SELF_REFRESH → SAVE.
- [ ] Stated the hold-up budget number (~10-30 ms) and why it matters.
- [ ] Drew Core C6 entry sequence with PCU coordination from memory.
- [ ] Drew Core C6 exit sequence with the CFC race window highlighted.
- [ ] Distinguished Core C6 vs PkgC6 in <30 sec verbally.
- [ ] Spoke the PCU/ADR opcode-ordering bug in <90 sec without notes.
- [ ] Spoke the Core C6 wake-lost bug in <90 sec without notes.
- [ ] Used the vocabulary correctly: PCU, PUnit, P-channel, P2P opcode, IOSF-SB, S3M SRAM, CFC, ARAT, WPQ, PLI.
- [ ] Can name 3 validation tools used (Zebu, Perspec, Lauterbach/ITP/DCI).
- [ ] Stated the learning from each bug in one sentence.

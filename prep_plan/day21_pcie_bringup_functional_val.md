# Day 21 — PCIe Bring-up + Functional Validation

## Concept

PCIe bring-up is the structured process of taking a designed link from reset to **L0 carrying real traffic**, then proving — through analyser captures, AER monitoring, compliance test patterns, and traffic stress — that the link, the transaction layer, and the SW driver path all match the spec under every realistic scenario. The hardest part is **diagnosing a link that won't reach L0**: PCIe gives you a handful of LTSSM substates and a pile of side-band signals, and the bug could be electrical, polarity, clock, equalisation, side-band, or SW. **A structured LTSSM-stuck-state ladder is the load-bearing diagnostic skill.**

## Why this day in the flow

D18 gave you the LTSSM state machine. D19 enumerated the device. D20 added PM. **Today is the validation craft** — how a PCIe IP val engineer triages a failure, runs compliance, sweeps eye/SI, captures TLPs, and proves the IP works at every speed/width under every traffic pattern. This is the day that maps to "I can run PCIe link bring-up in the lab and tell you what's wrong from the analyser trace."

In the Layer map this is the **fourth day of Phase 4 (PCIe)**, the val-craft cap before D22's PM intersection.

---

## Deep Study Notes

### 1. The PCIe bring-up ladder

```
0. Refclk + power rails       (clock present, rails stable, PERST# asserted)
   │
   ▼
1. PERST# deasserted          (controller exits hot-reset)
   │
   ▼
2. Detect                     (electrical idle, RX detect on each lane)
   │
   ▼
3. Polling                    (TS1/TS2 ordered sets exchanged, bit-lock + symbol-lock)
   │
   ▼
4. Configuration              (link width + lane reversal + polarity negotiated)
   │
   ▼
5. L0 at Gen1                 (link up at lowest speed)
   │
   ▼
6. Equalisation               (Gen3+: phases 0-3 negotiate TX coefficients)
   │
   ▼
7. Recovery → L0 at target    (link retrains at negotiated speed)
   │
   ▼
8. Config space accessible    (host can enumerate)
   │
   ▼
9. TLP traffic                (memory reads/writes flow through)
   │
   ▼
10. Long-soak                 (24-72 hr error-free, no AER, no retrains)
```

Each step has a **stuck-state failure mode** with a structured diagnostic.

### 2. LTSSM stuck-state diagnostic ladder

| Stuck in | What it means | Hunt these |
|---|---|---|
| **Detect** | RX never sees electrical-idle exit on any lane | Refclk dead? PERST# stuck? RX terminations off? Cable/connector? Polarity? |
| **Polling.Active** | TS1s sent, partner's TS1s not seen | TX/RX swap? Polarity reversed on all lanes? PHY PLL not locked? |
| **Polling.Configuration** | Bit-lock OK, can't agree on a common config | Lane count mismatch, partner advertising width > capability |
| **Polling.Compliance** | Stuck in compliance pattern | Compliance bit set in TS1 by partner; check link partner config |
| **Configuration.Linkwidth.Start** | Width negotiation hung | Lane-to-lane skew, lane reversal mis-detected |
| **Configuration.Lanenum.Accept** | Lane numbering not agreed | Lane reversal not handled by IP |
| **Recovery.RcvrLock** | Lost bit-lock during retrain | Equalisation phase failed, SI margin marginal |
| **Recovery.Equalization** | Gen3+ eq did not converge | TX preset/coefficient range insufficient, RX CTLE limits |
| **Recovery.Speed** | Speed change failed | Eq result not applied, PLL re-lock time exceeded |
| **L0 → Recovery loop (L0p)** | Link in L0 briefly then back to Recovery | Marginal SI: random symbol errors trigger retrain; eq drifted |
| **L0s entry → no exit** | TX in L0s, partner doesn't issue FTS | FTS count mismatch, partner's L0s entry timing wrong |

This ladder is **the** PCIe debug skill. You read the LTSSM trace, find the deepest substate the link reached, and the table tells you where to look.

### 3. Equalisation (Gen3+) — the most error-prone bring-up step

Gen3 introduced 8b/10b → 128b/130b encoding and a much more aggressive eye, requiring **TX equalisation**. The eq protocol has 4 phases:

```
Phase 0  — start of eq, baseline; uses preset coefficients
Phase 1  — exchange TX coefficients; receivers measure eye
Phase 2  — receiver requests TX coefficient adjustments
Phase 3  — final convergence; lock in chosen coefficients
```

Eq settings live in **TX FFE coefficients (C-1, C0, C+1)** and **RX CTLE setting**. Mis-converged eq means the link "trains" but every few seconds drops to Recovery for re-training — the classic L0p loop.

Debug: analyser captures eq exchange in Recovery.Equalization; compare requested vs applied coefficients; if RX kept asking for more pre-emphasis than TX could provide, **add an Re-driver** or fix board impedance.

### 4. AER — Advanced Error Reporting

The transaction layer reports errors via the AER extended capability (ID 0x0001). Categories:

| Class | Examples | AER bit |
|---|---|---|
| **Correctable** | Receiver Error (8b/10b decode), Bad TLP, Bad DLLP, Replay Timer Timeout, Replay Number Rollover | Cor Status |
| **Uncorrectable Non-Fatal** | Poisoned TLP, Completion Timeout, Completer Abort, Unexpected Completion | UnCor Status (non-fatal) |
| **Uncorrectable Fatal** | DLP, Surprise Down, Flow Control Protocol Error, Malformed TLP | UnCor Status (fatal) |

```
# Read AER state on Linux
$ sudo setpci -s 01:00.0 ECAP_AER+0x10.L     # Uncor Status
$ sudo setpci -s 01:00.0 ECAP_AER+0x14.L     # Uncor Mask
$ sudo setpci -s 01:00.0 ECAP_AER+0x1C.L     # Cor Status
```

**Validation rule:** AER must be **silent under correctly programmed traffic.** Any AER bit set during a clean test is a bug — usually a TLP that violates ordering, flow-control, or completion-timeout rules.

The val team writes:
- One row per AER error type × per error-injection scenario.
- An assertion that AER `Status` is clear at end of every functional test.
- An error-injection harness (often via dedicated debug regs) that exercises every reporting path.

### 5. Compliance testing — the spec-defined toolbox

PCIe-SIG defines a **compliance pattern** (CPAT) and a set of **compliance test sequences** that the IP must pass to legally claim PCIe compliance.

Test categories:
- **Electrical (PHY):** eye-mask at the spec compliance point, jitter tolerance, common-mode noise.
- **Protocol (DLL/TL):** TS1/TS2 format, DLLP timing, replay buffer behaviour, flow-control credit accounting.
- **Configuration:** all required caps present, sized correctly.

Compliance pattern is requested via TS1's compliance bit; the link goes to `Polling.Compliance` and the TX drives the CPAT continuously, allowing a scope to measure the eye.

**Validation lab uses:**
- PCIe-SIG GoldenFinger / PCIe protocol test card.
- Vendor compliance tool (Lecroy, Teledyne).
- Eye-mask analyser at the compliance point on the channel.

### 6. Traffic stress — the functional regression

After bring-up, the IP val team runs **traffic stress**:

| Pattern | Purpose |
|---|---|
| **Sequential reads/writes** | Baseline throughput |
| **Random TLP sizes** | TLP-buffer + flow-credit stress |
| **Read-completion stress** | Many outstanding NPNP, completion-credit close to zero |
| **Posted vs non-posted mix** | Ordering rule coverage |
| **Cross-traffic between multiple endpoints** | Arbitration fairness |
| **Error injection mid-traffic** | Recovery + replay |
| **PM transitions mid-traffic** | L1 entry with pending TLPs |
| **Link-speed change mid-traffic** | Recovery without TLP loss |
| **Hot-reset / FLR mid-traffic** | Reset semantics |

Each is a coverage row; scoreboard tracks bytes-in == bytes-out, ordering preserved, no AER.

### 7. SI (Signal Integrity) and eye-margin

For a real product, the val team works alongside the SI team:

- **Eye opening at the receiver pad** must clear the spec mask at the **compliance point.**
- **Loss budget** is shared across PCB + connector + cable + Re-driver/Re-timer.
- **Per-lane skew** must be under spec.

Bring-up issues caused by SI usually show as:
- Stuck in `Recovery.RcvrLock` at higher speeds (eq margin gone).
- High `Receiver Error` AER rate.
- L0p loops at sustained traffic.

Fixes are **outside** the IP — board layout, Re-timer addition, connector change. The IP val engineer needs to recognise SI symptoms so the bug is filed against the right team.

### 8. Hot-reset, FLR, Surprise Down

The IP must handle three reset / link-down events cleanly:

| Event | Source | Recovery |
|---|---|---|
| **Hot-reset** | Rootport issues TS1 with hot-reset bit set | Endpoint resets, link re-trains |
| **FLR (Function Level Reset)** | SW writes FLR bit in PCIe Cap | Function resets without link disturbance |
| **Surprise Down** | Link goes electrically idle unexpectedly | Endpoint signals SDU, fatal AER, driver re-binds |

Validation: for each event × every pending traffic class, verify clean recovery + no AER × proper SW notification.

### 9. The PCIe IP val program structure (functional, not PM)

```
P1 — Unit RTL stable
  ├─ LTSSM coverage: every state, every transition
  ├─ Config-space coverage: every register × read/write paths
  ├─ TLP layer: every TLP type, every payload size class
  ├─ DLLP layer: every DLLP, replay buffer, flow control
  └─ AER: every error type, every reporting path

P2 — Subsystem integration
  ├─ Multiple endpoints + bridge
  ├─ Real driver path (or SW model)
  └─ Traffic stress with PM transitions

P3 — SoC integration
  ├─ Rootport + switch + multiple endpoints
  ├─ Linux boot + enumeration + driver init
  └─ Long-soak

P4 — Emulation
  ├─ Boot full OS, run real workloads (NVMe IO, NIC traffic)
  └─ Compliance pattern verification

P5 — Post-Si bring-up
  ├─ Lab board with refclk + PERST# + side-band wired
  ├─ Scope + analyser + lspci
  └─ Long-soak + compliance + interop matrix
```

### 10. Interop matrix — the real-world test

A PCIe IP is exercised against **other vendors' silicon** to catch spec interpretations that differ across the industry:

| Endpoint | Rootport | Typical surprises |
|---|---|---|
| NVMe drive | Intel rootport | Eq settings, MSI-X vector affinity |
| 100GbE NIC | AMD rootport | Flow-control credit accounting |
| GPU | Intel rootport | ASPM L1 negotiation, BAR sizing >4GB |
| FPGA card | ARM rootport | LTR, cap chain ordering |

Each new interop pair finds at least one bug — usually in the *less validated* of the two parties. The IP val team maintains an **interop test list** and runs the matrix at each silicon stepping.

---

## Bring-up Flow — Lab (Concrete PCIe Link)

```
Day-1 in the lab:
   ├─ Board mounted, refclk source verified on scope (100 MHz, jitter spec)
   ├─ Rails up, PERST# asserted (probe with logic analyser)
   ├─ PCIe analyser interposer in series with link
   ├─ Side-band CLKREQ#/WAKE# probed
   └─ Host machine with `lspci`, `setpci`, `dmesg`, vendor tool

PERST# release:
   ├─ Watch LTSSM trace on analyser
   ├─ Expect: Detect → Polling → Configuration → L0
   ├─ If stuck → consult diagnostic ladder
   └─ Capture analyser trace from PERST# release through L0 for archive

At L0:
   ├─ `lspci` should show device
   ├─ If link width or speed lower than expected → AER + retrain analysis
   └─ `lspci -vvv` should show clean Link Status (no errors)

Enumeration + driver:
   ├─ `dmesg | grep pci` for enum + driver init log
   ├─ Verify all BARs assigned and accessible
   ├─ MSI-X allocated, `cat /proc/interrupts`
   └─ Driver functional test (vendor tool)

Traffic stress:
   ├─ Sequential + random IO via `fio` (NVMe) or `pktgen` (NIC)
   ├─ Monitor `setpci -s ... ECAP_AER+0x1C.L` for correctable counts
   └─ Long-soak 24 hr; AER counts should be 0 or trivially low

Compliance:
   ├─ Vendor / SIG compliance tool
   ├─ Eye-mask capture at compliance point
   └─ Sign-off report
```

---

## What's Required

- **PCIe IP RTL** at the validation speed/width.
- **Refclk source** with jitter inside spec; **PERST# control** observable.
- **PCIe protocol analyser** (Lecroy Summit T3, Teledyne PETrainer) with interposer for the speed under test.
- **Eye-mask oscilloscope** with PCIe spec masks loaded.
- **Linux host** with `lspci`, `setpci`, `dmesg`, `msr-tools`, vendor driver.
- **Traffic generators** (`fio`, `pktgen`, vendor tool).
- **AER monitor script** that polls `ECAP_AER+0x1C` and alerts on count change.
- **LTSSM stuck-state diagnostic ladder** as a printed reference.
- **Interop test list** maintained per stepping.
- **Compliance test bench** (PCIe-SIG GoldenFinger or vendor equivalent).

---

## Tools

| Phase | Tool | Use |
|---|---|---|
| **Pre-Si sim** | Synopsys VCS / Cadence Xcelium + PCIe VIP (Synopsys VC VIP, Cadence Denali) | TLP/DLLP/LTSSM coverage |
| **Pre-Si formal** | JasperGold PCIe app | LTSSM + DLLP property proofs |
| **Pre-Si stim** | Perspec PCIe lib | Scenario stim across sim/emul/post-Si |
| **Pre-Si waveform** | Verdi + PCIe protocol decoder | LTSSM + TLP view in sim |
| **Emulation** | ZeBu / Palladium + PCIe transactor or speed-bridge | Boot OS; real driver |
| **Post-Si — control** | `lspci -vvv -PP`, `setpci`, `/sys/bus/pci/...` | Cfg space, AER, ASPM/L1SS state |
| **Post-Si — analyser** | Lecroy Summit T3, Teledyne PETrainer Z, Keysight U4154 | TLP/DLLP capture, LTSSM trace, eq capture |
| **Post-Si — eye** | Keysight DSAV / Tektronix DPO + PCIe app | Compliance eye-mask measurement |
| **Post-Si — error mon** | `aer-inject` / `pcieport-aer` driver | Inject errors; verify reporting |
| **Compliance** | PCIe-SIG GoldenFinger / vendor compliance card | CPAT generation + capture |
| **Traffic** | `fio`, `iperf3`, `pktgen`, vendor tools | Load |

---

## Bugs in this Topic

- **Stuck in Detect — refclk dead on one side:** customer board had wrong refclk source-select strap. *Catch: scope on refclk both sides; pre-Si had no equivalent — caught only in lab.*
- **Stuck in Polling — lane polarity reversed:** all 8 lanes had differential polarity swapped on the board; IP didn't auto-detect polarity. *Catch: spec-mandated polarity reversal coverage in pre-Si + board bring-up test card.*
- **Stuck in Recovery.Equalization at Gen4:** TX FFE range insufficient for the channel loss; eq protocol kept requesting more pre-emphasis than TX could provide. *Catch: add re-driver; pre-Si coverage of eq-coefficient-out-of-range.*
- **L0p loop under sustained traffic:** marginal SI at Gen4; receiver errors trigger retrain every ~30 s. *Catch: long-soak AER count; SI fix (board, re-timer); pre-Si SVA `p_no_retrain_during_clean_traffic` over long window.*
- **Completion timeout under heavy NPNP traffic:** completer's response-credit window narrower than master's outstanding-NPNP. *Catch: pre-Si NPNP stress; analyser capture; flow-credit cover.*
- **AER UnCor bit set during clean test:** a Malformed-TLP error fired because checker reacted to a TLP that crossed a Max_Payload boundary; turned out checker was too aggressive. *Catch: AER-must-be-silent assertion + per-error injection coverage.*
- **Hot-reset leaves driver state inconsistent:** endpoint reset cleanly, but driver didn't re-init MSI-X table → no IRQs post-reset. *Catch: hot-reset + IRQ-fire cross-coverage.*
- **FLR doesn't quiesce pending DMA:** FLR completed but a DMA in flight wrote *after* FLR claimed completion. *Catch: FLR + outstanding-DMA scenario; SVA on FLR completion gating.*
- **Lane reversal not handled:** IP advertised lane-reversal support but lane-numbering FSM had off-by-one. *Catch: directed lane-reversal scenarios across all width × lane-rev combos.*
- **Interop: NVMe vendor X fails enum on rootport Y:** vendor X's cap chain had a self-referencing extended cap; rootport Y's enumerator hung. *Catch: interop matrix; cap-chain-loop SVA.*

---

## Interview Explanation (60-90 sec)

> "PCIe bring-up is a ladder: refclk on, PERST# released, the LTSSM walks Detect → Polling → Configuration → L0 at Gen1, then equalisation, then Recovery to the target speed. When a link doesn't reach L0 I work the stuck-state ladder — Detect means refclk or RX terminations; Polling means polarity or PLL lock; Recovery.Equalization means eq coefficients out of range, usually a SI fix outside the IP. The validation craft is in the assertions and the traffic stress — AER must be silent under clean traffic, every error class must have an injection scenario, every TLP/DLLP type must be exercised, every reset event (hot, FLR, surprise-down) must leave the IP and the SW in a defined state. We run compliance with the SIG GoldenFinger and eye-mask at the compliance point. We run a long-soak with AER count monitoring. We run an interop matrix against other vendors' silicon, because every new endpoint × rootport pair finds a spec-interpretation difference. The marquee post-Si bug I'd point to is the L0p loop — link in L0 briefly then back to Recovery — usually marginal SI causing receiver errors that trigger retrain; the fix is board-level (re-timer) but the val signature is a slow rise in AER correctable count plus periodic re-train events on the analyser. The pre-Si rule that prevents the next variant is an SVA over a long window: no retrain during a clean traffic burst."

---

## Architect Drills

*PCIe bring-up failures have a structured causal graph. Reason without an answer key.*

**D1. Link is stuck in Polling.Active. Reason about how to decide between TX/RX swap, polarity reversal, and PLL lock as the most likely cause.**
*Constraints to weigh:* what each fault makes the analyser show, what board-level evidence (probe RX termination) reveals, ordered cost of each remediation.

**D2. Gen3+ eq doesn't converge — reason about whether the bug is in TX, RX, or the channel.**
*Constraints to weigh:* eq request/response visibility on analyser, eye captured at TX vs RX pad, what changes when a re-driver is added to the channel.

**D3. AER correctable count is climbing at 100/s under traffic. Reason about whether this is acceptable.**
*Constraints to weigh:* spec allows correctable (link still works), what it predicts about future uncorrectable rate, customer SLA, whether SI margin is being eroded.

**D4. The link is in L0 but a `setpci` read returns all-1s. Reason about the next 3 diagnostics.**
*Constraints to weigh:* device cfg vs MMIO access path differences, AER state, link width at L0 (could be 1 instead of 16), driver vs raw access.

**D5. FLR completed but the device is still issuing DMA. Reason about where the FLR completion check failed.**
*Constraints to weigh:* spec says FLR must quiesce inflight transactions before signalling completion, IP FSM may have signaled completion at wrong point, validation gap.

**D6. The interop matrix finds a bug with one vendor's endpoint only. Reason about how to decide if it's our bug or theirs.**
*Constraints to weigh:* spec wording vs implementation interpretation, what other rootports do with the same endpoint, what other endpoints do on our rootport, who has the larger market share (and thus dictates the de-facto interpretation).

**D7. A pre-Si vplan covered every LTSSM state but post-Si saw L0p loops not seen in sim. Reason about what's missing.**
*Constraints to weigh:* long-window properties not expressible at sim runtime, SI/electrical reality absent from sim, what assertion (over many cycles) would catch the symptom in emul.

---

## Coding A — Systems C: LTSSM stuck-state classifier

Given an LTSSM trace (list of substates with timestamps), classify the failure mode and emit the recommended next diagnostic.

```c
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *substate;
    int         dwell_ms;
} trace_entry_t;

static const char *classify(const trace_entry_t *t, int n) {
    /* Final substate decides the diagnosis */
    const char *last = t[n-1].substate;
    int dwell = t[n-1].dwell_ms;

    if (strcmp(last, "Detect.Quiet") == 0 && dwell > 100)
        return "STUCK in Detect.Quiet → check PERST# release, refclk on partner side, RX termination enabled";
    if (strcmp(last, "Detect.Active") == 0)
        return "STUCK in Detect.Active → no RX detect; check cable/connector, lane polarity, partner powered";
    if (strncmp(last, "Polling", 7) == 0)
        return "STUCK in Polling → check TX/RX swap, polarity, PHY PLL lock, TS1 format";
    if (strncmp(last, "Configuration", 13) == 0)
        return "STUCK in Configuration → check lane width caps both sides, lane reversal, lane skew";
    if (strncmp(last, "Recovery.Equalization", 21) == 0)
        return "STUCK in Recovery.Eq → coefficients out of range; consider re-driver or fix SI";
    if (strncmp(last, "Recovery", 8) == 0)
        return "STUCK in Recovery (not Eq) → marginal SI, eye margin low; check AER correctable count";

    /* L0 reached — check for L0p loop */
    int recov_count = 0;
    for (int i = 0; i < n; i++)
        if (strncmp(t[i].substate, "Recovery", 8) == 0) recov_count++;
    if (recov_count >= 3 && strcmp(last, "L0") == 0)
        return "L0p LOOP → frequent Recovery cycles under traffic; SI marginal, check AER cor count";

    return "L0 stable";
}

int main(void) {
    trace_entry_t healthy[] = {
        {"Detect.Quiet", 1}, {"Detect.Active", 2}, {"Polling.Active", 24},
        {"Polling.Configuration", 8}, {"Configuration.Linkwidth.Start", 4},
        {"L0", 60000}
    };
    printf("Healthy: %s\n", classify(healthy, 6));

    trace_entry_t stuck_polling[] = {
        {"Detect.Quiet", 1}, {"Detect.Active", 2}, {"Polling.Active", 5000}
    };
    printf("Stuck-polling: %s\n", classify(stuck_polling, 3));

    trace_entry_t l0p[] = {
        {"Detect.Active",2},{"Polling.Active",20},{"L0",1500},
        {"Recovery.RcvrLock",2},{"L0",1100},
        {"Recovery.RcvrLock",2},{"L0",900},
        {"Recovery.RcvrLock",2},{"L0",1200}
    };
    printf("L0p: %s\n", classify(l0p, 9));

    trace_entry_t stuck_eq[] = {
        {"Polling.Active",20},{"L0",100},
        {"Recovery.RcvrLock",2},{"Recovery.Equalization",3000}
    };
    printf("Eq stuck: %s\n", classify(stuck_eq, 4));
    return 0;
}
```

---

## Bug Anchor

- **Day 18 (LTSSM):** the stuck-state ladder is the practical use of that day's FSM.
- **Day 19 (enum):** enum can't start until L0 reached here.
- **Day 20 (PCIe PM):** PM transitions stress the same LTSSM paths.
- **Day 17 (post-Si PM val):** L0p surfaces as a PM blocker (link can't reach L1.2 if it's re-training every few seconds).
- **Day 22 (PkgC ↔ L1.2):** L1.2 only entered from a clean L0 — bring-up failures here block PM there.

## Exit Check

- [ ] Drew the 10-step bring-up ladder.
- [ ] Recited the LTSSM stuck-state diagnostic table.
- [ ] Explained Gen3+ equalisation in 4 phases.
- [ ] Stated when L0p indicates SI vs IP bug.
- [ ] Named 6 AER error classes and stated the "AER silent under clean traffic" rule.
- [ ] Stated 3 reset events (hot, FLR, surprise-down) and recovery semantics.
- [ ] Named the post-Si tools used to capture LTSSM + TLP + eye + AER.
- [ ] Explained why an interop matrix is necessary even after compliance passes.

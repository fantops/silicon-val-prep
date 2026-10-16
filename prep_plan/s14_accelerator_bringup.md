# S14 - Accelerator bring-up

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [DSA fundamentals §1–§3, §8](../dsa-val/fundamentals/dsa_ip_fundamentals.md).
- **Must answer:**
  - The chain applied: RCiEP → enumerated (S11) → BARs mapped → **idxd** probes.
  - Architecture: **WQs (dedicated vs shared) → groups → engines**. Configure → enable device → enable WQ. Each enable is a handshake FSM (S2) with status and error registers.
  - Bring-up failures: an enable that never completes, a config rejected with an error code, PASID/ATS not enabled (S11), or the IOMMU not in scalable mode (S6/S13).
- **Work link:** write the configure → enable sequence you use at work as an FSM, and list the observable at each step.
- **Handoff →** The WQ is enabled. What happens when one job is submitted?

## Study notes

### DSA architecture and staged bring-up

DSA is presented to software as a PCIe Root Complex Integrated Endpoint (RCiEP). The driver discovers the function, maps its BARs, reads capabilities, and configures work queues (WQs), groups, and engines. A group associates WQs with available engines. A dedicated WQ has a single owner; a shared WQ supports multiple submitters and backpressure. Bring-up should proceed in observable stages: discover -> validate capabilities -> configure resources -> enable device -> enable WQ -> submit a controlled operation.

- A successful config read proves visibility, not a usable queue.
- Keep configuration and enablement distinct; check each status/error register after its handshake.
- If an enable bit does not reach the expected state, capture configuration, state, error code, timeout, and reset history before retrying.
- Confirm the IOMMU mode and any PASID/ATS prerequisites before attempting shared-virtual-address workloads.

### Worked example: WQ enable times out

Freeze submissions, read back the requested WQ configuration, verify group/engine assignment and capability limits, check device and WQ state/error fields, then confirm translation and interrupt prerequisites. Compare the first state transition that differs from the expected enable FSM; do not repeat enable writes blindly.

### Concept - [day36_pcie_bringup_functional_val.md](archive/day36_pcie_bringup_functional_val.md)

PCIe bring-up is the structured process of taking a designed link from reset to **L0 carrying real traffic**, then proving — through analyser captures, AER monitoring, compliance test patterns, and traffic stress — that the link, the transaction layer, and the SW driver path all match the spec under every realistic scenario. The hardest part is **diagnosing a link that won't reach L0**: PCIe gives you a handful of LTSSM substates and a pile of side-band signals, and the bug could be electrical, polarity, clock, equalisation, side-band, or SW. **A structured LTSSM-stuck-state ladder is the load-bearing diagnostic skill.**

### Deep Study Notes - [day36_pcie_bringup_functional_val.md](archive/day36_pcie_bringup_functional_val.md)

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
Phase 4  — exchange TX coefficients; receivers measure eye
Phase 6  — receiver requests TX coefficient adjustments
Phase 7  — final convergence; lock in chosen coefficients
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

### 9a. What PCIe emulation covers vs cannot cover

This is the Nvidia/PythonSV-style boundary question. Answer in two columns: what emulation is excellent for, and what must wait for silicon/board.

| Area | Emulation can cover | Not covered / weak in emulation |
|---|---|---|
| **FW + driver path** | Real BIOS/UEFI/Linux enumerating RTL through transactors | Real boot-time board races from straps, PERST#, refclk stability |
| **Config / BAR / MSI** | Enumeration, cap walk, BAR sizing, MSI/MSI-X programming, AER paths | OS timing sensitivity caused by real device init latency / analog reset release |
| **TLP/DLL protocol** | TLP types, ordering, credits, replay/error injection via VIP/transactor | True electrical bit errors, jitter-driven CRC failures, marginal receiver behavior |
| **LTSSM logic** | State transitions, timeouts, directed stuck-state scenarios, illegal transitions | Real PHY training quality, CDR lock margin, lane skew, connector/cable effects |
| **Traffic stress** | Real workloads at MHz speed, long software flows, cross-IP integration | Full-rate bandwidth, thermal/power stress, SI-induced intermittent retrains |
| **Power management** | ASPM/L1/L1SS control sequencing and register programming | Actual L1.2 common-mode voltage off, CLKREQ# board routing, wake latency on real PHY |
| **Interop** | Modelled endpoints/rootports, some speed-bridge setups | Other vendors' silicon quirks and de-facto spec interpretations |

**60-sec answer:**

> "Emulation is strong for HW/FW integration: real BIOS or Linux can enumerate real RTL through a PCIe transactor, program BARs, MSI/MSI-X, AER, run drivers, and exercise LTSSM/control logic at MHz speed. It is weak for what depends on the real PHY and board: eye margin, jitter, CDR lock, equalization quality, lane skew, refclk/PERST# timing on the platform, L1.2 analog behavior, full-rate traffic, thermal/power effects, and interop with other vendors' silicon. So if a bug is protocol, config, ordering, reset sequencing, or FW/driver integration, I expect emulation to catch it. If it is SI/PI, analog PHY, compliance eye, marginal retrains, or vendor interop, I expect post-Si lab tools to catch it."

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

### Bugs in this Topic - [day36_pcie_bringup_functional_val.md](archive/day36_pcie_bringup_functional_val.md)

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

### Interview Explanation (60-90 sec) - [day36_pcie_bringup_functional_val.md](archive/day36_pcie_bringup_functional_val.md)

> "PCIe bring-up is a ladder: refclk on, PERST# released, the LTSSM walks Detect → Polling → Configuration → L0 at Gen1, then equalisation, then Recovery to the target speed. When a link doesn't reach L0 I work the stuck-state ladder — Detect means refclk or RX terminations; Polling means polarity or PLL lock; Recovery.Equalization means eq coefficients out of range, usually a SI fix outside the IP. The validation craft is in the assertions and the traffic stress — AER must be silent under clean traffic, every error class must have an injection scenario, every TLP/DLLP type must be exercised, every reset event (hot, FLR, surprise-down) must leave the IP and the SW in a defined state. We run compliance with the SIG GoldenFinger and eye-mask at the compliance point. We run a long-soak with AER count monitoring. We run an interop matrix against other vendors' silicon, because every new endpoint × rootport pair finds a spec-interpretation difference. The marquee post-Si bug I'd point to is the L0p loop — link in L0 briefly then back to Recovery — usually marginal SI causing receiver errors that trigger retrain; the fix is board-level (re-timer) but the val signature is a slow rise in AER correctable count plus periodic re-train events on the analyser. The pre-Si rule that prevents the next variant is an SVA over a long window: no retrain during a clean traffic burst."

---

### Architect Drills - [day36_pcie_bringup_functional_val.md](archive/day36_pcie_bringup_functional_val.md)

*PCIe bring-up failures have a structured causal graph. Reason without an answer key.*

**D1. Link is stuck in Polling.Active. Reason about how to decide between TX/RX swap, polarity reversal, and PLL lock as the most likely cause.**
*Constraints to weigh:* what each fault makes the analyser show, what board-level evidence (probe RX termination) reveals, ordered cost of each remediation.

**D2. Gen3+ eq doesn't converge — reason about whether the bug is in TX, RX, or the channel.**
*Constraints to weigh:* eq request/response visibility on analyser, eye captured at TX vs RX pad, what changes when a re-driver is added to the channel.

**D3. AER correctable count is climbing at 100/s under traffic. Reason about whether this is acceptable.**
*Constraints to weigh:* spec allows correctable (link still works), what it predicts about future uncorrectable rate, customer SLA, whether SI margin is being eroded.

**D16. The link is in L0 but a `setpci` read returns all-1s. Reason about the next 3 diagnostics.**
*Constraints to weigh:* device cfg vs MMIO access path differences, AER state, link width at L0 (could be 1 instead of 16), driver vs raw access.

**D17. FLR completed but the device is still issuing DMA. Reason about where the FLR completion check failed.**
*Constraints to weigh:* spec says FLR must quiesce inflight transactions before signalling completion, IP FSM may have signaled completion at wrong point, validation gap.

**D18. The interop matrix finds a bug with one vendor's endpoint only. Reason about how to decide if it's our bug or theirs.**
*Constraints to weigh:* spec wording vs implementation interpretation, what other rootports do with the same endpoint, what other endpoints do on our rootport, who has the larger market share (and thus dictates the de-facto interpretation).

**D19. A pre-Si vplan covered every LTSSM state but post-Si saw L0p loops not seen in sim. Reason about what's missing.**
*Constraints to weigh:* long-window properties not expressible at sim runtime, SI/electrical reality absent from sim, what assertion (over many cycles) would catch the symptom in emul.

---

### Exit Check - [day36_pcie_bringup_functional_val.md](archive/day36_pcie_bringup_functional_val.md)

- [ ] Drew the 10-step bring-up ladder.
- [ ] Recited the LTSSM stuck-state diagnostic table.
- [ ] Explained Gen3+ equalisation in 4 phases.
- [ ] Stated when L0p indicates SI vs IP bug.
- [ ] Named 6 AER error classes and stated the "AER silent under clean traffic" rule.
- [ ] Stated 3 reset events (hot, FLR, surprise-down) and recovery semantics.
- [ ] Named the post-Si tools used to capture LTSSM + TLP + eye + AER.
- [ ] Explained why an interop matrix is necessary even after compliance passes.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S13](s13_iommu_pasid_ats.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S15](s15_accelerator_job_e2e.md)

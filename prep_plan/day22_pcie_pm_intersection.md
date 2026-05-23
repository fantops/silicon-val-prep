# Day 22 — PCIe ↔ Package PM Intersection

## Concept

PCIe and CPU package PM are **cross-domain dependencies** — neither can reach its deepest state without the other in lockstep. The platform's PCU computes PkgC eligibility from a set of **blocker inputs**, and **"any rootport not in L1.2"** is one of those blockers. Conversely, when the package enters PC6, the rootport's clock + power are also affected — if PCIe wakes mid-PC entry, the negotiation has to abort cleanly. **Most marquee PM bugs on modern x86 platforms live in this intersection.** ADR, S0ix, mid-FLIT wake, refclk-after-PkgC — every one is a PCIe-and-PCU joint failure mode.

## Why this day in the flow

D16 covered pre-Si PM val. D17 covered post-Si PM val with the ADR + C6 bug stories. D20 covered PCIe PM with L1.2 in isolation. **Today is the day those threads merge into the actual platform behaviour** — and the day the val engineer learns to debug across the IP boundary. This is the most-asked-about topic in PM val interviews because it's where two big specs collide.

In the Layer map this is the **closing day of Phase 4 (PCIe)**, and the synthesis day across PM + PCIe — the right place to land before the wrap days (D23 Microsoft pointers + D24 Intel mock).

---

## Deep Study Notes

### 1. The PkgC6 blocker chain

```
   PkgC6 entry permitted   ⇔   ALL of:
                                ├─ every active core in CC6 (or off)
                                ├─ all caches flushed / coherent
                                ├─ display in self-refresh (or off)
                                ├─ DRAM IMC ready for self-refresh
                                ├─ all PCIe rootports in L1.2
                                ├─ all USB ports U3 (deep sleep)
                                ├─ all audio DMA idle
                                ├─ management engine idle
                                └─ no pending DMA / interrupt
```

Each blocker is a **bit in a PCU input register** that the IP owning that domain sets/clears. **The race surface is: who sets/clears their bit when, relative to PCU's PkgC6-entry decision cycle.**

### 2. Why PCIe matters most

PCIe rootports almost always **gate PkgC** on real platforms because:

1. Rootport refclk is on the **PCU power island** — if the PCU collapses, refclk is gone.
2. PCIe L1.2 is the *only* link state where refclk can be removed safely.
3. Any device that doesn't enter L1.2 (driver disabled it, LTR too tight, CLKREQ# stuck) holds the whole package up.
4. socwatch and equivalent telemetry will almost always show PCIe as the blocker.

### 3. The L1.2 ↔ PkgC handshake (concrete sequence)

```
   T0: All cores idle in CC6; OS governor requests PkgC6
       │
       ▼
   T1: PCU reads blocker chain.
       Cores: ✓     Display: ✓     DRAM: ✓     PCIe RP0: L0    ← NOT READY
       │
       ▼
   T2: PCU is permitted to send a "request L1.2 entry" hint to RP0
       via SW, or wait for RP0's own ASPM logic to enter L1.2
       │
       ▼
   T3: RP0 + EP negotiate L1 → L1.1 → L1.2 (over many µs)
       │
       ▼
   T4: PCU re-reads chain. All blockers clear.
       │
       ▼
   T5: PCU begins PkgC6 entry sequence:
       ├─ Snoop / cache flush per-IP
       ├─ Save uncore PSU state
       ├─ Gate refclk to PCIe rootport (RP0 now relies on Vcm staying off)
       ├─ Save mesh / LLC ways
       └─ Open header on uncore rail
       │
       ▼
   T6: Package in PC6. Only AON alive.
       │
       ▼
   T7: Wake event arrives (e.g., timer at PCU, IRQ from AON GPIO).
       │
       ▼
   T8: PCU wakes:
       ├─ Close header, rail stable
       ├─ Restore mesh / LLC
       ├─ Re-enable refclk to PCIe rootport
       └─ Restore uncore PSU
       │
       ▼
   T9: PCIe RP0 sees refclk back; CLKREQ# from device may already be asserted
       (if device woke first); link goes Recovery → L0
       │
       ▼
  T10: Cores wake from CC6; PkgC6 exit complete
```

**Note T8–T9:** PCIe link wake races package wake. If refclk arrives at rootport *after* CLKREQ# was already asserted by the device, the rootport has to gracefully resume; if refclk arrives *before* CLKREQ#, the rootport sits idle waiting. Both must work.

### 4. The deadlock farm

| Deadlock scenario | Symptom |
|---|---|
| PCU requests PCIe L1.2; PCIe denies because pending TLP; PCU keeps polling but won't service the TLP because it's mid-PkgC eval | Spin |
| PCIe device asserts CLKREQ# mid-PkgC6 entry (T5); PCU is gating refclk; refclk-gate sees stale "in L1.2" signal | Refclk gated despite CLKREQ# — link can never wake |
| PCU enters PC6; PCIe rootport's PME path needs uncore alive; PME from EP drops | Lost wake |
| Two rootports race to report "in L1.2" to PCU; one reports stale state | Premature PkgC entry |
| Endpoint LTR update lands during PCU PkgC-eval cycle | PCU sees stale LTR, enters PkgC with too-loose tolerance |

Each is a coverage row in the **cross-IP PM vplan**.

### 5. The ADR ↔ PkgC interaction (recap + deeper)

ADR (Asynchronous DRAM Refresh) is the emergency path that flushes the CPU's write-pending queues to NVDIMM on impending power loss. The ADR sequence overlaps with normal PM in dangerous ways:

```
   Normal PkgC entry:                ADR-event PkgC entry:
   ├─ snoop flush                    ├─ snoop flush             (same)
   ├─ uncore quiesce                 ├─ uncore quiesce
   ├─ PCIe rootport gate refclk      ├─ ADR triggers WPQ flush  ← inserted out of order
   ├─ DRAM self-refresh              ├─ DRAM self-refresh
   └─ Header off                     └─ Header off
```

The bug from the D17 story: under one stepping the PCU FSM inserted the WPQ flush **between** PCIe rootport quiesce and the snoop flush, racing an in-flight DMA write through PCIe. Fix was a microcode-enforced barrier; the lasting fix was a coverage row `(ADR-event × PCIe-RP-state × DRAM-state)` and an SVA `p_adr_flush_after_pcie_quiesce`.

### 6. The L0p mid-FLIT story (90 sec)

**Context:** On a specific Gen4 SoC, when PkgC6 was being aborted (a wake landed mid-entry), the PCU re-enabled refclk to a rootport that was in the **middle of an outstanding FLIT (flow control unit)** on the link. The rootport had buffered a partial FLIT in its TX FIFO that was supposed to be drained pre-PC entry. Refclk coming back mid-FLIT caused the rootport to resume transmission from a corrupted FIFO state — emitting a malformed TLP that the endpoint AER'd as Malformed. The link dropped to Recovery → L0p loop → eventually surprise-down.

**Detection:** A customer reported intermittent NIC link drops every few hours on idle servers. socwatch showed PkgC entry rate matched the drop rate. PCIe analyser caught the malformed TLP. PCU opcode trace showed the abort path re-enabled refclk before the rootport's FIFO drain completed.

**Root cause:** PCU FSM's PC-abort path skipped the rootport-drain handshake under the assumption that the abort happened before refclk gate. But the abort signal could arrive *after* the refclk gate had partially closed — a 4-cycle window the FSM didn't account for.

**Fix:** PCU FW patched to always re-issue the rootport-drain handshake on abort, regardless of how far into the gate the entry had progressed.

**Lesson:** Cross-IP vplan needed `(PC-abort-phase × PCIe-RP-FIFO-state)` coverage. Pre-Si had `(PC-abort-phase)` but the second axis was missing. SVA `p_no_refclk_resume_with_pending_fifo` added.

### 7. socwatch as the cross-IP oracle (recap)

When `Pkg%pc6 = 0` and turbostat says cores are at C6, socwatch breaks down the block:

```
S0ix Block Reasons (% of non-S0ix time)
  Uncore not idle:           8%
  PCIe LTSSM not L1.2:      72%   ← this is the bug to chase
  DRAM not in self-refresh:  3%
  USB / Display / Audio:    17%
```

Then per-rootport:
```
  PCIe RP0 (NVMe):     L0 100%
  PCIe RP1 (NIC):      L1.2 95% / L1 5%
  PCIe RP2 (GPU):      L0 100%
```

→ chase RP0 + RP2. Common causes: driver disabled ASPM, LTR too tight, hot endpoint never idle, MSI delivery spikes during "idle."

### 8. LTR end-to-end — the silent killer

The LTR chain:

```
   endpoint driver computes "I need 5 µs of wake latency"
            │
            ▼
   endpoint emits LTR TLP with value = 5 µs
            │
            ▼
   rootport receives, propagates to PCU
            │
            ▼
   PCU aggregates LTR across all rootports → max-allowed PkgC depth
            │
            ▼
   if any LTR < PkgC-exit latency → PkgC denied
```

Failure modes:
- Endpoint never emits LTR → platform default (often "tight" = no PkgC).
- Endpoint emits one LTR at init, never updates → wrong value forever.
- Driver "disables" LTR thinking it's safer → blocks PkgC silently.
- Rootport drops LTR if AER bit set on the link → after one correctable error, PkgC blocked.

**Validation surface:** LTR-update mid-traffic, LTR-on-error, LTR-aggregation-across-rootports.

### 9. The cross-IP PM vplan (pattern)

| Scenario | Stim | Score | Coverpoints | SVAs |
|---|---|---|---|---|
| PkgC entry with one RP in L0 → blocked | All cores idle; RP0 not in L1.2 | PCU stays in PC2; no PkgC entry attempt | `cp_pkgc_blocked × cp_blocker=PCIE_RP0` | `p_no_pkgc_if_rp_not_l12` |
| PkgC abort by RP wake mid-entry | Aligned idle; mid-PCU-PkgC-eval, RP1 EP wake | PCU aborts cleanly; no link corruption | `cp_pkgc_abort_phase × cp_abort_source=RP` | `p_no_refclk_resume_with_pending_fifo` |
| L1.2 entry with PCU concurrently doing PkgC-eval | All cores idle; RP1 entering L1.2; PCU evaluating | Both succeed; no race | `cp_l12_entry x cp_pkgc_eval_phase` | `p_no_race_blocker_update_during_eval` |
| Two RPs race to report L1.2 | Aligned dual-RP L1.2 entry | Both blocker bits set atomically | `cp_dual_rp_l12` | `p_atomic_blocker_update` |
| LTR update during PkgC eval | RP1 endpoint updates LTR mid-eval | PCU re-evaluates with new value | `cp_ltr_update_during_eval` | `p_ltr_consistent_with_decision` |
| ADR triggered during PkgC | Force ADR + PkgC entry simultaneously | WPQ flushed before any uncore PD | `cp_adr × cp_pkgc_phase` | `p_adr_flush_before_uncore_pd` |
| PME from L1.2 during PkgC | EP in L1.2, PCU in PC6, EP issues PME | Wake propagates correctly | `cp_pme × cp_pkgc` | `p_pme_wakes_pkg` |

A real platform vplan has 50-100 cross-IP scenarios. Each must be exercised in unit sim, integrated sim, emul, and post-Si.

### 10. Debug workflow when PkgC won't enter

```
Step 1: turbostat — confirm cores at C6, but Pkg%pc6 low
        │
        ▼
Step 2: socwatch — read block-reason breakdown
        │
        ▼
Step 3: if PCIe: socwatch per-RP — which rootport
        │
        ▼
Step 4: lspci -vvv on that RP + downstream — ASPM enabled? L1SS enabled? LTR value?
        │
        ▼
Step 5: /sys/.../power/control on the device — runtime PM on?
        │
        ▼
Step 6: PCIe analyser — is link actually entering L1.2 or sitting in L0?
        │
        ▼
Step 7: scope on CLKREQ# + refclk — side-band timing
        │
        ▼
Step 8: PCU opcode trace — what did PCU decide?
        │
        ▼
Step 9: rdmsr PKG_CST_CONFIG_CONTROL — is max PkgC capped?
        │
        ▼
Step 10: bisect: disable IPs one-by-one until PkgC enters; the last disabled IP is the bug
```

This is the **playbook** for the most common post-Si PM ticket on the planet.

---

## Bring-up Flow — Cross-IP PM (Lab)

```
Stage A — Confirm each axis works in isolation
   ├─ Cores reach CC6 (Day 13)
   ├─ Each PCIe rootport reaches L1.2 alone (Day 20)
   ├─ Display can self-refresh
   ├─ DRAM can self-refresh
   └─ All blocker IPs verified individually

Stage B — Align all blockers, attempt PkgC
   ├─ Idle workload; all PM enabled
   ├─ Observe Pkg%pc6 via turbostat
   ├─ If non-zero: success baseline
   └─ If zero: jump to bisect (Step 10 above)

Stage C — Per-RP characterisation
   ├─ socwatch per-RP residency
   ├─ For each non-L1.2 RP: lspci, /sys, LTR check
   └─ File CRs per offending IP/driver

Stage D — Stress
   ├─ Run repeated PkgC entry/exit cycles (busy-wait + sleep)
   ├─ Watch for AER, link drop, PME-lost
   └─ Long-soak 24-72 hr

Stage E — Corner sweeps
   ├─ PVT corners; thermal sweeps
   ├─ Multi-RP simultaneous entry
   └─ ADR injection if platform supports

Stage F — Sign-off
   ├─ Expected PkgC residency met at all corners
   ├─ Zero AER under PM cycling
   └─ Hand cross-IP vplan + scenario library back to pre-Si team
```

---

## What's Required

- **Cross-IP PM vplan** (50-100 scenarios) covering PkgC × PCIe × ADR × DRAM × display etc.
- **PCU stimulator** that can force PkgC-eval phases for cross-IP scenario stim.
- **PCIe IP harness** with controllable L1.2 entry/exit, refclk gate, CLKREQ#.
- **DRAM model** with self-refresh entry/exit timing.
- **ADR injector** in TB (and a way to fire it post-Si via debug pin).
- **socwatch + per-RP residency** in OS image.
- **PCIe analyser** with multi-rootport capture if available.
- **Scope channels** on refclk + CLKREQ# + PERST# for each RP.
- **Quarch on uncore + per-RP rails** for analog correlation.

---

## Tools

| Phase | Tool | Use |
|---|---|---|
| **Pre-Si sim** | VCS / Xcelium with PCIe + DRAM + PCU models | Cross-IP scenario sim with UPF |
| **Pre-Si formal** | JasperGold (PCU FSM + blocker logic) | Liveness + safety on cross-IP arbitration |
| **Pre-Si scenario** | Perspec (SoC-level PM) | Cross-IP scenarios reusable into emul + post-Si |
| **Emulation** | ZeBu / Palladium booting Linux | Real driver behaviour drives the cross-IP surface |
| **Post-Si — telemetry** | `turbostat`, `socwatch` (Intel) | Cross-IP block-reason attribution |
| **Post-Si — per-RP** | `lspci -vvv -PP`, `/sys/bus/pci/devices/.../power/control`, `/sys/.../link/l1_2_aspm` | Per-rootport PM state + control |
| **Post-Si — PCU** | SVTools PMC trace | PCU opcode trace + blocker view |
| **Post-Si — analyser** | Lecroy Summit T3, Teledyne PETrainer | L1.2 entry/exit per RP |
| **Post-Si — scope** | Keysight + scope-probes on CLKREQ#/refclk | Side-band timing during PkgC |
| **Post-Si — power** | Quarch PPM multi-channel | Per-rail current correlation |

---

## Bugs in this Topic

- **L0p mid-FLIT on PC abort** (story above): PCU refclk re-enable raced rootport FIFO drain. *Fix: microcode handshake; SVA + cross-IP coverage.*
- **ADR-WPQ flush re-ordered** (D17 story): cross-IP PCU FSM. *Fix: barrier insertion; cross-coverage.*
- **One rootport's driver disables L1.2 in /sys; PkgC stuck at 0%:** distro chose conservative default. *Fix: doc + product-validation script that enables L1.2 per platform.*
- **LTR sticky at tight value after first packet:** endpoint never updates LTR after init. *Fix: driver fix or PCU re-aggregation policy.*
- **Two rootports race for blocker bit:** PCU latches one's value, misses the other; intermittent premature PkgC. *Fix: atomic blocker bus.*
- **PME from L1.2 drops during PkgC exit:** PME path gated by uncore-not-yet-restored. *Fix: PME path moved to AON.*
- **Refclk arrives before CLKREQ# acknowledged on wake:** rootport idle-spins for spec-min before issuing recovery. *Fix: small FSM tweak; SVA on CLKREQ#-vs-refclk ordering.*
- **CLKREQ# board glitch during PkgC:** spurious wake every 8 ms → PkgC residency 5% instead of expected 90%. *Fix: board (RC filter on CLKREQ#); val signature is Quarch + scope correlation.*
- **Hot-plug during PkgC entry:** new device asserts PRSNT# mid-entry; PCU aborts but rootport's enumeration state was already partially torn. *Fix: hot-plug-during-PM-entry SVA + coverage row.*

---

## Interview Explanation (60-90 sec)

> "PCIe and CPU package PM are the joint at the most-validated cross-IP surface on a modern x86. The PCU computes PkgC eligibility from a blocker chain — every active core in C6, all caches flushed, display in self-refresh, DRAM in self-refresh, every PCIe rootport in L1.2 — and any of those not being ready holds the whole package up. PCIe is almost always the dominant blocker because rootport refclk lives on the PCU island; so L1.2 is the only state where refclk can drop. The race surface is two FSMs running in parallel: the PCU PkgC-eval FSM and each rootport's L1.2-entry FSM, plus side-band CLKREQ#/refclk timing. The marquee bug I'd cite is the L0p mid-FLIT: a PkgC abort re-enabled refclk while a rootport had a partial FLIT in its TX FIFO; resumed transmission was malformed; the EP AER'd, link dropped, eventually surprise-down. PCU FW fix was a forced rootport-drain handshake on abort; the lasting fix was a cross-IP coverage cell `(PC-abort-phase × RP-FIFO-state)` and an SVA blocking refclk resume with a pending FIFO. The post-Si debug playbook is socwatch for block-attribution, then lspci/sys for per-RP state, then analyser for actual L1.2 transitions, then scope for side-band timing, then PCU opcode trace if it's still unexplained. Cross-IP PM is where pre-Si vplans are weakest because each unit team validates in isolation, and the cross-products are what bite in the lab."

---

## Architect Drills

*Cross-IP PM is fundamentally about coordinated FSMs racing for resources. Reason without an answer key.*

**D1. Reason about why PCIe rootport refclk lives on the PCU island rather than on always-on.**
*Constraints to weigh:* refclk power cost when always-on (over thousands of rootports across the SKU lineup), wake-latency vs PMC, what "rootport refclk always on" would prevent.

**D2. Two rootports concurrently complete L1.2 entry. Reason about whether the PCU should sample the blocker chain edge-triggered, level-triggered, or with a sampling window.**
*Constraints to weigh:* race window for one transition vs the other, premature PkgC if level-triggered too eagerly, missed entry if edge-triggered, deadlock if sampling window too narrow.

**D3. Reason about whether LTR should be authoritative or advisory.**
*Constraints to weigh:* what an authoritative LTR breaks if device is buggy/conservative, what an advisory LTR doesn't help with if platform ignores, the spec's positioning.

**D4. ADR is a time-critical flush. Reason about how the cross-IP val team proves the flush completes within the rail-decay window.**
*Constraints to weigh:* analog simulation cost, worst-case path through PCU + IMC + WPQ, what cross-IP coverage cells prove the path, whether formal can express the deadline.

**D5. The val team has 8 cross-IP scenarios it can afford to author for emul. Reason about which 8 cover the highest bug surface.**
*Constraints to weigh:* bug-history weighting (ADR, L0p, LTR, refclk race), cross-product coverage, whether random emul can cover the gaps.

**D6. socwatch attributes a PkgC blocker to PCIe RP1 but the analyser shows RP1 actually in L1.2 most of the time. Reason about where the discrepancy could live.**
*Constraints to weigh:* socwatch sample rate vs L1.2 window, PCU's blocker view may lag the actual state, telemetry vs ground-truth mismatch, whose clock is authoritative.

**D7. A future SoC promises "no PC blocker from PCIe — always L1.2 capable instantly." Reason about what HW changes this would require.**
*Constraints to weigh:* refclk power islands, per-port refclk gating, CLKREQ# scaling, side-band per-port, validation surface explosion.

---

## Coding A — Systems C: PkgC blocker arbiter sim

Models the PCU's blocker aggregation across cores, PCIe rootports, display, DRAM. PkgC enters only when all blockers clear; abort if any blocker re-asserts mid-entry.

```c
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool core_c6[8];           /* 8 cores */
    bool rp_l12[4];            /* 4 PCIe rootports */
    bool display_sr;
    bool dram_sr;
    bool usb_idle;
} blockers_t;

typedef enum { PC0, EVAL, ENTERING, PC6, ABORTING } pkgc_t;

static bool all_clear(const blockers_t *b) {
    for (int i = 0; i < 8; i++) if (!b->core_c6[i]) return false;
    for (int i = 0; i < 4; i++) if (!b->rp_l12[i])  return false;
    return b->display_sr && b->dram_sr && b->usb_idle;
}

static const char *first_blocker(const blockers_t *b) {
    for (int i = 0; i < 8; i++) if (!b->core_c6[i]) {
        static char buf[16]; snprintf(buf, sizeof buf, "core[%d]", i); return buf;
    }
    for (int i = 0; i < 4; i++) if (!b->rp_l12[i]) {
        static char buf[16]; snprintf(buf, sizeof buf, "RP[%d]", i); return buf;
    }
    if (!b->display_sr) return "display";
    if (!b->dram_sr)    return "DRAM";
    if (!b->usb_idle)   return "USB";
    return "(none)";
}

static pkgc_t tick(pkgc_t s, const blockers_t *b) {
    switch (s) {
    case PC0:
        if (all_clear(b)) return EVAL;
        return PC0;
    case EVAL:
        if (all_clear(b)) return ENTERING;
        printf("  PkgC eval aborted: blocker=%s\n", first_blocker(b));
        return PC0;
    case ENTERING:
        if (!all_clear(b)) {
            printf("  PkgC entry aborted mid-flight: blocker=%s\n", first_blocker(b));
            return ABORTING;
        }
        return PC6;
    case PC6:
        if (!all_clear(b)) {
            printf("  PkgC exit (wake): blocker=%s\n", first_blocker(b));
            return PC0;
        }
        return PC6;
    case ABORTING:
        return PC0;
    }
    return PC0;
}

int main(void) {
    blockers_t b = {0};
    for (int i = 0; i < 8; i++) b.core_c6[i] = true;
    for (int i = 0; i < 4; i++) b.rp_l12[i]  = true;
    b.display_sr = true; b.dram_sr = true; b.usb_idle = true;

    pkgc_t s = PC0;
    /* Cycle 1: all clear → eval → entering → PC6 */
    for (int t = 0; t < 4; t++) {
        s = tick(s, &b);
        printf("t=%d state=%d\n", t, s);
    }

    /* RP1 wakes mid-PC6 */
    b.rp_l12[1] = false;
    s = tick(s, &b);
    printf("after RP1 wake: state=%d\n", s);

    /* Bug scenario: RP3 oscillates - tests abort path */
    b.rp_l12[1] = true; b.rp_l12[3] = false;
    s = tick(s, &b);  /* PC0 -> EVAL would fail */
    s = tick(s, &b);
    return 0;
}
```

---

## Bug Anchor

- **Day 13 (C-states / S0ix):** PkgC architecture lives there.
- **Day 14 (UPF):** uncore power gating lives there; PCU sequencer races UPF cells.
- **Day 16 (pre-Si PM val):** cross-IP vplan rows from this day go there.
- **Day 17 (post-Si PM val):** ADR and C6 wake-loss stories; this day adds the L0p mid-FLIT story.
- **Day 20 (PCIe PM):** L1.2 mechanics; this day uses them in cross-IP context.
- **Day 21 (PCIe bring-up):** the L0p loop pattern surfaces here when PM races traffic.

## Exit Check

- [ ] Drew the PkgC6 blocker chain with at least 8 inputs.
- [ ] Explained why PCIe is almost always the dominant blocker.
- [ ] Walked through the 10-step L1.2 ↔ PkgC handshake.
- [ ] Named at least 5 cross-IP deadlock scenarios.
- [ ] Told the L0p mid-FLIT story in 90 sec with root cause + pre-Si feedback.
- [ ] Recited the 10-step "PkgC won't enter" post-Si debug playbook.
- [ ] Stated three LTR failure modes that block PkgC.
- [ ] Named 7 cross-IP PM vplan scenarios.

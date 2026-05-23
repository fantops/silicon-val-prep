# Day 20 — PCIe Power Management (ASPM, D-states, L1.2, CLKREQ#, LTR)

## Concept

PCIe defines two orthogonal power-management dimensions: **link-state PM** (ASPM, L0/L0s/L1/L1.1/L1.2) which controls the PHY and refclk, and **device-state PM** (D-states, D0/D1/D2/D3hot/D3cold) which controls device function power. The two interact: a device in D3hot still has its link, a device in D3cold loses both. **L1.2 is the marquee deep state** — refclk is removed, common-mode is dropped, side-band CLKREQ# is the only way back — and it's also the most fragile to validate because exit timing and side-band handshakes are easy to break. **LTR (Latency Tolerance Reporting)** is the SW-side hint that lets the platform pick how deep to go.

## Why this day in the flow

D19 enumerated the device. **Today is how that device and its link save power once enumerated.** PCIe PM is the most-validated cross-IP PM surface — every PkgC on a modern x86 SoC depends on PCIe being in L1.2, and every L1.2 entry depends on the refclk/CLKREQ#/LTR plumbing being correct. **You cannot validate PkgC without validating PCIe PM first.**

In the Layer map this is the **third day of Phase 4 (PCIe)**, the bridge between link-bring-up (D18, D19) and full functional validation (D21) — and the necessary precondition for D22's PkgC ↔ L1.2 intersection.

---

## Deep Study Notes

### 1. Link-state PM — ASPM states overview

| State | Refclk | Common-mode | TX | RX | Wake mechanism | Exit latency |
|---|---|---|---|---|---|---|
| **L0** | on | on | active | active | always-on | 0 |
| **L0s** | on | on | electrical-idle | electrical-idle | any TLP | <1 µs |
| **L1** | on | on | electrical-idle | electrical-idle | electrical idle exit | ~10 µs |
| **L1.1** | on | on | electrical-idle | electrical-idle | CLKREQ# | ~30 µs |
| **L1.2** | **off** | **off** | electrical-idle | electrical-idle | CLKREQ# (assert) | ~100-200 µs |
| **L2/L3** | off | off | off | off | wake event (PME, button) | full re-train |

L0s is symmetric per-direction (TX-L0s and RX-L0s are independent). L1 takes the whole link down to idle. L1.1 and L1.2 are the **L1-SubStates** — added to allow much deeper idle savings on links that are intermittently used.

### 2. L0s — the fastest dive

```
   L0  ──(no TLP for IdleTime)──►  L0s  ──(TLP arrives)──►  L0
```

- Per-direction: TX enters L0s when its own queue idle; RX enters L0s when partner's TX is in L0s.
- Exit: TX leaves L0s by sending FTS (Fast Training Sequences) for ~1 µs to re-establish bit-lock; then resumes data.
- **Not great for power** on modern links — exit cost > save for anything but the lightest workloads.

### 3. L1 — full link idle but clock on

```
   L0  ──(both sides agree)──►  L1  ──(electrical-idle-exit)──►  Recovery  ──►  L0
```

- Negotiated: both sides agree via PM_Active_State_Request_L1 / Accept handshake.
- Refclk still running, common-mode bias still on — quick to recover (~10 µs).
- ASPM control bit in Link Control register at PCIe cap offset 0x10.

### 4. L1 sub-states (L1.1 and L1.2) — where the savings live

L1 sub-states were added in PCIe r3.1 to enable **deep platform idle** (S0ix). The key idea: introduce a **side-band CLKREQ#** signal so the link can drop refclk + common-mode and still be woken predictably.

```
                                CLKREQ#
   refclk source ──┐                ▲
                   ▼                │
                refclk gate  ◄──────┤
                                    │
   root port ◄──── side-band ───────┴────► endpoint
       │                                       │
       └──── main lane (TX/RX) ────────────────┘
```

| | L1.1 | L1.2 |
|---|---|---|
| Refclk | on | **off** |
| Common-mode | on | **off** (Vcm collapses) |
| Side-band CLKREQ# | unused | **used to request refclk back** |
| Power save | ~50% over L1 | ~95% over L1 |
| Exit latency | ~30 µs | ~100-200 µs |
| Mandatory on PkgC platforms | yes (L1.2) | yes |

**Entry handshake (L1.2):**
```
   1. SW enables L1.2 in both endpoint + rootport L1SS Cap
   2. Link goes to L1
   3. Both sides assert PM_L1.2_Entry pattern
   4. Refclk gate closes (refclk off)
   5. Common-mode bias dropped
   6. CLKREQ# de-asserted (high)
   7. Both sides in L1.2; only side-band can wake
```

**Exit handshake (L1.2):**
```
   1. Either side asserts CLKREQ# (drives low)
   2. Refclk source receives CLKREQ#, re-enables refclk
   3. Refclk stabilises (~tens of µs depending on PLL)
   4. Common-mode bias re-applied (~T_CommonMode)
   5. Electrical-idle exit sequence
   6. Link enters Recovery, retrains, returns to L0
```

### 5. LTR — Latency Tolerance Reporting

A device can tell the platform "I can tolerate up to T_snoop and T_nosnoop microseconds of wake latency." The platform's PCU uses this to decide:
- Tight LTR (small T) → stay in L0 or L1.
- Loose LTR (large T) → permit L1.2 and PkgC6.

```
LTR message (in-band TLP):
  +  snoop latency value/scale
  +  no-snoop latency value/scale
```

A device that doesn't issue LTR is treated as "infinite tolerance" or "zero tolerance" depending on platform default — both are usually wrong. **LTR mis-programming is one of the top causes of "PkgC stuck at PC2" bugs.**

### 6. D-states — device function PM

Independent of link state. Programmed in the PM cap (ID 0x01).

| State | Description | Link state implied |
|---|---|---|
| **D0** | Fully on | L0 |
| **D1** | Light sleep, function-defined | L0 or L1 |
| **D2** | Deeper sleep, function-defined | L0 or L1 |
| **D3hot** | Function powered down but rails on; only cfg accessible | L1.2 commonly |
| **D3cold** | Function rails removed | L2/L3 (link down) |

Transitions controlled by writing to `PMCSR` in PM cap:
```
   PMCSR[1:0] = 11 → D3hot
   PMCSR[1:0] = 00 → D0
```

D3cold requires *platform* support to remove rails — most modern designs route this through a GPIO-controlled regulator.

### 7. PME — Power Management Events

A device in D3hot can request wakeup by issuing a **PME message** (PM_PME TLP). The rootport collects PMEs and signals the host (via IRQ or ACPI _GPE_ method).

```
device in D3hot ──(internal wake event)──► assert PME bit + send PME TLP
                                                            │
                                                            ▼
                                           rootport ──► host ACPI/driver
                                                            │
                                                            ▼
                                              SW writes D0 to PMCSR
```

PME from L1.2 requires CLKREQ# assertion first (must wake the link).

### 8. The PCIe PM coverage matrix

A PCIe IP val team owns this cross-product:

```
   ASPM-state    ×    D-state    ×    transition    ×    wake-source    ×    LTR-value
   {L0,L0s,L1,         {D0,D1,D2,      {entry,             {TLP,               {tight,
    L1.1,L1.2,          D3hot,           exit,               CLKREQ#,            loose,
    L2,L3}              D3cold}          abort,              PME,                infinite,
                                         denial}             button}             zero}
```

Plus PVT × link-speed × link-width × cross-IP (with PkgC for D22 anchor).

### 9. Side-band signal timing

CLKREQ# timing is brutal — it's an open-drain GPIO with a pull-up, shared on some boards. Real bugs:

- **CLKREQ# de-assert before refclk gate closes** → refclk stays on, no power save.
- **CLKREQ# assert too late on wake** → spec violates T_CMRT (common-mode restore) and link won't re-train.
- **Multiple devices share CLKREQ#** (board-level) → one device asserts, all wake.
- **CLKREQ# glitch** (board layout issue) → spurious wake every few ms, kills S0ix.

The val team owns **assertions on CLKREQ# timing relative to refclk and Vcm** + **board-level coverage of shared CLKREQ#**.

### 10. L1.2 in a PkgC6 platform — the dependency

```
PkgC6 entry blockers (typical x86 platform):
   ├─ All cores in CC6
   ├─ Display in self-refresh
   ├─ DRAM in self-refresh
   ├─ All PCIe rootports in L1.2     ←  this day
   ├─ All USB ports idle
   ├─ Audio DMA idle
   └─ ME / management engine idle
```

If any one rootport's downstream device fails to enter L1.2 (because its LTR is too tight, or its driver disables ASPM L1.2, or its CLKREQ# is stuck), **PkgC6 never enters.** socwatch reports the blocker; root-cause is in this day's content.

This is **why D22 exists** — the intersection is so critical it gets its own day.

---

## Bring-up Flow — PCIe PM (Validation Lens)

```
Stage A — Link at L0 with all ASPM/L1SS disabled
   ├─ Confirm functional traffic works (baseline)
   └─ No PM yet

Stage B — Enable L0s (per-direction)
   ├─ Both ends advertise L0s in Link Cap
   ├─ Enable in Link Control
   ├─ Idle the link, observe L0s entry in LTSSM trace
   └─ Wake on TLP; check FTS exchange + L0 recovery

Stage C — Enable L1
   ├─ PM_Active_State_Request_L1 negotiated
   ├─ Idle link → L1
   ├─ Wake via electrical-idle exit
   └─ Latency match spec (~10 µs)

Stage D — Enable L1.1
   ├─ Both ends advertise L1.1 in L1SS Cap
   ├─ Enable in L1SS Control
   ├─ Idle → L1 → L1.1 (refclk still on)
   ├─ Wake via CLKREQ# assert
   └─ Coverage on entry/exit

Stage E — Enable L1.2
   ├─ Both ends advertise L1.2
   ├─ Enable in L1SS Control
   ├─ Idle → L1 → L1.2 (refclk OFF, Vcm dropped)
   ├─ Wake via CLKREQ# assert → refclk restore → Vcm restore → Recovery → L0
   ├─ Measure exit latency on scope; compare to T_PowerOn + T_CMRT spec
   └─ Coverage on every entry/exit phase

Stage F — D-state programming
   ├─ Device D0 → D3hot via PMCSR
   ├─ Verify only cfg accessible (MMIO returns errors)
   ├─ D3hot → D0; verify functional restored
   ├─ D3cold path if platform supports (rail control)
   └─ PME from D3hot → wake to D0

Stage G — LTR exercise
   ├─ Device emits LTR with tight value → PkgC blocker
   ├─ Device emits LTR with loose value → PkgC allowed
   └─ Cover LTR-update mid-traffic

Stage H — Cross-IP / PkgC
   ├─ All rootports L1.2 + other blockers cleared → PkgC6 entry
   └─ See Day 22 for the deep dive
```

---

## What's Required

- **PCIe IP RTL** with L1SS Cap (rootport and/or endpoint flavour).
- **Refclk gate model** that responds to CLKREQ#.
- **Common-mode model** that captures Vcm drop + restore timing.
- **Side-band CLKREQ# routing** in TB (open-drain with pull-up).
- **PM cap programming agent** to drive D-state transitions.
- **LTR generator** at endpoint side.
- **PCU model** that consumes LTR and decides PkgC.
- **Vplan rows** for the PM matrix (ASPM × D × transition × wake × LTR).
- **SVAs** on side-band timing (CLKREQ# vs refclk vs Vcm).
- **Post-Si:** PCIe analyser able to capture L1.2 entry/exit.

---

## Tools

| Phase | Tool | Use |
|---|---|---|
| **Pre-Si sim** | VCS / Xcelium + PCIe VIP | Drive ASPM/L1SS handshakes; cover transitions |
| **Pre-Si formal** | JasperGold PCIe app | Prove side-band timing properties |
| **Emulation** | ZeBu / Palladium with PCIe transactor | Real driver + governor exercising PM |
| **Post-Si** | `lspci -vvv` | Read L1SS Cap + Control |
| | `setpci -s BB:DD.F CAP_EXP+10.W=...` | Toggle ASPM/L1SS bits live |
| | `/sys/bus/pci/devices/*/power/control` | Runtime PM enable |
| | `/sys/module/pcie_aspm/parameters/policy` | ASPM policy at kernel level |
| | `socwatch` | PkgC blocker breakdown — names PCIe if rootport not L1.2 |
| | PCIe analyser (Lecroy Summit T3, Teledyne PETrainer) | Capture L0/L1/L1.2 ladder; measure exit latency |
| | Scope on CLKREQ# + refclk + Vcm | Side-band timing ground truth |

---

## Bugs in this Topic

- **L1.2 entry but refclk stays on:** CLKREQ# de-asserted but refclk gate didn't see it (board layout, missed handshake). *Catch: side-band SVA + scope.*
- **L1.2 exit Vcm never restores:** common-mode bias circuit slow, link enters Recovery without Vcm settled → train fails → drops to L0p loop. *Catch: T_CMRT SVA + analyser capture.*
- **CLKREQ# shared between two devices:** one device wakes the other unnecessarily. *Catch: board-level coverage + cross-device wake test.*
- **LTR value stuck at tight:** driver never updates LTR after idle; PkgC blocked forever. *Catch: LTR-value coverage; socwatch root-cause.*
- **D3hot register access works for cfg but MMIO doesn't error properly:** spec says MMIO returns UR; design returned all-1s; SW interpreted as a real value. *Catch: MMIO-in-D3hot test.*
- **PME from D3hot doesn't wake link from L1.2:** PME logic didn't assert CLKREQ# before TLP. *Catch: PME × link-state cross.*
- **ASPM L1 disable in BIOS overridden by OS:** OS re-enabled, but device disabled it again on driver init → mismatched policies, intermittent dropped TLPs. *Catch: end-to-end ASPM-policy coverage.*
- **L1.2 entry races with inbound TLP:** TLP arrived mid-entry; link stuck in undefined sub-state. *Catch: SVA `p_no_entry_with_pending_tlp` + random PM stress.*
- **CLKREQ# glitch from board reflection** (post-Si): false wakes every 8 ms, killing S0ix residency. *Catch: scope + Quarch correlation; board layout fix.*

---

## Interview Explanation (60-90 sec)

> "PCIe PM has two axes: link state and device state. Link states are L0 active, L0s per-direction idle with FTS-based wake, L1 link idle with refclk still on, and the L1 sub-states L1.1 and L1.2 — L1.2 being the deep one where refclk is gated off and common-mode bias is dropped. L1.2 wake is via side-band CLKREQ#: the device asserts CLKREQ#, the platform restarts refclk, Vcm restores, the link goes to Recovery and trains back to L0 in roughly 100-200 µs. Device states D0-D3hot are independent and programmed via the PM cap's PMCSR; D3cold removes function rails entirely. LTR is the device's hint to the platform about how much wake latency it can tolerate — without LTR or with a too-tight LTR, the platform won't enter PkgC. The validation surface is a cross-product of ASPM state × D-state × transition × wake source × LTR value, plus side-band timing assertions on CLKREQ# vs refclk vs Vcm. The bugs that escape into post-Si are usually side-band: shared CLKREQ# waking neighbours, CLKREQ# board glitches causing false wakes, refclk gate timing, or LTR misprogramming that blocks PkgC. socwatch is the oracle that tells you which rootport is blocking PkgC, and that's the entry point to every L1.2 debug."

---

## Architect Drills

*PCIe PM is timing-sensitive and cross-IP. Reason without an answer key.*

**D1. L0s vs L1 — when is L0s the right choice on a modern 16-lane Gen5 link?**
*Constraints to weigh:* FTS overhead per direction per entry/exit, ratio of busy:idle windows, exit latency budget, real-world workloads where bursty traffic at <100 ns gaps lives.

**D2. Why does L1.2 require side-band CLKREQ# rather than electrical-idle exit?**
*Constraints to weigh:* refclk off means receiver has no clock to recover bit-lock from, common-mode collapse means no DC reference for differential pair, latency cost of refclk restart vs FTS.

**D3. Two endpoints share one CLKREQ# pin on a budget board. Reason about all the things that can go wrong.**
*Constraints to weigh:* open-drain wired-OR semantics, wake amplification, asymmetric LTR, debug visibility on which device woke, whether SW can know.

**D4. LTR is "infinite tolerance." Reason about whether this is safe.**
*Constraints to weigh:* worst-case wake latency at L1.2 + PkgC6 + DRAM-SR-exit, what breaks (real-time audio, NIC RX dropping), why platforms default infinite when device doesn't say.

**D5. Reason about D3cold: it removes function rails. What does the device's PM cap say to SW about this?**
*Constraints to weigh:* PMCSR only reads/writes when device powered, cfg access in D3cold requires platform path, ACPI _PS3 method, link state L2 vs L3.

**D6. PME from D3hot must wake the link from L1.2. Reason about the ordering of CLKREQ# assert and PME-TLP send.**
*Constraints to weigh:* TLP can't be sent without a link, link can't come up without refclk, refclk needs CLKREQ#, so PME → CLKREQ# → refclk → train → L0 → PME TLP.

**D7. The val team has 200 hours of emulation budget for PCIe PM. Reason about how to allocate across ASPM directed, L1.2 stress, cross-IP with PkgC, LTR sweeps, and PVT corners.**
*Constraints to weigh:* bug-find-rate per scenario class, what only emul catches (real driver behaviour), what sim catches more cheaply, where post-Si is unforgiving.

---

## Coding A — Systems C: ASPM/L1.2 state machine

Models a PCIe link's PM FSM with CLKREQ#/refclk/Vcm timing. Asserts catch the classic ordering bugs.

```c
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

typedef enum { L0, L0s, L1, L1_1, L1_2, RECOV } pm_t;
static const char *N[] = {"L0","L0s","L1","L1.1","L1.2","RECOV"};

typedef struct {
    pm_t  state;
    bool  refclk_on;
    bool  vcm_on;
    bool  clkreq_low;       /* active-low: low = request refclk */
    int   idle_cyc;
} link_t;

static void enter(link_t *l, pm_t s) {
    printf("  %s -> %s\n", N[l->state], N[s]);
    l->state = s;
    switch (s) {
    case L0:    l->refclk_on = true;  l->vcm_on = true;  break;
    case L0s:   l->refclk_on = true;  l->vcm_on = true;  break;
    case L1:    l->refclk_on = true;  l->vcm_on = true;  break;
    case L1_1:  l->refclk_on = true;  l->vcm_on = true;  break;
    case L1_2:  /* CLKREQ# de-asserted first, then refclk, then Vcm */
                assert(!l->clkreq_low && "L1.2 entry while CLKREQ# still asserted!");
                l->refclk_on = false;
                l->vcm_on    = false;
                break;
    case RECOV: assert(l->refclk_on && l->vcm_on &&
                       "Recovery without refclk + Vcm restored!");
                break;
    }
}

static void wake_via_clkreq(link_t *l) {
    if (l->state != L1_2) return;
    /* Step 1: assert CLKREQ# */
    l->clkreq_low = true;
    /* Step 2: refclk restart */
    l->refclk_on = true;
    /* Step 3: Vcm restore (T_CMRT) */
    l->vcm_on = true;
    /* Step 4: enter Recovery, then L0 */
    enter(l, RECOV);
    enter(l, L0);
    l->clkreq_low = false;
}

int main(void) {
    link_t lk = { L0, true, true, false, 0 };
    printf("Link starts at %s\n", N[lk.state]);

    enter(&lk, L1);
    enter(&lk, L1_1);
    /* Drop CLKREQ# (high) to permit L1.2 */
    lk.clkreq_low = false;
    enter(&lk, L1_2);

    printf("In L1.2: refclk=%d vcm=%d\n", lk.refclk_on, lk.vcm_on);

    printf("Wake via CLKREQ#...\n");
    wake_via_clkreq(&lk);
    printf("Final: %s  refclk=%d vcm=%d\n", N[lk.state], lk.refclk_on, lk.vcm_on);

    /* Bug demo: try L1.2 entry with CLKREQ# stuck low */
    link_t bug = { L1_1, true, true, true /* stuck */, 0 };
    printf("Bug: entering L1.2 with CLKREQ# stuck asserted...\n");
    enter(&bug, L1_2);  /* asserts */
    return 0;
}
```

---

## Bug Anchor

- **Day 18 (LTSSM):** L1.2 exit goes through Recovery (an LTSSM state).
- **Day 19 (enum):** L1SS Cap programmed during enum; mis-program here kills PM.
- **Day 13 (C-states / S0ix):** S0ix requires PCIe L1.2 — every PCIe PM bug is also a PkgC bug.
- **Day 17 (post-Si PM val):** socwatch reports PCIe as PkgC blocker — root-cause sits in this day.
- **Day 22 (PkgC ↔ L1.2):** the deep cross-IP intersection.

## Exit Check

- [ ] Drew the ASPM state ladder (L0 → L0s → L1 → L1.1 → L1.2) with refclk/Vcm/CLKREQ# columns.
- [ ] Stated L0s, L1, L1.1, L1.2 exit latencies in order of magnitude.
- [ ] Walked through L1.2 entry handshake in 7 steps.
- [ ] Walked through L1.2 exit handshake in 6 steps.
- [ ] Stated the difference between ASPM states (link) and D-states (function).
- [ ] Explained what LTR is and why mis-programming kills PkgC.
- [ ] Named the LTR/CLKREQ#/refclk SVAs the val team owns.
- [ ] Named three post-Si PCIe-PM bugs and the telemetry path to each.

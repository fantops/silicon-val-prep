# Day 13 — C-states, PkgC6, S0ix, Cross-IP Idle

## Concept

P-states control the **active** core; C-states control the **idle** core. Deeper C-states save more power but take longer to wake. **C1/C1E** = clock-gated (state preserved, fast wake). **C6** = power-gated (state saved to retention SRAM, voltage collapsed, slow wake ~100 µs+). When *all* cores reach C6 *and* every uncore IP asserts idle, the package can enter **PkgC6** — LLC flushed to DRAM, uncore fabric power-gated, main package rail collapsed. **S0ix / Modern Standby** targets PkgC10 — if PkgC6 is broken, S0ix power is over-budget. The most common PkgC6 blocker in real bring-ups is a **PCIe link not in L1.2**.

## Why this day in the flow

Day 12 was active-power. Day 13 is idle-power and **the cross-IP handshake** that makes deep package idle work. Day 14 then tells two real bug stories at this exact layer (ADR, Core C6 wake-lost). Together Days 12-14 are the power chapter; Day 15 closes it with thermal and counters.

In the Layer map this is **Layer 5 — idle side**.

---

## Deep Study Notes

### 1. The C-state ladder

| C-state | Mechanism | What's preserved | Wake latency | Power saved |
|---|---|---|---|---|
| **C0** | active | — | — | none |
| **C1** | clock gating | all state in flops | ~1 µs | dynamic only |
| **C1E** | clock gate + small V drop | all state | ~2 µs | a bit more |
| **C6** | power gating | state copied to retention SRAM, then rail collapses | ~100 µs+ | dynamic + leakage |

### 2. Clock gating vs power gating

```
Clock gating:    Clock ─┤├─┤├─┤├─┤├─    →    ───────────────  (held low)
                 - flops retain values; no switching, no dynamic power
                 - leakage continues
                 - wake = ungate clock, microseconds

Power gating:    VDD  ═══════════           →    ─ ─ ─ ─ ─ ─    (FET off)
                 - all state lost; must save to retention SRAM first
                 - dynamic AND leakage eliminated
                 - wake = ramp rail, relock PLL, restore state, ~100 µs+
```

The retention SRAM (a few KB) stays powered through C6 — its leakage is tiny compared to a multi-MB cache + execution units.

### 3. Core C6 entry sequence

```
1. OS issues MWAIT with C6 hint (or Linux intel_idle / cpuidle equivalent)
2. Core flushes L1/L2 (data must be evictable to LLC or memory)
3. Core saves architectural state (regs, MSRs) to save/restore SRAM
4. Core asserts C6 request via P-channel handshake to PCU/PMU
5. PCU checks: is anything blocking?  ← cross-IP gate; bug magnet
6. If clear: PCU opens core power-gate FET → core voltage drops to 0
7. Core is now OFF — only retention SRAM + interrupt-wake logic powered
```

### 4. The P-channel handshake

The core *requests*, the package *coordinates*. Other IPs (LLC, snoop filter, fabric, modem on Snapdragon) can refuse if they need that core's clock domain alive. Symptoms of a broken P-channel:

- C6 entry never completes (request asserted, ack never returned).
- Core enters C6 but is woken almost immediately (wake mask wrong).
- Spurious resets attributed to "core hang" because the core was actually power-gated mid-transaction.

### 5. Package C-states

When **all** cores reach C6, the package can deepen further:

| State | Property |
|---|---|
| **PC2** | all cores idle, shared resources still on |
| **PC6** | LLC flushed to DRAM, LLC power-gated; uncore fabric gated; main package Vcc collapsed |
| **PC10** | almost everything off except wake/RTC; target of S0ix / Modern Standby |

### 6. PkgC6 entry — the cross-IP dance

```
1. OS targets C6 per-core (idle driver)
2. Each core flushes L1/L2, saves arch state, asserts C6 request
3. PCU waits for ALL cores in C6 (one active core blocks the package)
4. PCU polls every uncore IP for an idle assertion:
     LLC            : no pending transactions
     Memory ctrl    : no pending DRAM requests
     PCIe ctrl      : link must be in L1.2 (PHY off, common-mode off)
     Display ctrl   : no scanout / panel self-refresh active
     USB / NIC etc. : no pending traffic
     Interrupt ctrl : no pending interrupts
   → ANY IP not asserting idle blocks PkgC6.
5. LLC contents flushed to DRAM, LLC power-gated
6. Uncore fabric power-gated in defined order
7. PCU collapses main package Vcc rail via PMU
8. Only PCU + RTC + wake logic remain on always-on rail
```

### 7. PkgC6 exit — and why latency dominates

```
1. Wake event (interrupt, timer, PCIe activity, USB SOF, ...)
2. Always-on logic signals PCU
3. PMU ramps Vcc back, waits for stability
4. Uncore powers up in reverse entry order; PLLs relock
5. LLC reloads from DRAM ← dominant cost (~hundreds of µs for multi-MB LLC)
6. Cores ungate, restore arch state from retention SRAM
7. OS scheduler resumes
```

That LLC reload is the reason aggressive PkgC6 hurts latency-sensitive workloads. Trade-off: more residency = more battery, less responsiveness.

### 8. Why PCIe blocks PkgC6 (the #1 bug)

- PCIe controller asserts idle only when its link is in **L1.2** — PHY powered down, common-mode voltage off.
- L1.2 requires:
  1. **ASPM enabled on both root port and endpoint** (BIOS/UEFI + driver).
  2. **L1 PM Substates capability advertised** by the endpoint.
  3. **CLKREQ#** properly routed on the board.
  4. **LTR** (Latency Tolerance Reporting) values that allow the platform to enter L1.2.
- If any of those is wrong, the link sits in L0/L0s/L1, controller never asserts idle, PCU never sees the green light, **PkgC6 never enters**.

Debug:

```
1. lspci -vvv : check ASPM, L1 PM Substates capability, current LTSSM state
2. Read PCU "blocking reason" MSR  → which IP refused?
3. Verify endpoint advertises L1.2
4. Check CLKREQ# pin routing on the board (HW)
5. Check LTR settings — too tight a value pins the link in L1.0
```

### 9. ADR — Asynchronous DRAM Refresh

ADR is the **emergency cache-flush + DRAM-self-refresh** flow on power loss. The sequence (full bug story comes Day 14):

```
1. Power-supply loss detected (PSON#, UPS warning, etc.)
2. PCU asserts ADR
3. Memory controller drains write-pending queue (WPQ) → DRAM
4. CPU caches flushed → DRAM
5. DRAM enters self-refresh on hold-up power (capacitors)
6. On power restore, kernel + memory controller re-attach DRAM contents
```

Crucial in **persistent-memory** systems and storage controllers (SanDisk power-loss-protection capacitor banks). The Day 14 ADR bug is about opcode ordering between PCU and the memory controller.

### 10. S0ix / Modern Standby

The OS-visible **connected standby** mode. Under S0ix, the platform targets PkgC10:

- Wake on touch, timer, network within ms.
- Power budget < 100 mW for many designs.
- Builds on PkgC6: if PkgC6 is broken, S0ix is over-budget.
- Snapdragon analogue: the deep-sleep state RPMh aggregates into via PSCI (Day 9).

### 11. Common PkgC6 bugs

| Bug | Symptom | Root cause |
|---|---|---|
| IP not asserting idle | PkgC6 residency = 0 | PM handshake misconfigured |
| PCIe blocking | Low residency at full system idle | ASPM L1.2 not enabled on EP |
| Wrong uncore sequence | Hang or data corruption on entry | Power-gate ordering violation |
| Spurious wakeup | PkgC6 entered then immediately exited | Wake mask wrong; LTR too tight |
| Core wake lost | Core never resumes after wake; OS treats as hang | Core C6 wake-vector overwritten by reset window (Day 14 anchor) |
| ADR opcode race | Data loss on power failure | PCU asserts WPQ_FLUSH before MC IO_QUIESCE_ACK (Day 14 anchor) |

### 12. Snapdragon analogue (Day 9 link)

- OS issues PSCI request (`PSCI_CPU_OFF`, `PSCI_SYSTEM_SUSPEND`).
- RPMh aggregates votes from all subsystems.
- TCS sequences fire on idle entry/exit.
- PMIC over SPMI drops the right rails.
- Same debug ladder: did the OS request the right state? did RPMh aggregate? did TCS fire? did rail drop?

### 13. Negative-path debug — "OS requested C3, residency = 0, why?"

*The Intel-Bangalore probe: not "how does C3 work" but "it didn't happen — walk me through the why." Run this ladder top-to-bottom; never skip.*

```
1. Did the OS actually request it?
   — cpuidle governor stats, MWAIT hint matches the C-state, idle driver loaded.
   — Common miss: BIOS hides the deeper rung; OS only sees up to C1E.
2. Did the core PMA accept the request?
   — Local conditions: pending interrupts, IPI in flight, fixed timer too close (TSC deadline).
   — LAPIC/APIC unmasked-but-pending blocks entry.
3. Did the platform/PCU veto?  ((cross-IP)) — the long list:
   — Peripheral idle not asserted: PCIe link not in L1.2, USB not in U3, SATA not in slumber, display not in PSR.
   — LTR (Latency Tolerance Reporting) too tight: an EP advertised "I need <10µs wake" so PCU caps depth at C1.
   — Broadcast block: a higher-priority IP (ME / SMU / EC) is mid-transaction; PCU-wide gate held.
   — VR settling timer still running from the last DVFS transition.
   — Thermal flag pending; PCU refuses deep entry until DTS clears.
4. Did the OS-side governor demote it?
   — "C-state demotion" MSR: PCU told the core "you asked C6, I gave you C1" because predicted residency < break-even.
   — Visible as: requested-count high, achieved-count low, demoted-count matches the gap.
5. Did it enter but immediately exit?
   — Wake mask too permissive: USB SOF, EC heartbeat, ME polling, broken LTR-no-snoop interaction.
   — Residency ≈ 0 with high entry/exit count is the signature.
```

**Reflex one-liner:** *"Requested vs achieved vs demoted vs entered-then-exited — four different counters, four different root causes. Always pull all four before guessing."*

**Tools to name:** PCU blocking-reason MSR, `turbostat`, `powertop`, vendor PM tracer (Intel SoCWatch / Qualcomm Trepn), PCIe LTSSM trace, `lspci -vvv` for ASPM/L1SS/LTR caps.

### 14. Design a core C-state from scratch — 9-step scaffold

*Amazon Lab126 26 May probe: "design a core C-state for a PC from scratch." Walk the scaffold in order; each step is a question the spec / architect has to answer before RTL exists.*

1. **Entry triggers** — what causes the OS / scheduler to request this state? (`MWAIT` hint from cpuidle governor, predicted-residency calculation, OS tickless idle). Define the request interface (hint encoding, MSR, PSCI call).
2. **Eligibility checks** — local conditions before the core PMA accepts: no pending interrupts, no IPI in flight, TSC deadline far enough out, LAPIC unmasked-pending clear. List every veto source.
3. **Save set** — what architectural state must survive: GPRs, FP/vector regs, system regs, LAPIC state, perf counters, debug regs. Where it lives: retention SRAM (always-on rail) vs stack-in-DRAM vs banked regs. Size budget drives retention-SRAM area.
4. **Power/clock action** — clock gate (C1/C1E) vs power gate (C6+). For power gate: order of clock-stop → state-save → isolation cells assert → power-switch off → rail collapse. Spec the sequence FSM.
5. **Wake sources** — which interrupts/events can wake: LAPIC, IPI, timer, NMI, debug, thermal, EC heartbeat. Wake mask register defines the set. Trade depth vs latency.
6. **Exit sequence** — wake event → rail ramp → isolation deassert → reset deassert → clock ungate → state-restore → fetch resumes at saved PC. Each substep has a settling timer.
7. **Demotion + governor feedback** — `demoted_count` MSR; PCU rejects request if predicted residency < break-even (entry+exit energy / leakage-saved-per-µs). Governor adjusts next prediction. *Why:* false-wake penalty. *How:* HW-side promotion-block + OS-side governor weighting + LTR / peripheral-blocker interaction.
8. **Measurement** — entry count, exit count, residency, demoted count, wake-source histogram. Tools: `turbostat`, SoCWatch, PCU blocking-reason MSR. Without counters you cannot debug.
9. **Validation matrix** — coverage axes: state × wake-source × workload × thermal × peripheral-blocker. Negative paths: wake during save, fault during restore, rail droop, retention-SRAM ECC error. Pre-Si vs post-Si split.

**Reflex one-liner:** *"Entry → eligibility → save → power/clock → wake → exit → demotion → measurement → validation. Nine steps. Same scaffold for any C-state, any vendor."*

*Cross-link: ladder + ICG internals = Sec 1-2 above; PkgC6 cross-IP dance = Sec 6; negative-path debug = Sec 13; thermal + counter side = [Day 15](day15_thermal_power_perf_counters.md); post-Si lab measurement = [Day 17](day17_post_si_pm_validation.md).*

---

## Bugs in this Topic

- **PkgC6 never enters even at full system idle:** PCIe endpoint missing L1.2 in L1 PM Substates capability; or ASPM disabled in BIOS; or LTR value pins the link awake. Read PCU blocking-reason MSR, then `lspci -vvv` on the suspect EP.
- **PkgC6 entered then immediately exited:** wake mask too permissive — a periodic timer (USB SOF, ME heartbeat, embedded controller) keeps tripping wake. Trace wake source via PCU logs.
- **Core C6 wake-vector lost (Day 14 anchor):** firmware wrote a transient wake-vector during the same window a CFC reset routine cleared it; the core power-ungated but jumped to garbage; system appears hung. Fix: reorder wake-vector write after CFC reset clear, or stash in retention SRAM.
- **ADR data loss on power failure (Day 14 anchor):** PCU asserted `WPQ_FLUSH` before the memory controller acknowledged `IO_QUIESCE`; in-flight writes never reached DRAM before self-refresh. Fix: opcode-ordering — wait for `IO_QUIESCE_ACK` before `WPQ_FLUSH`.
- **High S0ix power:** PkgC6 residency low; root-cause via the same PCU blocking-reason MSR ladder.
- **Cores stuck in C0 on a quiet system:** scheduler tickling them with timers; check `tickless` config and per-CPU timer migration.
- **Spurious thermal alerts only at deep idle:** DTS reading ungated post-wake before stable; treat as known transient.

---

## Interview Explanation (60-90 sec)

> "C-states control the idle core. C1 and C1E are clock-gated — state preserved, microsecond wake. C6 is power-gated — architectural state saved to retention SRAM, the core rail collapses, leakage and dynamic both go to zero, but wake is around a hundred microseconds. PkgC6 happens when all cores are in C6 and every uncore IP — LLC, memory controller, PCIe controller, display, interrupt controller — asserts idle. The PCU then flushes LLC to DRAM, gates the uncore fabric, and collapses the main package rail. The most common blocker in real bring-ups is PCIe: the controller only asserts idle when the link is in L1.2 specifically, which needs ASPM and the L1 PM Substates capability on both root port and endpoint, plus correct CLKREQ# routing. If any of those is missing, PkgC6 never enters. Exit is dominated by the LLC reload from DRAM, which is why aggressive PkgC6 hurts latency-sensitive workloads. S0ix builds on this — if PkgC6 is broken, Modern Standby power is over budget. On Snapdragon the same loop is PSCI requests aggregated by RPMh and programmed into the PMIC over SPMI."

---

## Architect Drills

*Idle is an architecture problem disguised as a power one. Reason from constraints — entry cost, exit cost, residency, break-even — not from the C-state names.*

**D1. Why a discrete ladder (C0/C1/C6/C10) and not a continuous knob — "sleep deeper the longer I'm idle"?**
*Constraints to weigh:* every state needs a coordinated entry/exit sequence (rails, clocks, retention save, cache flush, snoop quiesce); each sequence is FW + HW co-designed and validated separately. Continuous would mean infinite validation matrix. Why are the chosen rungs spaced where they are (what makes the gap from C1 to C6 worth the validation cost)? What would a 5th rung between C6 and C10 actually buy?

**D2. Why does PkgC6 require *every* uncore IP to be idle? Why can't the package power-gate cores independently and leave uncore running?**
*Constraints to weigh:* shared rails (the same VR feeds cores and uncore on most designs), snoop traffic (an active uncore agent can issue a coherent probe into a power-gated core), the PCU as a single arbiter with a single global view, retention-state consistency. Where does the assumption break — is there a SoC topology where independent core gating is genuinely possible?

**D3. Retention SRAM stays powered in C6. Why retention at all — why not save core state to DRAM and skip the dedicated array?**
*Constraints to weigh:* exit latency budget (DRAM access ~100 ns + path setup vs SRAM ~1 ns), DRAM controller itself may be in self-refresh and need wake first, energy of one SRAM array vs energy of waking the memory subsystem to read 4 KB, and the chicken-and-egg of "who restores the path that restores the cores." When does the trade flip — what would force a future SoC to drop retention SRAM?

**D4. S0ix depends on PCIe L1.2. What architectural change would let a platform reach S0ix-class power without L1.2?**
*Constraints to weigh:* common-mode current on the link (~mW per lane) is the dominant non-CPU residual; alternatives are full link-down + re-train (latency cost), side-band wake signalling, or moving the device to a different rail entirely. CXL idle states are a real-world rerun of this question — what did the spec authors land on, and why?

**D5. Derive the break-even residency for entering a deeper C-state. When is going deeper a *net loss*?**
*Constraints to weigh:* entry energy $E_{\text{in}}$, exit energy $E_{\text{out}}$, savings rate $\Delta P$ while in the state, expected residency $T_r$. Inequality: enter only if $T_r > (E_{\text{in}} + E_{\text{out}}) / \Delta P$. How does the OS/PCU estimate $T_r$ in practice, and what happens to the estimator under bursty wake patterns (the "ping-pong" failure mode)?

**D6. If wake latency could be made arbitrarily fast (~ns), what changes about the C-state ladder?**
*Constraints to weigh:* the ladder exists because exit cost is high enough to justify staying shallow for short idles. Collapse exit cost → the ladder collapses to two states (on/deep). What's then the bottleneck — is it OS scheduler granularity, interrupt-controller wake path, or the physics of restoring rail voltage? Pick one and argue why it is now load-bearing.

**D7. PkgC6 residency = 0 on a quiet system. Reason about where you look *before* reading any blocking-reason MSR — what does the system shape predict?**
*Constraints to weigh:* what *can* keep an IP busy on a "quiet" system (tick timers, USB SOFs, PCIe LTR, audio DMA at 10 ms cadence, GPU compositor, SMI storms). Rank these by base rate. Does "residency = 0" vs "residency = 30%" point to different culprit classes? The blocking-reason MSR is the *confirmation*, not the *hypothesis*.

---

## Coding A — Systems C: C-state request + PkgC6 PCU gate model

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum { C0=0, C1=1, C1E=2, C6=6 } cstate_t;

#define NCORES 4
typedef struct {
    cstate_t cs;
    bool     work_pending;
} core_t;

typedef struct {
    bool llc_idle;
    bool mc_idle;
    bool pcie_in_l1_2;
    bool display_idle;
    bool intc_idle;
} uncore_t;

static core_t cores[NCORES];
static uncore_t un = { true, true, false, true, true };  /* PCIe NOT in L1.2 by default */

static const char *blocker(void) {
    if (!un.llc_idle)     return "LLC pending";
    if (!un.mc_idle)      return "Memory controller busy";
    if (!un.pcie_in_l1_2) return "PCIe link not in L1.2";
    if (!un.display_idle) return "Display scanout active";
    if (!un.intc_idle)    return "Pending interrupt";
    return NULL;
}

static bool request_core_cs(int id, cstate_t target) {
    if (cores[id].work_pending && target > C1) {
        printf("Core %d: refused C%d (work pending)\n", id, target);
        return false;
    }
    cores[id].cs = target;
    printf("Core %d -> C%d\n", id, target);
    return true;
}

static bool try_pkg_c6(void) {
    for (int i = 0; i < NCORES; i++) {
        if (cores[i].cs != C6) {
            printf("PkgC6 BLOCKED: core %d in C%d\n", i, cores[i].cs);
            return false;
        }
    }
    const char *b = blocker();
    if (b) { printf("PkgC6 BLOCKED: %s\n", b); return false; }
    printf("PkgC6 ENTERED: LLC flushed, uncore gated, package rail collapsed\n");
    return true;
}

int main(void) {
    for (int i = 0; i < NCORES; i++) request_core_cs(i, C6);
    try_pkg_c6();              /* fails: PCIe not in L1.2 */
    un.pcie_in_l1_2 = true;
    try_pkg_c6();              /* succeeds */
    return 0;
}
```

---

## Interview Reasoning Pack (system-level thinking)

### Q: Static vs dynamic power — what's the difference, and why do I care?

**Two independent loss mechanisms that the same transistor pays simultaneously.**

- **Dynamic power** is the cost of *switching* a node. Every time a flip-flop or wire toggles `0→1`, it charges the load capacitance to $V_{dd}$, then dumps the charge to ground on `1→0`. Energy per toggle = $\frac{1}{2} C V^2$. Power = energy × switching rate:

$$P_{dyn} = \alpha \cdot C \cdot V^2 \cdot F$$

  where $\alpha$ is activity factor (fraction of clock edges that actually toggle the node).

- **Static (leakage) power** is the cost of *just being on*. Even a "stopped" transistor leaks: subthreshold current through the channel, gate-oxide tunneling, junction leakage. Roughly:

$$P_{leak} \approx V_{dd} \cdot I_{leak}(V, T)$$

  $I_{leak}$ rises **exponentially with temperature** and roughly linearly with $V_{dd}$. *This is the runaway thermal feedback loop: hot → leaky → hotter.*

**Why this distinction drives every architectural choice:**

| Lever | Hits dynamic? | Hits leakage? |
|---|---|---|
| Lower $V_{dd}$ | $V^2$ — huge | Linear-ish — modest |
| Lower $F$ | Linear | None directly |
| **Clock-gate** | Kills it (no toggles) | Untouched |
| **Power-gate** (rail off) | Kills it | **Kills it** |
| Cool the die | None | Big (exponential) |

**One-liner:** *"Dynamic is $CV^2F$ — the cost of toggling. Leakage is $V \cdot I_{leak}(T)$ — the cost of being powered, exponential in temperature. Clock gating kills dynamic only; power gating kills both. Every C-state and DVFS decision is choosing which lever to pull and paying the wake-up cost."*

### Q: Why is clock gating needed? Fine-grain vs coarse-grain? What's an ICG cell?

**The problem clock gating solves:** a clock tree is the largest single contributor to dynamic power on a modern SoC — often **30-40%** of total dynamic power. Every flip-flop sees the clock toggle every cycle even when its data doesn't change, charging/discharging the clock-pin capacitance for nothing.

**Clock gating = "if this block has no work, don't toggle its clock."** Insert an AND-style gate so the clock only propagates when an `enable` signal is high.

### Q: Walk me through *how* clock gating is actually built — RTL to silicon.

**Five layers, each answering a different "how" question.**

**Layer 1 — RTL: where the intent is expressed.**
The designer writes a flop with an enable condition:

```verilog
always_ff @(posedge clk) begin
  if (en) data_q <= data_d;   // hold value when en=0
end
```

A naive synthesis would build this as a flop with a feedback mux: when `en=0`, mux selects the old value back into the D input. The flop *still toggles its clock pin every cycle* — burning dynamic power for nothing.

**Layer 2 — Synthesis: the tool replaces the mux with an ICG cell.**
With clock-gating enabled (e.g. Synopsys DC `set_clock_gating_style`, or `insert_clock_gating`), the synthesis tool sees the enable pattern and rewrites it as:

```
       data_d ─────────────► D ┐
                                │ flop
       clk_gated ──────────► CK ┘   (no feedback mux)
                  ▲
                  │
       clk ──┐    │
             ▼    │
         [ ICG cell ]
             ▲
             │
       en ───┘
```

The flop's clock pin now only toggles when `en=1`. The mux is gone; the gating moved upstream to the clock.

**Layer 3 — The ICG cell internals (the cell that actually does the gating).**
A plain `clk & en` AND gate **glitches**: if `en` rises while `clk` is high, you get a runt pulse that the downstream flop sees as a clock edge → metastability or wrong data. The ICG fixes this with a **level-sensitive latch** on the enable, transparent only when clock is low:

```
              ┌─────────────────────┐
              │  Latch (D-latch)    │
   en  ──────►│ D                Q ├──────┐
              │                     │      │
   clk ──┬───►│ G (gate, active-low)│      ▼
         │    └─────────────────────┘   ┌─────┐
         │                              │ AND │── clk_gated
         └─────────────────────────────►│     │
                                        └─────┘
```

Behaviour:
- While `clk = 0`, the latch is **transparent** — it samples `en`.
- When `clk` rises, the latch **freezes** the sampled value; the AND outputs `clk_gated = clk & en_latched`.
- If `en` changes mid-cycle (while `clk = 1`), the latch ignores it. **No glitch.**

This cell is in every standard-cell library (Synopsys: `CKLNQD*`, ARM Artisan: `CKGATEH`, TSMC: `CKLHQ*`). Designers don't instantiate it by hand; the tool inserts it.

**Layer 4 — Hierarchical gating: where in the clock tree the ICG sits.**

```
        PLL
         │
         ▼
   ┌─────────┐    Root / global gate (block-level enable)
   │   ICG   │◄── from CPM / power-mgmt FSM
   └────┬────┘
        │
   ┌────┴────┬────────┬─────────┐
   ▼         ▼        ▼         ▼
  ICG       ICG      ICG       ICG    Regional gates (per FU: FPU, decode, LSU)
   │         │        │         │
  flop      flop     flop      flop   Leaf gates (per flop bank, ~8-32 flops)
  bank      bank     bank      bank
```

Three tiers, three sources of the enable:
- **Leaf (fine-grain):** enable comes from local logic — "this lane's instruction is valid this cycle." Inserted by the tool. Saves **~10-20%** dynamic power, ~free in design effort.
- **Regional (coarse-grain):** enable comes from a unit-idle detector — "FPU has had no μop for 8 cycles." Hand-architected. Saves another **~10-15%**, requires a wake latency of 1-few cycles when work returns.
- **Root (block-level):** enable comes from the **clock/power management FSM** — driven by software entering C1 (MWAIT) or by the PCU. Saves the rest, but wake costs nanoseconds-to-microseconds because the whole clock tree has to refill and the receiving flops need a clean first edge.

**Layer 5 — Verification (what makes this *hard*).**

This is where pre-silicon validation lives — the layer that pays your salary:

1. **Logical equivalence (LEC):** prove the gated netlist is functionally identical to the un-gated RTL — that the enable was correctly derived and no flop "lost" a write. Synopsys Formality / Cadence Conformal.
2. **Glitch checks:** confirm every `clk_gated` net is sourced by an ICG, never a raw AND. Lint rules in SpyGlass-CDC.
3. **X-propagation:** at reset, the latch's `Q` is X. If the AND lets X propagate as the clock, you wedge the chip. ICG cells include reset/test logic to force `clk_gated = 0` during reset and `clk_gated = clk` during scan test (controlled by a `test_en` pin tied to DFT).
4. **Coverage:** RTL assertions like *"if `en = 0` for 3 cycles, no flop in this bank changed"* — confirms the gating actually fires. PPA reports show **observed gating efficiency** (% of cycles each ICG was off) — typical target 40-70% on idle workloads.
5. **Power-aware sim (UPF):** with the power-intent file, RTL sim models clock gating + power gating together. Catches the bug where you gate the clock but a downstream domain is in retention and isn't ready — read returns X.

**Granularity tradeoff — the design knob:**

| Style | Granularity | Power saved | Implementation cost | Wake latency |
|---|---|---|---|---|
| **Fine-grain (leaf)** | Per flop bank (8-32 flops) | ~10-20% dyn | Tool-inserted, automatic; ICG area ~3-5% | 0 cycles (next edge) |
| **Coarse-grain (regional)** | Per functional unit | ~10-15% dyn | Hand-architected enable; idle detector | 1-few cycles |
| **Block / root** | Whole IP / clock domain | The rest | Coordinated with reset, CDC, software FSM | 10s of ns – μs |

Modern SoCs do **all three layers simultaneously** — leaf for everything the tool can find, regional for big units (FPU, SIMD, decode lanes), root for whole IPs entering deep idle.

**Clock gating vs power gating in one sentence:** *Clock gating freezes the clock — kills dynamic, leakage stays. Power gating drops the rail — kills both, but you lose state and pay a wake-up of microseconds.*

**One-liner for the interview:** *"Clock gating is RTL `if (en)` patterns being rewritten by synthesis into ICG cells — a latch-plus-AND that gates the clock pin without glitching. Three tiers: leaf gates the tool inserts automatically, regional gates the architect hand-codes per unit, and root gates the power FSM drives for whole IPs. Together they kill the 30-40% of dynamic power that the clock tree would otherwise burn on idle flops. Validation is LEC, glitch checks, X-prop at reset, and gating-efficiency coverage."*

### Q: Why multiple C-states instead of one idle state?

**Frame it as "every wake-up has a cost; pick the deepest state whose savings out-earn the cost."**

The fundamental tradeoff per state:

$$\text{Net energy saved} = (P_{active} - P_{state}) \cdot t_{residency} \;-\; E_{entry+exit}$$

For each state there's a **break-even residency** $t_{BE} = E_{entry+exit} / (P_{active} - P_{state})$. Below $t_{BE}$, you *lose* energy by entering. Above it, you win.

**Why a single deep state is wrong:**
- If the only option is "all the way off" (e.g., cores power-gated, caches flushed, PLLs shut), entry/exit cost might be **100 μs** of wasted energy. For a 50 μs idle window, you're worse off than just spinning.
- If the only option is "shallow" (clock-gated only), you leave huge leakage savings on the table during long idles.

**The ladder gives the OS a menu, indexed by predicted idle time:**

| State | Mechanism | Entry/exit | Saves | Use when idle ≈ |
|---|---|---|---|---|
| **C1** | Clock-gate core pipeline | ns | Dynamic | μs |
| **C1E** | C1 + voltage drop | ns-μs | Dynamic + some leakage | tens of μs |
| **C6** | Power-gate core, save state to SRAM | μs-tens | Dynamic + leakage on core | ms |
| **PkgC6/S0ix** | Power-gate uncore + LLC flush + DRAM self-refresh + PCIe L1.2 | tens-hundreds μs | Almost everything | tens of ms+ |

**The menuconfig governor** in Linux (and equivalent in Windows ppm) **predicts** the next idle duration from history and timer expiries, looks up the deepest state with $t_{BE} <$ prediction, and enters it. This is why **PkgC6 residency** is a real KPI — it measures whether the prediction model and dependency chain (PCIe in L1.2, DRAM in self-refresh, fabric quiesced) is actually letting you reach the deep state.

**One-liner:** *"You need a ladder because every idle state pays an entry/exit energy cost, and only deeper states save more leakage. A single state is wrong for short idles (you waste more entering than you save) and wrong for long idles (you leak more than you need to). The OS picks the deepest state whose break-even residency fits the predicted idle window. Multiple C-states = multiple price points on the energy/latency curve."*

---

## Depth Drill — Pointer Triggers

*Recall only. If a trigger goes blank, that's the layer to re-read.*

- **C-state ladder** — *what:* C0 active, C1–C10 progressively deeper idle / *why:* break-even residency = entry/exit cost ÷ leakage saved per cycle / *how:* OS picks deepest state whose predicted idle exceeds break-even / *connects:* wake latency vs power saved, idle governor (menu, teo)
- **clock gating** — *what:* stop the clock to idle flops / *why:* kills $αCV^2F$ when $α$ = 0 / *how:* ICG cell = latch + AND, glitch-free / *connects:* leaf (auto) / regional (architect) / root (FSM) tiers; LEC + glitch + X-prop verification
- **power gating** — *what:* cut Vdd to a block / *why:* kills leakage too, not just dynamic / *how:* header/footer FETs, retention flops save state / *connects:* state save/restore latency, in-rush current at wake
- **static vs dynamic power** — *what:* $V·I_{leak}(T)$ vs $αCV^2F$ / *why:* different levers, exponential vs cubic / *how:* CG kills dynamic only, PG kills both / *connects:* hot chip → exponential leakage growth, why thermal couples to leakage
- **S0ix / Pkg C-states** — *what:* platform-wide low-power with OS still running / *why:* modern standby, instant resume / *how:* all uncore quiesced, devices in D3, displays off / *connects:* PMC firmware orchestrates, broken D3 on any device blocks S0ix

---

## Bug Anchor

- **Day 10 (PCIe LTSSM L1.2):** the dependency that makes or breaks PkgC6.
- **Day 14 (ADR + Core C6 bugs):** two real firmware stories at this exact layer.
- **Day 9 (Snapdragon RPMh):** the QC analogue of the PCU + uncore handshake.
- **Day 15 (Thermal + perf counters):** PkgC6 residency is a counter you score against budget.

## Exit Check

- [ ] Stated clock-gating vs power-gating (saves what, wakes how fast).
- [ ] Walked Core C6 entry in 6 steps from MWAIT to power gate.
- [ ] Walked PkgC6 entry across the cross-IP gate.
- [ ] Stated why PCIe must be in L1.2 specifically.
- [ ] Stated why LLC reload dominates PkgC6 exit.
- [ ] Mapped S0ix to PkgC6 dependency.
- [ ] Mapped Snapdragon PSCI/RPMh equivalent.

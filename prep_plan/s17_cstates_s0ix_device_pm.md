# S17 - C-states, S0ix, device, and PCIe PM

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Read:** [D27 C-states/PkgC/S0ix](archive/day27_cstates_pkgc_s0ix.md), [D35 PCIe PM](archive/day35_pcie_power_management.md), [D37 PCIe×PM](archive/day37_pcie_pm_intersection.md).
- **Must answer:**
  - Core C1 → C6: clock stop → cache flush (S8/S9) → state save → power gate (S16).
  - Package C-states need all cores idle and all devices agreeing. Devices vote with **D-states, L1/L1.2 (CLKREQ#), and LTR**. Pending DMA or a short LTR blocks deep package C-states. Retell **"PkgC6 blocked by PCIe L1.2"**.
  - S0ix: always-on wake domains (S5), DRAM self-refresh (S8), wake sources.
  - A **"PkgC6 won't enter" ladder:** residency counters → blocker status → device D-states/LTR → link states.
- **Work link:** is DSA a potential package C-state blocker while it has queued or in-flight work? What should happen when it's idle?
- **Story re-tell (story 3):** **PkgC6 blocked by PCIe L1.2** (full vote chain). Then revisit stories 1 and 2 as wake-path failures. Finish by saying P vs C vs D states in 30 s.
- **Handoff →** How do you debug the whole chain on real silicon?

## Study notes

### Concept - [day27_cstates_pkgc_s0ix.md](archive/day27_cstates_pkgc_s0ix.md)

P-states control the **active** core; C-states control the **idle** core. Deeper C-states save more power but take longer to wake. **C1/C1E** = clock-gated (state preserved, fast wake). **C6** = power-gated (state saved to retention SRAM, voltage collapsed, slow wake ~100 µs+). When *all* cores reach C6 *and* every uncore IP asserts idle, the package can enter **PkgC6** — LLC flushed to DRAM, uncore fabric power-gated, main package rail collapsed. **S0ix / Modern Standby** targets PkgC10 — if PkgC6 is broken, S0ix power is over-budget. The most common PkgC6 blocker in real bring-ups is a **PCIe link not in L1.2**.

### Deep Study Notes - [day27_cstates_pkgc_s0ix.md](archive/day27_cstates_pkgc_s0ix.md)

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

ADR is the **emergency cache-flush + DRAM-self-refresh** flow on power loss. The sequence (full bug story comes Day 28):

```
1. Power-supply loss detected (PSON#, UPS warning, etc.)
2. PCU asserts ADR
3. Memory controller drains write-pending queue (WPQ) → DRAM
4. CPU caches flushed → DRAM
5. DRAM enters self-refresh on hold-up power (capacitors)
6. On power restore, kernel + memory controller re-attach DRAM contents
```

Crucial in **persistent-memory** systems and storage controllers (SanDisk power-loss-protection capacitor banks). The Day 28 ADR bug is about opcode ordering between PCU and the memory controller.

### 10. S0ix / Modern Standby

The OS-visible **connected standby** mode. Under S0ix, the platform targets PkgC10:

- Wake on touch, timer, network within ms.
- Power budget < 100 mW for many designs.
- Builds on PkgC6: if PkgC6 is broken, S0ix is over-budget.
- Snapdragon analogue: the deep-sleep state RPMh aggregates into via PSCI (Day 21).

### 11. Common PkgC6 bugs

| Bug | Symptom | Root cause |
|---|---|---|
| IP not asserting idle | PkgC6 residency = 0 | PM handshake misconfigured |
| PCIe blocking | Low residency at full system idle | ASPM L1.2 not enabled on EP |
| Wrong uncore sequence | Hang or data corruption on entry | Power-gate ordering violation |
| Spurious wakeup | PkgC6 entered then immediately exited | Wake mask wrong; LTR too tight |
| Core wake lost | Core never resumes after wake; OS treats as hang | Core C6 wake-vector overwritten by reset window (Day 28 anchor) |
| ADR opcode race | Data loss on power failure | PCU asserts WPQ_FLUSH before MC IO_QUIESCE_ACK (Day 28 anchor) |

### 12. Snapdragon analogue (Day 21 link)

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

*Cross-link: ladder + ICG internals = Sec 1-2 above; PkgC6 cross-IP dance = Sec 6; negative-path debug = Sec 13; thermal + counter side = [Day 29](archive/day29_thermal_power_perf_counters.md); post-Si lab measurement = [Day 31](archive/day31_post_si_pm_validation.md).*

---

### Bugs in this Topic - [day27_cstates_pkgc_s0ix.md](archive/day27_cstates_pkgc_s0ix.md)

- **PkgC6 never enters even at full system idle:** PCIe endpoint missing L1.2 in L1 PM Substates capability; or ASPM disabled in BIOS; or LTR value pins the link awake. Read PCU blocking-reason MSR, then `lspci -vvv` on the suspect EP.
- **PkgC6 entered then immediately exited:** wake mask too permissive — a periodic timer (USB SOF, ME heartbeat, embedded controller) keeps tripping wake. Trace wake source via PCU logs.
- **Core C6 wake-vector lost (Day 28 anchor):** firmware wrote a transient wake-vector during the same window a CFC reset routine cleared it; the core power-ungated but jumped to garbage; system appears hung. Fix: reorder wake-vector write after CFC reset clear, or stash in retention SRAM.
- **ADR data loss on power failure (Day 28 anchor):** PCU asserted `WPQ_FLUSH` before the memory controller acknowledged `IO_QUIESCE`; in-flight writes never reached DRAM before self-refresh. Fix: opcode-ordering — wait for `IO_QUIESCE_ACK` before `WPQ_FLUSH`.
- **High S0ix power:** PkgC6 residency low; root-cause via the same PCU blocking-reason MSR ladder.
- **Cores stuck in C0 on a quiet system:** scheduler tickling them with timers; check `tickless` config and per-CPU timer migration.
- **Spurious thermal alerts only at deep idle:** DTS reading ungated post-wake before stable; treat as known transient.

---

### Interview Explanation (60-90 sec) - [day27_cstates_pkgc_s0ix.md](archive/day27_cstates_pkgc_s0ix.md)

> "C-states control the idle core. C1 and C1E are clock-gated — state preserved, microsecond wake. C6 is power-gated — architectural state saved to retention SRAM, the core rail collapses, leakage and dynamic both go to zero, but wake is around a hundred microseconds. PkgC6 happens when all cores are in C6 and every uncore IP — LLC, memory controller, PCIe controller, display, interrupt controller — asserts idle. The PCU then flushes LLC to DRAM, gates the uncore fabric, and collapses the main package rail. The most common blocker in real bring-ups is PCIe: the controller only asserts idle when the link is in L1.2 specifically, which needs ASPM and the L1 PM Substates capability on both root port and endpoint, plus correct CLKREQ# routing. If any of those is missing, PkgC6 never enters. Exit is dominated by the LLC reload from DRAM, which is why aggressive PkgC6 hurts latency-sensitive workloads. S0ix builds on this — if PkgC6 is broken, Modern Standby power is over budget. On Snapdragon the same loop is PSCI requests aggregated by RPMh and programmed into the PMIC over SPMI."

---

### Architect Drills - [day27_cstates_pkgc_s0ix.md](archive/day27_cstates_pkgc_s0ix.md)

*Idle is an architecture problem disguised as a power one. Reason from constraints — entry cost, exit cost, residency, break-even — not from the C-state names.*

**D1. Why a discrete ladder (C0/C1/C6/C10) and not a continuous knob — "sleep deeper the longer I'm idle"?**
*Constraints to weigh:* every state needs a coordinated entry/exit sequence (rails, clocks, retention save, cache flush, snoop quiesce); each sequence is FW + HW co-designed and validated separately. Continuous would mean infinite validation matrix. Why are the chosen rungs spaced where they are (what makes the gap from C1 to C6 worth the validation cost)? What would a 5th rung between C6 and C10 actually buy?

**D2. Why does PkgC6 require *every* uncore IP to be idle? Why can't the package power-gate cores independently and leave uncore running?**
*Constraints to weigh:* shared rails (the same VR feeds cores and uncore on most designs), snoop traffic (an active uncore agent can issue a coherent probe into a power-gated core), the PCU as a single arbiter with a single global view, retention-state consistency. Where does the assumption break — is there a SoC topology where independent core gating is genuinely possible?

**D3. Retention SRAM stays powered in C6. Why retention at all — why not save core state to DRAM and skip the dedicated array?**
*Constraints to weigh:* exit latency budget (DRAM access ~100 ns + path setup vs SRAM ~1 ns), DRAM controller itself may be in self-refresh and need wake first, energy of one SRAM array vs energy of waking the memory subsystem to read 4 KB, and the chicken-and-egg of "who restores the path that restores the cores." When does the trade flip — what would force a future SoC to drop retention SRAM?

**D16. S0ix depends on PCIe L1.2. What architectural change would let a platform reach S0ix-class power without L1.2?**
*Constraints to weigh:* common-mode current on the link (~mW per lane) is the dominant non-CPU residual; alternatives are full link-down + re-train (latency cost), side-band wake signalling, or moving the device to a different rail entirely. CXL idle states are a real-world rerun of this question — what did the spec authors land on, and why?

**D17. Derive the break-even residency for entering a deeper C-state. When is going deeper a *net loss*?**
*Constraints to weigh:* entry energy $E_{\text{in}}$, exit energy $E_{\text{out}}$, savings rate $\Delta P$ while in the state, expected residency $T_r$. Inequality: enter only if $T_r > (E_{\text{in}} + E_{\text{out}}) / \Delta P$. How does the OS/PCU estimate $T_r$ in practice, and what happens to the estimator under bursty wake patterns (the "ping-pong" failure mode)?

**D18. If wake latency could be made arbitrarily fast (~ns), what changes about the C-state ladder?**
*Constraints to weigh:* the ladder exists because exit cost is high enough to justify staying shallow for short idles. Collapse exit cost → the ladder collapses to two states (on/deep). What's then the bottleneck — is it OS scheduler granularity, interrupt-controller wake path, or the physics of restoring rail voltage? Pick one and argue why it is now load-bearing.

**D19. PkgC6 residency = 0 on a quiet system. Reason about where you look *before* reading any blocking-reason MSR — what does the system shape predict?**
*Constraints to weigh:* what *can* keep an IP busy on a "quiet" system (tick timers, USB SOFs, PCIe LTR, audio DMA at 10 ms cadence, GPU compositor, SMI storms). Rank these by base rate. Does "residency = 0" vs "residency = 30%" point to different culprit classes? The blocking-reason MSR is the *confirmation*, not the *hypothesis*.

---

### Exit Check - [day27_cstates_pkgc_s0ix.md](archive/day27_cstates_pkgc_s0ix.md)

- [ ] Stated clock-gating vs power-gating (saves what, wakes how fast).
- [ ] Walked Core C6 entry in 6 steps from MWAIT to power gate.
- [ ] Walked PkgC6 entry across the cross-IP gate.
- [ ] Stated why PCIe must be in L1.2 specifically.
- [ ] Stated why LLC reload dominates PkgC6 exit.
- [ ] Mapped S0ix to PkgC6 dependency.
- [ ] Mapped Snapdragon PSCI/RPMh equivalent.

### Concept - [day35_pcie_power_management.md](archive/day35_pcie_power_management.md)

PCIe defines two orthogonal power-management dimensions: **link-state PM** (ASPM, L0/L0s/L1/L1.1/L1.2) which controls the PHY and refclk, and **device-state PM** (D-states, D0/D1/D2/D3hot/D3cold) which controls device function power. The two interact: a device in D3hot still has its link, a device in D3cold loses both. **L1.2 is the marquee deep state** — refclk is removed, common-mode is dropped, side-band CLKREQ# is the only way back — and it's also the most fragile to validate because exit timing and side-band handshakes are easy to break. **LTR (Latency Tolerance Reporting)** is the SW-side hint that lets the platform pick how deep to go.

### Deep Study Notes - [day35_pcie_power_management.md](archive/day35_pcie_power_management.md)

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

Plus PVT × link-speed × link-width × cross-IP (with PkgC for D37 anchor).

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

This is **why D37 exists** — the intersection is so critical it gets its own day.

---

### Bugs in this Topic - [day35_pcie_power_management.md](archive/day35_pcie_power_management.md)

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

### Interview Explanation (60-90 sec) - [day35_pcie_power_management.md](archive/day35_pcie_power_management.md)

> "PCIe PM has two axes: link state and device state. Link states are L0 active, L0s per-direction idle with FTS-based wake, L1 link idle with refclk still on, and the L1 sub-states L1.1 and L1.2 — L1.2 being the deep one where refclk is gated off and common-mode bias is dropped. L1.2 wake is via side-band CLKREQ#: the device asserts CLKREQ#, the platform restarts refclk, Vcm restores, the link goes to Recovery and trains back to L0 in roughly 100-200 µs. Device states D0-D3hot are independent and programmed via the PM cap's PMCSR; D3cold removes function rails entirely. LTR is the device's hint to the platform about how much wake latency it can tolerate — without LTR or with a too-tight LTR, the platform won't enter PkgC. The validation surface is a cross-product of ASPM state × D-state × transition × wake source × LTR value, plus side-band timing assertions on CLKREQ# vs refclk vs Vcm. The bugs that escape into post-Si are usually side-band: shared CLKREQ# waking neighbours, CLKREQ# board glitches causing false wakes, refclk gate timing, or LTR misprogramming that blocks PkgC. socwatch is the oracle that tells you which rootport is blocking PkgC, and that's the entry point to every L1.2 debug."

---

### Architect Drills - [day35_pcie_power_management.md](archive/day35_pcie_power_management.md)

*PCIe PM is timing-sensitive and cross-IP. Reason without an answer key.*

**D1. L0s vs L1 — when is L0s the right choice on a modern 16-lane Gen5 link?**
*Constraints to weigh:* FTS overhead per direction per entry/exit, ratio of busy:idle windows, exit latency budget, real-world workloads where bursty traffic at <100 ns gaps lives.

**D2. Why does L1.2 require side-band CLKREQ# rather than electrical-idle exit?**
*Constraints to weigh:* refclk off means receiver has no clock to recover bit-lock from, common-mode collapse means no DC reference for differential pair, latency cost of refclk restart vs FTS.

**D3. Two endpoints share one CLKREQ# pin on a budget board. Reason about all the things that can go wrong.**
*Constraints to weigh:* open-drain wired-OR semantics, wake amplification, asymmetric LTR, debug visibility on which device woke, whether SW can know.

**D16. LTR is "infinite tolerance." Reason about whether this is safe.**
*Constraints to weigh:* worst-case wake latency at L1.2 + PkgC6 + DRAM-SR-exit, what breaks (real-time audio, NIC RX dropping), why platforms default infinite when device doesn't say.

**D17. Reason about D3cold: it removes function rails. What does the device's PM cap say to SW about this?**
*Constraints to weigh:* PMCSR only reads/writes when device powered, cfg access in D3cold requires platform path, ACPI _PS3 method, link state L2 vs L3.

**D18. PME from D3hot must wake the link from L1.2. Reason about the ordering of CLKREQ# assert and PME-TLP send.**
*Constraints to weigh:* TLP can't be sent without a link, link can't come up without refclk, refclk needs CLKREQ#, so PME → CLKREQ# → refclk → train → L0 → PME TLP.

**D19. The val team has 200 hours of emulation budget for PCIe PM. Reason about how to allocate across ASPM directed, L1.2 stress, cross-IP with PkgC, LTR sweeps, and PVT corners.**
*Constraints to weigh:* bug-find-rate per scenario class, what only emul catches (real driver behaviour), what sim catches more cheaply, where post-Si is unforgiving.

---

### Exit Check - [day35_pcie_power_management.md](archive/day35_pcie_power_management.md)

- [ ] Drew the ASPM state ladder (L0 → L0s → L1 → L1.1 → L1.2) with refclk/Vcm/CLKREQ# columns.
- [ ] Stated L0s, L1, L1.1, L1.2 exit latencies in order of magnitude.
- [ ] Walked through L1.2 entry handshake in 7 steps.
- [ ] Walked through L1.2 exit handshake in 6 steps.
- [ ] Stated the difference between ASPM states (link) and D-states (function).
- [ ] Explained what LTR is and why mis-programming kills PkgC.
- [ ] Named the LTR/CLKREQ#/refclk SVAs the val team owns.
- [ ] Named three post-Si PCIe-PM bugs and the telemetry path to each.

### Concept - [day37_pcie_pm_intersection.md](archive/day37_pcie_pm_intersection.md)

PCIe and CPU package PM are **cross-domain dependencies** — neither can reach its deepest state without the other in lockstep. The platform's PCU computes PkgC eligibility from a set of **blocker inputs**, and **"any rootport not in L1.2"** is one of those blockers. Conversely, when the package enters PC6, the rootport's clock + power are also affected — if PCIe wakes mid-PC entry, the negotiation has to abort cleanly. **Most marquee PM bugs on modern x86 platforms live in this intersection.** ADR, S0ix, mid-FLIT wake, refclk-after-PkgC — every one is a PCIe-and-PCU joint failure mode.

### Deep Study Notes - [day37_pcie_pm_intersection.md](archive/day37_pcie_pm_intersection.md)

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

The bug from the D31 story: under one stepping the PCU FSM inserted the WPQ flush **between** PCIe rootport quiesce and the snoop flush, racing an in-flight DMA write through PCIe. Fix was a microcode-enforced barrier; the lasting fix was a coverage row `(ADR-event × PCIe-RP-state × DRAM-state)` and an SVA `p_adr_flush_after_pcie_quiesce`.

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

### Bugs in this Topic - [day37_pcie_pm_intersection.md](archive/day37_pcie_pm_intersection.md)

- **L0p mid-FLIT on PC abort** (story above): PCU refclk re-enable raced rootport FIFO drain. *Fix: microcode handshake; SVA + cross-IP coverage.*
- **ADR-WPQ flush re-ordered** (D31 story): cross-IP PCU FSM. *Fix: barrier insertion; cross-coverage.*
- **One rootport's driver disables L1.2 in /sys; PkgC stuck at 0%:** distro chose conservative default. *Fix: doc + product-validation script that enables L1.2 per platform.*
- **LTR sticky at tight value after first packet:** endpoint never updates LTR after init. *Fix: driver fix or PCU re-aggregation policy.*
- **Two rootports race for blocker bit:** PCU latches one's value, misses the other; intermittent premature PkgC. *Fix: atomic blocker bus.*
- **PME from L1.2 drops during PkgC exit:** PME path gated by uncore-not-yet-restored. *Fix: PME path moved to AON.*
- **Refclk arrives before CLKREQ# acknowledged on wake:** rootport idle-spins for spec-min before issuing recovery. *Fix: small FSM tweak; SVA on CLKREQ#-vs-refclk ordering.*
- **CLKREQ# board glitch during PkgC:** spurious wake every 8 ms → PkgC residency 5% instead of expected 90%. *Fix: board (RC filter on CLKREQ#); val signature is Quarch + scope correlation.*
- **Hot-plug during PkgC entry:** new device asserts PRSNT# mid-entry; PCU aborts but rootport's enumeration state was already partially torn. *Fix: hot-plug-during-PM-entry SVA + coverage row.*

---

### Interview Explanation (60-90 sec) - [day37_pcie_pm_intersection.md](archive/day37_pcie_pm_intersection.md)

> "PCIe and CPU package PM are the joint at the most-validated cross-IP surface on a modern x86. The PCU computes PkgC eligibility from a blocker chain — every active core in C6, all caches flushed, display in self-refresh, DRAM in self-refresh, every PCIe rootport in L1.2 — and any of those not being ready holds the whole package up. PCIe is almost always the dominant blocker because rootport refclk lives on the PCU island; so L1.2 is the only state where refclk can drop. The race surface is two FSMs running in parallel: the PCU PkgC-eval FSM and each rootport's L1.2-entry FSM, plus side-band CLKREQ#/refclk timing. The marquee bug I'd cite is the L0p mid-FLIT: a PkgC abort re-enabled refclk while a rootport had a partial FLIT in its TX FIFO; resumed transmission was malformed; the EP AER'd, link dropped, eventually surprise-down. PCU FW fix was a forced rootport-drain handshake on abort; the lasting fix was a cross-IP coverage cell `(PC-abort-phase × RP-FIFO-state)` and an SVA blocking refclk resume with a pending FIFO. The post-Si debug playbook is socwatch for block-attribution, then lspci/sys for per-RP state, then analyser for actual L1.2 transitions, then scope for side-band timing, then PCU opcode trace if it's still unexplained. Cross-IP PM is where pre-Si vplans are weakest because each unit team validates in isolation, and the cross-products are what bite in the lab."

---

### Architect Drills - [day37_pcie_pm_intersection.md](archive/day37_pcie_pm_intersection.md)

*Cross-IP PM is fundamentally about coordinated FSMs racing for resources. Reason without an answer key.*

**D1. Reason about why PCIe rootport refclk lives on the PCU island rather than on always-on.**
*Constraints to weigh:* refclk power cost when always-on (over thousands of rootports across the SKU lineup), wake-latency vs PMC, what "rootport refclk always on" would prevent.

**D2. Two rootports concurrently complete L1.2 entry. Reason about whether the PCU should sample the blocker chain edge-triggered, level-triggered, or with a sampling window.**
*Constraints to weigh:* race window for one transition vs the other, premature PkgC if level-triggered too eagerly, missed entry if edge-triggered, deadlock if sampling window too narrow.

**D3. Reason about whether LTR should be authoritative or advisory.**
*Constraints to weigh:* what an authoritative LTR breaks if device is buggy/conservative, what an advisory LTR doesn't help with if platform ignores, the spec's positioning.

**D16. ADR is a time-critical flush. Reason about how the cross-IP val team proves the flush completes within the rail-decay window.**
*Constraints to weigh:* analog simulation cost, worst-case path through PCU + IMC + WPQ, what cross-IP coverage cells prove the path, whether formal can express the deadline.

**D17. The val team has 8 cross-IP scenarios it can afford to author for emul. Reason about which 8 cover the highest bug surface.**
*Constraints to weigh:* bug-history weighting (ADR, L0p, LTR, refclk race), cross-product coverage, whether random emul can cover the gaps.

**D18. socwatch attributes a PkgC blocker to PCIe RP1 but the analyser shows RP1 actually in L1.2 most of the time. Reason about where the discrepancy could live.**
*Constraints to weigh:* socwatch sample rate vs L1.2 window, PCU's blocker view may lag the actual state, telemetry vs ground-truth mismatch, whose clock is authoritative.

**D19. A future SoC promises "no PC blocker from PCIe — always L1.2 capable instantly." Reason about what HW changes this would require.**
*Constraints to weigh:* refclk power islands, per-port refclk gating, CLKREQ# scaling, side-band per-port, validation surface explosion.

---

### Exit Check - [day37_pcie_pm_intersection.md](archive/day37_pcie_pm_intersection.md)

- [ ] Drew the PkgC6 blocker chain with at least 8 inputs.
- [ ] Explained why PCIe is almost always the dominant blocker.
- [ ] Walked through the 10-step L1.2 ↔ PkgC handshake.
- [ ] Named at least 5 cross-IP deadlock scenarios.
- [ ] Told the L0p mid-FLIT story in 90 sec with root cause + pre-Si feedback.
- [ ] Recited the 10-step "PkgC won't enter" post-Si debug playbook.
- [ ] Stated three LTR failure modes that block PkgC.
- [ ] Named 7 cross-IP PM vplan scenarios.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S16](s16_gating_dvfs.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S18](s18_postsi_debug_capstone.md)

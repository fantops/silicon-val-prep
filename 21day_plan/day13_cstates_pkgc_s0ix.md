# Day 13 — C-states, PkgC6, S0ix, Cross-IP Idle

> **Source merge:** [30day_plan/day05_cstates_idle.md](../30day_plan/day05_cstates_idle.md), [30day_plan/INTC notes (Core cstate Flow)](../30day_plan/), [30day_plan/QUALCOMM_CHEATSHEET.md](../30day_plan/QUALCOMM_CHEATSHEET.md). 30-day files are unchanged.

## Concept

P-states control the **active** core; C-states control the **idle** core. Deeper C-states save more power but take longer to wake. **C1/C1E** = clock-gated (state preserved, fast wake). **C6** = power-gated (state saved to retention SRAM, voltage collapsed, slow wake ~100 µs+). When *all* cores reach C6 *and* every uncore IP asserts idle, the package can enter **PkgC6** — LLC flushed to DRAM, uncore fabric power-gated, main package rail collapsed. **S0ix / Modern Standby** targets PkgC10 — if PkgC6 is broken, S0ix power is over-budget. The most common PkgC6 blocker in real bring-ups is a **PCIe link not in L1.2**.

## Why this day in the flow

Day 12 was active-power. Day 13 is idle-power and **the cross-IP handshake** that makes deep package idle work. Day 14 then tells two real bug stories at this exact layer (ADR, Core C6 wake-lost). Together Days 12-14 are the power chapter; Day 15 closes it with thermal and counters.

In the 30-day Layer map this is **Layer 5 — idle side**.

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

## Practice Q&A

**Q1. Clock gating vs power gating — what does each save and cost?**
Clock gating: dynamic only, ~µs wake, no state loss. Power gating: dynamic + leakage, ~100 µs wake, state must be saved to retention SRAM.

**Q2. Why does PCIe being out of L1.2 block PkgC6?**
PCIe controller only asserts idle when the link is in L1.2 (common-mode off). PCU waits for every uncore IP to assert idle. No L1.2 → no PCIe idle → PCU blocks PkgC6.

**Q3. Why is LLC reload the dominant PkgC6 exit cost?**
LLC was flushed to DRAM and power-gated on entry. On exit it starts cold; every line must re-fetch from DRAM. Hundreds of µs for multi-MB LLC.

**Q4. The retention SRAM stays powered in C6 — why isn't that defeating the purpose?**
A few KB, almost never accessed (only entry/exit). Its leakage is tiny vs a multi-MB cache + execution units running at high V.

**Q5. PkgC6 residency = 0 on a quiet system — where do you look first?**
Read the PCU blocking-reason MSR. Whatever IP it names → check that IP's PM contract.

**Q6. PkgC6 entered then immediately exited — what's happening?**
A wake source kept tripping. Trace wake reason — usually a periodic timer or device heartbeat that wasn't masked.

**Q7. Why does Modern Standby (S0ix) depend on PkgC6 working?**
S0ix targets PkgC10; PkgC6 is the first big residency contributor. If PkgC6 is broken, the platform never deepens past PC2 and S0ix power is over budget.

**Q8. Snapdragon equivalent of PkgC6 entry?**
OS PSCI request → RPMh aggregates votes → TCS fires → PMIC drops rails over SPMI. Same debug ladder.

**Q9. Why is ADR a cross-IP bug magnet?**
PCU + memory controller + cache hierarchy + power supply must coordinate within milliseconds; opcode ordering bugs cause data loss (Day 14 story).

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

## Coding B — Interview DS&A: Climbing Stairs (LC 70) — same shape as C-state path counting

```python
def climbStairs(n):
    a, b = 1, 1
    for _ in range(n):
        a, b = b, a + b
    return a
```

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

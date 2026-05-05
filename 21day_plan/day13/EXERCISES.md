# Day 13 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day13_cstates_pkgc_s0ix.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: C-state request + PkgC6 PCU gate model

**Problem statement:** Model the PCU's PkgC6 gating logic. Given 4 cores (`cores[NCORES]`), each with its own `cstate_t cs` and a `work_pending` flag, implement `request_core_cs(id, target)` that refuses any state deeper than C1 if work is pending. Then `try_pkg_c6()` checks **all** cores are in C6 *and* every uncore IP (`llc_idle`, `mc_idle`, `pcie_in_l1_2`, `display_idle`, `intc_idle`) reports idle. If blocked, print which IP blocks. Default `pcie_in_l1_2 = false` so the first attempt fails.

**You must implement:**
```c
typedef enum { C0=0, C1=1, C1E=2, C6=6 } cstate_t;
typedef struct { cstate_t cs; bool work_pending; } core_t;
typedef struct { bool llc_idle, mc_idle, pcie_in_l1_2, display_idle, intc_idle; } uncore_t;

bool request_core_cs(int id, cstate_t target);
bool try_pkg_c6(void);
const char *blocker(void);
```

**Driver / tests to write:**
- All 4 cores → request C6.
- `try_pkg_c6()` → expect "BLOCKED: PCIe link not in L1.2".
- Set `un.pcie_in_l1_2 = true`, retry → expect "PkgC6 ENTERED".

**Be ready to explain in interview:**
- C1 vs C1E vs C6 vs C10 — clock-gated vs power-gated, retention SRAM, wake latency.
- Why does PkgC6 need *every* uncore IP idle? Shared-rail and snoop-traffic arguments.
- Break-even residency formula: enter only if $T_r > (E_{\text{in}} + E_{\text{out}}) / \Delta P$.
- L1.2 dependency for PkgC6; LTR/CLKREQ#/ASPM bring-up checklist.

## E2 — DS&A: Climbing Stairs (LC 70)

**Problem statement:** Same as Day 10 — count distinct ways to reach step n with steps of size 1 or 2. Reuse exercise here as path-counting analogue for C-state transitions.
**Signature:** `long long climb_stairs(int n);`
**Talking points:** DP recurrence `f(n) = f(n-1) + f(n-2)`; corresponds to counting valid C-state transition paths through a strict-ordering ladder.

## E3 — Architect Drills (no code, write your answers)

**D1.** Why a discrete C-state ladder vs a continuous knob? What would a 5th rung between C6 and C10 buy?
**D2.** Why does PkgC6 require all uncore IPs idle? Shared-rail + snoop-quiesce reasoning.
**D3.** Why retention SRAM in C6? When does the trade flip?
**D4.** S0ix needs PCIe L1.2. What architectural change would let a platform reach S0ix without L1.2?
**D5.** Derive break-even residency. What is the ping-pong failure mode?
**D6.** If wake latency dropped to ns, what changes about the C-state ladder?
**D7.** PkgC6 residency = 0 on a quiet system. Where do you look *before* reading any blocking-reason MSR?

## E4 — Bug Anchor

**Symptoms:** PkgC6 never enters even at full system idle. Walk the debug tree: PCIe endpoint missing L1.2 in L1 PM Substates; or ASPM disabled in BIOS; or LTR pinning the link awake. Where in `lspci -vvv` and PCU blocking-reason MSR do you confirm?

Cross-link: Day 14 — Core C6 wake-vector lost (CFC reset clear race); ADR data loss (`WPQ_FLUSH` before `IO_QUIESCE_ACK`).

## Reference solution

Once you've attempted, see `../day13_cstates_pkgc_s0ix.md` sections "Coding A" and "Coding B" for full solutions and discussion.

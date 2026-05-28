# Day 11 — CPU Bring-up + IP Functional Validation

## Concept

A CPU IP comes up in a **strict, layered sequence**: reset is released, the architectural register file is brought to a defined state, the boot vector fetches the first instruction, exceptions are validated against a tiny ISA smoke test, the MMU is enabled in identity-mapped mode, caches are turned on, a second core is brought up via IPI + spinlock, then SMMU/IO coherence is enabled, and finally a full OS boots. **Each step gates the next** — you do not enable the cache until you can prove address translation works. **IP Functional Validation** is the discipline of writing the directed + random tests, the coverpoints, and the assertions that prove the IP follows the spec at every step, *before* it's integrated into the SoC and *before* it's taped out.

## Why this day in the flow

Days 5-10 gave you the mechanics of a CPU: coherence, virtual memory, IOMMU, DMA, pipeline, memory consistency. Today is **the validation lens on all of it** — what a CPU IP val team actually does day-to-day. This is also the day that names the **deliverables** of a CPU IP Functional Val role (testbench, coverage plan, assertions, regression).

In the Layer map this is the **cap of Phase 1**: the CPU IP exists, you can describe it from reset to user-space; now we move to Phase 2 (CPU PM) on top of it.

---

## Deep Study Notes

### 1. The canonical CPU bring-up ladder

```
0. Power & reset ladder      (rails good, resets released in order)
   │
   ▼
1. Reset smoke               (PC advances, no X's, clock observable)
   │
   ▼
2. Trivial loop              (fetch + decode + execute path alive)
   │
   ▼
3. Exception path            (illegal-instr, syscall, page-fault take vector)
   │
   ▼
4. MMU enable (identity)     (translation table walked, TLB fills, identity map works)
   │
   ▼
5. Cache enable              (I$ + D$ on, coherence within core)
   │
   ▼
6. Second core (SMP)         (release CPU1 from holding pen, IPI, spinlock works)
   │
   ▼
7. SMMU / IO coherence       (device DMA translates through IOMMU, snoop is honoured)
   │
   ▼
8. Full Linux / Android boot (real workload, page tables, scheduler, drivers)
```

Each step has a corresponding **test bucket** in the IP val plan. The principle: **"the cheapest passing test that proves this step is on"** — not a smoke test that proves nothing.

### 2. What an IP Functional Val engineer actually owns

The deliverables of a CPU IP Val team, ordered from earliest to latest in the program:

| Deliverable | Purpose | Typical owner |
|---|---|---|
| **Verification plan (vplan)** | Maps every spec section → testcase + coverpoint | Val lead + arch |
| **Testbench (UVM env)** | Drivers, monitors, scoreboards, ref model | Val IC |
| **Reference model** | Golden ISA simulator (Spike, Arm FastModel, in-house) | Val + arch |
| **Directed tests** | One per spec corner; one per known failure mode | Val IC |
| **Constrained-random tests** | Stress + coverage closure | Val IC |
| **SVA assertions** | Inline checks that fire on illegal protocol/state | Val + design |
| **Functional coverage** | Covergroups for state/transition/cross | Val IC |
| **Code coverage** | RTL line/branch/toggle | Val IC + tool |
| **Regression suite** | Nightly + weekly, with triage dashboard | Val infra |
| **Bug triage + RCA** | Reproduce, isolate, file CR, verify fix | Val IC |
| **Sign-off report** | Coverage closed, all CRs resolved or waived | Val lead |

The **IP Functional Validation** role title means you sit in this column. You are not designing the IP; you are proving it does what the architecture spec says, and nothing it doesn't.

### 3. The verification plan (vplan) — the load-bearing artifact

A vplan is a giant table. Every row maps **one spec requirement** → **one or more testcases** → **one or more coverpoints** → **status**.

Example rows for a CPU IP:

| Spec ref | Feature | Test | Coverpoint | Status |
|---|---|---|---|---|
| Sec 3.4.1 | MMU 4-level walk | `mmu_walk_4lvl_directed` | `cp_mmu_levels[1..4]` | covered |
| Sec 3.4.2 | TLB shootdown via IPI | `tlb_shootdown_smp` | `cp_tlbi_ipi_cross[NCORES]` | covered |
| Sec 5.2 | Acquire-release memory model | `mem_model_acqrel_random` | `cp_mo_acqrel x cp_addr_pair` | 87% |
| Sec 6.1 | Exception priority | `excp_priority_directed` | `cp_excp_prio_matrix` | covered |
| Sec 7.3 | DVFS request via MWAIT | `mwait_cstate_directed` | `cp_cstate x cp_wake_src` | covered |

**The vplan is the contract with the architect.** If a coverpoint isn't covered, you haven't validated the feature, regardless of how many random tests passed.

### 4. Coverage — three flavours, all mandatory

| Type | What it answers | Tool |
|---|---|---|
| **Functional** | Did the *intended scenario* fire? (covergroup hits) | Simulator (VCS / Xcelium) |
| **Code** | Did the RTL *line / branch / toggle / FSM* execute? | Same simulator, coverage DB |
| **Assertion** | Did each `assert` get exercised (cover_property)? | Simulator + JasperGold for formal |

**The trap:** 100% code coverage with 40% functional coverage means you executed every line but never under the spec-intended scenario. Functional coverage is the metric the architect cares about; code coverage is the metric the tool reports first.

### 5. Assertions — SVA in three categories

```systemverilog
// Immediate: fired on procedural path
always_ff @(posedge clk)
  if (cs == M && other_core_writes) $error("MESI: M without invalidate");

// Concurrent: temporal property over time
property pcie_req_grant;
  @(posedge clk) req |-> ##[1:8] grant;
endproperty
assert property (pcie_req_grant);

// Cover: same syntax, but counts hits instead of failing
cover property (@(posedge clk) cs == M ##1 cs == I);
```

Assertions live in the **design** (white-box) or in a **bind file** (no RTL change). The IP Val team owns the bind file. Every spec "shall" sentence should map to an assertion.

### 6. Reference model + scoreboard pattern

```
   stimulus  ──┬──►  DUT (RTL)        ──►  monitor  ──┐
               │                                       │
               └──►  Reference model (Spike/FM)  ──►  scoreboard
                                                       │
                                                       ▼
                                            mismatch → $error
```

The reference model executes the same ISA in software. The scoreboard compares architectural state (regs, MSRs, memory snapshot) after every retired instruction. **Any divergence is a bug — in the RTL, the model, or the test.** Triage starts at the first diverging cycle.

### 7. Bring-up infrastructure — pre-Si

| Layer | What it provides |
|---|---|
| **UVM env** | Driver (stimulus into IP), monitor (observe outputs), scoreboard (check against ref model), config object |
| **Bus functional models (BFM)** | Stand-in for AXI/CHI/PCIe agents so the IP can be exercised without the SoC |
| **ISA stim generator** | Random instruction streams (riscv-dv, Genesys-style) or hand-coded ASM tests |
| **Trace replay** | Replay real-OS traces captured from FastModel / Spike onto the RTL |
| **Power-aware sim (UPF)** | Models power-down / retention / isolation so PM transitions are exercised |
| **Emulation port (Zebu / Palladium)** | Same env, scaled to MHz, boots Linux on the IP |

### 8. Bring-up infrastructure — post-Si

| Tool | What it does |
|---|---|
| **JTAG / Lauterbach** | Halt core, single-step, read registers, set HW breakpoints |
| **DCI / ITP (Intel)** | Direct Connect Interface to PCH for in-target probing |
| **SVTools** | Intel SoC validation toolkit (PMC trace, P/C-state inspection) |
| **`rdmsr` / `wrmsr`** | Read/write model-specific registers from OS |
| **`perf` / VTune** | Hardware counters, top-down attribution |
| **`turbostat`** | Per-core C-state residency, P-state, MHz, watts |
| **`socwatch`** | Intel package-level PM telemetry |
| **Logic analyser** | Board-level signal capture (PERST#, CLKREQ#, PLI) |
| **Scope** | Rail droop, signal integrity |

### 9. Coverage-driven verification (CDV) — the workflow loop

```
   write vplan  →  write tests  →  run regression  →  read coverage report
        ▲                                                   │
        │                                                   ▼
        └─────────  add tests where coverage is low  ◄──────┘
```

Closure criterion: every coverpoint in the vplan at 100%, every assertion in `cover` mode hit at least once, every CR resolved or explicitly waived with sign-off.

### 10. Bug-cost ladder (why catch left)

| Phase | Cost-per-bug | What this phase catches cheaply |
|---|---|---|
| **RTL sim** | ~$1 | Functional, protocol, FSM, register |
| **Emulation** | ~$10 | HW × FW integration, cross-IP, multi-core |
| **FPGA proto** | ~$100 | Full SW stack at MHz, real driver |
| **Post-Si** | ~$1000 | Analog, PVT, real workload, interop |

**Shift-left rule:** any bug class that *can* be caught in RTL sim *must* be. Escaping to post-Si is a coverage gap, not bad luck.

---

## Bring-up Flow — CPU IP (Detailed)

```
Stage A — Power & reset ladder (HW only, no code)
   ├─ Always-on rail up
   ├─ Standby rail up
   ├─ Main core/uncore rails up (in order, one waits for previous)
   └─ Reset deasserted (standby reset before main reset)

Stage B — Reset smoke (first instruction)
   ├─ Core fetches from BOOT_VECTOR
   ├─ PC observable on debug port → confirms fetch path alive
   └─ No X's on any architectural reg

Stage C — Tiny ISA test (alu+branch+memory)
   ├─ Sequence: nop, add, sub, store, load, conditional branch
   └─ Compare retired state vs reference model after each insn

Stage D — Exception smoke
   ├─ Execute illegal opcode → exception vector taken, esr/cs set, returns cleanly
   ├─ Issue syscall → SVC/SMC vector
   └─ Read from unmapped address → page-fault vector

Stage E — MMU on (identity map)
   ├─ Build identity page table covering reset code + first KB of RAM
   ├─ Enable MMU bit
   ├─ Next instruction fetched through translation (TLB miss → walk → fill)
   └─ Verify same code runs unchanged

Stage F — Cache on
   ├─ Enable I$
   ├─ Enable D$
   ├─ Run a small loop; observe miss rate fall on second iteration
   └─ Verify no coherence violation (single-core)

Stage G — SMP (release CPU1)
   ├─ CPU0 writes "release" flag to known address
   ├─ CPU1 was spinning in holding pen on that flag
   ├─ CPU1 walks its own bring-up (D-F above)
   ├─ Spinlock between CPU0+CPU1 acquired and released
   └─ IPI from CPU0 → CPU1 takes IRQ vector

Stage H — SMMU + IO coherence
   ├─ Program SMMU stream table, install identity IOVA→PA
   ├─ DMA agent issues a write through SMMU → lands at expected PA
   └─ Coherent DMA: CPU sees the write without explicit cache flush

Stage I — Full OS boot
   ├─ Hand off to bootloader (U-Boot / Tianocore)
   ├─ Bootloader loads kernel, sets up final page tables, jumps to kernel entry
   └─ Kernel inits scheduler, drivers, user-space — login prompt
```

Each stage has a **failure-symptom triage table** the val team maintains: "If PC stuck at BOOT_VECTOR → check reset deassert order, check refclk, check fetch arbiter."

---

## What's Required

To bring up a CPU IP in a pre-Si testbench you need:

- **DUT RTL** (the CPU IP) wired into a top-level wrapper.
- **Clock + reset generator** with controllable assertion order.
- **AXI/CHI BFM** for memory side; **APB BFM** for debug + CSR access.
- **Memory model** (golden DRAM that answers reads/writes with known latency).
- **Boot ROM image** containing the reset code + tiny ISA test.
- **Reference model** (Spike / Arm FastModel) configured to the same ISA variant.
- **Vplan** with at least Stage A-I rows enumerated.
- **Regression infrastructure**: simulator licenses, compute farm, results database, triage dashboard.

---

## Tools

| Phase | Tool | Use |
|---|---|---|
| **Pre-Si sim** | Synopsys VCS / Cadence Xcelium | RTL simulation; nightly regression |
| | Synopsys Verdi | Waveform debug (FSDB); SVA debug |
| | JasperGold / VC Formal | Formal proof on FSM + register-protocol assertions |
| | Synopsys UVM | Verification methodology |
| | riscv-dv / proprietary stim | Random ISA stream generation |
| | Spike / Arm FastModel | Golden reference ISA simulator |
| **Pre-Si emul** | Synopsys ZeBu / Cadence Palladium / Mentor Veloce | RTL at MHz; boots OS pre-tape-out |
| | Synopsys HAPS / Cadence Protium | FPGA prototype; faster than emul, less visibility |
| | Synopsys Perspec | Scenario-based directed test |
| **Post-Si** | Lauterbach TRACE32 + JTAG | Halt, single-step, breakpoints, read regs |
| | Intel DCI / ITP / SVTools | In-target probing on Intel silicon |
| | `rdmsr` / `wrmsr` / `lspci` / `setpci` | OS-level register access |
| | `perf` / VTune / `turbostat` / `socwatch` | Counter + residency + telemetry |
| | Logic analyser (Tektronix / Keysight) | Board-signal capture |
| | Oscilloscope + rail probes | Analog / SI |

---

## Bugs in this Topic

- **PC stuck at BOOT_VECTOR after reset:** reset deassert ordering wrong; refclk dead; fetch arbiter held; ROM not driving data.
- **First instruction retires wrong value:** ref-model mismatch — usually a tool-version skew, sometimes a real ISA-variant config bug.
- **MMU enable hangs:** identity-map page table malformed; one PTE has wrong PFN; cache holds stale translation. *Fix: flush TLB + I-cache after enabling MMU; re-verify under formal.*
- **Cache enable corrupts data:** snoop arbiter races between cores; D-cache way replacement gives wrong line. *Catch: scoreboard with cache-snapshot at every retired memory op.*
- **CPU1 never leaves holding pen:** release flag write didn't reach the address CPU1 polls (wrong PA, no cache flush, no barrier). *Fix: write + DMB + DSB on releaser; ensure address is uncached or invalidated on poller.*
- **IPI lost:** GIC/APIC not initialised before CPU0 fires the IPI. *Fix: GIC distributor + redistributor init must complete before the first IPI.*
- **SMMU translates to wrong PA:** stream-table entry has wrong context bank; identity map missing one page. *Fix: dump stream table + IOVA→PA walk in the test.*
- **OS boots but oopses in init:** missed cache flush at hand-off; cold L2 lines hold stale values from boot ROM. *Fix: bootloader flushes L1+L2 before jumping to kernel.*

---

## Interview Explanation (60-90 sec)

> "Bringing up a CPU IP is a strict ladder: power and reset in order, first instruction fetched at the boot vector, a tiny ISA smoke test, exceptions, MMU on in identity map, caches on, second core released via spinlock + IPI, SMMU, then a real OS. Each stage gates the next — you don't enable the cache before you can prove translation works. As an IP Functional Validation engineer my job is to write the verification plan that maps every spec section to a testcase and a coverpoint, build the UVM environment with a reference model and scoreboard, write SVA assertions for every 'shall' sentence in the spec, run the regression, and triage every failure to a CR. The closure criterion is every coverpoint at 100%, every assertion covered, every CR resolved or signed-off waiver. The shift-left discipline matters because cost rises 10× per phase — RTL sim is $1 per bug, post-Si is $1000, so anything findable in sim *must* be found in sim. The hard part of the role is not the tooling — it's reading the spec, finding the ambiguous corner the architect hadn't pinned down, and writing the test that exposes whether the RTL implements the corner or the architect's other interpretation of it."

---

## Test-plan from spec — 8-step scaffold (reusable)

*Cerebras 13 May probe: "design a test plan for this device from scratch." Same shape lands at any AI-accelerator / IP-val shop. Walk the scaffold in order — don't skip steps, don't reorder.*

1. **Spec ingest** — read the spec; list every "shall" sentence and every mode/state diagram. This is the source of truth; every later artifact traces back to a spec line.
2. **Feature list** — collapse the spec into a flat feature table (one row per testable behaviour). This is what the vplan rows are derived from.
3. **Modes & configurations** — enumerate every mode (operational / debug / low-power / error) and every legal config (lane counts, speeds, cache sizes, MSI vs MSI-X). The cross-product defines the matrix size.
4. **Stimulus + checks** — for each feature × mode, name the directed stimulus, the constrained-random stimulus, and the scoreboard / SVA / reference-model check that says "correct." No check = no test.
5. **Coverage axes** — functional coverpoints (state × transition × residency × config), code coverage (line / branch / FSM / toggle), cross-coverage between independent axes. Coverage is the closure metric, not test count.
6. **Negative + error injection** — illegal opcodes, protocol violations, parity/ECC injection, timeout paths, reset-during-X for every X. Spec ambiguity lives here.
7. **Perf + power** — throughput / latency / power-state residency checks; gate on regressions, not just functional pass. PnP bugs hide in functional-pass runs.
8. **Pre-Si vs post-Si split** — what's only catchable in RTL sim (assertion-level corners, coverage closure), what's only catchable post-Si (real workloads, SI/PI, thermal, mass-population variance). Map each feature row to one or both columns.

**Reflex one-liner:** *"Spec → features → modes → stimulus + checks → coverage → negative → perf/power → pre-Si vs post-Si. Eight steps. Every device. Don't skip."*

*Cross-link: same scaffold is the PM-coverage backbone in [Day 16 Sec pre-Si PM val](day16_pre_si_pm_validation.md) and [Day 17 post-Si PM val](day17_post_si_pm_validation.md). For PCIe-specific axes see [Day 19 enumeration coverage](day19_pcie_enumeration_config_msi.md) and [Day 21 PCIe bring-up](day21_pcie_bringup_functional_val.md).*

---

## Architect Drills

*Bring-up is a ladder because each step's correctness is a precondition for the next. Reason about why the ladder is ordered the way it is — no answer key.*

**D1. Why is MMU enabled *before* caches, never the other way around?**
*Constraints to weigh:* cache is physically-indexed-physically-tagged (PIPT) on modern designs vs VIPT, what a stale cache holds before MMU is on, what a TLB miss does if cache is on but cold. Reason about a system where the order would reverse — does one exist?

**D2. Why is the reference model in software (Spike, FastModel) rather than another RTL block?**
*Constraints to weigh:* sim speed of two RTL models side-by-side, debug visibility, source-of-truth for "what the architecture says," cost of keeping two RTLs in sync. When would two RTLs be the right answer instead of RTL+SW?

**D3. The vplan has one row per spec requirement. Reason about what goes wrong if you skip the vplan and just write random tests.**
*Constraints to weigh:* coverage-without-intent, regression that proves nothing, the day the architect asks "did we validate Sec 5.2.3?" and you can't answer. What's the smallest-vplan that still beats no-vplan?

**D4. Code coverage is 100%; functional coverage is 40%. What does this gap mean?**
*Constraints to weigh:* every line ran, but not under the intended scenario; random tests hit easy paths repeatedly; coverpoints are tighter than line-cov. What test changes close the gap *without* gaming the coverpoint definitions?

**D5. SVA assertions in the design (white-box) vs in a bind file (black-box). Reason about the tradeoff.**
*Constraints to weigh:* design-team ownership vs val-team ownership, RTL re-spin cost when an assertion changes, formal-tool friendliness, what each enables that the other doesn't. Which one survives a designer who pushes back on "asserts cluttering my code"?

**D6. You find a bug in post-Si that should have been caught in RTL sim. Reason about what process change prevents the next escape.**
*Constraints to weigh:* coverpoint addition vs assertion addition vs random-seed expansion, whether the bug class was reachable in sim at all, what testbench limitation made it unreachable. What's the *minimum* process change — and where does over-engineering start?

**D7. Imagine the IP you're validating has a brand-new architectural feature (e.g., a new memory ordering primitive). Reason about the order in which you write tests for it.**
*Constraints to weigh:* directed tests prove the spec interpretation, formal proves the property holds, random builds confidence, scoreboard catches cross-feature interactions. Which goes first, and why does the order matter for catching the architect's spec ambiguity?

---

## Coding A — Systems C: CPU bring-up ladder simulator

A small state-machine harness that walks the 9-stage bring-up ladder and reports the first failing stage with a diagnostic — the same logic an automated bring-up gate runs.

```c
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    STAGE_RESET, STAGE_FETCH, STAGE_ISA, STAGE_EXCP, STAGE_MMU,
    STAGE_CACHE, STAGE_SMP, STAGE_SMMU, STAGE_OS, STAGE_DONE
} stage_t;

static const char *stage_name(stage_t s) {
    static const char *n[] = { "RESET","FETCH","ISA","EXCP","MMU",
                                "CACHE","SMP","SMMU","OS","DONE" };
    return n[s];
}

typedef struct {
    bool rails_good;
    bool refclk_alive;
    bool pc_advances;
    bool ref_model_match;
    bool excp_vector_taken;
    bool mmu_identity_works;
    bool cache_no_corruption;
    bool cpu1_released;
    bool smmu_translates;
    bool os_login_prompt;
} dut_t;

/* Returns first failing stage, or STAGE_DONE if all pass. */
static stage_t walk_ladder(const dut_t *d) {
    if (!d->rails_good || !d->refclk_alive)     return STAGE_RESET;
    if (!d->pc_advances)                         return STAGE_FETCH;
    if (!d->ref_model_match)                     return STAGE_ISA;
    if (!d->excp_vector_taken)                   return STAGE_EXCP;
    if (!d->mmu_identity_works)                  return STAGE_MMU;
    if (!d->cache_no_corruption)                 return STAGE_CACHE;
    if (!d->cpu1_released)                       return STAGE_SMP;
    if (!d->smmu_translates)                     return STAGE_SMMU;
    if (!d->os_login_prompt)                     return STAGE_OS;
    return STAGE_DONE;
}

int main(void) {
    /* Golden run */
    dut_t good = {1,1,1,1,1,1,1,1,1,1};
    printf("Good DUT: first failing stage = %s\n", stage_name(walk_ladder(&good)));

    /* MMU bug */
    dut_t mmu_bug = {1,1,1,1,1, /*mmu*/0, 1,1,1,1};
    printf("MMU bug:  first failing stage = %s  -> "
           "check identity PTEs, TLB flush, I-cache invalidate after enable\n",
           stage_name(walk_ladder(&mmu_bug)));

    /* SMP holding-pen bug */
    dut_t smp_bug = {1,1,1,1,1,1,1, /*smp*/0, 1,1};
    printf("SMP bug:  first failing stage = %s  -> "
           "check release-flag PA, DMB/DSB on writer, cache state on poller\n",
           stage_name(walk_ladder(&smp_bug)));
    return 0;
}
```

---

## Bug Anchor

- **Day 5 (MESI):** cache-enable bugs in Stage F.
- **Day 6 (VM):** MMU identity-map bugs in Stage E.
- **Day 7 (TLB/IOMMU):** SMMU bugs in Stage H.
- **Day 8 (DMA/barriers):** SMP release-flag-without-barrier bug in Stage G.
- **Day 9 (pipeline):** ref-model mismatches in Stage C often live here.
- **Day 10 (memory consistency):** SMP spinlock + IPI bugs.
- **Day 17 (post-Si PM val):** bring-up surfaces the same PM-related cross-IP bugs.

## Exit Check

- [ ] Drew the 9-stage bring-up ladder from memory.
- [ ] Named one test bucket per stage.
- [ ] Stated the difference between functional, code, and assertion coverage.
- [ ] Stated what a vplan is and why the architect cares.
- [ ] Named three pre-Si tools and three post-Si tools used in CPU IP val.
- [ ] Recited the bug-cost ladder (~10× per phase) and the shift-left rule.
- [ ] Can describe in 90 sec what an IP Functional Val engineer owns end-to-end.

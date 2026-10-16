# S06 - CPU firmware boot

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Primer (Intel x86, public-level):**
  1. **Before the reset vector:** FIT table → microcode patch → Boot Guard ACM verifies the initial boot block.
  2. **Reset vector 0xFFFFFFF0** with no DRAM yet. Firmware uses **cache-as-RAM** as its first stack.
  3. **SEC → PEI** (MRC memory training, so DRAM comes alive) **→ DXE** (drivers, **PCIe enumeration**, **ACPI tables**) **→ BDS → OS loader**.
  4. BIOS sets PM policy and locks it. ACPI publishes `_CST` (C-states), `_PSS`/CPPC (P-states), LPIT (S0ix), and **DMAR** (IOMMU units).
  5. The BSP wakes the APs with INIT-SIPI-SIPI.
- **Also read:** [D23 CPU bring-up](archive/day23_cpu_bringup_functional_val.md).
- **Must answer:**
  - Why does early firmware need cache-as-RAM?
  - Capability layering is **fuse → BIOS → ACPI → OS driver**. A feature (C-state, IOMMU, device) is "missing": which layer do you check first?
  - Hang at a POST code during memory init: how do you tell DRAM from the CPU?
- **Work link:** what does BIOS have to enable for DSA to work with shared virtual memory (VT-d on, scalable mode, the device visible)? Where would you check each?
- **Week narration:** CDC → rails → power-good → PLL/fuses → reset → reset vector → MRC → DXE → OS.
- **Handoff →** How does software actually talk to hardware?


## Study notes

### Firmware boot: reset vector to OS handoff

On x86, the bootstrap processor begins at the reset vector. DRAM is not yet available, so early firmware uses cache-as-RAM or another platform-defined temporary memory. Firmware proceeds through SEC and PEI; PEI performs memory initialization/training (MRC on Intel platforms). DXE dispatches drivers and commonly discovers/configures PCIe devices and builds ACPI tables; BDS selects the boot target and loads the OS. Exact implementation details vary by firmware and platform.

```text
reset / reset vector -> SEC -> PEI + memory init -> DXE drivers + PCIe
enumeration + ACPI tables -> BDS / boot manager -> OS loader -> kernel
```

- Treat **fuse -> firmware policy -> ACPI/platform tables -> OS driver** as separate gates: capability at one layer does not prove exposure or enablement at the next.
- ACPI may describe processor idle/performance capabilities and IOMMU remapping units; the OS still applies policy and driver setup.
- AP startup is platform-defined; INIT-SIPI-SIPI is a common x86 mechanism, not the explanation for BSP entry.
- For a hang, record the last POST code and ask whether DRAM training, DXE dispatch, device enumeration, or boot selection is the first incomplete step.

### Worked example: device capability appears missing in the OS

Check in order: platform fuse/capability, firmware setup policy, firmware enumeration/configuration, ACPI exposure where relevant, OS IOMMU and driver policy, then runtime device state. This prevents diagnosing an OS driver when firmware never enabled or exposed the feature.

### Concept - [day23_cpu_bringup_functional_val.md](archive/day23_cpu_bringup_functional_val.md)

A CPU IP comes up in a **strict, layered sequence**: reset is released, the architectural register file is brought to a defined state, the boot vector fetches the first instruction, exceptions are validated against a tiny ISA smoke test, the MMU is enabled in identity-mapped mode, caches are turned on, a second core is brought up via IPI + spinlock, then SMMU/IO coherence is enabled, and finally a full OS boots. **Each step gates the next** — you do not enable the cache until you can prove address translation works. **IP Functional Validation** is the discipline of writing the directed + random tests, the coverpoints, and the assertions that prove the IP follows the spec at every step, *before* it's integrated into the SoC and *before* it's taped out.

### Deep Study Notes - [day23_cpu_bringup_functional_val.md](archive/day23_cpu_bringup_functional_val.md)

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

Each step has a corresponding **test bucket** in the IP val plan. The principle: **"the cheapest passing test that proves this step is on"** — not a smoke test that proves nothing. **Step 0 (rails → PLL lock → reset release) is [Day 11](archive/day11_clocking_reset_architecture.md) in depth** — this ladder starts the cycle reset is deasserted, giving a clean **bottom-to-top** chain: clocking/reset (D11) → first instruction → MMU → cache → SMP → OS.

### 1a. Who triggers startup code after reset?

This is the Nvidia-screening trap: startup code does not run because "software starts." Hardware creates the first executable condition.

**Actor chain:**

```
PMIC / platform controller
   → rails stable + clocks valid
   → reset controller releases CPU reset
   → CPU hardware loads PC from reset vector / boot vector
   → fetch unit reads Boot ROM at that address
   → Boot ROM runs first startup code
   → ROM initializes minimal stack/DRAM/security state
   → ROM loads next-stage firmware / bootloader
   → bootloader initializes richer platform state
   → kernel entry / OS boot
```

**60-sec answer:**

> "The trigger is reset release, not an OS thread. Once rails and clocks are good, the reset controller deasserts CPU reset. The CPU hardware forces the PC to the architectural reset vector and starts fetching from Boot ROM. That Boot ROM is the first startup code: it sets minimal state, may authenticate or load the next stage, initializes memory enough for the bootloader, and jumps to firmware or kernel entry. In validation I debug this as a ladder: rail/clock/reset visible, PC leaves boot vector, first instruction retires, ROM access works, then firmware handoff works. If startup code never runs, I first check reset order, boot-vector mapping, ROM visibility, and whether the fetch path is held by clock/reset gating."

**Follow-up hooks:**
- In a multi-core SoC, one boot core normally starts first; other cores stay in reset or a holding pen until released.
- In secure boot, Boot ROM may authenticate the next image before jumping.
- In emulation, the ROM image and reset-vector mapping are controllable; on silicon, straps/fuses, board reset timing, and ROM visibility become real failure sources.

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

### Bugs in this Topic - [day23_cpu_bringup_functional_val.md](archive/day23_cpu_bringup_functional_val.md)

- **PC stuck at BOOT_VECTOR after reset:** reset deassert ordering wrong; refclk dead; fetch arbiter held; ROM not driving data.
- **First instruction retires wrong value:** ref-model mismatch — usually a tool-version skew, sometimes a real ISA-variant config bug.
- **MMU enable hangs:** identity-map page table malformed; one PTE has wrong PFN; cache holds stale translation. *Fix: flush TLB + I-cache after enabling MMU; re-verify under formal.*
- **Cache enable corrupts data:** snoop arbiter races between cores; D-cache way replacement gives wrong line. *Catch: scoreboard with cache-snapshot at every retired memory op.*
- **CPU1 never leaves holding pen:** release flag write didn't reach the address CPU1 polls (wrong PA, no cache flush, no barrier). *Fix: write + DMB + DSB on releaser; ensure address is uncached or invalidated on poller.*
- **IPI lost:** GIC/APIC not initialised before CPU0 fires the IPI. *Fix: GIC distributor + redistributor init must complete before the first IPI.*
- **SMMU translates to wrong PA:** stream-table entry has wrong context bank; identity map missing one page. *Fix: dump stream table + IOVA→PA walk in the test.*
- **OS boots but oopses in init:** missed cache flush at hand-off; cold L2 lines hold stale values from boot ROM. *Fix: bootloader flushes L1+L2 before jumping to kernel.*

---

### Interview Explanation (60-90 sec) - [day23_cpu_bringup_functional_val.md](archive/day23_cpu_bringup_functional_val.md)

> "Bringing up a CPU IP is a strict ladder: power and reset in order, first instruction fetched at the boot vector, a tiny ISA smoke test, exceptions, MMU on in identity map, caches on, second core released via spinlock + IPI, SMMU, then a real OS. Each stage gates the next — you don't enable the cache before you can prove translation works. As an IP Functional Validation engineer my job is to write the verification plan that maps every spec section to a testcase and a coverpoint, build the UVM environment with a reference model and scoreboard, write SVA assertions for every 'shall' sentence in the spec, run the regression, and triage every failure to a CR. The closure criterion is every coverpoint at 100%, every assertion covered, every CR resolved or signed-off waiver. The shift-left discipline matters because cost rises 10× per phase — RTL sim is $1 per bug, post-Si is $1000, so anything findable in sim *must* be found in sim. The hard part of the role is not the tooling — it's reading the spec, finding the ambiguous corner the architect hadn't pinned down, and writing the test that exposes whether the RTL implements the corner or the architect's other interpretation of it."

---

### Architect Drills - [day23_cpu_bringup_functional_val.md](archive/day23_cpu_bringup_functional_val.md)

*Bring-up is a ladder because each step's correctness is a precondition for the next. Reason about why the ladder is ordered the way it is — no answer key.*

**D1. Why is MMU enabled *before* caches, never the other way around?**
*Constraints to weigh:* cache is physically-indexed-physically-tagged (PIPT) on modern designs vs VIPT, what a stale cache holds before MMU is on, what a TLB miss does if cache is on but cold. Reason about a system where the order would reverse — does one exist?

**D2. Why is the reference model in software (Spike, FastModel) rather than another RTL block?**
*Constraints to weigh:* sim speed of two RTL models side-by-side, debug visibility, source-of-truth for "what the architecture says," cost of keeping two RTLs in sync. When would two RTLs be the right answer instead of RTL+SW?

**D3. The vplan has one row per spec requirement. Reason about what goes wrong if you skip the vplan and just write random tests.**
*Constraints to weigh:* coverage-without-intent, regression that proves nothing, the day the architect asks "did we validate Sec 5.2.3?" and you can't answer. What's the smallest-vplan that still beats no-vplan?

**D16. Code coverage is 100%; functional coverage is 40%. What does this gap mean?**
*Constraints to weigh:* every line ran, but not under the intended scenario; random tests hit easy paths repeatedly; coverpoints are tighter than line-cov. What test changes close the gap *without* gaming the coverpoint definitions?

**D17. SVA assertions in the design (white-box) vs in a bind file (black-box). Reason about the tradeoff.**
*Constraints to weigh:* design-team ownership vs val-team ownership, RTL re-spin cost when an assertion changes, formal-tool friendliness, what each enables that the other doesn't. Which one survives a designer who pushes back on "asserts cluttering my code"?

**D18. You find a bug in post-Si that should have been caught in RTL sim. Reason about what process change prevents the next escape.**
*Constraints to weigh:* coverpoint addition vs assertion addition vs random-seed expansion, whether the bug class was reachable in sim at all, what testbench limitation made it unreachable. What's the *minimum* process change — and where does over-engineering start?

**D19. Imagine the IP you're validating has a brand-new architectural feature (e.g., a new memory ordering primitive). Reason about the order in which you write tests for it.**
*Constraints to weigh:* directed tests prove the spec interpretation, formal proves the property holds, random builds confidence, scoreboard catches cross-feature interactions. Which goes first, and why does the order matter for catching the architect's spec ambiguity?

---

### Exit Check - [day23_cpu_bringup_functional_val.md](archive/day23_cpu_bringup_functional_val.md)

- [ ] Drew the 9-stage bring-up ladder from memory.
- [ ] Named one test bucket per stage.
- [ ] Stated the difference between functional, code, and assertion coverage.
- [ ] Stated what a vplan is and why the architect cares.
- [ ] Named three pre-Si tools and three post-Si tools used in CPU IP val.
- [ ] Recited the bug-cost ladder (~10× per phase) and the shift-left rule.
- [ ] Can describe in 90 sec what an IP Functional Val engineer owns end-to-end.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S05](s05_power_on_sequence.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S07](s07_registers_mmio.md)

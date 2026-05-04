# Senior Silicon Validation — Interview Revision Guide (Minimal)

> Scan-once-before-the-call sheet. One concrete example beats three textbook bullets. Lead with the *tradeoff*, then the mechanism, then the validation hook. If you don't know — say so cleanly and pivot to the closest thing you do know.
>
> Depth lives in the [21-day plan](21day_plan/README.md). This file is reflexes only.

---

## 1. Pre-call 5-min checklist

- [ ] Water, DND, notebook.
- [ ] 30-sec career narrative said out loud once.
- [ ] One bug story rehearsed (matched to lens — Section 6 table).
- [ ] One coverage-plan answer rehearsed.
- [ ] ARM↔x86 mapping ready if ARM shop.
- [ ] 3 questions for them on a sticky:
  1. Sprint split between infra / test content / debug?
  2. Where is this product/IP in its programme cycle?
  3. How does this team interact with the adjacent team (pre-Si↔post-Si, vendor↔OEM, IP↔SoC)?

### Detect lens in 60 sec

| JD cues | Lens |
|---|---|
| RTL, UVM, emulation, coverage plan, pre-tape-out, formal | **Pre-Si** |
| Bring-up, EVT/DVT, JTAG, OEM, field issue, stepping | **Post-Si** |
| Platform, end-to-end, cross-IP, SoC integration | **System** |
| NPU, ORT, EP, Hexagon HTP, OpenVINO | **NPU/AI** |

If mixed → ask: *"Pre-Si in emulation, post-Si on a board, or end-to-end on a platform?"*

---

## 2. Spine reflexes (career + role-fit)

| Theme | One-liner |
|---|---|
| Why pre-Si after post-Si | Post-Si proved I debug across the stack; pre-Si is where I apply that judgement and shape silicon before it's cast |
| Why post-Si after pre-Si | Pre-Si gave me RTL discipline; post-Si is where I see what escapes look like and feed that back upstream |
| Why system / platform | One IP teaches depth; the platform teaches which seams hide bugs no IP-level test catches |
| Day-1 contribution | Lens-relevant experience + cross-stack debug + tool-builder mindset + low-ego learner |
| Why leaving | Frame *toward* the new thing, never *away from* the old |
| Weakness | Specific gap + bridge from adjacent fluency + closing plan |

---

## 3. Methodology reflexes

| Topic | One-liner |
|---|---|
| Sporadic emul/silicon fail | Lock seed → minimise repro → bisect (RTL/FW/test) → waveform window around divergence → don't touch design until failure is deterministic and minimal |
| Coverage plan | Spec → state space → functional coverpoints + crosses → directed corners → CR fill → SVA for protocols → review with arch+design → exit criteria up front |
| Functional vs code coverage | Code = "did the line execute"; functional = "did the *intended scenario* fire". Functional is the real metric |
| Failing assertion triage | Reproduce → re-read assertion vs spec → check test preconditions → check RTL under legal stimulus → loop in arch when ambiguous; never silently disable |
| New problem | Restate in one sentence → define done → known vs assumed → cheapest disambiguating signal → minimum reproducer → write plan → timebox |
| Workload distribution | Decompose, name a DRI per stream, staff critical path first with backup, parallelise the independent, keep 20% buffer, re-plan at milestones |

### Probe-then-push answer pattern

> "First layer: **[what it is]**. It exists because **[constraint/tradeoff]**. Mechanism: **[implementation]**. Adjacent failure mode: **[bug / validation hook]**."

---

## 4. Tech-spine reflexes

| Topic | One-liner |
|---|---|
| Multi-level cache | Single cache can't be 1-cycle AND 32 MB AND shared — physics, area, contention forbid it |
| SRAM vs DRAM | 6T (fast, sparse) vs 1T1C (dense, leaks, refresh). SRAM where latency dominates, DRAM where capacity does |
| MESI | Silicon tax for one-memory illusion across N private caches. False sharing = 4th miss class |
| Virtual memory | One indirection → isolation + sharing + oversubscription. Page fault is the kernel's "do work lazily" signal |
| TLB / huge pages | Translation has its own locality, needs its own cache. Huge pages stretch TLB reach; cost = fragmentation + coarser perms |
| DMA / barriers | Descriptor → barrier → doorbell. Two ordering universes meet; cache mgmt bridges non-coherent fabrics |
| PCIe | Packet-switched layered serial network. LTSSM trains because Gbps links can't just turn on. Credits eliminate idle on lossless link |
| NVMe vs AHCI | Per-core SQ/CQ kills queue lock; MSI-X per CQ kills cross-core wakes; phase-bit polling kills IRQ at high IOPS |
| DVFS | $P_{dyn}=CV^2F$; V & F coupled by physics → near-cubic energy savings. Governor exists because HW sees load but not intent |
| Static vs dynamic power | Dynamic = $CV^2F$ (cost of toggling). Leakage = $V \cdot I_{leak}(T)$ (cost of being on, exponential in T) |
| Clock gating | RTL `if(en)` → synth → ICG cell (latch+AND, no glitch). Tiers: leaf / regional / root. Cuts dynamic, not leakage |
| Power gating | Cuts leakage by shutting the rail; pays state-save and wake cost |
| C-state ladder | Each rung has a break-even residency; entry/exit cost > savings if too short. One state can't fit short and long idles |
| System boot | POWER → ROM → PLATFORM INIT (DDR train) → BOOTLOADER → OS+INIT. Chain of trust at every hop, fuses pin the root |
| OoO + consistency | OoO extracts ILP from a serial stream; consistency model trades HW freedom for SW programmability (TSO strong, ARM weak) |
| Lock-free queues | SPSC fastest, MPMC slowest; ABA needs tagged ptrs; head/tail on separate cache lines |
| NPU vs CPU SIMD | Dedicated matmul silicon is 100–1000× more energy-efficient; cost is separate ISA, memory, address space, power domain |
| Where NPU bugs live | At boundaries — CPU↔NPU memory, VA↔IOVA, coherent↔non-coherent, runtime↔driver |
| Setup / hold | Setup = data stable before edge; hold = stable after. Setup fails scale with F/V/T; hold/CDC near reset & alignment windows |
| CDC | Single-bit level → 2-flop sync; pulse → toggle/stretcher; multi-bit → handshake or async FIFO |
| MMIO | `*(volatile uint32_t*)addr` — `volatile` makes access compiler-observable; doesn't give CPU/device ordering (need barriers) |
| Polling vs IRQ | Crossover from arrival rate: high IOPS → polling wins (no ISR/wake cost) |

### ARM ↔ x86 mapping

| ARM | x86 |
|---|---|
| RPMh | PCU |
| SMMU | IOMMU |
| ASID | PCID |
| WFI/WFE | MWAIT |
| OPP | P-state |
| PMIC over SPMI | FIVR |
| LMh | PL1/PL2 |
| EL0/1/2/3 + Secure | Ring 3/0 + SMM |
| PSCI SMC | INIT-SIPI / ACPI |
| LDAR / STLR / DMB ISH | implicit (TSO) |

---

## 5. Career narrative

### 30-second version
> "8 years silicon validation. Microsoft now — NPU platform validation Copilot+ PCs (Intel/AMD/QC), built EdgeOptimizer (per-inference attribution via ETW+WPA+RAPL). Intel before — Cambria Xeon pre-Si emulation (C-states, ADR, PCIe Gen6) on Zebu, Star of the Month for a CXL FW bug. L&T@SanDisk before — NVMe post-Si bring-up. M.Tech NIT Durgapur."

### Three current-work framings (pick to match audience)

1. **Silicon-side (pre-Si CPU/SoC):** "I run platform-level NPU validation across three vendors. I find the behaviours that only show up at platform level — graph-partitioning escapes, driver/FW/HW interaction bugs, perf regressions across DVFS/C-states — and feed them back to silicon teams as actionable bugs with reproducers."
2. **System / cross-stack:** "I own NPU validation across the full stack — app → ORT → EP → driver → FW → silicon — on Intel/AMD/QC. The interesting bugs live at layer boundaries. Built EdgeOptimizer to attribute energy/utilisation per-inference."
3. **Tooling / lead:** "I lead the platform NPU charter for Copilot+ PCs. Beyond regression and bring-up, I built the tooling — EdgeOptimizer, ETW pipelines — that the team and OEMs now use. Recent leverage = turning one-off debug into reusable infra."

### Honest framing
- IC + small-team tech lead today (4-6); growing into 10-15 person mgr.
- ARM/Snapdragon-native vocab still building; map cleanly from Intel equivalents.

---

## 6. Bug story bank

> Pattern (90 sec): *Context → Symptom → Repro → Root cause → Fix + impact → Lesson / coverage added.* End on the lesson.

| # | Bug | Lens | One-liner |
|---|---|---|---|
| 1 ⭐ | **Cambria Core C6 wake-loss** | Pre-Si | 1-deep CFC FIFO between PMA and PCU dropped a wake-ack when an opcode arrived in same cycle as snoop drain. Added `{snoop_active, irq_pending, c6_exit_state}` cross. *Would have been P0 silicon escape.* |
| 2 ⭐ | **QNN context-cache** | Post-Si NPU | Driver's invalidation key over-invalidated on benign metadata change → recompile every cold launch → 200 ms first-launch hit. Added cold-launch latency as first-class regression metric. |
| 3 ⭐ | **Silent EP fallback** | System | ORT partitioner required newer opset for an op the EP supported in older opset → EP refused subgraph silently → CPU fallback → 8× latency. Built silent-fallback detector in EdgeOptimizer. |
| 4 | **NVMe CSTS.RDY hang** | Post-Si bring-up | PHY calibration FSM raced power-good at fast V-ramp; FW added 2 ms guard delay. Rail-ramp variance now in standard bring-up matrix. |
| 5 | **CXL FW ordering** | Pre-Si subsys | FW issued config-write before credit-return on same VC; legal per spec, exposed RTL ordering assumption. Added SVA. *Star of the Month.* |

### Story → audience

| Audience | Lead with |
|---|---|
| Pre-Si CPU / RTL / emul | Bug 1 |
| Pre-Si subsystem / fabric / FW | Bug 5 |
| Post-Si platform / driver / FW | Bug 2 |
| Post-Si bring-up / characterisation | Bug 4 |
| System / cross-stack / perf regression | Bug 3 |
| "Hardest bug" (open) | Bug 1 (pre-Si) or Bug 3 (system) |
| "Most impactful" | Bug 2 (quantified OEM) or Bug 1 (would-have-been-respin) |

---

## 7. Behavioural reflexes

| Scenario | Reflex |
|---|---|
| Lead team of 15 | Tech-lead + sub-leads, daily standups, weekly 1:1s, ownership matrix, escalate fast. Today comfortable at 4-6; want 6-month ramp to 15 |
| Underperforming teammate | First conversation: **can't** (skill/blocker → pair, scope, escalate) vs **won't** (motivation → direct convo, written expectations, perf process if needed) |
| Disagree with designer | Disagree-and-commit, data-led, 1:1 first never email-war, document outcome |
| Conflict | Get the data → 1:1 first → propose solution not just problem → document |
| Strength | Cross-stack debug + tool-builder mindset |
| Weakness | Specific gap + bridge from adjacent fluency + closing plan |
| "Hardest bug" | Section 6 table |
| "Time you missed something" | Honest, short, lesson > miss |

---

## 8. Power / thermal / DVFS / C-states (concise backup)

> If asked, expand the relevant row. Don't volunteer all of this.

| Q | Answer |
|---|---|
| Why DVFS works | $P=CV^2F$; V & F coupled → drop F 10% lets V drop too → P drops ~25-30% |
| Static (leakage) | Grows ≈ exp(T); at 100°C can be >50% of total → C-state power gating cuts leakage to ~zero on gated rail |
| Tj | $T_j = T_{amb} + P \cdot \theta_{JA}$. Dried TIM doubles $\theta_{CS}$ → throttle. Tjmax 100°C consumer, 105°C server |
| Throttle reason (Intel) | `MSR_CORE_PERF_LIMIT_REASONS` 0x64F → PROCHOT / Tjmax / PL1 / PL2 / Iccmax / VR thermal. **Always step 1.** Wrong fix worse than no fix |
| Thermal-up, hold perf | Pick lever: cooling headroom, voltage binning, workload spread, hybrid migration, DVFS curve, burst+rest (PL1/PL2/Tau), gate idle IPs |
| RAPL caveat | VR-input estimate, 20-30% off wall power. Ground truth = Quarch / Monsoon |
| C-state ladder | C0 → C1 (clock-gate) → C1E → C3 (flush L1/L2, PLL off) → **C6 (arch-state to retention SRAM, core power-gated)** → C7-C10 (LLC flush, package) |
| C-state break-even | Enter only if expected idle > 2× wake latency. cpuidle governor enforces |
| ARM C-state path | WFI/WFE + PSCI `CPU_OFF`/`CPU_SUSPEND`, RPMh arbitrates rails |

---

## 9. Pre-Si CPU core (concise backup)

> Read once if interviewing pre-Si. Full depth in [Day 16](21day_plan/day16_pipeline_memory_consistency_simd.md).

**Pipeline (modern OoO):** Fetch → Decode → Rename → Dispatch → Issue (RS) → Execute (ALU/FPU/LSU/branch) → Writeback → Commit (ROB).

**Hazards:** Structural (dup or stall); Data RAW (forward), WAR/WAW (rename eliminates); Control (predict + speculate + flush). Mispredict cost ≈ depth × IPC ≈ 15-20 cycles.

**Memory subsystem:** L1 (~4c) → L2 (~12c) → L3/SLC (~30c) → DRAM (~100 ns). MESI / MOESI (Arm). CHI = Arm on-chip coherence. Snoop filter to avoid broadcast.

**Consistency:** x86 TSO (only store→later-load reorders). ARMv8 weak — ordered by `DMB ISH` / `DSB ISH` / `ISB` / `LDAR`-`STLR`.

**Common pre-Si bug shapes:** mispredict recovery doesn't drain LSU → stale forward; renamer free-list leak → deadlock; WAW collision wide-issue; forwarding from flushed uop → silent corruption; exception in shadow of mispredict → ROB unwind misorders; CDC missing sync → metastability; clock-gate too eager → flop holds wrong value.

**Bring-up canonical sequence:** reset/clock smoke (no X's) → trivial loop (PC advance) → exception path → MMU on (identity map) → cache on (coherence) → 2nd core (SMP, IPI, spinlock) → SMMU/IO coherence → full Linux/Android boot. Each step gates the next.

**Tools to name-drop:** VCS / Xcelium / Riviera; Verdi (FSDB); Zebu / Palladium / Veloce; HAPS / Protium; JasperGold / VC Formal; UVM, SVA, Perspec; Arm FastModel / Spike.

---

## 10. NPU post-Si (concise)

**Stack:** App → ORT/DirectML → EP (QNN/OpenVINO/XDNA) → vendor driver+FW → NPU silicon (Hexagon HMX+HVX) → DDR / VTCM SRAM.

**Canonical bugs:** silent EP fallback (Bug 3); context-cache over-invalidation (Bug 2); page-fault spike usually NOT silicon — almost always allocator/driver layer.

**Where bugs live:** boundaries — CPU↔NPU memory, VA↔IOVA, coherent↔non-coherent, runtime↔driver. Each side correct in isolation; the contract is fragile.

**Honest framing:** "Platform-side NPU validation — graph partitioning, EP fallback, energy/inference, version-matrix regression. Not Hexagon RTL."

---

## 11. EE fundamentals (only if asked)

| Topic | Reflex |
|---|---|
| CMOS dynamic power | $\approx CV^2F$ |
| Short-circuit power | Brief current during switching when both devices partially conduct |
| Leakage | Strong with T and process corner |
| Clock gating | Cuts dynamic; not leakage |
| Power gating | Cuts leakage; pays state-save + wake cost |
| Setup / hold | See Section 4 |
| Metastability | 2-flop sync reduces probability, not to zero |
| PCIe SI | LTSSM stuck Polling/Recovery → check channel, refclk, equalization, reset, lane polarity *before* protocol debug |
| Eye diagram | Open eye = margin; closed = bit errors |

---

## 12. Puzzles

| Puzzle | Reflex |
|---|---|
| 2-egg / N-floor | $X(X+1)/2 \geq N$ → first drop = 14 for N=100, worst case 14 |
| Sum of 5 odd nums = 30 | Impossible by parity |
| $x^y$ vs $y^x$ | Compare $\ln x / x$ vs $\ln y / y$; peaks at $e$. Both ≥ 3 → smaller base wins. Tie: $2^4=4^2$ |
| Burning rope | Light both ends of one + one end of other → 45 min from two 1-hr ropes |

---

## 13. Final breath

- Lens-relevant depth + adjacent breadth + low-ego learner.
- Tradeoff first, mechanism next, validation hook last.
- One concrete example > three textbook bullets.
- Don't know? Say so cleanly, pivot to the closest thing you do know.
- Let them drill if they want depth.

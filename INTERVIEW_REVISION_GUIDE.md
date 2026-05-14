# Silicon Validation — 30-Minute Pre-Interview Skim

> Generic reflex sheet. Read once in the 30 minutes before any silicon validation / SoC / pre-Si / post-Si / NPU interview. Topic pointers and one-liners only — depth lives in the [21-day plan](21day_plan/README.md). Personal narrative + per-company tailoring lives in [day21_final_mock_company_tailoring.md](21day_plan/day21_final_mock_company_tailoring.md) (Section 3 + Section "Personal Narrative").
>
> Rule: **lead with the tradeoff, then the mechanism, then the validation hook.** One concrete example beats three textbook bullets. Don't know? Say so cleanly and pivot to the closest thing you do know.

---

## 1. Detect the lens in 60 sec

| JD / question cues | Lens | What they want |
|---|---|---|
| RTL, UVM, coverage, formal, pre-tape-out, emulation, FSDB | **Pre-Si** | DV/coverage rigor, ordering bugs, simulator/emul fluency |
| Bring-up, EVT/DVT, JTAG, OEM, stepping, field issue | **Post-Si bring-up** | Real-silicon debug, lab tools, characterisation |
| Platform, end-to-end, cross-IP, integration, driver/FW | **System** | Cross-stack triage, layer attribution |
| NPU, ORT, EP, Hexagon, OpenVINO, XDNA, DirectML | **NPU/AI** | Graph partition, EP fallback, energy/inference |
| Power, thermal, DVFS, C-state, RAPL, MSR | **PnP** | Tj math, throttle reasons, residency, governor logic |

If mixed → ask: *"Pre-Si in emulation, post-Si on a board, or end-to-end on a platform?"*

---

## 2. Default answer pattern (any technical question)

> "First layer: **[what it is]**. It exists because **[constraint / tradeoff]**. Mechanism: **[implementation]**. Adjacent failure mode / validation hook: **[bug class or counter]**."

Four sentences, then stop. Let them drill.

### Behavioural questions (90-sec STAR)

*Context → Symptom → Repro → Root cause → Fix + impact → Lesson / coverage added.* End on the lesson, not the fix.

---

## 3. Tech-spine reflexes (one-liners)

| Topic | One-liner |
|---|---|
| Multi-level cache | One cache can't be 1-cycle AND 32 MB AND shared — physics, area, contention forbid it |
| SRAM vs DRAM | 6T (fast, sparse) vs 1T1C (dense, leaks, refresh). SRAM where latency dominates, DRAM where capacity does |
| MESI | Silicon tax for one-memory illusion across N private caches. False sharing = 4th miss class |
| MOESI / MESIF | AMD adds Owner (dirty forward without writeback); Intel adds Forwarder (one S-copy responds) |
| Virtual memory | One indirection → isolation + sharing + oversubscription. Page fault = kernel's "do work lazily" signal |
| TLB / huge pages | Translation has its own locality, needs its own cache. Huge pages stretch TLB reach; cost = fragmentation + coarser perms |
| IOMMU / SMMU | Device-side MMU. IOVA→PA translation, per-BDF context, isolates VMs/containers from rogue DMA |
| DMA / barriers | **Descriptor → barrier → doorbell.** Two ordering universes meet; cache mgmt bridges non-coherent fabrics |
| Posted vs non-posted | MWr posted (fire-forget); MRd / Cfg non-posted (wait for completion). Doorbell = MWr by design |
| PCIe layers | PHY (8b/10b → 128b/130b) / DLL (ACK/NAK, credits) / TL (TLPs). LTSSM trains because Gbps links can't just turn on |
| LTSSM checklist | Stuck in Polling → check refclk, terminations, lane polarity *before* protocol debug |
| NVMe vs AHCI | Per-core SQ/CQ kills queue lock; MSI-X per CQ kills cross-core wakes; phase-bit polling kills IRQ at high IOPS |
| NVMe bring-up | read CAP → disable (CC.EN=0) → set AQA/ASQ/ACQ → enable → poll CSTS.RDY → identify → create I/O queues |
| DVFS | $P_{dyn}=CV^2F$. V & F coupled by setup-time physics → near-cubic energy savings |
| Ramp order | **V before F when ramping up; F before V when ramping down.** Reverse = setup-timing violation |
| Static (leakage) power | $V \cdot I_{leak}(T)$, exponential in T. >50% of total at 100°C without gating |
| Clock gating | RTL `if(en)` → ICG cell. Cuts dynamic, not leakage. Leaf / regional / root tiers |
| Power gating | Cuts leakage by shutting the rail; pays state-save + wake cost |
| C-state ladder | C0 → C1 (clock-gate) → C1E → C3 (flush L1/L2, PLL off) → **C6 (state→retention SRAM, core power-gated)** → C7-C10 (LLC flush, package) |
| C-state break-even | Enter only if expected idle > 2× wake latency. cpuidle governor enforces |
| PkgC6 / S0ix | Needs *every* uncore IP idle (LLC, MC, PCIe in L1.2, display, INTC). One IP awake → blocks pkg state |
| Tj | $T_j = T_a + P \cdot \theta_{JA}$. Tjmax 100°C consumer, 105°C server. Dried TIM doubles $\theta_{CS}$ → throttle |
| Throttle reason (Intel) | `MSR_CORE_PERF_LIMIT_REASONS` 0x64F. **Always step 1.** Wrong fix worse than no fix |
| RAPL caveat | VR-input estimate, 20-30% off wall power. Ground truth = Quarch / Monsoon |
| OoO + ROB | Extracts ILP from serial stream. Mispredict cost ≈ depth × IPC ≈ 15-20 cycles |
| Memory consistency | x86 = TSO (only store→later-load reorders). ARMv8 = weak (need DMB ISH / LDAR-STLR) |
| MMIO | `*(volatile uint32_t*)addr`. `volatile` makes access compiler-observable; *not* a CPU/device-ordering barrier |
| `volatile` (3 things it prevents) | caching in register, reordering of accesses to that address, eliding the access. Does **not** imply atomicity or barrier |
| W1C status bits | Write-1-to-clear; never RMW or struct-overlay copy (silently clears every set bit) |
| Polling vs IRQ | High event rate / hot path → polling wins (no ISR/wake cost). NVMe CQ does both — polled hot, IRQ cold |
| Lock-free SPSC vs MPMC | SPSC needs no CAS; MPMC does. ABA → tagged ptr / hazard ptr / RCU. head/tail on separate cache lines |
| Atomics ordering | Pick weakest correct: relaxed (counters), acq/rel (locks, producer-consumer), seq_cst (rare) |
| Setup / hold | Setup = data stable *before* edge; hold = stable *after*. Setup fails scale with F/V/T; hold/CDC near reset & alignment windows |
| CDC | Single-bit level → 2-FF sync; pulse → toggle/stretcher; multi-bit → handshake or async FIFO. 2-FF reduces metastability prob, doesn't eliminate it |
| NPU vs CPU SIMD | Dedicated matmul silicon = 100–1000× more energy-efficient. Cost: separate ISA, memory, address space, power domain |
| Where NPU bugs live | Boundaries — CPU↔NPU memory, VA↔IOVA, coherent↔non-coherent, runtime↔driver. Each side correct in isolation |
| EP fallback | One unsupported op → ORT splits subgraph → CPU EP runs it → 5-10× latency from tensor copies. **The silent perf cliff.** |
| QDQ fusion | `Quantize → Op → Dequantize` collapsed to single INT8 op. If it doesn't fuse, you run FP32 → silent regression |
| Boot chain | POWER → ROM → PLATFORM INIT (DDR train) → BOOTLOADER → OS+INIT. Chain of trust at every hop, fuses pin the root |

### ARM ↔ x86 vocabulary mapping

| ARM | x86 | What it does |
|---|---|---|
| RPMh | PCU | Power-management coordinator |
| PSCI SMC | INIT-SIPI / ACPI | OS↔FW power state interface |
| SMMU | IOMMU / VT-d | Device-side address translation |
| ASID | PCID | TLB tag for context |
| WFI / WFE | MWAIT | Idle hint |
| OPP | P-state | DVFS operating point |
| PMIC over SPMI | FIVR (integrated) | Voltage regulation |
| LMh | PL1 / PL2 / Tau | Power-limit governor |
| EL0/1/2/3 + Secure | Ring 3/0 + SMM + TrustZone | Privilege levels |
| LDAR / STLR / DMB ISH | implicit (TSO) | Acquire/release semantics |
| GIC + ITS | IOAPIC + MSI-X | Interrupt routing |
| big.LITTLE / DynamIQ | Hybrid (P-core/E-core) | Heterogeneous core topology |

---

## 4. Methodology reflexes

| Scenario | Reflex |
|---|---|
| Sporadic emul / silicon fail | Lock seed → minimise repro → bisect (RTL/FW/test) → waveform window around divergence → don't touch design until failure is deterministic and minimal |
| Coverage plan | Spec → state space → functional coverpoints + crosses → directed corners → CR fill → SVA for protocols → review with arch+design → exit criteria up front |
| Functional vs code coverage | Code = "did the line execute"; functional = "did the *intended scenario* fire". Functional is the real metric |
| Failing assertion triage | Reproduce → re-read assertion vs spec → check test preconditions → check RTL under legal stimulus → loop in arch when ambiguous; **never silently disable** |
| New problem | Restate in one sentence → define done → known vs assumed → cheapest disambiguating signal → minimum reproducer → write plan → timebox |
| Workload distribution (lead) | Decompose, name a DRI per stream, staff critical path first with backup, parallelise the independent, keep 20% buffer, re-plan at milestones |
| 7-step debug framework | Observe → reproduce → isolate → hypothesise → test → root-cause → fix |
| Bug classification by repro rate | 100% = functional/FW; intermittent (<50%) = race/timing; corner-dependent + multi-unit = PVT; single-unit = defect |
| Cost-per-stage rule | RTL → emul → FPGA → post-Si is roughly 10× per stage. Catch left, debug right |
| "Never blame silicon first" | Cross-check FW, driver, OS, runtime version matrix before escalating to design. The escalation has a reputational cost |

---

## 5. Pre-Si CPU core (read once if pre-Si lens)

**Pipeline (modern OoO):** Fetch → Decode → Rename → Dispatch → Issue (RS) → Execute (ALU/FPU/LSU/branch) → Writeback → Commit (ROB).

**Hazards:** Structural (dup or stall); Data RAW (forward), WAR/WAW (rename eliminates); Control (predict + speculate + flush).

**Memory subsystem typical latencies:** L1 ~4c → L2 ~12c → L3/SLC ~30c → DRAM ~100 ns.

**Coherence:** MESI / MOESI (Arm). CHI = Arm on-chip. Snoop filter avoids broadcast at high core counts. Directory-based coherence past ~16 cores.

**Common pre-Si bug shapes:**

- Mispredict recovery doesn't drain LSU → stale forward.
- Renamer free-list leak → deadlock under pressure.
- WAW collision on wide-issue when both writers hit same physreg slot.
- Forwarding from a flushed uop → silent corruption.
- Exception in shadow of mispredict → ROB unwind misorders.
- CDC missing sync → metastability → 1-in-N functional fail.
- Clock-gate too eager → flop holds wrong value across gate window.

**Bring-up canonical sequence:** reset/clock smoke (no X's) → trivial loop (PC advance) → exception path → MMU on (identity map) → cache on (coherence) → 2nd core (SMP, IPI, spinlock) → SMMU/IO coherence → full Linux/Android boot. Each step gates the next.

**Tools to name-drop only if asked:** VCS / Xcelium / Riviera; Verdi (FSDB); Zebu / Palladium / Veloce; HAPS / Protium; JasperGold / VC Formal; UVM, SVA, Perspec; Arm FastModel / Spike.

---

## 6. Post-Si NPU (read once if NPU lens)

**Six-layer stack:** App → ONNX Runtime / DirectML → EP (QNN / OpenVINO / XDNA) → vendor driver + FW → NPU silicon (HMX + HVX, or equivalents) → DDR + local SRAM (VTCM on Hexagon).

**Where bugs live (boundary list):**

- ORT ↔ EP: opset mismatch → silent fallback to CPU.
- EP ↔ vendor compiler: QDQ fusion failure → INT8 op runs as FP32.
- Vendor compiler ↔ runtime: tile-size cliff → DMA storms.
- Driver ↔ FW: context-cache invalidation key over-broad → recompile every cold launch.
- FW ↔ silicon: scheduler stall on shared queue (e.g. one model starves another).
- Runtime ↔ OS: keep-alive pins device awake → blocks PkgC10 / S0ix.

**Canonical post-Si NPU bug classes:** silent EP fallback; context-cache regression (cold-launch latency); page-fault spike (almost always allocator/driver, not silicon); single-queue starvation; thermal-throttle coupling to a data path; DLL-version-matrix regression (EP × ORT × SDK × model build).

**Honest framing:** "Platform-side NPU validation — graph partitioning, EP fallback, energy/inference, version-matrix regression. Not vendor RTL."

---

## 7. Power / thermal / DVFS / C-states (concise backup)

| Q | Reflex |
|---|---|
| Why DVFS works | $P=CV^2F$; F drop 10% → V can drop too (setup-slack) → P drops ~25-30% |
| Static (leakage) | Grows ≈ exp(T); at 100°C >50% of total → C-state power-gating cuts leakage to ~0 on gated rail |
| Throttle reason (Intel) | `MSR_CORE_PERF_LIMIT_REASONS` 0x64F: PROCHOT / Tjmax / PL1 / PL2 / Iccmax / VR thermal |
| Hold perf under thermal | Pick lever: cooling headroom, V binning, workload spread, hybrid migration, DVFS curve, burst+rest (PL1/PL2/Tau), gate idle IPs |
| C-state break-even | $T_{residency} > (E_{enter} + E_{exit}) / \Delta P$. Ping-pong below break-even is net energy loss |
| ARM C-state path | WFI/WFE + PSCI `CPU_OFF`/`CPU_SUSPEND`, RPMh arbitrates rails over SPMI to PMIC |
| ADR (server) | Loss-of-power → IO quiesce ack → fabric quiesce → WPQ flush ack → DRAM self-refresh → NVDIMM save. Order matters; ack matters |
| eADR | LLC writeback added to ADR sequence — makes the cache persistent under power loss |
| Top-down (TMA) | Frontend bound / Bad spec / Retiring / Backend (Memory or Core). 4 buckets by what cycles spent |

---

## 8. Hardware-C / coding-round reflexes

**Edge-case pre-amble (always say this *before* writing code):** *"Before I code, the cases I'll handle are: NULL/empty input, single element, max size / overflow, boundary (first/last), and the spec'd error path. I'll mention any I'm choosing to skip."* This 15-second opener separates "works" from "clean" — lost a Cerebras edge-case round to skipping it.

| Topic | Reflex |
|---|---|
| `BIT(n)` | `(UINT32_C(1) << (n))` — `UINT32_C` avoids `1 << 31` UB |
| `MASK(msb,lsb)` | `(UINT32_MAX >> (31 - (msb-lsb))) << lsb` — never `(1<<32)-1` (UB) |
| `FIELD_PREP(v,msb,lsb)` | `((v) << lsb) & MASK(msb,lsb)` — mask **after** shift |
| `reg_read32 / reg_write32` | typed `volatile uint32_t *`; don't cast struct overlay onto MMIO |
| `wait_for_bit_set` | mask + expected + timeout + return rc. Never bare `while(!(*reg & bit));` |
| RMW pattern | `*reg = (*reg & ~mask) | val` — never on W1C status |
| Endianness | LE on PCIe wire. Parse byte-by-byte (`read_le32`); never cast `uint8_t*` → `uint32_t*` |
| Aligned alloc | over-alloc by `align-1+sizeof(void*)`, round up, stash original ptr one slot before |
| `container_of` | `(type*)((char*)ptr - offsetof(type, member))` |
| Linked list reverse | 3-pointer dance: `prev / curr / next`. O(n)/O(1) iterative; recursion costs stack |
| Floyd cycle detect | tortoise/hare; constant memory; finds *whether* and *start* |
| LRU O(1) | hash → doubly-linked list; move-to-front on get; evict tail on put |
| Binary search safe mid | `lo + (hi - lo) / 2` (overflow-safe) |
| Spinlock | TTAS + `acquire` on CAS, `release` on store. Never hold across a sleep |
| SPSC ring | head/tail on **separate cache lines**; load tail relaxed, head acquire on push; no CAS needed |
| MMIO doorbell | descriptor write → barrier → tail update → MMIO doorbell. Order is the spec |
| Moving average (stream) | circular buffer + running sum, O(1)/sample. Re-sum every K samples to fight float drift |
| `grep` clone (Python) | compile pattern once, stream lines (don't slurp), `-i/-n/-r/-v`, handle binary + bad regex + missing file |

---

## 9. EE / silicon fundamentals (only if asked)

| Topic | Reflex |
|---|---|
| CMOS dynamic power | $\approx C V^2 F$ |
| Short-circuit power | Brief crowbar current when both N and P partially conduct during switching |
| Leakage | Sub-Vth + gate + junction; strong with T and process corner |
| Clock gating | Cuts dynamic; not leakage |
| Power gating | Cuts leakage; pays state-save + wake cost |
| Setup / hold | See Section 3 |
| Metastability | 2-FF sync reduces probability, never to zero — MTBF math |
| PCIe SI | LTSSM stuck Polling/Recovery → channel, refclk, equalisation, reset, lane polarity *before* protocol debug |
| Eye diagram | Open eye = margin; closed = bit errors. Width = jitter, height = noise |
| BER vs link-up | Spec is BER ≤ 1e-12 post-FEC; link-up does not imply healthy margin |

---

## 10. Behavioural / leadership reflex prompts

| Scenario | Reflex |
|---|---|
| "Tell me about yourself" | 60-90 sec: current role → past experience → why this role. End on the *forward-looking* sentence |
| "Hardest bug" | One STAR story, ≤90 sec. Lead with the symptom, end on the lesson |
| "Time you missed something" | Honest, short, lesson > miss. Show the process change |
| "Disagree with a designer" | Disagree-and-commit, data-led, 1:1 first never email-war, document outcome |
| "Underperforming teammate" | Diagnose **can't** (skill/blocker → pair, scope, escalate) vs **won't** (motivation → direct convo, written expectations, perf process if needed) |
| "Lead a team of N" | Tech-lead + sub-leads, daily standups, weekly 1:1s, ownership matrix, escalate fast |
| "Strength" | Cross-stack debug + tool-builder mindset (or your equivalent — pick one and commit) |
| "Weakness" | Specific gap + bridge from adjacent fluency + closing plan. Never "I work too hard" |
| "Why this role / company" | 4-sentence template: what I do now → what I want → why my background fits → why **this** company specifically (research one product line) |
| "Why switching again" / "left Intel, why come back" | Honest arc: what I went to learn (NPU runtime + edge tooling at MS) → what I built (EdgeOptimizer, cross-vendor NPU validation) → why now (consolidate that breadth back into deep silicon validation). Don't apologise for the move; frame it as a loop that closed |
| "Most work is prep, only 5-10% translates to CV" pushback | Agree without flinching: that's the nature of validation — the 90% is what *prevents* the bug; the 10% that ships is the visible tip. The skill is owning the 90% so the 10% lands. Cite one prep-heavy effort (PM coverage matrix / ADR characterisation) that *became* the bug story |
| "No CPU-core experience — why hire you / value-add?" | Three things: (1) validation rigor + system view from post-Si platform + NPU — transfers cleanly to core val; (2) cross-vendor exposure (Intel uncore, Qualcomm Hexagon, Arm SoCs) gives me the *contrast* most core-only engineers lack; (3) ownership pattern — own end-to-end, escalate fast, write the tool. Close on hard work + dedication — that framing is what's being listened for |
| "If switching for money, don't come" / cross-offer probe | Lead with fit, not leverage. "I'm choosing for engineering scope and the team I'd learn from" — then if pressed on a specific other offer (Arm vs QC), pick on *technical reason* ("QC's CPU + NPU + modem stack gives a wider system surface to validate against"), never on comp |

---

## 11. Pre-call 5-min checklist

- [ ] Water, DND, notebook, pen.
- [ ] 30-sec career narrative said out loud once.
- [ ] One bug story rehearsed (matched to the lens — Section 1 table).
- [ ] One coverage-plan / triage answer rehearsed.
- [ ] ARM↔x86 mapping (Section 3) re-skimmed if ARM shop / vice versa.
- [ ] 3 questions for them on a sticky:
  1. Sprint split between infra / test content / debug?
  2. Where is this product / IP in its programme cycle?
  3. How does this team interact with the adjacent team (pre-Si↔post-Si, vendor↔OEM, IP↔SoC)?

---

## 12. Common puzzles (drop only if asked)

| Puzzle | Reflex |
|---|---|
| 2-egg / N-floor | $X(X+1)/2 \geq N$ → first drop 14 for N=100, worst case 14 |
| Sum of 5 odd nums = 30 | Impossible by parity |
| $x^y$ vs $y^x$ | Compare $\ln x / x$ vs $\ln y / y$; peaks at $e$. Both ≥ 3 → smaller base wins. Tie: $2^4 = 4^2$ |
| Burning rope (1 hr each) | Light both ends of one + one end of other → 45 min |
| 100 prisoners / boxes | Cycle-following strategy → ~31% survival, vs $2^{-100}$ random |

---

## 13. Final breath (10 seconds before joining the call)

- Lens-relevant depth + adjacent breadth + low-ego learner.
- **Tradeoff first, mechanism next, validation hook last.**
- One concrete example > three textbook bullets.
- Don't know? Say so cleanly, pivot to the closest thing you do know.
- Let them drill if they want depth; don't volunteer the whole map.

---

## Pointers — where to go deep if a topic surfaces

| Topic | Day file |
|---|---|
| C / pointers / address fluency | [Day 0](21day_plan/day00_c_basics.md) – [Day 2](21day_plan/day02_pointers_memory_layout_alignment.md) |
| Bits / MMIO / volatile / polling | [Day 3](21day_plan/day03_bits_mmio_volatile_polling.md) |
| Cache / locality / AMAT | [Day 4](21day_plan/day04_cache_lines_locality_amat.md) |
| MESI / coherence / false sharing | [Day 5](21day_plan/day05_coherence_mesi_false_sharing.md) |
| Virtual memory / page faults | [Day 6](21day_plan/day06_virtual_memory_page_faults.md) |
| TLB / huge pages / IOMMU / SMMU | [Day 7](21day_plan/day07_tlb_huge_pages_iommu_smmu.md) |
| DMA / doorbells / barriers | [Day 8](21day_plan/day08_dma_doorbells_ordering_barriers.md) |
| ARM / Qualcomm platform vocab | [Day 9](21day_plan/day09_arm_qualcomm_platform_vocabulary.md) |
| PCIe + LTSSM | [Day 10](21day_plan/day10_pcie_fundamentals_ltssm.md) |
| NVMe queues + bring-up | [Day 11](21day_plan/day11_nvme_queues_bringup.md) |
| DVFS / P-states / PVT | [Day 12](21day_plan/day12_dvfs_pstates_pvt.md) |
| C-states / PkgC / S0ix | [Day 13](21day_plan/day13_cstates_pkgc_s0ix.md) |
| ADR / PCU / Core C6 bugs | [Day 14](21day_plan/day14_adr_pcu_core_c6_bugs.md) |
| Thermal / RAPL / TMA / multi-IP | [Day 15](21day_plan/day15_thermal_power_perf_counters.md) |
| Pipeline / memory model / SIMD | [Day 16](21day_plan/day16_pipeline_memory_consistency_simd.md) |
| Concurrency / lock-free / atomics | [Day 17](21day_plan/day17_concurrency_queues_dsa.md) |
| NPU stack / EP / Hexagon HTP | [Day 18](21day_plan/day18_npu_ort_ep_hexagon_htp.md) |
| NPU memory / runtime / bugs | [Day 19](21day_plan/day19_npu_memory_runtime_bugs.md) |
| Debug method + 90-sec bug stories | [Day 20](21day_plan/day20_debug_method_bug_polish.md) |
| Per-company tailoring + personal narrative | [Day 21](21day_plan/day21_final_mock_company_tailoring.md) |

Coding exercises: [coding/EXERCISES.md](coding/EXERCISES.md).

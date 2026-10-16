# Revision Sweep — 42-Day Flashcard Index

> **One-minute-per-day spaced-repetition sweep.** Cover the answer, say it out loud, check. Run the whole sheet the night before any conversation, or one phase per day as continuous review. This is the condensed recall layer over the full [day files](README.md); the timed mock + delivery drill is [Day 41](archive/day41_final_mock.md).
>
> **Framing:** Intel IP-Val is the **current** role ([Day 40](archive/day40_intel_role_narrative.md)); Microsoft is the prior chapter. **Active focus = CPU-PM debug** (Phases 6 + D35/D37, supported by D11–D14 CDC/wake races) — sweep those weekly; others on demand. **Bold = a Broadcom-R2 gap.**

---

## Phase 0 — C & Programming Foundations (D0–D4)

- **D0 C basics** — `memcpy` vs `memmove` (overlap); struct padding/alignment; stack vs heap; pointer-to-pointer.
- **D1 address fluency** — `BIT/MASK/FIELD_GET/FIELD_PREP`; typed MMIO accessors; bounded poll-with-timeout.
- **D2 pointers/layout** — 5 segments (text/data/bss/heap/stack); `container_of` via `offsetof`; padding is field-order-driven.
- **D3 bits/MMIO/volatile** — `volatile` prevents 3 (cache/elide/reorder-vs-volatile), not memory-barrier/atomic/flush; **W1C** RMW silently clears status; descriptor→barrier→doorbell on weak ISAs.
- **D4 bit-manip** — **`x && !(x&(x-1))` = pow2; floor-pow2 = smear then `x-(x>>1)`; next = `x--`,smear,`+1`**; popcount Kernighan; **widen before `<<32`**; find-first-set = priority encoder; XOR-all finds unique/missing.

## Phase 1 — Digital Design (D5–D10)

- **D5 combinational** — comb vs seq = "depends on history?"; K-map (Gray order, group powers of 2, don't-cares); De Morgan; **XOR from 4 NANDs**; MUX universal (Shannon); priority vs plain encoder; **incomplete case → inferred latch**; glitch fatal on clock/reset/CDC.
- **D6 arithmetic** — ripple (∝N) vs CLA (∝log N, G/P) vs prefix; **Booth + Wallace tree + final prefix adder**; two's-comp overflow = cin≠cout of MSB; **FP: non-associative, NaN≠NaN, never `==`**; bf16 (fp32 range) vs fp16; fixed-point `Qm.n` shift+round+saturate.
- **D7 sequential** — latch (level) vs FF (edge, master-slave); setup/hold/Tcq aperture; **`fmax = 1/(Tcq+Tcomb+Tsu+Tskew)`**; **hold has no `Tclk` → slowing clock can't fix it** (add delay/skew); counters (binary/ring/Johnson/Gray); LFSR (XOR taps, all-zeros lockup); nonblocking-seq/blocking-comb.
- **D8 timing/STA** — exhaustive/vectorless; 4 path types (reg→reg, in→reg, reg→out, in→out); slack = required−arrival, WNS/TNS; **setup=slow corner, hold=fast corner**; exceptions: multicycle / false / **CDC (`set_clock_groups -async`)**; false-pathing a real path = silicon escape; close setup first, then hold.
- **D9 FSM** — 7-step recipe; **`101` overlapping detector** (longest-suffix back-edge → `10101` fires twice); Moore (registered/glitch-free) vs Mealy (fewer states/can glitch); encoding binary/one-hot(`$onehot`)/Gray; safe FSM `default→reset`.
- **D10 Verilog/SV** — 4-state (X=unknown, Z=hi-Z); **X-propagation** traces to uninit/contention/no-reset; **blocking in clocked block collapses shift reg**; `always_comb/_ff` catch latch/reset bugs; sim ⊅ synth; clocking block pins TB sample/drive; Verdi drivers/loads + FSM view.

## Phase 2 — Clocking, Reset & CDC (D11–D14) · *biggest gap*

- **D11 clocking/reset** — PLL (PFD→charge-pump→VCO→÷fb), lock+jitter; CTS balances **skew (trades setup vs hold)**; **glitchless clock mux** (old clock low before new starts → no runt); **reset: async assert / sync de-assert**, one synchronizer per domain; reset sequencing (clock/config before enable); RDC.
- **D12 metastability** — setup/hold violation → hilltop, unbounded resolve; **`MTBF = e^(t_r/τ)/(T0·f_clk·f_data)`, lever = `t_r`**; **2-FF synchronizer** (gives ~1 cycle), 3rd at high freq; **can't carry: bus (skew), pulse (lost), correlated signals**; no logic between flops, no FF1 fanout.
- **D13 CDC techniques** — taxonomy: **level→2FF, pulse→toggle+edge-detect/handshake, bus→req/ack or async FIFO, counter→Gray, reset→reset-sync**; open-loop toggle loses back-to-back → closed-loop handshake (the back-to-back-wake bug); **RDC ≠ CDC** (reset-domain mismatch).
- **D14 async FIFO** — dual-clock RAM, **Gray pointers** (1-bit change → mid-flight sample legal); **extra MSB** distinguishes full/empty; full early/empty late = safe; **depth = 32** (equal avg rates → burst-backlog, not rate); CDC sign-off = structural lint + RDC + metastability injection (sim can't show it).

## Phase 3 — Memory Systems (D15–D16)

- **D15 memory/ECC** — **address selects *where*, data is *what*** (`2^N×W`); **6T SRAM** (fast, no refresh) vs **1T1C DRAM** (dense, refresh, destructive-read); decode: precharge→row decoder→wordline→sense amp→column mux; `[bank|row|col|byte]`; open vs closed page; **Hamming SECDED** (check bits at pow-2 positions; syndrome locates 1-bit; overall parity flags 2-bit); MBIST march faults.
- **D16 cache** — $\text{AMAT}=T_{hit}+\text{miss-rate}\times\text{penalty}$; tag|set|offset; associativity; 3 C's (compulsory/capacity/conflict); write-back vs write-through.

## Phase 4 — CPU Architecture (D17–D23)

- **D17 MESI** — 4 states; write-from-Invalid → RFO + invalidate others; false sharing = two vars same line ping-pong; snoop filter.
- **D18 virtual memory** — 4-level x86-64 walk; PTE bits (P/RW/US/A/D/NX); page fault types; COW.
- **D19 TLB/IOMMU** — TLB coverage; shootdown via IPI (ARM TLBI broadcast); IOMMU/SMMU/VT-d translates device DMA (the QNN bug surface).
- **D20 DMA/barriers** — descriptor → barrier → doorbell → DMA → MSI; barrier publishes descriptor before doorbell; fails at line rate without it.
- **D21 pipeline** — 5-stage; hazards structural/data(RAW/WAR/WAW)/control + mitigations; OoO (RAT/ROB/RS/LSQ); mispredict flush; prefetcher can hide a SW bug.
- **D22 consistency** — TSO vs weak; acquire/release/seq-cst in one sentence each; LDAR/STLR; SPSC ring needs acq/rel.
- **D23 CPU bring-up** — reset→clocks→BIST→MMU→cache→INT→timers→SMP→boot; **who triggers startup: reset controller releases reset → PC to vector → Boot ROM**; vplan = spec→test→coverpoint→status; coverage axes.

## Phase 5 — Verification Engineering (D24–D25) · *parked — interview-only, skip in the weekly sweep*

- **D24 vplan/coverage** — **8-step scaffold: Spec→Features→Stimulus→Checks→Coverage→Corners→Perf→Platform**; **functional vs code coverage (100%-code/40%-functional trap)**; **deliver a 16-port 1.6T switch plan via the scaffold, not ad-hoc**; directed vs CRV vs formal.
- **D25 testbench/debug** — driver/monitor/scoreboard/ref-model; **self-checking** (else verifies nothing); **functional vs error/negative tests** (assert graceful handling); SVA immediate/concurrent/`cover` (`req|->##[1:8]grant`, `$onehot`); **debug from first divergence**, log seeds, regression test per fix.

## Phase 6 — CPU Power Management (D26–D31)

- **D26 DVFS** — $P=\alpha CV^2f$; V-before-F up, F-before-V down (setup-slack); Turbo; FF/SS corners.
- **D27 C-states** — C0–C6/C10 what's off; **PkgC6 blockers** (all cores C6, all uncore idle, PCIe L1.2); S0ix; exit latency.
- **D28 gating/UPF** — ICG (latch-based, no glitch) vs plain AND; header/footer FET; retention flops; UPF (power/iso/retention/level-shifter); SAVE→ISO→CLAMP→POWER_OFF.
- **D29 thermal/counters** — $T_j=T_a+P\theta_{JA}$; PROCHOT; RAPL; throttle-reason MSR; TMA top-down; EDP.
- **D30 pre-Si PM val** — coverage axes (state × transition × residency); UPF assertions; low-power sim; Perspec.
- **D31 post-Si PM val** — residency, MSR sweeps, governors; **ADR opcode-ordering hang + Core C6 wake-loss (CFC FIFO race)** = CDC structure bugs (D12–14).

## Phase 7 — Interconnect & PCIe (D32–D38)

- **D32 AXI/CHI** — **AXI 5 channels (AW/W/B/AR/R), VALID/READY, VALID must not wait on READY (deadlock)**; same-ID ordered / cross-ID reorderable; INCR/WRAP/FIXED, 4KB boundary; **CHI = MESI at interconnect scale** (RN/HN/SN, snoop-filter directory).
- **D33 PCIe + LTSSM** — 3 layers, TLPs, ECAM; LTSSM Detect→Polling→Config→L0→Recovery→L1.x; Gen6 FLIT/L0p.
- **D34 enum/config** — depth-first enum; BAR sizing (write all-1s, read, invert+1); capability chain at 0x34; MSI vs MSI-X.
- **D35 PCIe PM** — ASPM L0s/L1/L1.1/L1.2 (what each gates); D0–D3hot/D3cold; CLKREQ#; PME; LTR.
- **D36 PCIe bring-up** — link-train debug, LTSSM stuck-state ladder; equalization 4 phases; AER (corr/non-fatal/fatal); eye/SI; analyser.
- **D37 PCIe×PM** — **PkgC6 blocked if PCIe not in L1.2**; root-complex PM; ADR's PCIe quiesce; L0p mid-FLIT bug.
- **D38 NVMe** — SQ/CQ + doorbells, CQ phase bit; PRP vs SGL; APST × L1.2; SDExpress.

## Phase 8 — Reasoning, Narrative & Wrap (D39–D41)

- **D39 puzzles** — method: **restate → name strategy → math → sanity**; strategies (invariant/pigeonhole/binary-search/work-back/extremal/info-bound); **100-coins blindfolded (separate K, flip them)**; coin-game (`N mod K+1`); 100-prisoners cycle-following (~31%); weighing bound `3^k`.
- **D40 Intel narrative** — **30-sec: Intel IP-Val current** (CPU-subsystem PM/PCIe pre-Si) → L&T storage bring-up → Intel pre-Si → MS cross-IHV AI val → back to Intel; 5 anchor traces; through-line = cross-IP/cross-vendor debug + coverage.
- **D41 mock** — 90-min loop; default answer shape (what→constraint→mechanism→failure-hook); coding pre-amble (edge/invariant/termination); STAR bug stories end on the coverage lesson.

---

## Bug-story bank (one-liners, ≤90 sec each)

| Story | One-line | Day |
|---|---|---|
| **QNN page-fault / IOMMU race** (cross-IHV anchor) | wake issued DMA before IOMMU stream-table restored → unmapped IOVA → hang; fix = restore-before-DMA barrier + fault-retry; missing `(wake × stream-state)` coverpoint | [D40](archive/day40_intel_role_narrative.md) |
| **ADR opcode-ordering hang** | missing wait-for-ack between PCU FW opcodes (CDC structure) | [D31](archive/day31_post_si_pm_validation.md) |
| **Core C6 wake-loss** | CFC FIFO race — pulse crossed a domain and was lost (D12–14) | [D31](archive/day31_post_si_pm_validation.md) |
| **PkgC6 blocked by PCIe** | endpoint not in L1.2 → root-complex won't deep-idle | [D37](archive/day37_pcie_pm_intersection.md) |
| **PCIe lane-polarity reversed** | board-level catch, spec-mandated reversal coverage | [D36](archive/day36_pcie_bringup_functional_val.md) |

---

## The five must-pass anchor traces ([Day 40](archive/day40_intel_role_narrative.md))

1. MS perf-regression flow · 2. Intel PCIe transactor + debug · 3. L&T JTAG · 4. P-states vs C-states vs D-states · 5. CPU architecture diagram from memory.

> **Lead sentence:** *"Intel IP-Val engineer with an RTL-to-system foundation — digital design, CDC, memory, CPU arch, PM, PCIe — plus cross-IP debug; I apply it where coverage leverage is highest, at the IP boundary."*

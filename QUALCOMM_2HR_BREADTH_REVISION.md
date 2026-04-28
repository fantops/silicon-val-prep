# Qualcomm 2-hr Breadth Revision (concepts > depth)

> **Rule for this round:** answer **clearly, concretely, in 60-90 sec**. One concrete example beats three textbook bullets. If they want depth, they will drill - let them lead.

---

## SECTION 0 - Last-5-min cheat sheet

| Reflex | One-liner |
|---|---|
| C-state entry | flush dirty $, save arch state, gate clocks, gate power, drop voltage |
| C-state exit | wake event → restore voltage → ungate power → ungate clocks → restore arch state → resume |
| C-states cost order | **C0 → C1 → C1E → C3 → C6 → C7** (deeper = more saved + slower wake) |
| CDC (clock-domain crossing) | always need a synchronizer (2-FF for 1-bit, async-FIFO or req/ack for multi-bit) |
| Pipeline (5-stage classic) | **IF → ID → EX → MEM → WB** |
| `MOV A, B` (reg-reg) | uses no ALU op + no MEM access; rename usually elides it |
| `MOV A, [B]` (load) | uses MEM stage; AGU computes address, hits L1/L2/LLC/DRAM |
| Virtual addr → Physical | done at **runtime by MMU**, every memory access, cached in TLB |
| TLB miss | HW page walk (x86) or SW handler (some MIPS) |
| Variable physical addr assigned | only when the page is first **touched** (demand paging) |
| MMIO read | `volatile uint32_t v = *((volatile uint32_t*)REG_ADDR);` |
| Set bit n | `reg \|=  (1u << n);`  Clear: `reg &= ~(1u << n);`  Toggle: `reg ^= (1u << n);` |
| Read field (mask+shift) | `(reg >> SHIFT) & MASK` |
| Function pointer decl | `int (*fp)(int, int);`  Assign: `fp = &add;`  Call: `fp(2,3);` |
| Array of fp | `int (*ops[N])(int,int) = { add, sub, mul };` |
| Real use | ISR vector table, driver `file_operations`, state machines, callbacks |

---

## SECTION 1 - C-states (full flow + bugs) - DIRECT HIT

### 1.1 What a C-state is

**C-state = idle state of the CPU core** when no software is ready to run on it. Higher number = more aggressive power saving, longer wake latency. Orthogonal to **P-state** (active freq/voltage when running).

### 1.2 C-state ladder (Intel/AMD/ARM concept maps)

| State | What's saved | What stays | Wake latency | Notes |
|---|---|---|---|---|
| **C0** | nothing | core fully active | 0 | running |
| **C1** | core clock gated | caches hot, voltage held | ~1 µs | `HLT` instruction |
| **C1E** | C1 + lower voltage | caches hot | ~1-10 µs | enhanced halt |
| **C3** | L1/L2 flushed, clock + PLL off | L3, voltage on | ~50 µs | rarely used now |
| **C6** | **arch state saved to SRAM, core power-gated** | L3 (PkgC2), uncore on | ~100 µs | most common deep idle |
| **C7 / C8 / C10** | + LLC flush, more rails off | platform wakes possible | 100s µs - ms | deepest, package-level |

ARM equivalent terminology: WFI / WFE / power-down via PSCI `CPU_OFF`, `CPU_SUSPEND`. RPMh on Snapdragon arbitrates these across cores.

### 1.3 Entry flow (what HW + FW do step-by-step)

```
1. OS scheduler: no runnable thread on this core → emit MWAIT(target_cstate)
                 or WFI on ARM
2. Core HW: drain pipeline, retire in-flight ops, reach quiescence
3. Save arch state (GPRs, MSRs, FPU/vector regs) to power-gated SRAM
4. Flush dirty L1/L2 lines per target depth (C6+ flushes private $)
5. Notify uncore / PCU (Intel) / RPMh (QC):
       "core ready for C6 - pull my voltage rail"
6. PCU asserts handshake to FIVR / IVR → V-rail droops to retention voltage
7. PCU gates clocks → power-gates the core → core is dark
8. PCU stays awake, monitors wake events (interrupts, snoop, IPI, timer)
```

### 1.4 Exit flow (what wakes a sleeping core)

```
1. Wake event arrives at PCU/RPMh:
   - Local APIC interrupt routed via PCH/IIO
   - Inter-Processor Interrupt from another core
   - Timer expiry (deadline timer or HPET)
   - Snoop request that needs this core to participate
   - IO device MSI-X targeted at this core
2. PCU starts wake sequence:
   a. ungate power (V-rail back to operating voltage)
   b. release reset + ungate clocks (PLL relock - biggest latency contributor)
   c. core fetches reset vector → microcode restores arch state from SRAM
   d. caches re-warm via demand fetches (cold-cache penalty)
3. Core re-enters C0; resumes the instruction after MWAIT/WFI
```

### 1.5 Trade-offs

- **Saving** scales with depth: C1 saves leakage of clock tree only; C6 saves leakage of entire core (~70% of idle power).
- **Cost** scales with depth: wake latency, cold-cache penalty, OS scheduler "miss" cost.
- **Break-even:** C-state is worth entering only if expected idle time > 2× wake latency. cpuidle drivers use this heuristic.
- **Risk:** thrashing in/out of C6 on bursty workload → more energy than just staying in C1.

### 1.6 Real C-state bug to tell (your Cambria Core C6 wake-loss)

> "On Cambria pre-Si, Core C6 wake occasionally lost. Symptom: thread blocked forever after a timer interrupt that should have woken it. Other cores fine. Root cause: a 1-deep CFC FIFO between core PMA and PCU dropped the wake-ack when a back-to-back snoop + timer arrived in adjacent PCU cycles. PCU saw the wake intent, never confirmed completion, core stayed power-gated. Repro on Zebu emulation needed forcing the snoop+IRQ overlap. Fix: PCU FW added an explicit re-arm after timeout. Caught pre-Si - would have been a P0 silicon bug."

### 1.7 Common interview probes

- *"Why can't we always stay in C6?"* - wake latency hurts bursty workloads; OS scheduler avoids it under load via cpuidle governor.
- *"Why is PkgC6 harder than Core C6?"* - needs **all** cores in C6 + uncore conditions met (no IO traffic, no DMA, no LLC dirty lines that need snooping). One stray driver wakeup blocks the whole package.
- *"What's the relation between C-states and S-states?"* - C-states are CPU-only (running OS). S-states are system-wide sleep (S0 active, S3 suspend-to-RAM, S4 hibernate). Modern Connected Standby = **S0ix** which uses deep PkgC10 with S0 still nominally active.

---

## SECTION 2 - Cross-domain crossings (CDC) - DIRECT HIT

### 2.1 What is a domain

- **Clock domain** = set of logic driven by one clock source. Different domains may run at different freqs / phases.
- **Power domain** = set of logic on one voltage rail; can be independently gated.
- A signal crossing between domains can sample mid-transition → **metastability** → undefined value latched → state corruption.

### 2.2 Why critical

- A single missed sample at a CDC = silent functional bug, often timing-dependent (works on bench, fails at PVT corner).
- Industry data: **~30% of pre-Si bugs in modern SoCs are CDC issues.** Coverage gap in regular sim because async events are rare.

### 2.3 Synchronization techniques

| Crossing | Standard solution |
|---|---|
| **1-bit control / level signal** | 2-FF synchronizer (2 flops in destination clock) |
| **1-bit pulse** | toggle-synchronizer (toggle in source, edge-detect in dest) |
| **Multi-bit data** | **async FIFO** with Gray-coded read/write pointers, OR full **req/ack handshake** |
| **Reset** | async assert / sync deassert reset synchronizer |
| **Power-gated → always-on** | isolation cells (clamp output), retention flops, level shifters if voltages differ |

**Why Gray code in async FIFOs:** only 1 bit changes per increment → metastability on at most 1 bit → resync gives valid pointer.

### 2.4 Bug shapes

- **2-FF MTBF too low:** synchronizer depth insufficient at high freq → resync fails 1-in-N years → field corruption.
- **Combinational logic between sync stages:** illegal - second flop now sampling glitching net.
- **Unrelated signals taking different sync paths:** arrive in different cycles in destination → ordering broken (e.g., data valid before address valid).
- **Reset CDC missed:** async reset asserted mid-clock → some flops reset, some don't → inconsistent state.

### 2.5 Validation reflexes

- Pre-Si: CDC linter (Spyglass-CDC, Conformal-CDC) flags every crossing; engineer signs off each waiver.
- Post-Si: a regression that only repros at certain voltage / temperature / freq combo → suspect CDC or analog timing margin.
- Power-domain gating: check **isolation enable timing** - must assert before power-down, deassert after power-up.

### 2.6 Firmware angle (where your Cambria PCU work lives)

PCU FW writes opcodes that toggle V-rails, clock gates, PLL bypasses across many domains. Wrong **opcode order** = effectively a CDC violation at the FW layer (e.g., enabling a signal before its destination clock is alive). Same shape as the ADR bug story.

---

## SECTION 3 - CPU pipeline (concept-level) - DIRECT HIT

### 3.1 Classic 5-stage RISC pipeline (the textbook + the one to draw)

```
    IF        ID        EX        MEM       WB
  Instr    Decode    Execute    Memory    Write
  Fetch    +Reg-Read  ALU/AGU   Load/Store Back
```

| Stage | What happens | HW units |
|---|---|---|
| **IF** | PC → I-cache fetch; predict next PC | I$, BPU |
| **ID** | decode opcode; read source regs from regfile | decoder, register file |
| **EX** | ALU op, branch decide, AGU compute load/store address | ALU, AGU, branch unit |
| **MEM** | access D-cache for load/store (only if needed) | D$, LSQ |
| **WB** | write result back to architectural register | regfile write port |

Every cycle, ideal throughput = 1 instruction completes (CPI = 1). Modern OoO superscalar = same 5 logical stages but **wider** (4-6 wide) and **deeper** (14-20 stages because Fetch + Decode + Rename + Dispatch + Schedule + Execute ports + Commit each split into multiple sub-stages).

### 3.2 Walkthrough - `MOV A, B` (register-to-register)

Assume `A`, `B` are CPU registers (not memory).

| Stage | What happens for `MOV A, B` |
|---|---|
| IF | fetch the `MOV A,B` opcode bytes from I$ |
| ID | decode → "copy reg B to reg A"; read register B value from regfile |
| EX | **no ALU op needed** - value just passes through (or 0+B in some impls) |
| MEM | **skipped** - no memory access |
| WB | write B's value into reg A in regfile |

**Modern OoO twist:** the rename stage **eliminates** reg-reg `MOV` entirely. The Register Alias Table just remaps A's logical-reg pointer to point at B's physical reg. Zero ALU usage, zero EX latency. (This is called **move elimination**, present on Intel since Ivy Bridge.)

### 3.3 Walkthrough - `MOV A, [B]` (load) and `MOV [B], A` (store)

| Stage | Load: `MOV A, [B]` | Store: `MOV [B], A` |
|---|---|---|
| IF | fetch | fetch |
| ID | decode; read B | decode; read A and B |
| EX | AGU computes address = B (no ALU op on data) | AGU computes address = B |
| MEM | **D-cache read** at addr; on L1 miss → L2/L3/DRAM walk | **D-cache write** (or store-buffer enqueue) |
| WB | result written into reg A | nothing to write back |

### 3.4 Hazards (concept-level)

| Hazard | Cause | Resolved by |
|---|---|---|
| **Structural** | two instructions need the same HW unit same cycle | extra ports / pipeline / stall |
| **Data RAW** | instr B reads what A writes; A not yet written | forwarding (EX→EX bypass); else stall |
| **Data WAR / WAW** | output dependency | register renaming (kills both) |
| **Control** | branch: next PC unknown until EX | branch prediction; misprediction → flush + refetch |

For `MOV A, B`: only RAW possible if a later instr reads A and A was written here - handled by forwarding or rename.

### 3.5 Key one-liners interview-ready

- *"Why pipeline at all?"* - overlaps stages → throughput = 1 instr/cycle ideally instead of 1/N.
- *"What's the cost of a branch mispredict?"* - flush all in-flight stages = ~pipeline depth × IPC = 15-20 cycles modern.
- *"What does superscalar add?"* - multiple instructions enter the pipeline per cycle (4-6 wide).
- *"What does OoO add?"* - execution order ≠ program order → independent instructions don't wait on slow ones; retirement still in order for precise exceptions.

---

## SECTION 4 - Virtual Memory + MMU + TLB - DIRECT HIT

### 4.1 Virtual vs Physical

- **Virtual address (VA)** - what the program/CPU uses. Each process sees its own large flat address space.
- **Physical address (PA)** - what actually goes on the memory bus to DRAM.
- Translation enables: process isolation, demand paging, swap, shared memory, ASLR, DMA isolation (via IOMMU).

### 4.2 The translation chain

```
program issues load to VA  →  MMU walks page tables  →  PA emitted to bus
                              ↑
                        TLB caches recent VA→PA
```

Page table is typically a **multi-level tree** (x86-64: 4-level or 5-level; ARMv8: 3 or 4 level). Each level indexed by a slice of the VA.

### 4.3 MMU role

- Performs VA→PA translation **in hardware** on every memory access.
- Enforces **permissions** (R / W / X / U/S) per page → SIGSEGV on violation.
- Manages **TLB** caches.
- On x86: page-walker is a HW state machine inside MMU. On some MIPS/early RISC: TLB miss raises a trap and SW walks the table.

### 4.4 TLB

- Translation Lookaside Buffer - small fully-associative or set-associative cache of recent VA→PA mappings.
- Hit = ~1 cycle (parallel to L1 access). Miss = page walk → 4-5 memory accesses worst case → 30-100+ cycles.
- L1 dTLB: tens of entries. L2 STLB: ~1.5K entries (Intel client). Separate iTLB for instructions.
- **Huge pages (2 MB / 1 GB)** map larger ranges per entry → fewer TLB misses on big working sets. Standard fix for memory-heavy workloads (DBs, ML, NPU input buffers).

### 4.5 TLB miss flow

```
1. Load instruction issued; MMU checks TLB → miss
2. Page-walker reads PML4 → PDPT → PD → PT entries from memory
   (these page-table accesses themselves go through D-cache, can hit/miss)
3. Final PTE gives PA + permissions
4. Install entry in TLB; retry the load
```

Counters: `DTLB_LOAD_MISSES.WALK_COMPLETED`, `WALK_DURATION`.

### 4.6 When does VA→PA translation happen?

**Runtime, on every memory access.** Not at compile time. Not at link time. Not at load time.

- **Compile time:** compiler emits relative or symbolic addresses, plus relocation info.
- **Link time:** linker assigns **virtual** addresses to symbols (e.g., `.text` at 0x400000).
- **Load time:** loader maps the binary into the process VA space (with ASLR offset). PTEs marked **not-present** until first access.
- **First access (page fault):** kernel allocates a physical page, fills PTE, returns. **This is when a PA is first assigned to that VA.**
- **Every subsequent access:** MMU does VA→PA at runtime via TLB / page walk.

### 4.7 "When does my C variable get a physical address?"

```c
int x;            // global → linker reserves a VA in .bss
int *p = malloc(4096);   // glibc returns a VA from heap region
*p = 42;          // FIRST WRITE → page fault → kernel allocates physical page
                  //   → PTE installed → write completes
                  //   → from now on, this VA always maps to that PA (until swap/free)
```

So the answer to *"when does the variable's physical address get assigned?"* is: **on first touch (demand paging)**, not at declaration, not at malloc.

### 4.8 Page fault types (concept, no depth needed)

| Type | Cause | Outcome |
|---|---|---|
| **Minor** | page is in memory but not in this PTE (e.g., shared lib already loaded) | kernel just installs PTE, fast |
| **Major** | page on disk/swap | I/O to fetch, slow (ms) |
| **Invalid** | access to unmapped VA or permission violation | SIGSEGV |
| **COW** | write to copy-on-write page (post-fork) | kernel copies page, updates PTE, retry |

### 4.9 IOMMU / SMMU (one line)

Same MMU concept but for DMA-capable devices: each device sees an **IO Virtual Address** that the IOMMU translates to PA. Stops a buggy/malicious device from DMAing into arbitrary memory. Intel = **VT-d**; ARM = **SMMU**; Stream IDs identify which device's translation context to use.

---

## SECTION 5 - MMIO + Bit Manipulation (code-ready)

### 5.1 MMIO read

```c
#include <stdint.h>

#define REG_BASE   0xFE000000UL
#define REG_STATUS (REG_BASE + 0x10)

static inline uint32_t read_reg(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

uint32_t v = read_reg(REG_STATUS);
```

### 5.2 MMIO write

```c
static inline void write_reg(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}

write_reg(REG_STATUS, 0x1);
```

### 5.3 Why `volatile` for MMIO

- Tells compiler **do not** cache the value, **do not** reorder, **do not** elide repeat reads.
- Without `volatile`, a polling loop `while (*reg & 1) ;` would be hoisted out and never see a status-bit change. Compiler thinks the value can't change.
- `volatile` does **not** give CPU memory ordering - it's a compiler-only directive. For HW ordering use `mb()` / `dmb` / `mfence` or use UC/WC mappings.

### 5.4 Bit manipulation reflexes

```c
#define BIT(n)            (1u << (n))

#define SET_BIT(reg, n)   ((reg) |=  BIT(n))
#define CLR_BIT(reg, n)   ((reg) &= ~BIT(n))
#define TGL_BIT(reg, n)   ((reg) ^=  BIT(n))
#define TST_BIT(reg, n)   (((reg) >> (n)) & 1u)
```

### 5.5 Field via mask + shift

For a status register where bits [7:4] = link-speed:

```c
#define LINK_SPEED_SHIFT  4
#define LINK_SPEED_MASK   0xFu          /* 4 bits = 0xF */

/* read: shift down then mask */
uint32_t speed = (status >> LINK_SPEED_SHIFT) & LINK_SPEED_MASK;

/* write: clear field, then OR in new value (already masked) */
status &= ~(LINK_SPEED_MASK << LINK_SPEED_SHIFT);
status |=  ((new_speed & LINK_SPEED_MASK) << LINK_SPEED_SHIFT);
```

### 5.6 Polling pattern (with timeout - interview gold)

```c
static inline int poll_bit_set(uintptr_t reg, uint32_t mask, unsigned us) {
    while (us--) {
        if (read_reg(reg) & mask) return 0;     /* success */
        udelay(1);
    }
    return -ETIMEDOUT;                          /* never assume forever */
}
```

Always include the timeout. Polling without bound = the bug story they love.

### 5.7 Endianness one-liner

- **Little-endian** (x86, ARM default): least-significant byte at lowest address.
- For MMIO across endianness boundaries (e.g., big-endian device on little-endian CPU) use `cpu_to_le32` / `le32_to_cpu` helpers - never raw cast.

---

## SECTION 6 - Function Pointers (code-ready)

### 6.1 Declare + assign + call

```c
int add(int a, int b) { return a + b; }

int (*fp)(int, int);   /* declaration: pointer to fn taking (int,int) returning int */

fp = &add;             /* assign - & is optional, fn name decays to addr */
fp = add;              /* equivalent */

int r1 = (*fp)(2, 3);  /* call - explicit deref */
int r2 =   fp (2, 3);  /* call - implicit, more common */
```

### 6.2 typedef makes it readable

```c
typedef int (*binop_fn_t)(int, int);

binop_fn_t  fp = add;
int r = fp(2, 3);
```

### 6.3 Where they show up (real use cases)

| Use case | Example |
|---|---|
| **Driver ops table** | Linux `struct file_operations { ssize_t (*read)(...); int (*open)(...); ... };` |
| **Interrupt vector table** | array indexed by IRQ#, each entry = ISR function pointer |
| **State machine** | `state_t (*handlers[NUM_STATES])(event_t)` - dispatch by current state |
| **Callbacks** | `qsort(arr, n, sz, compare_fn)`, libuv async I/O, GUI events |
| **Plugin / dynamic dispatch** | `dlsym()` returns a `void*` cast to fn pointer |
| **Test harness** | array of test fn pointers run in order |

### 6.4 Why kernels/firmware love them

- C's substitute for virtual functions: lets one piece of code dispatch to many implementations without `if/else` chains.
- Decouples policy (caller) from mechanism (driver).

---

## SECTION 7 - Array of Function Pointers (code-ready) - DIRECT HIT

### 7.1 Declaration

```c
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

int (*ops[3])(int, int) = { add, sub, mul };
```

Read the declaration **inside-out**: `ops` is an array of 3 elements, each is a pointer to a function taking `(int,int)` returning `int`.

### 7.2 With typedef (preferred for sanity)

```c
typedef int (*binop_fn_t)(int, int);

binop_fn_t ops[] = { add, sub, mul };
size_t n = sizeof(ops) / sizeof(ops[0]);
```

### 7.3 Invocation

```c
for (size_t i = 0; i < n; i++) {
    int r = ops[i](10, 3);          /* dispatch by index */
    printf("op %zu = %d\n", i, r);
}
```

### 7.4 Real example - opcode dispatch table (interpreter style)

```c
typedef void (*op_handler_t)(cpu_t *cpu);

void op_nop (cpu_t *c) { c->pc++; }
void op_add (cpu_t *c) { c->reg[0] += c->reg[1]; c->pc++; }
void op_halt(cpu_t *c) { c->halted = 1; }

op_handler_t handlers[256] = {
    [0x00] = op_nop,
    [0x01] = op_add,
    [0xFF] = op_halt,
    /* others NULL - caller checks */
};

void step(cpu_t *cpu) {
    uint8_t opc = cpu->mem[cpu->pc];
    op_handler_t h = handlers[opc];
    if (h) h(cpu);
    else   trap_illegal_opcode(cpu);
}
```

This is **literally how interpreters and bytecode VMs dispatch.** Same shape used in network packet handlers, NVMe admin command tables, ACPI method dispatch.

### 7.5 Real example - state machine

```c
typedef enum { S_IDLE, S_RUN, S_DONE, S_NUM } state_t;
typedef state_t (*state_fn_t)(event_t e);

state_t st_idle(event_t e) { return (e == EV_START) ? S_RUN  : S_IDLE; }
state_t st_run (event_t e) { return (e == EV_STOP)  ? S_DONE : S_RUN;  }
state_t st_done(event_t e) { (void)e; return S_DONE; }

state_fn_t fsm[S_NUM] = { st_idle, st_run, st_done };

state_t cur = S_IDLE;
cur = fsm[cur](next_event());
```

Eliminates large `switch` blocks; each state is a self-contained function.

---

## SECTION 8 - All 21 days at concept-breadth (don't volunteer depth)

### Day 1-2: C address fluency, pointers, alignment
- Pointer = address; `*p` deref, `&x` address-of.
- `int *p` vs `int **pp` - pointer levels.
- Alignment: type T must sit at addr divisible by `alignof(T)`. Misaligned access on ARM = trap (or slow); on x86 = slow but allowed.
- `container_of(ptr, type, member)` - kernel idiom to recover the enclosing struct from a pointer to one of its fields.

### Day 3: Bits / MMIO / volatile / polling
Covered §5.

### Day 4: Cache lines, locality, AMAT
- Line = 64B. **Spatial locality** = adjacent addresses; **temporal** = same address soon again.
- AMAT = hit_time + miss_rate × miss_penalty (recursive across L1→L2→L3→DRAM).
- Write policies: write-back vs write-through; write-allocate vs no-write-allocate.

### Day 5: Coherence, MESI, false sharing
- MESI = Modified / Exclusive / Shared / Invalid per line per cache.
- MESIF (Intel) adds Forward; MOESI (AMD/ARM) adds Owned.
- False sharing: two threads write different vars on same 64B line → ping-pong → MT scaling collapses. Fix: pad to 64B.

### Day 6-7: Virtual memory, TLB, IOMMU/SMMU
Covered §4.

### Day 8: DMA, doorbells, ordering, barriers
- DMA = device moves data without CPU; CPU just sets up descriptors + writes a doorbell MMIO reg to kick.
- **Critical ordering:** descriptor must be visible in memory **before** doorbell write (`wmb()` between them) - or device reads stale descriptor.
- x86 TSO usually enough; ARM weak ordering needs explicit `dmb st`.

### Day 9: ARM/Qualcomm vocabulary (skim, don't claim)
- AArch64 EL0-3 (user / kernel / hypervisor / secure-monitor).
- PSCI = power state coordination interface (CPU on/off/suspend).
- RPMh = resource-power-mgr in hardware on Snapdragon (replaces RPM µC).
- SMMU = ARM IOMMU. GIC = ARM interrupt controller.
- One-line frame: "I'm coming from x86; I know the QC names map to PCU/PMC/IOMMU equivalents."

### Day 10: PCIe + LTSSM
- Transaction layer (TLP) → Data Link (DLLP/ACK/NAK) → Physical (8b/10b or 128b/130b).
- LTSSM states to know: **Detect → Polling → Configuration → L0 (active) → L0s/L1/L2 (low power) → Recovery → Hot-Reset/Disabled**.
- L0s = ~ns wake; L1 = µs; L1.2 = ms but lowest power.
- Common bug shape: stuck in **Recovery** = signal integrity / equalization fail.

### Day 11: NVMe queues
- SQ (submission) + CQ (completion) per queue pair; admin queue + N IO queues.
- Doorbell registers = `SQyTDBL` (tail) and `CQyHDBL` (head).
- Bring-up flow: enable controller (CC.EN=1) → wait CSTS.RDY=1 with timeout → create admin SQ/CQ → identify ctrl → create IO queues.
- **CSTS.RDY-stuck bug** = your story (PCIe link OK, controller never asserts ready → root cause was incorrect CC.MPS setting).

### Day 12: DVFS, P-states, PVT
- P-state = (V, F) operating point. PCU/RPMh picks one based on load.
- HWP (Intel) = HW autonomously picks; legacy P-state = OS picks via MSR write.
- PVT = Process / Voltage / Temperature corners; binning sorts dies into SKUs by best stable F at given V.

### Day 13: C-states, S0ix, PkgC6
Covered §1.

### Day 14: ADR, PCU FW, Core C6 bugs
- ADR = Asynchronous DRAM Refresh - on power loss, platform has ~100 µs (battery/cap) to flush write-pending queues (WPQ) to NVDIMM.
- PCU FW = micro-engine running opcodes that sequence V/F/clock/power changes.
- Core C6 bug story = your strongest bullet.

### Day 15: Thermal, perf counters, multi-IP
- Tj = T_amb + P × θ_JA. Throttle at Tjmax (100°C client / 105°C server).
- RAPL MSRs (PKG/PP0/PP1/DRAM) = energy counters. RAPL ≠ wall power (20-30% gap).
- Multi-IP interference: shared LLC, shared NoC, shared DDR BW. Validate single-IP baseline first.

### Day 16: Pipeline, OoO, memory consistency, SIMD
Covered §3 + (in 1HR file). Extra: TSO (x86) vs weak (ARM); AVX license levels (PCU cuts F when AVX-512 active).

### Day 17: Concurrency primitives + DSA
- Spinlock vs mutex vs RWlock vs RCU. Atomics: `compare_and_swap`, `fetch_add`.
- Lock-free queue (MPMC) = pointer + CAS; ABA problem solved by tagged pointers / hazard pointers.
- DSA reflexes: hashmap O(1) avg, BST O(log n), heap O(log n), reverse linked list (3-pointer iterative), binary search variants.

### Day 18-19: NPU stack
- ORT → Execution Provider (QNN for Hexagon, OpenVINO for Intel NPU, DML fallback for GPU).
- Silent EP fallback = unsupported op drops to CPU EP → perf cliff. Detect via per-node EP-assignment log.
- VTCM = on-chip SRAM (8 MB on HTP v79). Tiling fits activations into VTCM.

### Day 20: Debug methodology
- Reproduce → minimize → isolate variable → form hypothesis → test → fix → add regression.
- Bisect across versions / configs / platforms.
- "Last known good" + "first known bad" = bisect anchor.

### Day 21: Tailoring
- For each company: lead with **their** stack mapping (Intel → x86 first; QC → ARM/Snapdragon first; AMD → similar to Intel).
- Have 3 polished bug stories ready, pick the one that matches the role.

---

## SECTION 9 - Behavioral / fluency reflexes

| Question | 30-sec answer shape |
|---|---|
| "Walk me through your background." | M.Tech CSE NIT-D 2018 → L&T@SanDisk NVMe post-Si → Intel Cambria pre-Si emulation (PCIe Gen6, ADR, C-states) → Microsoft NPU platform validation (Copilot+ PCs, EdgeOptimizer). Common thread = silicon validation across the stack. |
| "Why Qualcomm?" | Want to do silicon work on cutting-edge mobile/PC SoC (X Elite, premium tier). My x86 background gives me a translation lens; eager to learn ARM/Snapdragon native tooling. |
| "Tell me a hard bug." | Pick **one** - Core C6 wake-loss (PCU/CFC FIFO) or NVMe CSTS.RDY (CC.MPS misconfig). STAR: situation, task, action (debug ladder), result. |
| "Strength?" | Cross-stack debug - comfortable from RTL traces up to ETW / OS scheduler. |
| "Weakness?" | ARM/Snapdragon native vocabulary still building; mitigating by mapping to x86 equivalents I know deeply. |
| "Question for us?" | Team's split between pre-Si and post-Si? What's the regression cadence? Tooling stack? |

---

## SECTION 10 - Final 10-min checklist before walking in

- [ ] `MOV A, B` walkthrough rehearsed (5 stages + "rename elides it" twist)
- [ ] C-state entry+exit flows (8 steps each) - out loud, no notes
- [ ] CDC: 2-FF synchronizer / async FIFO / Gray code - why each
- [ ] Virtual → Physical: "MMU at runtime, every access, TLB caches it, PA assigned on first touch (page fault)"
- [ ] MMIO read/write code with `volatile`, mask-shift, set/clear bit macros
- [ ] Function pointer: declare + typedef + array + state-machine example
- [ ] One bug story you OWN end-to-end (Core C6 OR NVMe CSTS.RDY)
- [ ] Honest framing: "I do platform/system validation; for ARM specifics I'm mapping from x86 - call out anything I should sharpen"
- [ ] Breathe. Concrete > exhaustive. Let them drill if they want depth.

---

> **Key meta:** previous interview was depth-on-CPU-core. This 2-hr will likely sweep more areas. Aim for **clear, concrete, 60-90 sec answers** on each topic. Have one good code/diagram per topic ready. If you don't know - say so cleanly and pivot to the closest thing you do know. Confidence + clarity beats over-claim every time.

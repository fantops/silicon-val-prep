# Day 14 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day14_adr_pcu_core_c6_bugs.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: ADR opcode-sequence simulator

**Problem statement:** Model the PCU's ADR (Asynchronous DRAM Refresh) flow as an opcode sequence. Implement two versions:

(a) **Buggy** — issues `WPQ_FLUSH` without first waiting for `IO_QUIESCE_ACK`. The model detects this and prints `BUG: WPQ_FLUSH issued before IO_QUIESCE_ACK -> WPQ refills, hang`.

(b) **Fixed** — strict ordering: `IO_QUIESCE` + ack → `FABRIC_QUIESCE` → optional `LLC_WRITEBACK` (eADR) → `WPQ_FLUSH` + ack → `DDR_SELF_REFRESH` → `NVDIMM_SAVE`. Report `in_self_refresh` true on success.

**You must implement:**
```c
typedef enum {
    OP_IO_QUIESCE, OP_IO_QUIESCE_ACK, OP_FABRIC_QUIESCE, OP_LLC_WRITEBACK,
    OP_WPQ_FLUSH, OP_WPQ_FLUSH_ACK, OP_DDR_SELF_REFRESH, OP_NVDIMM_SAVE, OP_DONE
} adr_op_t;

typedef struct {
    bool io_quiesced, fabric_quiesced, wpq_drained, in_self_refresh, eadr;
} adr_state_t;

int adr_run_buggy(adr_state_t *s);
int adr_run_fixed(adr_state_t *s);
```

**Driver / tests to write:**
- Run buggy → expect failure print.
- Run fixed → expect `rc=0`, `in_self_refresh=1`.

**Be ready to explain in interview:**
- The Cambria PCU FW bug (`WPQ_FLUSH` before `IO_QUIESCE_ACK`); how Zebu opcode trace caught it; the Perspec assertion that gates regression.
- Core C6 wake-lost CFC FIFO race; Lauterbach trigger condition `wake_in==1 && cfc_init_busy==1`; fix is hold latch in always-on until `init_done && drain_done`.
- ADR hold-up budget physics (10-30 ms) and what sets it.
- Why PCU is firmware not RTL.

## E2 — DS&A: Number of Islands (LC 200)

**Problem statement:** Given a 2D grid of `'1'` (land) and `'0'` (water), count distinct connected land regions (4-directional).
**Signature:** `int num_islands(char **grid, int rows, int cols);`
**Constraints / edge cases:** empty grid, all water, all land (1 island), single cell.
**Talking points:** O(R·C) BFS/DFS flood-fill; same shape as connected-component analysis on a die-thermal hotspot map or on a fault-cluster localisation in JTAG scan.

## E3 — Architect Drills (no code, write your answers)

**D1.** Why is the ADR hold-up budget 10-30 ms — what physics sets it?
**D2.** Why is the ADR opcode order exactly `IO_QUIESCE → WPQ_FLUSH → SR_ENTRY → NVDIMM_SAVE`? Walk each pair-swap failure mode.
**D3.** Why isn't the PCU opcode table burned into hardware?
**D4.** Why is the always-on / CFC FIFO crossing the bug-prone domain in C6? What does a 2-FF synchroniser fix and what does it not?
**D5.** Construct the workload where eADR is necessary, not luxury. What does CXL.mem persistence change?
**D6.** Why doesn't the PCU just force-quiesce uncore IPs instead of waiting?
**D7.** "Works in unit-sim, fails in full-die emulation" — what's the cheapest pre-Si hook that finds 80% of these?

## E4 — Bug Anchor

**Story 1 — ADR opcode-ordering hang (Cambria):** PSU loss simulated, PLI asserted, PCU FW started its sequence, DDR never reached self-refresh. Walk: hypothesis (IMC issue) → evidence (Zebu PCU opcode log) → root cause (`WPQ_FLUSH` before `IO_QUIESCE_ACK`) → fix (explicit wait-for-ack, Perspec assertion) → lesson (firmware sequences need explicit handshakes).

**Story 2 — Core C6 wake lost on back-to-back ARAT timer (Cambria):** sporadic 50-500 ms hangs; perfmon shows C6 residency past expected exit. Walk: hypothesis (timer/IRQ delivery) → evidence (Lauterbach trigger on `wake_in==1 && cfc_init_busy==1`) → root cause (two-clock-domain race during CFC FIFO mid-reset) → fix (hold wake latch until `init_done && drain_done`) → lesson (always-on → power-gated handoff must hold intent until sink clock + reset stable).

## Reference solution

Once you've attempted, see `../day14_adr_pcu_core_c6_bugs.md` sections "Coding A" and "Coding B" for full solutions and discussion.

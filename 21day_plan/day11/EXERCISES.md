# Day 11 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day11_nvme_queues_bringup.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: NVMe bring-up + SQ submit + CQ poll

**Problem statement:** Build a software model of the NVMe controller bring-up sequence on top of an in-memory `bar[BAR_SZ]` array.

Implement:
- `r32(off)`, `w32(off, v)`, `w64(off, v)` accessors (with the side effect that writing `CC.EN=1` causes `CSTS.RDY` to follow EN — that's the model's stand-in for the real device asserting ready).
- `sim_init()` populates `CAP` with `MQES=255, TO=10, DSTRD=0`.
- `db_off(q, is_cq, dstrd)` returns `0x1000 + (2*q + (is_cq?1:0)) * (4 << dstrd)`.
- `nvme_bringup()` performs the canonical 5-step bring-up: read CAP; disable + poll RDY=0; program AQA/ASQ/ACQ (admin queue, size = 32, AQA encodes `size-1` in both halves); enable with `IOSQES=6, IOCQES=4, EN=1`; poll RDY=1; print computed doorbell offsets.

**Register map:** `CAP=0x00, VS=0x08, CC=0x14, CSTS=0x1C, AQA=0x24, ASQ=0x28, ACQ=0x30, DB=0x1000`.

**You must implement:**
```c
uint32_t r32(uint32_t off);
void     w32(uint32_t off, uint32_t v);
void     w64(uint32_t off, uint64_t v);
uint32_t db_off(uint16_t q, bool is_cq, uint8_t dstrd);
bool     nvme_bringup(void);
```

**Driver / tests to write:**
- Call `sim_init()` then `nvme_bringup()`. Expect printed CAP fields, "Controller READY", and four doorbell offsets for queues 0 and 1.
- Stretch: simulate the `AQA` off-by-one bug and show the failure path.

**Be ready to explain in interview:**
- Walk `CSTS.RDY` debug through 5+ suspects (CFS, AQA off-by-one, DSTRD, PRP IOVA, IOMMU EVTQ, MSI-X timing).
- PRP vs SGL — what workload forced SGLs?
- Phase bit on CQ — when does it lose?
- Per-core SQ/CQ pair scaling vs AHCI.

## E2 — DS&A: Group Anagrams (LC 49)

**Problem statement:** Group strings that are anagrams of each other.
**Signature (C-friendly):**
```c
typedef struct { char **items; int count; } group_t;
group_t *group_anagrams(char **strs, int n, int *out_groups);
```
**Constraints / edge cases:** empty input, single string, all anagrams, no duplicates.
**Talking points:** sort-as-key → O(n·k log k); count-tuple key → O(n·k). Hash-key choice equivalent to descriptor-bucket assignment in firmware log triage.

## E3 — Architect Drills (no code, write your answers)

**D1.** Which design choice (per-CPU queues / MSI-X-per-queue / 64B commands / posted doorbells / no SATA TL) mattered most to NVMe scaling? Defend the ranking.

**D2.** Why must the host disable `CC.EN=0` before writing AQA/ASQ/ACQ? What HW state would the alternative break?

**D3.** Phase bit on completions saves a doorbell. Construct the workload where it loses.

**D4.** PRPs vs SGLs — why did NVMe start with PRPs and add SGLs later?

**D5.** Derive the `CSTS.RDY` standard suspect list from the bring-up state machine alone.

**D6.** With memory-semantic SSD over CXL.mem, which NVMe artefacts survive?

**D7.** Per-CPU queue is the scaling unlock. What limits it as core counts hit 1024?

## E4 — Bug Anchor

**Symptoms:** `CSTS.RDY` never asserts after `CC.EN=1`. Walk the debug tree: check `CSTS.CFS` for fatal; verify nothing wrote AQA/ASQ/ACQ while controller was still enabled; recompute `DSTRD` from `CAP[35:32]` (not `CAP[3:0]`); inspect IOMMU event queue for first SQE fetch; verify `AQA` encodes `size-1` not size.

Cross-link: NVMe descriptor + doorbell follows Day 8's pattern; PCIe link state + AER is the first check (Day 10); IOMMU mapping for PRPs (Day 7).

## Reference solution

Once you've attempted, see `../day11_nvme_queues_bringup.md` sections "Coding A" and "Coding B" for full solutions and discussion.

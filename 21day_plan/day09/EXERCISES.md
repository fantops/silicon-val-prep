# Day 09 — Exercises (Question First)

> Read the prompts. Attempt each in `exercises/<file>.c` BEFORE peeking at the reference in `../day09_arm_qualcomm_platform_vocabulary.md` ("Coding A" and "Coding B" sections).

---

## E1 — Systems C: PSCI dispatcher + RPMh vote aggregator

**Problem statement:** Build two small models that mirror the Qualcomm/ARM power stack.

(a) **PSCI dispatcher.** Implement `psci_call(fid, a1, a2)` recognising `CPU_ON (0x84000003)`, `CPU_OFF (0x84000002)`, `SYSTEM_SUSPEND (0xC400000E)`, `SYSTEM_RESET (0x84000009)`. Maintain `cpus[MAX_CPUS]` with CPU0 initially `ON`. `CPU_ON` validates the target index, errors if already on, and prints the entry-point address.

(b) **RPMh aggregator.** Each of N=4 voters submits a `vote_t { freq_khz, volt_mv }`. Aggregator returns the per-field max across all voters (each rail floor).

**You must implement:**
```c
int     psci_call(uint32_t fid, uint64_t a1, uint64_t a2);
typedef struct { uint32_t freq_khz; uint32_t volt_mv; } vote_t;
vote_t  rpmh_aggregate(void);
```

**Driver / tests to write:**
- `CPU_ON` for CPU 1 with entry `0x80000000` → success.
- Repeat → error (already on).
- Populate 4 voter slots (Apps/Modem/CDSP/idle), call `rpmh_aggregate`, print result.
- `SYSTEM_SUSPEND` with resume entry.

**Be ready to explain in interview:**
- ARM EL0/EL1/EL2/EL3 vs x86 ring 0/3 — why four levels?
- TTBR0/1_EL1 + ASID vs CR3 + PCID; granule sizes in TCR_EL1.
- Why does Snapdragon need RPMh on top of PSCI when x86 has PCU alone?
- GIC SGI / PPI / SPI / LPI; how does ITS handle PCIe MSI?

## E2 — DS&A: Valid Parentheses (LC 20)

**Problem statement:** Given a string of `()[]{}`, return whether every opener has a matching closer in the right order.
**Signature:** `int is_valid_parens(const char *s);`
**Constraints / edge cases:** empty string (true), unbalanced opener, unbalanced closer, nested mismatch, very long input.
**Talking points:** O(n) stack walk; same shape as walking PCIe capability chains (Day 10) or matching `BEGIN`/`END` markers in a debug-log triage helper (Day 20).

## E3 — Architect Drills (no code, write your answers)

**D1.** Why does ARM expose four exception levels when x86 collapsed to two practical rings?

**D2.** Why does Snapdragon need RPMh on top of PSCI when x86 has PCU alone?

**D3.** ARM is weak, x86 is TSO. If ARM moved to TSO tomorrow, who wins and who loses?

**D4.** PSCI is an OS↔firmware spec. What does it actually save?

**D5.** Snapdragon boots PBL → SBL → ATF → ABL → kernel with a sig check at every handoff. Why so many stages? What does merging SBL+ATF break?

**D6.** Per-core voltage on DynamIQ is mostly a lie because cores share a rail. What architectural choice makes it real?

**D7.** GIC routes SGI/PPI/SPI/LPI; ITS handles MSI. Why was MSI retrofitted via the ITS rather than as a fifth class?

## E4 — Bug Anchor

**Symptoms:** Driver passes on x86, hangs on Snapdragon. Walk the debug tree: missing `DMB ST` between descriptor write and doorbell. Why does it manifest only on ARM? What scope barrier is correct?

Cold boot fails at -20°C, works at room temp — what fails? (LPDDR retraining across PVT.)

## Reference solution

Once you've attempted, see `../day09_arm_qualcomm_platform_vocabulary.md` sections "Coding A" and "Coding B" for full solutions and discussion.

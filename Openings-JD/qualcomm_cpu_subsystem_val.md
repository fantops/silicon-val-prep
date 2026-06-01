# Qualcomm — CPU Subsystem Validation (Hardware Engineer)

**Interview:** 2 Jun 2026 — 3 rounds, same day, 3 hrs total
**Experience band:** 4–15 yrs (you: 8 — in band)

## General Summary

As a leading technology innovator, Qualcomm pushes the boundaries of what's possible to enable next-generation experiences and drives digital transformation to help create a smarter, connected future for all. As a Qualcomm Hardware Engineer, you will plan, design, optimize, verify, and test electronic systems, bring-up yield, circuits, mechanical systems, Digital/Analog/RF/optical systems, equipment and packaging, test systems, FPGA, and/or DSP systems that launch cutting-edge, world class products. Qualcomm Hardware Engineers collaborate with cross-functional teams to develop solutions and meet performance requirements.

## Minimum Qualifications

- Bachelor's in CS / EE / Engineering + **3+ yrs** HW eng experience, OR
- Master's + **2+ yrs**, OR
- PhD + **1+ yr**

## Focus Areas

- Development and maintenance of **CPU subsystem validation infrastructure** — both **bare-metal and OS-based full-stack** environments (TFA, Linux, hypervisor, TEE).
- Execution and debugging of **functional validation for custom CPU features**.
- Enablement and validation of **low-power and DVFS** features for CPU domains.
- Debugging and profiling across bare-metal and full-stack environments.

## Required Skills

### CPU Microarchitecture
- ARM v8 / v9 architecture (preferred).
- Low-level firmware and architecture-specific code.

### Subsystem Fundamentals
- Caches, MMU, interrupts, debug subsystems, exception handling.

### SW–HW Stack
- One or more of: RTOS, Linux kernel, trusted firmware (TF-A), hypervisors, device drivers.
- Strong bring-up + debugging skills in complex environments.

---

## Mapping to your prep (silicon-val-prep)

| JD bullet | Your day | Your experience hook |
|---|---|---|
| Bare-metal + full-stack val infra | day11, day17 | MS EdgeOptimizer post-Si profiling stack |
| Custom CPU feature functional val | day09, day11 | Intel Zebu pre-Si CPU feature val |
| DVFS / low-power for CPU | day12, day13, day16, day17 | Intel C-state, ADR, P-channel handshake |
| Caches / MMU / interrupts / excp | day05, day06, day07 | — |
| ARM v8/v9 | day10 (memory model), day07 (SMMU) | (gap — never claimed Oryon-specific) |
| TF-A / Linux / hypervisor / TEE | — | Linux kernel + driver exposure from MS |
| Debug + profiling | day17 | turbostat / socwatch / ETL / WPA / perf |

## Pitch angle for YOE (8 yrs, band is 4–15)

In-band, no gap to pre-empt. Lead with: **pre-Si Zebu (Intel, 3 yrs) + post-Si AI SoC (MS, 2 yrs) = full pre→post loop**, the exact spectrum this role spans.

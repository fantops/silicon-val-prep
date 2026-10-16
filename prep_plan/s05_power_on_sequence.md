# S05 - Power-on: rails to reset release

> This guide is the working session; archived day notes are optional deeper references.

## Session brief

- **Primer (generic x86, public-level):**
  1. The **always-on/RTC well** powers first, so wake logic survives every sleep state.
  2. **Standby rails** come up. The platform controller's always-on domain leaves reset (RSMRST#), and management-engine firmware starts from SPI flash.
  3. Power button or wake → SLP_S5#/S4#/S3# deassert → **main rails ramp in order**. Each VR reports **power-good**.
  4. All power-goods → PWROK → **PLLs lock** → **fuses sensed** (configuration, V/F limits) → **PM controller firmware** (PCU class) starts.
  5. **PLTRST# deasserts.** The CPU leaves reset. PCIe devices see **PERST# deassert with a stable refclk**.
- **Must answer:**
  - Why is rail and power-good ordering strict, and what does a late rail cause?
  - Which domains are always-on, and why must wake logic live there?
  - Debug "doesn't power on": what do you check first (power-good chain, resets, POST codes)?
- **Work link:** on your test platform, what is the earliest point where the DSA device could be reached (after which reset, and after which firmware stage)?
- **Handoff →** The CPU is out of reset. What does it run first, and how does it reach the OS?

## Study notes

### Power-on sequence: rails to reset release

A platform starts in an always-on/RTC domain so that reset and wake logic remain powered while the rest of the system is off. Standby rails stabilize first; the always-on controller exits its reset. A power-button or wake event requests the main power sequence. Regulators ramp in dependency order and report power-good. Only after the required rails are valid should the platform assert power-ok, release clocks/PLLs, sample fuses, and start the power-management controller. Platform reset and endpoint reset are released later, with the PCIe reference clock stable before PERST# deassertion.

```text
always-on/RTC -> standby rails -> power-good -> main rails -> PWROK
-> clocks/PLL lock -> fuse/config capture -> platform firmware
-> PLTRST# release -> device refclk stable -> PERST# release -> enumeration
```

- **Always-on domain:** holds wake detection, reset sequencing, and state needed to restart switched domains.
- **Power-good is a contract:** downstream logic must not be released while its supply is outside the valid range.
- **Reset sequencing is dependency-driven:** a block may need a stable rail, reference clock, and configuration before reset release.
- **Board details vary:** signal names and exact ordering are platform-specific; use the schematic, power-sequencing spec, and POST/trace evidence rather than assuming generic timing.

### Worked example: device missing after cold boot

If an integrated endpoint is absent, establish the earliest failed milestone instead of starting with the driver: check the rail power-good chain, then reference-clock presence, reset assertion/deassertion timing, platform POST progress, and finally config-space visibility. If the device appears only after a warm reset, compare cold-boot rail ramp and reset timing against the successful sequence.

### Failure patterns

| Symptom | Likely layer | First evidence |
|---|---|---|
| No POST progress | rail, clock, or reset dependency | power-good timeline, reset state, POST code |
| Discrete endpoint absent | refclk / PERST# / link training | scope/trace, LTSSM, config response |
| RCiEP absent | integrated power/reset/config exposure | internal reset status, firmware enumeration, BDF scan |
| Intermittent cold boot | sequencing margin or power-good glitch | rail waveform and reset-to-clock timing across failing seeds |

### Exit check

- [ ] Draw the power-good -> clock lock -> reset-release dependency chain for the target platform.
- [ ] Name the first always-on state that can detect a wake request.
- [ ] Given a missing device, identify what evidence separates power/reset failure from enumeration failure.
- [ ] Explain why exact rail/reset timing must come from platform documentation.

## Exit check

- [ ] Explain the session flow from memory, including the key state or ordering dependency.
- [ ] Give one realistic failure signature and the first observable that would distinguish it.
- [ ] Answer the work-link prompt in the session brief at public/spec level.

## Navigation

Previous: [S04](s04_metastability_cdc.md) | [Foundation plan](FOUNDATION_PLAN.md) | Next: [S06](s06_firmware_boot.md)

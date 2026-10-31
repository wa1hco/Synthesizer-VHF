# Synthesizer-VHF firmware review (as-built branch)

2026-10-30 · `wa1hco/Synthesizer-VHF` branch `as-built` at `6753e29`, `software/`. Read only, no files changed.
This replaces the earlier review, which was of an older synced copy.

## What the system is

- ATtiny1616 at 20 MHz internal, bit-banging 3-wire SPI to an ADF4351 (CLK PC1, DATA PC0, LE PB0, CE PB1, PDBRF PC3, MUXOUT PC2, LD PA1). The reference is the 25 MHz TCXO.
- `adf4351_calc.cpp` is the new tuner. It uses fundamental feedback (R4 DB23 = 1), picks the RF divider, prefers an exact integer-N solution, then an exact fractional-N solution sweeping R from 1 to 200, and falls back to the best Farey approximation at R = 1. It also packs and unpacks R0 to R5.
- `ADF4351.cpp` holds the globals, `setup()`/`loop()`, and the SPI. The keyboard tuning is unchanged. `UserInterface.cpp` and the EEPROM code are still not wired in.
- `test/test_adf4351.cpp` is a host test that decodes the packed words independently.

## Status of the earlier findings

Fixed:
- SPI data is sent now.
- R and Fpfd are written through to `r_counter`.
- The remainder math is now exact 64-bit integer arithmetic instead of a 16-bit float.
- INT ≥ 23/75 is enforced.
- The band-select divider is rounded up and recomputed for the final R.
- FRAC < MOD is guaranteed.
- The prescaler switches at 3.6 GHz.
- The disassemble masks are correct.
- In integer-N, charge cancel and ABP 3 ns are set.
- The float Farey code is excluded from the build.

**Verified:** `scripts/test-adf4351` passes (`ok 4389 frequencies`). My own sweep agrees. Across 50 to 54 MHz and 144 to 148 MHz in 1 kHz steps, and 144.19 to 144.21 MHz in 10 Hz steps, every channel tunes **exactly** with no rejects. At 1 Hz steps the worst error is 3.2 Hz.

---

## High

### H1. Integer-N preference drops the PFD to 200 kHz on "round" channels
The tuner tries integer-N first. On a 25 MHz reference, integer-N on round frequencies usually needs a large R. The fractional-N branch at R = 1 (25 MHz PFD) would hit the same frequency exactly, but it is never tried.

| Fout | Chosen | N | Neighbor |
|---|---|---|---|
| 144.170 MHz | frac, Fpfd 25 MHz | 92.27 | |
| **144.200 MHz** | **int, Fpfd 200 kHz** | **11536** | |
| 144.000 MHz | int, Fpfd 1 MHz | 2304 | |
| 50.200 MHz | int, Fpfd 200 kHz | 16064 | |
| 432.100 MHz | int, Fpfd 200 kHz | 17284 | |

Of the 4,001 channels on 2 m at 1 kHz steps, 128 land at 200 kHz, 27 at 1 MHz, 5 at 5 MHz, and the rest at 25 MHz. The effects:
- **In-band phase noise.** The floor scales as 10·log(Fpfd) + 20·log(N). Going from 25 MHz/N≈92 to 200 kHz/N≈11536 is about **21 dB worse**, and it falls on 144.200, the SSB calling frequency.
- **Loop bandwidth and phase margin.** With fundamental feedback, loop gain scales with 1/N, so these channels have about 125× less loop gain than their neighbors. One fixed loop filter (and CP = 2.5 mA) can't be right for both. Lock time also stretches on the low-PFD channels.

**Suggested fix:** pick the smallest R that gives an exact solution, whether integer or fractional. That means running the R = 1..r_hi loop first, treating `rem == 0` as integer-N at that R, and falling through to the GCD branch only if nothing is found. With that change 144.200 becomes R = 1, INT 92, FRAC 36/125. The golden words for 50.200 MHz in the test would need updating. If you'd rather keep integer-N for its spur performance, cap R so that Fpfd stays near whatever the loop filter was designed for.
*Question for you: which PFD was the loop filter (680p / 6.8n / 2.7n …) designed for? That decides the cap.*

#### Loop filter reverse-engineered (added after review)

**History:** the loop filter values have been unchanged since the first schematic commit (`dc18e45`, 2024-12-20). No commit message or file records a design PFD. `Phase Noise Calculations.ods` covers regulator, VCO, and TCXO noise only.

**Circuit from the PCB netlist:** a third-order passive filter.
- CPout to GND through C1 = C20, 2.7 nF.
- CPout to GND through C2 = C17 (4.7 nF), in series with R2 = R14 (560 Ω) and R20 (0 Ω).
- CPout to Vtune through R3 = R25 (1 kΩ), with C3 = C31 (680 pF) to GND.

Other values: RSET = R22 = 4.7 kΩ, so CP code 7 gives about 2.7 mA. Kv is taken as 40 MHz/V (the datasheet typical).

C1, C3, R3, and RSET match the ADF4350 eval board (UG-109) exactly. **The eval board's C2 is 47 nF.**

**Model results** (ideal small-signal, 2 m, VCO about 2.31 GHz, CP 2.7 mA):

| Filter | Best phase margin | at loop BW | PFD that puts you there |
|---|---|---|---|
| As built (C2 = 4.7 nF) | **about 10°** at any PFD | about 50 kHz | about 2.3 MHz |
| C2 = 47 nF | about 56° | about 19 kHz | about 0.8 MHz |

As built, the zero (60 kHz) and the C1/C2 pole (165 kHz) are too close together to give real phase lead. The loop is badly under-damped whatever PFD the firmware picks, and at the 25 MHz PFD that the tuner uses for most channels the margin is negative. Changing CP current moves the best-fit PFD (it scales with Icp) but doesn't fix the shape.

**Conclusion:** C17 = 4.7 nF looks like a decade slip from the eval board's 47 nF. Fix that before tuning the firmware's PFD choice. Even with 47 nF, a 25 MHz PFD would need a new filter design, because it would need Icp below the chip's minimum. A bench check would confirm all this: look for noise peaking or a spur shoulder at about 50 to 200 kHz offset on a spectrum analyzer.

### H2. Lock detect is still never checked, and RF is live before programming
- `InitPins()` drives CE and PDBRF high immediately. `setup()` then waits 7 s before the first SPI write, so the output carries whatever the chip powers up into for 7 s.
- `mute_till_ld = 0`, and nothing reads LD after a tune. The LD LED only updates after a keypress.

**Fix:** hold RF_EN (PC3) low until after the first `updateAllRegisters()`, then poll LD with a short timeout and print LOCKED or UNLOCKED. Consider `mute_till_ld = 1`.

---

## Medium

- **M1. Fout wraps to about 4.29 GHz.** Starting near 35 MHz, repeated presses of '5' (−10 MHz) give 25 M → 15 M → 5 M, each rejected. The next press underflows the `uint32_t` to 4,289,967,296 Hz, which **is** in range, so the chip tunes there. Also, on any rejection Fout keeps the bad value while the registers keep the old one. **Fix:** compute the new value into a temporary, and commit it only if `calculate_fields()` succeeds (or clamp to 34.375 MHz to 4.4 GHz).
- **M2. MUXOUT defaults to R-counter output** (`muxout = 3`, and the test's `board_defaults()` locks it in). This puts a square wave at the PFD rate on a pin near the loop, which can couple in as reference spurs. Use 6 (digital LD) or 0.
- **M3. Fractional spurs with small MOD.** Many channels use MOD 125, 625, or 3125 at a 25 MHz PFD. This gives fractional spurs at 25 MHz/MOD multiples (40 to 200 kHz at the VCO, divided down at the output). That is not a bug, but it is worth a spectrum check. `noise_mode = 3` (low spur) is available if the spurs show up.

## Low / cleanup

- `Fout` is a `uint32_t` but the tuner accepts up to 4.4 GHz. Irrelevant for VHF, but the global can't represent anything above 4.29 GHz.
- **Unmeasured AVR cost:** a frequency with no exact solution sweeps up to 200 values of R, each with a 64-bit divide and a gcd. I estimate a few hundred ms worst case on a 20 MHz AVR. That is fine for keypress tuning, but worth a timing print if you later add sweeping.
- `nmin` uses `fvco + fref > PRE_MAX`, which demands INT ≥ 75 for VCO between 3.575 and 3.6 GHz. That is conservative and harmless.
- `UserInterface.cpp` is unchanged and still dormant, so the earlier notes stand:
  - `UpdateConfig;` is missing `()`.
  - The invalid-command branch dereferences a NULL `Token`.
  - `FreqCmd()` returns 0 on error.
  - `errno` isn't reset.
  - `SerialReadLine.h` defines `reader` in a header.
- `Global.h` still declares `extern uint32_t Fvco`, which is never defined.
- `'p'` sends R2 and then the full R5..R0 resend runs anyway, so the special case is redundant.

## What checked out

- **R0 to R5 bit packing and unpacking** match the datasheet. The test checks the reserved bits independently.
- **Tuning math:** all integer arithmetic is exact. The VCO window, prescaler, PFD maximums (32/45 MHz), band-select clock (≤125 kHz low mode, ≤500 kHz high mode), and the integer-N vs fractional-N LDF/LDP/ABP/charge-cancel settings are all correct.
- **SPI:** MSB first, LE latch after 32 bits, R5 down to R0 with R0 last. The timing has huge margin.

## Suggested next steps

1. Prefer the smallest exact R over integer-N (H1), after confirming the loop filter's design PFD.
2. Keep RF off until programmed, and report lock (H2).
3. Make the Fout step atomic (M1), and default MUXOUT to digital LD (M2).

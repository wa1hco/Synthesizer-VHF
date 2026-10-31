# Firmware rework plan for diecast-2x2

Plan only. The board is the diecast-2x2 schematic: ADF4351, an on-board 10 MHz TCXO with an external REF IN mux, ATtiny1616, FT231X. The ADF4351 loop filter on that schematic is the one this firmware must match. There is no ADF4002 and no REF OUT.

Host checks stay behind `scripts/` (`scripts/test-adf4351`, `scripts/build-firmware`, `scripts/flash-firmware`). Launchers take no flags and no environment variables. A person runs the script; the script knows the paths. New checks are added the same way.

`software/firmware-review.md` is the review this plan implements.

## 1. Hardware baseline

Reference chain:

- The synthesizer reference is 10 MHz, not 25 MHz. `ADF4351_REF_HZ` is still `25000000` until the firmware change.
- J5 is external REF IN: 51 ohm to ground, 100 nF into U13. U13 is the 74LVC1GU04 already on the sheet, with a 1 MΩ feedback resistor. Its output net is REF_BUF.
- X2 is the on-board fallback, YXC OW2EL89CEIUXK7YLC-10M (LCSC C55266004), 10 MHz clipped sine, 3.3 V. A 10 ohm resistor and 100 nF isolate its supply. U14 (74LVC1G17, mid-supply bias) squares it. Pin 1 is output-enable and is left open; this family treats a floating enable as on.
- U15 is a 74LVC1G157. I0 is the squared TCXO, I1 is REF_BUF. S low selects I0. S is PA6, net REF_SEL, with a 10 kΩ pulldown, so a blank chip uses the TCXO. Drive PA6 high to select the external reference.
- PA5, net REF_DET, is REF_BUF through 1 kΩ. Count edges there to decide that an external reference is present. U13 inverts, which does not matter for an edge count.
- The mux output goes through 33 ohm and C3 into ADF4351 REFIN.
- R22 on the ADF4351 is 4.7 kΩ, so Icp max = 25.5/4.7 = 5.43 mA at Register 2 DB[12:9] = 1111. Code 0011 is 1.36 mA. The 10 MHz integer-N filter was designed at 0011. The 2.5 MHz fine-step filter uses 1111, which is exactly 4× current for 4× N. The firmware comment that assumes a 5.1 kΩ RSET is wrong for this board.

ATtiny1616 (U3), every pin, QFN-20. Chip pin numbers match the package drawing in `HardwareConfig.h`. Direction is MCU direction.

| Chip | Port | Net | Use |
|---|---|---|---|
| 1 | PA2 | DBG1 | Test header J2 pin 3. |
| 2 | PA3 | DBG2 | Test header J2 pin 4. |
| 3 | GND | GND | |
| 4 | VCC | VDD | 3.3 V |
| 5 | PA4 | RST | Optiboot ersatz reset. FT231X DTR, C54 100 nF, PA4. R33 10 kΩ to VDD. |
| 6 | PA5 | REF_DET | Input. Squared REF IN through 1 kΩ. Edge-count to detect an external reference. |
| 7 | PA6 | REF_SEL | Output. Mux select. Pulled down, so low selects the TCXO. High selects REF IN. |
| 8 | PA7 | unconnected | No connection. Do not drive it. |
| 9 | PB5 | SYNTH_LOCK | SYNTH LOCK LED D13, red, active high, R39 1 kΩ to the anode, cathode to GND. About 1.2 mA. |
| 10 | PB4 | REF_LOCK | REF LOCK LED D12, red, active high, R38 1 kΩ. About 1.2 mA. |
| 11 | PB3 | RXD_M | MCU RX from FT231X TXD through R16 1 kΩ. |
| 12 | PB2 | TXD_M | MCU TX to FT231X RXD. |
| 13 | PB1 | CE | ADF4351 CE. Hardware pull-down R48 100 kΩ. |
| 14 | PB0 | LE | ADF4351 LE. Hardware pull-down R47 10 kΩ. |
| 15 | PC0 | DATA | SPI data to the ADF4351. |
| 16 | PC1 | CLK | Shared SPI clock. |
| 17 | PC2 | MUX | ADF4351 MUXOUT, input. |
| 18 | PC3 | RF_EN | ADF4351 RF output enable (PDBRF). Hardware pull-down R49 100 kΩ. Low mutes both RF outputs. |
| 19 | PA0 | UPDI | J12 pin 1. J12 is UPDI, VDD, GND. Recovery programming. Not a GPIO. |
| 20 | PA1 | LD | ADF4351 lock-detect pin, input. |
| EP | | GND | Exposed pad. |

The other two LEDs are not MCU pins:

- D11 PWR, green, anode to VDD through R37 1 kΩ, cathode to GND. On whenever 3.3 V is up.
- D14 COMM, green, anode to VDD, cathode through R40 1 kΩ to FT231X CBUS2 (pin 7). Active low. Program CBUS2 as TX&RXLED#.

FT231X VCCIO is VDD (3.3 V). MCU serial is already 3.3 V. Boot is Optiboot on the UART (PB2/PB3) with the DTR capacitor reset. A failed flash is recovered on J12 with UPDI, not by bit-banging PA0 from the application.

SPI is the ADF4351 only. LE (PB0) idles low, which is also the pull-down, so a reset does not latch a partial word.

## 2. Tuning modes

Two modes. The mode is part of the operating setup the firmware already knows (transverter LO versus IMD measurement). It is not a command-line flag.

Charge-pump codes below are for R22 = 4.7 kΩ. Noise mode: Register 2 DB[30:29], 0 = low noise, 3 = low spur. Feedback stays fundamental (Register 4 DB23 = 1), which is what `adf4351_calculate` already writes:

```
RFOUT = [INT + FRAC/MOD] × fPFD / RF_divider
fPFD  = 10 MHz / R
```

### Transverter LO

Fixed fPFD = 10 MHz. R = 1, doubler off, RDIV/2 off. Icp code 0011 (1.36 mA). Low-noise mode. Output divider 1 whenever the wanted synthesizer frequency is inside 2.2–4.4 GHz. Below 2.2 GHz, use the smallest on-chip divider that puts the VCO back in that range.

Integer-N when the VCO is a multiple of 10 MHz. With divider 1 that is the same statement as "the LO is a multiple of 10 MHz." Otherwise a small MOD is allowed. Do not hunt R. R is 1.

Starting points for the usual weak-signal IFs. "INT" means the VCO lands on a 10 MHz grid. Where 144 MHz does not, the row is the conventional LO anyway, and the calculator uses a small MOD. A higher IF is in bounds when it is what makes the VCO an integer multiple of 10 MHz; 28 and 144 stay the defaults.

| Band | IF | Side | Multiply | Synth | Divider | VCO | Grid |
|---|---|---|---|---|---|---|---|
| 1296 | 144 | high | 1 | 1440 | 2 | 2880 | INT |
| 1296 | 144 | low | 1 | 1152 | 2 | 2304 | frac |
| 2304 | 144 | low | 1 | 2160 | 2 | 4320 | INT |
| 2304 | 144 | high | 1 | 2448 | 1 | 2448 | frac |
| 3456 | 144 | high | 1 | 3600 | 1 | 3600 | INT |
| 3456 | 144 | low | 1 | 3312 | 1 | 3312 | frac |
| 5760 | 144 | low | 2 | 2808 | 1 | 2808 | frac |
| 5760 | 144 | high | 2 | 2952 | 1 | 2952 | frac |
| 10368 | 144 | low | 4 | 2556 | 1 | 2556 | frac |
| 10368 | 144 | low | 3 | 3408 | 1 | 3408 | frac |
| 10368 | 432 | high | 4 | 2700 | 1 | 2700 | INT |
| 24048 | 144 | low | 8 | 2988 | 1 | 2988 | frac |
| 24048 | 432 | high | 8 | 3060 | 1 | 3060 | INT |
| 24192 | 144 | low | 8 | 3006 | 1 | 3006 | frac |
| 24192 | 432 | low | 8 | 2970 | 1 | 2970 | INT |
| 47088 | 144 | low | 16 | 2934 | 1 | 2934 | frac |
| 47088 | 432 | high | 16 | 2970 | 1 | 2970 | INT |

5760, 10368, 24 GHz, and 47 GHz with a 144 MHz IF are not 10 MHz multiples at the VCO, for any small integer multiplier. Those rows are small-MOD. Integer-N on those bands means picking an IF that the table already shows (432 MHz on the higher bands) or another IF the operator actually uses.

On-chip divider adds 20·log10(divider) of phase-noise improvement at the synth output and is the right tool for 1152 and 2160 MHz. External multiplier chains do the opposite; see section 6.

### IMD fine-step

fPFD = 2.5 MHz (R = 4). Icp code 1111 (5.43 mA), so Icp/N matches the 10 MHz design and the same loop filter stays at the same bandwidth and phase margin. MOD up to 4095. Low-noise mode unless the operator is doing the A/B spur comparison in section 3.

A 144 MHz output uses VCO 2304 MHz and divider 16. The output step is

```
fPFD / (MOD × RF_divider) = 2.5e6 / (4095 × 16) ≈ 38 Hz
```

That is inside the 10–50 Hz target. Code 1111 is already the maximum current. A lower fPFD would shrink the loop bandwidth (N rises, Icp cannot rise) and needs a fresh bandwidth and phase-margin check before it becomes a mode. Do not add that mode in the first pass.

## 3. IM3-avoiding spur check

Used when two tones f1 < f2 are programmed, delta = f2 − f1. The IM3 products sit at f1 − delta and f2 + delta, which is ±delta from the near tone and ±2·delta from the far tone.

For each tone, after `adf4351_calculate` (or the mode-specific replacement):

- Reduced modulus: `MOD_red = MOD / gcd(FRAC, MOD)`. Fractional-spur offsets at the VCO are `k · fPFD / MOD_red`. At the output they are divided by `RF_divider`.
- Integer-boundary offset at the output: `min(FRAC, MOD−FRAC) / MOD · fPFD / RF_divider`.

If any of those offsets falls within the measurement RBW of delta or of 2·delta, the spur can be read as an IM3 product. Default RBW for this check is 10 Hz, because that is the bandwidth in the phase-noise margin below. The IMD script owns that number. It is not an environment variable.

Nudge, in order, and keep both tones on the same fPFD and the same Icp code:

1. Try a nearby MOD (and the FRAC that holds the frequency), still ≤ 4095, still a legal step.
2. If the mode still has fPFD room that does not change Icp/N, try the other legal R. For the first IMD pass, R stays 4. Do not walk R from 1 to 1023.
3. Shift both tones by a few hertz in the same direction so delta is unchanged. A few means a few output steps, not a retune of the IF. Recompute both spurs. The integer-boundary distance moves; the fractional grid spacing does not, until MOD or fPFD changes.

Stop at the first candidate that clears both tones. If none clears, keep the original tones, leave the low-spur bit off, and report that the spur check failed so the measurement is not treated as clean.

Optional A/B: the same tones with Register 2 noise mode forced to low spur (dither on) versus low noise. One comparison, same fPFD and Icp, so the loop does not move. Default remains low noise.

## 4. Review items

H1. Delete the R sweep in `adf4351_calculate` (the loop `for (rr = r_lo; rr <= r_hi; rr++)`, and the "smallest R that still fits" fallback under it). Each mode has one R. The sweep existed to approximate an arbitrary frequency from a 25 MHz reference down to a 125 kHz PFD floor. That floor is not this board's design.

H2. RF stays off until the registers are written and lock is real. Boot and retune order:

1. PC3 low, and CE (PB1) low, before any register write. The pull-downs already do this at reset; the firmware holds it.
2. Write all six ADF4351 registers with the RF muted.
3. Poll PA1. ADF4351 MUXOUT and the LD pin are both digital lock detect (section M2). Timeout is short, on the order of a few milliseconds times a small constant, because the loop is 50–70 kHz. On timeout, leave RF off and SYNTH LOCK (PB5) off.
4. Count edges on PA5 (REF_DET) for a short window. Edges mean an external 10 MHz is present: drive PA6 high so the mux selects REF IN, and light REF LOCK (PB4). No edges: leave PA6 low (the pulldown already does this) so the mux stays on the TCXO, and leave REF LOCK off.
5. Only then raise PC3.

If the external reference disappears later, drop PA6 and REF LOCK. The TCXO is still the ADF4351 reference. Check PA5 again on a slow cadence so a reference that comes and goes does not chatter the mux.

M1. One frequency update is one local `Adf4351Fields`, validated, then one programming burst. Validation is: calculate returned true, the reassembled frequency matches the request within one output step, VCO is inside 2.2–4.4 GHz, and Icp and R are the mode's values. On failure, do not write the chip and do not publish the new frequency. The live globals (`Fout`, `Fvco`, `reg[]`) update only after that success. Today a key handler can call `sendRegisterToAdf` and then fall through into `updateAllRegisters`.

M2. Default MUXOUT is digital lock detect (code 6), and the LD pin mode is digital lock detect (code 1). `InitFields` currently leaves `muxout` at 3, which is the R-divider output, so PA1 and PC2 do not mean lock until something changes it. The mode setup writes both explicitly. PA5 is REF_DET, not a mux output.

Later, not in the first steps:

- Dormant `UserInterface` command parser (`Boot`, `Init`, the single-key mux and power toggles). It still targets the sequencer pin map and the 25 MHz reference. Leave it until the two modes program the chip without it.
- `Fvco` as a global extern. The calculator already returns the divider and INT/FRAC/MOD; the VCO frequency should be a result of that struct, not a second source of truth.
- The `p` key toggles `powerdown`, writes register 2, then falls through into `calculate_fields` and `updateAllRegisters`, so the single write is immediately overwritten. Remove that fall-through when the key parser is touched. Until then, do not build the new tune path on top of `handleKey`.

## 5. Tests

Keep the host suite. `scripts/test-adf4351` compiles `adf4351_calc.cpp` with `test/test_adf4351.cpp` and runs it. No board, no PlatformIO, no flags.

Regenerate golden register words for a 10 MHz reference. The current file asserts `ADF4351_REF_HZ == 25000000` and checks words built with R = 25 (1 MHz PFD from 25 MHz). Those words will not match R = 1 at 10 MHz. Replace them; do not weaken the comparison to "any legal tune."

Add, in the same host binary so `scripts/test-adf4351` still runs everything:

- Mode selection. A 3600 MHz transverter request is integer-N, R = 1, fPFD = 10 MHz, Icp code 0011, divider 1, low noise. A 144 MHz IMD request is R = 4, fPFD = 2.5 MHz, Icp code 1111, divider 16, MOD ≤ 4095, output step about 38 Hz. A 1152 MHz request may be fractional. A 3600 MHz request may not.
- Spur check. A constructed pair whose integer-boundary or fractional offset lands within 10 Hz of delta or 2·delta is nudged, delta unchanged, and the nudged pair is clean. A pair that is already clean is unchanged. A pair that cannot be cleaned returns the failure the firmware will report.
- Lock-policy unit check, without GPIO: RF-enable is false when calculate fails or when the lock flag is false, and true only after a successful program and a true lock flag. REF_DET edges select REF IN (PA6 high); no edges leave PA6 low so the mux stays on the TCXO.

The on-chip sweep over 35 MHz–4.4 GHz can stay as a "legal registers" property test, pointed at 10 MHz.

## 6. Phase noise budget (estimates)

These are estimates for choosing the mode, not a measured plot.

ADF4351 normalized integer-N floor is about −221 dBc/Hz. In-band floor at the VCO:

```
L ≈ −221 + 10·log10(fPFD) + 20·log10(Fvco / fPFD)    dBc/Hz
```

The output divider subtracts `20·log10(RF_divider)`.

IMD mode, 144 MHz out, Fvco = 2304 MHz, divider 16, fPFD = 2.5 MHz, N = 921.6:

```
10·log10(2.5e6) ≈ 64.0
20·log10(921.6) ≈ 59.3
20·log10(16)    ≈ 24.1
L ≈ −221 + 64.0 + 59.3 − 24.1 ≈ −122 dBc/Hz
```

Fractional operation is several dB worse than that integer floor, so the in-band estimate at 144 MHz is about −115 to −122 dBc/Hz.

Close-in, the reference dominates. There is no narrow disciplining loop anymore. A 10 MHz TCXO or external reference near −130 dBc/Hz at 100 Hz, brought up to 144 MHz, picks up `20·log10(144/10) ≈ 23 dB`, which lands near −107 dBc/Hz at 100 Hz. That number moves with the actual source. It is not the floor that sets IM3 at a tone spacing of kilohertz or more. These figures are estimates.

IM3 margin at the 144 MHz synth output, −50 dBc products, 10 Hz RBW. Phase noise in that bin is `L + 10·log10(10) = L + 10`:

| In-band L | Power in 10 Hz | Margin to −50 dBc |
|---|---|---|
| −115 dBc/Hz | −105 dBc | 55 dB |
| −122 dBc/Hz | −112 dBc | 62 dB |

About 50–60 dB of margin at 144 MHz. A spur that fails section 3 can eat this margin even when the floor is fine, which is why the spur check exists.

External multipliers add `20·log10(M)` to the LO phase noise and to spur level at the multiplied frequency. ×2 is 6 dB, ×4 is 12 dB, ×8 is 18 dB, ×16 is 24 dB. The 50–60 dB figure is at 144 MHz. A ×16 chain to 47 GHz gives back 24 dB of it. Do not quote the 144 MHz margin at 47 GHz.

## 7. Implementation order

Each step should leave `scripts/test-adf4351` passing. Golden words change only in the step that changes the reference.

1. Pin map. Replace the sequencer table in `HardwareConfig.h` with section 1. Idle levels: LE, CE, and RF_EN low; REF LOCK and SYNTH LOCK off; PA6 low so the mux stays on the TCXO; PA7, PA2, and PA3 untouched. No frequency math yet.
2. Reference constant 10 MHz. Regenerate the golden words for R = 1, fPFD = 10 MHz, on the frequencies the suite already cares about. Delete the assertion that the reference is 25 MHz.
3. Mode struct: transverter versus IMD, with the R, Icp code, noise mode, and divider rule from section 2. Tests for 3600 MHz integer-N and 144 MHz fine-step. Calculator takes that R instead of searching.
4. Remove the R sweep (H1) once step 3 is the only caller.
5. MUXOUT and LD pin default to digital lock detect (M2), in the mode setup, covered by a register-word assertion.
6. Atomic tune (M1): local fields, validate, then commit. Failed tune does not change published `Fout` or the chip image the test records.
7. Lock policy (H2), still testable as a pure function of "program ok", "LD bit", and "REF_DET edges": RF_EN and the two lock LEDs. Edges map to PA6 high. No edges map to PA6 low.
8. Wire the real GPIO behind that policy. This is the first step that needs the board.
9. Spur check (section 3) and its host tests. Tone nudge preserves delta.
10. Later cleanup: `UserInterface` parser, `Fvco` extern, the `p` fall-through. Only after steps 1–9 no longer call that path.

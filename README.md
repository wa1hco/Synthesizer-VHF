# Synthesizer-VHF

An ADF4351 synthesizer, 35 MHz to 4.4 GHz, with an ATtiny1616 and USB. This branch, `diecast-2x2`, is the Hammond 1590LB version: the board is the lid of the box. Designed by Jeffrey Millar (WA1HCO).

The first board in this repository was built and sent to fab with a TCXO, two RF outputs, USB serial, and a 12 V input. SPI between the microcontroller and the PLL is wired to ordinary pins, and the firmware bit-bangs it. That built board is the `as-built` branch. `main` carries the later layout of that same board, including the four-layer stackup and the 50 ohm coplanar RF routes. `diecast-2x2` is the new enclosure and a new 10 MHz reference chain. It has not been built.

Open the board from `hardware/Synthesizer-VHF.kicad_pro`. The project is KiCad 10.

## diecast-2x2

The PCB is the lid of a Hammond 1590LB. Parts sit on the inside face. The SMA connectors and the USB-C receptacle sit on the outside face. A contact strip on the rim ties the board ground to the box. Four mounting holes (H1–H4) match the 1590LB #6-32 pattern, 40.5 mm square, and are bonded to ground.

The synthesizer is an ADF4351. Its VCO covers 2.2–4.4 GHz, and the on-chip divider brings the outputs down to 35 MHz. Two RF jacks leave the box.

The reference is a 10 MHz VCTCXO. An ADF4002 locks it to an external reference when that reference is there. REF IN is 50 ohm and is squared (inverter with feedback) before the ADF4002. With no external reference, the charge pump is tri-stated and the microcontroller holds the VCTCXO tune line from its DAC. REF OUT is the squared 10 MHz clock after a series resistor and a capacitor to ground, which softens the edges. It is not a sine-wave filter.

The controller is an ATtiny1616. An FT231X provides USB-C serial. Everyday firmware updates use Optiboot: plugging in USB resets the chip through DTR and a capacitor. If that path is broken, the UPDI header (J12) is the recovery programmer. A 1×5 header (J2) brings lock detect, mux, and two debug lines out to one place, with ground.

Power is 5 V from USB, or 12 V through a regulator, joined by Schottky diodes so either source can run the board. Two LP5907 regulators make 3.3 V: one for the VCO, and one for the PLL analog rail. The digital 3.3 V rail comes from that analog rail through a ferrite bead.

Four reverse-mount LEDs shine through the lid: power, USB activity, reference lock, and synthesizer lock.

## Intended uses

Microwave transverter local oscillators, from 1296 MHz through 47 GHz, with external multipliers where the band is above the ADF4351. The synthesizer runs integer-N with a 10 MHz phase-detector frequency when the VCO lands on a 10 MHz multiple, and a small fractional modulus otherwise. The output divider stays at 1 when the wanted frequency is already inside 2.2–4.4 GHz.

SSB amplifier IMD testing, with several boards sharing the 10 MHz reference so the tones stay synchronized. That mode uses a finer fractional-N step and checks that fractional and integer-boundary spurs do not land on the IM3 products.

Both modes are described in `software/FIRMWARE-PLAN.md`. Any phase-noise numbers in that plan are estimates. This board has no measured phase noise.

## Status

The diecast-2x2 hardware is in layout. The schematic is the working design; the board has not been built or tested.

The firmware in this branch is still the as-built firmware. It assumes a 25 MHz reference. The rework for the 10 MHz reference, the two operating modes, and the lock behavior is planned in `software/FIRMWARE-PLAN.md` and is not done yet.

## Branches

- `main` — the original board, with the layout work that followed the fab order (stackup and 50 ohm RF routing).
- `as-built` — the board that was built, and the firmware written for it.
- `diecast-2x2` — this Hammond 1590LB variant. Schematic in progress toward a new layout; firmware not yet moved off the 25 MHz design.

## Directory layout

- `hardware/` — KiCad project (`Synthesizer-VHF.kicad_pro`), design rules, the phase-noise spreadsheet, and fabrication-toolkit options.
- `hardware/libraries/` — project symbol and footprint libraries. See Libraries below.
- `hardware/datasheets/` — component datasheets.
- `hardware/jlcpcb/` — fab outputs committed with the board (drill maps and the fabrication-toolkit database). New files generated into that directory are left untracked.
- `software/` — PlatformIO firmware for the ATtiny1616. `src/` and `include/` are the firmware, `lib/` is for private libraries, and `algorithms/` holds the fractional-N notes.
- `scripts/` — build, flash, and host-test launchers.

## Libraries

Project libraries sit next to the board. The library tables refer to them with `${KIPRJMOD}`:

- `hardware/libraries/Synthesizer.kicad_sym` — project symbols. The ADP150 symbol remains from the first board. This variant uses LP5907 regulators.
- `hardware/libraries/adf4350.kicad_sym` — ADF4350 symbol.
- `hardware/libraries/Synthesizer.pretty` — project footprints (0402 passives, the edge-mount SMA, the USB-C receptacle, the 2.5×2.0 mm oscillator, and the ADF4351 LFCSP).

Passives, connectors, the ATtiny1616, the FT231X, and their 3D shapes come from KiCad's standard libraries. Where a footprint has a 3D model, the path uses `${KICAD8_3DMODEL_DIR}`. KiCad 10 resolves that older name to its own 3D library (`KICAD10_3DMODEL_DIR`) unless `KICAD8_3DMODEL_DIR` is set on its own. This board has no separate project 3D model. The old MAX2870 download is not used, so it is not in the library tables.

KiCad 10 may warn that some standard symbols and footprints differ from the copies stored in the schematic and board. Those stored copies are the ones the layout uses.

## Build the firmware

Install [PlatformIO](https://platformio.org/). From the repository root, run:

```
scripts/build-firmware
```

That builds `software/` for an ATtiny1616 at 20 MHz on the internal oscillator.

The host check of the ADF4351 register math, with no board attached, is:

```
scripts/test-adf4351
```

## Flash the firmware

Install `pymcuprog` as well. The serial port is already set in `software/platformio.ini` for the FT231X on this project. Connect USB, then from the repository root run:

```
scripts/flash-firmware
```

Optiboot on the USB serial port is the usual path. J12 (UPDI, 3.3 V, ground) is there when the serial bootloader cannot take an update.

The current firmware still speaks the as-built serial console at 57600 baud. Single keys change the output:

| Key | Effect |
| --- | --- |
| `x` / `c` | −10 Hz / +10 Hz |
| `s` / `d` | −100 Hz / +100 Hz |
| `w` / `e` | −1 kHz / +1 kHz |
| `2` / `3` | −10 kHz / +10 kHz |
| `1` / `4` | −100 kHz / +100 kHz |
| `5` / `6` | −10 MHz / +10 MHz |
| `/` | toggle the RF output |
| `m` | cycle the ADF4351 muxout setting |

That console is the 25 MHz firmware. The diecast-2x2 behavior is the plan in `software/FIRMWARE-PLAN.md`.

## Older checkouts

Commit or stash any edits to `Hardware/Synthesizer-VHF.kicad_pcb` before updating a very old checkout. That file lives at `hardware/Synthesizer-VHF.kicad_pcb`.

## License

- Software (`software/`, `scripts/`): GPL-3.0-or-later. See `LICENSE`.
- Hardware design files (`hardware/`): CERN-OHL-S-2.0. See `LICENSE-HARDWARE`.

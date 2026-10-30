# Synthesizer-VHF

VHF synthesizer board built around an Analog Devices ADF4351 PLL, an ATtiny1616, a TCXO reference, USB serial, and a 12 V input with onboard regulators. The board has two RF outputs. SPI between the ATtiny1616 and the PLL is wired to general-purpose pins, and the firmware bit-bangs it.

The board design targets KiCad 10.

## Directory layout

- `hardware/` — KiCad project (`Synthesizer-VHF.kicad_pro`), design rules, the phase-noise spreadsheet, and fabrication-toolkit options.
- `hardware/libraries/` — project symbol and footprint libraries. See Libraries below.
- `hardware/datasheets/` — component datasheets.
- `hardware/jlcpcb/` — fab outputs committed with the board (drill maps and the fabrication-toolkit database). New files generated into that directory are left untracked.
- `software/` — PlatformIO firmware for the ATtiny1616. `src/` and `include/` are the firmware, `lib/` is for private libraries, and `algorithms/` holds the fractional-N notes.
- `scripts/` — build and flash launchers.

Open the board from `hardware/Synthesizer-VHF.kicad_pro`.

## Libraries

Project libraries are next to the board and the library tables refer to them with `${KIPRJMOD}`:

- `hardware/libraries/Synthesizer.kicad_sym` — project symbols, including the ADP150 regulator on the schematic
- `hardware/libraries/adf4350.kicad_sym` — ADF4350 symbol
- `hardware/libraries/Synthesizer.pretty` — project footprints (0402 passives, the edge-mount SMA, the USB-C receptacle, the 2.5×2.0 mm oscillator, and the ADF4351 LFCSP)

Passives, connectors, the ATtiny1616, the FT231X, and their 3D shapes come from KiCad's standard libraries. Where a footprint has a 3D model, the path uses `${KICAD8_3DMODEL_DIR}`. KiCad 10 resolves that older name to its own 3D library (`KICAD10_3DMODEL_DIR`) unless `KICAD8_3DMODEL_DIR` is set on its own. This board has no separate project 3D model. The old MAX2870 download is not used on this board, so it is not in the library tables.

KiCad 10 may warn that some standard symbols and footprints differ from the copies stored in the schematic and board. Those stored copies are the ones the layout uses. The project libraries match the symbols and footprints this design places.

The schematic, board, and these libraries stay in the KiCad 8 file format so KiCad 10 can open them without a conversion step. Saving any of those files from KiCad 10 rewrites them into the KiCad 10 format.

## Build the firmware

Install [PlatformIO](https://platformio.org/), then from the repository root:

```
scripts/build-firmware
```

That builds `software/` for an ATtiny1616 running at 20 MHz on the internal oscillator (megaTinyCore, via the PlatformIO `atmelmegaavr` platform).

## Flash the firmware

Install `pymcuprog` as well. The serial port and the UPDI upload command are already set in `software/platformio.ini` (the port there is the FT231X by-id path for this board, not `/dev/ttyUSB0`). Connect the programmer, then from the repository root:

```
scripts/flash-firmware
```

The serial console runs at 57600 baud. Single keys change the output:

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

## Updating a checkout that still has local board edits

Commit or stash any edits to `Hardware/Synthesizer-VHF.kicad_pcb` before updating. That file now lives at `hardware/Synthesizer-VHF.kicad_pcb`.

## License

- Software (`software/`, `scripts/`): GPL-3.0-or-later, see `LICENSE`.
- Hardware design files (`hardware/`): CERN-OHL-S-2.0, see `LICENSE-HARDWARE`.

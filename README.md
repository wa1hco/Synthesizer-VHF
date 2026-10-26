# Synthesizer-VHF

VHF synthesizer board built around an Analog Devices ADF4351 PLL, an ATtiny1616, a TCXO reference, USB serial, and a 12 V input with onboard regulators. The board has two RF outputs. SPI between the ATtiny1616 and the PLL is wired to general-purpose pins, and the firmware bit-bangs it.

The design is released under CC0 (see `LICENSE`).

## Directory layout

- `hardware/` — KiCad 8 project (`Synthesizer-VHF.kicad_pro`), project symbol libraries, design rules, the phase-noise spreadsheet, and fabrication-toolkit options.
- `hardware/datasheets/` — component datasheets.
- `hardware/jlcpcb/` — fab outputs committed with the board (drill maps and the fabrication-toolkit database). New files generated into that directory are left untracked.
- `software/` — PlatformIO firmware for the ATtiny1616. `src/` and `include/` are the firmware, `lib/` is for private libraries, and `algorithms/` holds the fractional-N notes.
- `scripts/` — build and flash launchers.

Open the board from `hardware/Synthesizer-VHF.kicad_pro`.

The symbol libraries resolve next to the project:

- `${KIPRJMOD}/Synthesizer.kicad_sym`
- `${KIPRJMOD}/adf4350.kicad_sym`

3D models use KiCad's official shapes through `${KICAD8_3DMODEL_DIR}`. Footprints are stored in the board file. The footprint library table still names two libraries that live on the original design machine and are not in this repository: `/home/jeff/kicad/Synthesizer.pretty` and `/home/jeff/Downloads/MAX2870`. The board opens from the embedded footprints. Updating a footprint from the library needs those directories present at those paths.

## Build the firmware

Install [PlatformIO](https://platformio.org/), then from the repository root:

```
scripts/build-firmware
```

That builds `software/` for an ATtiny1616 running at 20 MHz on the internal oscillator (megaTinyCore, via the PlatformIO `atmelmegaavr` platform).

## Flash the firmware

Install `pymcuprog` as well. The serial port and the UPDI upload command are already set in `software/platformio.ini` (the port there is `/dev/ttyUSB0`). Connect the programmer, then from the repository root:

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

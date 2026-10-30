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

The board and project files are KiCad 10 format, saved from KiCad 10.0.7. The symbol libraries are still the KiCad 8/9 library format, which KiCad 10 opens directly.

## Stackup to order

Order JLCPCB's default 4-layer 1.6 mm board (JLC04161H-7628). This is not the 3313 stackup and it is not an impedance-controlled order. Copper plus dielectric is 1.586 mm; KiCad's board-thickness field is 1.6472 mm because it also counts the solder mask on both sides. The fab still builds the nominal 1.6 mm board.

| Layer | Thickness | Material | Er |
| --- | --- | --- | --- |
| F.Cu | 0.035 mm | copper | |
| Prepreg | 0.2104 mm | 7628 | 4.4 |
| In1.Cu | 0.0152 mm | copper, GND | |
| Core | 1.065 mm | FR-4 | 4.6 |
| In2.Cu | 0.0152 mm | copper, VCC | |
| Prepreg | 0.2104 mm | 7628 | 4.4 |
| B.Cu | 0.035 mm | copper | |
| Solder mask | 0.0305 mm each side | JLC 1.2 mil mask | 3.8 |

## RF traces

The ADF4351 RF nets are net class `RF_50`: 0.22 mm traces with 0.20 mm clearance, drawn on F.Cu over the In1 ground plane as grounded coplanar waveguide. That is the 50 ohm geometry for this stackup with JLCPCB's solder mask.

Hammerstad–Jensen microstrip on the 0.2104 mm / Er 4.4 prepreg, with 0.035 mm copper, puts a bare 0.22 mm trace in the mid-60s of ohms. A bare 50 ohm microstrip on the same dielectric is about 0.36 mm wide. The coplanar ground is then applied as a ratio: grounded-CPW impedance at a 0.20 mm gap, divided by the same formula's wide-gap limit, so a large gap still matches the microstrip number. KiCad's GCPW calculator skips that correction and reads this geometry about 20% high; that figure was not used. Solder mask (0.0305 mm, Er 3.8) lowers the result by roughly 1.5 ohms. JLCPCB etch, about 0.018 mm narrower at the top of the trace, raises it by about an ohm. The two nearly cancel, which is why the drawn size is 0.22 mm rather than the slightly wider bare-copper answer. A 0.33 mm trace with the same 0.20 mm gap would sit in the low 40s. This default stackup is not an impedance-controlled order, so the fab spread is the ordinary process tolerance. The minimum trace and space used here (0.20 mm) is above JLCPCB's 0.127 mm limit.

The four southbound outputs leave the 0.5 mm LFCSP pitch on 0.22 mm traces. The copper gap between neighbors is 0.28 mm, so a ground web cannot fit between them and those few millimetres stay coupled. The pour returns to a 0.20 mm gap on the outer sides and once the runs separate. `REFIN` is on F.Cu except for a 4.2 mm hop on B.Cu under `/MUX`, `/LD`, `/RF_EN`, and the 5 V bus. That hop is short at the reference (250 MHz maximum). Ground vias fence the open runs at about 1 mm pitch, 0.65 mm off the trace centerline. The same fence cannot sit between the 0.5 mm pin escapes: a 0.3 mm drill needs more clearance than that web has. The SMA launches at J5, J7, and J9 taper 0.22 → 0.40 → 0.80 mm into the signal pad, with a copper keepout so the pour does not close up around the pad.

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

## License

- Software (`software/`, `scripts/`): GPL-3.0-or-later, see `LICENSE`.
- Hardware design files (`hardware/`): CERN-OHL-S-2.0, see `LICENSE-HARDWARE`.

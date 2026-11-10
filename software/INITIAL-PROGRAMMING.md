# Initial board programming

Use this note when the first hammond-1550Q boards are built. It records how Optiboot is installed and how an application is loaded afterward.

The firmware in this branch still builds for an ATtiny1616 at 20 MHz. The schematic value is ATtiny3216-M on the ATtiny1616 VQFN-20 footprint. Program the part that is soldered. The steps below match an ATtiny1616.

Two ports are involved. Keep the names separate.

- **UPDI** is PA0, on scope loop TP1. The ground for the probe is scope loop TP4.
- **Application UART** is the FT231X on USB-C J1. The MCU TX pin is PB2. The MCU RX pin is PB3. The FT231X is not wired to PA0.

`scripts/flash-firmware` runs `pymcuprog write --erase` on the FT231X port named in `software/platformio.ini`. That port is the application UART. The command speaks SerialUPDI. It does not install Optiboot, and it does not load an application through Optiboot.

The port string in `platformio.ini` is the FT231X with serial `DP04DFOI`. Each new board has its own FT231X serial number. Read the name under `/dev/serial/by-id/` after that board is plugged in.

## 1. Install Optiboot once, over UPDI

Use a 3.3 V USB-serial adapter as a SerialUPDI programmer. The MCU rail is 3.3 V. A 5 V adapter on TP1 can damage PA0.

Wire the adapter:

1. Tie the adapter TX and RX together.
2. Connect that node to TP1 through 470 Ω to 1 kΩ.
3. Connect the adapter ground to TP4.
4. Power the board from its own USB cable.

Do not use a high-voltage UPDI pulse. PA0 is already UPDI.

Confirm the adapter sees the chip. Replace the port with the adapter port.

```
pymcuprog ping -t uart -d attiny1616 -u /dev/ttyUSB0 -c 230400
```

If the ping fails, repeat it at 115200, then at 57600.

The bootloader image is the megaTinyCore file for a 14-pin to 24-pin tinyAVR 0/1-series part, default UART, 8 second wait:

`~/.platformio/packages/framework-arduino-megaavr-megatinycore/bootloaders/hex/optiboot_txyz_all8sec.hex`

That image uses TX on PB2 and RX on PB3. The PC baud is 115200. The bootloader runs for 8 seconds after power-up, then jumps to the application. Its activity pin is PA7. PA7 is open on this board, so there is no blink.

Write the fuses, then the image. A chip erase clears flash. It does not clear fuses. Fuse 5 value `0xC4` keeps PA0 as UPDI. Fuse 2 value `0x02` selects the 20 MHz internal oscillator. Fuse 8 value `0x02` reserves 512 bytes at the start of flash. Fuse 1 value `0x00` leaves the brown-out detector off.

| Fuse | Name | Value | Meaning |
| --- | --- | --- | --- |
| 0 | WDTCFG | `0x00` | Watchdog off |
| 1 | BODCFG | `0x00` | Brown-out detector off |
| 2 | OSCCFG | `0x02` | 20 MHz internal oscillator |
| 5 | SYSCFG0 | `0xC4` | PA0 stays UPDI |
| 6 | SYSCFG1 | `0x04` | 8 ms startup |
| 7 | APPEND | `0x00` | No application-code section |
| 8 | BOOTEND | `0x02` | 512 byte bootloader |

```
pymcuprog write -t uart -d attiny1616 -u /dev/ttyUSB0 -c 230400 -m fuses -o 0 -b 3 -l 0x00 0x00 0x02
pymcuprog write -t uart -d attiny1616 -u /dev/ttyUSB0 -c 230400 -m fuses -o 5 -b 4 -l 0xC4 0x04 0x00 0x02
pymcuprog write -t uart -d attiny1616 -u /dev/ttyUSB0 -c 230400 --erase --verify -f ~/.platformio/packages/framework-arduino-megaavr-megatinycore/bootloaders/hex/optiboot_txyz_all8sec.hex
```

Leave PA0 as UPDI. Fusing PA0 as reset requires a high-voltage programmer before UPDI works again.

## 2. Load an application through Optiboot

This step is not ready for the firmware in this branch.

The current build starts at address 0. Optiboot occupies the first 512 bytes. An application loaded through Optiboot must be linked at byte `0x200`. The largest application image is 15872 bytes. The link line is:

```
-Wl,--section-start=.text=0x200
```

`scripts/build-firmware` does not pass that line today. Do not send the current hex through Optiboot.

When a hex does start at `0x200`, use the board FT231X port and avrdude. Unplug USB, plug it back in, and start avrdude within 8 seconds. Replace the port with the FT231X that belongs to that board.

```
avrdude -C ~/.platformio/packages/framework-arduino-megaavr-megatinycore/avrdude.conf -p attiny1616 -c arduino -P /dev/serial/by-id/usb-FTDI_FT231X_USB_UART_DP04DFOI-if00-port0 -b 115200 -D -U flash:w:firmware.hex:i
```

The PC baud for the bootloader is 115200. The bootloader measures that baud from the reset clock, which is the 20 MHz oscillator divided by 6. The running application console is a different rate. The current firmware console is 57600.

After Optiboot is installed, do not point `pymcuprog write --erase` at this chip. That erase removes the bootloader.

## 3. What enters the bootloader

`optiboot_txyz_all8sec` runs after power-up. It also runs after a software reset. It does not run after a watchdog reset.

Plug-in works when USB is the only power source, because plug-in is a power-up. Open the serial port while the board is already powered, and the bootloader does not start.

The DTR capacitor does not enter this bootloader. FT231X DTR goes through C13, 100 nF, to PA4. R13, 10 kΩ, pulls PA4 up to VDD. Stock Optiboot treats only the real reset pin as an external reset. On this part that pin is PA0. PA4 is not that pin.

`Boot` in `software/src/UserInterface.cpp` writes the software-reset register. This image would accept that reset. The call is commented out in `software/src/ADF4351.cpp` (`UserConfig()` is not called from `loop()`). The live console is the single-key interface. It has no bootloader command.

## 4. Checks before the first chip

1. Ping over UPDI succeeds.
2. Fuse 2 reads back `0x02`.
3. Fuse 5 reads back `0xC4`.
4. Fuse 8 reads back `0x02`.
5. The bootloader hex verify passes.
6. The application hex, when it exists, starts at `0x200`.
7. Avrdude at 115200 connects within 8 seconds of plug-in.
8. The application console, after that, answers at 57600.

Read the fuses with:

```
pymcuprog read -t uart -d attiny1616 -u /dev/ttyUSB0 -c 230400 -m fuses
```

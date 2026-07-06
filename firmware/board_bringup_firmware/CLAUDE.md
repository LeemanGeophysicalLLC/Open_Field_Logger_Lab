# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project context

Board bring-up and self-test firmware for the Open Field Logger Lab — a low-cost educational data logger built around an ESP32-DevKitC (USB-C, 38-pin) with an ADS1115 16-bit ADC, MCP7940 RTC, microSD card, two status LEDs, and a log start/stop button. Serial output at 115200 baud drives the test procedure.

## Build system

PlatformIO with the Arduino framework. All commands run from this directory.

```bash
pio run                          # compile
pio run -t upload                # compile and flash
pio device monitor -b 115200     # open serial monitor
pio run -t upload && pio device monitor -b 115200   # flash then monitor
```

The PlatformIO environment is `featheresp32` (`platformio.ini`). The actual hardware is an ESP32-DevKitC USB-C module, not an Adafruit Feather — GPIO numbers are identical but the board target may need to change to `esp32dev` if peripheral conflicts arise (e.g. built-in LED on GPIO13).

Current `lib_deps` in `platformio.ini`:
```ini
lib_deps =
    adafruit/Adafruit ADS1X15@^2.5.0
    adafruit/Adafruit BusIO@^1.16.2
```
The `SD`, `SPI`, and `Wire` libraries come from the framework and need no explicit entry. The MCP7940 RTC is driven directly over I2C in `main.cpp` (no external library) to keep the register writes transparent during bring-up.

## Hardware pin assignments

Derived from schematic (`hardware/KiCAD_Project/Open_Field_Logger_Lab.kicad_sch`).

| Signal | GPIO | Notes |
|--------|------|-------|
| Serial TX | IO1 | UART0, USB-CDC via DevKit CH340/CP2102 |
| Serial RX | IO3 | UART0 |
| I2C SDA | IO21 | Shared: ADS1115 + MCP7940 |
| I2C SCL | IO22 | Shared: ADS1115 + MCP7940 |
| SPI SCK | IO18 | SD card SPI clock |
| SPI MOSI | IO23 | SD card SPI MOSI |
| SPI MISO | IO19 | SD card SPI MISO |
| SD_CS | IO5 | microSD chip select |
| SD_CD | IO32 | microSD card detect (card present = LOW) |
| ADS1115 ALRT | IO26 | Alert/ready interrupt from ADS1115 |
| RTC MFP | IO33 | MCP7940 multi-function pin / square-wave / alarm |
| Error LED | IO27 | Active HIGH |
| Log LED | IO14 | Active HIGH |
| Log Button | IO16 | Log start/stop switch |
| EXT_CS | IO25 | External SPI chip select (expansion connector) |

## I2C peripheral addresses

- **ADS1115**: 0x48 (ADDR pin tied to GND)
- **MCP7940 RTC**: 0x6F

## Source layout

All firmware lives in `src/main.cpp`. The `include/` directory is for project headers, `lib/` for any local libraries not in the PlatformIO registry.

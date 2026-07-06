#pragma once

// Open Field Logger Lab — hardware pin map.
//
// Copied from the schematic-derived table in
// firmware/board_bringup_firmware/CLAUDE.md so both firmwares agree on the
// same wiring. If the schematic ever changes, update both places.

#include <stdint.h>

// Status LEDs / button
static constexpr uint8_t PIN_ERROR_LED  = 27;  // active HIGH
static constexpr uint8_t PIN_LOG_LED    = 14;  // active HIGH
static constexpr uint8_t PIN_LOG_BUTTON = 16;  // active LOW, INPUT_PULLUP

// I2C bus (shared by the ADS1115 ADC and the MCP7940 RTC)
static constexpr uint8_t PIN_I2C_SDA = 21;
static constexpr uint8_t PIN_I2C_SCL = 22;

// SPI bus (SD card)
static constexpr uint8_t PIN_SPI_SCK  = 18;
static constexpr uint8_t PIN_SPI_MISO = 19;
static constexpr uint8_t PIN_SPI_MOSI = 23;
static constexpr uint8_t PIN_SD_CS    = 5;
static constexpr uint8_t PIN_SD_CD    = 32;  // LOW = card present

// I2C peripheral addresses
static constexpr uint8_t ADS1115_I2C_ADDR = 0x48;  // ADDR pin tied to GND
static constexpr uint8_t MCP7940_I2C_ADDR = 0x6F;

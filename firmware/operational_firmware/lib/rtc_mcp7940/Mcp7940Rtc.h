#pragma once

// Mcp7940Rtc — minimal driver for the MCP7940 real-time clock.
//
// This talks to the RTC with raw I2C register reads/writes, the same way
// firmware/board_bringup_firmware/src/main.cpp's testRTC() does. There's no
// RTClib dependency here on purpose: reading and writing seven BCD-encoded
// registers is simple enough that spelling it out directly is more useful
// for students than hiding it behind a library. If you want to see the
// original one-shot version this was ported from, read testRTC() and
// printRTCCurrentTime() in the bring-up firmware.
//
// MCP7940 register map used here (see datasheet section on RTCC registers):
//   0x00  seconds (bits 0-6, BCD) | ST bit (bit 7, oscillator start/enable)
//   0x01  minutes (bits 0-6, BCD)
//   0x02  hours   (bits 0-5, BCD, 24-hour mode assumed, bit 6 = 12/24 select)
//   0x03  weekday (bits 0-2, unused here) | VBATEN bit (bit 3, battery backup)
//   0x04  date    (bits 0-5, BCD)
//   0x05  month   (bits 0-4, BCD) | LPYR bit (bit 5, leap year, read-only)
//   0x06  year    (bits 0-7, BCD, two digits — 20xx is assumed)

#include <Wire.h>

#include "logger_types.h"

class Mcp7940Rtc {
 public:
  // Starts I2C communication and checks whether the RTC responds. Does NOT
  // set the time — call setDateTime() explicitly (or leave whatever time is
  // already held in the battery-backed registers).
  bool begin(TwoWire &wire = Wire, uint8_t i2c_addr = 0x6F);

  // True if the oscillator's ST (start) bit is set, i.e. the clock is
  // actually ticking rather than sitting stopped on a dead/fresh chip.
  bool isRunning();

  // Reads the current date/time. Returns false (and leaves *out* unchanged)
  // if the RTC couldn't be read over I2C.
  bool now(RtcDateTime &out);

  // Writes a new date/time and makes sure the oscillator is running
  // afterward (sets the ST bit) and that battery backup stays enabled
  // (preserves VBATEN), exactly like the bring-up firmware's testRTC().
  bool setDateTime(const RtcDateTime &dt);

  // Formats "YYYY-MM-DD HH:MM:SS" into out (out_len must be >= 20).
  static void formatTimestamp(const RtcDateTime &dt, char *out, size_t out_len);

 private:
  TwoWire *wire_ = &Wire;
  uint8_t addr_ = 0x6F;
  bool detected_ = false;
};

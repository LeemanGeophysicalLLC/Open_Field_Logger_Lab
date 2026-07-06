#pragma once

// SettingsStore — persists LoggerSettings to flash (NVS) so the ADC gain,
// logging rate, and averaging count survive a power cycle.
//
// Uses the ESP32 Arduino core's Preferences library, which is a small
// key-value store built on top of the chip's NVS (Non-Volatile Storage)
// flash partition — no SD card or external EEPROM involved.

#include "logger_types.h"

// Loads settings from flash into `out`. If this is the first boot (no
// values saved yet) or a value is missing/out of range, the compiled-in
// default from LoggerSettings's member initializers is used instead, so
// `out` is always left in a valid state.
void settingsLoad(LoggerSettings &out);

// Saves `in` to flash. Returns false only if the underlying NVS write
// failed.
bool settingsSave(const LoggerSettings &in);

#pragma once

// Open Field Logger Lab — shared data types.
//
// Every module (ADC, RTC, SD logger, sampling engine, web portal) and
// main.cpp all speak the same small set of plain structs/enums defined
// here. Keeping them in one place means the web page, the NVS-persisted
// settings, and the ADS1115 driver can never drift out of sync with each
// other.

#include <stdint.h>

// ---------------------------------------------------------------------------
// ADS1115 PGA gain / input voltage range.
//
// The ADS1115 has a single Programmable Gain Amplifier shared by all four
// single-ended inputs — you cannot pick a different range per channel, only
// one range for the whole chip. These six options match the six gain
// settings the ADS1115 actually supports.
//
// The chip's PGA full-scale is technically a +/- range, but every input on
// this board is single-ended and ground-referenced, so it can only ever read
// zero and above. fullScaleVolts() below is that +/- magnitude (used to size
// the top of the usable 0V-to-+FS range), and gainLabel() is worded that way
// for the same reason.
// ---------------------------------------------------------------------------
enum class AdcGain : uint8_t {
  TWOTHIRDS = 0,  // 0-6.144V
  ONE       = 1,  // 0-4.096V
  TWO       = 2,  // 0-2.048V
  FOUR      = 3,  // 0-1.024V
  EIGHT     = 4,  // 0-0.512V
  SIXTEEN   = 5,  // 0-0.256V
};

static constexpr uint8_t ADC_GAIN_COUNT = 6;

// Full-scale input voltage (the top of the usable 0V-to-+FS range) for each
// gain setting.
inline float fullScaleVolts(AdcGain gain) {
  switch (gain) {
    case AdcGain::TWOTHIRDS: return 6.144f;
    case AdcGain::ONE:       return 4.096f;
    case AdcGain::TWO:       return 2.048f;
    case AdcGain::FOUR:      return 1.024f;
    case AdcGain::EIGHT:     return 0.512f;
    case AdcGain::SIXTEEN:   return 0.256f;
  }
  return 4.096f;
}

// Human-readable label for the Configuration tab's dropdown.
inline const char *gainLabel(AdcGain gain) {
  switch (gain) {
    case AdcGain::TWOTHIRDS: return "0 - 6.144 V";
    case AdcGain::ONE:       return "0 - 4.096 V";
    case AdcGain::TWO:       return "0 - 2.048 V";
    case AdcGain::FOUR:      return "0 - 1.024 V";
    case AdcGain::EIGHT:     return "0 - 0.512 V";
    case AdcGain::SIXTEEN:   return "0 - 0.256 V";
  }
  return "0 - 4.096 V";
}

inline bool isValidAdcGain(uint8_t value) { return value < ADC_GAIN_COUNT; }

// ---------------------------------------------------------------------------
// RTC date/time, used both for reading the MCP7940 and for setting it from
// the web page (browser-sync button or the manual override fields).
// ---------------------------------------------------------------------------
struct RtcDateTime {
  uint16_t year   = 2026;  // full 4-digit year (MCP7940 only stores the low 2 digits)
  uint8_t  month  = 1;     // 1-12
  uint8_t  day    = 1;     // 1-31
  uint8_t  hour   = 0;     // 0-23
  uint8_t  minute = 0;     // 0-59
  uint8_t  second = 0;     // 0-59
};

// ---------------------------------------------------------------------------
// The entire student-configurable, NVS-persisted, live-applied settings
// surface. Everything here can be changed from the Configuration tab and
// takes effect immediately without a reboot.
// ---------------------------------------------------------------------------
struct LoggerSettings {
  AdcGain adc_gain           = AdcGain::ONE;  // 0-4.096V default
  uint8_t sample_rate_hz     = 5;             // target logging cadence, one of kSampleRateChoicesHz
  uint8_t averages_per_sample = 4;            // raw ADC reads averaged per logged point, one of kAverageChoices
};

// Allowed data rates (Hz). Kept to a short list rather than a free 1-10
// range so the Configuration tab's dropdown only ever offers rates that
// make sense to pick between.
static constexpr uint8_t kSampleRateChoicesHz[] = {1, 2, 5, 10};
static constexpr uint8_t kSampleRateChoiceCount = 4;

// Allowed averaging counts. Restricted to powers of two — it keeps the
// microcontroller-side math simple (dividing by a power of two is cheap)
// and gives students a clean set of choices instead of an arbitrary 1-16
// range.
static constexpr uint8_t kAverageChoices[] = {1, 2, 4, 8, 16};
static constexpr uint8_t kAverageChoiceCount = 5;

inline bool isValidSampleRateHz(uint8_t hz) {
  for (uint8_t i = 0; i < kSampleRateChoiceCount; i++) {
    if (kSampleRateChoicesHz[i] == hz) return true;
  }
  return false;
}

inline bool isValidAverages(uint8_t n) {
  for (uint8_t i = 0; i < kAverageChoiceCount; i++) {
    if (kAverageChoices[i] == n) return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// One instantaneous reading of all four single-ended channels, in volts.
// ---------------------------------------------------------------------------
struct ChannelSample {
  float volts[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

// ---------------------------------------------------------------------------
// One row as written to the SD card.
// ---------------------------------------------------------------------------
struct SampleRecord {
  char timestamp[20] = "";  // "YYYY-MM-DD HH:MM:SS" from the RTC
  uint32_t uptime_ms = 0;   // millis() at the time of the sample
  ChannelSample channels;
};

// ---------------------------------------------------------------------------
// Everything the web page's status strip / logging badge / health readouts
// need, refreshed by main.cpp each loop() and served as JSON from
// /api/status.
// ---------------------------------------------------------------------------
struct LiveStatus {
  bool logging_active   = false;
  char current_log_file[16] = "";  // e.g. "log007.txt", empty when idle

  bool sd_ok             = false;  // SD.begin() succeeded at boot
  bool sd_card_present   = false;  // card-detect pin reads "card present" right now
  bool rtc_ok            = false;  // MCP7940 oscillator is running

  bool error_active      = false;  // mirrors the physical Error LED
  char error_reason[48]  = "";     // short human-readable cause, for the web page

  char rtc_time_str[20]  = "--";   // "YYYY-MM-DD HH:MM:SS" or "--" if RTC unavailable
  uint32_t uptime_ms     = 0;
  uint32_t samples_logged = 0;     // rows written in the current logging session
  float achieved_sample_hz = 0.0f; // real measured cadence, not the requested one
};

// ---------------------------------------------------------------------------
// One entry in the SD card's root-directory listing, used by the File
// Explorer tab.
// ---------------------------------------------------------------------------
struct SdFileInfo {
  char name[32] = "";
  uint32_t size_bytes = 0;
};

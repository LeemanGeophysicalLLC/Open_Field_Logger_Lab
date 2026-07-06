#pragma once

// SamplingEngine — the non-blocking heart of the logger.
//
// This is the one piece of the firmware that answers "how do we keep
// sampling the ADC AND keep the web server responsive at the same time?"
// The answer is a small state machine: tick() is called once per loop()
// iteration, and every call does at most one cheap step (start a
// conversion, or check/collect one that's already running) before
// returning. Nothing in here ever blocks waiting on the ADS1115, so a
// slow/busy web request next door in loop() never gets stuck behind a
// sample cycle, and a sample cycle never gets stuck behind a web request.
//
// One full "cycle" = for each of the 4 channels: one throwaway conversion
// to let the ADS1115's input multiplexer settle after switching channels,
// then N kept conversions that get averaged together. The engine paces
// cycles to the requested rate on a best-effort basis: if a cycle finishes
// faster than the target period it waits, and if it's slower (e.g. a high
// rate + high averaging combination the ADS1115 can't quite keep up with)
// it just starts the next cycle immediately — no error, no blocking, the
// achieved rate simply comes out lower than requested and every logged row
// is timestamped with when it actually happened.
//
// The ADC free-runs continuously regardless of whether logging is active,
// so the Reading and Graph tabs are always useful (e.g. for checking sensor
// wiring before ever starting a log session). Only the SD-append step in
// finalizeCycle() is gated on isLoggingActive().

#include <stdint.h>

#include "AdcManager.h"
#include "Mcp7940Rtc.h"
#include "SdLogger.h"
#include "logger_types.h"

class SamplingEngine {
 public:
  void begin(AdcManager *adc, Mcp7940Rtc *rtc, SdLogger *sd_logger);

  // Can be called at any time, including mid-cycle, to apply new settings
  // live. A setting change mid-cycle may make the in-flight cycle's
  // averaging count briefly inconsistent; it self-corrects on the very next
  // cycle, which is an acceptable trade-off for how rarely settings change.
  void configure(uint8_t sample_rate_hz, uint8_t averages_per_sample);

  // Only main.cpp calls this, only in response to the physical button —
  // this class has no idea a button even exists.
  void setLoggingActive(bool active) { logging_active_ = active; }
  bool isLoggingActive() const { return logging_active_; }

  // Call once per loop() iteration. Performs at most one small I2C
  // transaction (a conversion-ready poll, or starting/collecting a
  // conversion) before returning.
  void tick(uint32_t now_ms);

  ChannelSample getLatestReading() const { return latest_; }
  float getAchievedSampleHz() const { return achieved_sample_hz_; }

 private:
  enum class Phase { WAITING, CONVERTING };

  AdcManager *adc_ = nullptr;
  Mcp7940Rtc *rtc_ = nullptr;
  SdLogger *sd_logger_ = nullptr;

  bool logging_active_ = false;

  uint32_t target_period_ms_ = 200;   // 1000 / sample_rate_hz
  uint8_t averages_per_sample_ = 4;

  Phase phase_ = Phase::WAITING;
  uint8_t channel_index_ = 0;
  bool is_settle_read_ = true;
  uint8_t reads_kept_ = 0;
  float accum_[4] = {0, 0, 0, 0};

  uint32_t cycle_start_ms_ = 0;      // when the in-progress cycle began
  uint32_t next_cycle_due_ms_ = 0;   // fixed schedule point for the next cycle

  ChannelSample latest_;
  float achieved_sample_hz_ = 0.0f;

  void startNewCycle(uint32_t now_ms);
  void beginChannelSettle();
  void beginKeptConversion();
  void finalizeCycle(uint32_t now_ms);
};

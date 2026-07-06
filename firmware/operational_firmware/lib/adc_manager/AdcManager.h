#pragma once

// AdcManager — thin, honest wrapper around the Adafruit_ADS1115 driver.
//
// "Thin and honest" means: this class only knows how to talk to the chip
// (set the gain, start a conversion on one channel, ask if it's done, read
// the result). It deliberately does NOT know about averaging or cycling
// through all four channels — that sequencing lives in SamplingEngine, kept
// separate so each class has exactly one job.
//
// The one thing this class is careful about is never blocking. The
// Adafruit library's convenience function readADC_SingleEnded() spins in a
// `while (!conversionComplete()) {}` loop internally, which would freeze
// the whole firmware (including the web server) for a millisecond or more
// on every single reading. Instead we call the library's lower-level
// primitives directly: startADCReading() to kick off one conversion,
// conversionComplete()/readLastConversion() to poll and collect it. Nothing
// here ever waits.

#include <Adafruit_ADS1X15.h>

#include "logger_types.h"

class AdcManager {
 public:
  // Starts I2C communication with the ADS1115 and fixes its internal
  // conversion speed at the fastest available setting (860 samples/sec).
  // Note this is NOT the "data rate" the student picks on the web page —
  // that's the logging cadence, handled by SamplingEngine. Running the chip
  // itself as fast as possible just gives the averaging/mux-settling logic
  // the most headroom.
  bool begin(uint8_t i2c_addr = 0x48, TwoWire &wire = Wire);

  bool isDetected() const { return detected_; }

  // The PGA gain is shared by all four single-ended inputs (one register on
  // the chip) — there is no way to give each channel a different range.
  void setGain(AdcGain gain);

  // Starts a single-ended conversion on one of the four inputs (0-3).
  void beginChannelConversion(uint8_t channel);

  // Non-blocking poll — call this from loop() as often as you like.
  bool conversionReady();

  // Only valid to call after conversionReady() has returned true.
  int16_t readLastConversion();

  float countsToVolts(int16_t counts);

 private:
  Adafruit_ADS1115 ads_;
  bool detected_ = false;
};

#include "AdcManager.h"

namespace {

// Maps our own AdcGain enum (declared in logger_types.h, which intentionally
// doesn't depend on the Adafruit library) onto the vendor library's
// adsGain_t. Kept local to this file since nothing outside AdcManager needs
// to know the vendor's gain constants.
adsGain_t toAdafruitGain(AdcGain gain) {
  switch (gain) {
    case AdcGain::TWOTHIRDS: return GAIN_TWOTHIRDS;
    case AdcGain::ONE:       return GAIN_ONE;
    case AdcGain::TWO:       return GAIN_TWO;
    case AdcGain::FOUR:      return GAIN_FOUR;
    case AdcGain::EIGHT:     return GAIN_EIGHT;
    case AdcGain::SIXTEEN:   return GAIN_SIXTEEN;
  }
  return GAIN_ONE;
}

}  // namespace

bool AdcManager::begin(uint8_t i2c_addr, TwoWire &wire) {
  detected_ = ads_.begin(i2c_addr, &wire);
  if (detected_) {
    ads_.setDataRate(RATE_ADS1115_860SPS);
  }
  return detected_;
}

void AdcManager::setGain(AdcGain gain) { ads_.setGain(toAdafruitGain(gain)); }

void AdcManager::beginChannelConversion(uint8_t channel) {
  ads_.startADCReading(MUX_BY_CHANNEL[channel], /*continuous=*/false);
}

bool AdcManager::conversionReady() { return ads_.conversionComplete(); }

int16_t AdcManager::readLastConversion() { return ads_.getLastConversionResults(); }

float AdcManager::countsToVolts(int16_t counts) { return ads_.computeVolts(counts); }

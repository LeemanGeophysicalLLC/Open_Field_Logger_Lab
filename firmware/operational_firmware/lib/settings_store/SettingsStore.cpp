#include "SettingsStore.h"

#include <Preferences.h>

namespace {
constexpr char kNamespace[] = "ofl_cfg";
}  // namespace

void settingsLoad(LoggerSettings &out) {
  const LoggerSettings defaults;  // compiled-in defaults from logger_types.h

  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/true);

  const uint8_t gain_raw =
      prefs.getUChar("gain", static_cast<uint8_t>(defaults.adc_gain));
  const uint8_t rate_hz =
      prefs.getUChar("rate_hz", defaults.sample_rate_hz);
  const uint8_t averages =
      prefs.getUChar("avg", defaults.averages_per_sample);

  prefs.end();

  out.adc_gain = isValidAdcGain(gain_raw) ? static_cast<AdcGain>(gain_raw) : defaults.adc_gain;
  out.sample_rate_hz = isValidSampleRateHz(rate_hz) ? rate_hz : defaults.sample_rate_hz;
  out.averages_per_sample = isValidAverages(averages) ? averages : defaults.averages_per_sample;
}

bool settingsSave(const LoggerSettings &in) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return false;
  }

  bool ok = true;
  ok &= (prefs.putUChar("gain", static_cast<uint8_t>(in.adc_gain)) > 0);
  ok &= (prefs.putUChar("rate_hz", in.sample_rate_hz) > 0);
  ok &= (prefs.putUChar("avg", in.averages_per_sample) > 0);

  prefs.end();
  return ok;
}

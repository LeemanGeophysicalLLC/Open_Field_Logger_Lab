/*
 * Open Field Logger Lab — Operational Firmware
 *
 * Continuously samples the four ADS1115 single-ended inputs, logs them to
 * sequentially-numbered SD card files (log001.txt, log002.txt, ...) with an
 * MCP7940 RTC timestamp, and hosts a small local web page over its own WiFi
 * access point (Configuration / Reading / File Explorer / Graph / Help
 * tabs) so students can watch and configure the logger from a laptop or
 * phone.
 *
 * This file is intentionally thin: it just wires the modules in lib/
 * together and runs the main loop. Read the header comment at the top of
 * each module (UiIo, Mcp7940Rtc, AdcManager, SamplingEngine, SdLogger,
 * SdFiles, SettingsStore, WebPortal) to see how each peripheral is used —
 * that's where the actual explanation of *how* lives.
 *
 * Hardware control summary:
 *   - The physical Log Button carries two gestures. A single click starts
 *     or stops logging — the web page mirrors that state but can never
 *     change it. A double click toggles the WiFi access point on/off.
 *   - WiFi starts OFF at boot to save power in the field, with no
 *     auto-timeout once turned on — a student double-clicks to bring it up,
 *     and turns it back off with a second double-click or the page's own
 *     "Turn Off WiFi" button. (Since WiFi is off by default, a printed
 *     label on the enclosure describing the double-click gesture is worth
 *     having — a brand new board can't show its own instructions until
 *     WiFi is already on.)
 *   - Both LEDs blink a few times at boot as a quick self-test. After that
 *     the Log LED never blinks while idle (regardless of WiFi state) — it
 *     only blinks while logging: one quick pulse every 1.5s with WiFi off,
 *     or three quick pulses every 1.5s with WiFi on. Separately, turning
 *     WiFi on/off gets its own one-time acknowledgment blink (2 blinks for
 *     on, 5 for off) regardless of whether logging is active, so a student
 *     gets instant feedback that their double-click registered.
 *   - The Error LED lights for: RTC not running, ADC not detected, the SD
 *     card missing when a log-start was attempted, an SD write failure, or
 *     all 999 sequential log numbers already being used.
 *   - The ADC free-runs continuously (Reading/Graph tabs are always live)
 *     regardless of logging state; only the SD-append step is gated on it.
 *   - Bluetooth is released and never used, and the CPU runs at a reduced
 *     160 MHz — both purely to cut power draw for battery-powered field use.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_bt.h>
#include <string.h>

#include "AdcManager.h"
#include "Mcp7940Rtc.h"
#include "SamplingEngine.h"
#include "SdFiles.h"
#include "SdLogger.h"
#include "SettingsStore.h"
#include "UiIo.h"
#include "WebPortal.h"
#include "logger_types.h"
#include "pins.h"

namespace {

UiIo g_ui;
Mcp7940Rtc g_rtc;
AdcManager g_adc;
SdLogger g_sdlog;
SamplingEngine g_engine;

LoggerSettings g_settings;
bool g_adc_detected = false;

// ---------------------------------------------------------------------------
// WebPortalHooks implementations — the only place the web layer's function
// pointers touch the actual application state.
// ---------------------------------------------------------------------------

void hookGetSettings(LoggerSettings &out) { out = g_settings; }

bool hookApplySettings(const LoggerSettings &in, char *err, size_t err_len) {
  (void)err;
  (void)err_len;
  g_settings = in;
  if (g_adc_detected) {
    g_adc.setGain(g_settings.adc_gain);
  }
  g_engine.configure(g_settings.sample_rate_hz, g_settings.averages_per_sample);
  settingsSave(g_settings);  // best-effort; a flash write failure doesn't undo the live change
  return true;
}

void hookGetStatus(LiveStatus &out) {
  out.logging_active = g_engine.isLoggingActive();
  strlcpy(out.current_log_file, g_sdlog.currentLogFileName(), sizeof(out.current_log_file));
  out.sd_ok = g_sdlog.isReady();
  out.sd_card_present = g_sdlog.isCardPresent();
  out.rtc_ok = g_rtc.isRunning();

  const bool board_fault = !out.rtc_ok || !g_adc_detected;
  out.error_active = board_fault || g_sdlog.hasError();
  if (g_sdlog.hasError()) {
    strlcpy(out.error_reason, g_sdlog.errorReason(), sizeof(out.error_reason));
  } else if (!g_adc_detected) {
    strlcpy(out.error_reason, "ADS1115 not detected", sizeof(out.error_reason));
  } else if (!out.rtc_ok) {
    strlcpy(out.error_reason, "RTC not running", sizeof(out.error_reason));
  } else {
    out.error_reason[0] = '\0';
  }

  RtcDateTime dt;
  if (g_rtc.now(dt)) {
    Mcp7940Rtc::formatTimestamp(dt, out.rtc_time_str, sizeof(out.rtc_time_str));
  } else {
    strlcpy(out.rtc_time_str, "--", sizeof(out.rtc_time_str));
  }

  out.uptime_ms = millis();
  out.samples_logged = g_sdlog.samplesLoggedThisSession();
  out.achieved_sample_hz = g_engine.getAchievedSampleHz();
}

void hookGetReading(ChannelSample &out, uint32_t &uptime_ms, bool &logging_active) {
  out = g_engine.getLatestReading();
  uptime_ms = millis();
  logging_active = g_engine.isLoggingActive();
}

bool hookSyncRtc(const RtcDateTime &dt, char *err, size_t err_len) {
  if (!g_rtc.setDateTime(dt)) {
    snprintf(err, err_len, "RTC write failed");
    return false;
  }
  return true;
}

bool hookDeleteFile(const char *name, char *err, size_t err_len) {
  const bool is_active =
      g_engine.isLoggingActive() && strcmp(name, g_sdlog.currentLogFileName()) == 0;
  if (is_active) {
    snprintf(err, err_len, "cannot delete the active log file while logging");
    return false;
  }
  if (!deleteFile(name, false)) {
    snprintf(err, err_len, "file not found or could not be deleted");
    return false;
  }
  return true;
}

// Fires once from webPortalSetWifiOn() whenever WiFi actually changes
// state, whichever gesture triggered it (button double-click or the page's
// "Turn Off WiFi" button) — 2 quick blinks for on, 5 for off.
void hookOnWifiChanged(bool now_on) { g_ui.wifiChangeBlink(now_on ? 2 : 5); }

// Builds a unique SSID from the ESP32's own MAC address, e.g.
// "OpenFieldLogger-A1B2C3" — guarantees every one of the ~20 units in a
// classroom gets a distinct network with zero manual setup.
void buildSsid(char *out, size_t out_len) {
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  snprintf(out, out_len, "OpenFieldLogger-%02X%02X%02X", mac[3], mac[4], mac[5]);
}

// The Log LED never blinks while idle, regardless of WiFi state — it only
// has something to say while logging is active:
//   logging, WiFi off -> one quick pulse every 1.5s
//   logging, WiFi on  -> three quick pulses every 1.5s
// (WiFi on/off gets its own one-time acknowledgment blink instead — see
// hookOnWifiChanged() — rather than an ongoing idle pattern here.)
bool computeLogLedOn(uint32_t now_ms, bool logging, bool wifi_on) {
  static constexpr uint32_t kPulsePeriodMs = 1500;
  static constexpr uint32_t kPulseOnMs = 80;
  static constexpr uint32_t kPulseGapMs = 120;

  if (!logging) {
    return false;
  }

  const uint32_t t = now_ms % kPulsePeriodMs;
  if (!wifi_on) {
    return t < kPulseOnMs;
  }
  for (uint8_t i = 0; i < 3; i++) {
    const uint32_t slot_start = i * (kPulseOnMs + kPulseGapMs);
    if (t >= slot_start && t < slot_start + kPulseOnMs) {
      return true;
    }
  }
  return false;
}

}  // namespace

void setup() {
  // Power saving, done before anything else spins up: this firmware never
  // uses Bluetooth, so releasing its controller memory up front frees RAM
  // and keeps that radio fully powered down for the rest of the boot. The
  // CPU only needs to be fast enough to keep the web server and I2C/SPI
  // peripherals responsive, not run at full speed — 160 MHz (down from the
  // 240 MHz default) cuts a meaningful amount of power while staying well
  // above the ~80 MHz floor WiFi needs to stay reliable.
  esp_bt_mem_release(ESP_BT_MODE_BTDM);
  setCpuFrequencyMhz(160);

  Serial.begin(115200);
  delay(200);  // let USB-serial enumerate, matches board_bringup_firmware

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);  // I2C fast mode — buys back margin for the ADC's mux-settling reads

  g_ui.begin();
  g_ui.bootBlinkTest();  // a few blinks so a student watching the board sees both LEDs work

  settingsLoad(g_settings);

  g_rtc.begin(Wire, MCP7940_I2C_ADDR);
  g_adc_detected = g_adc.begin(ADS1115_I2C_ADDR, Wire);
  if (g_adc_detected) {
    g_adc.setGain(g_settings.adc_gain);
  }

  g_sdlog.begin(PIN_SD_CS, PIN_SD_CD);

  g_engine.begin(&g_adc, &g_rtc, &g_sdlog);
  g_engine.configure(g_settings.sample_rate_hz, g_settings.averages_per_sample);

  char ssid[33];
  buildSsid(ssid, sizeof(ssid));
  Serial.print(F("Open Field Logger — WiFi SSID (starts OFF, double-click Log button to enable): "));
  Serial.println(ssid);

  WebPortalHooks hooks;
  hooks.get_settings = hookGetSettings;
  hooks.apply_settings = hookApplySettings;
  hooks.get_status = hookGetStatus;
  hooks.get_reading = hookGetReading;
  hooks.sync_rtc = hookSyncRtc;
  hooks.list_files = listRootFiles;
  hooks.open_file_for_download = openForDownload;
  hooks.delete_file = hookDeleteFile;
  hooks.on_wifi_changed = hookOnWifiChanged;

  webPortalInit(hooks, ssid);
  webPortalBegin();
}

void loop() {
  const uint32_t now = millis();

  const UiIo::ButtonEvent button_event = g_ui.pollButtonEvent(now);
  if (button_event == UiIo::ButtonEvent::kSingleClick) {
    if (g_engine.isLoggingActive()) {
      g_engine.setLoggingActive(false);
      g_sdlog.endLogSession();
    } else {
      char err[48] = "";
      if (g_sdlog.startNewLogSession(err, sizeof(err))) {
        g_engine.setLoggingActive(true);
      }
      // else: session couldn't start (no card / 999 exhausted) — stay idle;
      // g_sdlog.hasError() is now latched and lights the Error LED below.
    }
  } else if (button_event == UiIo::ButtonEvent::kDoubleClick) {
    webPortalSetWifiOn(!webPortalIsWifiOn());
  }

  g_engine.tick(now);

  g_ui.setLogLed(computeLogLedOn(now, g_engine.isLoggingActive(), webPortalIsWifiOn()));

  const bool board_fault = !g_rtc.isRunning() || !g_adc_detected;
  g_ui.setErrorLed(board_fault || g_sdlog.hasError());

  webPortalHandleClient();
}

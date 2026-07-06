#pragma once

// WebPortal — the SoftAP + WebServer + single 4-tab web page.
//
// This module never touches main.cpp's globals directly. Instead
// main.cpp fills in a WebPortalHooks struct of plain function pointers,
// the same "hooks" pattern used by the Leeman Geophysical Ploughmeter
// logger's webconfig module. That keeps this file focused purely on HTTP
// plumbing and page rendering, with zero knowledge of AdcManager,
// SdLogger, etc.
//
// The web page itself is one static page (Configuration / Reading / File
// Explorer / Graph tabs, switched client-side with JavaScript) rather than
// four separate server routes, because every value on the page is dynamic
// and gets filled in after load via fetch() — there's nothing for the
// server to template per request, so the whole page can be a single
// constant string sent straight from flash (PROGMEM) instead of being
// rebuilt on every request like a more typical templated page would be.

#include <SD.h>
#include <stdint.h>

#include "logger_types.h"

struct WebPortalHooks {
  void (*get_settings)(LoggerSettings &out) = nullptr;
  // Validates ranges, persists to flash, and applies live. On failure fills
  // err (err_len bytes) with a human-readable reason and returns false.
  bool (*apply_settings)(const LoggerSettings &in, char *err, size_t err_len) = nullptr;

  void (*get_status)(LiveStatus &out) = nullptr;

  // Cheap, frequently-polled reading feed for the Reading + Graph tabs.
  void (*get_reading)(ChannelSample &out, uint32_t &uptime_ms, bool &logging_active) = nullptr;

  // Handles both the "Sync From Browser Clock" button and the manual
  // override fields — the browser-sync button just fills the same
  // year/month/day/hour/minute/second fields from JS `Date` local getters
  // before submitting, so both paths land on one hook. (Sending a raw UTC
  // epoch timestamp instead would have made the RTC show UTC rather than
  // the student's local wall-clock time, which is why this takes calendar
  // fields rather than a timestamp.)
  bool (*sync_rtc)(const RtcDateTime &dt, char *err, size_t err_len) = nullptr;

  int (*list_files)(SdFileInfo *out, int max_count) = nullptr;
  File (*open_file_for_download)(const char *name) = nullptr;
  bool (*delete_file)(const char *name, char *err, size_t err_len) = nullptr;

  // Fired once from inside webPortalSetWifiOn() whenever it actually
  // changes the radio state — regardless of whether that was triggered by
  // the physical button's double-click or the page's "Turn Off WiFi"
  // button — so main.cpp has one place to hang a physical LED
  // acknowledgment off of instead of duplicating it at both call sites.
  void (*on_wifi_changed)(bool now_on) = nullptr;
};

// Stores the hooks and the AP SSID. Call once from setup(), before
// webPortalBegin().
void webPortalInit(const WebPortalHooks &hooks, const char *ssid);

// Registers all routes and starts the HTTP server. Call once from setup(),
// regardless of whether the WiFi radio is on yet — this only sets up the
// server object itself, it doesn't touch WiFi state (see
// webPortalSetWifiOn() for that). Routes stay registered and reachable
// whenever the radio is later turned on.
void webPortalBegin();

// Turns the SoftAP (open, no password) on or off. WiFi starts OFF by
// default at boot to save power in the field; main.cpp brings it up in
// response to a double-click of the physical Log button, and takes it back
// down on a second double-click or via the page's "Turn Off WiFi" button.
// There is deliberately no auto-timeout — once on, it stays on until
// explicitly turned off.
void webPortalSetWifiOn(bool on);
bool webPortalIsWifiOn();

// Pumps the web server. Call once per loop() iteration; near-zero cost
// when no client is connected or mid-request (and effectively free with
// the radio off, since there's no interface for a client to reach it on).
void webPortalHandleClient();

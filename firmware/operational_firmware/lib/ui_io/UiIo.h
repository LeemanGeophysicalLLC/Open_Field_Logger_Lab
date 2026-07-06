#pragma once

// UiIo — the two status LEDs and the log button.
//
// This is the persistent-application version of what board_bringup_firmware
// tests one time each: driving the LEDs and reading the button. The one
// difference that matters here is the button read. The bring-up test can
// afford to block with `while (digitalRead(...) == LOW) delay(10);` because
// it only ever runs once. A logger that also has to keep a web server
// responsive can't block like that, so button reads here are non-blocking
// debounces that you call once per loop().
//
// The single Log button carries two gestures: a single click toggles
// logging, and a double click toggles the WiFi access point on/off (off by
// default at boot, to save power — see main.cpp for what each gesture
// does). Telling those apart means a single click can't be reported the
// instant the button is released — pollButtonEvent() has to wait out the
// double-click window first to make sure a second click isn't coming, so a
// reported kSingleClick lags the actual button release by a little under
// kDoubleClickWindowMs.

#include <stdint.h>

class UiIo {
 public:
  enum class ButtonEvent : uint8_t { kNone, kSingleClick, kDoubleClick };

  // Configures the LED and button pins. Call once from setup().
  void begin();

  // Blinks both LEDs together a few times so a student watching the board
  // at power-up gets visual confirmation the LEDs (and the wiring to them)
  // actually work, before settling into whatever the real logging/error
  // state is. This runs once at boot before the main loop starts, so it's
  // fine for it to block briefly with delay() the same way
  // board_bringup_firmware's LED tests do.
  void bootBlinkTest();

  // A brief one-time acknowledgment on the Log LED — `count` quick
  // blinks — right when WiFi is turned on or off, so a student gets instant
  // feedback that their double-click (or the page's "Turn Off WiFi"
  // button) actually registered. Blocks for well under a second, which is
  // fine since it only ever runs in response to a deliberate, rare user
  // action, not on every loop() iteration.
  void wifiChangeBlink(uint8_t count);

  void setErrorLed(bool on);
  void setLogLed(bool on);

  // Call this once every loop() iteration. Returns kSingleClick or
  // kDoubleClick once the corresponding gesture has completed, kNone
  // otherwise.
  ButtonEvent pollButtonEvent(uint32_t now_ms);

 private:
  static constexpr uint32_t kDebounceMs = 30;
  static constexpr uint32_t kDoubleClickWindowMs = 400;

  // Non-blocking debounce, returns true exactly once per physical press (on
  // the press, not the release). Private: main.cpp only ever wants the
  // higher-level single/double click gesture from pollButtonEvent().
  bool buttonPressedEdge(uint32_t now_ms);

  bool stable_pressed_ = false;   // debounced button state
  bool raw_pressed_last_ = false;
  uint32_t last_change_ms_ = 0;

  bool click_pending_ = false;    // a first click is waiting to see if a second one follows
  uint32_t first_click_ms_ = 0;
};

#include "UiIo.h"

#include <Arduino.h>

#include "pins.h"

void UiIo::begin() {
  pinMode(PIN_ERROR_LED, OUTPUT);
  pinMode(PIN_LOG_LED, OUTPUT);
  pinMode(PIN_LOG_BUTTON, INPUT_PULLUP);

  digitalWrite(PIN_ERROR_LED, LOW);
  digitalWrite(PIN_LOG_LED, LOW);
}

void UiIo::bootBlinkTest() {
  static constexpr uint8_t kBlinkCount = 3;
  static constexpr uint32_t kBlinkOnMs = 150;
  static constexpr uint32_t kBlinkOffMs = 150;

  for (uint8_t i = 0; i < kBlinkCount; i++) {
    setErrorLed(true);
    setLogLed(true);
    delay(kBlinkOnMs);
    setErrorLed(false);
    setLogLed(false);
    delay(kBlinkOffMs);
  }
}

void UiIo::wifiChangeBlink(uint8_t count) {
  static constexpr uint32_t kBlinkOnMs = 100;
  static constexpr uint32_t kBlinkOffMs = 100;

  for (uint8_t i = 0; i < count; i++) {
    setLogLed(true);
    delay(kBlinkOnMs);
    setLogLed(false);
    delay(kBlinkOffMs);
  }
}

void UiIo::setErrorLed(bool on) { digitalWrite(PIN_ERROR_LED, on ? HIGH : LOW); }

void UiIo::setLogLed(bool on) { digitalWrite(PIN_LOG_LED, on ? HIGH : LOW); }

UiIo::ButtonEvent UiIo::pollButtonEvent(uint32_t now_ms) {
  const bool pressed_edge = buttonPressedEdge(now_ms);

  if (pressed_edge) {
    if (click_pending_ && (now_ms - first_click_ms_) <= kDoubleClickWindowMs) {
      // Second press arrived in time — this is the double click.
      click_pending_ = false;
      return ButtonEvent::kDoubleClick;
    }
    // Either the first press we've seen, or a new one after a previous
    // single click already resolved — start (or restart) the wait.
    click_pending_ = true;
    first_click_ms_ = now_ms;
    return ButtonEvent::kNone;
  }

  if (click_pending_ && (now_ms - first_click_ms_) > kDoubleClickWindowMs) {
    // No second press showed up in time — resolve as a single click.
    click_pending_ = false;
    return ButtonEvent::kSingleClick;
  }

  return ButtonEvent::kNone;
}

bool UiIo::buttonPressedEdge(uint32_t now_ms) {
  // The button is active LOW (INPUT_PULLUP): pressed == LOW.
  const bool raw_pressed = (digitalRead(PIN_LOG_BUTTON) == LOW);

  // Restart the debounce timer whenever the raw reading changes.
  if (raw_pressed != raw_pressed_last_) {
    last_change_ms_ = now_ms;
    raw_pressed_last_ = raw_pressed;
  }

  // Once the raw reading has been stable for kDebounceMs, accept it as the
  // real button state. Report an edge only on the LOW-going transition.
  if ((now_ms - last_change_ms_) >= kDebounceMs && raw_pressed != stable_pressed_) {
    stable_pressed_ = raw_pressed;
    return stable_pressed_;  // true only when the new stable state is "pressed"
  }

  return false;
}

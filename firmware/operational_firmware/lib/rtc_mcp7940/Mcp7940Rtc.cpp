#include "Mcp7940Rtc.h"

#include <Arduino.h>
#include <stdio.h>

namespace {

uint8_t toBCD(uint8_t v) { return static_cast<uint8_t>(((v / 10) << 4) | (v % 10)); }
uint8_t fromBCD(uint8_t b) { return static_cast<uint8_t>(((b >> 4) & 0x0F) * 10 + (b & 0x0F)); }

}  // namespace

bool Mcp7940Rtc::begin(TwoWire &wire, uint8_t i2c_addr) {
  wire_ = &wire;
  addr_ = i2c_addr;

  // A cheap "is anything there" probe: try to read register 0x00.
  wire_->beginTransmission(addr_);
  wire_->write(uint8_t(0x00));
  const uint8_t tx_err = wire_->endTransmission(false);
  if (tx_err != 0 || wire_->requestFrom(addr_, uint8_t(1)) < 1) {
    detected_ = false;
    return false;
  }
  wire_->read();  // discard, we only wanted to confirm the chip answers
  detected_ = true;
  return true;
}

bool Mcp7940Rtc::isRunning() {
  if (!detected_) {
    return false;
  }
  wire_->beginTransmission(addr_);
  wire_->write(uint8_t(0x00));
  if (wire_->endTransmission(false) != 0 || wire_->requestFrom(addr_, uint8_t(1)) < 1) {
    return false;
  }
  const uint8_t sec_raw = wire_->read();
  return (sec_raw & 0x80) != 0;  // ST bit
}

bool Mcp7940Rtc::now(RtcDateTime &out) {
  if (!detected_) {
    return false;
  }

  wire_->beginTransmission(addr_);
  wire_->write(uint8_t(0x00));
  if (wire_->endTransmission(false) != 0 || wire_->requestFrom(addr_, uint8_t(7)) < 7) {
    return false;
  }

  const uint8_t sec_raw = wire_->read();
  const uint8_t min_raw = wire_->read();
  const uint8_t hour_raw = wire_->read();
  wire_->read();  // weekday register, unused
  const uint8_t day_raw = wire_->read();
  const uint8_t month_raw = wire_->read();
  const uint8_t year_raw = wire_->read();

  out.second = fromBCD(sec_raw & 0x7F);   // mask ST bit
  out.minute = fromBCD(min_raw);
  out.hour   = fromBCD(hour_raw & 0x3F);  // mask 12/24 select bit
  out.day    = fromBCD(day_raw);
  out.month  = fromBCD(month_raw & 0x1F);  // mask LPYR bit
  out.year   = 2000 + fromBCD(year_raw);
  return true;
}

bool Mcp7940Rtc::setDateTime(const RtcDateTime &dt) {
  if (!detected_) {
    return false;
  }

  const uint8_t year_2digit = static_cast<uint8_t>(dt.year % 100);

  wire_->beginTransmission(addr_);
  wire_->write(uint8_t(0x00));
  wire_->write(uint8_t(toBCD(dt.second) | 0x80));  // seconds | ST=1 (start oscillator)
  wire_->write(toBCD(dt.minute));                  // minutes
  wire_->write(toBCD(dt.hour));                    // hours (24h mode, bit6=0)
  wire_->write(uint8_t(0x08));                     // weekday unused, VBATEN=1 (bit3)
  wire_->write(toBCD(dt.day));                     // date
  wire_->write(toBCD(dt.month));                   // month
  wire_->write(toBCD(year_2digit));                // year
  return wire_->endTransmission() == 0;
}

void Mcp7940Rtc::formatTimestamp(const RtcDateTime &dt, char *out, size_t out_len) {
  snprintf(out, out_len, "%04u-%02u-%02u %02u:%02u:%02u", dt.year, dt.month, dt.day, dt.hour,
           dt.minute, dt.second);
}

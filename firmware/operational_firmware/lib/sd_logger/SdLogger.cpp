#include "SdLogger.h"

#include <Arduino.h>
#include <SPI.h>
#include <ctype.h>
#include <string.h>

#include "pins.h"

namespace {

// Matches exactly "logNNN.txt" where NNN is three digits, e.g. "log007.txt".
// Returns true and fills *out_num on a match.
bool parseLogNumber(const char *name, int *out_num) {
  if (strlen(name) != 10) return false;
  if (strncmp(name, "log", 3) != 0) return false;
  if (strncmp(name + 6, ".txt", 4) != 0) return false;
  for (int i = 3; i < 6; i++) {
    if (!isdigit(static_cast<unsigned char>(name[i]))) return false;
  }
  *out_num = (name[3] - '0') * 100 + (name[4] - '0') * 10 + (name[5] - '0');
  return true;
}

}  // namespace

bool SdLogger::begin(uint8_t cs_pin, uint8_t cd_pin) {
  cs_pin_ = cs_pin;
  cd_pin_ = cd_pin;
  pinMode(cd_pin_, INPUT_PULLUP);

  if (!isCardPresent()) {
    sd_ready_ = false;
    return false;
  }

  // Explicit SPI pin assignment, same as board_bringup_firmware's testSD().
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, cs_pin_);
  sd_ready_ = SD.begin(cs_pin_, SPI);
  return sd_ready_;
}

bool SdLogger::isCardPresent() const { return digitalRead(cd_pin_) == LOW; }

int SdLogger::findHighestLogNumber() {
  File root = SD.open("/");
  if (!root) {
    return -1;
  }

  int highest = -1;
  for (File entry = root.openNextFile(); entry; entry = root.openNextFile()) {
    if (!entry.isDirectory()) {
      // Some ESP32 core versions return the name with a leading '/'.
      const char *raw_name = entry.name();
      const char *name = (raw_name[0] == '/') ? raw_name + 1 : raw_name;
      int num = -1;
      if (parseLogNumber(name, &num) && num > highest) {
        highest = num;
      }
    }
    entry.close();
  }
  root.close();
  return highest;
}

bool SdLogger::startNewLogSession(const RtcDateTime &dt, char *err, size_t err_len) {
  if (!sd_ready_ || !isCardPresent()) {
    setError("SD card not present");
    if (err != nullptr) snprintf(err, err_len, "%s", error_reason_);
    return false;
  }

  const int highest = findHighestLogNumber();
  const int next_num = highest + 1;
  if (next_num > 999) {
    setError("999 log files already used");
    if (err != nullptr) snprintf(err, err_len, "%s", error_reason_);
    return false;
  }

  char name[16];
  snprintf(name, sizeof(name), "log%03d.txt", next_num);
  char path[20];
  snprintf(path, sizeof(path), "/%s", name);

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    setError("Could not create log file");
    if (err != nullptr) snprintf(err, err_len, "%s", error_reason_);
    return false;
  }
  f.println(F("timestamp,uptime_ms,ch1_v,ch2_v,ch3_v,ch4_v"));
  f.flush();
  f.close();

  strlcpy(current_file_name_, name, sizeof(current_file_name_));
  samples_logged_ = 0;
  session_year_ = dt.year;
  session_month_ = dt.month;
  session_day_ = dt.day;
  clearError();
  return true;
}

void SdLogger::endLogSession() {
  // Every row is already flushed+closed as it's written, so there's nothing
  // left to finalize — just forget the filename so the web page shows idle.
  current_file_name_[0] = '\0';
}

void SdLogger::rolloverIfNewDay(const RtcDateTime &dt) {
  if (current_file_name_[0] == '\0') return;  // no session open
  if (dt.year == session_year_ && dt.month == session_month_ && dt.day == session_day_) {
    return;
  }

  endLogSession();
  char err[48] = "";
  startNewLogSession(dt, err, sizeof(err));
  // On failure this leaves current_file_name_ empty and hasError() latched
  // (set inside startNewLogSession), exactly like a session that failed to
  // start from the button — the caller's next appendRecord() will simply
  // no-op until logging is manually restarted.
}

bool SdLogger::appendRecord(const SampleRecord &record) {
  if (!sd_ready_ || current_file_name_[0] == '\0') {
    return false;
  }

  char path[20];
  snprintf(path, sizeof(path), "/%s", current_file_name_);

  File f = SD.open(path, FILE_APPEND);
  if (!f) {
    setError("SD write failed (card full?)");
    return false;
  }

  f.print(record.timestamp);
  f.print(',');
  f.print(record.uptime_ms);
  for (uint8_t i = 0; i < 4; i++) {
    f.print(',');
    f.print(record.channels.volts[i], 4);
  }
  f.println();
  f.flush();
  f.close();

  samples_logged_++;
  return true;
}

void SdLogger::setError(const char *reason) {
  error_active_ = true;
  strlcpy(error_reason_, reason, sizeof(error_reason_));
}

void SdLogger::clearError() {
  error_active_ = false;
  error_reason_[0] = '\0';
}

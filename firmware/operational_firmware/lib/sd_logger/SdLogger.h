#pragma once

// SdLogger — writes logged samples to sequentially-numbered SD card files.
//
// Every time the student presses the Log button to *start* a session, we
// scan the card's root directory for the highest-numbered "logNNN.txt"
// already there and open the next one (log007.txt -> log008.txt, etc).
// That means files are never overwritten and there's no need for the RTC
// to be set correctly just to name a file (the RTC is still used for the
// per-row timestamp column, but not for the filename).
//
// Every row is written with FILE_APPEND, then flush()+close() immediately,
// so a sample is never lost to a power interruption mid-session — the same
// durability pattern used by the Ploughmeter reference logger
// (lib/SDLog/SDLog.cpp in that project), just with a different naming
// scheme.

#include <SD.h>
#include <stdint.h>

#include "logger_types.h"

class SdLogger {
 public:
  bool begin(uint8_t cs_pin, uint8_t cd_pin);

  // Card-detect pin reading, refreshed on demand (call periodically from
  // loop() to keep LiveStatus.sd_card_present current).
  bool isCardPresent() const;

  bool isReady() const { return sd_ready_; }

  // Scans for the next available logNNN.txt and opens it for this session,
  // writing the CSV header row. Fails (returns false, fills err) if the
  // card is missing/unreadable or if log999.txt already exists (no numbers
  // left). On failure, the caller must NOT mark logging as active.
  bool startNewLogSession(char *err, size_t err_len);

  // Closes out the current session. Nothing special to flush since every
  // row is already flushed+closed as it's written.
  void endLogSession();

  // Appends one CSV row to the current session's file. Returns false (and
  // latches hasError()) if the write fails, e.g. the card fills up.
  bool appendRecord(const SampleRecord &record);

  // True once latched by a failed startNewLogSession() or a failed
  // appendRecord(); cleared the next time startNewLogSession() succeeds.
  bool hasError() const { return error_active_; }
  const char *errorReason() const { return error_reason_; }

  const char *currentLogFileName() const { return current_file_name_; }
  uint32_t samplesLoggedThisSession() const { return samples_logged_; }

 private:
  uint8_t cs_pin_ = 0;
  uint8_t cd_pin_ = 0;
  bool sd_ready_ = false;

  char current_file_name_[16] = "";  // "" when no session is open
  uint32_t samples_logged_ = 0;

  bool error_active_ = false;
  char error_reason_[48] = "";

  void setError(const char *reason);
  void clearError();

  // Returns the highest N found among "logNNN.txt" files at SD root, or -1
  // if none exist.
  int findHighestLogNumber();
};

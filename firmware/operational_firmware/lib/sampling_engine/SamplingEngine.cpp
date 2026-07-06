#include "SamplingEngine.h"

#include <string.h>

void SamplingEngine::begin(AdcManager *adc, Mcp7940Rtc *rtc, SdLogger *sd_logger) {
  adc_ = adc;
  rtc_ = rtc;
  sd_logger_ = sd_logger;
  next_cycle_due_ms_ = 0;  // start the first cycle as soon as tick() is called
}

void SamplingEngine::configure(uint8_t sample_rate_hz, uint8_t averages_per_sample) {
  if (sample_rate_hz < 1) sample_rate_hz = 1;
  if (sample_rate_hz > 10) sample_rate_hz = 10;
  if (averages_per_sample < 1) averages_per_sample = 1;
  if (averages_per_sample > 16) averages_per_sample = 16;

  target_period_ms_ = 1000UL / sample_rate_hz;
  averages_per_sample_ = averages_per_sample;
}

void SamplingEngine::tick(uint32_t now_ms) {
  switch (phase_) {
    case Phase::WAITING:
      if (now_ms >= next_cycle_due_ms_) {
        startNewCycle(now_ms);
      }
      return;

    case Phase::CONVERTING:
      if (!adc_->conversionReady()) {
        return;  // still converting — check again on the next tick()
      }

      {
        const int16_t counts = adc_->readLastConversion();
        if (!is_settle_read_) {
          accum_[channel_index_] += adc_->countsToVolts(counts);
          reads_kept_++;
        }
      }

      if (is_settle_read_) {
        // Mux-settle read discarded; start the first kept reading.
        is_settle_read_ = false;
        reads_kept_ = 0;
        accum_[channel_index_] = 0.0f;
        beginKeptConversion();
      } else if (reads_kept_ < averages_per_sample_) {
        beginKeptConversion();
      } else {
        // This channel is done — average it and move on.
        latest_.volts[channel_index_] = accum_[channel_index_] / static_cast<float>(averages_per_sample_);
        channel_index_++;
        if (channel_index_ < 4) {
          beginChannelSettle();
        } else {
          finalizeCycle(now_ms);
        }
      }
      return;
  }
}

void SamplingEngine::startNewCycle(uint32_t now_ms) {
  cycle_start_ms_ = now_ms;
  channel_index_ = 0;
  beginChannelSettle();
}

void SamplingEngine::beginChannelSettle() {
  is_settle_read_ = true;
  reads_kept_ = 0;
  accum_[channel_index_] = 0.0f;
  adc_->beginChannelConversion(channel_index_);
  phase_ = Phase::CONVERTING;
}

void SamplingEngine::beginKeptConversion() {
  adc_->beginChannelConversion(channel_index_);
  phase_ = Phase::CONVERTING;
}

void SamplingEngine::finalizeCycle(uint32_t now_ms) {
  const uint32_t elapsed_ms = now_ms - cycle_start_ms_;
  achieved_sample_hz_ = elapsed_ms > 0 ? (1000.0f / static_cast<float>(elapsed_ms)) : 0.0f;

  if (logging_active_ && sd_logger_ != nullptr) {
    SampleRecord record;
    record.uptime_ms = now_ms;
    record.channels = latest_;

    RtcDateTime dt;
    if (rtc_ != nullptr && rtc_->now(dt)) {
      Mcp7940Rtc::formatTimestamp(dt, record.timestamp, sizeof(record.timestamp));
    } else {
      strlcpy(record.timestamp, "--", sizeof(record.timestamp));
    }

    sd_logger_->appendRecord(record);
  }

  // Schedule the next cycle a fixed `target_period_ms_` after this one
  // started (not after it finished) — if this cycle ran long, the next one
  // starts immediately instead of waiting a full extra period.
  next_cycle_due_ms_ = cycle_start_ms_ + target_period_ms_;
  phase_ = Phase::WAITING;
}

#include <algorithm>
#include <cmath>
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "ewh_component.h"

namespace esphome {
namespace ewh {
static const char *const TAG = "ewh.component";

void EWHComponent::setup() {
  EWHComponentBase::setup();
  this->set_interval("ewh_diagnostics", DIAGNOSTICS_INTERVAL_MS, [this]() { this->publish_diagnostics_(); });
}
void EWHComponent::dump_config() {
  LOG_EWH();
  ESP_LOGCONFIG(TAG, "  Communication watchdog: shared RKA layer, 45 s timeout");
}

void EWHComponent::on_invalid_state(const char *reason) {
  this->semantic_invalid_frames_++;
  ESP_LOGW(TAG, "Ignoring semantically invalid state frame: %s", reason != nullptr ? reason : "unknown reason");
  this->publish_diagnostics_();
}

void EWHComponent::on_state(const ewh_state_t &state) {
#ifdef USE_TIME
  this->sync_clock_if_needed_(state);
#endif
#ifdef USE_SWITCH
  if (this->bst_ != nullptr) this->bst_->publish_state(state.bst.state != ewh_bst_t::STATE_OFF);
#endif
#ifdef USE_SENSOR
  if (this->error_code_ != nullptr &&
      (std::isnan(this->error_code_->state) || static_cast<uint8_t>(this->error_code_->state) != state.error))
    this->error_code_->publish_state(state.error);
#endif
  this->publish_diagnostics_();
}

#ifdef USE_TIME
void EWHComponent::sync_clock_if_needed_(const ewh_state_t &state) {
  if (this->time_ == nullptr || state.clock_hours > 23 || state.clock_minutes > 59) return;
  const auto now_time = this->time_->now();
  if (!now_time.is_valid()) return;
  if (state.clock_hours == now_time.hour && state.clock_minutes == now_time.minute) return;

  const int device_minutes = static_cast<int>(state.clock_hours) * 60 + state.clock_minutes;
  const int host_minutes = static_cast<int>(now_time.hour) * 60 + now_time.minute;
  int diff = std::abs(host_minutes - device_minutes);
  diff = std::min(diff, 24 * 60 - diff);
  const uint32_t now_ms = millis();
  const uint32_t elapsed = static_cast<uint32_t>(now_ms - this->last_clock_sync_ms_);
  const bool first_sync = !this->clock_synced_once_;
  const bool materially_wrong = diff >= 2 && elapsed >= CLOCK_SYNC_MIN_INTERVAL_MS;
  const bool periodic_correction = elapsed >= CLOCK_SYNC_FORCE_INTERVAL_MS;
  if (!first_sync && !materially_wrong && !periodic_correction) return;

  this->clock_synced_once_ = true;
  this->last_clock_sync_ms_ = now_ms;
  ESP_LOGD(TAG, "Synchronizing heater clock %02u:%02u -> %02u:%02u", state.clock_hours, state.clock_minutes,
           now_time.hour, now_time.minute);
  this->defer("ewh_clock_sync", [this]() {
    if (this->time_ == nullptr) return;
    const auto current = this->time_->now();
    if (current.is_valid()) this->api_->set_clock(current.hour, current.minute);
  });
}
#endif

void EWHComponent::note_command_retry_() {
  this->command_retries_count_++;
  this->publish_diagnostics_();
}
void EWHComponent::note_command_failure_() {
  this->command_failures_count_++;
  this->publish_diagnostics_();
}

void EWHComponent::publish_diagnostics_() {
#ifdef USE_BINARY_SENSOR
  if (this->communication_ != nullptr) this->communication_->publish_state(this->is_communication_ok());
#endif
#ifdef USE_SENSOR
  auto publish_counter = [](sensor::Sensor *sens, uint32_t value) {
    if (sens != nullptr && (std::isnan(sens->state) || static_cast<uint32_t>(sens->state) != value))
      sens->publish_state(value);
  };
  if (this->communication_age_ != nullptr) {
    const float age = this->get_communication_age_seconds();
    if ((!std::isnan(age) && (std::isnan(this->communication_age_->state) ||
                             std::abs(this->communication_age_->state - age) >= 1.0f)) ||
        (std::isnan(age) && !std::isnan(this->communication_age_->state)))
      this->communication_age_->publish_state(age);
  }
  publish_counter(this->rx_frames_, this->api_->get_rx_frames());
  publish_counter(this->tx_frames_, this->api_->get_tx_frames());
  publish_counter(this->crc_errors_, this->api_->get_crc_errors());
  publish_counter(this->frame_timeouts_, this->api_->get_frame_timeouts());
  publish_counter(this->invalid_frames_, this->api_->get_parser_invalid_frames() + this->semantic_invalid_frames_);
  publish_counter(this->command_retries_sensor_, this->command_retries_count_);
  publish_counter(this->command_failures_sensor_, this->command_failures_count_);
  publish_counter(this->queue_overflows_, this->api_->get_queue_overflows());
#endif
}

}  // namespace ewh
}  // namespace esphome

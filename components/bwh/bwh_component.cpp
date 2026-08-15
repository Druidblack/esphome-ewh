#include <cmath>
#include "esphome/core/log.h"
#include "bwh_component.h"

namespace esphome {
namespace bwh {
static const char *const TAG = "bwh.component";

void BWHComponent::setup() {
  BWHComponentBase::setup();
  this->set_interval("bwh_diagnostics", DIAGNOSTICS_INTERVAL_MS, [this]() { this->publish_diagnostics_(); });
}

void BWHComponent::on_state(const bwh_state_t &state) {
  this->mark_valid_state_frame_();
  (void) state;
  this->publish_diagnostics_();
}

void BWHComponent::on_invalid_state(const char *reason) {
  this->semantic_invalid_frames_++;
  ESP_LOGW(TAG, "Ignoring semantically invalid state frame: %s", reason != nullptr ? reason : "unknown reason");
  this->publish_diagnostics_();
}

void BWHComponent::note_command_retry() {
  this->command_retries_count_++;
  this->publish_diagnostics_();
}

void BWHComponent::note_command_failure() {
  this->command_failures_count_++;
  this->publish_diagnostics_();
}

void BWHComponent::publish_diagnostics_() {
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

}  // namespace bwh
}  // namespace esphome

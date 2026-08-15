#include <cmath>
#include "esphome/core/log.h"
#include "ets_climate.h"

namespace esphome {
namespace ets {

static const char *const TAG = "ets_climate";

void ETSClimate::dump_config() {
  LOG_CLIMATE("", "Electrolux Thermotronic Smart", this);
  this->dump_config_(TAG);
}

ClimateTraits ETSClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_visual_min_temperature(5.0f);
  traits.set_visual_max_temperature(45.0f);
  traits.set_visual_temperature_step(0.1f);
  traits.set_supported_modes({ClimateMode::CLIMATE_MODE_OFF, ClimateMode::CLIMATE_MODE_HEAT});
  return traits;
}

void ETSClimate::control(const ClimateCall &call) {
  if (!call.get_target_temperature().has_value() && !call.get_mode().has_value()) return;

  const float target = call.get_target_temperature().value_or(this->target_temperature);
  const auto mode = call.get_mode().value_or(this->mode);
  if (std::isnan(target) || std::isnan(this->current_temperature) || !this->api_->is_initialized()) {
    ESP_LOGW(TAG, "Thermostat state is not initialized yet; requesting state before control");
    this->api_->request_state();
    return;
  }
  if (target < 5.0f || target > 45.0f) {
    ESP_LOGW(TAG, "Target temperature %.1f is out of range", target);
    return;
  }

  this->start_pending_command_(mode != ClimateMode::CLIMATE_MODE_OFF, target);
}

void ETSClimate::start_pending_command_(bool enabled, float target_temperature) {
  this->cancel_timeout("ets_verify_request");
  this->cancel_timeout("ets_verify_response");
  this->cancel_timeout("ets_command_retry");
  this->pending_command_ = true;
  this->verification_requested_ = false;
  this->verification_mismatch_seen_ = false;
  this->pending_enabled_ = enabled;
  this->pending_target_temperature_ = target_temperature;
  this->pending_retries_ = 0;
  this->send_pending_command_();
}

void ETSClimate::send_pending_command_() {
  if (!this->pending_command_) return;
  if (std::isnan(this->current_temperature) || !this->api_->is_initialized()) {
    this->retry_or_fail_pending_("thermostat state unavailable");
    return;
  }

  this->verification_requested_ = false;
  this->verification_mismatch_seen_ = false;
  bool enabled = this->pending_enabled_;
  this->api_->set_mode(&enabled, this->pending_target_temperature_, this->current_temperature);

  this->set_timeout("ets_verify_request", VERIFY_REQUEST_DELAY_MS, [this]() {
    if (!this->pending_command_) return;
    this->verification_requested_ = true;
    this->verification_mismatch_seen_ = false;
    this->api_->request_state();
    this->set_timeout("ets_verify_response", VERIFY_RESPONSE_TIMEOUT_MS, [this]() {
      if (this->pending_command_ && this->verification_requested_)
        this->retry_or_fail_pending_(this->verification_mismatch_seen_ ? "state mismatch" : "verification state timeout");
    });
  });
}

bool ETSClimate::state_matches_pending_(const ets_state_t &state) const {
  if (state.is_off() == this->pending_enabled_) return false;
  return std::abs(state.target_temp() - this->pending_target_temperature_) <= 0.11f;
}

void ETSClimate::retry_or_fail_pending_(const char *reason) {
  this->cancel_timeout("ets_verify_request");
  this->cancel_timeout("ets_verify_response");
  if (!this->pending_command_) return;
  this->verification_requested_ = false;

  if (this->pending_retries_ < MAX_COMMAND_RETRIES) {
    this->pending_retries_++;
    this->note_command_retry();
    ESP_LOGW(TAG, "Command not confirmed (%s), retry %u/%u", reason, this->pending_retries_, MAX_COMMAND_RETRIES);
    this->set_timeout("ets_command_retry", 150, [this]() { this->send_pending_command_(); });
    return;
  }

  ESP_LOGW(TAG, "Command failed after %u retry (%s); accepting physical thermostat state", MAX_COMMAND_RETRIES, reason);
  this->note_command_failure();
  this->clear_pending_command_();
}

void ETSClimate::clear_pending_command_() {
  this->cancel_timeout("ets_verify_request");
  this->cancel_timeout("ets_verify_response");
  this->cancel_timeout("ets_command_retry");
  this->pending_command_ = false;
  this->verification_requested_ = false;
  this->verification_mismatch_seen_ = false;
  this->pending_retries_ = 0;
}

void ETSClimate::on_state(const ets_state_t &state) {
  this->mark_valid_state_frame_();
  this->api_->init_unk0C(state.unk0C);

  if (this->pending_command_ && this->verification_requested_) {
    if (this->state_matches_pending_(state)) {
      ESP_LOGD(TAG, "Control command confirmed by thermostat state");
      this->clear_pending_command_();
    } else {
      this->verification_mismatch_seen_ = true;
    }
  }

  bool changed = false;
  const float target = state.target_temp();
  const float current = state.air_temp();
  const auto mode = state.is_off() ? climate::CLIMATE_MODE_OFF : climate::CLIMATE_MODE_HEAT;

  if (std::isnan(this->target_temperature) || std::abs(this->target_temperature - target) > 0.05f) {
    this->target_temperature = target;
    changed = true;
  }
  if (std::isnan(this->current_temperature) || std::abs(this->current_temperature - current) > 0.05f) {
    this->current_temperature = current;
    changed = true;
  }
  if (this->mode != mode) {
    this->mode = mode;
    changed = true;
  }
  if (changed) this->publish_state();

  if (this->floor_temp_) {
    const float floor_temp = state.floor_temp();
    if (floor_temp > -10.0f &&
        (std::isnan(this->floor_temp_->state) || std::abs(this->floor_temp_->state - floor_temp) > 0.05f))
      this->floor_temp_->publish_state(floor_temp);
  }
}

}  // namespace ets
}  // namespace esphome

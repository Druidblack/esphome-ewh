#include <cmath>
#include <string>
#include "esphome/core/log.h"
#include "bwh_climate.h"

namespace esphome {
namespace bwh {

static const char *const TAG = "bwh.climate";
static const char *const PRESET_MODE2 = "1.3 kW";
static const char *const PRESET_MODE3 = "2.0 kW";
static const char *const PRESET_DEFAULT = PRESET_MODE2;

void BWHClimate::setup() {
  this->set_supported_custom_presets({PRESET_MODE2, PRESET_MODE3});
  this->set_custom_preset_(PRESET_DEFAULT);
  BWHComponent::setup();
}

void BWHClimate::dump_config() { LOG_CLIMATE("", "Ballu Water Heater", this); }

ClimateTraits BWHClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_visual_min_temperature(bwh::MIN_TEMPERATURE);
  traits.set_visual_max_temperature(bwh::MAX_TEMPERATURE);
  traits.set_visual_temperature_step(1);
  traits.set_supported_modes({ClimateMode::CLIMATE_MODE_OFF, ClimateMode::CLIMATE_MODE_HEAT});
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);
  return traits;
}

bwh_mode_t::Mode BWHClimate::to_wh_mode_(ClimateMode mode, const std::string &preset) const {
  if (mode == ClimateMode::CLIMATE_MODE_OFF) return bwh_mode_t::MODE_OFF;
  if (preset == PRESET_MODE2) return bwh_mode_t::MODE_1300W;
  if (preset == PRESET_MODE3) return bwh_mode_t::MODE_2000W;
  if (mode == ClimateMode::CLIMATE_MODE_HEAT) return bwh_mode_t::MODE_1300W;
  return bwh_mode_t::MODE_OFF;
}

void BWHClimate::control(const ClimateCall &call) {
  const auto mode = call.get_mode().value_or(this->mode);
  std::string preset = this->has_custom_preset() ? this->get_custom_preset().c_str() : PRESET_DEFAULT;
  if (call.has_custom_preset()) preset = call.get_custom_preset().c_str();
  const auto wh_mode = this->to_wh_mode_(mode, preset);
  const auto temp = call.get_target_temperature().value_or(this->target_temperature);

  uint8_t target;
  if (std::isnan(temp)) {
    if (wh_mode != bwh_mode_t::MODE_OFF) {
      ESP_LOGW(TAG, "No target temperature detected");
      return;
    }
    target = bwh::MIN_TEMPERATURE;
  } else {
    target = static_cast<uint8_t>(temp);
  }

  if (target < bwh::MIN_TEMPERATURE || target > bwh::MAX_TEMPERATURE) {
    ESP_LOGW(TAG, "Target temperature is out of range: %u", target);
    return;
  }

  if (wh_mode == bwh_mode_t::MODE_OFF && call.has_custom_preset()) {
    if (this->set_custom_preset_(preset.c_str())) this->publish_state();
  }

  this->start_pending_command_(wh_mode, target);
}

void BWHClimate::start_pending_command_(bwh_mode_t::Mode mode, uint8_t temperature) {
  this->cancel_timeout("bwh_verify_request");
  this->cancel_timeout("bwh_verify_response");
  this->cancel_timeout("bwh_command_retry");
  this->pending_command_ = true;
  this->verification_requested_ = false;
  this->verification_mismatch_seen_ = false;
  this->pending_mode_ = mode;
  this->pending_temperature_ = temperature;
  this->pending_retries_ = 0;
  this->send_pending_command_();
}

void BWHClimate::send_pending_command_() {
  if (!this->pending_command_) return;
  this->verification_requested_ = false;
  this->verification_mismatch_seen_ = false;
  this->api_->set_mode(this->pending_mode_, this->pending_temperature_);
  this->set_timeout("bwh_verify_request", VERIFY_REQUEST_DELAY_MS, [this]() {
    if (!this->pending_command_) return;
    this->verification_requested_ = true;
    this->verification_mismatch_seen_ = false;
    this->api_->request_state();
    this->set_timeout("bwh_verify_response", VERIFY_RESPONSE_TIMEOUT_MS, [this]() {
      if (this->pending_command_ && this->verification_requested_)
        this->retry_or_fail_pending_(this->verification_mismatch_seen_ ? "state mismatch" : "verification state timeout");
    });
  });
}

bool BWHClimate::state_matches_pending_(const bwh_state_t &state) const {
  bwh_state_t::State expected = bwh_state_t::STATE_OFF;
  switch (this->pending_mode_) {
    case bwh_mode_t::MODE_OFF: expected = bwh_state_t::STATE_OFF; break;
    case bwh_mode_t::MODE_1300W: expected = bwh_state_t::STATE_1300W; break;
    case bwh_mode_t::MODE_2000W: expected = bwh_state_t::STATE_2000W; break;
    default: return false;
  }
  if (state.state != expected) return false;
  if (this->pending_mode_ != bwh_mode_t::MODE_OFF) return state.target_temperature == this->pending_temperature_;
  return true;
}

void BWHClimate::retry_or_fail_pending_(const char *reason) {
  this->cancel_timeout("bwh_verify_request");
  this->cancel_timeout("bwh_verify_response");
  if (!this->pending_command_) return;
  this->verification_requested_ = false;
  if (this->pending_retries_ < MAX_COMMAND_RETRIES) {
    this->pending_retries_++;
    this->note_command_retry();
    ESP_LOGW(TAG, "Command not confirmed (%s), retry %u/%u", reason, this->pending_retries_, MAX_COMMAND_RETRIES);
    this->set_timeout("bwh_command_retry", 100, [this]() { this->send_pending_command_(); });
    return;
  }
  ESP_LOGW(TAG, "Command failed after %u retries (%s); accepting physical heater state", MAX_COMMAND_RETRIES, reason);
  this->note_command_failure();
  this->clear_pending_command_();
}

void BWHClimate::clear_pending_command_() {
  this->cancel_timeout("bwh_verify_request");
  this->cancel_timeout("bwh_verify_response");
  this->cancel_timeout("bwh_command_retry");
  this->pending_command_ = false;
  this->verification_requested_ = false;
  this->verification_mismatch_seen_ = false;
  this->pending_retries_ = 0;
}

void BWHClimate::on_state(const bwh_state_t &status) {
  if (this->pending_command_ && this->verification_requested_) {
    if (this->state_matches_pending_(status)) {
      ESP_LOGD(TAG, "Control command confirmed by heater state");
      this->clear_pending_command_();
    } else {
      this->verification_mismatch_seen_ = true;
    }
  }

  bool has_changes{};
  if (status.target_temperature < bwh::MIN_TEMPERATURE || status.target_temperature > bwh::MAX_TEMPERATURE) {
    ESP_LOGW(TAG, "Target temperature is out of range %u", status.target_temperature);
    if (!std::isnan(this->target_temperature)) {
      this->target_temperature = NAN;
      has_changes = true;
    }
  } else if (std::isnan(this->target_temperature) ||
             static_cast<uint8_t>(this->target_temperature) != status.target_temperature) {
    this->target_temperature = status.target_temperature;
    has_changes = true;
  }

  if (std::isnan(this->current_temperature) ||
      static_cast<uint8_t>(this->current_temperature) != status.current_temperature) {
    this->current_temperature = status.current_temperature;
    has_changes = true;
  }

  auto mode = climate::CLIMATE_MODE_OFF;
  auto action = climate::CLIMATE_ACTION_OFF;
  std::string preset = this->has_custom_preset() ? this->get_custom_preset().c_str() : PRESET_DEFAULT;
  if (status.state != bwh_state_t::STATE_OFF) {
    mode = climate::CLIMATE_MODE_HEAT;
    const bool is_heating =
        (static_cast<int>(status.target_temperature) - static_cast<int>(status.current_temperature)) > 1;
    action = is_heating ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;
    if (status.state == bwh_state_t::STATE_1300W) preset = PRESET_MODE2;
    else if (status.state == bwh_state_t::STATE_2000W) preset = PRESET_MODE3;
    else ESP_LOGW(TAG, "Unknown state %02X", status.state);
  }

  if (mode != this->mode) { this->mode = mode; has_changes = true; }
  if (action != this->action) { this->action = action; has_changes = true; }
  if (!this->has_custom_preset() || preset != this->get_custom_preset().c_str()) {
    if (this->set_custom_preset_(preset.c_str())) has_changes = true;
  }

  if (has_changes) this->publish_state();
  BWHComponent::on_state(status);
}

}  // namespace bwh
}  // namespace esphome

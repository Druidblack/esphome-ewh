#include <algorithm>
#include <cmath>
#include "esphome/core/log.h"
#include "ehu_component.h"

namespace esphome {
namespace ehu {

static const char *const TAG = "ehu.component";

static const std::string PRESET__EMPTY = "";
static const std::string PRESET_AUTO = "Auto";
static const std::string PRESET_HEALTH = "Health";
static const std::string PRESET_NIGHT = "Night";
static const std::string PRESET_BABY = "Baby";
static const std::string PRESET_FITNESS = "Fitness";
static const std::string PRESET_YOGA = "Yoga";
static const std::string PRESET_MEDITATION = "Meditation";
static const std::string PRESET_PRANA = "Prana";
static const std::string PRESET_MANUAL = "Manual";

static const std::string &LED_PRESET__EMPTY = PRESET__EMPTY;
static const std::string LED_PRESET_OFF = "None";
static const std::string LED_PRESET_RANDOM = "Random";
static const std::string LED_PRESET_BLUE = "Blue";
static const std::string LED_PRESET_GREEN = "Green";
static const std::string LED_PRESET_WHITE = "White";

#define ALL_FAN_PRESETS \
  { \
    PRESET_AUTO, PRESET_HEALTH, PRESET_NIGHT, PRESET_BABY, PRESET_FITNESS, PRESET_YOGA, PRESET_MEDITATION, \
        PRESET_PRANA, PRESET_MANUAL, \
  }

#define ALL_LED_PRESETS \
  { LED_PRESET_OFF, LED_PRESET_RANDOM, LED_PRESET_BLUE, LED_PRESET_GREEN, LED_PRESET_WHITE }

void EHUComponent::setup() {
  EHUComponentBase::setup();
  this->set_interval("ehu_diagnostics", DIAGNOSTICS_INTERVAL_MS, [this]() { this->publish_diagnostics_(); });
}

void EHUComponent::dump_config_(const char *TAG) const {
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SWITCH("  ", "Warm Mist", this->warm_mist_);
  LOG_SWITCH("  ", "UV", this->uv_);
  LOG_SWITCH("  ", "Ionizer", this->ionizer_);
  LOG_SWITCH("  ", "Lock", this->lock_);
  LOG_SWITCH("  ", "Mute", this->mute_);
  LOG_BINARY_SENSOR("  ", "Water", this->water_);
  LOG_NUMBER("  ", "Target Humidity", this->target_humidity_);
  LOG_NUMBER("  ", "Speed", this->fan_speed_);
  LOG_SELECT("  ", "Preset", this->fan_preset_);
  LOG_NUMBER("  ", "LED Brightness", this->led_brightness_);
  LOG_SWITCH("  ", "LED Top", this->led_top_);
  LOG_SWITCH("  ", "LED Bottom", this->led_bottom_);
  LOG_SELECT("  ", "LED Preset", this->led_preset_);
}

void EHUComponent::on_invalid_state(const char *reason) {
  this->semantic_invalid_frames_++;
  ESP_LOGW(TAG, "Ignoring semantically invalid state frame: %s", reason != nullptr ? reason : "unknown reason");
  this->publish_diagnostics_();
}

void EHUComponent::on_state(const ehu_state_t &state) {
  this->mark_valid_state_frame_();
  this->st_ = state;
  this->has_st_ = true;

  if (this->pending_controls_ && this->verification_requested_) {
    if (this->clear_confirmed_commands_(state)) {
      ESP_LOGD(TAG, "Control batch confirmed by humidifier state");
      this->clear_pending_controls_();
    } else {
      // An unsolicited state can cross the verification request. Keep the
      // mismatched fields pending until the response window expires.
      this->verification_mismatch_seen_ = true;
    }
  }

#ifdef USE_TIME
  this->sync_clock_if_needed_(state);
#endif

  this->publish_state_(this->temperature_, state.temperature);
  this->publish_state_(this->humidity_, state.humidity);
  this->publish_state_(this->warm_mist_, state.water_flags & ehu_state_t::WATER_WARM_MIST);
  this->publish_state_(this->uv_, state.water_flags & ehu_state_t::WATER_UV);
  this->publish_state_(this->ionizer_, state.ionizer);
  this->publish_state_(this->lock_, state.lock);
  this->publish_state_(this->mute_, state.mute);
  this->publish_state_(this->water_, !state.water_tank_empty);

  this->publish_fan_state_(state);

  // The device may transiently change target humidity when fan speed changes.
  this->set_timeout("update_target_humidity", 100,
                    [this]() { this->publish_state_(this->target_humidity_, this->st_.target_humidity); });

  if (state.fan_speed > 0) this->publish_state_(this->fan_speed_, state.fan_speed);

  if (this->fan_preset_) {
    const auto &preset = this->get_fan_preset_(state);
    if (!preset.empty() && this->fan_preset_->state != preset) this->fan_preset_->publish_state(preset);
  }

  // Random changes brightness continuously; do not bounce the HA slider.
  if (state.led_preset != ehu_state_t::LED_PRESET_RANDOM)
    this->publish_state_(this->led_brightness_, state.led_brightness);

  this->publish_state_(this->led_top_, state.led_mode & ehu_state_t::LED_MODE_TOP);
  this->publish_state_(this->led_bottom_, state.led_mode & ehu_state_t::LED_MODE_BOTTOM);

  if (this->led_preset_) {
    const auto &preset = this->get_led_preset_(state);
    if (!preset.empty() && this->led_preset_->state != preset) this->led_preset_->publish_state(preset);
  }

  this->publish_state_(this->humidification_, state.fan_speed != 0);
  this->publish_diagnostics_();
}

#ifdef USE_TIME
void EHUComponent::sync_clock_if_needed_(const ehu_state_t &state) {
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
  ESP_LOGD(TAG, "Synchronizing humidifier clock %02u:%02u -> %02u:%02u", state.clock_hours, state.clock_minutes,
           now_time.hour, now_time.minute);
  this->defer("ehu_clock_sync", [this]() {
    if (this->time_ == nullptr) return;
    const auto current = this->time_->now();
    if (current.is_valid()) this->api_->set_clock(current.hour, current.minute);
  });
}
#endif

void EHUComponent::send_control_byte_(uint8_t command, uint8_t value) {
  this->remember_pending_command_(command, value);
  this->pending_controls_ = true;
  this->pending_retries_ = 0;
  this->verification_requested_ = false;
  this->verification_mismatch_seen_ = false;
  this->api_->write_byte(command, value);
  this->schedule_control_verification_();
}

void EHUComponent::remember_pending_command_(uint8_t command, uint8_t value) {
  for (auto &pending : this->pending_commands_) {
    if (pending.active && pending.command == command) {
      pending.value = value;
      return;
    }
  }
  for (auto &pending : this->pending_commands_) {
    if (!pending.active) {
      pending.command = command;
      pending.value = value;
      pending.active = true;
      return;
    }
  }
  // This should not normally happen because there are fewer controllable byte
  // fields than slots. Preserve the latest command and make it visible.
  ESP_LOGW(TAG, "Pending control table full; replacing oldest slot with command %02X", command);
  this->pending_commands_[0] = PendingByteCommand{command, value, true};
  this->note_command_failure_();
}

void EHUComponent::schedule_control_verification_() {
  this->cancel_timeout("ehu_verify_request");
  this->cancel_timeout("ehu_verify_response");
  this->set_timeout("ehu_verify_request", VERIFY_REQUEST_DELAY_MS, [this]() {
    if (!this->pending_controls_) return;
    this->verification_requested_ = true;
    this->verification_mismatch_seen_ = false;
    this->api_->request_state_ex();
    this->set_timeout("ehu_verify_response", VERIFY_RESPONSE_TIMEOUT_MS, [this]() {
      if (this->pending_controls_ && this->verification_requested_)
        this->retry_or_fail_controls_(this->verification_mismatch_seen_ ? "state mismatch" : "verification state timeout");
    });
  });
}

void EHUComponent::resend_pending_commands_() {
  for (const auto &pending : this->pending_commands_) {
    if (pending.active) this->api_->write_byte(pending.command, pending.value);
  }
}

bool EHUComponent::command_matches_state_(const PendingByteCommand &command, const ehu_state_t &state) const {
  switch (command.command) {
    case ehu_packet_type_t::PACKET_REQ_SET_POWER:
      return state.power == (command.value != 0);
    case ehu_packet_type_t::PACKET_REQ_SET_PRESET:
      return state.preset == command.value;
    case ehu_packet_type_t::PACKET_REQ_SET_SPEED:
      return state.fan_speed == command.value;
    case ehu_packet_type_t::PACKET_REQ_SET_HUMIDITY:
      return state.target_humidity == command.value;
    case ehu_packet_type_t::PACKET_REQ_SET_WARM_MIST_UV:
      return state.water_flags == (command.value & (ehu_state_t::WATER_WARM_MIST | ehu_state_t::WATER_UV));
    case ehu_packet_type_t::PACKET_REQ_SET_LOCK:
      return state.lock == (command.value != 0);
    case ehu_packet_type_t::PACKET_REQ_SET_MUTE:
      return state.mute == (command.value != 0);
    case ehu_packet_type_t::PACKET_REQ_SET_IONIZER:
      return state.ionizer == (command.value != 0);
    case ehu_packet_type_t::PACKET_REQ_SET_LED_BRIGHTNESS:
      return state.led_brightness == command.value;
    case ehu_packet_type_t::PACKET_REQ_SET_LED_PRESET:
      return state.led_preset == command.value;
    case ehu_packet_type_t::PACKET_REQ_SET_LED_MODE:
      return state.led_mode == command.value;
    default:
      // Unknown byte commands cannot be semantically verified here.
      return true;
  }
}

bool EHUComponent::clear_confirmed_commands_(const ehu_state_t &state) {
  bool any_active = false;
  for (auto &pending : this->pending_commands_) {
    if (!pending.active) continue;
    if (this->command_matches_state_(pending, state)) pending.active = false;
    else any_active = true;
  }
  return !any_active;
}

void EHUComponent::retry_or_fail_controls_(const char *reason) {
  this->cancel_timeout("ehu_verify_request");
  this->cancel_timeout("ehu_verify_response");
  if (!this->pending_controls_) return;
  this->verification_requested_ = false;

  if (this->pending_retries_ < MAX_COMMAND_RETRIES) {
    this->pending_retries_++;
    this->note_command_retry_();
    ESP_LOGW(TAG, "Control batch not confirmed (%s), retry %u/%u", reason, this->pending_retries_, MAX_COMMAND_RETRIES);
    this->set_timeout("ehu_command_retry", 150, [this]() {
      if (!this->pending_controls_) return;
      this->resend_pending_commands_();
      this->schedule_control_verification_();
    });
    return;
  }

  ESP_LOGW(TAG, "Control batch failed after %u retries (%s); accepting physical humidifier state",
           MAX_COMMAND_RETRIES, reason);
  this->note_command_failure_();
  this->clear_pending_controls_();
}

void EHUComponent::clear_pending_controls_() {
  this->cancel_timeout("ehu_verify_request");
  this->cancel_timeout("ehu_verify_response");
  this->cancel_timeout("ehu_command_retry");
  for (auto &pending : this->pending_commands_) pending.active = false;
  this->pending_controls_ = false;
  this->verification_requested_ = false;
  this->verification_mismatch_seen_ = false;
  this->pending_retries_ = 0;
}

void EHUComponent::note_command_retry_() {
  this->command_retries_count_++;
  this->publish_diagnostics_();
}

void EHUComponent::note_command_failure_() {
  this->command_failures_count_++;
  this->publish_diagnostics_();
}

void EHUComponent::publish_diagnostics_() {
  if (this->communication_ != nullptr) this->communication_->publish_state(this->is_communication_ok());

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
}

void EHUComponent::publish_fan_state_(const ehu_state_t &state) {
  if (this->fan_ == nullptr) return;

  bool has_changes{};
  if (state.power != this->fan_->state) {
    this->fan_->state = state.power;
    has_changes = true;
  }
  if (state.fan_speed > 0 && state.fan_speed != this->fan_->speed) {
    this->fan_->speed = state.fan_speed;
    has_changes = true;
  }
  const auto &preset = this->get_fan_preset_(state);
  if (preset != this->fan_->preset_mode) {
    this->fan_->preset_mode = preset;
    has_changes = true;
  }
  if (has_changes) this->fan_->publish_state();
}

const std::string &EHUComponent::get_fan_preset_(const ehu_state_t &state) const {
  switch (state.preset) {
    case ehu_state_t::PRESET_AUTO: return PRESET_AUTO;
    case ehu_state_t::PRESET_BABY: return PRESET_BABY;
    case ehu_state_t::PRESET_FITNESS: return PRESET_FITNESS;
    case ehu_state_t::PRESET_HEALTH: return PRESET_HEALTH;
    case ehu_state_t::PRESET_MANUAL: return PRESET_MANUAL;
    case ehu_state_t::PRESET_MEDITATION: return PRESET_MEDITATION;
    case ehu_state_t::PRESET_NIGHT: return PRESET_NIGHT;
    case ehu_state_t::PRESET_PRANA: return PRESET_PRANA;
    case ehu_state_t::PRESET_YOGA: return PRESET_YOGA;
    default:
      ESP_LOGW(TAG, "Unknown fan preset %u", state.preset);
      return PRESET__EMPTY;
  }
}

void EHUComponent::write_fan_preset_(const std::string &preset) {
  if (preset.empty()) return;
  uint8_t value = ehu_state_t::PRESET_MANUAL;
  if (preset == PRESET_AUTO) value = ehu_state_t::PRESET_AUTO;
  else if (preset == PRESET_HEALTH) value = ehu_state_t::PRESET_HEALTH;
  else if (preset == PRESET_NIGHT) value = ehu_state_t::PRESET_NIGHT;
  else if (preset == PRESET_BABY) value = ehu_state_t::PRESET_BABY;
  else if (preset == PRESET_FITNESS) value = ehu_state_t::PRESET_FITNESS;
  else if (preset == PRESET_YOGA) value = ehu_state_t::PRESET_YOGA;
  else if (preset == PRESET_MEDITATION) value = ehu_state_t::PRESET_MEDITATION;
  else if (preset == PRESET_PRANA) value = ehu_state_t::PRESET_PRANA;
  this->send_control_byte_(ehu_packet_type_t::PACKET_REQ_SET_PRESET, value);
}

const std::string &EHUComponent::get_led_preset_(const ehu_state_t &state) const {
  switch (state.led_preset) {
    case ehu_state_t::LED_PRESET_OFF: return LED_PRESET_OFF;
    case ehu_state_t::LED_PRESET_RANDOM: return LED_PRESET_RANDOM;
    case ehu_state_t::LED_PRESET_BLUE: return LED_PRESET_BLUE;
    case ehu_state_t::LED_PRESET_GREEN: return LED_PRESET_GREEN;
    case ehu_state_t::LED_PRESET_WHITE: return LED_PRESET_WHITE;
    default:
      ESP_LOGW(TAG, "Unknown LED preset %u", state.led_preset);
      return LED_PRESET__EMPTY;
  }
}

void EHUComponent::set_fan_speed_(uint8_t fan_speed) {
  uint8_t target_humidity = this->target_humidity_ ? static_cast<uint8_t>(this->target_humidity_->state) : 0;
  this->send_control_byte_(ehu_packet_type_t::PACKET_REQ_SET_SPEED, fan_speed);
  if (target_humidity >= HUMIDITY_MIN && target_humidity <= HUMIDITY_MAX)
    this->send_control_byte_(ehu_packet_type_t::PACKET_REQ_SET_HUMIDITY, target_humidity);
}

void EHUFan::control(const fan::FanCall &call) {
  if (call.get_state().has_value() && !*call.get_state()) this->parent_->set_fan_speed_(0);
  if (call.get_speed().has_value()) this->parent_->set_fan_speed_(*call.get_speed());
  this->parent_->write_fan_preset_(call.get_preset_mode());
}

fan::FanTraits EHUFan::get_traits() {
  auto traits = fan::FanTraits();
  traits.set_speed(true);
  traits.set_supported_speed_count(3);
  traits.set_supported_preset_modes(ALL_FAN_PRESETS);
  return traits;
}

void EHUFanPreset::setup() { this->traits.set_options(ALL_FAN_PRESETS); }
void EHULedPreset::setup() { this->traits.set_options(ALL_LED_PRESETS); }

void EHULedPreset::control(const std::string &value) {
  if (value.empty()) return;

  if (value == LED_PRESET_OFF) {
    this->parent_->send_control_byte_(ehu_packet_type_t::PACKET_REQ_SET_LED_PRESET, ehu_state_t::LED_PRESET_OFF);
    return;
  }
  if (value == LED_PRESET_RANDOM) {
    this->parent_->send_control_byte_(ehu_packet_type_t::PACKET_REQ_SET_LED_PRESET, ehu_state_t::LED_PRESET_RANDOM);
    return;
  }

  uint8_t preset;
  if (value == LED_PRESET_BLUE) preset = ehu_state_t::LED_PRESET_BLUE;
  else if (value == LED_PRESET_GREEN) preset = ehu_state_t::LED_PRESET_GREEN;
  else if (value == LED_PRESET_WHITE) preset = ehu_state_t::LED_PRESET_WHITE;
  else {
    ESP_LOGW(TAG, "Invalid LED preset %s", value.c_str());
    return;
  }

  const bool powered = this->parent_->has_st() && this->parent_->st().power;
  if (!powered)
    this->parent_->send_control_byte_(ehu_packet_type_t::PACKET_REQ_SET_LED_PRESET, ehu_state_t::LED_PRESET_RANDOM);

  // When the humidifier itself is off, Random must be applied first to wake
  // the LEDs; only then can a fixed color be selected.
  const uint32_t timeout = powered ? 0 : 100;
  this->parent_->set_timeout("set_led_preset", timeout, [parent = this->parent_, preset]() {
    parent->send_control_byte_(ehu_packet_type_t::PACKET_REQ_SET_LED_PRESET, preset);
  });
}

}  // namespace ehu
}  // namespace esphome

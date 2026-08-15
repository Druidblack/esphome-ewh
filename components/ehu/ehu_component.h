#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include "esphome/core/component.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#include "../rka_api/rka_component.h"
#include "ehu_api.h"

namespace esphome {
namespace ehu {

using EHUComponentBase = rka_api::RKAComponent<rka_api::rka_dev_type_t::EHU, EHUListener, EHUApi, PollingComponent>;

class EHUComponent : public EHUComponentBase {
  template<uint16_t cmd_v> friend class EHUCommandComponent;
  friend class EHUFan;
  friend class EHUFanPreset;
  friend class EHULedPreset;
  friend class EHUPowerSwitch;

 public:
  explicit EHUComponent(EHUApi *api) : EHUComponentBase(api) {}
  void setup() override;
  void on_state(const ehu_state_t &state) override;
  void on_invalid_state(const char *reason) override;

  void update() override { this->api_->request_state_ex(); }

#ifdef USE_TIME
  void set_time_id(esphome::time::RealTimeClock *time) { this->time_ = time; }
#endif

  void set_temperature_sensor(sensor::Sensor *temperature) { this->temperature_ = temperature; }
  void set_humidity_sensor(sensor::Sensor *humidity) { this->humidity_ = humidity; }
  void set_warm_mist_switch(switch_::Switch *warm_mist) { this->warm_mist_ = warm_mist; }
  void set_uv_switch(switch_::Switch *uv) { this->uv_ = uv; }
  void set_ionizer_switch(switch_::Switch *ionizer) { this->ionizer_ = ionizer; }
  void set_lock_switch(switch_::Switch *lock) { this->lock_ = lock; }
  void set_mute_switch(switch_::Switch *mute) { this->mute_ = mute; }
  void set_water_binary_sensor(binary_sensor::BinarySensor *water) { this->water_ = water; }
  void set_humidification_binary_sensor(binary_sensor::BinarySensor *humidification) {
    this->humidification_ = humidification;
  }
  void set_fan(fan::Fan *fan) { this->fan_ = fan; }
  void set_target_humidity_number(number::Number *target_humidity) { this->target_humidity_ = target_humidity; }
  void set_fan_speed_number(number::Number *fan_speed) { this->fan_speed_ = fan_speed; }
  void set_fan_preset_select(select::Select *fan_preset) { this->fan_preset_ = fan_preset; }
  void set_led_brightness_number(number::Number *led_brightness) { this->led_brightness_ = led_brightness; }
  void set_led_top_switch(switch_::Switch *led_top) { this->led_top_ = led_top; }
  void set_led_bottom_switch(switch_::Switch *led_bottom) { this->led_bottom_ = led_bottom; }
  void set_led_preset_select(select::Select *led_preset) { this->led_preset_ = led_preset; }

  void set_communication(binary_sensor::BinarySensor *sensor) { this->communication_ = sensor; }
  void set_communication_age(sensor::Sensor *sensor) { this->communication_age_ = sensor; }
  void set_rx_frames(sensor::Sensor *sensor) { this->rx_frames_ = sensor; }
  void set_tx_frames(sensor::Sensor *sensor) { this->tx_frames_ = sensor; }
  void set_crc_errors(sensor::Sensor *sensor) { this->crc_errors_ = sensor; }
  void set_frame_timeouts(sensor::Sensor *sensor) { this->frame_timeouts_ = sensor; }
  void set_invalid_frames(sensor::Sensor *sensor) { this->invalid_frames_ = sensor; }
  void set_command_retries(sensor::Sensor *sensor) { this->command_retries_sensor_ = sensor; }
  void set_command_failures(sensor::Sensor *sensor) { this->command_failures_sensor_ = sensor; }
  void set_queue_overflows(sensor::Sensor *sensor) { this->queue_overflows_ = sensor; }

  const ehu_state_t &st() const { return this->st_; }
  bool has_st() const { return this->has_st_; }

 protected:
  static constexpr uint8_t MAX_COMMAND_RETRIES = 2;
  static constexpr uint32_t VERIFY_REQUEST_DELAY_MS = 250;
  static constexpr uint32_t VERIFY_RESPONSE_TIMEOUT_MS = 1200;
  static constexpr uint32_t DIAGNOSTICS_INTERVAL_MS = 5000;
#ifdef USE_TIME
  static constexpr uint32_t CLOCK_SYNC_MIN_INTERVAL_MS = 10UL * 60UL * 1000UL;
  static constexpr uint32_t CLOCK_SYNC_FORCE_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
#endif

  struct PendingByteCommand {
    uint8_t command{};
    uint8_t value{};
    bool active{};
  };

  ehu_state_t st_{};
  bool has_st_{};
  std::array<PendingByteCommand, 12> pending_commands_{};
  bool pending_controls_{};
  bool verification_requested_{};
  bool verification_mismatch_seen_{};
  uint8_t pending_retries_{};
  uint32_t semantic_invalid_frames_{};
  uint32_t command_retries_count_{};
  uint32_t command_failures_count_{};

#ifdef USE_TIME
  esphome::time::RealTimeClock *time_{};
  bool clock_synced_once_{};
  uint32_t last_clock_sync_ms_{};
#endif

  sensor::Sensor *temperature_{};
  sensor::Sensor *humidity_{};

  switch_::Switch *warm_mist_{};
  switch_::Switch *uv_{};
  switch_::Switch *ionizer_{};
  switch_::Switch *lock_{};
  switch_::Switch *mute_{};

  binary_sensor::BinarySensor *water_{};
  binary_sensor::BinarySensor *humidification_{};
  binary_sensor::BinarySensor *communication_{};

  fan::Fan *fan_{};

  number::Number *target_humidity_{};
  number::Number *fan_speed_{};
  select::Select *fan_preset_{};
  number::Number *led_brightness_{};
  switch_::Switch *led_top_{};
  switch_::Switch *led_bottom_{};
  select::Select *led_preset_{};

  sensor::Sensor *communication_age_{};
  sensor::Sensor *rx_frames_{};
  sensor::Sensor *tx_frames_{};
  sensor::Sensor *crc_errors_{};
  sensor::Sensor *frame_timeouts_{};
  sensor::Sensor *invalid_frames_{};
  sensor::Sensor *command_retries_sensor_{};
  sensor::Sensor *command_failures_sensor_{};
  sensor::Sensor *queue_overflows_{};

  void dump_config_(const char *TAG) const;

  template<class T, typename V> inline void publish_state_(T *var, V val) {
    if (var != nullptr && static_cast<V>(var->state) != val) var->publish_state(val);
  }

  void publish_fan_state_(const ehu_state_t &state);
  void write_fan_preset_(const std::string &preset);
  const std::string &get_fan_preset_(const ehu_state_t &state) const;
  const std::string &get_led_preset_(const ehu_state_t &state) const;
  void set_fan_speed_(uint8_t fan_speed);

  void send_control_byte_(uint8_t command, uint8_t value);
  void remember_pending_command_(uint8_t command, uint8_t value);
  void schedule_control_verification_();
  void resend_pending_commands_();
  bool command_matches_state_(const PendingByteCommand &command, const ehu_state_t &state) const;
  bool clear_confirmed_commands_(const ehu_state_t &state);
  void retry_or_fail_controls_(const char *reason);
  void clear_pending_controls_();
  void note_command_retry_();
  void note_command_failure_();
  void publish_diagnostics_();
#ifdef USE_TIME
  void sync_clock_if_needed_(const ehu_state_t &state);
#endif
};

template<uint16_t cmd_v> class EHUCommandComponent : public Component, public Parented<EHUComponent> {
 public:
  EHUCommandComponent(EHUComponent *c) : Parented(c) {}

 protected:
  void write_byte_(uint8_t data) {
    if constexpr (cmd_v != 0) this->parent_->send_control_byte_(cmd_v, data);
  }
};

template<uint16_t cmd_v> class EHUSwitch : public switch_::Switch, public EHUCommandComponent<cmd_v> {
 public:
  EHUSwitch(EHUComponent *c) : EHUCommandComponent<cmd_v>(c) {}
  void write_state(bool state) override { this->write_byte_(state); }
};

template<uint16_t cmd_v, uint16_t state_flag_first_v, uint16_t state_flag_second_v>
class EHUDependedSwitch : public EHUSwitch<cmd_v> {
 public:
  EHUDependedSwitch(EHUComponent *c, const uint8_t *st_value) : EHUSwitch<cmd_v>(c), st_value_(st_value) {}
  void set_st_value(uint8_t *st_value) { this->st_value_ = st_value; }

  void write_state(bool state) override {
    uint8_t data = 0;
    if (state) data |= state_flag_first_v;
    if (this->st_value_ && *this->st_value_ & state_flag_second_v) data |= state_flag_second_v;
    this->write_byte_(data);
  }

 protected:
  const uint8_t *st_value_;
};

class EHUFan : public Component, public fan::Fan, Parented<EHUComponent> {
 public:
  explicit EHUFan(EHUComponent *c) : Parented(c) {}
  fan::FanTraits get_traits() override;
  void control(const fan::FanCall &call) override;
};

template<uint16_t cmd_v> class EHUNumber : public number::Number, public EHUCommandComponent<cmd_v> {
 public:
  EHUNumber(EHUComponent *c) : EHUCommandComponent<cmd_v>(c) {}
  void control(float value) override { this->write_byte_(static_cast<uint8_t>(value)); }
};

class EHUFanPreset : public select::Select, public Component, Parented<EHUComponent> {
 public:
  EHUFanPreset(EHUComponent *c) : Parented(c) {}
  void setup() override;
  void control(const std::string &value) override { this->parent_->write_fan_preset_(value); }
};

class EHULedPreset : public select::Select, public Component, Parented<EHUComponent> {
 public:
  EHULedPreset(EHUComponent *c) : Parented(c) {}
  void setup() override;
  void control(const std::string &value) override;
};

class EHUBrightnessNumber : public EHUNumber<ehu_packet_type_t::PACKET_REQ_SET_LED_BRIGHTNESS> {
 public:
  EHUBrightnessNumber(EHUComponent *c) : EHUNumber(c) {}
  void control(float value) override {
    // Random mode continuously changes brightness itself. Only allow manual
    // brightness when state is unknown or the current preset is not Random.
    if (!this->parent_->has_st() || this->parent_->st().led_preset != ehu_state_t::LED_PRESET_RANDOM)
      EHUNumber::control(value);
  }
};

}  // namespace ehu
}  // namespace esphome

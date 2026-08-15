#pragma once

#include "../rka_api/rka_api.h"
#include "vport/ehu_vport.h"
#include "ehu_data.h"

namespace esphome {
namespace ehu {

class EHUListener : public rka_api::RKAListener<ehu_state_t> {
 public:
  void on_frame(const rka_api::rka_any_frame_t &frame, size_t size) override {
    if (frame.type == rka_api::PACKET_CMD_STATE || frame.type == rka_api::PACKET_RSP_STATE) {
      if (!this->check_packet_size_(size, sizeof(ehu_state_t))) {
        this->on_invalid_state("invalid packet size");
        return;
      }
      const auto &state = *reinterpret_cast<const ehu_state_t *>(frame.data);
      const char *reason = nullptr;
      if (!validate_state(state, &reason)) {
        this->on_invalid_state(reason != nullptr ? reason : "invalid state");
        return;
      }
      this->on_state(state);
      return;
    }
    rka_api::RKAListener<ehu_state_t>::on_frame(frame, size);
  }

  static bool validate_state(const ehu_state_t &state, const char **reason = nullptr) {
    switch (state.preset) {
      case 0:  // Some firmwares can report no active preset while powered off.
      case ehu_state_t::PRESET_AUTO:
      case ehu_state_t::PRESET_HEALTH:
      case ehu_state_t::PRESET_NIGHT:
      case ehu_state_t::PRESET_BABY:
      case ehu_state_t::PRESET_FITNESS:
      case ehu_state_t::PRESET_YOGA:
      case ehu_state_t::PRESET_MEDITATION:
      case ehu_state_t::PRESET_PRANA:
      case ehu_state_t::PRESET_MANUAL:
        break;
      default:
        if (reason != nullptr) *reason = "fan preset out of range";
        return false;
    }
    if (state.water_flags & ~(ehu_state_t::WATER_WARM_MIST | ehu_state_t::WATER_UV)) {
      if (reason != nullptr) *reason = "water flags out of range";
      return false;
    }
    if (state.led_preset > ehu_state_t::LED_PRESET_WHITE) {
      if (reason != nullptr) *reason = "LED preset out of range";
      return false;
    }
    if (state.led_mode & ~(ehu_state_t::LED_MODE_BOTTOM | ehu_state_t::LED_MODE_TOP)) {
      if (reason != nullptr) *reason = "LED mode out of range";
      return false;
    }
    if (state.fan_speed > 3) {
      if (reason != nullptr) *reason = "fan speed out of range";
      return false;
    }
    if (state.target_humidity != 0 &&
        (state.target_humidity < HUMIDITY_MIN || state.target_humidity > HUMIDITY_MAX)) {
      if (reason != nullptr) *reason = "target humidity out of range";
      return false;
    }
    return true;
  }

  virtual void on_invalid_state(const char *reason) {
    ESP_LOGW("ehu.listener", "Ignoring invalid state frame: %s", reason != nullptr ? reason : "unknown reason");
  }
};

using EHUApiBase = rka_api::RKAApi<EHUVPort>;

class EHUApi : public EHUApiBase {
 public:
  explicit EHUApi(EHUVPort *vport) : EHUApiBase(vport) {}

  void set_power(bool power) const;
  void set_preset(uint8_t preset) const;
  void set_speed(uint8_t speed) const;
  void set_humidity(uint8_t humidity) const;

  void set_ionizer(bool lock) const;
  void set_lock(bool lock) const;
  void set_mute(bool mute) const;

  void set_clock(uint8_t hours, uint8_t minutes) const;
  void set_led_preset(uint8_t led_preset) const;
};

}  // namespace ehu
}  // namespace esphome

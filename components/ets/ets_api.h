#pragma once

#include "../rka_api/rka_api.h"
#include "vport/ets_vport.h"
#include "ets_data.h"

namespace esphome {
namespace ets {

class ETSListener : public rka_api::RKAListener<ets_state_t> {
 public:
  void on_frame(const rka_api::rka_any_frame_t &frame, size_t size) override {
    if (frame.type == rka_api::PACKET_RSP_SET_COMMAND || frame.type == rka_api::PACKET_RSP_STATE) {
      if (!this->check_packet_size_(size, sizeof(ets_state_t))) {
        this->on_invalid_state("invalid packet size");
        return;
      }
      const auto &state = *reinterpret_cast<const ets_state_t *>(frame.data);
      const char *reason = nullptr;
      if (!validate_state(state, &reason)) {
        this->on_invalid_state(reason != nullptr ? reason : "invalid state");
        return;
      }
      this->on_state(state);
      return;
    }
    rka_api::RKAListener<ets_state_t>::on_frame(frame, size);
  }

  static bool validate_state(const ets_state_t &state, const char **reason = nullptr) {
    if (state.state_ > 1) {
      if (reason != nullptr) *reason = "power state out of range";
      return false;
    }
    if (state.ctl_type > CT_FLOOR_AIR) {
      if (reason != nullptr) *reason = "control type out of range";
      return false;
    }
    if (state.sens_type > ST_EBERIE_33KOHM) {
      if (reason != nullptr) *reason = "sensor type out of range";
      return false;
    }
    return true;
  }

  virtual void on_invalid_state(const char *reason) {
    ESP_LOGW("ets.listener", "Ignoring invalid state frame: %s", reason != nullptr ? reason : "unknown reason");
  }
};

using ETSApiBase = rka_api::RKAApi<ETSVPort>;

class ETSApi : public ETSApiBase {
 public:
  explicit ETSApi(ETSVPort *vport) : ETSApiBase(vport) {}

  void init_unk0C(uint16_t unk0C) { this->unk0C_ = unk0C; }
  bool is_initialized() const { return this->unk0C_ != 0; }
  void set_mode(bool *state, float target_temp, float air_temp) const;

 protected:
  uint16_t unk0C_{};
};

}  // namespace ets
}  // namespace esphome

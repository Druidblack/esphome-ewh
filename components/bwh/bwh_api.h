#pragma once

#include "../rka_api/rka_api.h"
#include "vport/bwh_vport.h"
#include "bwh_data.h"

namespace esphome {
namespace bwh {

class BWHListener : public rka_api::RKAListener<bwh_state_t> {
 public:
  void on_frame(const rka_api::rka_any_frame_t &frame, size_t size) override {
    if (frame.type == rka_api::PACKET_CMD_STATE || frame.type == rka_api::PACKET_RSP_STATE) {
      if (!this->check_packet_size_(size, sizeof(bwh_state_t))) {
        this->on_invalid_state("invalid packet size");
        return;
      }
      const auto &state = *reinterpret_cast<const bwh_state_t *>(frame.data);
      const char *reason = nullptr;
      if (!validate_state(state, &reason)) {
        this->on_invalid_state(reason != nullptr ? reason : "invalid state");
        return;
      }
      this->on_state(state);
      return;
    }
    rka_api::RKAListener<bwh_state_t>::on_frame(frame, size);
  }

  static bool validate_state(const bwh_state_t &state, const char **reason = nullptr) {
    if (state.state > bwh_state_t::STATE_2000W) {
      if (reason != nullptr) *reason = "state code out of range";
      return false;
    }
    // Temperature and error fields are intentionally not rejected: some
    // firmware can expose transient values while booting.
    return true;
  }

  virtual void on_invalid_state(const char *reason) {
    ESP_LOGW("bwh.listener", "Ignoring invalid state frame: %s", reason != nullptr ? reason : "unknown reason");
  }
};

using BWHApiBase = rka_api::RKAApi<BWHVPort>;

class BWHApi : public BWHApiBase {
 public:
  explicit BWHApi(BWHVPort *vport) : BWHApiBase(vport) {}

  void set_mode(bwh_mode_t::Mode mode, uint8_t target_temperature) const;
  void set_mode(const bwh_mode_t &mode) const;
};

}  // namespace bwh
}  // namespace esphome

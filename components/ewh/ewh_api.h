#pragma once
#include "../rka_api/rka_api.h"
#include "vport/ewh_vport.h"
#include "ewh_data.h"

namespace esphome {
namespace ewh {

class EWHListener : public rka_api::RKAListener<ewh_state_t> {
 public:
  void on_frame(const rka_api::rka_any_frame_t &frame, size_t size) override {
    if (frame.type == rka_api::PACKET_CMD_STATE || frame.type == rka_api::PACKET_RSP_STATE) {
      if (!this->check_packet_size_(size, sizeof(ewh_state_t))) {
        this->on_invalid_state("invalid packet size");
        return;
      }
      const auto &state = *reinterpret_cast<const ewh_state_t *>(frame.data);
      const char *reason = nullptr;
      if (!validate_state(state, &reason)) {
        this->on_invalid_state(reason != nullptr ? reason : "invalid state");
        return;
      }
      this->on_state(state);
      return;
    }
    rka_api::RKAListener<ewh_state_t>::on_frame(frame, size);
  }

  static bool validate_state(const ewh_state_t &state, const char **reason = nullptr) {
    if (state.state > ewh_state_t::STATE_NO_FROST) {
      if (reason != nullptr) *reason = "state code out of range";
      return false;
    }
    if (state.bst.state > ewh_bst_t::STATE_ON) {
      if (reason != nullptr) *reason = "BST value out of range";
      return false;
    }
    // Temperature/clock/timer fields are intentionally not rejected here:
    // some firmware revisions can expose transient/non-user values at boot.
    return true;
  }

  virtual void on_invalid_state(const char *reason) {
    ESP_LOGW("ewh.listener", "Ignoring invalid state frame: %s", reason != nullptr ? reason : "unknown reason");
  }
};

using EWHApiBase = rka_api::RKAApi<EWHVPort>;
class EWHApi : public EWHApiBase {
 public:
  explicit EWHApi(EWHVPort *vport) : EWHApiBase(vport) {}
  void set_mode(ewh_mode_t::Mode mode, uint8_t target_temperature) const;
  void set_clock(uint8_t hours, uint8_t minutes) const;
  void set_timer(uint8_t hours, uint8_t minutes, uint8_t temperature,
                 ewh_mode_t::Mode mode = ewh_mode_t::MODE_700W) const;
  void set_bst(bool value) const;
  void set_clock(const ewh_clock_t &clock) const;
  void set_timer(const ewh_timer_t &timer) const;
  void set_mode(const ewh_mode_t &mode) const;
  void set_bst(const ewh_bst_t &bst) const;

  uint32_t get_rx_frames() const { return this->vport_->get_rx_frames(); }
  uint32_t get_tx_frames() const { return this->vport_->get_tx_frames(); }
  uint32_t get_crc_errors() const { return this->vport_->get_crc_errors(); }
  uint32_t get_frame_timeouts() const { return this->vport_->get_frame_timeouts(); }
  uint32_t get_parser_invalid_frames() const { return this->vport_->get_parser_invalid_frames(); }
  uint32_t get_queue_overflows() const { return this->vport_->get_queue_overflows(); }
  size_t get_queue_size() const { return this->vport_->get_queue_size(); }
  size_t get_queue_capacity() const { return this->vport_->get_queue_capacity(); }
};

}  // namespace ewh
}  // namespace esphome

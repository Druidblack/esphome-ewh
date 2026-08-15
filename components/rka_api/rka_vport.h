#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/vport/vport_uart.h"
#include "rka_data.h"
#include "rka_io.h"

namespace esphome {
namespace rka_api {

using RKAVPort = vport::VPort<rka_any_frame_t>;

template<class io_t> using RKAVPortUARTComponent = vport::VPortUARTComponent<io_t, rka_any_frame_t>;

// Shared reliability wrapper for every RKA UART device (EWH/BWH/ETS/EHU).
// It keeps low-priority polling requests from flooding the queue, exposes
// transport counters and makes queue overflow visible instead of silent.
template<class io_t> class StableRKAVPortUARTComponent : public RKAVPortUARTComponent<io_t> {
  using Base = RKAVPortUARTComponent<io_t>;

 public:
  explicit StableRKAVPortUARTComponent(io_t *io) : Base(io) {}

  void write(const rka_any_frame_t &frame, size_t size) override {
    const uint32_t now = millis();
    if (frame.type == PACKET_REQ_STATE) {
      if (this->has_state_request_time_ &&
          static_cast<uint32_t>(now - this->last_state_request_ms_) < LOW_PRIORITY_DEDUP_MS) {
        ESP_LOGV("rka.vport", "Suppressing duplicate state request");
        return;
      }
      this->has_state_request_time_ = true;
      this->last_state_request_ms_ = now;
    } else if (frame.type == PACKET_REQ_DEV_TYPE) {
      if (this->has_dev_type_request_time_ &&
          static_cast<uint32_t>(now - this->last_dev_type_request_ms_) < LOW_PRIORITY_DEDUP_MS) {
        ESP_LOGV("rka.vport", "Suppressing duplicate device-type request");
        return;
      }
      this->has_dev_type_request_time_ = true;
      this->last_dev_type_request_ms_ = now;
    }

    if (this->awaited_.size() >= this->awaited_.capacity()) {
      this->queue_overflows_++;
      if (frame.type == PACKET_REQ_STATE || frame.type == PACKET_REQ_DEV_TYPE) {
        ESP_LOGW("rka.vport", "Command queue full (%zu), dropping low-priority request %02X",
                 this->awaited_.capacity(), frame.type);
        return;
      }
      // The upstream VPort queue preserves the newest command by replacing
      // the oldest entry when full. Keep that behavior for control commands,
      // but report it so the condition is diagnosable.
      ESP_LOGW("rka.vport", "Command queue full (%zu), preserving newest control command",
               this->awaited_.capacity());
    }
    Base::write(frame, size);
  }

  uint32_t get_rx_frames() const { return this->io_->get_rx_frames(); }
  uint32_t get_tx_frames() const { return this->io_->get_tx_frames(); }
  uint32_t get_crc_errors() const { return this->io_->get_crc_errors(); }
  uint32_t get_frame_timeouts() const { return this->io_->get_frame_timeouts(); }
  uint32_t get_parser_invalid_frames() const { return this->io_->get_invalid_frames(); }
  uint32_t get_queue_overflows() const { return this->queue_overflows_; }
  size_t get_queue_size() const { return this->awaited_.size(); }
  size_t get_queue_capacity() const { return this->awaited_.capacity(); }

 protected:
  static constexpr uint32_t LOW_PRIORITY_DEDUP_MS = 100;
  uint32_t queue_overflows_{};
  bool has_state_request_time_{};
  bool has_dev_type_request_time_{};
  uint32_t last_state_request_ms_{};
  uint32_t last_dev_type_request_ms_{};
};

using RKAVPortListener = vport::VPortListener<rka_any_frame_t>;

}  // namespace rka_api
}  // namespace esphome

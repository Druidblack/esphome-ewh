#pragma once
#include <functional>
#include "esphome/core/log.h"
#include "esphome/components/uart/uart_component.h"
#include "uart_frame_io.h"

namespace esphome {
namespace rka_api {

template<class data_type> struct rka_frame_t {
  uint8_t type;
  data_type data;
} PACKED;
using rka_any_frame_t = rka_frame_t<uint8_t[0]>;

template<size_t max_frame_size_v> class RKAUartIO {
  using io_t = UartFrameIO<sizeof(rka_frame_t<uint8_t[max_frame_size_v]>)>;
 public:
  explicit RKAUartIO(uart::UARTComponent *uart) : uart_(uart) {
    this->io_.set_reader([this](const void *data, size_t size) { this->on_frame_(data, size); });
  }
  void poll() { this->io_.read(this->uart_); }
  using on_frame_type = std::function<void(const rka_any_frame_t &data, size_t size)>;
  void set_on_frame(on_frame_type &&reader) { this->reader_ = std::move(reader); }
  void write(const rka_any_frame_t &data, size_t size) {
    this->io_.write(this->uart_, reinterpret_cast<const uint8_t *>(&data), size);
  }
  void set_inter_byte_timeout(uint32_t timeout_ms) { this->io_.set_inter_byte_timeout(timeout_ms); }
  uint32_t get_rx_frames() const { return this->io_.get_rx_frames(); }
  uint32_t get_tx_frames() const { return this->io_.get_tx_frames(); }
  uint32_t get_crc_errors() const { return this->io_.get_crc_errors(); }
  uint32_t get_frame_timeouts() const { return this->io_.get_frame_timeouts(); }
  uint32_t get_invalid_frames() const { return this->io_.get_invalid_frames(); }
 protected:
  io_t io_;
  uart::UARTComponent *uart_;
  on_frame_type reader_;
  void on_frame_(const void *data, size_t size) {
    if (this->reader_) this->reader_(*static_cast<const rka_any_frame_t *>(data), size);
  }
};

}  // namespace rka_api
}  // namespace esphome

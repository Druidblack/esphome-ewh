#pragma once

#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <functional>

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

#ifndef RKA_DUMP
#define RKA_DUMP ESP_LOGV
#ifdef ESPHOME_LOG_HAS_VERBOSE
#define RKA_DO_DUMP_TX 1
#endif
#else
#define RKA_DO_DUMP_TX 1
#endif

namespace esphome {
namespace rka_api {

// max_frame_size_value - size of frame data without magic, size and crc
template<size_t max_frame_size_value, uint8_t magic_value = 0xAA, typename size_type = uint8_t,
         typename crc_type = uint8_t>
class UartFrameIO {
  static_assert(std::is_integral<size_type>::value);
  static_assert(std::is_integral<crc_type>::value);

  using magic_type = uint8_t;
  constexpr static const char *const TAG = "uart_frame_io";

 public:
  struct rx_frame_hdr_t {
    magic_type magic;
    size_type size;
  } PACKED;
  struct rx_frame_t : rx_frame_hdr_t {
    uint8_t data[sizeof(crc_type)];
    crc_type crc() const { return *reinterpret_cast<const crc_type *>(&this->data[this->size]); }
    bool is_valid() const { return this->magic == magic_value; }
    bool is_valid_size(size_t whole_frame_size) const {
      return whole_frame_size >= sizeof(rx_frame_t) && whole_frame_size - this->size - sizeof(rx_frame_t) == 0;
    }
  } PACKED;

  template<class U> void read(U *uart) {
    auto frame = this->rx_.frame();

    // Do not let a truncated frame poison the parser indefinitely.
    if (this->rx_in_progress_() &&
        static_cast<uint32_t>(millis() - this->last_rx_byte_ms_) > this->inter_byte_timeout_ms_) {
      ESP_LOGW(TAG, "RX frame timeout after %" PRIu32 " ms, resetting parser", this->inter_byte_timeout_ms_);
      this->frame_timeouts_++;
      this->rx_.reset();
      this->last_rx_byte_ms_ = 0;
      frame = this->rx_.frame();
    }

    while (uart->available()) {
      if (frame->magic == 0) {
        if (uart->read_array(&frame->magic, sizeof(magic_type))) {
          this->note_rx_byte_();
          ESP_LOGVV(TAG, "Read magic: 0x%X", frame->magic);
          if (frame->magic != magic_value) {
            ESP_LOGW(TAG, "Not expected magic: 0x%X", frame->magic);
            this->invalid_frames_++;
            frame->magic = 0;
          }
        }
        continue;
      }
      if (frame->size == 0) {
        if (uart->read_array(&frame->size, sizeof(size_type))) {
          this->note_rx_byte_();
          ESP_LOGVV(TAG, "Read size: %zu", static_cast<size_t>(frame->size));
          if (frame->size == 0 || frame->size > max_frame_size_value) {
            ESP_LOGW(TAG, "Invalid frame size: %zu (allowed 1..%zu)", static_cast<size_t>(frame->size),
                     max_frame_size_value);
            this->invalid_frames_++;
            this->rx_.reset();
            this->last_rx_byte_ms_ = 0;
            frame = this->rx_.frame();
          }
        }
        continue;
      }
      if (this->rx_.size < frame->size) {
        if (uart->read_array(&frame->data[this->rx_.size], sizeof(uint8_t))) {
          this->note_rx_byte_();
          ESP_LOGVV(TAG, "Read data[%02u]: 0x%02X", this->rx_.size, frame->data[this->rx_.size] & 0xFF);
          this->rx_.size++;
        }
        continue;
      }
      if (this->rx_.size == frame->size) {
        if (uart->read_array(&frame->data[this->rx_.size], sizeof(crc_type))) {
          this->note_rx_byte_();
          ESP_LOGVV(TAG, "Read CRC: 0x%02X", frame->data[this->rx_.size] & 0xFF);
          if (this->check_crc(frame)) {
            this->rx_frames_++;
            RKA_DUMP(TAG, "RX: %s", format_hex_pretty(this->rx_.data, this->rx_.size + sizeof(rx_frame_t)).c_str());
            if (this->reader_) this->reader_(frame->data, frame->size);
          } else {
            this->crc_errors_++;
            ESP_LOGW(TAG, "Invalid CRC for frame %s",
                     format_hex_pretty(this->rx_.data, this->rx_.size + sizeof(rx_frame_t)).c_str());
          }
          this->rx_.reset();
          this->last_rx_byte_ms_ = 0;
        }
        esphome::yield();
        break;
      }
      ESP_LOGW(TAG, "Unhandled read operation");
      this->invalid_frames_++;
      this->rx_.reset();
      this->last_rx_byte_ms_ = 0;
      frame = this->rx_.frame();
    }
  }

  template<class U, class T> void write(U *uart, const T &data) {
    this->write(uart, reinterpret_cast<const uint8_t *>(&data), sizeof(data));
  }

  template<class U> void write(U *uart, const uint8_t *data, size_t size) {
    if (size > max_frame_size_value) {
      ESP_LOGE(TAG, "TX frame is too large: %zu > %zu", size, max_frame_size_value);
      this->invalid_frames_++;
      return;
    }
    rx_frame_hdr_t hdr{.magic = magic_value, .size = static_cast<size_type>(size)};
    auto crc = calc_crc(&hdr, sizeof(hdr));
    uart->write_array(reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr));
    if (size != 0) {
      crc = calc_crc(crc, data, size);
      uart->write_array(data, size);
    }
    uart->write_array(reinterpret_cast<uint8_t *>(&crc), sizeof(crc));
    uart->flush();
    this->tx_frames_++;
#if RKA_DO_DUMP_TX
    std::string s = format_hex_pretty(reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr));
    if (size != 0) {
      s += format_hex_pretty(data, size).c_str();
      auto pos = s.find('(');
      if (pos != std::string::npos) s.resize(pos - 1);
    }
    s += ' ';
    s += format_hex_pretty(reinterpret_cast<uint8_t *>(&crc), sizeof(crc));
    RKA_DUMP(TAG, "TX: %s (%zu)", s.c_str(), sizeof(rx_frame_t) + size);
#endif
  }

  using reader_type = std::function<void(const void *data, size_t size)>;
  void set_reader(reader_type &&reader) { this->reader_ = std::move(reader); }
  void set_inter_byte_timeout(uint32_t timeout_ms) { this->inter_byte_timeout_ms_ = timeout_ms; }
  uint32_t get_rx_frames() const { return this->rx_frames_; }
  uint32_t get_tx_frames() const { return this->tx_frames_; }
  uint32_t get_crc_errors() const { return this->crc_errors_; }
  uint32_t get_frame_timeouts() const { return this->frame_timeouts_; }
  uint32_t get_invalid_frames() const { return this->invalid_frames_; }

  static crc_type calc_crc(crc_type init, const void *data, size_t size) {
    auto data8 = static_cast<const uint8_t *>(data);
    while (size--) init += *data8++;
    return init;
  }
  static crc_type calc_crc(const void *data, size_t size) { return calc_crc(0, data, size); }
  bool check_crc(const rx_frame_t *frame) {
    return calc_crc(frame, frame->size + sizeof(rx_frame_hdr_t)) == frame->crc();
  }

 protected:
  reader_type reader_;
  uint32_t inter_byte_timeout_ms_{100};
  uint32_t last_rx_byte_ms_{};
  uint32_t rx_frames_{};
  uint32_t tx_frames_{};
  uint32_t crc_errors_{};
  uint32_t frame_timeouts_{};
  uint32_t invalid_frames_{};

  void note_rx_byte_() { this->last_rx_byte_ms_ = millis(); }
  bool rx_in_progress_() const {
    const auto *frame = reinterpret_cast<const rx_frame_t *>(this->rx_.data);
    return frame->magic != 0 || frame->size != 0 || this->rx_.size != 0;
  }

  struct {
    size_type size;
    uint8_t data[max_frame_size_value + sizeof(rx_frame_t)];
    rx_frame_t *frame() { return reinterpret_cast<rx_frame_t *>(this->data); }
    void reset() {
      std::memset(this->data, 0, sizeof(this->data));
      this->size = 0;
    }
  } rx_{};
};

}  // namespace rka_api
}  // namespace esphome

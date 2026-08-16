#pragma once
#include <cinttypes>
#include <cmath>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "rka_api.h"

namespace esphome {
namespace rka_api {
namespace internal { extern const char *const TAG_COMPONENT; }

template<uint16_t dev_type_v, class listener_t, class api_t, class component_t = Component>
class RKAComponent : public component_t, public listener_t {
 public:
  explicit RKAComponent(api_t *api) : api_(api) { this->api_->add_listener(this); }

  void setup() override {
    // Preserve setup behavior of Component/PollingComponent. This is
    // especially important for ETS/EHU which inherit PollingComponent.
    component_t::setup();
    this->started_ms_ = millis();
    this->defer([this] { this->api_->request_dev_type(); });
    this->set_interval("rka_communication_watchdog", WATCHDOG_INTERVAL_MS,
                       [this]() { this->communication_watchdog_(); });
  }

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void on_frame(const rka_any_frame_t &frame, size_t size) override {
    // A frame reaching this point has already passed the transport-level CRC
    // check. Keep this timestamp for low-level UART diagnostics only. State
    // communication is marked separately, after the device-specific listener
    // has validated packet size and semantic contents.
    this->last_valid_frame_ms_ = millis();
    this->has_valid_frame_ = true;
    listener_t::on_frame(frame, size);
  }

  void on_dev_type(const rka_dev_type_t &dev_type) override {
    bool valid = true;
    if (dev_type.unknown != 0) {
      ESP_LOGW(internal::TAG_COMPONENT, "rka_dev_type_t.unknown, actual %08" PRIX32 ", expected 0x00000000",
               dev_type.unknown);
      valid = false;
    }
    if (dev_type.type != dev_type_v) {
      ESP_LOGW(internal::TAG_COMPONENT, "rka_dev_type_t.type, actual %04X, expected %04X", dev_type.type, dev_type_v);
      valid = false;
    }
    if (!valid) {
      this->device_confirmed_ = false;
      this->dev_type_errors_++;
      this->status_set_warning();
      this->set_timeout("rka_dev_type_retry", DEV_TYPE_RETRY_MS, [this] { this->api_->request_dev_type(); });
      return;
    }

    const bool was_confirmed = this->device_confirmed_;
    this->device_confirmed_ = true;
    this->cancel_timeout("rka_dev_type_retry");
    if (!this->communication_warning_) this->status_clear_warning();
    if (!was_confirmed) this->defer([this] { this->api_->request_state(); });
  }

  void on_result(const rka_result_t &result) override {
    if (result.result != rka_result_t::RESULT_OK)
      ESP_LOGW(internal::TAG_COMPONENT, "rka_result_t.result, actual %02X, expected %02X", result.result,
               rka_result_t::RESULT_OK);
  }

  void on_error(const rka_error_t &error) override {
    switch (error.code) {
      case rka_error_t::CODE_BAD_CRC:
        ESP_LOGW(internal::TAG_COMPONENT, "Operation failed, invalid CRC"); break;
      case rka_error_t::CODE_BAD_COMMAND:
        ESP_LOGW(internal::TAG_COMPONENT, "Operation failed, invalid command"); break;
      default:
        ESP_LOGW(internal::TAG_COMPONENT, "Operation failed, code %02X", error.code); break;
    }
  }

  api_t *get_api() const { return this->api_; }
  bool is_device_confirmed() const { return this->device_confirmed_; }
  bool has_valid_frame() const { return this->has_valid_frame_; }
  bool has_state_frame() const { return this->has_state_frame_; }
  uint32_t get_last_valid_frame_ms() const { return this->last_valid_frame_ms_; }
  uint32_t get_last_state_frame_ms() const { return this->last_state_frame_ms_; }
  uint32_t get_dev_type_errors() const { return this->dev_type_errors_; }
  uint32_t get_recovery_attempts() const { return this->recovery_attempts_; }

  bool is_communication_ok() const {
    if (!this->device_confirmed_ || !this->has_state_frame_) return false;
    return static_cast<uint32_t>(millis() - this->last_state_frame_ms_) <= COMMUNICATION_TIMEOUT_MS;
  }

  float get_communication_age_seconds() const {
    if (!this->has_state_frame_) return NAN;
    return static_cast<float>(static_cast<uint32_t>(millis() - this->last_state_frame_ms_)) / 1000.0f;
  }

 protected:
  static constexpr uint32_t COMMUNICATION_TIMEOUT_MS = 45000;
  static constexpr uint32_t WATCHDOG_INTERVAL_MS = 5000;
  static constexpr uint32_t RECOVERY_REQUEST_INTERVAL_MS = 5000;
  static constexpr uint32_t DEV_TYPE_RETRY_MS = 3000;

  api_t *api_;
  bool device_confirmed_{};
  bool has_valid_frame_{};
  bool has_state_frame_{};
  bool communication_warning_{};
  uint32_t started_ms_{};
  uint32_t last_valid_frame_ms_{};
  uint32_t last_state_frame_ms_{};
  uint32_t last_recovery_request_ms_{};
  uint32_t recovery_attempts_{};
  uint32_t dev_type_errors_{};

  void mark_valid_state_frame_() {
    const uint32_t now = millis();
    this->last_state_frame_ms_ = now;
    this->has_state_frame_ = true;
    if (this->communication_warning_ && this->device_confirmed_) {
      ESP_LOGI(internal::TAG_COMPONENT, "Communication restored");
      this->communication_warning_ = false;
      this->recovery_attempts_ = 0;
      this->last_recovery_request_ms_ = 0;
      this->status_clear_warning();
    }
  }

  void communication_watchdog_() {
    const uint32_t now = millis();
    if (this->is_communication_ok()) return;

    const uint32_t age = this->has_state_frame_ ? static_cast<uint32_t>(now - this->last_state_frame_ms_)
                                                : static_cast<uint32_t>(now - this->started_ms_);
    if (age <= COMMUNICATION_TIMEOUT_MS) return;

    if (!this->communication_warning_) {
      ESP_LOGW(internal::TAG_COMPONENT, "No valid RKA state frames for %" PRIu32 " s, starting recovery",
               static_cast<uint32_t>(COMMUNICATION_TIMEOUT_MS / 1000U));
      this->communication_warning_ = true;
      this->status_set_warning();
    }

    if (this->last_recovery_request_ms_ != 0 &&
        static_cast<uint32_t>(now - this->last_recovery_request_ms_) < RECOVERY_REQUEST_INTERVAL_MS)
      return;

    this->last_recovery_request_ms_ = now;
    if (!this->device_confirmed_ || (this->recovery_attempts_ % 3) == 2) {
      ESP_LOGD(internal::TAG_COMPONENT, "Recovery: request device type");
      this->api_->request_dev_type();
    } else {
      ESP_LOGD(internal::TAG_COMPONENT, "Recovery: request state");
      this->api_->request_state();
    }
    this->recovery_attempts_++;
  }
};

}  // namespace rka_api
}  // namespace esphome

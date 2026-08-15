#pragma once
#include <cstdint>
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "../rka_api/rka_component.h"
#include "ewh_api.h"

namespace esphome {
namespace ewh {

#ifdef USE_SWITCH
#define LOG_EWH() LOG_SWITCH("  ", "BST Switch", this->bst_)
#else
#define LOG_EWH()
#endif

using EWHComponentBase = rka_api::RKAComponent<rka_api::rka_dev_type_t::EWH, EWHListener, EWHApi>;
class EWHComponent : public EWHComponentBase {
 public:
#ifdef USE_SWITCH
  class BSTSwitch : public switch_::Switch {
   public:
    explicit BSTSwitch(EWHApi *api) : api_(api) {}
    void write_state(bool state) override { this->api_->set_bst(state); }
   protected:
    EWHApi *api_;
  };
#endif
  explicit EWHComponent(EWHApi *api) : EWHComponentBase(api) {}
  void setup() override;
  void dump_config() override;
#ifdef USE_TIME
  void set_time_id(esphome::time::RealTimeClock *time) { this->time_ = time; }
#endif
#ifdef USE_SWITCH
  void set_bst(switch_::Switch *bst) { this->bst_ = bst; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_communication(binary_sensor::BinarySensor *sensor) { this->communication_ = sensor; }
#endif
#ifdef USE_SENSOR
  void set_error_code(sensor::Sensor *sensor) { this->error_code_ = sensor; }
  void set_communication_age(sensor::Sensor *sensor) { this->communication_age_ = sensor; }
  void set_rx_frames(sensor::Sensor *sensor) { this->rx_frames_ = sensor; }
  void set_tx_frames(sensor::Sensor *sensor) { this->tx_frames_ = sensor; }
  void set_crc_errors(sensor::Sensor *sensor) { this->crc_errors_ = sensor; }
  void set_frame_timeouts(sensor::Sensor *sensor) { this->frame_timeouts_ = sensor; }
  void set_invalid_frames(sensor::Sensor *sensor) { this->invalid_frames_ = sensor; }
  void set_command_retries(sensor::Sensor *sensor) { this->command_retries_sensor_ = sensor; }
  void set_command_failures(sensor::Sensor *sensor) { this->command_failures_sensor_ = sensor; }
  void set_queue_overflows(sensor::Sensor *sensor) { this->queue_overflows_ = sensor; }
#endif
  void on_state(const ewh_state_t &state) override;
  void on_invalid_state(const char *reason) override;
  uint32_t get_command_retries() const { return this->command_retries_count_; }
  uint32_t get_command_failures() const { return this->command_failures_count_; }
  uint32_t get_semantic_invalid_frames() const { return this->semantic_invalid_frames_; }
 protected:
  static constexpr uint32_t DIAGNOSTICS_INTERVAL_MS = 5000;
  static constexpr uint32_t CLOCK_SYNC_MIN_INTERVAL_MS = 10UL * 60UL * 1000UL;
  static constexpr uint32_t CLOCK_SYNC_FORCE_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;

  uint32_t semantic_invalid_frames_{};
  uint32_t command_retries_count_{};
  uint32_t command_failures_count_{};
#ifdef USE_SWITCH
  switch_::Switch *bst_{};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *communication_{};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *error_code_{};
  sensor::Sensor *communication_age_{};
  sensor::Sensor *rx_frames_{};
  sensor::Sensor *tx_frames_{};
  sensor::Sensor *crc_errors_{};
  sensor::Sensor *frame_timeouts_{};
  sensor::Sensor *invalid_frames_{};
  sensor::Sensor *command_retries_sensor_{};
  sensor::Sensor *command_failures_sensor_{};
  sensor::Sensor *queue_overflows_{};
#endif
#ifdef USE_TIME
  esphome::time::RealTimeClock *time_{};
  bool clock_synced_once_{};
  uint32_t last_clock_sync_ms_{};
#endif
  void publish_diagnostics_();
  void note_command_retry_();
  void note_command_failure_();
#ifdef USE_TIME
  void sync_clock_if_needed_(const ewh_state_t &state);
#endif
};

}  // namespace ewh
}  // namespace esphome

#pragma once

#include <cmath>
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "../rka_api/rka_component.h"
#include "ets_api.h"

namespace esphome {
namespace ets {

using ETSComponentBase = rka_api::RKAComponent<rka_api::rka_dev_type_t::ETS, ETSListener, ETSApi, PollingComponent>;

class ETSComponent : public ETSComponentBase {
 public:
  explicit ETSComponent(ETSApi *api) : ETSComponentBase(api) {}

  void setup() override;
  void update() override { this->api_->request_state(); }
  void on_invalid_state(const char *reason) override;

  void set_floor_temp(sensor::Sensor *floor_temp) { this->floor_temp_ = floor_temp; }
#ifdef USE_BINARY_SENSOR
  void set_communication(binary_sensor::BinarySensor *sensor) { this->communication_ = sensor; }
#endif
  void set_communication_age(sensor::Sensor *sensor) { this->communication_age_ = sensor; }
  void set_rx_frames(sensor::Sensor *sensor) { this->rx_frames_ = sensor; }
  void set_tx_frames(sensor::Sensor *sensor) { this->tx_frames_ = sensor; }
  void set_crc_errors(sensor::Sensor *sensor) { this->crc_errors_ = sensor; }
  void set_frame_timeouts(sensor::Sensor *sensor) { this->frame_timeouts_ = sensor; }
  void set_invalid_frames(sensor::Sensor *sensor) { this->invalid_frames_ = sensor; }
  void set_command_retries(sensor::Sensor *sensor) { this->command_retries_sensor_ = sensor; }
  void set_command_failures(sensor::Sensor *sensor) { this->command_failures_sensor_ = sensor; }
  void set_queue_overflows(sensor::Sensor *sensor) { this->queue_overflows_ = sensor; }

  void note_command_retry();
  void note_command_failure();

 protected:
  static constexpr uint32_t DIAGNOSTICS_INTERVAL_MS = 5000;
  void dump_config_(const char *TAG) const;
  void publish_diagnostics_();

  sensor::Sensor *floor_temp_{};
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *communication_{};
#endif
  sensor::Sensor *communication_age_{};
  sensor::Sensor *rx_frames_{};
  sensor::Sensor *tx_frames_{};
  sensor::Sensor *crc_errors_{};
  sensor::Sensor *frame_timeouts_{};
  sensor::Sensor *invalid_frames_{};
  sensor::Sensor *command_retries_sensor_{};
  sensor::Sensor *command_failures_sensor_{};
  sensor::Sensor *queue_overflows_{};
  uint32_t semantic_invalid_frames_{};
  uint32_t command_retries_count_{};
  uint32_t command_failures_count_{};
};

}  // namespace ets
}  // namespace esphome

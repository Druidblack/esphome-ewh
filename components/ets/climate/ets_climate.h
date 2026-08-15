#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "../ets_component.h"

namespace esphome {
namespace ets {

using namespace esphome::climate;

class ETSClimate : public ETSComponent, public Climate {
 public:
  explicit ETSClimate(ETSApi *api) : ETSComponent(api) {}

  void dump_config() override;
  ClimateTraits traits() override;
  void control(const ClimateCall &call) override;
  void on_state(const ets_state_t &state) override;

 protected:
  static constexpr uint8_t MAX_COMMAND_RETRIES = 1;
  static constexpr uint32_t VERIFY_REQUEST_DELAY_MS = 250;
  static constexpr uint32_t VERIFY_RESPONSE_TIMEOUT_MS = 1000;

  void start_pending_command_(bool enabled, float target_temperature);
  void send_pending_command_();
  bool state_matches_pending_(const ets_state_t &state) const;
  void retry_or_fail_pending_(const char *reason);
  void clear_pending_command_();

  bool pending_command_{};
  bool verification_requested_{};
  bool verification_mismatch_seen_{};
  bool pending_enabled_{};
  float pending_target_temperature_{NAN};
  uint8_t pending_retries_{};
};

}  // namespace ets
}  // namespace esphome

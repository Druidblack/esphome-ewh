#pragma once

#include <string>
#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "../bwh_component.h"

namespace esphome {
namespace bwh {

using namespace esphome::climate;

class BWHClimate : public BWHComponent, public Climate {
 public:
  explicit BWHClimate(BWHApi *api) : BWHComponent(api) {}

  void setup() override;
  void dump_config() override;
  ClimateTraits traits() override;
  void control(const ClimateCall &call) override;
  void on_state(const bwh_state_t &state) override;

 protected:
  static constexpr uint8_t MAX_COMMAND_RETRIES = 2;
  static constexpr uint32_t VERIFY_REQUEST_DELAY_MS = 250;
  static constexpr uint32_t VERIFY_RESPONSE_TIMEOUT_MS = 1000;

  bwh_mode_t::Mode to_wh_mode_(ClimateMode mode, const std::string &preset) const;
  bool state_matches_pending_(const bwh_state_t &state) const;
  void start_pending_command_(bwh_mode_t::Mode mode, uint8_t temperature);
  void send_pending_command_();
  void retry_or_fail_pending_(const char *reason);
  void clear_pending_command_();

  bool pending_command_{};
  bool verification_requested_{};
  bool verification_mismatch_seen_{};
  bwh_mode_t::Mode pending_mode_{bwh_mode_t::MODE_OFF};
  uint8_t pending_temperature_{};
  uint8_t pending_retries_{};
};

}  // namespace bwh
}  // namespace esphome

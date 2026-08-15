#pragma once

#include "../../rka_api/rka_vport.h"
#include "../bwh_data.h"

namespace esphome {
namespace bwh {

struct bwh_config {
  static constexpr auto max_data_size = std::max(sizeof(bwh_state_t), esphome::rka_api::rka_max_data_size_t::value);
};

using BWHUartIO = rka_api::RKAUartIO<bwh_config::max_data_size>;
using BWHVPortBase = rka_api::StableRKAVPortUARTComponent<BWHUartIO>;

class BWHVPort : public BWHVPortBase {
 public:
  explicit BWHVPort(BWHUartIO *io) : BWHVPortBase(io) {}
  void dump_config() override;
};

}  // namespace bwh
}  // namespace esphome

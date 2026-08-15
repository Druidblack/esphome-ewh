#pragma once

#include "../../rka_api/rka_vport.h"
#include "../ewh_data.h"

namespace esphome {
namespace ewh {

struct ewh_config {
  static constexpr auto max_data_size = std::max(sizeof(ewh_state_t), esphome::rka_api::rka_max_data_size_t::value);
};

using EWHUartIO = rka_api::RKAUartIO<ewh_config::max_data_size>;
using EWHVPortBase = rka_api::StableRKAVPortUARTComponent<EWHUartIO>;

class EWHVPort : public EWHVPortBase {
 public:
  explicit EWHVPort(EWHUartIO *io) : EWHVPortBase(io) {}
  void dump_config() override;
};

}  // namespace ewh
}  // namespace esphome

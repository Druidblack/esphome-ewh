#pragma once

#include "../../rka_api/rka_vport.h"
#include "../ets_data.h"

namespace esphome {
namespace ets {

struct ets_config {
  static constexpr auto max_data_size =
      std::max(sizeof(ets_state_t), std::max(sizeof(ets_mode_t), esphome::rka_api::rka_max_data_size_t::value));
};

using ETSUartIO = rka_api::RKAUartIO<ets_config::max_data_size>;
using ETSVPortBase = rka_api::StableRKAVPortUARTComponent<ETSUartIO>;

class ETSVPort : public ETSVPortBase {
 public:
  explicit ETSVPort(ETSUartIO *io) : ETSVPortBase(io) {}
  void dump_config() override;
};

}  // namespace ets
}  // namespace esphome

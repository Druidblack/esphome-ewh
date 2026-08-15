#pragma once

#include "../../rka_api/rka_vport.h"
#include "../ehu_data.h"

namespace esphome {
namespace ehu {

struct ehu_config {
  static constexpr auto max_data_size = std::max(sizeof(ehu_state_t), esphome::rka_api::rka_max_data_size_t::value);
};

using EHUUartIO = rka_api::RKAUartIO<ehu_config::max_data_size>;
using EHUVPortBase = rka_api::StableRKAVPortUARTComponent<EHUUartIO>;

class EHUVPort : public EHUVPortBase {
 public:
  explicit EHUVPort(EHUUartIO *io) : EHUVPortBase(io) {}
  void dump_config() override;
};

}  // namespace ehu
}  // namespace esphome

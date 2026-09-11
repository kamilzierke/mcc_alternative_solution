#pragma once

#include <cstdint>

namespace mccfw {

struct BqIdentityProbe {
  const char *slot;
  uint8_t tca_address;
  uint8_t tca_channel_mask;
  uint8_t bq_address;
  uint8_t register_address;
};

constexpr BqIdentityProbe kBqC01IdentityProbe = {
    "C01",
    0x70,
    0x01,
    0x6B,
    0x0A,
};

constexpr uint8_t kBqReadableFirstRegister = 0x00;
constexpr uint8_t kBqReadableLastRegister = 0x0A;

inline bool bq_register_is_in_snapshot_scope(uint8_t register_address) {
  return register_address >= kBqReadableFirstRegister && register_address <= kBqReadableLastRegister;
}

inline bool tca_channel_mask_is_single_channel(uint8_t mask) {
  return mask != 0 && (mask & static_cast<uint8_t>(mask - 1)) == 0;
}

}  // namespace mccfw
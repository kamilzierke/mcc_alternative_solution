#pragma once

#include <cstddef>
#include <cstdint>

namespace mccfw {

struct I2cProbeTarget {
  uint8_t address;
  const char *component;
};

constexpr I2cProbeTarget kMainBusProbeTargets[] = {
    {0x27, "PCF8574 0x27"},
    {0x3C, "SSD1306 0x3C"},
    {0x70, "TCA9548A 0x70"},
    {0x71, "TCA9548A 0x71"},
};

constexpr size_t kMainBusProbeTargetCount = sizeof(kMainBusProbeTargets) / sizeof(kMainBusProbeTargets[0]);

constexpr I2cProbeTarget kTcaControlRegisterProbeTargets[] = {
  {0x70, "TCA9548A 0x70"},
  {0x71, "TCA9548A 0x71"},
};

constexpr size_t kTcaControlRegisterProbeTargetCount = sizeof(kTcaControlRegisterProbeTargets) / sizeof(kTcaControlRegisterProbeTargets[0]);

inline bool main_bus_probe_acknowledged(uint8_t wire_result) {
  return wire_result == 0;
}

inline bool tca_control_register_is_idle(uint8_t value) {
  return value == 0x00;
}

inline const I2cProbeTarget *main_bus_probe_target_for_address(uint8_t address) {
  for (size_t index = 0; index < kMainBusProbeTargetCount; index++) {
    if (kMainBusProbeTargets[index].address == address) return &kMainBusProbeTargets[index];
  }
  return nullptr;
}

}  // namespace mccfw
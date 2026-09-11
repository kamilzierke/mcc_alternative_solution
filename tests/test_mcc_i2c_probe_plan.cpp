#include "mcc_i2c_probe_plan.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

void expect(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  using namespace mccfw;

  expect(kMainBusProbeTargetCount == 4, "probe plan contains four approved main-bus targets");
  const uint8_t expectedAddresses[] = {0x27, 0x3C, 0x70, 0x71};
  const char *expectedComponents[] = {"PCF8574 0x27", "SSD1306 0x3C", "TCA9548A 0x70", "TCA9548A 0x71"};
  for (size_t index = 0; index < kMainBusProbeTargetCount; index++) {
    expect(kMainBusProbeTargets[index].address == expectedAddresses[index], "probe target address");
    expect(std::strcmp(kMainBusProbeTargets[index].component, expectedComponents[index]) == 0, "probe target component");
    expect(main_bus_probe_target_for_address(expectedAddresses[index]) == &kMainBusProbeTargets[index], "probe target lookup");
  }
  expect(main_bus_probe_target_for_address(0x41) == nullptr, "unapproved INA219 address excluded");
  expect(main_bus_probe_target_for_address(0x4F) == nullptr, "conflicted address excluded");
  expect(main_bus_probe_target_for_address(0x6B) == nullptr, "downstream BQ address excluded");
  expect(main_bus_probe_acknowledged(0), "Wire success is acknowledged");
  expect(!main_bus_probe_acknowledged(1), "Wire buffer overflow is not acknowledged");
  expect(!main_bus_probe_acknowledged(2), "Wire address NACK is not acknowledged");
  expect(!main_bus_probe_acknowledged(3), "Wire data NACK is not acknowledged");
  expect(!main_bus_probe_acknowledged(4), "Wire bus error is not acknowledged");
  expect(kTcaControlRegisterProbeTargetCount == 2, "TCA state probe has two approved targets");
  expect(kTcaControlRegisterProbeTargets[0].address == 0x70, "first TCA state target");
  expect(kTcaControlRegisterProbeTargets[1].address == 0x71, "second TCA state target");
  expect(tca_control_register_is_idle(0x00), "TCA idle selection is zero");
  expect(!tca_control_register_is_idle(0x01), "TCA channel zero selection is not idle");
  expect(!tca_control_register_is_idle(0x80), "TCA channel seven selection is not idle");
  std::cout << "MCC I2C probe plan tests passed.\n";
  return 0;
}
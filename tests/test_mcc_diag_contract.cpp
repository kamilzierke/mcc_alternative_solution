#include "mcc_diag_contract.h"

#include <cmath>
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

void expect_close(float actual, float expected, const char *message) {
  expect(std::fabs(actual - expected) < 0.0001f, message);
}

void expect_text(const char *actual, const char *expected, const char *message) {
  expect(std::strcmp(actual, expected) == 0, message);
}

}  // namespace

int main() {
  using namespace mccdiag;

  expect_close(ina_bus_v_from_raw(0x0000), 0.0f, "INA bus voltage at zero");
  expect_close(ina_bus_v_from_raw(0x0008), 0.004f, "INA bus voltage LSB");
  expect_close(ina_bus_v_from_raw(0xFFFF), 32.764f, "INA bus voltage mask");
  expect_close(ina_shunt_mv_from_raw(0x0064), 1.0f, "INA shunt positive conversion");
  expect_close(ina_shunt_mv_from_raw(0xFF9C), -1.0f, "INA shunt signed conversion");
  expect_close(tc1047_temp_c_from_v(0.500f), 0.0f, "TC1047 zero Celsius conversion");
  expect_close(tc1047_temp_c_from_v(0.750f), 25.0f, "TC1047 room temperature conversion");
  expect_close(tc1047_temp_c_from_v(0.100f), -40.0f, "TC1047 low temperature conversion");
  expect_close(ina_current_ma_from_shunt_mv(1.2f, 60.0f), 20.0f, "INA current conversion");
  expect(std::isnan(ina_current_ma_from_shunt_mv(NAN, 60.0f)), "INA current rejects NaN");
  expect(std::isnan(ina_current_ma_from_shunt_mv(1.0f, 0.0f)), "INA current rejects zero shunt");

  const uint16_t iinlimMa[] = {100, 150, 500, 900, 1200, 1500, 2000, 3000};
  const char *iinlimText[] = {"100mA", "150mA", "500mA", "900mA", "1.2A", "1.5A", "2.0A", "3.0A"};
  for (uint8_t value = 0; value < 8; value++) {
    expect(bq_iinlim_ma(value) == iinlimMa[value], "BQ input-current decode");
    expect_text(bq_iinlim_str(value), iinlimText[value], "BQ input-current label");
  }

  expect(bq_ichg_ma(0x00) == 512, "BQ charge current minimum");
  expect(bq_ichg_ma(0x20) == 1024, "BQ charge current 1024mA");
  expect(bq_ichg_ma(0xFC) == 4544, "BQ charge current maximum register value");
  const char *chargeText[] = {"not-charging", "pre-charge", "fast-charge", "term-done"};
  const char *faultText[] = {"normal", "input-fault", "thermal-shutdown", "timer-fault"};
  const char *watchdogText[] = {"off", "40s", "80s", "160s"};
  for (uint8_t value = 0; value < 4; value++) {
    expect_text(bq_chrg_str(static_cast<uint8_t>(value << 4)), chargeText[value], "BQ charge-state label");
    expect_text(bq_fault_str(static_cast<uint8_t>(value << 4)), faultText[value], "BQ fault-state label");
    expect_text(bq_watchdog_str(static_cast<uint8_t>(value << 4)), watchdogText[value], "BQ watchdog label");
  }

  std::cout << "MCC diagnostic contract tests passed.\n";
  return 0;
}
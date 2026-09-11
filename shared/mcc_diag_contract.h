#pragma once

#include <cmath>
#include <cstdio>
#include <cstdint>

namespace mccdiag {

inline float ina_bus_v_from_raw(uint16_t raw) {
  return ((raw >> 3) & 0x1FFF) * 0.004f;
}

inline float ina_shunt_mv_from_raw(uint16_t raw) {
  return static_cast<int16_t>(raw) * 0.01f;
}

inline float tc1047_temp_c_from_v(float voltage_v) {
  return (voltage_v - 0.500f) * 100.0f;
}

inline float ina_current_ma_from_shunt_mv(float shunt_mv, float shunt_mohm) {
  if (std::isnan(shunt_mv) || shunt_mohm <= 0.0f) return NAN;
  return shunt_mv * 1000.0f / shunt_mohm;
}

inline const char *bq_chrg_str(uint8_t reg08) {
  switch ((reg08 >> 4) & 0x03) {
    case 0: return "not-charging";
    case 1: return "pre-charge";
    case 2: return "fast-charge";
    case 3: return "term-done";
  }
  return "?";
}

inline const char *bq_fault_str(uint8_t reg09) {
  switch ((reg09 >> 4) & 0x03) {
    case 0: return "normal";
    case 1: return "input-fault";
    case 2: return "thermal-shutdown";
    case 3: return "timer-fault";
  }
  return "?";
}

inline const char *bq_iinlim_str(uint8_t reg00) {
  switch (reg00 & 0x07) {
    case 0: return "100mA";
    case 1: return "150mA";
    case 2: return "500mA";
    case 3: return "900mA";
    case 4: return "1.2A";
    case 5: return "1.5A";
    case 6: return "2.0A";
    case 7: return "3.0A";
  }
  return "?";
}

inline uint16_t bq_iinlim_ma(uint8_t reg00) {
  switch (reg00 & 0x07) {
    case 0: return 100;
    case 1: return 150;
    case 2: return 500;
    case 3: return 900;
    case 4: return 1200;
    case 5: return 1500;
    case 6: return 2000;
    case 7: return 3000;
  }
  return 0;
}

inline uint16_t bq_ichg_ma(uint8_t reg02) {
  return static_cast<uint16_t>(512U + static_cast<uint16_t>(((reg02 >> 2) & 0x3FU) * 64U));
}

inline const char *bq_watchdog_str(uint8_t reg05) {
  switch ((reg05 >> 4) & 0x03) {
    case 0: return "off";
    case 1: return "40s";
    case 2: return "80s";
    case 3: return "160s";
  }
  return "?";
}

inline void bq_snapshot_summary(const uint8_t *registers, char *buffer, size_t buffer_size) {
  std::snprintf(buffer, buffer_size,
                "HIZ %s; IIN %s; CHG %s; OTG %s; ICHG %umA; WD %s; timer %s; %s; DPM %s; PG %s; VSYS %s; fault %s; part%u rev%u",
                (registers[0x00] & 0x80U) ? "on" : "off", bq_iinlim_str(registers[0x00]),
                (registers[0x01] & 0x10U) ? "on" : "off", (registers[0x01] & 0x20U) ? "on" : "off",
                bq_ichg_ma(registers[0x02]), bq_watchdog_str(registers[0x05]),
                (registers[0x05] & 0x08U) ? "on" : "off", bq_chrg_str(registers[0x08]),
                (registers[0x08] & 0x08U) ? "yes" : "no", (registers[0x08] & 0x04U) ? "yes" : "no",
                (registers[0x08] & 0x01U) ? "yes" : "no", bq_fault_str(registers[0x09]),
                (registers[0x0A] >> 3) & 0x07U, registers[0x0A] & 0x07U);
}

}  // namespace mccdiag
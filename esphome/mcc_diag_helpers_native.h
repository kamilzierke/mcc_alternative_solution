#pragma once

#include "esphome.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <string.h>

namespace mccdiag {

// MCC Pro native-ESPHome diagnostic helper.
// Native components used in YAML:
//   - tca9548a: exposes C01..C16 BQ24195 branches as virtual I2C buses
//   - pcf8574: PCF8574 exists in ESPHome, but this helper drives it by full-byte I2C writes
//     for HC4067 enables; do not use PCF8574 read-modify-write.
//   - i2c_device: BQ24195 devices and two INA219 measurement devices
// Remaining direct GPIO use: 74HC4067 address lines S0..S3 and ESP8266 A0.
// Confirmed HC4067 enables, one active-low PCF8574 output per mux:
//   - U10  HC4067: PCF8574 P0 LOW -> internal TC1047 x16 temperature mux
//   - U10E HC4067: PCF8574 P1 LOW -> external temperature x16 mux
//   - U34  HC4067: PCF8574 P2 LOW -> shunt mux C1-C16; internal INA requires U2 and U34 enabled together on the same channel
//   - U3   HC4067: PCF8574 P3 LOW -> external-cell voltage path into INA219 #1 @ 0x41
//   - U2   HC4067: PCF8574 P4 LOW -> internal-slot battery voltage path into INA219 #2 @ 0x4F

static constexpr uint8_t PIN_MUX_S0 = 13;
static constexpr uint8_t PIN_MUX_S1 = 12;
static constexpr uint8_t PIN_MUX_S2 = 14;
static constexpr uint8_t PIN_MUX_S3 = 16;

static constexpr uint8_t ADDR_PCF8574 = 0x27;
static constexpr uint8_t ADDR_TCA0 = 0x70;
static constexpr uint8_t ADDR_TCA1 = 0x71;
static constexpr uint8_t PCF_SAFE_IDLE = 0xFF;
static constexpr uint8_t PCF_U10_TC1047_MUX_ENABLE = 0xFE;  // 11111110, P0 LOW
static constexpr uint8_t PCF_U10E_EXTERNAL_TEMP_ENABLE = 0xFD;  // 11111101, P1 LOW
static constexpr uint8_t PCF_U34_MUX_ENABLE        = 0xFB;  // 11111011, P2 LOW; U34 shunt mux for internal INA219.
static constexpr uint8_t PCF_U3_EXTERNAL_INA_ENABLE    = 0xF7;  // 11110111, P3 LOW
static constexpr uint8_t PCF_U2_INTERNAL_INA_ENABLE    = 0xEF;  // 11101111, P4 LOW
static constexpr uint8_t PCF_U2_U34_INTERNAL_INA_ENABLE = 0xEB;  // 11101011, P2+P4 LOW: U34 shunt mux + U2 BAT+ mux for internal INA.
static constexpr uint8_t PCF_U2_U34_INTERNAL_C16_INA_ENABLE = PCF_U2_U34_INTERNAL_INA_ENABLE;  // Compatibility alias.

enum class Hc4067Mux : uint8_t {
  U10_TC1047 = 0,
  U10E = 1,
  U3_INA219 = 3,
  U2 = 4,
};

struct Hc4067AdcSnapshot {
  bool ok{false};
  uint16_t min_raw{0};
  uint16_t max_raw{0};
  float raw_avg{NAN};
  float v{NAN};
};

struct InaSnapshot {
  bool ok{false};
  bool reg_ok[6]{false, false, false, false, false, false};
  uint16_t reg[6]{0, 0, 0, 0, 0, 0};  // INA219 raw registers: 0=CONFIG, 1=SHUNT, 2=BUS, 3=POWER, 4=CURRENT, 5=CAL.
  uint16_t bus_raw{0};
  int16_t shunt_raw{0};
  float bus_v{NAN};
  float shunt_mv{NAN};
  float current_mA{NAN};
};

static constexpr uint8_t SLOT0_C16 = 15;
static constexpr uint8_t ADDR_BQ24195 = 0x6B;
static constexpr uint8_t ADDR_PCA9685 = 0x4F;  // WARNING: same numeric address as internal INA219 #2 if PCA9685 is really strapped to 0x4F.
static constexpr uint8_t PCA9685_CH_C16_Q34 = 15;  // U36 LED15 -> R226 -> Q34 gate on the provided C16 schematic fragment.
static constexpr uint8_t ADDR_INA219_EXTERNAL_U3 = 0x41;  // INA219 #1: U3 external-cell voltage path, A1=LOW A0=HIGH.
static constexpr uint8_t ADDR_INA219_INTERNAL_U2 = 0x4F;  // INA219 #2: U2 internal-slot voltage path, A1=SCL A0=SCL.

static const char *TAG_BQWEB = "mcc_bqweb";
static const char *TAG_BQFULL = "mcc_bqfull";
static const char *TAG_TEMP = "mcc_tc1047";

#ifndef MCC_TC1047_ADC_FULL_SCALE_V
#define MCC_TC1047_ADC_FULL_SCALE_V 3.02f
#endif

#ifndef MCC_TC1047_TEMP_OFFSET_C
#define MCC_TC1047_TEMP_OFFSET_C 0.0f
#endif

#ifndef MCC_EXTERNAL_TEMP_ADC_FULL_SCALE_V
#define MCC_EXTERNAL_TEMP_ADC_FULL_SCALE_V MCC_TC1047_ADC_FULL_SCALE_V
#endif

#ifndef MCC_EXTERNAL_TEMP_OFFSET_C
#define MCC_EXTERNAL_TEMP_OFFSET_C MCC_TC1047_TEMP_OFFSET_C
#endif

#ifndef MCC_INA219_INTERNAL_SHUNT_MOHM
#define MCC_INA219_INTERNAL_SHUNT_MOHM 60.0f
#endif

#ifndef MCC_INA219_EXTERNAL_SHUNT_MOHM
#define MCC_INA219_EXTERNAL_SHUNT_MOHM 60.0f
#endif

struct SlotSnapshot {
  bool bq_ok{false};
  uint8_t reg00{0};
  uint8_t reg08{0};
  uint8_t reg09a{0};
  uint8_t reg09b{0};
  uint8_t reg0a{0};

  // Legacy fields kept for compatibility with older UI/sensors. They mirror internal_ina.
  bool ina_ok{false};
  uint16_t ina_bus_raw{0};
  int16_t ina_shunt_raw{0};
  float bus_v{NAN};
  float shunt_mv{NAN};

  InaSnapshot internal_ina;  // U2 HC4067 -> INA219 #2 @ 0x4F.
  InaSnapshot external_ina;  // U3 HC4067 -> INA219 #1 @ 0x41.

  bool temp_ok{false};
  uint16_t temp_min_raw{0};
  uint16_t temp_max_raw{0};
  float temp_raw_avg{NAN};
  float temp_v{NAN};
  float temp_c{NAN};
  int a0_raw{-1};

  Hc4067AdcSnapshot u10e;  // external temperature raw voltage.
  bool external_temp_ok{false};
  float external_temp_c{NAN};
};

struct BqRoute {
  uint8_t tca_index;
  uint8_t tca_channel;
  const char *bus_id;
  const char *ref;
};

static constexpr BqRoute BQ_ROUTES[16] = {
  {0, 0, "tca0_ch0", "TCA0_C01_C08"},
  {0, 1, "tca0_ch1", "TCA0_C01_C08"},
  {0, 2, "tca0_ch2", "TCA0_C01_C08"},
  {0, 3, "tca0_ch3", "TCA0_C01_C08"},
  {0, 4, "tca0_ch4", "TCA0_C01_C08"},
  {0, 5, "tca0_ch5", "TCA0_C01_C08"},
  {0, 6, "tca0_ch6", "TCA0_C01_C08"},
  {0, 7, "tca0_ch7", "TCA0_C01_C08"},
  {1, 0, "tca1_ch0", "U38_C09_C16"},
  {1, 1, "tca1_ch1", "U38_C09_C16"},
  {1, 2, "tca1_ch2", "U38_C09_C16"},
  {1, 3, "tca1_ch3", "U38_C09_C16"},
  {1, 4, "tca1_ch4", "U38_C09_C16"},
  {1, 5, "tca1_ch5", "U38_C09_C16"},
  {1, 6, "tca1_ch6", "U38_C09_C16"},
  {1, 7, "tca1_ch7", "U38_C09_C16"},
};

struct BQFullRegs {
  bool ok{false};
  uint8_t r[11]{0};
};

static SlotSnapshot bq_cached_slots[16];
static BQFullRegs bq_cached_regs[16];
static bool bq_cached_valid[16] = {false};
static uint32_t bq_cached_read_ms[16] = {0};

static volatile uint16_t bq_web_pending_mask = 0;
static volatile uint16_t auto_poll_pending_mask = 0;  // Lightweight 10s polling: internal TC1047 + internal INA219 only.
static volatile bool bq_web_read_busy = false;
static volatile uint8_t bq_web_pending_slot0 = 0;
static constexpr uint8_t BQ_ACTION_NONE = 0;
static constexpr uint8_t BQ_ACTION_CHARGE_ON = 1;
static constexpr uint8_t BQ_ACTION_CHARGE_OFF = 2;
static constexpr uint8_t BQ_ACTION_WD_RESET = 3;
static constexpr uint8_t BQ_ACTION_IIN_500 = 10;
static constexpr uint8_t BQ_ACTION_IIN_900 = 11;
static constexpr uint8_t BQ_ACTION_IIN_1500 = 12;
static constexpr uint8_t BQ_ACTION_IIN_3000 = 13;
static constexpr uint8_t BQ_ACTION_ICHG_512 = 20;
static constexpr uint8_t BQ_ACTION_ICHG_1024 = 21;
static constexpr uint8_t BQ_ACTION_ICHG_2048 = 22;
static constexpr uint8_t BQ_ACTION_ICHG_3008 = 23;
static volatile uint8_t bq_web_pending_actions[16] = {0};
static volatile uint8_t bq_web_current_action = BQ_ACTION_NONE;
static constexpr uint8_t C16_HW_ACTION_NONE = 0;
static constexpr uint8_t C16_HW_ACTION_Q34_OFF = 1;
static constexpr uint8_t C16_HW_ACTION_Q34_1P = 2;
static constexpr uint8_t C16_HW_ACTION_Q34_5P = 3;
static constexpr uint8_t C16_HW_ACTION_Q34_25P = 4;
static constexpr uint8_t C16_HW_ACTION_Q34_100P = 5;
static constexpr uint8_t C16_HW_ACTION_Q34_PULSE_5P_1000MS = 6;
static constexpr uint8_t C16_HW_ACTION_Q35_PROBE = 20;
static constexpr uint8_t C16_HW_ACTION_HOLD_U2_INA = 30;
static constexpr uint8_t C16_HW_ACTION_HOLD_U3_INA = 31;
static constexpr uint8_t C16_HW_ACTION_HOLD_RELEASE = 32;
static volatile uint8_t c16_hw_pending_action = C16_HW_ACTION_NONE;
static volatile uint8_t c16_hw_current_action = C16_HW_ACTION_NONE;
static volatile bool c16_hc4067_hold_active = false;
static volatile uint8_t c16_hc4067_hold_mux = 0xFF;
static volatile uint8_t c16_hc4067_hold_channel = SLOT0_C16;


inline void feed() { yield(); }
inline const char *yesno(bool v) { return v ? "YES" : "NO"; }

inline const BqRoute &bq_route_for_slot(uint8_t slot0) {
  return BQ_ROUTES[slot0 & 0x0F];
}

inline uint8_t tca_addr_for_slot(uint8_t slot0) {
  return bq_route_for_slot(slot0).tca_index == 0 ? ADDR_TCA0 : ADDR_TCA1;
}

inline uint8_t tca_channel_for_slot(uint8_t slot0) {
  return bq_route_for_slot(slot0).tca_channel;
}

inline uint8_t tca_mask_for_slot(uint8_t slot0) {
  return (uint8_t) (1U << tca_channel_for_slot(slot0));
}

inline const char *tca_ref_for_slot(uint8_t slot0) {
  return bq_route_for_slot(slot0).ref;
}

inline const char *bus_id_for_slot(uint8_t slot0) {
  return bq_route_for_slot(slot0).bus_id;
}

inline esphome::i2c::I2CDevice &bq_dev_for_slot(uint8_t slot0) {
  switch (slot0 & 0x0F) {
    case 0: return id(bq_c01);
    case 1: return id(bq_c02);
    case 2: return id(bq_c03);
    case 3: return id(bq_c04);
    case 4: return id(bq_c05);
    case 5: return id(bq_c06);
    case 6: return id(bq_c07);
    case 7: return id(bq_c08);
    case 8: return id(bq_c09);
    case 9: return id(bq_c10);
    case 10: return id(bq_c11);
    case 11: return id(bq_c12);
    case 12: return id(bq_c13);
    case 13: return id(bq_c14);
    case 14: return id(bq_c15);
    default: return id(bq_c16);
  }
}

inline esphome::i2c::I2CDevice &ina_external_dev() {
  return id(ina_external_u3);
}

inline esphome::i2c::I2CDevice &ina_internal_dev() {
  return id(ina_internal_u2);
}

// U34 is an HC4067 mux for shunt paths, not a raw ADC diagnostic path and not an INA219.
// The two real INA219 paths used by current firmware are:
//   external path: U3 HC4067 -> INA219 #1 @ 0x41
//   internal path: U2 HC4067 plus U34 shunt mux for C16 -> INA219 #2 @ 0x4F

inline bool dev_read_reg8(esphome::i2c::I2CDevice &dev, uint8_t reg, uint8_t &value) {
  return dev.read_byte(reg, &value);
}

inline bool dev_write_reg8(esphome::i2c::I2CDevice &dev, uint8_t reg, uint8_t value) {
  return dev.write_byte(reg, value);
}

inline bool dev_read_reg16_be(esphome::i2c::I2CDevice &dev, uint8_t reg, uint16_t &value) {
  uint8_t data[2] = {0, 0};
  if (!dev.read_bytes(reg, data, 2)) return false;
  value = ((uint16_t) data[0] << 8) | data[1];
  return true;
}

inline bool dev_write_reg16_be(esphome::i2c::I2CDevice &dev, uint8_t reg, uint16_t value) {
  uint8_t data[2] = {
    static_cast<uint8_t>((value >> 8) & 0xFF),
    static_cast<uint8_t>(value & 0xFF),
  };
  return dev.write_bytes(reg, data, 2);
}

inline bool i2c_write_u8_wire(uint8_t addr, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

inline bool disable_tcas_on_main_bus() {
  const bool ok0 = i2c_write_u8_wire(ADDR_TCA0, 0x00);
  const bool ok1 = i2c_write_u8_wire(ADDR_TCA1, 0x00);
  delay(2);
  if (!(ok0 && ok1)) {
    ESP_LOGW(TAG_BQWEB, "TCA disable before main-bus analog mux read failed: TCA0=%s TCA1=%s",
             yesno(ok0), yesno(ok1));
  }
  return ok0 && ok1;
}

inline uint8_t pcf_mask_for_hc4067(Hc4067Mux mux) {
  switch (mux) {
    case Hc4067Mux::U10_TC1047:
      return PCF_U10_TC1047_MUX_ENABLE;
    case Hc4067Mux::U10E:
      return PCF_U10E_EXTERNAL_TEMP_ENABLE;
    case Hc4067Mux::U3_INA219:
      return PCF_U3_EXTERNAL_INA_ENABLE;
    case Hc4067Mux::U2:
      return PCF_U2_INTERNAL_INA_ENABLE;
  }
  return PCF_SAFE_IDLE;
}

inline const char *hc4067_mux_name(Hc4067Mux mux) {
  switch (mux) {
    case Hc4067Mux::U10_TC1047: return "U10/TC1047";
    case Hc4067Mux::U10E: return "U10E/ext-temp";
    case Hc4067Mux::U3_INA219: return "U3/ext-INA";
    case Hc4067Mux::U2: return "U2/int-INA";
  }
  return "?";
}

inline bool hc4067_disable_all() {
  return i2c_write_u8_wire(ADDR_PCF8574, PCF_SAFE_IDLE);
}

inline bool hc4067_enable(Hc4067Mux mux) {
  // PCF8574 reads return physical pin levels, not a safe output latch snapshot.
  // Therefore all HC4067 enables are controlled by explicit full-byte writes.
  return i2c_write_u8_wire(ADDR_PCF8574, pcf_mask_for_hc4067(mux));
}

inline void mux_select(uint8_t ch);

inline const char *c16_hc4067_hold_label() {
  if (!c16_hc4067_hold_active) return "OFF";
  const Hc4067Mux mux = static_cast<Hc4067Mux>(c16_hc4067_hold_mux);
  if (mux == Hc4067Mux::U2) return "ON: U2+U34/internal INA C16";
  if (mux == Hc4067Mux::U3_INA219) return "ON: U3/external INA C16";
  return "ON: unknown mux";
}

inline bool c16_hold_hc4067_mux(Hc4067Mux mux) {
  // Manual probing mode. This keeps exactly one HC4067 enabled on C16 so a DMM/scope
  // can measure COM/Z and INA219 VIN pins. This function intentionally does not read
  // INA219 and does not start a normal slot capture.
  bq_web_pending_mask = 0;
  for (uint8_t slot0 = 0; slot0 < 16; slot0++) bq_web_pending_actions[slot0] = BQ_ACTION_NONE;
  disable_tcas_on_main_bus();
  hc4067_disable_all();
  mux_select(SLOT0_C16);
  delay(5);
  const bool use_u2_u34_pair = (mux == Hc4067Mux::U2);
  const uint8_t hold_mask = use_u2_u34_pair ? PCF_U2_U34_INTERNAL_INA_ENABLE : pcf_mask_for_hc4067(mux);
  const char *hold_name = use_u2_u34_pair ? "U2+U34/int-INA" : hc4067_mux_name(mux);
  const bool ok = i2c_write_u8_wire(ADDR_PCF8574, hold_mask);
  if (ok) {
    c16_hc4067_hold_active = true;
    c16_hc4067_hold_mux = static_cast<uint8_t>(mux);
    c16_hc4067_hold_channel = SLOT0_C16;
    ESP_LOGW(TAG_BQWEB, "C16 HC4067 hold active: %s ch=%u pcf_mask=0x%02X",
             hold_name, SLOT0_C16, hold_mask);
  } else {
    c16_hc4067_hold_active = false;
    c16_hc4067_hold_mux = 0xFF;
    c16_hc4067_hold_channel = SLOT0_C16;
    ESP_LOGE(TAG_BQWEB, "C16 HC4067 hold failed: %s ch=%u pcf_mask=0x%02X",
             hold_name, SLOT0_C16, hold_mask);
  }
  return ok;
}

inline bool c16_release_hc4067_hold() {
  const bool was_active = c16_hc4067_hold_active;
  const uint8_t old_mux = c16_hc4067_hold_mux;
  c16_hc4067_hold_active = false;
  c16_hc4067_hold_mux = 0xFF;
  c16_hc4067_hold_channel = SLOT0_C16;
  const bool ok = hc4067_disable_all();
  ESP_LOGW(TAG_BQWEB, "C16 HC4067 hold release: was_active=%s old_mux=%u idle_ok=%s",
           yesno(was_active), old_mux, yesno(ok));
  return ok;
}

inline void init_pins() {
  pinMode(PIN_MUX_S0, OUTPUT);
  pinMode(PIN_MUX_S1, OUTPUT);
  pinMode(PIN_MUX_S2, OUTPUT);
  pinMode(PIN_MUX_S3, OUTPUT);
  digitalWrite(PIN_MUX_S0, LOW);
  digitalWrite(PIN_MUX_S1, LOW);
  digitalWrite(PIN_MUX_S2, LOW);
  digitalWrite(PIN_MUX_S3, LOW);
  delay(5);

  disable_tcas_on_main_bus();
  hc4067_disable_all();
}

inline void mux_select(uint8_t ch) {
  ch &= 0x0F;
  digitalWrite(PIN_MUX_S0, (ch & 0x01) ? HIGH : LOW);
  digitalWrite(PIN_MUX_S1, (ch & 0x02) ? HIGH : LOW);
  digitalWrite(PIN_MUX_S2, (ch & 0x04) ? HIGH : LOW);
  digitalWrite(PIN_MUX_S3, (ch & 0x08) ? HIGH : LOW);
  delay(3);
}

inline float ina_bus_v_from_raw(uint16_t raw) {
  return ((raw >> 3) & 0x1FFF) * 0.004f;
}

inline float ina_shunt_mv_from_raw(uint16_t raw) {
  return ((int16_t) raw) * 0.01f;
}

inline float tc1047_temp_c_from_v(float v) {
  return (v - 0.500f) * 100.0f;
}

inline float ina_current_ma_from_shunt_mv(float shunt_mv, float shunt_mohm) {
  if (isnan(shunt_mv) || shunt_mohm <= 0.0f) return NAN;
  return shunt_mv * 1000.0f / shunt_mohm;
}

inline bool configure_ina219(esphome::i2c::I2CDevice &dev) {
  return dev_write_reg16_be(dev, 0x00, 0x399F);
}

inline bool read_ina_registers(esphome::i2c::I2CDevice &dev, InaSnapshot &out, float shunt_mohm) {
  for (uint8_t reg = 0; reg <= 5; reg++) {
    uint16_t value = 0;
    const bool ok = dev_read_reg16_be(dev, reg, value);
    out.reg_ok[reg] = ok;
    out.reg[reg] = value;
    yield();
  }

  out.ok = out.reg_ok[0x01] && out.reg_ok[0x02];
  if (out.ok) {
    out.shunt_raw = static_cast<int16_t>(out.reg[0x01]);
    out.bus_raw = out.reg[0x02];
    out.bus_v = ina_bus_v_from_raw(out.bus_raw);
    out.shunt_mv = ina_shunt_mv_from_raw(static_cast<uint16_t>(out.shunt_raw));
    out.current_mA = ina_current_ma_from_shunt_mv(out.shunt_mv, shunt_mohm);
  }
  return out.ok;
}

inline void mirror_internal_ina_to_legacy(SlotSnapshot &s) {
  s.ina_ok = s.internal_ina.ok;
  s.ina_bus_raw = s.internal_ina.bus_raw;
  s.ina_shunt_raw = s.internal_ina.shunt_raw;
  s.bus_v = s.internal_ina.bus_v;
  s.shunt_mv = s.internal_ina.shunt_mv;
}

inline bool capture_hc4067_adc_slot(uint8_t slot0, Hc4067Mux mux, Hc4067AdcSnapshot &out, uint8_t samples = 8) {
  // Generic raw ADC read for HC4067 muxes whose common output is routed to ESP8266 A0.
  // U2 and U3 are intentionally excluded: they route selected cell voltage into INA219, not A0.
  // U34 is also intentionally excluded from raw diagnostics: it selects shunts for the internal INA219 path.
  if (mux == Hc4067Mux::U3_INA219 || mux == Hc4067Mux::U2) {
    ESP_LOGW(TAG_BQWEB, "Refusing raw ADC read on %s C%02u; U2/U3 must be read through their INA219 devices.",
             hc4067_mux_name(mux), slot0 + 1);
    out.ok = false;
    return false;
  }

  disable_tcas_on_main_bus();
  hc4067_disable_all();
  delay(2);

  mux_select(slot0 & 0x0F);
  delay(3);

  if (!hc4067_enable(mux)) {
    ESP_LOGW(TAG_BQWEB, "PCF8574 %s mux-enable write failed before raw ADC read for C%02u.",
             hc4067_mux_name(mux), slot0 + 1);
    hc4067_disable_all();
    out.ok = false;
    return false;
  }

  delay(5);

  uint16_t min_raw = 1023;
  uint16_t max_raw = 0;
  uint32_t sum_raw = 0;
  for (uint8_t i = 0; i < samples; i++) {
    const uint16_t raw = analogRead(A0);
    sum_raw += raw;
    if (raw < min_raw) min_raw = raw;
    if (raw > max_raw) max_raw = raw;
    delay(2);
    yield();
  }

  const bool idle_ok = hc4067_disable_all();
  if (!idle_ok) {
    ESP_LOGW(TAG_BQWEB, "PCF8574 safe-idle restore failed after %s raw ADC read for C%02u.",
             hc4067_mux_name(mux), slot0 + 1);
  }
  delay(2);

  out.ok = idle_ok;
  out.min_raw = min_raw;
  out.max_raw = max_raw;
  out.raw_avg = (float) sum_raw / (float) samples;
  out.v = out.raw_avg * (MCC_TC1047_ADC_FULL_SCALE_V / 1023.0f);

  ESP_LOGI(TAG_BQWEB, "C%02u %s via HC4067: ok=%s raw_avg=%.2f min=%u max=%u voltage=%.4fV",
           slot0 + 1, hc4067_mux_name(mux), yesno(out.ok), out.raw_avg, min_raw, max_raw, out.v);
  return out.ok;
}

inline bool capture_external_temp_slot(uint8_t slot0, SlotSnapshot &s) {
  const bool ok = capture_hc4067_adc_slot(slot0, Hc4067Mux::U10E, s.u10e, 8);
  s.external_temp_ok = ok && s.u10e.ok && !isnan(s.u10e.raw_avg);
  if (s.external_temp_ok) {
    s.u10e.v = s.u10e.raw_avg * (MCC_EXTERNAL_TEMP_ADC_FULL_SCALE_V / 1023.0f);
    s.external_temp_c = tc1047_temp_c_from_v(s.u10e.v) + MCC_EXTERNAL_TEMP_OFFSET_C;
  }
  ESP_LOGI(TAG_TEMP, "C%02u external temp via U10E: ok=%s raw=%.2f voltage=%.4fV temp=%.1fC",
           slot0 + 1, yesno(s.external_temp_ok), s.u10e.raw_avg, s.u10e.v, s.external_temp_c);
  return s.external_temp_ok;
}

inline bool capture_ina219_via_mux(uint8_t slot0, Hc4067Mux mux, esphome::i2c::I2CDevice &dev,
                                   InaSnapshot &out, float shunt_mohm, const char *label) {
  // Per-slot INA path: disable all muxes, select same slot channel, enable required HC4067 mux/mux-pair, read the selected INA219, return PCF to idle.
  disable_tcas_on_main_bus();
  hc4067_disable_all();
  delay(2);

  mux_select(slot0 & 0x0F);
  delay(3);

  const bool use_u2_u34_pair = (mux == Hc4067Mux::U2);
  const uint8_t pcf_mask = use_u2_u34_pair ? PCF_U2_U34_INTERNAL_INA_ENABLE : pcf_mask_for_hc4067(mux);
  const char *mux_label = use_u2_u34_pair ? "U2+U34/int-INA" : hc4067_mux_name(mux);

  if (!i2c_write_u8_wire(ADDR_PCF8574, pcf_mask)) {
    ESP_LOGW(TAG_BQWEB, "PCF8574 %s mux-enable write failed before %s INA219 read for C%02u; mask=0x%02X.",
             mux_label, label, slot0 + 1, pcf_mask);
    hc4067_disable_all();
    return false;
  }

  delay(5);
  const bool cfg_ok = configure_ina219(dev);
  const bool read_ok = cfg_ok && read_ina_registers(dev, out, shunt_mohm);
  const bool idle_ok = hc4067_disable_all();

  if (!cfg_ok) {
    ESP_LOGW(TAG_BQWEB, "%s INA219 config failed for C%02u while %s channel %u was selected; mask=0x%02X.",
             label, slot0 + 1, mux_label, slot0 & 0x0F, pcf_mask);
  }
  if (!idle_ok) {
    ESP_LOGW(TAG_BQWEB, "PCF8574 safe-idle restore failed after %s INA219 read for C%02u.", label, slot0 + 1);
  }
  ESP_LOGI(TAG_BQWEB, "C%02u %s INA219 via %s: ok=%s bus=%.3fV shunt=%.2fmV current=%.1fmA pcf_mask=0x%02X",
           slot0 + 1, label, mux_label, yesno(read_ok), out.bus_v, out.shunt_mv, out.current_mA, pcf_mask);
  return read_ok && idle_ok;
}

inline bool capture_internal_ina219_slot(uint8_t slot0, SlotSnapshot &s) {
  const bool ok = capture_ina219_via_mux(slot0, Hc4067Mux::U2, ina_internal_dev(), s.internal_ina,
                                         MCC_INA219_INTERNAL_SHUNT_MOHM, "internal/U2");
  mirror_internal_ina_to_legacy(s);
  return ok;
}

inline bool capture_external_ina219_slot(uint8_t slot0, SlotSnapshot &s) {
  return capture_ina219_via_mux(slot0, Hc4067Mux::U3_INA219, ina_external_dev(), s.external_ina,
                                MCC_INA219_EXTERNAL_SHUNT_MOHM, "external/U3");
}

// Compatibility wrapper name: now captures both voltage paths, with legacy fields mirroring internal/U2.
inline bool capture_ina219_slot(uint8_t slot0, SlotSnapshot &s) {
  const bool int_ok = capture_internal_ina219_slot(slot0, s);
  const bool ext_ok = capture_external_ina219_slot(slot0, s);
  return int_ok || ext_ok;
}

// Compatibility wrapper: direct internal INA read without HC4067 selection. Prefer capture_internal_ina219_slot().
inline bool read_ina(SlotSnapshot &s) {
  const bool ok = read_ina_registers(ina_internal_dev(), s.internal_ina, MCC_INA219_INTERNAL_SHUNT_MOHM);
  mirror_internal_ina_to_legacy(s);
  return ok;
}

inline void publish_temp_for_slot(uint8_t slot0, float value) {
  switch (slot0 & 0x0F) {
    case 0: id(c1_tc1047_temperature).publish_state(value); break;
    case 1: id(c2_tc1047_temperature).publish_state(value); break;
    case 2: id(c3_tc1047_temperature).publish_state(value); break;
    case 3: id(c4_tc1047_temperature).publish_state(value); break;
    case 4: id(c5_tc1047_temperature).publish_state(value); break;
    case 5: id(c6_tc1047_temperature).publish_state(value); break;
    case 6: id(c7_tc1047_temperature).publish_state(value); break;
    case 7: id(c8_tc1047_temperature).publish_state(value); break;
    case 8: id(c9_tc1047_temperature).publish_state(value); break;
    case 9: id(c10_tc1047_temperature).publish_state(value); break;
    case 10: id(c11_tc1047_temperature).publish_state(value); break;
    case 11: id(c12_tc1047_temperature).publish_state(value); break;
    case 12: id(c13_tc1047_temperature).publish_state(value); break;
    case 13: id(c14_tc1047_temperature).publish_state(value); break;
    case 14: id(c15_tc1047_temperature).publish_state(value); break;
    default: id(c16_tc1047_temperature).publish_state(value); break;
  }
}

inline bool capture_tc1047_slot(uint8_t slot0, SlotSnapshot &s) {
  // U10 HC4067: TC1047 temperature mux, enabled by PCF8574 P0 LOW.
  // Use explicit PCF8574 full-byte writes; never read-modify-write PCF8574.
  disable_tcas_on_main_bus();
  hc4067_disable_all();
  delay(2);

  mux_select(slot0 & 0x0F);
  delay(3);

  if (!hc4067_enable(Hc4067Mux::U10_TC1047)) {
    ESP_LOGW(TAG_TEMP, "PCF8574 U10 TC1047 mux-enable write failed before TC1047 read for C%02u.", slot0 + 1);
    hc4067_disable_all();
    return false;
  }

  delay(5);

  constexpr uint8_t samples = 8;
  uint16_t min_raw = 1023;
  uint16_t max_raw = 0;
  uint32_t sum_raw = 0;

  for (uint8_t i = 0; i < samples; i++) {
    uint16_t raw = analogRead(A0);
    sum_raw += raw;
    if (raw < min_raw) min_raw = raw;
    if (raw > max_raw) max_raw = raw;
    delay(2);
    yield();
  }

  const bool idle_ok = hc4067_disable_all();
  if (!idle_ok) {
    ESP_LOGW(TAG_TEMP, "PCF8574 safe-idle restore failed after TC1047 read for C%02u.", slot0 + 1);
  }
  delay(2);

  const float raw_avg = (float) sum_raw / (float) samples;
  const float voltage = raw_avg * (MCC_TC1047_ADC_FULL_SCALE_V / 1023.0f);
  const float temp_c = tc1047_temp_c_from_v(voltage) + MCC_TC1047_TEMP_OFFSET_C;

  s.temp_ok = true;
  s.temp_min_raw = min_raw;
  s.temp_max_raw = max_raw;
  s.temp_raw_avg = raw_avg;
  s.a0_raw = (int) lroundf(raw_avg);
  s.temp_v = voltage;
  s.temp_c = temp_c;

  publish_temp_for_slot(slot0, temp_c);

  ESP_LOGI(TAG_TEMP,
           "C%02u/Y%02u TC1047 via U10: raw_avg=%.2f min=%u max=%u voltage=%.4fV temp=%.1fC",
           slot0 + 1, slot0, raw_avg, min_raw, max_raw, voltage, temp_c);
  return idle_ok;
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

inline const char *bq_onoff(bool value) {
  return value ? "on" : "off";
}

inline const char *bq_yesno_short(bool value) {
  return value ? "yes" : "no";
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
  return (uint16_t) (512U + (uint16_t) (((reg02 >> 2) & 0x3FU) * 64U));
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

inline bool read_bq_web_regs(esphome::i2c::I2CDevice &dev, SlotSnapshot &s, BQFullRegs &bq) {
  bool all_ok = true;
  for (uint8_t reg = 0; reg <= 0x0A; reg++) {
    const bool ok = dev_read_reg8(dev, reg, bq.r[reg]);
    all_ok = all_ok && ok;
    yield();
  }

  uint8_t reg09_second = bq.r[0x09];
  const bool ok09b = dev_read_reg8(dev, 0x09, reg09_second);
  yield();

  bq.ok = all_ok && ok09b;
  s.bq_ok = bq.ok;
  s.reg00 = bq.r[0x00];
  s.reg08 = bq.r[0x08];
  s.reg09a = bq.r[0x09];
  s.reg09b = reg09_second;
  s.reg0a = bq.r[0x0A];
  return bq.ok;
}
inline void capture_slot_full(uint8_t slot0, SlotSnapshot &s, BQFullRegs &bq) {
  capture_tc1047_slot(slot0, s);
  capture_external_temp_slot(slot0, s);

  // INA219 voltage/current reads are per-slot and split into two physical paths:
  // internal: U2+U34 HC4067 pair -> INA219 #2 @ 0x4F; external: U3 HC4067 -> INA219 #1 @ 0x41.
  capture_ina219_slot(slot0, s);

  esphome::i2c::I2CDevice &bq_dev = bq_dev_for_slot(slot0);
  read_bq_web_regs(bq_dev, s, bq);
}

inline void publish_bq_slot_measurements(uint8_t slot0, const SlotSnapshot &s, const BQFullRegs &bq) {
  // ESP8266 RAM guard:
  // Keep full BQ data in /bq and /status. Publishing BQ entities for all 16 slots
  // created hundreds of ESPHome entities and crashed after WiFi/API startup.
  // Preserve only the legacy C16 BQ diagnostic entities that existed in the stable YAML.
  if ((slot0 & 0x0F) != SLOT0_C16) return;

  id(c16_bq_present).publish_state(s.bq_ok ? 1.0f : 0.0f);
  id(c16_bq_reg08).publish_state(bq.ok ? (float) bq.r[0x08] : NAN);
  id(c16_bq_reg09).publish_state(bq.ok ? (float) s.reg09b : NAN);
  id(c16_bq_reg0a).publish_state(bq.ok ? (float) bq.r[0x0A] : NAN);
  id(c16_bq_dpm_stat).publish_state(bq.ok ? (float) ((bq.r[0x08] >> 3) & 1) : NAN);
  id(c16_bq_pg_stat).publish_state(bq.ok ? (float) ((bq.r[0x08] >> 2) & 1) : NAN);
  id(c16_bq_vsys_stat).publish_state(bq.ok ? (float) (bq.r[0x08] & 1) : NAN);
  id(c16_bq_charge_status).publish_state(bq.ok ? bq_chrg_str(bq.r[0x08]) : "unavailable");
  id(c16_bq_fault_status).publish_state(bq.ok ? bq_fault_str(s.reg09b) : "unavailable");
}

inline void publish_ina_slot_measurements(uint8_t slot0, const SlotSnapshot &s) {
  const float bus = s.internal_ina.ok ? s.internal_ina.bus_v : NAN;
  const float shunt = s.internal_ina.ok ? s.internal_ina.shunt_mv : NAN;
  const float current = s.internal_ina.ok ? s.internal_ina.current_mA : NAN;
  // Keep this limited to the internal INA219 values requested for automatic polling.
  // The previous crash was caused by hundreds of BQ entities, not by these 16 current sensors.
  switch (slot0 & 0x0F) {
    case 0: id(c1_ina_bus_voltage).publish_state(bus); id(c1_ina_shunt_voltage).publish_state(shunt); id(c1_ina_current).publish_state(current); break;
    case 1: id(c2_ina_bus_voltage).publish_state(bus); id(c2_ina_shunt_voltage).publish_state(shunt); id(c2_ina_current).publish_state(current); break;
    case 2: id(c3_ina_bus_voltage).publish_state(bus); id(c3_ina_shunt_voltage).publish_state(shunt); id(c3_ina_current).publish_state(current); break;
    case 3: id(c4_ina_bus_voltage).publish_state(bus); id(c4_ina_shunt_voltage).publish_state(shunt); id(c4_ina_current).publish_state(current); break;
    case 4: id(c5_ina_bus_voltage).publish_state(bus); id(c5_ina_shunt_voltage).publish_state(shunt); id(c5_ina_current).publish_state(current); break;
    case 5: id(c6_ina_bus_voltage).publish_state(bus); id(c6_ina_shunt_voltage).publish_state(shunt); id(c6_ina_current).publish_state(current); break;
    case 6: id(c7_ina_bus_voltage).publish_state(bus); id(c7_ina_shunt_voltage).publish_state(shunt); id(c7_ina_current).publish_state(current); break;
    case 7: id(c8_ina_bus_voltage).publish_state(bus); id(c8_ina_shunt_voltage).publish_state(shunt); id(c8_ina_current).publish_state(current); break;
    case 8: id(c9_ina_bus_voltage).publish_state(bus); id(c9_ina_shunt_voltage).publish_state(shunt); id(c9_ina_current).publish_state(current); break;
    case 9: id(c10_ina_bus_voltage).publish_state(bus); id(c10_ina_shunt_voltage).publish_state(shunt); id(c10_ina_current).publish_state(current); break;
    case 10: id(c11_ina_bus_voltage).publish_state(bus); id(c11_ina_shunt_voltage).publish_state(shunt); id(c11_ina_current).publish_state(current); break;
    case 11: id(c12_ina_bus_voltage).publish_state(bus); id(c12_ina_shunt_voltage).publish_state(shunt); id(c12_ina_current).publish_state(current); break;
    case 12: id(c13_ina_bus_voltage).publish_state(bus); id(c13_ina_shunt_voltage).publish_state(shunt); id(c13_ina_current).publish_state(current); break;
    case 13: id(c14_ina_bus_voltage).publish_state(bus); id(c14_ina_shunt_voltage).publish_state(shunt); id(c14_ina_current).publish_state(current); break;
    case 14: id(c15_ina_bus_voltage).publish_state(bus); id(c15_ina_shunt_voltage).publish_state(shunt); id(c15_ina_current).publish_state(current); break;
    default: id(c16_ina_bus_voltage).publish_state(bus); id(c16_ina_shunt_voltage).publish_state(shunt); id(c16_ina_current).publish_state(current); break;
  }
}

inline void publish_c16_diagnostics(const SlotSnapshot &s, const BQFullRegs &bq) {
  publish_bq_slot_measurements(SLOT0_C16, s, bq);
  publish_ina_slot_measurements(SLOT0_C16, s);
}

inline void log_bq_line(uint8_t slot0, const SlotSnapshot &s, const BQFullRegs &bq, const char *prefix) {
  ESP_LOGI(TAG_BQFULL,
           "%s slot=C%02u route=%s bus=%s TCA=0x%02X ch=%u mask=0x%02X BQ=%s REG00=0x%02X REG01=0x%02X REG02=0x%02X REG05=0x%02X REG08=0x%02X chg=%s DPM=%u PG=%u VSYS=%u REG09=0x%02X fault=%s REG0A=0x%02X int_temp=%.1fC int_INA=%.3fV/%.2fmV/%.1fmA ext_temp=%.1fC ext_INA=%.3fV/%.2fmV/%.1fmA",
           prefix, slot0 + 1, tca_ref_for_slot(slot0), bus_id_for_slot(slot0),
           tca_addr_for_slot(slot0), tca_channel_for_slot(slot0), tca_mask_for_slot(slot0),
           yesno(bq.ok), bq.r[0x00], bq.r[0x01], bq.r[0x02], bq.r[0x05], bq.r[0x08], bq_chrg_str(bq.r[0x08]),
           (bq.r[0x08] >> 3) & 1, (bq.r[0x08] >> 2) & 1, bq.r[0x08] & 1,
           s.reg09b, bq_fault_str(s.reg09b), bq.r[0x0A],
           s.temp_c, s.internal_ina.bus_v, s.internal_ina.shunt_mv, s.internal_ina.current_mA,
           s.external_temp_c, s.external_ina.bus_v, s.external_ina.shunt_mv, s.external_ina.current_mA);
}

inline uint8_t pending_count() {
  uint16_t mask = bq_web_pending_mask;
  uint8_t n = 0;
  while (mask != 0) {
    n += mask & 1U;
    mask >>= 1;
  }
  return n;
}

inline uint8_t pending_action_count() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < 16; i++) {
    if (bq_web_pending_actions[i] != BQ_ACTION_NONE) n++;
  }
  return n;
}

inline const char *bq_action_label(uint8_t action) {
  switch (action) {
    case BQ_ACTION_CHARGE_ON: return "Charge ON";
    case BQ_ACTION_CHARGE_OFF: return "Charge OFF";
    case BQ_ACTION_WD_RESET: return "WD Reset";
    case BQ_ACTION_IIN_500: return "IIN 500mA";
    case BQ_ACTION_IIN_900: return "IIN 900mA";
    case BQ_ACTION_IIN_1500: return "IIN 1.5A";
    case BQ_ACTION_IIN_3000: return "IIN 3A";
    case BQ_ACTION_ICHG_512: return "ICHG 512mA";
    case BQ_ACTION_ICHG_1024: return "ICHG 1A";
    case BQ_ACTION_ICHG_2048: return "ICHG 2A";
    case BQ_ACTION_ICHG_3008: return "ICHG 3A";
    default: return "-";
  }
}

inline const char *bq_action_short(uint8_t action) {
  switch (action) {
    case BQ_ACTION_CHARGE_ON: return "ON";
    case BQ_ACTION_CHARGE_OFF: return "OFF";
    case BQ_ACTION_WD_RESET: return "WD";
    case BQ_ACTION_IIN_500: return "IIN.5";
    case BQ_ACTION_IIN_900: return "IIN.9";
    case BQ_ACTION_IIN_1500: return "IIN1.5";
    case BQ_ACTION_IIN_3000: return "IIN3";
    case BQ_ACTION_ICHG_512: return "CHG.5";
    case BQ_ACTION_ICHG_1024: return "CHG1";
    case BQ_ACTION_ICHG_2048: return "CHG2";
    case BQ_ACTION_ICHG_3008: return "CHG3";
    default: return "-";
  }
}


inline const char *c16_hw_action_label(uint8_t action) {
  switch (action) {
    case C16_HW_ACTION_Q34_OFF: return "Q34 OFF";
    case C16_HW_ACTION_Q34_1P: return "Q34 PWM 1%";
    case C16_HW_ACTION_Q34_5P: return "Q34 PWM 5%";
    case C16_HW_ACTION_Q34_25P: return "Q34 PWM 25%";
    case C16_HW_ACTION_Q34_100P: return "Q34 PWM 100%";
    case C16_HW_ACTION_Q34_PULSE_5P_1000MS: return "Q34 pulse 5%/1s";
    case C16_HW_ACTION_Q35_PROBE: return "Q35 probe";
    case C16_HW_ACTION_HOLD_U2_INA: return "hold U2+U34/internal INA";
    case C16_HW_ACTION_HOLD_U3_INA: return "hold U3/external INA";
    case C16_HW_ACTION_HOLD_RELEASE: return "release mux hold";
    default: return "-";
  }
}

inline bool pca9685_write_reg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ADDR_PCA9685);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

inline bool pca9685_write4(uint8_t reg, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  Wire.beginTransmission(ADDR_PCA9685);
  Wire.write(reg);
  Wire.write(b0);
  Wire.write(b1);
  Wire.write(b2);
  Wire.write(b3);
  return Wire.endTransmission() == 0;
}

inline bool pca9685_wake_for_test() {
  // Keep this conservative: do not change prescale/frequency here. Ensure the device is awake and supports auto-increment.
  // MODE1: AI=1, SLEEP=0. Existing output polarity/drive mode in MODE2 is not modified.
  return pca9685_write_reg(0x00, 0x20);
}

inline bool pca9685_channel_full_off(uint8_t ch) {
  if (ch > 15) return false;
  const uint8_t base = (uint8_t) (0x06U + 4U * ch);
  return pca9685_write4(base, 0x00, 0x00, 0x00, 0x10);  // LEDn_OFF_H bit4 = full OFF.
}

inline bool pca9685_channel_full_on(uint8_t ch) {
  if (ch > 15) return false;
  const uint8_t base = (uint8_t) (0x06U + 4U * ch);
  return pca9685_write4(base, 0x00, 0x10, 0x00, 0x00);  // LEDn_ON_H bit4 = full ON.
}

inline bool pca9685_channel_pwm_12bit(uint8_t ch, uint16_t off_count) {
  if (ch > 15) return false;
  if (off_count == 0) return pca9685_channel_full_off(ch);
  if (off_count >= 4095) return pca9685_channel_full_on(ch);
  const uint8_t base = (uint8_t) (0x06U + 4U * ch);
  return pca9685_write4(base, 0x00, 0x00, (uint8_t) (off_count & 0xFFU), (uint8_t) ((off_count >> 8) & 0x0FU));
}

inline bool c16_q34_set_pwm_permille(uint16_t permille) {
  if (permille > 1000) permille = 1000;
  const bool wake_ok = pca9685_wake_for_test();
  if (!wake_ok) return false;
  if (permille == 0) return pca9685_channel_full_off(PCA9685_CH_C16_Q34);
  if (permille >= 1000) return pca9685_channel_full_on(PCA9685_CH_C16_Q34);
  const uint16_t off_count = (uint16_t) ((4096UL * (uint32_t) permille) / 1000UL);
  return pca9685_channel_pwm_12bit(PCA9685_CH_C16_Q34, off_count == 0 ? 1 : off_count);
}

inline bool queue_c16_hw_action(uint8_t action) {
  if (action == C16_HW_ACTION_NONE) return false;
  c16_hw_pending_action = action;
  esphome::Application::wake_loop_any_context();
  ESP_LOGW(TAG_BQWEB, "Queued C16 hardware action %s.", c16_hw_action_label(action));
  return true;
}

inline bool queue_slot(uint8_t slot0) {
  if (slot0 > 15) return false;
  if (c16_hc4067_hold_active) {
    ESP_LOGW(TAG_BQWEB, "Ignored slot read request for C%02u because C16 HC4067 hold is active: %s. Release hold first.",
             slot0 + 1, c16_hc4067_hold_label());
    return false;
  }
  bq_web_pending_mask |= (uint16_t) (1U << slot0);
  esphome::Application::wake_loop_any_context();
  return true;
}

inline bool queue_all_slots() {
  bq_web_pending_mask |= 0xFFFFU;
  esphome::Application::wake_loop_any_context();
  ESP_LOGW(TAG_BQWEB, "Queued all slot diagnostics C01..C16.");
  return true;
}

inline bool queue_auto_poll_all_slots() {
  if (c16_hc4067_hold_active) {
    ESP_LOGW(TAG_BQWEB, "Skipped auto poll queue because C16 HC4067 hold is active: %s.", c16_hc4067_hold_label());
    return false;
  }
  auto_poll_pending_mask |= 0xFFFFU;
  esphome::Application::wake_loop_any_context();
  ESP_LOGW(TAG_BQWEB, "Queued lightweight auto poll C01..C16: internal TC1047 + internal INA219 only.");
  return true;
}

inline bool queue_bq_action(uint8_t slot0, uint8_t action) {
  if (slot0 > 15 || action == BQ_ACTION_NONE) return false;
  bq_web_pending_actions[slot0] = action;
  esphome::Application::wake_loop_any_context();
  ESP_LOGW(TAG_BQWEB, "Queued BQ action %s for C%02u.", bq_action_label(action), slot0 + 1);
  return true;
}

inline bool queue_bq_action_all(uint8_t action) {
  if (action == BQ_ACTION_NONE) return false;
  for (uint8_t slot0 = 0; slot0 < 16; slot0++) {
    bq_web_pending_actions[slot0] = action;
  }
  esphome::Application::wake_loop_any_context();
  ESP_LOGW(TAG_BQWEB, "Queued BQ action %s for all slots C01..C16.", bq_action_label(action));
  return true;
}

inline void clear_pending_queue() {
  bq_web_pending_mask = 0;
  auto_poll_pending_mask = 0;
  for (uint8_t slot0 = 0; slot0 < 16; slot0++) {
    bq_web_pending_actions[slot0] = BQ_ACTION_NONE;
  }
  c16_hw_pending_action = C16_HW_ACTION_NONE;
  ESP_LOGW(TAG_BQWEB, "Cleared pending /bq read/action/C16-HW queues. Busy operation, if any, will finish.");
}

struct BqHtmlChunkBuffer {
  String data;

  explicit BqHtmlChunkBuffer(size_t reserve_size = 1024) {
    data.reserve(reserve_size);
  }

  void clear() { data = ""; }
  size_t length() const { return data.length(); }
  const char *c_str() const { return data.c_str(); }

  void print(const char *value) { if (value != nullptr) data += value; }
  void print(char *value) { if (value != nullptr) data += value; }
  void print(const String &value) { data += value; }
  void print(char value) { data += value; }
  void print(int value) { data += String(value); }
  void print(unsigned int value) { data += String(value); }
  void print(unsigned long value) { data += String(value); }
};

template<typename TResponse>
inline void bq_web_print_cell(TResponse *response, const char *value) {
  response->print("<td>");
  response->print(value);
  response->print("</td>");
}

template<typename TResponse>
inline void bq_web_print_cell_u8_hex(TResponse *response, uint8_t value) {
  char buf[8];
  snprintf(buf, sizeof(buf), "0x%02X", value);
  bq_web_print_cell(response, buf);
}

template<typename TResponse>
inline void bq_web_print_cell_float(TResponse *response, float value, uint8_t decimals) {
  char buf[16];
  if (isnan(value)) {
    snprintf(buf, sizeof(buf), "nan");
  } else if (decimals == 3) {
    snprintf(buf, sizeof(buf), "%.3f", value);
  } else if (decimals == 1) {
    snprintf(buf, sizeof(buf), "%.1f", value);
  } else {
    snprintf(buf, sizeof(buf), "%.2f", value);
  }
  bq_web_print_cell(response, buf);
}

template<typename TResponse>
inline void bq_web_print_reg00(TResponse *response, uint8_t reg00) {
  char buf[48];
  snprintf(buf, sizeof(buf), "HIZ %s IIN %s (%02X)",
           bq_onoff((reg00 & 0x80U) != 0), bq_iinlim_str(reg00), reg00);
  bq_web_print_cell(response, buf);
}

template<typename TResponse>
inline void bq_web_print_reg01(TResponse *response, uint8_t reg01) {
  char buf[48];
  snprintf(buf, sizeof(buf), "CHG %s OTG %s (%02X)",
           bq_onoff((reg01 & 0x10U) != 0), bq_onoff((reg01 & 0x20U) != 0), reg01);
  bq_web_print_cell(response, buf);
}

template<typename TResponse>
inline void bq_web_print_reg02(TResponse *response, uint8_t reg02) {
  char buf[48];
  snprintf(buf, sizeof(buf), "ICHG %umA 20%% %s (%02X)",
           bq_ichg_ma(reg02), bq_onoff((reg02 & 0x01U) != 0), reg02);
  bq_web_print_cell(response, buf);
}

template<typename TResponse>
inline void bq_web_print_reg05(TResponse *response, uint8_t reg05) {
  char buf[48];
  snprintf(buf, sizeof(buf), "WD %s TMR %s (%02X)",
           bq_watchdog_str(reg05), bq_onoff((reg05 & 0x08U) != 0), reg05);
  bq_web_print_cell(response, buf);
}

template<typename TResponse>
inline void bq_web_print_reg08(TResponse *response, uint8_t reg08) {
  char buf[72];
  snprintf(buf, sizeof(buf), "%s DPM %s PG %s VSYS %s (%02X)",
           bq_chrg_str(reg08), bq_yesno_short((reg08 & 0x08U) != 0),
           bq_yesno_short((reg08 & 0x04U) != 0), bq_yesno_short((reg08 & 0x01U) != 0), reg08);
  bq_web_print_cell(response, buf);
}

template<typename TResponse>
inline void bq_web_print_reg09(TResponse *response, uint8_t reg09) {
  char buf[40];
  snprintf(buf, sizeof(buf), "%s (%02X)", bq_fault_str(reg09), reg09);
  bq_web_print_cell(response, buf);
}

template<typename TResponse>
inline void bq_web_print_reg0a(TResponse *response, uint8_t reg0a) {
  char buf[40];
  snprintf(buf, sizeof(buf), "part%u rev%u (%02X)", (reg0a >> 3) & 0x07U, reg0a & 0x07U, reg0a);
  bq_web_print_cell(response, buf);
}

template<typename TResponse>
inline void bq_web_print_action_cell(TResponse *response, uint8_t slot0, const char *action, const char *label) {
  response->print("<td><form class=inline method=get action=/bq>");
  char slot_buf[4];
  snprintf(slot_buf, sizeof(slot_buf), "%u", slot0 + 1);
  response->print("<input type=hidden name=s value=");
  response->print(slot_buf);
  response->print("><button type=submit name=a value=\"");
  response->print(action);
  response->print("\">");
  response->print(label);
  response->print("</button></form></td>");
}

template<typename TResponse>
inline void bq_web_print_iin_cell(TResponse *response, uint8_t slot0) {
  response->print("<td>");
  char slot_buf[4];
  snprintf(slot_buf, sizeof(slot_buf), "%u", slot0 + 1);
  const char *values[] = {"i2", "i3", "i5", "i7"};
  const char *labels[] = {"0.5A", "0.9A", "1.5A", "3A"};
  for (uint8_t i = 0; i < 4; i++) {
    response->print("<form class=inline method=get action=/bq><input type=hidden name=s value=");
    response->print(slot_buf);
    response->print("><button type=submit name=a value=\"");
    response->print(values[i]);
    response->print("\">");
    response->print(labels[i]);
    response->print("</button></form>");
  }
  response->print("</td>");
}

inline const char *bq_ichg_action_for_reg02(uint8_t reg02) {
  const uint8_t code = (reg02 >> 2) & 0x3FU;
  if (code == 0) return "c0";
  if (code == 8) return "c8";
  if (code == 24) return "c24";
  if (code == 39) return "c39";
  return "";
}

template<typename TResponse>
inline void bq_web_print_ichg_option(TResponse *response, const char *value, const char *label, const char *selected_value) {
  response->print("<option value=\"");
  response->print(value);
  response->print("\"");
  if (selected_value != nullptr && strcmp(value, selected_value) == 0) {
    response->print(" selected");
  }
  response->print(">");
  response->print(label);
  response->print("</option>");
}

template<typename TResponse>
inline void bq_web_print_ichg_select_cell(TResponse *response, uint8_t slot0, bool have_data, uint8_t reg02) {
  const char *selected = have_data ? bq_ichg_action_for_reg02(reg02) : "";
  response->print("<td><form class=inline method=get action=/bq>");
  char slot_buf[4];
  snprintf(slot_buf, sizeof(slot_buf), "%u", slot0 + 1);
  response->print("<input type=hidden name=s value=");
  response->print(slot_buf);
  response->print("><select name=a>");
  if (selected[0] == '\0') {
    response->print("<option value=none selected>set ICHG</option>");
  }
  bq_web_print_ichg_option(response, "c0", "0.5A", selected);
  bq_web_print_ichg_option(response, "c8", "1A", selected);
  bq_web_print_ichg_option(response, "c24", "2A", selected);
  bq_web_print_ichg_option(response, "c39", "3A", selected);
  response->print("</select><button type=submit>Set</button></form></td>");
}

template<typename TResponse>
inline void bq_web_print_ichg_cell(TResponse *response, uint8_t slot0) {
  bq_web_print_ichg_select_cell(response, slot0, false, 0);
}

template<typename TResponse>
inline void bq_web_print_bulk_action_cell(TResponse *response, const char *action, const char *label) {
  response->print("<td><form class=inline method=get action=/bq>");
  response->print("<input type=hidden name=all value=1>");
  response->print("<button type=submit name=a value=\"");
  response->print(action);
  response->print("\">");
  response->print(label);
  response->print("</button></form></td>");
}

template<typename TResponse>
inline void bq_web_print_hc4067_adc_cell(TResponse *response, const Hc4067AdcSnapshot &snap) {
  char buf[40];
  if (!snap.ok || isnan(snap.raw_avg) || isnan(snap.v)) {
    snprintf(buf, sizeof(buf), "nan");
  } else {
    snprintf(buf, sizeof(buf), "%.0f/%.3f", snap.raw_avg, snap.v);
  }
  bq_web_print_cell(response, buf);
}

template<typename TResponse>
inline void bq_web_print_ina_cell(TResponse *response, const InaSnapshot &snap) {
  char buf[56];
  if (!snap.ok || isnan(snap.bus_v)) {
    snprintf(buf, sizeof(buf), "nan");
  } else {
    snprintf(buf, sizeof(buf), "%.3fV/%.0fmA", snap.bus_v, snap.current_mA);
  }
  bq_web_print_cell(response, buf);
}

template<typename TResponse>
inline void bq_web_print_bulk_read_cell(TResponse *response) {
  response->print("<td><form class=inline method=get action=/bq>");
  response->print("<button type=submit name=ra value=1>Read all</button>");
  response->print("</form></td>");
}

template<typename TResponse>
inline void bq_web_print_bulk_clear_cell(TResponse *response) {
  response->print("<td><form class=inline method=get action=/bq>");
  response->print("<button type=submit name=clear value=1>Clear</button>");
  response->print("</form></td>");
}

template<typename TResponse>
inline void bq_web_print_bulk_ichg_select_cell(TResponse *response) {
  response->print("<td><form class=inline method=get action=/bq>");
  response->print("<input type=hidden name=all value=1>");
  response->print("<select name=a>");
  response->print("<option value=none selected>set all</option>");
  bq_web_print_ichg_option(response, "c0", "0.5A", "");
  bq_web_print_ichg_option(response, "c8", "1A", "");
  bq_web_print_ichg_option(response, "c24", "2A", "");
  bq_web_print_ichg_option(response, "c39", "3A", "");
  response->print("</select><button type=submit>Set</button></form></td>");
}

template<typename TResponse>
inline void bq_web_print_bulk_row(TResponse *response) {
  response->print("<tr class=bulk><th>ALL</th>");
  bq_web_print_bulk_read_cell(response);
  bq_web_print_bulk_action_cell(response, "1", "ON all");
  bq_web_print_bulk_action_cell(response, "0", "OFF all");
  bq_web_print_bulk_action_cell(response, "w", "WD all");
  bq_web_print_bulk_action_cell(response, "i2", "0.5A all");
  bq_web_print_bulk_action_cell(response, "i3", "0.9A all");
  bq_web_print_bulk_action_cell(response, "i5", "1.5A all");
  bq_web_print_bulk_action_cell(response, "i7", "3A all");
  bq_web_print_bulk_ichg_select_cell(response);
  bq_web_print_bulk_clear_cell(response);
  response->print("<td colspan=15>Bulk commands are queued for C01..C16 and executed one slot at a time in the main loop. Each row reads internal temp/INA via U10 and U2+U34, external temp/INA via U10E/U3 on the matching channel.</td></tr>");
}

template<typename TResponse>
inline void bq_web_print_queue_cell(TResponse *response, uint8_t slot0, uint16_t pending_mask) {
  response->print("<td>");
  const uint8_t pending_action = bq_web_pending_actions[slot0];
  if (bq_web_read_busy && slot0 == bq_web_pending_slot0 && bq_web_current_action != BQ_ACTION_NONE) {
    response->print(bq_action_short(bq_web_current_action));
  } else if (pending_action != BQ_ACTION_NONE) {
    response->print(bq_action_short(pending_action));
  } else if ((pending_mask & (1U << slot0)) != 0) {
    response->print("R");
  } else {
    response->print("-");
  }
  response->print("</td>");
}

template<typename TResponse>
inline void bq_web_print_row(TResponse *response, uint8_t slot0) {
  const bool have_data = bq_cached_valid[slot0];
  const SlotSnapshot *s = have_data ? &bq_cached_slots[slot0] : nullptr;
  const BQFullRegs *bq = have_data ? &bq_cached_regs[slot0] : nullptr;
  const uint16_t pending_mask = bq_web_pending_mask;

  response->print("<tr>");
  char buf[192];

  snprintf(buf, sizeof(buf), "C%02u", slot0 + 1);
  bq_web_print_cell(response, buf);

  if (bq_web_read_busy && slot0 == bq_web_pending_slot0) {
    snprintf(buf, sizeof(buf), "Rding");
  } else if ((pending_mask & (1U << slot0)) != 0) {
    snprintf(buf, sizeof(buf), "Q");
  } else {
    snprintf(buf, sizeof(buf),
             "<form class=inline method=get action=/bq><input type=hidden name=s value=%u><button type=submit>R</button></form>",
             slot0 + 1);
  }
  bq_web_print_cell(response, buf);

  bq_web_print_action_cell(response, slot0, "1", "ON");
  bq_web_print_action_cell(response, slot0, "0", "OFF");
  bq_web_print_action_cell(response, slot0, "w", "WD");
  bq_web_print_action_cell(response, slot0, "i2", ".5");
  bq_web_print_action_cell(response, slot0, "i3", ".9");
  bq_web_print_action_cell(response, slot0, "i5", "1.5");
  bq_web_print_action_cell(response, slot0, "i7", "3");
  bq_web_print_ichg_select_cell(response, slot0, have_data && bq != nullptr && bq->ok, have_data && bq != nullptr ? bq->r[0x02] : 0);
  bq_web_print_queue_cell(response, slot0, pending_mask);

  bq_web_print_cell(response, bq_route_for_slot(slot0).tca_index == 0 ? "T0" : "T1");

  snprintf(buf, sizeof(buf), "%u", tca_channel_for_slot(slot0));
  bq_web_print_cell(response, buf);

  if (!have_data) {
    for (uint8_t i = 0; i < 13; i++) bq_web_print_cell(response, "-");
    response->print("</tr>");
    return;
  }

  bq_web_print_cell(response, bq->ok ? "YES" : "NO");
  if (bq->ok) {
    bq_web_print_reg00(response, bq->r[0x00]);
    bq_web_print_reg01(response, bq->r[0x01]);
    bq_web_print_reg02(response, bq->r[0x02]);
    bq_web_print_reg05(response, bq->r[0x05]);
    bq_web_print_reg08(response, bq->r[0x08]);
    bq_web_print_reg09(response, s->reg09b);
    bq_web_print_reg0a(response, bq->r[0x0A]);
  } else {
    for (uint8_t i = 0; i < 7; i++) bq_web_print_cell(response, "unavailable");
  }

  bq_web_print_cell_float(response, s->temp_c, 1);
  bq_web_print_ina_cell(response, s->internal_ina);
  bq_web_print_cell_float(response, s->external_temp_c, 1);
  bq_web_print_ina_cell(response, s->external_ina);

  snprintf(buf, sizeof(buf), "%lus", (unsigned long) ((millis() - bq_cached_read_ms[slot0]) / 1000UL));
  bq_web_print_cell(response, buf);
  response->print("</tr>");
}


template<typename TResponse>
inline void bq_web_print_c16_hw_button(TResponse *response, const char *action, const char *label) {
  response->print("<form class=inline method=get action=/bq>");
  response->print("<button type=submit name=c16hw value=\"");
  response->print(action);
  response->print("\">");
  response->print(label);
  response->print("</button></form>");
}

template<typename TResponse>
inline void bq_web_print_c16_hw_panel(TResponse *response) {
  response->print("<h4>C16 Q34/Q35 hardware tests</h4><p>");
  response->print("Pending: ");
  response->print(c16_hw_action_label(c16_hw_pending_action));
  response->print("; running: ");
  response->print(c16_hw_action_label(c16_hw_current_action));
  response->print("</p><p>Q34: ");
  bq_web_print_c16_hw_button(response, "q34_off", "OFF");
  bq_web_print_c16_hw_button(response, "q34_1", "1%");
  bq_web_print_c16_hw_button(response, "q34_5", "5%");
  bq_web_print_c16_hw_button(response, "q34_25", "25%");
  bq_web_print_c16_hw_button(response, "q34_100", "100%");
  bq_web_print_c16_hw_button(response, "q34_pulse5", "5% pulse 1s");
  response->print("</p><p>Q35: ");
  bq_web_print_c16_hw_button(response, "q35_probe", "probe/read C16");
  response->print(" <small>Q35 is not directly drivable from the visible schematic fragment; this button forces Q34 OFF and queues a C16 read so you can probe C16_BAT_GND/gate safely.</small></p>");
  response->print("<p><a href=/c16>C16 raw diagnostics</a></p>");
}

struct BqWebChunkState {
  uint8_t phase{0};
  uint8_t slot0{0};
  size_t offset{0};
  BqHtmlChunkBuffer chunk{1400};
};

inline bool bq_web_build_next_chunk(BqWebChunkState &state, const String &notice) {
  state.chunk.clear();

  if (state.phase == 0) {
    state.chunk.print("<!doctype html><html><head><meta charset=utf-8><title>MCC /bq</title>");
    state.chunk.print("<style>body{font-family:sans-serif}.wrap{overflow-x:auto}table{border-collapse:collapse;white-space:nowrap}th,td{padding:2px 4px}.inline{display:inline;margin:0}button,select{font:inherit;padding:2px 6px}select{max-width:7em}.bulk th,.bulk td{background:#eee}</style>");
    if (bq_web_read_busy || bq_web_pending_mask != 0 || pending_action_count() != 0) {
      state.chunk.print("<meta http-equiv=refresh content=\"2;url=/bq\">");
    }
    state.chunk.print("</head><body><h3>MCC Pro slot diagnostics</h3>");

    if (notice.length() > 0) {
      state.chunk.print("<p>");
      state.chunk.print(notice);
      state.chunk.print("</p>");
    }

    char status[112];
    snprintf(status, sizeof(status), "<p>Busy: %s; pending reads: %u; pending BQ actions: %u; pending C16 HW: %s</p>",
             yesno(bq_web_read_busy), pending_count(), pending_action_count(), c16_hw_action_label(c16_hw_pending_action));
    state.chunk.print(status);

    bq_web_print_c16_hw_panel(&state.chunk);

    state.chunk.print("<div class=wrap><table border=1>");
    bq_web_print_bulk_row(&state.chunk);
    state.chunk.print("<tr>"
                      "<th colspan=2>Web</th><th colspan=8>BQ cmd</th><th>Web</th><th colspan=2>TCA</th>"
                      "<th colspan=8>BQ</th><th colspan=4>HC4067 slot channel</th><th>Cache</th></tr>"
                      "<tr><th>Slot</th><th>Read</th><th>ON</th><th>OFF</th><th>WD</th>"
                      "<th>I.5</th><th>I.9</th><th>I1.5</th><th>I3</th><th>ICHG</th>"
                      "<th>Q</th><th>Mux</th><th>Ch</th>"
                      "<th>Present</th><th>Input source</th><th>Power config</th><th>Charge current</th><th>Watchdog</th>"
                      "<th>System status</th><th>Fault</th><th>Part/rev</th>"
                      "<th>Int Temp C</th><th>Int V/mA</th><th>Ext Temp C</th><th>Ext V/mA</th><th>Age</th></tr>");
    state.phase = 1;
    return true;
  }

  if (state.phase == 1) {
    if (state.slot0 < 16) {
      bq_web_print_row(&state.chunk, state.slot0);
      state.slot0++;
      return true;
    }
    state.phase = 2;
  }

  if (state.phase == 2) {
    state.chunk.print("</table></div></body></html>");
    state.phase = 3;
    return true;
  }

  return false;
}

inline size_t bq_web_copy_chunk(BqWebChunkState &state, const String &notice,
                                uint8_t *buffer, size_t max_len) {
  if (max_len == 0) return 0;

  while (state.offset >= state.chunk.length()) {
    state.offset = 0;
    if (!bq_web_build_next_chunk(state, notice)) return 0;
  }

  const size_t available = state.chunk.length() - state.offset;
  const size_t to_copy = available < max_len ? available : max_len;
  memcpy(buffer, state.chunk.c_str() + state.offset, to_copy);
  state.offset += to_copy;
  return to_copy;
}

inline void send_bq_table_response(AsyncWebServerRequest *request, const char *notice) {
  ESP_LOGW(TAG_BQWEB, "Serving /bq as chunked table response; no whole-page RAM buffer.");

  const String notice_copy = notice != nullptr ? String(notice) : String("");
  BqWebChunkState state;
  auto *response = request->beginChunkedResponse(
    "text/html",
    [notice_copy, state](uint8_t *buffer, size_t max_len, size_t index) mutable -> size_t {
      (void) index;
      return bq_web_copy_chunk(state, notice_copy, buffer, max_len);
    }
  );

  request->send(response);
  ESP_LOGW(TAG_BQWEB, "Serving /bq chunked response started.");
}


template<typename TResponse>
inline void status_web_print_row(TResponse *response, uint8_t slot0) {
  const bool have_data = bq_cached_valid[slot0];
  const SlotSnapshot *s = have_data ? &bq_cached_slots[slot0] : nullptr;
  const BQFullRegs *bq = have_data ? &bq_cached_regs[slot0] : nullptr;
  char buf[160];

  response->print("<tr>");
  snprintf(buf, sizeof(buf), "C%02u", slot0 + 1);
  bq_web_print_cell(response, buf);

  if (!have_data || s == nullptr || bq == nullptr) {
    for (uint8_t i = 0; i < 9; i++) bq_web_print_cell(response, "-");
    response->print("</tr>");
    return;
  }

  bq_web_print_cell_float(response, s->internal_ina.bus_v, 3);
  if (!s->internal_ina.ok || isnan(s->internal_ina.current_mA)) {
    bq_web_print_cell(response, "nan");
  } else {
    snprintf(buf, sizeof(buf), "%.1f", s->internal_ina.current_mA);
    bq_web_print_cell(response, buf);
  }
  bq_web_print_cell_float(response, s->temp_c, 1);

  if (bq->ok) {
    bq_web_print_cell(response, bq_chrg_str(bq->r[0x08]));
    bq_web_print_cell(response, bq_fault_str(s->reg09b));
    snprintf(buf, sizeof(buf), "%u", bq_iinlim_ma(bq->r[0x00]));
    bq_web_print_cell(response, buf);
    snprintf(buf, sizeof(buf), "%u", bq_ichg_ma(bq->r[0x02]));
    bq_web_print_cell(response, buf);
    snprintf(buf, sizeof(buf), "PG=%u DPM=%u VSYS=%u", (bq->r[0x08] >> 2) & 1U, (bq->r[0x08] >> 3) & 1U, bq->r[0x08] & 1U);
    bq_web_print_cell(response, buf);
  } else {
    for (uint8_t i = 0; i < 5; i++) bq_web_print_cell(response, "unavailable");
  }

  snprintf(buf, sizeof(buf), "%lus", (unsigned long) ((millis() - bq_cached_read_ms[slot0]) / 1000UL));
  bq_web_print_cell(response, buf);
  response->print("</tr>");
}

struct StatusWebChunkState {
  uint8_t phase{0};
  uint8_t slot0{0};
  size_t offset{0};
  BqHtmlChunkBuffer chunk{1400};
};

inline bool status_web_build_next_chunk(StatusWebChunkState &state) {
  state.chunk.clear();

  if (state.phase == 0) {
    state.chunk.print("<!doctype html><html><head><meta charset=utf-8><title>MCC /status</title>");
    state.chunk.print("<style>body{font-family:sans-serif}.wrap{overflow-x:auto}table{border-collapse:collapse;white-space:nowrap}th,td{padding:2px 4px}.inline{display:inline;margin:0}button{font:inherit;padding:2px 6px}</style>");
    if (bq_web_read_busy || bq_web_pending_mask != 0 || pending_action_count() != 0) {
      state.chunk.print("<meta http-equiv=refresh content=\"2;url=/status\">");
    }
    state.chunk.print("</head><body><h3>MCC Pro status</h3>");
    state.chunk.print("<p><a href=/bq>Full /bq</a> | <a href=/c16>C16 raw</a></p>");
    state.chunk.print("<p><form class=inline method=get action=/status><button type=submit name=ra value=1>Read all</button></form></p>");
    char status[128];
    snprintf(status, sizeof(status), "<p>Busy: %s; pending reads: %u; pending BQ actions: %u</p>",
             yesno(bq_web_read_busy), pending_count(), pending_action_count());
    state.chunk.print(status);
    state.chunk.print("<div class=wrap><table border=1>");
    state.chunk.print("<tr><th>Slot</th><th>Int V</th><th>Int mA</th><th>Temp C</th><th>Charge</th><th>Fault</th><th>IIN mA</th><th>ICHG mA</th><th>Flags</th><th>Age</th></tr>");
    state.phase = 1;
    return true;
  }

  if (state.phase == 1) {
    if (state.slot0 < 16) {
      status_web_print_row(&state.chunk, state.slot0);
      state.slot0++;
      return true;
    }
    state.phase = 2;
  }

  if (state.phase == 2) {
    state.chunk.print("</table></div></body></html>");
    state.phase = 3;
    return true;
  }

  return false;
}

inline size_t status_web_copy_chunk(StatusWebChunkState &state, uint8_t *buffer, size_t max_len) {
  if (max_len == 0) return 0;

  while (state.offset >= state.chunk.length()) {
    state.offset = 0;
    if (!status_web_build_next_chunk(state)) return 0;
  }

  const size_t available = state.chunk.length() - state.offset;
  const size_t to_copy = available < max_len ? available : max_len;
  memcpy(buffer, state.chunk.c_str() + state.offset, to_copy);
  state.offset += to_copy;
  return to_copy;
}

inline void send_status_response(AsyncWebServerRequest *request) {
  ESP_LOGW(TAG_BQWEB, "Serving /status as chunked summary table.");
  StatusWebChunkState state;
  auto *response = request->beginChunkedResponse(
    "text/html",
    [state](uint8_t *buffer, size_t max_len, size_t index) mutable -> size_t {
      (void) index;
      return status_web_copy_chunk(state, buffer, max_len);
    }
  );
  request->send(response);
}


inline bool bq_write_charge_config(esphome::i2c::I2CDevice &dev, uint8_t chg_config, uint8_t &old_reg01, uint8_t &new_reg01) {
  if (!dev_read_reg8(dev, 0x01, old_reg01)) return false;
  new_reg01 = (old_reg01 & (uint8_t) ~0x30U) | (uint8_t) ((chg_config & 0x03U) << 4);
  return dev_write_reg8(dev, 0x01, new_reg01);
}

inline bool bq_clear_hiz(esphome::i2c::I2CDevice &dev, uint8_t &old_reg00, uint8_t &new_reg00) {
  if (!dev_read_reg8(dev, 0x00, old_reg00)) return false;
  new_reg00 = old_reg00 & 0x7FU;
  return dev_write_reg8(dev, 0x00, new_reg00);
}

inline bool bq_disable_watchdog(esphome::i2c::I2CDevice &dev, uint8_t &old_reg05, uint8_t &new_reg05) {
  if (!dev_read_reg8(dev, 0x05, old_reg05)) return false;
  new_reg05 = old_reg05 & 0xCFU;  // REG05[5:4] WATCHDOG = 00 disabled.
  return dev_write_reg8(dev, 0x05, new_reg05);
}

inline bool bq_set_input_current_limit(esphome::i2c::I2CDevice &dev, uint8_t iin_code,
                                       uint8_t &old_reg00, uint8_t &new_reg00) {
  if (!dev_read_reg8(dev, 0x00, old_reg00)) return false;
  new_reg00 = (old_reg00 & 0xF8U) | (iin_code & 0x07U);
  return dev_write_reg8(dev, 0x00, new_reg00);
}

inline bool bq_set_charge_current(esphome::i2c::I2CDevice &dev, uint8_t ichg_code,
                                  uint8_t &old_reg02, uint8_t &new_reg02) {
  if (!dev_read_reg8(dev, 0x02, old_reg02)) return false;
  new_reg02 = (old_reg02 & 0x03U) | (uint8_t) ((ichg_code & 0x3FU) << 2);
  return dev_write_reg8(dev, 0x02, new_reg02);
}

inline bool bq_reset_watchdog(esphome::i2c::I2CDevice &dev, uint8_t &old_reg01, uint8_t &new_reg01) {
  if (!dev_read_reg8(dev, 0x01, old_reg01)) return false;
  new_reg01 = old_reg01 | 0x40U;
  return dev_write_reg8(dev, 0x01, new_reg01);
}

inline bool execute_bq_action(uint8_t slot0, uint8_t action) {
  esphome::i2c::I2CDevice &dev = bq_dev_for_slot(slot0);
  bool ok = false;

  if (action == BQ_ACTION_CHARGE_ON) {
    uint8_t old_reg00 = 0;
    uint8_t new_reg00 = 0;
    uint8_t old_reg05 = 0;
    uint8_t new_reg05 = 0;
    uint8_t old_reg01 = 0;
    uint8_t new_reg01 = 0;
    const bool ok_hiz = bq_clear_hiz(dev, old_reg00, new_reg00);
    delay(2);
    const bool ok_wd = bq_disable_watchdog(dev, old_reg05, new_reg05);
    delay(2);
    const bool ok_chg = bq_write_charge_config(dev, 0x01, old_reg01, new_reg01);
    ok = ok_hiz && ok_wd && ok_chg;
    ESP_LOGW(TAG_BQWEB,
             "BQ action Charge ON C%02u: HIZ %s REG00 0x%02X->0x%02X, WD %s REG05 0x%02X->0x%02X, CHG %s REG01 0x%02X->0x%02X.",
             slot0 + 1, yesno(ok_hiz), old_reg00, new_reg00, yesno(ok_wd), old_reg05, new_reg05,
             yesno(ok_chg), old_reg01, new_reg01);
  } else if (action == BQ_ACTION_CHARGE_OFF) {
    uint8_t old_reg05 = 0;
    uint8_t new_reg05 = 0;
    uint8_t old_reg01 = 0;
    uint8_t new_reg01 = 0;
    const bool ok_wd = bq_disable_watchdog(dev, old_reg05, new_reg05);
    delay(2);
    const bool ok_chg = bq_write_charge_config(dev, 0x00, old_reg01, new_reg01);
    ok = ok_wd && ok_chg;
    ESP_LOGW(TAG_BQWEB, "BQ action Charge OFF C%02u: WD %s REG05 0x%02X->0x%02X, CHG %s REG01 0x%02X->0x%02X.",
             slot0 + 1, yesno(ok_wd), old_reg05, new_reg05, yesno(ok_chg), old_reg01, new_reg01);
  } else if (action == BQ_ACTION_WD_RESET) {
    uint8_t old_reg01 = 0;
    uint8_t new_reg01 = 0;
    ok = bq_reset_watchdog(dev, old_reg01, new_reg01);
    ESP_LOGW(TAG_BQWEB, "BQ action WD Reset C%02u: %s REG01 0x%02X->0x%02X.",
             slot0 + 1, yesno(ok), old_reg01, new_reg01);
  } else if (action >= BQ_ACTION_IIN_500 && action <= BQ_ACTION_IIN_3000) {
    uint8_t iin_code = 2;
    if (action == BQ_ACTION_IIN_900) iin_code = 3;
    if (action == BQ_ACTION_IIN_1500) iin_code = 5;
    if (action == BQ_ACTION_IIN_3000) iin_code = 7;
    uint8_t old_reg00 = 0;
    uint8_t new_reg00 = 0;
    ok = bq_set_input_current_limit(dev, iin_code, old_reg00, new_reg00);
    ESP_LOGW(TAG_BQWEB, "BQ action %s C%02u: %s REG00 0x%02X->0x%02X.",
             bq_action_label(action), slot0 + 1, yesno(ok), old_reg00, new_reg00);
  } else if (action >= BQ_ACTION_ICHG_512 && action <= BQ_ACTION_ICHG_3008) {
    uint8_t ichg_code = 0;
    if (action == BQ_ACTION_ICHG_1024) ichg_code = 8;
    if (action == BQ_ACTION_ICHG_2048) ichg_code = 24;
    if (action == BQ_ACTION_ICHG_3008) ichg_code = 39;
    uint8_t old_reg02 = 0;
    uint8_t new_reg02 = 0;
    ok = bq_set_charge_current(dev, ichg_code, old_reg02, new_reg02);
    ESP_LOGW(TAG_BQWEB, "BQ action %s C%02u: %s REG02 0x%02X->0x%02X.",
             bq_action_label(action), slot0 + 1, yesno(ok), old_reg02, new_reg02);
  }

  queue_slot(slot0);
  return ok;
}

inline bool process_one_bq_action() {
  for (uint8_t slot0 = 0; slot0 < 16; slot0++) {
    const uint8_t action = bq_web_pending_actions[slot0];
    if (action == BQ_ACTION_NONE) continue;

    bq_web_pending_actions[slot0] = BQ_ACTION_NONE;
    bq_web_pending_slot0 = slot0;
    bq_web_current_action = action;
    bq_web_read_busy = true;

    ESP_LOGW(TAG_BQWEB, "Processing queued BQ action %s for C%02u in main loop.",
             bq_action_label(action), slot0 + 1);
    const uint32_t started = millis();
    const bool ok = execute_bq_action(slot0, action);
    const uint32_t elapsed = millis() - started;

    bq_web_current_action = BQ_ACTION_NONE;
    bq_web_read_busy = false;
    ESP_LOGW(TAG_BQWEB, "Queued BQ action %s for C%02u complete: %s in %lums.",
             bq_action_label(action), slot0 + 1, yesno(ok), (unsigned long) elapsed);
    return true;
  }
  return false;
}


inline bool process_one_c16_hw_action() {
  const uint8_t action = c16_hw_pending_action;
  if (action == C16_HW_ACTION_NONE) return false;

  c16_hw_pending_action = C16_HW_ACTION_NONE;
  c16_hw_current_action = action;
  bq_web_pending_slot0 = SLOT0_C16;
  bq_web_read_busy = true;

  ESP_LOGW(TAG_BQWEB, "Processing queued C16 hardware action %s in main loop.", c16_hw_action_label(action));
  const uint32_t started = millis();
  bool ok = false;

  switch (action) {
    case C16_HW_ACTION_Q34_OFF:
      ok = c16_q34_set_pwm_permille(0);
      break;
    case C16_HW_ACTION_Q34_1P:
      ok = c16_q34_set_pwm_permille(10);
      break;
    case C16_HW_ACTION_Q34_5P:
      ok = c16_q34_set_pwm_permille(50);
      break;
    case C16_HW_ACTION_Q34_25P:
      ok = c16_q34_set_pwm_permille(250);
      break;
    case C16_HW_ACTION_Q34_100P:
      ok = c16_q34_set_pwm_permille(1000);
      break;
    case C16_HW_ACTION_Q34_PULSE_5P_1000MS:
      ok = c16_q34_set_pwm_permille(50);
      delay(1000);
      ok = c16_q34_set_pwm_permille(0) && ok;
      break;
    case C16_HW_ACTION_Q35_PROBE:
      // Q35 gate is driven by the LM393 path in the visible fragment, not by a PCA9685/BQ output.
      // Make the discharge path safe, then queue a C16 BQ read for correlated probing.
      ok = c16_q34_set_pwm_permille(0);
      queue_slot(SLOT0_C16);
      ESP_LOGW(TAG_BQWEB, "Q35 probe queued: Q34 forced OFF. Probe Q35 gate and C16_BAT_GND externally; no direct Q35 GPIO/I2C drive is visible in the schematic fragment.");
      break;
    case C16_HW_ACTION_HOLD_U2_INA:
      ok = c16_hold_hc4067_mux(Hc4067Mux::U2);
      break;
    case C16_HW_ACTION_HOLD_U3_INA:
      ok = c16_hold_hc4067_mux(Hc4067Mux::U3_INA219);
      break;
    case C16_HW_ACTION_HOLD_RELEASE:
      ok = c16_release_hc4067_hold();
      break;
    default:
      ok = false;
      break;
  }

  const uint32_t elapsed = millis() - started;
  c16_hw_current_action = C16_HW_ACTION_NONE;
  bq_web_read_busy = false;
  ESP_LOGW(TAG_BQWEB, "C16 hardware action %s complete: %s in %lums.",
           c16_hw_action_label(action), yesno(ok), (unsigned long) elapsed);
  return true;
}

inline bool process_one_auto_poll_slot() {
  if (auto_poll_pending_mask == 0 || bq_web_read_busy) return false;
  if (c16_hc4067_hold_active) {
    ESP_LOGW(TAG_BQWEB, "Auto poll deferred because C16 HC4067 hold is active: %s.", c16_hc4067_hold_label());
    return false;
  }

  uint8_t slot0 = 0;
  uint16_t mask = auto_poll_pending_mask;
  while (slot0 < 16 && ((mask & (1U << slot0)) == 0)) slot0++;
  if (slot0 >= 16) {
    auto_poll_pending_mask = 0;
    return false;
  }

  auto_poll_pending_mask &= (uint16_t) ~(1U << slot0);
  bq_web_pending_slot0 = slot0;
  bq_web_current_action = BQ_ACTION_NONE;
  bq_web_read_busy = true;

  const uint32_t started = millis();
  ESP_LOGW(TAG_BQWEB, "Processing lightweight auto poll for C%02u: internal TC1047 + internal INA219.", slot0 + 1);

  SlotSnapshot s;
  capture_tc1047_slot(slot0, s);
  capture_internal_ina219_slot(slot0, s);

  SlotSnapshot &cached = bq_cached_slots[slot0];
  cached.temp_ok = s.temp_ok;
  cached.temp_min_raw = s.temp_min_raw;
  cached.temp_max_raw = s.temp_max_raw;
  cached.temp_raw_avg = s.temp_raw_avg;
  cached.temp_v = s.temp_v;
  cached.temp_c = s.temp_c;
  cached.a0_raw = s.a0_raw;
  cached.internal_ina = s.internal_ina;
  cached.ina_ok = s.ina_ok;
  cached.ina_bus_raw = s.ina_bus_raw;
  cached.ina_shunt_raw = s.ina_shunt_raw;
  cached.bus_v = s.bus_v;
  cached.shunt_mv = s.shunt_mv;

  bq_cached_valid[slot0] = true;
  bq_cached_read_ms[slot0] = millis();

  publish_temp_for_slot(slot0, s.temp_c);
  publish_ina_slot_measurements(slot0, s);

  const uint32_t elapsed = millis() - started;
  bq_web_read_busy = false;
  ESP_LOGW(TAG_BQWEB, "Lightweight auto poll complete for C%02u in %lums.", slot0 + 1, (unsigned long) elapsed);
  return true;
}

inline void process_bq_web_read_request() {
  if (bq_web_read_busy) return;
  if (process_one_c16_hw_action()) return;
  if (process_one_bq_action()) return;
  if (bq_web_pending_mask == 0) {
    process_one_auto_poll_slot();
    return;
  }

  uint8_t slot0 = 0;
  uint16_t mask = bq_web_pending_mask;
  while (slot0 < 16 && ((mask & (1U << slot0)) == 0)) slot0++;
  if (slot0 >= 16) {
    bq_web_pending_mask = 0;
    return;
  }

  bq_web_pending_mask &= (uint16_t) ~(1U << slot0);
  bq_web_pending_slot0 = slot0;
  bq_web_current_action = BQ_ACTION_NONE;
  bq_web_read_busy = true;

  ESP_LOGW(TAG_BQWEB, "Processing queued native slot read for C%02u in main loop.", slot0 + 1);
  const uint32_t started = millis();

  SlotSnapshot s;
  BQFullRegs bq;
  capture_slot_full(slot0, s, bq);

  bq_cached_slots[slot0] = s;
  bq_cached_regs[slot0] = bq;
  bq_cached_valid[slot0] = true;
  bq_cached_read_ms[slot0] = millis();

  publish_ina_slot_measurements(slot0, s);
  publish_bq_slot_measurements(slot0, s, bq);

  log_bq_line(slot0, s, bq, "native_slot");

  const uint32_t elapsed = millis() - started;
  if (elapsed > 1500UL) {
    bq_web_pending_mask = 0;
    ESP_LOGE(TAG_BQWEB, "Slot read for C%02u took %lums. Cleared remaining queue to protect ESP8266.",
             slot0 + 1, (unsigned long) elapsed);
  }

  bq_web_read_busy = false;
  ESP_LOGW(TAG_BQWEB, "Queued native slot read complete for C%02u in %lums.", slot0 + 1, (unsigned long) elapsed);
}


inline uint8_t c16_hw_action_from_request(AsyncWebServerRequest *request) {
  if (!request->hasParam("c16hw")) return C16_HW_ACTION_NONE;
  const String action = request->getParam("c16hw")->value();
  if (action == "q34_off") return C16_HW_ACTION_Q34_OFF;
  if (action == "q34_1") return C16_HW_ACTION_Q34_1P;
  if (action == "q34_5") return C16_HW_ACTION_Q34_5P;
  if (action == "q34_25") return C16_HW_ACTION_Q34_25P;
  if (action == "q34_100") return C16_HW_ACTION_Q34_100P;
  if (action == "q34_pulse5") return C16_HW_ACTION_Q34_PULSE_5P_1000MS;
  if (action == "q35_probe") return C16_HW_ACTION_Q35_PROBE;
  if (action == "hold_u2_ina") return C16_HW_ACTION_HOLD_U2_INA;
  if (action == "hold_u3_ina") return C16_HW_ACTION_HOLD_U3_INA;
  if (action == "hold_release") return C16_HW_ACTION_HOLD_RELEASE;
  return 0xFF;
}

inline uint8_t bq_web_action_from_request(AsyncWebServerRequest *request) {
  if (!request->hasParam("action") && !request->hasParam("a")) return BQ_ACTION_NONE;
  const AsyncWebParameter *param = request->hasParam("action") ? request->getParam("action") : request->getParam("a");
  String action = param->value();
  if (action == "" || action == "none" || action == "noop") return BQ_ACTION_NONE;
  if (action == "charge_on" || action == "1") return BQ_ACTION_CHARGE_ON;
  if (action == "charge_off" || action == "0") return BQ_ACTION_CHARGE_OFF;
  if (action == "wd_reset" || action == "w") return BQ_ACTION_WD_RESET;
  if (action == "iin_500" || action == "i2") return BQ_ACTION_IIN_500;
  if (action == "iin_900" || action == "i3") return BQ_ACTION_IIN_900;
  if (action == "iin_1500" || action == "i5") return BQ_ACTION_IIN_1500;
  if (action == "iin_3000" || action == "i7") return BQ_ACTION_IIN_3000;
  if (action == "ichg_512" || action == "c0") return BQ_ACTION_ICHG_512;
  if (action == "ichg_1024" || action == "c8") return BQ_ACTION_ICHG_1024;
  if (action == "ichg_2048" || action == "c24") return BQ_ACTION_ICHG_2048;
  if (action == "ichg_3008" || action == "c39") return BQ_ACTION_ICHG_3008;
  return 0xFF;
}

inline int8_t bq_web_slot_from_request(AsyncWebServerRequest *request) {
  if (!request->hasParam("slot") && !request->hasParam("s")) return -1;
  const AsyncWebParameter *param = request->hasParam("slot") ? request->getParam("slot") : request->getParam("s");
  int slot = param->value().toInt();
  if (slot < 1 || slot > 16) return -2;
  return (int8_t) (slot - 1);
}


template<typename TResponse>
inline void c16_web_print_row(TResponse *response, const char *block, const char *item,
                              const char *raw, const char *decoded, const char *note) {
  response->print("<tr><td>");
  response->print(block != nullptr ? block : "");
  response->print("</td><td>");
  response->print(item != nullptr ? item : "");
  response->print("</td><td><code>");
  response->print(raw != nullptr ? raw : "");
  response->print("</code></td><td>");
  response->print(decoded != nullptr ? decoded : "");
  response->print("</td><td>");
  response->print(note != nullptr ? note : "");
  response->print("</td></tr>");
}

inline void c16_float_to_buf(char *buf, size_t len, float value, uint8_t decimals) {
  if (buf == nullptr || len == 0) return;
  if (isnan(value)) {
    snprintf(buf, len, "nan");
  } else if (decimals == 4) {
    snprintf(buf, len, "%.4f", value);
  } else if (decimals == 3) {
    snprintf(buf, len, "%.3f", value);
  } else if (decimals == 1) {
    snprintf(buf, len, "%.1f", value);
  } else {
    snprintf(buf, len, "%.2f", value);
  }
}

template<typename TResponse>
inline void c16_web_print_u8_reg_row(TResponse *response, const char *block, const char *name,
                                     uint8_t reg, const char *decoded) {
  char raw[16];
  snprintf(raw, sizeof(raw), "0x%02X", reg);
  c16_web_print_row(response, block, name, raw, decoded != nullptr ? decoded : "", "BQ24195 raw 8-bit register");
}

template<typename TResponse>
inline void c16_web_print_adc_row(TResponse *response, const char *block, const char *name,
                                  const Hc4067AdcSnapshot &snap, float temp_c, const char *note) {
  char raw[80];
  char decoded[96];
  char vbuf[20];
  char tbuf[20];
  c16_float_to_buf(vbuf, sizeof(vbuf), snap.v, 4);
  c16_float_to_buf(tbuf, sizeof(tbuf), temp_c, 1);
  snprintf(raw, sizeof(raw), "ok=%u avg=%.2f min=%u max=%u", snap.ok ? 1U : 0U, snap.raw_avg, snap.min_raw, snap.max_raw);
  snprintf(decoded, sizeof(decoded), "V=%s temp=%s C", vbuf, tbuf);
  c16_web_print_row(response, block, name, raw, decoded, note);
}

template<typename TResponse>
inline void c16_web_print_ina_reg_rows(TResponse *response, const char *block,
                                       const InaSnapshot &ina, uint8_t addr, Hc4067Mux mux,
                                       float shunt_mohm, const char *note) {
  char raw[96];
  char decoded[112];
  const char *reg_names[6] = {"CONFIG", "SHUNT", "BUS", "POWER", "CURRENT", "CAL"};
  for (uint8_t reg = 0; reg <= 5; reg++) {
    snprintf(raw, sizeof(raw), "reg0x%02X ok=%u value=0x%04X", reg, ina.reg_ok[reg] ? 1U : 0U, ina.reg[reg]);
    if (reg == 0x01) {
      snprintf(decoded, sizeof(decoded), "shunt_raw_signed=%d shunt=%.2f mV", ina.shunt_raw, ina.shunt_mv);
    } else if (reg == 0x02) {
      snprintf(decoded, sizeof(decoded), "bus_raw=0x%04X bus=%.3f V CNVR=%u OVF=%u", ina.bus_raw, ina.bus_v,
               (ina.bus_raw & 0x0002U) ? 1U : 0U, (ina.bus_raw & 0x0001U) ? 1U : 0U);
    } else if (reg == 0x00) {
      snprintf(decoded, sizeof(decoded), "expected config after write: 0x399F");
    } else if (reg == 0x05) {
      snprintf(decoded, sizeof(decoded), "calibration register; helper does not use INA current/power calibration");
    } else {
      snprintf(decoded, sizeof(decoded), "raw INA219 register");
    }
    c16_web_print_row(response, block, reg_names[reg], raw, decoded, note);
  }

  char summary_raw[112];
  char summary_decoded[128];
  const bool summary_u2_u34_pair = (mux == Hc4067Mux::U2);
  const uint8_t summary_mask = summary_u2_u34_pair ? PCF_U2_U34_INTERNAL_INA_ENABLE : pcf_mask_for_hc4067(mux);
  const char *summary_mux = summary_u2_u34_pair ? "U2+U34/int-INA" : hc4067_mux_name(mux);
  snprintf(summary_raw, sizeof(summary_raw), "addr=0x%02X mux=%s pcf_mask=0x%02X ch=%u ok=%u", addr, summary_mux, summary_mask, SLOT0_C16, ina.ok ? 1U : 0U);
  snprintf(summary_decoded, sizeof(summary_decoded), "bus=%.3f V shunt=%.2f mV current=%.1f mA @ %.1f mOhm", ina.bus_v, ina.shunt_mv, ina.current_mA, shunt_mohm);
  c16_web_print_row(response, block, "summary", summary_raw, summary_decoded, note);
}

template<typename TResponse>
inline void c16_web_print_bq_regs(TResponse *response, const BQFullRegs &bq, const SlotSnapshot &s) {
  char decoded[112];
  for (uint8_t reg = 0; reg <= 0x0A; reg++) {
    decoded[0] = '\0';
    if (reg == 0x00) {
      snprintf(decoded, sizeof(decoded), "HIZ=%u IIN=%s", (bq.r[reg] >> 7) & 1U, bq_iinlim_str(bq.r[reg]));
    } else if (reg == 0x01) {
      snprintf(decoded, sizeof(decoded), "CHG_CONFIG=%u SYS_MIN_code=%u WD_RESET=%u", (bq.r[reg] >> 4) & 3U, (bq.r[reg] >> 1) & 7U, (bq.r[reg] >> 6) & 1U);
    } else if (reg == 0x02) {
      snprintf(decoded, sizeof(decoded), "ICHG=%u mA FORCE_20PCT=%u", bq_ichg_ma(bq.r[reg]), bq.r[reg] & 1U);
    } else if (reg == 0x05) {
      snprintf(decoded, sizeof(decoded), "watchdog=%s safety_timer_en=%u", bq_watchdog_str(bq.r[reg]), (bq.r[reg] >> 3) & 1U);
    } else if (reg == 0x08) {
      snprintf(decoded, sizeof(decoded), "charge=%s DPM=%u PG=%u VSYS=%u", bq_chrg_str(bq.r[reg]), (bq.r[reg] >> 3) & 1U, (bq.r[reg] >> 2) & 1U, bq.r[reg] & 1U);
    } else if (reg == 0x09) {
      snprintf(decoded, sizeof(decoded), "fault_first=%s; second_read=0x%02X/%s", bq_fault_str(bq.r[reg]), s.reg09b, bq_fault_str(s.reg09b));
    } else if (reg == 0x0A) {
      snprintf(decoded, sizeof(decoded), "part=%u revision=%u", (bq.r[reg] >> 3) & 7U, bq.r[reg] & 7U);
    }
    char name[12];
    snprintf(name, sizeof(name), "REG%02X", reg);
    c16_web_print_u8_reg_row(response, "BQ24195 C16", name, bq.r[reg], decoded);
  }
}


struct C16WebChunkState {
  uint8_t phase{0};
  size_t offset{0};
  BqHtmlChunkBuffer chunk{1400};
};

inline void c16_web_print_controls(BqHtmlChunkBuffer *response) {
  response->print("<!doctype html><html><head><meta charset=utf-8><title>MCC /c16 raw</title>");
  response->print("<style>body{font-family:sans-serif}table{border-collapse:collapse;white-space:nowrap}th,td{padding:3px 6px;border:1px solid #999;vertical-align:top}.inline{display:inline;margin:0}code{font-family:monospace}th{background:#eee}.warn{color:#a00;font-weight:bold}button{font:inherit;padding:2px 6px}</style>");
  if (bq_web_read_busy || (bq_web_pending_mask & (1U << SLOT0_C16)) != 0 || c16_hw_pending_action != C16_HW_ACTION_NONE) {
    response->print("<meta http-equiv=refresh content=\"2;url=/c16\">");
  }
  response->print("</head><body><h3>MCC Pro C16 raw diagnostics</h3>");
  response->print("<p><a href=/bq>Back to /bq</a></p>");
  response->print("<form class=inline method=get action=/c16><button type=submit name=read value=1>Queue C16 read</button></form>");
  response->print(" <form class=inline method=get action=/bq><input type=hidden name=s value=16><button type=submit>Queue C16 from /bq</button></form>");
  response->print("<h4>C16 INA219 mux hold for manual probing</h4><p>");
  response->print("<form class=inline method=get action=/c16><button type=submit name=c16hw value=hold_u2_ina>Hold U2+U34/internal INA C16</button></form> " );
  response->print("<form class=inline method=get action=/c16><button type=submit name=c16hw value=hold_u3_ina>Hold U3/external INA C16</button></form> " );
  response->print("<form class=inline method=get action=/c16><button type=submit name=c16hw value=hold_release>Release mux hold</button></form>");
  response->print("</p><p><small>Hold mode sets S0..S3 to C16 and keeps U2+U34 for internal INA or U3 for external INA enabled. Release hold before normal diagnostics.</small></p>");

  char status[288];
  snprintf(status, sizeof(status), "<p>Busy: %s; C16 pending read: %s; pending reads: %u; pending BQ actions: %u; pending C16 HW: %s; mux hold: %s</p>",
           yesno(bq_web_read_busy), yesno((bq_web_pending_mask & (1U << SLOT0_C16)) != 0), pending_count(), pending_action_count(), c16_hw_action_label(c16_hw_pending_action), c16_hc4067_hold_label());
  response->print(status);
}

inline bool c16_web_build_next_chunk(C16WebChunkState &state) {
  state.chunk.clear();

  if (state.phase == 0) {
    c16_web_print_controls(&state.chunk);
    if (!bq_cached_valid[SLOT0_C16]) {
      state.chunk.print("<p class=warn>No cached C16 snapshot yet. Press <b>Queue C16 read</b>, wait for refresh, then compare raw values.</p>");
      state.chunk.print("</body></html>");
      state.phase = 255;
      return true;
    }
    state.chunk.print("<table><tr><th>Block</th><th>Item</th><th>Raw</th><th>Decoded</th><th>Notes</th></tr>");
    state.phase = 1;
    return true;
  }

  if (!bq_cached_valid[SLOT0_C16]) return false;

  const SlotSnapshot &s = bq_cached_slots[SLOT0_C16];
  const BQFullRegs &bq = bq_cached_regs[SLOT0_C16];
  char raw[112];
  char decoded[192];

  if (state.phase == 1) {
    snprintf(raw, sizeof(raw), "slot0=%u age_ms=%lu", SLOT0_C16, (unsigned long) (millis() - bq_cached_read_ms[SLOT0_C16]));
    snprintf(decoded, sizeof(decoded), "TCA addr=0x%02X ch=%u mask=0x%02X bus=%s", tca_addr_for_slot(SLOT0_C16), tca_channel_for_slot(SLOT0_C16), tca_mask_for_slot(SLOT0_C16), bus_id_for_slot(SLOT0_C16));
    c16_web_print_row(&state.chunk, "meta", "route/cache", raw, decoded, "C16 is zero-based slot 15 / displayed slot C16");
    snprintf(raw, sizeof(raw), "PCF idle=0x%02X ch_select=%u", PCF_SAFE_IDLE, SLOT0_C16);
    snprintf(decoded, sizeof(decoded), "U10=0x%02X U10E=0x%02X U34=0x%02X U3=0x%02X U2=0x%02X U2+U34=0x%02X", PCF_U10_TC1047_MUX_ENABLE, PCF_U10E_EXTERNAL_TEMP_ENABLE, PCF_U34_MUX_ENABLE, PCF_U3_EXTERNAL_INA_ENABLE, PCF_U2_INTERNAL_INA_ENABLE, PCF_U2_U34_INTERNAL_INA_ENABLE);
    c16_web_print_row(&state.chunk, "HC4067", "PCF masks", raw, decoded, "All HC4067 enables are active-low and written as full PCF8574 byte masks");
    snprintf(raw, sizeof(raw), "active=%s mux=%u ch=%u", yesno(c16_hc4067_hold_active), c16_hc4067_hold_mux, c16_hc4067_hold_channel);
    snprintf(decoded, sizeof(decoded), "%s", c16_hc4067_hold_label());
    c16_web_print_row(&state.chunk, "HC4067", "manual hold", raw, decoded, "Hold keeps selected HC4067 mux enabled for DMM/scope probing of U2/U3 COM and INA219 VIN pins");
    state.phase = 2;
    return true;
  }

  if (state.phase == 2) {
    Hc4067AdcSnapshot temp_int;
    temp_int.ok = s.temp_ok;
    temp_int.min_raw = s.temp_min_raw;
    temp_int.max_raw = s.temp_max_raw;
    temp_int.raw_avg = s.temp_raw_avg;
    temp_int.v = s.temp_v;
    c16_web_print_adc_row(&state.chunk, "ADC U10", "internal TC1047", temp_int, s.temp_c, "Internal cell temperature path: U10/P0 LOW -> A0");
    c16_web_print_adc_row(&state.chunk, "ADC U10E", "external temp", s.u10e, s.external_temp_c, "External connector temperature path: U10E/P1 LOW -> A0");
    state.phase = 3;
    return true;
  }

  if (state.phase == 3) {
    c16_web_print_ina_reg_rows(&state.chunk, "INA219 internal", s.internal_ina, ADDR_INA219_INTERNAL_U2, Hc4067Mux::U2,
                               MCC_INA219_INTERNAL_SHUNT_MOHM, "Internal slot voltage/current: U2/P4 LOW -> INA219 #2 @ 0x4F");
    state.phase = 4;
    return true;
  }

  if (state.phase == 4) {
    c16_web_print_ina_reg_rows(&state.chunk, "INA219 external", s.external_ina, ADDR_INA219_EXTERNAL_U3, Hc4067Mux::U3_INA219,
                               MCC_INA219_EXTERNAL_SHUNT_MOHM, "External connector voltage/current: U3/P3 LOW -> INA219 #1 @ 0x41");
    state.phase = 5;
    return true;
  }

  if (state.phase == 5) {
    c16_web_print_bq_regs(&state.chunk, bq, s);
    state.phase = 6;
    return true;
  }

  if (state.phase == 6) {
    state.chunk.print("</table>");
    state.chunk.print("<p>Interpretation anchor: INA219 bus voltage is VIN- to GND; shunt voltage is VIN+ - VIN-. If bus voltage differs from battery terminal voltage, probe VIN- physically.</p>");
    state.chunk.print("</body></html>");
    state.phase = 255;
    return true;
  }

  return false;
}

inline size_t c16_web_copy_chunk(C16WebChunkState &state, uint8_t *buffer, size_t max_len) {
  if (max_len == 0) return 0;

  while (state.offset >= state.chunk.length()) {
    state.offset = 0;
    if (!c16_web_build_next_chunk(state)) return 0;
  }

  const size_t available = state.chunk.length() - state.offset;
  const size_t to_copy = available < max_len ? available : max_len;
  memcpy(buffer, state.chunk.c_str() + state.offset, to_copy);
  state.offset += to_copy;
  return to_copy;
}

inline void send_c16_raw_response(AsyncWebServerRequest *request) {
  ESP_LOGW(TAG_BQWEB, "Serving /c16 as chunked raw diagnostics; no whole-page RAM buffer.");

  C16WebChunkState state;
  auto *response = request->beginChunkedResponse(
    "text/html",
    [state](uint8_t *buffer, size_t max_len, size_t index) mutable -> size_t {
      (void) index;
      return c16_web_copy_chunk(state, buffer, max_len);
    }
  );

  request->send(response);
  ESP_LOGW(TAG_BQWEB, "Serving /c16 chunked response started.");
}

class C16RawHandler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) const override {
    return request->method() == HTTP_GET && (request->url() == "/c16" || request->url() == "/c16/");
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    ESP_LOGW(TAG_BQWEB, "Handling /c16 request: url=%s", request->url().c_str());
    if (request->hasParam("c16hw")) {
      const uint8_t hw_action = c16_hw_action_from_request(request);
      if (hw_action == 0xFF || hw_action == C16_HW_ACTION_NONE) {
        request->send(400, "text/plain", "invalid or missing C16 hardware action");
        return;
      }
      queue_c16_hw_action(hw_action);
      request->redirect("/c16");
      return;
    }
    if (request->hasParam("read")) {
      queue_slot(SLOT0_C16);
      request->redirect("/c16");
      return;
    }
    send_c16_raw_response(request);
  }
};

class StatusHandler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) const override {
    return request->method() == HTTP_GET && (request->url() == "/status" || request->url() == "/status/");
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    if (request->hasParam("read_all") || request->hasParam("ra")) {
      queue_all_slots();
      request->redirect("/status");
      return;
    }
    send_status_response(request);
  }
};

class BqTableHandler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) const override {
    return request->method() == HTTP_GET && request->url() == "/bq";
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    char notice[96] = {0};
    bool command_queued = false;

    if (request->hasParam("clear")) {
      clear_pending_queue();
      snprintf(notice, sizeof(notice), "Queue cleared.");
      command_queued = true;
    }

    if (request->hasParam("c16hw")) {
      const uint8_t hw_action = c16_hw_action_from_request(request);
      if (hw_action == 0xFF) {
        request->send(400, "text/plain", "invalid C16 hardware action");
        return;
      }
      if (hw_action == C16_HW_ACTION_NONE) {
        request->send(400, "text/plain", "missing C16 hardware action");
        return;
      }
      queue_c16_hw_action(hw_action);
      snprintf(notice, sizeof(notice), "Queued %s.", c16_hw_action_label(hw_action));
      command_queued = true;
    }

    if (request->hasParam("read_all") || request->hasParam("ra")) {
      queue_all_slots();
      snprintf(notice, sizeof(notice), "Queued read_all for C01..C16.");
      command_queued = true;
    } else if (request->hasParam("all")) {
      const uint8_t action = bq_web_action_from_request(request);
      if (action == 0xFF) {
        request->send(400, "text/plain", "invalid action");
        return;
      }
      if (action == BQ_ACTION_NONE) {
        request->send(400, "text/plain", "missing bulk action");
        return;
      }
      queue_bq_action_all(action);
      snprintf(notice, sizeof(notice), "Queued %s for all slots C01..C16.", bq_action_label(action));
      command_queued = true;
    } else {
      int8_t slot0 = bq_web_slot_from_request(request);
      if (slot0 == -2) {
        request->send(400, "text/plain", "slot must be 1..16");
        return;
      }
      if (slot0 >= 0) {
        const uint8_t action = bq_web_action_from_request(request);
        if (action == 0xFF) {
          request->send(400, "text/plain", "invalid action");
          return;
        }
        if (action != BQ_ACTION_NONE) {
          queue_bq_action((uint8_t) slot0, action);
          snprintf(notice, sizeof(notice), "Queued %s for C%02u.", bq_action_label(action),
                   static_cast<unsigned>(slot0 + 1));
          command_queued = true;
        } else {
          queue_slot((uint8_t) slot0);
          snprintf(notice, sizeof(notice), "Queued read for C%02u.", static_cast<unsigned>(slot0 + 1));
          ESP_LOGW(TAG_BQWEB, "Queued /bq native slot read request for C%02u.", slot0 + 1);
          command_queued = true;
        }
      }
    }

    if (command_queued) {
      request->redirect("/bq");
      return;
    }

    send_bq_table_response(request, notice);
  }
};

inline void setup_bq_web_handler() {
  static bool registered = false;
  static bool reported_missing = false;
  if (registered) return;

  auto *base = esphome::web_server_base::global_web_server_base;
  if (base == nullptr) {
    if (!reported_missing) {
      ESP_LOGW(TAG_BQWEB, "Cannot register /bq yet: web_server_base is not available.");
      reported_missing = true;
    }
    return;
  }

  base->add_handler(new BqTableHandler());  // NOLINT(cppcoreguidelines-owning-memory)
  base->add_handler(new C16RawHandler());  // NOLINT(cppcoreguidelines-owning-memory)
  base->add_handler(new StatusHandler());  // NOLINT(cppcoreguidelines-owning-memory)
  registered = true;
  ESP_LOGW(TAG_BQWEB, "Registered native ESPHome BQ endpoints at /bq, /c16 and /status.");
}

inline void dump_bq_c16_u38() {
  queue_slot(SLOT0_C16);
  ESP_LOGW(TAG_BQFULL, "Queued C16 diagnostics via native ESPHome TCA9548A/i2c_device path.");
}

}  // namespace mccdiag

#pragma once
#include "esphome.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

namespace mccdiag {

// MCC Pro inventory status:
// Identified in repo/docs: 16x TC1047, 16x BQ24195, 2x TCA9548A,
// 2x INA219B, 1x PCF8574, 1x PCA9685, 1x SSD1306 OLED, 1x U10 74HC4067
// plus additional power/analog ICs still documented but not actively driven here.
// Actively handled by this lean ESP8266 build: TC1047 via U10/PCF8574,
// BQ24195 via TCA0/TCA1, U47 INA219 diagnostics, SSD1306 OLED and /bq web table.

static constexpr uint8_t PIN_MUX_S0 = 13;
static constexpr uint8_t PIN_MUX_S1 = 12;
static constexpr uint8_t PIN_MUX_S2 = 14;
static constexpr uint8_t PIN_MUX_S3 = 16;
static constexpr uint8_t PIN_I2C_SDA = 4;
static constexpr uint8_t PIN_I2C_SCL = 5;

static constexpr uint8_t ADDR_PCF8574 = 0x27;
static constexpr uint8_t ADDR_OLED    = 0x3C;
static constexpr uint8_t ADDR_INA219_U47 = 0x41;  // U47: A0=HIGH, A1=LOW.
static constexpr uint8_t ADDR_INA219_U34 = 0x45;  // U34: A0=HIGH, A1=HIGH; slot/path still being traced.
static constexpr uint8_t ADDR_INA219 = ADDR_INA219_U47;  // Current tested diagnostic path.
static constexpr uint8_t ADDR_PCA9685 = 0x4F;
static constexpr uint8_t ADDR_TCA0    = 0x70;  // C1..C8 side, exact PCB ref still being traced.
static constexpr uint8_t ADDR_TCA1    = 0x71;  // U38: A0=5V, A1=GND, A2=GND; C9..C16 side.
static constexpr uint8_t ADDR_BQ24195 = 0x6B;

// PCF8574 is not register-based. Reads return physical pin levels, not a safe
// output latch snapshot. Keep a conservative output policy: all pins high when
// idle, only P0 low while enabling the TC1047 analog mux.
static constexpr uint8_t PCF_SAFE_IDLE = 0xFF;
static constexpr uint8_t PCF_TC1047_MUX_ENABLE = 0xFE;

static constexpr uint8_t SLOT0_C16 = 15;

static const char *TAG_BQWEB = "mcc_bqweb";
static const char *TAG_BQFULL = "mcc_bqfull";

#ifndef MCC_TC1047_ADC_FULL_SCALE_V
#define MCC_TC1047_ADC_FULL_SCALE_V 3.02f
#endif

#ifndef MCC_TC1047_TEMP_OFFSET_C
#define MCC_TC1047_TEMP_OFFSET_C 0.0f
#endif

struct SlotSnapshot {
  bool bq_ok{false};
  uint8_t reg00{0};
  uint8_t reg08{0};
  uint8_t reg09a{0};
  uint8_t reg09b{0};
  uint8_t reg0a{0};
  bool ina_ok{false};
  uint16_t ina_bus_raw{0};
  int16_t ina_shunt_raw{0};
  int a0_raw{-1};
  bool temp_ok{false};
  uint16_t temp_min_raw{0};
  uint16_t temp_max_raw{0};
  float temp_raw_avg{NAN};
  float temp_v{NAN};
  float temp_c{NAN};
  float bus_v{NAN};
  float shunt_mv{NAN};
};

struct BqRoute {
  uint8_t tca_addr;
  uint8_t tca_channel;
  const char *ref;
};

static constexpr BqRoute BQ_ROUTES[16] = {
  {ADDR_TCA0, 0, "TCA0_C01_C08"},
  {ADDR_TCA0, 1, "TCA0_C01_C08"},
  {ADDR_TCA0, 2, "TCA0_C01_C08"},
  {ADDR_TCA0, 3, "TCA0_C01_C08"},
  {ADDR_TCA0, 4, "TCA0_C01_C08"},
  {ADDR_TCA0, 5, "TCA0_C01_C08"},
  {ADDR_TCA0, 6, "TCA0_C01_C08"},
  {ADDR_TCA0, 7, "TCA0_C01_C08"},
  {ADDR_TCA1, 0, "U38_C09_C16"},
  {ADDR_TCA1, 1, "U38_C09_C16"},
  {ADDR_TCA1, 2, "U38_C09_C16"},
  {ADDR_TCA1, 3, "U38_C09_C16"},
  {ADDR_TCA1, 4, "U38_C09_C16"},
  {ADDR_TCA1, 5, "U38_C09_C16"},
  {ADDR_TCA1, 6, "U38_C09_C16"},
  {ADDR_TCA1, 7, "U38_C09_C16"},  // Confirmed: U38 SC7/SD7 route to C16 BQ24195.
};

inline void feed() {
  yield();
}

inline void configure_wire_runtime_limits() {
#if defined(ESP8266)
  // Keep one bad downstream branch from blocking the ESP8266 for tens of seconds.
  Wire.setClock(50000);
  Wire.setClockStretchLimit(50000);
#endif
}

inline bool i2c_present(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

inline bool i2c_write_u8(uint8_t addr, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

inline bool i2c_write_reg16(uint8_t addr, uint8_t reg, uint16_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write((uint8_t) ((value >> 8) & 0xFF));
  Wire.write((uint8_t) (value & 0xFF));
  return Wire.endTransmission() == 0;
}

inline bool i2c_read_u8(uint8_t addr, uint8_t &value) {
  uint8_t n = Wire.requestFrom((int) addr, 1);
  if (n != 1 || !Wire.available()) return false;
  value = Wire.read();
  return true;
}

inline bool i2c_read_reg8(uint8_t addr, uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t n = Wire.requestFrom((int) addr, 1);
  if (n != 1 || !Wire.available()) return false;
  value = Wire.read();
  return true;
}

inline bool i2c_read_reg16(uint8_t addr, uint8_t reg, uint16_t &value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t n = Wire.requestFrom((int) addr, 2);
  if (n != 2 || Wire.available() < 2) return false;
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  value = ((uint16_t) msb << 8) | lsb;
  return true;
}

inline const char *yesno(bool v) { return v ? "YES" : "NO"; }

inline void recover_i2c_bus(const char *reason) {
#if defined(ESP8266)
  ESP_LOGW(TAG_BQWEB, "I2C recovery start: %s SDA=%d SCL=%d",
           reason ? reason : "?", digitalRead(PIN_I2C_SDA), digitalRead(PIN_I2C_SCL));

  // If a selected TCA branch left a slave holding SDA low, manually clock SCL.
  // This is cheap and often releases a slave stuck mid-byte.
  pinMode(PIN_I2C_SDA, INPUT_PULLUP);
  pinMode(PIN_I2C_SCL, INPUT_PULLUP);
  delay(1);

  for (uint8_t i = 0; i < 18 && digitalRead(PIN_I2C_SDA) == LOW; i++) {
    pinMode(PIN_I2C_SCL, OUTPUT);
    digitalWrite(PIN_I2C_SCL, LOW);
    delayMicroseconds(10);
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);
    delayMicroseconds(10);
    yield();
  }

  // Generate a STOP condition: SDA low while SCL high, then release SDA.
  pinMode(PIN_I2C_SDA, OUTPUT);
  digitalWrite(PIN_I2C_SDA, LOW);
  delayMicroseconds(10);
  pinMode(PIN_I2C_SCL, INPUT_PULLUP);
  delayMicroseconds(10);
  pinMode(PIN_I2C_SDA, INPUT_PULLUP);
  delayMicroseconds(10);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  configure_wire_runtime_limits();
  delay(2);

  // Try to leave both muxes disconnected. Do not call disable_tcas() here:
  // recovery must not recurse if the bus is still sick.
  bool tca0_off = i2c_write_u8(ADDR_TCA0, 0x00);
  bool tca1_off = i2c_write_u8(ADDR_TCA1, 0x00);
  delay(2);

  ESP_LOGW(TAG_BQWEB, "I2C recovery done: SDA=%d SCL=%d TCA0_off=%s TCA1_off=%s",
           digitalRead(PIN_I2C_SDA), digitalRead(PIN_I2C_SCL),
           yesno(tca0_off), yesno(tca1_off));
#else
  (void) reason;
#endif
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

  configure_wire_runtime_limits();
  // Safe idle for PCF8574-controlled lines. Do not read-modify-write PCF8574.
  i2c_write_u8(ADDR_PCF8574, PCF_SAFE_IDLE);
}

inline void mux_select(uint8_t ch) {
  ch &= 0x0F;
  digitalWrite(PIN_MUX_S0, (ch & 0x01) ? HIGH : LOW);
  digitalWrite(PIN_MUX_S1, (ch & 0x02) ? HIGH : LOW);
  digitalWrite(PIN_MUX_S2, (ch & 0x04) ? HIGH : LOW);
  digitalWrite(PIN_MUX_S3, (ch & 0x08) ? HIGH : LOW);
  delay(3);
}

inline int adc_avg(uint8_t samples = 8) {
  long sum = 0;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(A0);
    delay(2);
  }
  return (int) lroundf((float) sum / (float) samples);
}

inline bool disable_tcas() {
  bool ok0 = i2c_write_u8(ADDR_TCA0, 0x00);
  bool ok1 = i2c_write_u8(ADDR_TCA1, 0x00);
  delay(2);

  if (!(ok0 && ok1)) {
    ESP_LOGW(TAG_BQWEB, "disable_tcas failed: TCA0=%s TCA1=%s; attempting I2C recovery.",
             yesno(ok0), yesno(ok1));
    recover_i2c_bus("disable_tcas failed");
    ok0 = i2c_write_u8(ADDR_TCA0, 0x00);
    ok1 = i2c_write_u8(ADDR_TCA1, 0x00);
    delay(2);
  }

  return ok0 && ok1;
}

inline const BqRoute &bq_route_for_slot(uint8_t slot0) {
  return BQ_ROUTES[slot0 & 0x0F];
}

inline uint8_t tca_addr_for_slot(uint8_t slot0) {
  return bq_route_for_slot(slot0).tca_addr;
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

inline bool select_tca_slot(uint8_t slot0) {
  if (!disable_tcas()) {
    ESP_LOGW(TAG_BQWEB, "TCA select aborted for C%02u: cannot disconnect TCA muxes first.", slot0 + 1);
    return false;
  }
  uint8_t tca = tca_addr_for_slot(slot0);
  bool ok = i2c_write_u8(tca, tca_mask_for_slot(slot0));
  if (!ok) {
    ESP_LOGW(TAG_BQWEB, "TCA select write failed for C%02u: TCA=0x%02X ch=%u; attempting I2C recovery.",
             slot0 + 1, tca, tca_channel_for_slot(slot0));
    recover_i2c_bus("TCA select failed");
    ok = i2c_write_u8(tca, tca_mask_for_slot(slot0));
  }
  delay(4);
  return ok;
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

inline bool configure_ina219() {
  return i2c_write_reg16(ADDR_INA219, 0x00, 0x399F);
}

inline bool read_ina(SlotSnapshot &s) {
  uint16_t bus = 0;
  uint16_t shunt = 0;
  bool ok_bus = i2c_read_reg16(ADDR_INA219, 0x02, bus);
  bool ok_shunt = i2c_read_reg16(ADDR_INA219, 0x01, shunt);
  s.ina_ok = ok_bus && ok_shunt;
  if (s.ina_ok) {
    s.ina_bus_raw = bus;
    s.ina_shunt_raw = (int16_t) shunt;
    s.bus_v = ina_bus_v_from_raw(bus);
    s.shunt_mv = ina_shunt_mv_from_raw(shunt);
  }
  return s.ina_ok;
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

struct BQFullRegs {
  bool ok{false};
  uint8_t r[11]{0};
};

static SlotSnapshot bq_cached_slots[16];
static BQFullRegs bq_cached_regs[16];
static bool bq_cached_valid[16] = {false};
static uint32_t bq_cached_read_ms[16] = {0};
static volatile uint16_t slot_web_pending_mask = 0;
static volatile bool slot_web_read_busy = false;
static volatile uint8_t slot_web_pending_slot0 = 0;

inline bool read_bq_full_regs(BQFullRegs &bq) {
  for (uint8_t reg = 0; reg <= 0x0A; reg++) {
    if (!i2c_read_reg8(ADDR_BQ24195, reg, bq.r[reg])) {
      bq.ok = false;
      return false;
    }
    yield();
  }
  bq.ok = true;
  return true;
}

inline bool read_bq_web_regs(BQFullRegs &bq, SlotSnapshot &s) {
  if (!i2c_read_reg8(ADDR_BQ24195, 0x00, bq.r[0x00])) { bq.ok = false; return false; }
  yield();
  if (!i2c_read_reg8(ADDR_BQ24195, 0x08, bq.r[0x08])) { bq.ok = false; return false; }
  yield();
  if (!i2c_read_reg8(ADDR_BQ24195, 0x09, bq.r[0x09])) { bq.ok = false; return false; }
  yield();
  if (!i2c_read_reg8(ADDR_BQ24195, 0x0A, bq.r[0x0A])) { bq.ok = false; return false; }
  s.reg00 = bq.r[0x00];
  s.reg08 = bq.r[0x08];
  s.reg09a = bq.r[0x09];
  s.reg09b = bq.r[0x09];
  s.reg0a = bq.r[0x0A];
  bq.ok = true;
  return true;
}

inline bool capture_tc1047_slot(uint8_t slot0, SlotSnapshot &s) {
  configure_wire_runtime_limits();

  // PCF8574 hazard: reading it does NOT give a reliable output latch snapshot.
  // Reads return physical pin levels. Writing that value back can accidentally
  // drive unrelated PCF pins low and latch the board into a bad state that may
  // survive ESP soft reset. Therefore use explicit safe states only.
  if (!i2c_write_u8(ADDR_PCF8574, PCF_TC1047_MUX_ENABLE)) {
    ESP_LOGW(TAG_BQWEB, "PCF8574 mux-enable write failed before TC1047 read for C%02u.", slot0 + 1);
    return false;
  }

  delay(5);
  mux_select(slot0 & 0x0F);
  delay(5);

  constexpr uint8_t samples = 8;
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

  const bool idle_ok = i2c_write_u8(ADDR_PCF8574, PCF_SAFE_IDLE);
  if (!idle_ok) {
    ESP_LOGW(TAG_BQWEB, "PCF8574 safe-idle restore failed after TC1047 read for C%02u.", slot0 + 1);
  }

  s.temp_raw_avg = (float) sum_raw / (float) samples;
  s.temp_min_raw = min_raw;
  s.temp_max_raw = max_raw;
  s.a0_raw = (int) lroundf(s.temp_raw_avg);
  s.temp_v = s.temp_raw_avg * (MCC_TC1047_ADC_FULL_SCALE_V / 1023.0f);
  s.temp_c = tc1047_temp_c_from_v(s.temp_v) + MCC_TC1047_TEMP_OFFSET_C;
  s.temp_ok = true;
  return idle_ok;
}

inline void publish_tc1047_slot_temperature(uint8_t slot0, const SlotSnapshot &s) {
  if (!s.temp_ok) return;
  switch (slot0 & 0x0F) {
    case 0: id(c1_tc1047_temperature).publish_state(s.temp_c); break;
    case 1: id(c2_tc1047_temperature).publish_state(s.temp_c); break;
    case 2: id(c3_tc1047_temperature).publish_state(s.temp_c); break;
    case 3: id(c4_tc1047_temperature).publish_state(s.temp_c); break;
    case 4: id(c5_tc1047_temperature).publish_state(s.temp_c); break;
    case 5: id(c6_tc1047_temperature).publish_state(s.temp_c); break;
    case 6: id(c7_tc1047_temperature).publish_state(s.temp_c); break;
    case 7: id(c8_tc1047_temperature).publish_state(s.temp_c); break;
    case 8: id(c9_tc1047_temperature).publish_state(s.temp_c); break;
    case 9: id(c10_tc1047_temperature).publish_state(s.temp_c); break;
    case 10: id(c11_tc1047_temperature).publish_state(s.temp_c); break;
    case 11: id(c12_tc1047_temperature).publish_state(s.temp_c); break;
    case 12: id(c13_tc1047_temperature).publish_state(s.temp_c); break;
    case 13: id(c14_tc1047_temperature).publish_state(s.temp_c); break;
    case 14: id(c15_tc1047_temperature).publish_state(s.temp_c); break;
    case 15: id(c16_tc1047_temperature).publish_state(s.temp_c); break;
  }
}

inline bool capture_slot_web(uint8_t slot0, SlotSnapshot &s, BQFullRegs &bq) {
  configure_wire_runtime_limits();
  if (!disable_tcas()) {
    ESP_LOGW(TAG_BQWEB, "Slot read aborted for C%02u: TCA muxes are not reachable.", slot0 + 1);
    return false;
  }

  capture_tc1047_slot(slot0, s);

  if (!select_tca_slot(slot0)) {
    ESP_LOGW(TAG_BQWEB, "TCA select failed for C%02u: TCA=0x%02X ch=%u. Skipping BQ read.",
             slot0 + 1, tca_addr_for_slot(slot0), tca_channel_for_slot(slot0));
    const bool tcas_off_ok = disable_tcas();
    if (tcas_off_ok) {
      configure_ina219();
      read_ina(s);
    } else {
      ESP_LOGW(TAG_BQWEB, "Skipping INA read after C%02u TCA select failure because TCA disconnect failed.", slot0 + 1);
    }
    return s.temp_ok || s.ina_ok;
  }

  s.bq_ok = i2c_present(ADDR_BQ24195);
  if (s.bq_ok) {
    read_bq_web_regs(bq, s);
  }

  const bool tcas_off_ok = disable_tcas();
  if (tcas_off_ok) {
    configure_ina219();
    read_ina(s);
  } else {
    ESP_LOGW(TAG_BQWEB, "Skipping INA read after C%02u because TCA disconnect failed.", slot0 + 1);
  }
  return s.temp_ok || bq.ok || s.ina_ok;
}

inline void capture_slot_full(uint8_t slot0, SlotSnapshot &s, BQFullRegs &bq) {
  select_tca_slot(slot0);
  mux_select(slot0 & 0x0F);
  s.a0_raw = adc_avg(4);
  s.bq_ok = i2c_present(ADDR_BQ24195);
  if (s.bq_ok) {
    read_bq_full_regs(bq);
    s.reg00 = bq.r[0x00];
    s.reg08 = bq.r[0x08];
    s.reg09a = bq.r[0x09];
    uint8_t reg09_second = 0;
    if (i2c_read_reg8(ADDR_BQ24195, 0x09, reg09_second)) {
      s.reg09b = reg09_second;
    } else {
      s.reg09b = bq.r[0x09];
    }
    s.reg0a = bq.r[0x0A];
  }
  read_ina(s);
  disable_tcas();
}

inline void log_bq_full_line(uint8_t slot0, const SlotSnapshot &s, const BQFullRegs &bq, const char *prefix, uint8_t sample_idx = 0) {
  ESP_LOGI(TAG_BQFULL,
           "%s sample=%02u slot=%02u ref=%s TCA=0x%02X ch=%u mask=0x%02X BQ=%s regs=[%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X] r08=%s DPM=%u PG=%u VSYS=%u r09=%s WD=%u BAT=%u INA_bus=%.3fV tc1047_if_bus=%.1fC shunt=%.2fmV A0=%d",
           prefix, sample_idx, slot0 + 1, tca_ref_for_slot(slot0), tca_addr_for_slot(slot0), tca_channel_for_slot(slot0), tca_mask_for_slot(slot0),
           yesno(bq.ok),
           bq.r[0], bq.r[1], bq.r[2], bq.r[3], bq.r[4], bq.r[5], bq.r[6], bq.r[7], bq.r[8], bq.r[9], bq.r[10],
           bq_chrg_str(bq.r[8]), (bq.r[8] >> 3) & 1, (bq.r[8] >> 2) & 1, bq.r[8] & 1,
           bq_fault_str(s.reg09b), (s.reg09b >> 7) & 1, (s.reg09b >> 3) & 1,
           s.bus_v, tc1047_temp_c_from_v(s.bus_v), s.shunt_mv, s.a0_raw);
}

inline void bq_web_print_cell(AsyncResponseStream *response, const char *value) {
  response->print("<td>");
  response->print(value);
  response->print("</td>");
}

inline void bq_web_print_cell_u8_hex(AsyncResponseStream *response, uint8_t value) {
  char buf[8];
  snprintf(buf, sizeof(buf), "0x%02X", value);
  bq_web_print_cell(response, buf);
}

inline void bq_web_print_cell_float(AsyncResponseStream *response, float value, uint8_t decimals) {
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

inline bool slot_web_is_queued(uint8_t slot0) {
  return (slot_web_pending_mask & ((uint16_t) 1U << (slot0 & 0x0F))) != 0;
}

inline void bq_web_print_row(AsyncResponseStream *response, uint8_t slot0, int8_t queued_slot0) {
  const bool have_data = bq_cached_valid[slot0];
  const SlotSnapshot *s = have_data ? &bq_cached_slots[slot0] : nullptr;
  const BQFullRegs *bq = have_data ? &bq_cached_regs[slot0] : nullptr;
  response->print("<tr>");
  char buf[40];
  snprintf(buf, sizeof(buf), "C%02u", slot0 + 1);
  bq_web_print_cell(response, buf);
  if (slot_web_read_busy && slot0 == slot_web_pending_slot0) {
    snprintf(buf, sizeof(buf), "Reading");
  } else if (queued_slot0 == (int8_t) slot0 || slot_web_is_queued(slot0)) {
    snprintf(buf, sizeof(buf), "Queued");
  } else {
    snprintf(buf, sizeof(buf), "<a href=\"/bq?slot=%u\">Read</a>", slot0 + 1);
  }
  bq_web_print_cell(response, buf);
  bq_web_print_cell(response, tca_ref_for_slot(slot0));
  snprintf(buf, sizeof(buf), "0x%02X/%u", tca_addr_for_slot(slot0), tca_channel_for_slot(slot0));
  bq_web_print_cell(response, buf);
  if (!have_data) {
    for (uint8_t i = 0; i < 15; i++) bq_web_print_cell(response, "-");
    response->print("</tr>");
    return;
  }
  bq_web_print_cell(response, bq->ok ? "YES" : "NO");
  bq_web_print_cell_u8_hex(response, bq->r[0x00]);
  bq_web_print_cell_u8_hex(response, bq->r[0x08]);
  bq_web_print_cell(response, bq->ok ? bq_chrg_str(bq->r[0x08]) : "unavailable");
  snprintf(buf, sizeof(buf), "%u", (bq->r[0x08] >> 3) & 1);
  bq_web_print_cell(response, buf);
  snprintf(buf, sizeof(buf), "%u", (bq->r[0x08] >> 2) & 1);
  bq_web_print_cell(response, buf);
  snprintf(buf, sizeof(buf), "%u", bq->r[0x08] & 1);
  bq_web_print_cell(response, buf);
  bq_web_print_cell_u8_hex(response, s->reg09b);
  bq_web_print_cell(response, bq->ok ? bq_fault_str(s->reg09b) : "unavailable");
  bq_web_print_cell_u8_hex(response, bq->r[0x0A]);
  bq_web_print_cell_float(response, s->bus_v, 3);
  bq_web_print_cell_float(response, s->shunt_mv, 2);
  bq_web_print_cell_float(response, s->temp_c, 1);
  snprintf(buf, sizeof(buf), "%d", s->a0_raw);
  bq_web_print_cell(response, buf);
  snprintf(buf, sizeof(buf), "%lus", (unsigned long) ((millis() - bq_cached_read_ms[slot0]) / 1000UL));
  bq_web_print_cell(response, buf);
  response->print("</tr>");
}

inline void send_bq_table_response(AsyncWebServerRequest *request, int8_t queued_slot0) {
  ESP_LOGW(TAG_BQWEB, "Serving /bq: table response only, no I2C read in web context.");

  static constexpr size_t BQ_TABLE_HTML_BUFFER_SIZE = 16384;

  auto *response = request->beginResponseStream(
    "text/html",
    BQ_TABLE_HTML_BUFFER_SIZE
  );

  if (queued_slot0 >= 0) {
    char msg[64];
    snprintf(msg, sizeof(msg), "<p>Queued slot read for C%02u.</p>", static_cast<unsigned>(queued_slot0 + 1));
    response->print(msg);
  } else if (slot_web_read_busy || slot_web_pending_mask != 0) {
    response->print("<p>Slot read queue active.</p>");
  }

  if (queued_slot0 >= 0 || slot_web_read_busy || slot_web_pending_mask != 0) {
    response->print("<meta http-equiv=\"refresh\" content=\"2;url=/bq\">");
  }

  response->print("<p><a href=\"/bq?read_all=1\">Read all slots</a></p>");
  response->print("<table border=\"1\"><tr><th>Slot</th><th>Read</th><th>Route</th><th>TCA/ch</th><th>BQ</th><th>REG00</th><th>REG08</th><th>Charge</th><th>DPM</th><th>PG</th><th>VSYS</th><th>REG09</th><th>Fault</th><th>REG0A</th><th>INA Bus V</th><th>Shunt mV</th><th>TC1047 C</th><th>A0</th><th>Age</th></tr>");
  for (uint8_t slot0 = 0; slot0 < 16; slot0++) {
    bq_web_print_row(response, slot0, queued_slot0);
  }
  response->print("</table>");
  request->send(response);
  ESP_LOGW(TAG_BQWEB, "Serving /bq complete.");
}

inline bool queue_slot_web_read(uint8_t slot0) {
  if (slot0 >= 16) return false;
  const uint16_t bit = ((uint16_t) 1U << slot0);
  if ((slot_web_pending_mask & bit) != 0) return false;
  if (slot_web_read_busy && slot_web_pending_slot0 == slot0) return false;
  slot_web_pending_mask |= bit;
  esphome::Application::wake_loop_any_context();
  return true;
}

inline bool queue_slot_web_read_all() {
  uint16_t mask = 0xFFFF;
  if (slot_web_read_busy) {
    mask &= (uint16_t) ~((uint16_t) 1U << (slot_web_pending_slot0 & 0x0F));
  }
  const uint16_t before = slot_web_pending_mask;
  slot_web_pending_mask |= mask;
  esphome::Application::wake_loop_any_context();
  return slot_web_pending_mask != before;
}

inline void process_slot_web_read_request() {
  if (slot_web_pending_mask == 0 || slot_web_read_busy) return;

  uint8_t slot0 = 0;
  uint16_t bit = 1;
  while (slot0 < 16 && (slot_web_pending_mask & bit) == 0) {
    slot0++;
    bit <<= 1;
  }
  if (slot0 >= 16) {
    slot_web_pending_mask = 0;
    return;
  }

  slot_web_pending_mask &= (uint16_t) ~bit;
  slot_web_pending_slot0 = slot0;
  slot_web_read_busy = true;

  ESP_LOGW(TAG_BQWEB, "Processing queued slot read for C%02u in main loop.", slot0 + 1);
  const uint32_t start_ms = millis();

  SlotSnapshot s;
  BQFullRegs bq;
  capture_slot_web(slot0, s, bq);

  const uint32_t elapsed_ms = millis() - start_ms;
  bq_cached_slots[slot0] = s;
  bq_cached_regs[slot0] = bq;
  bq_cached_valid[slot0] = true;
  bq_cached_read_ms[slot0] = millis();
  publish_tc1047_slot_temperature(slot0, s);

  if (elapsed_ms > 1500UL) {
    slot_web_pending_mask = 0;
    ESP_LOGE(TAG_BQWEB,
             "Slot read for C%02u took %lums. Cleared remaining queue to protect ESP8266 and recovering I2C.",
             slot0 + 1, (unsigned long) elapsed_ms);
    recover_i2c_bus("slow slot read");
  }

  slot_web_read_busy = false;
  ESP_LOGW(TAG_BQWEB, "Queued slot read complete for C%02u in %lums.",
           slot0 + 1, (unsigned long) elapsed_ms);
}

inline void process_bq_web_read_request() {
  process_slot_web_read_request();
}

inline int8_t bq_web_slot_from_request(AsyncWebServerRequest *request) {
  if (!request->hasParam("slot")) return -1;
  const AsyncWebParameter *param = request->getParam("slot");
  int slot = param->value().toInt();
  if (slot < 1 || slot > 16) return -2;
  return (int8_t) (slot - 1);
}

class BqTableHandler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) const override {
    return request->method() == HTTP_GET && request->url() == "/bq";
  }

  void handleRequest(AsyncWebServerRequest *request) override {
    int8_t slot0 = bq_web_slot_from_request(request);
    if (slot0 == -2) {
      request->send(400, "text/plain", "slot must be 1..16");
      return;
    }

    int8_t queued_slot0 = -1;
    if (request->hasParam("read_all")) {
      if (queue_slot_web_read_all()) {
        ESP_LOGW(TAG_BQWEB, "Queued /bq read_all request for C01..C16.");
      } else {
        ESP_LOGW(TAG_BQWEB, "Ignored /bq read_all request: all slots already queued or being read.");
      }
    }

    if (slot0 >= 0) {
      if (queue_slot_web_read((uint8_t) slot0)) {
        queued_slot0 = slot0;
        ESP_LOGW(TAG_BQWEB, "Queued /bq slot read request for C%02u.", slot0 + 1);
      } else {
        ESP_LOGW(TAG_BQWEB, "Rejected /bq slot read request for C%02u: slot already queued or being read.", slot0 + 1);
      }
    }

    send_bq_table_response(request, queued_slot0);
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
  registered = true;
  ESP_LOGW(TAG_BQWEB, "Registered read-only BQ table endpoint at /bq.");
}

inline void dump_bq_c16_u38() {
  ESP_LOGW(TAG_BQFULL, "========== BQ24195 C16 VIA U38 READ-ONLY DUMP START ==========");
  ESP_LOGW(TAG_BQFULL, "C16 route: U38 TCA9548APWR addr=0x%02X ch=7 mask=0x80, SC7/SD7 -> C16 BQ24195 at 0x%02X.", ADDR_TCA1, ADDR_BQ24195);
  ESP_LOGW(TAG_BQFULL, "Read-only for BQ24195; helper writes only TCA channel select and INA219 diagnostic config.");
  configure_ina219();
  SlotSnapshot s;
  BQFullRegs bq;
  capture_slot_full(SLOT0_C16, s, bq);
  log_bq_full_line(SLOT0_C16, s, bq, "c16_u38", 0);
  id(c16_bq_present).publish_state(s.bq_ok ? 1.0f : 0.0f);
  id(c16_bq_reg08).publish_state(bq.ok ? (float) bq.r[0x08] : NAN);
  id(c16_bq_reg09).publish_state(bq.ok ? (float) s.reg09b : NAN);
  id(c16_bq_reg0a).publish_state(bq.ok ? (float) bq.r[0x0A] : NAN);
  id(c16_bq_dpm_stat).publish_state(bq.ok ? (float) ((bq.r[0x08] >> 3) & 1) : NAN);
  id(c16_bq_pg_stat).publish_state(bq.ok ? (float) ((bq.r[0x08] >> 2) & 1) : NAN);
  id(c16_bq_vsys_stat).publish_state(bq.ok ? (float) (bq.r[0x08] & 1) : NAN);
  id(c16_ina_bus_voltage).publish_state(s.ina_ok ? s.bus_v : NAN);
  id(c16_ina_shunt_voltage).publish_state(s.ina_ok ? s.shunt_mv : NAN);
  id(c16_bq_charge_status).publish_state(bq.ok ? bq_chrg_str(bq.r[0x08]) : "unavailable");
  id(c16_bq_fault_status).publish_state(bq.ok ? bq_fault_str(s.reg09b) : "unavailable");
  disable_tcas();
  ESP_LOGW(TAG_BQFULL, "========== BQ24195 C16 VIA U38 READ-ONLY DUMP END ==========");
}

}  // namespace mccdiag

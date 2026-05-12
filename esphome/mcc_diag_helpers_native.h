#pragma once

#include "esphome.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <Arduino.h>
#include <math.h>

namespace mccdiag {

// MCC Pro native-ESPHome diagnostic helper.
// Native components used in YAML:
//   - tca9548a: exposes C01..C16 BQ24195 branches as virtual I2C buses
//   - pcf8574 + gpio switch: controls TC1047 analog mux enable, P0 active LOW
//   - i2c_device: BQ24195 devices and INA219 diagnostic device
// Remaining direct GPIO use: 74HC4067 address lines S0..S3 and ESP8266 A0.

static constexpr uint8_t PIN_MUX_S0 = 13;
static constexpr uint8_t PIN_MUX_S1 = 12;
static constexpr uint8_t PIN_MUX_S2 = 14;
static constexpr uint8_t PIN_MUX_S3 = 16;

static constexpr uint8_t SLOT0_C16 = 15;
static constexpr uint8_t ADDR_BQ24195 = 0x6B;
static constexpr uint8_t ADDR_INA219_U47 = 0x41;

static const char *TAG_BQWEB = "mcc_bqweb";
static const char *TAG_BQFULL = "mcc_bqfull";
static const char *TAG_TEMP = "mcc_tc1047";

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
  float bus_v{NAN};
  float shunt_mv{NAN};

  bool temp_ok{false};
  uint16_t temp_min_raw{0};
  uint16_t temp_max_raw{0};
  float temp_raw_avg{NAN};
  float temp_v{NAN};
  float temp_c{NAN};
  int a0_raw{-1};
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
static volatile bool bq_web_read_busy = false;
static volatile uint8_t bq_web_pending_slot0 = 0;

inline void feed() { yield(); }
inline const char *yesno(bool v) { return v ? "YES" : "NO"; }

inline const BqRoute &bq_route_for_slot(uint8_t slot0) {
  return BQ_ROUTES[slot0 & 0x0F];
}

inline uint8_t tca_addr_for_slot(uint8_t slot0) {
  return bq_route_for_slot(slot0).tca_index == 0 ? 0x70 : 0x71;
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

inline esphome::i2c::I2CDevice &ina_dev() {
  return id(ina_u47);
}

inline bool dev_read_reg8(esphome::i2c::I2CDevice &dev, uint8_t reg, uint8_t &value) {
  return dev.read_byte(reg, &value);
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

inline bool configure_ina219() {
  return dev_write_reg16_be(ina_dev(), 0x00, 0x399F);
}

inline bool read_ina(SlotSnapshot &s) {
  uint16_t bus = 0;
  uint16_t shunt = 0;
  bool ok_bus = dev_read_reg16_be(ina_dev(), 0x02, bus);
  bool ok_shunt = dev_read_reg16_be(ina_dev(), 0x01, shunt);
  s.ina_ok = ok_bus && ok_shunt;
  if (s.ina_ok) {
    s.ina_bus_raw = bus;
    s.ina_shunt_raw = static_cast<int16_t>(shunt);
    s.bus_v = ina_bus_v_from_raw(bus);
    s.shunt_mv = ina_shunt_mv_from_raw(shunt);
  }
  return s.ina_ok;
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
  // Native PCF8574 path: GPIO switch is inverted, therefore ON drives P0 LOW
  // and enables the active-LOW 74HC4067 enable pin.
  id(tc1047_mux_enable).turn_on();
  delay(8);

  mux_select(slot0 & 0x0F);
  delay(8);

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

  id(tc1047_mux_enable).turn_off();
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
           "C%02u/Y%02u TC1047: raw_avg=%.2f min=%u max=%u voltage=%.4fV temp=%.1fC",
           slot0 + 1, slot0, raw_avg, min_raw, max_raw, voltage, temp_c);
  return true;
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

inline bool read_bq_web_regs(esphome::i2c::I2CDevice &dev, SlotSnapshot &s, BQFullRegs &bq) {
  bool ok00 = dev_read_reg8(dev, 0x00, bq.r[0x00]);
  yield();
  bool ok08 = dev_read_reg8(dev, 0x08, bq.r[0x08]);
  yield();
  bool ok09a = dev_read_reg8(dev, 0x09, bq.r[0x09]);
  yield();
  uint8_t reg09_second = bq.r[0x09];
  bool ok09b = dev_read_reg8(dev, 0x09, reg09_second);
  yield();
  bool ok0a = dev_read_reg8(dev, 0x0A, bq.r[0x0A]);

  bq.ok = ok00 && ok08 && ok09a && ok09b && ok0a;
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

  // Main-bus INA read is intentionally before the downstream BQ branch read.
  // If a BQ branch is electrically bad, we still cache temperature and INA data.
  configure_ina219();
  read_ina(s);

  esphome::i2c::I2CDevice &bq_dev = bq_dev_for_slot(slot0);
  read_bq_web_regs(bq_dev, s, bq);
}

inline void publish_c16_diagnostics(const SlotSnapshot &s, const BQFullRegs &bq) {
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
}

inline void log_bq_line(uint8_t slot0, const SlotSnapshot &s, const BQFullRegs &bq, const char *prefix) {
  ESP_LOGI(TAG_BQFULL,
           "%s slot=C%02u route=%s bus=%s TCA=0x%02X ch=%u mask=0x%02X BQ=%s REG00=0x%02X REG08=0x%02X chg=%s DPM=%u PG=%u VSYS=%u REG09=0x%02X fault=%s REG0A=0x%02X temp=%.1fC INA_bus=%.3fV shunt=%.2fmV",
           prefix, slot0 + 1, tca_ref_for_slot(slot0), bus_id_for_slot(slot0),
           tca_addr_for_slot(slot0), tca_channel_for_slot(slot0), tca_mask_for_slot(slot0),
           yesno(bq.ok), bq.r[0x00], bq.r[0x08], bq_chrg_str(bq.r[0x08]),
           (bq.r[0x08] >> 3) & 1, (bq.r[0x08] >> 2) & 1, bq.r[0x08] & 1,
           s.reg09b, bq_fault_str(s.reg09b), bq.r[0x0A], s.temp_c, s.bus_v, s.shunt_mv);
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

inline bool queue_slot(uint8_t slot0) {
  if (slot0 > 15) return false;
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

inline void clear_pending_queue() {
  bq_web_pending_mask = 0;
  ESP_LOGW(TAG_BQWEB, "Cleared pending /bq slot queue. Busy read, if any, will finish.");
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

inline void bq_web_print_row(AsyncResponseStream *response, uint8_t slot0) {
  const bool have_data = bq_cached_valid[slot0];
  const SlotSnapshot *s = have_data ? &bq_cached_slots[slot0] : nullptr;
  const BQFullRegs *bq = have_data ? &bq_cached_regs[slot0] : nullptr;
  const uint16_t pending_mask = bq_web_pending_mask;

  response->print("<tr>");
  char buf[48];

  snprintf(buf, sizeof(buf), "C%02u", slot0 + 1);
  bq_web_print_cell(response, buf);

  if (bq_web_read_busy && slot0 == bq_web_pending_slot0) {
    snprintf(buf, sizeof(buf), "Reading");
  } else if ((pending_mask & (1U << slot0)) != 0) {
    snprintf(buf, sizeof(buf), "Queued");
  } else {
    snprintf(buf, sizeof(buf), "<a href=\"/bq?slot=%u\">Read</a>", slot0 + 1);
  }
  bq_web_print_cell(response, buf);

  snprintf(buf, sizeof(buf), "%s", ((pending_mask & (1U << slot0)) != 0) ? "pending" : "-");
  bq_web_print_cell(response, buf);

  bq_web_print_cell(response, tca_ref_for_slot(slot0));

  snprintf(buf, sizeof(buf), "%s/0x%02X/%u", bus_id_for_slot(slot0), tca_addr_for_slot(slot0), tca_channel_for_slot(slot0));
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
  bq_web_print_cell_float(response, s->temp_c, 1);
  bq_web_print_cell_float(response, s->temp_v, 3);
  bq_web_print_cell_float(response, s->bus_v, 3);
  bq_web_print_cell_float(response, s->shunt_mv, 2);

  snprintf(buf, sizeof(buf), "%lus", (unsigned long) ((millis() - bq_cached_read_ms[slot0]) / 1000UL));
  bq_web_print_cell(response, buf);
  response->print("</tr>");
}

inline void send_bq_table_response(AsyncWebServerRequest *request, const char *notice) {
  ESP_LOGW(TAG_BQWEB, "Serving /bq table response only; no I2C read in web context.");
  auto *response = request->beginResponseStream("text/html");

  response->print("<!doctype html><html><head><meta charset=\"utf-8\"><title>MCC /bq</title>");
  if (bq_web_read_busy || bq_web_pending_mask != 0) {
    response->print("<meta http-equiv=\"refresh\" content=\"2;url=/bq\">");
  }
  response->print("</head><body>");
  response->print("<h3>MCC Pro slot diagnostics</h3>");
  response->print("<p><a href=\"/bq?read_all=1\">Read all</a> | <a href=\"/bq?clear=1\">Clear queue</a></p>");

  if (notice != nullptr && notice[0] != '\0') {
    response->print("<p>");
    response->print(notice);
    response->print("</p>");
  }

  char status[96];
  snprintf(status, sizeof(status), "<p>Busy: %s; pending: %u</p>", yesno(bq_web_read_busy), pending_count());
  response->print(status);

  response->print("<table border=\"1\"><tr>"
                  "<th>Slot</th><th>Read</th><th>Queue</th><th>Route</th><th>I2C bus/TCA/ch</th>"
                  "<th>BQ</th><th>REG00</th><th>REG08</th><th>Charge</th><th>DPM</th><th>PG</th><th>VSYS</th>"
                  "<th>REG09</th><th>Fault</th><th>REG0A</th><th>TC1047 C</th><th>TC1047 V</th>"
                  "<th>INA Bus V</th><th>Shunt mV</th><th>Age</th></tr>");

  for (uint8_t slot0 = 0; slot0 < 16; slot0++) {
    bq_web_print_row(response, slot0);
  }

  response->print("</table></body></html>");
  request->send(response);
  ESP_LOGW(TAG_BQWEB, "Serving /bq complete.");
}

inline void process_bq_web_read_request() {
  if (bq_web_read_busy || bq_web_pending_mask == 0) return;

  uint8_t slot0 = 0;
  uint16_t mask = bq_web_pending_mask;
  while (slot0 < 16 && ((mask & (1U << slot0)) == 0)) slot0++;
  if (slot0 >= 16) {
    bq_web_pending_mask = 0;
    return;
  }

  bq_web_pending_mask &= (uint16_t) ~(1U << slot0);
  bq_web_pending_slot0 = slot0;
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

  if (slot0 == SLOT0_C16) {
    publish_c16_diagnostics(s, bq);
  }

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
    char notice[96] = {0};

    if (request->hasParam("clear")) {
      clear_pending_queue();
      snprintf(notice, sizeof(notice), "Queue cleared.");
    }

    if (request->hasParam("read_all")) {
      queue_all_slots();
      snprintf(notice, sizeof(notice), "Queued read_all for C01..C16.");
    } else {
      int8_t slot0 = bq_web_slot_from_request(request);
      if (slot0 == -2) {
        request->send(400, "text/plain", "slot must be 1..16");
        return;
      }
      if (slot0 >= 0) {
        queue_slot((uint8_t) slot0);
        snprintf(notice, sizeof(notice), "Queued read for C%02u.", static_cast<unsigned>(slot0 + 1));
        ESP_LOGW(TAG_BQWEB, "Queued /bq native slot read request for C%02u.", slot0 + 1);
      }
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
  registered = true;
  ESP_LOGW(TAG_BQWEB, "Registered native ESPHome BQ table endpoint at /bq.");
}

inline void dump_bq_c16_u38() {
  queue_slot(SLOT0_C16);
  ESP_LOGW(TAG_BQFULL, "Queued C16 diagnostics via native ESPHome TCA9548A/i2c_device path.");
}

}  // namespace mccdiag

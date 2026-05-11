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

static constexpr uint8_t ADDR_PCF8574 = 0x27;
static constexpr uint8_t ADDR_OLED    = 0x3C;
static constexpr uint8_t ADDR_INA219_U47 = 0x41;  // U47: A0=HIGH, A1=LOW.
static constexpr uint8_t ADDR_INA219_U34 = 0x45;  // U34: A0=HIGH, A1=HIGH; slot/path still being traced.
static constexpr uint8_t ADDR_INA219 = ADDR_INA219_U47;  // Current tested diagnostic path.
static constexpr uint8_t ADDR_PCA9685 = 0x4F;
static constexpr uint8_t ADDR_TCA0    = 0x70;  // C1..C8 side, exact PCB ref still being traced.
static constexpr uint8_t ADDR_TCA1    = 0x71;  // U38: A0=5V, A1=GND, A2=GND; C9..C16 side.
static constexpr uint8_t ADDR_BQ24195 = 0x6B;

static constexpr uint8_t SLOT0_C16 = 15;

static const char *TAG_BQWEB = "mcc_bqweb";
static const char *TAG_BQFULL = "mcc_bqfull";

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

inline int adc_avg(uint8_t samples = 8) {
  long sum = 0;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(A0);
    delay(2);
  }
  return (int) lroundf((float) sum / (float) samples);
}

inline void disable_tcas() {
  i2c_write_u8(ADDR_TCA0, 0x00);
  i2c_write_u8(ADDR_TCA1, 0x00);
  delay(2);
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
  disable_tcas();
  uint8_t tca = tca_addr_for_slot(slot0);
  bool ok = i2c_write_u8(tca, tca_mask_for_slot(slot0));
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

inline bool read_bq_full_regs(BQFullRegs &bq) {
  bool ok = true;
  for (uint8_t reg = 0; reg <= 0x0A; reg++) {
    ok &= i2c_read_reg8(ADDR_BQ24195, reg, bq.r[reg]);
    yield();
  }
  bq.ok = ok;
  return ok;
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
  } else {
    snprintf(buf, sizeof(buf), decimals == 3 ? "%.3f" : "%.2f", value);
  }
  bq_web_print_cell(response, buf);
}

inline void bq_web_print_row(AsyncResponseStream *response, uint8_t slot0, const SlotSnapshot *s, const BQFullRegs *bq) {
  const bool have_data = s != nullptr && bq != nullptr;
  response->print("<tr>");
  char buf[40];
  snprintf(buf, sizeof(buf), "C%02u", slot0 + 1);
  bq_web_print_cell(response, buf);
  snprintf(buf, sizeof(buf), "<a href=\"/bq?slot=%u\">Read</a>", slot0 + 1);
  bq_web_print_cell(response, buf);
  bq_web_print_cell(response, tca_ref_for_slot(slot0));
  snprintf(buf, sizeof(buf), "0x%02X/%u", tca_addr_for_slot(slot0), tca_channel_for_slot(slot0));
  bq_web_print_cell(response, buf);
  if (!have_data) {
    for (uint8_t i = 0; i < 13; i++) bq_web_print_cell(response, "-");
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
  snprintf(buf, sizeof(buf), "%d", s->a0_raw);
  bq_web_print_cell(response, buf);
  response->print("</tr>");
}

inline void send_bq_table_response(AsyncWebServerRequest *request, int8_t selected_slot0) {
  SlotSnapshot selected_s;
  BQFullRegs selected_bq;
  const bool read_slot = selected_slot0 >= 0 && selected_slot0 < 16;
  if (read_slot) {
    ESP_LOGW(TAG_BQWEB, "Serving /bq: reading one BQ24195 slot C%02u.", selected_slot0 + 1);
    configure_ina219();
    capture_slot_full((uint8_t) selected_slot0, selected_s, selected_bq);
  } else {
    ESP_LOGW(TAG_BQWEB, "Serving /bq: table only, no I2C read.");
  }
  auto *response = request->beginResponseStream("text/html");
  response->print("<table border=\"1\"><tr><th>Slot</th><th>Read</th><th>Route</th><th>TCA/ch</th><th>BQ</th><th>REG00</th><th>REG08</th><th>Charge</th><th>DPM</th><th>PG</th><th>VSYS</th><th>REG09</th><th>Fault</th><th>REG0A</th><th>INA Bus V</th><th>Shunt mV</th><th>A0</th></tr>");
  for (uint8_t slot0 = 0; slot0 < 16; slot0++) {
    if (read_slot && slot0 == (uint8_t) selected_slot0) {
      bq_web_print_row(response, slot0, &selected_s, &selected_bq);
    } else {
      bq_web_print_row(response, slot0, nullptr, nullptr);
    }
  }
  response->print("</table>");
  request->send(response);
  ESP_LOGW(TAG_BQWEB, "Serving /bq complete.");
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
    send_bq_table_response(request, slot0);
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

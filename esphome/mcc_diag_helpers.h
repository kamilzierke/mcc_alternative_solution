#pragma once

#include "esphome.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

namespace mccdiag {

static constexpr uint8_t PIN_MUX_S0 = 13;
static constexpr uint8_t PIN_MUX_S1 = 12;
static constexpr uint8_t PIN_MUX_S2 = 14;
static constexpr uint8_t PIN_MUX_S3 = 16;

static constexpr uint8_t ADDR_PCF8574 = 0x27;
static constexpr uint8_t ADDR_OLED    = 0x3C;
static constexpr uint8_t ADDR_INA219  = 0x41;
static constexpr uint8_t ADDR_PCA9685 = 0x4F;
static constexpr uint8_t ADDR_TCA0    = 0x70;  // C1..C8 side, exact PCB ref still being traced.
static constexpr uint8_t ADDR_TCA1    = 0x71;  // U38: A0=5V, A1=GND, A2=GND; C9..C16 side.
static constexpr uint8_t ADDR_BQ24195 = 0x6B;

static constexpr uint8_t SLOT0_C16 = 15;

static const char *TAG_SNAP = "mcc_snap";
static const char *TAG_DELTA = "mcc_delta";
static const char *TAG_MUX = "mcc_mux";
static const char *TAG_PCA = "mcc_pca";
static const char *TAG_PCF = "mcc_pcf";
static const char *TAG_TCA = "mcc_tca";

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

struct FullSnapshot {
  bool valid{false};
  uint8_t pcf{0};
  bool pcf_ok{false};
  uint8_t pca_mode1{0};
  uint8_t pca_mode2{0};
  uint8_t pca_prescale{0};
  bool pca_ok{false};
  uint16_t ina_cfg{0};
  uint16_t ina_cal{0};
  bool ina_meta_ok{false};
  SlotSnapshot slots[16];
};

static FullSnapshot baseline;

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

inline bool i2c_read_block(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  uint8_t n = Wire.requestFrom((int) addr, (int) len);
  if (n != len) return false;
  for (size_t i = 0; i < len; i++) {
    if (!Wire.available()) return false;
    buf[i] = Wire.read();
  }
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

inline bool read_bq_key_regs(SlotSnapshot &s) {
  bool ok = true;
  ok &= i2c_read_reg8(ADDR_BQ24195, 0x00, s.reg00);
  ok &= i2c_read_reg8(ADDR_BQ24195, 0x08, s.reg08);
  ok &= i2c_read_reg8(ADDR_BQ24195, 0x09, s.reg09a);
  ok &= i2c_read_reg8(ADDR_BQ24195, 0x09, s.reg09b);
  ok &= i2c_read_reg8(ADDR_BQ24195, 0x0A, s.reg0a);
  s.bq_ok = ok;
  return ok;
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

inline void read_peripheral_header(FullSnapshot &snap) {
  snap.pcf_ok = i2c_read_u8(ADDR_PCF8574, snap.pcf);
  snap.pca_ok = i2c_read_reg8(ADDR_PCA9685, 0x00, snap.pca_mode1) &&
                i2c_read_reg8(ADDR_PCA9685, 0x01, snap.pca_mode2) &&
                i2c_read_reg8(ADDR_PCA9685, 0xFE, snap.pca_prescale);
  uint16_t cfg = 0;
  uint16_t cal = 0;
  snap.ina_meta_ok = i2c_read_reg16(ADDR_INA219, 0x00, cfg) &&
                     i2c_read_reg16(ADDR_INA219, 0x05, cal);
  snap.ina_cfg = cfg;
  snap.ina_cal = cal;
}

inline void capture_slot(uint8_t slot0, SlotSnapshot &s) {
  select_tca_slot(slot0);
  mux_select(slot0 & 0x0F);
  s.a0_raw = adc_avg(8);
  s.bq_ok = i2c_present(ADDR_BQ24195);
  if (s.bq_ok) read_bq_key_regs(s);
  read_ina(s);
  disable_tcas();
}

inline void capture_full(FullSnapshot &snap) {
  configure_ina219();
  delay(10);
  read_peripheral_header(snap);
  for (uint8_t i = 0; i < 16; i++) {
    capture_slot(i, snap.slots[i]);
    feed();
  }
  snap.valid = true;
}

inline void log_header(const FullSnapshot &snap, const char *title) {
  ESP_LOGW(TAG_SNAP, "========== %s ==========%s", title, baseline.valid ? "" : "");
  ESP_LOGI(TAG_SNAP,
           "main: PCF8574=%s 0x%02X bits[P7..P0]=%d%d%d%d%d%d%d%d | PCA9685=%s MODE1=0x%02X MODE2=0x%02X PRE=0x%02X | INA=%s CFG=0x%04X CAL=0x%04X",
           yesno(snap.pcf_ok), snap.pcf,
           (snap.pcf >> 7) & 1, (snap.pcf >> 6) & 1, (snap.pcf >> 5) & 1, (snap.pcf >> 4) & 1,
           (snap.pcf >> 3) & 1, (snap.pcf >> 2) & 1, (snap.pcf >> 1) & 1, snap.pcf & 1,
           yesno(snap.pca_ok), snap.pca_mode1, snap.pca_mode2, snap.pca_prescale,
           yesno(snap.ina_meta_ok), snap.ina_cfg, snap.ina_cal);
}

inline void log_slot_line(uint8_t slot0, const SlotSnapshot &s) {
  float tc = !isnan(s.bus_v) ? tc1047_temp_c_from_v(s.bus_v) : NAN;
  ESP_LOGI(TAG_SNAP,
           "slot=%02u BQ=%s r00=0x%02X r08=0x%02X[%s DPM=%u PG=%u VSYS=%u] r09a=0x%02X r09b=0x%02X[%s WD=%u BAT=%u] r0A=0x%02X | INA=%s bus=%.3fV shunt=%.2fmV tc1047_if_bus=%.1fC | A0=%d",
           slot0 + 1,
           yesno(s.bq_ok), s.reg00, s.reg08, bq_chrg_str(s.reg08), (s.reg08 >> 3) & 1, (s.reg08 >> 2) & 1, s.reg08 & 1,
           s.reg09a, s.reg09b, bq_fault_str(s.reg09b), (s.reg09b >> 7) & 1, (s.reg09b >> 3) & 1, s.reg0a,
           yesno(s.ina_ok), s.bus_v, s.shunt_mv, tc, s.a0_raw);
}

inline void log_delta_line(uint8_t slot0, const SlotSnapshot &now, const SlotSnapshot &base) {
  float dbus_mv = (now.ina_ok && base.ina_ok) ? (now.bus_v - base.bus_v) * 1000.0f : NAN;
  float dshunt_mv = (now.ina_ok && base.ina_ok) ? now.shunt_mv - base.shunt_mv : NAN;
  int da0 = now.a0_raw - base.a0_raw;
  bool bq_changed = (now.reg00 != base.reg00) || (now.reg08 != base.reg08) || (now.reg09b != base.reg09b) || (now.reg0a != base.reg0a) || (now.bq_ok != base.bq_ok);
  ESP_LOGI(TAG_DELTA,
           "slot=%02u delta: dBUS=%+.1fmV dSHUNT=%+.2fmV dA0=%+d BQ_changed=%u now[r00=0x%02X r08=0x%02X/%s r09=0x%02X/%s r0A=0x%02X] base[r00=0x%02X r08=0x%02X r09=0x%02X r0A=0x%02X]",
           slot0 + 1, dbus_mv, dshunt_mv, da0, bq_changed ? 1 : 0,
           now.reg00, now.reg08, bq_chrg_str(now.reg08), now.reg09b, bq_fault_str(now.reg09b), now.reg0a,
           base.reg00, base.reg08, base.reg09b, base.reg0a);
}

inline void snapshot_all() {
  FullSnapshot snap;
  capture_full(snap);
  log_header(snap, "FULL SNAPSHOT ALL SLOTS");
  for (uint8_t i = 0; i < 16; i++) {
    log_slot_line(i, snap.slots[i]);
    feed();
  }
  ESP_LOGW(TAG_SNAP, "========== FULL SNAPSHOT END ==========");
}

inline void snapshot_half(uint8_t start_slot0, uint8_t count) {
  FullSnapshot snap;
  configure_ina219();
  delay(10);
  read_peripheral_header(snap);
  char title[64];
  snprintf(title, sizeof(title), "SNAPSHOT SLOTS %02u-%02u", start_slot0 + 1, start_slot0 + count);
  log_header(snap, title);
  for (uint8_t i = 0; i < count; i++) {
    uint8_t slot0 = start_slot0 + i;
    capture_slot(slot0, snap.slots[slot0]);
    log_slot_line(slot0, snap.slots[slot0]);
    feed();
  }
  ESP_LOGW(TAG_SNAP, "========== %s END ==========", title);
}

inline void set_baseline() {
  capture_full(baseline);
  log_header(baseline, "BASELINE CAPTURED");
  for (uint8_t i = 0; i < 16; i++) {
    log_slot_line(i, baseline.slots[i]);
    feed();
  }
  ESP_LOGW(TAG_SNAP, "Baseline stored in RAM. It will be lost after reboot/OTA.");
}

inline void compare_to_baseline() {
  if (!baseline.valid) {
    ESP_LOGE(TAG_DELTA, "No baseline stored. Run 'Diag 00 Set Baseline Snapshot' first.");
    return;
  }
  FullSnapshot now;
  capture_full(now);
  log_header(now, "COMPARE CURRENT SNAPSHOT AGAINST BASELINE");
  ESP_LOGW(TAG_DELTA,
           "peripherals delta: PCF 0x%02X->0x%02X | PCA MODE1 0x%02X->0x%02X MODE2 0x%02X->0x%02X PRE 0x%02X->0x%02X | INA CFG 0x%04X->0x%04X CAL 0x%04X->0x%04X",
           baseline.pcf, now.pcf,
           baseline.pca_mode1, now.pca_mode1, baseline.pca_mode2, now.pca_mode2, baseline.pca_prescale, now.pca_prescale,
           baseline.ina_cfg, now.ina_cfg, baseline.ina_cal, now.ina_cal);
  for (uint8_t i = 0; i < 16; i++) {
    log_delta_line(i, now.slots[i], baseline.slots[i]);
    feed();
  }
  ESP_LOGW(TAG_DELTA, "========== COMPARE END ==========");
}

inline void mux_adc_scan() {
  ESP_LOGW(TAG_MUX, "========== MUX/A0 SCAN START ==========");
  int direct = adc_avg(8);
  ESP_LOGI(TAG_MUX, "direct_before raw=%d", direct);
  for (uint8_t ch = 0; ch < 16; ch++) {
    mux_select(ch);
    int minv = 9999, maxv = -9999;
    long sum = 0;
    int samples[8];
    for (uint8_t i = 0; i < 8; i++) {
      int v = analogRead(A0);
      samples[i] = v;
      sum += v;
      if (v < minv) minv = v;
      if (v > maxv) maxv = v;
      delay(2);
    }
    float avg = sum / 8.0f;
    ESP_LOGI(TAG_MUX,
             "ch=%02u addr=%u bits[S3S2S1S0]=%u%u%u%u avg=%.1f min=%d max=%d spread=%d first=[%d,%d,%d,%d,%d,%d,%d,%d]",
             ch + 1, ch, (ch >> 3) & 1, (ch >> 2) & 1, (ch >> 1) & 1, ch & 1,
             avg, minv, maxv, maxv - minv,
             samples[0], samples[1], samples[2], samples[3], samples[4], samples[5], samples[6], samples[7]);
    feed();
  }
  ESP_LOGW(TAG_MUX, "========== MUX/A0 SCAN END ==========");
}

inline void dump_pcf() {
  ESP_LOGW(TAG_PCF, "========== PCF8574 READ START ==========");
  uint8_t first = 0, last = 0, minv = 0xFF, maxv = 0x00;
  int transitions = 0;
  bool have_prev = false;
  uint8_t prev = 0;
  for (uint8_t i = 0; i < 32; i++) {
    uint8_t v = 0;
    bool ok = i2c_read_u8(ADDR_PCF8574, v);
    if (i == 0) first = v;
    last = v;
    if (v < minv) minv = v;
    if (v > maxv) maxv = v;
    if (have_prev && v != prev) transitions++;
    prev = v;
    have_prev = true;
    ESP_LOGI(TAG_PCF, "sample=%02u ok=%s value=0x%02X bits[P7..P0]=%u%u%u%u%u%u%u%u",
             i, yesno(ok), v,
             (v >> 7) & 1, (v >> 6) & 1, (v >> 5) & 1, (v >> 4) & 1,
             (v >> 3) & 1, (v >> 2) & 1, (v >> 1) & 1, v & 1);
    delay(20);
    feed();
  }
  ESP_LOGW(TAG_PCF, "summary: first=0x%02X last=0x%02X min=0x%02X max=0x%02X transitions=%d", first, last, minv, maxv, transitions);
  ESP_LOGW(TAG_PCF, "========== PCF8574 READ END ==========");
}

inline void dump_pca() {
  ESP_LOGW(TAG_PCA, "========== PCA9685 READ-ONLY DUMP START ==========");
  uint8_t mode1 = 0, mode2 = 0, pre = 0;
  i2c_read_reg8(ADDR_PCA9685, 0x00, mode1);
  i2c_read_reg8(ADDR_PCA9685, 0x01, mode2);
  i2c_read_reg8(ADDR_PCA9685, 0xFE, pre);
  ESP_LOGI(TAG_PCA, "MODE1=0x%02X MODE2=0x%02X PRE_SCALE=0x%02X | MODE1[SLEEP=%u AI=%u ALLCALL=%u] MODE2[OUTDRV=%u OUTNE=%u]",
           mode1, mode2, pre, (mode1 >> 4) & 1, (mode1 >> 5) & 1, mode1 & 1, (mode2 >> 2) & 1, mode2 & 0x03);
  for (uint8_t ch = 0; ch < 16; ch++) {
    uint8_t b[4] = {0};
    bool ok = i2c_read_block(ADDR_PCA9685, (uint8_t) (0x06 + 4 * ch), b, 4);
    uint16_t on = ((uint16_t)(b[1] & 0x0F) << 8) | b[0];
    uint16_t off = ((uint16_t)(b[3] & 0x0F) << 8) | b[2];
    bool full_on = b[1] & 0x10;
    bool full_off = b[3] & 0x10;
    int duty = 0;
    if (full_on) duty = 4096;
    else if (full_off) duty = 0;
    else duty = (int) ((off + 4096 - on) & 0x0FFF);
    ESP_LOGI(TAG_PCA, "ch=%02u ok=%s regs=[0x%02X 0x%02X 0x%02X 0x%02X] full_on=%u full_off=%u on=%u off=%u approx_duty=%d/4096",
             ch, yesno(ok), b[0], b[1], b[2], b[3], full_on ? 1 : 0, full_off ? 1 : 0, on, off, duty);
    feed();
  }
  ESP_LOGW(TAG_PCA, "========== PCA9685 READ-ONLY DUMP END ==========");
}

inline void strict_tca_bq_map() {
  ESP_LOGW(TAG_TCA, "========== STRICT TCA -> BQ24195 MAP START ==========");
  disable_tcas();
  uint8_t c70 = 0, c71 = 0;
  bool bq0 = i2c_present(ADDR_BQ24195);
  i2c_read_u8(ADDR_TCA0, c70);
  i2c_read_u8(ADDR_TCA1, c71);
  ESP_LOGW(TAG_TCA, "baseline disabled: ctrl70=0x%02X ctrl71=0x%02X BQ_present=%u", c70, c71, bq0 ? 1 : 0);
  int count = 0;
  for (uint8_t i = 0; i < 16; i++) {
    uint8_t tca = tca_addr_for_slot(i);
    uint8_t ch = tca_channel_for_slot(i);
    uint8_t mask = tca_mask_for_slot(i);
    disable_tcas();
    bool sel = i2c_write_u8(tca, mask);
    delay(4);
    i2c_read_u8(ADDR_TCA0, c70);
    i2c_read_u8(ADDR_TCA1, c71);
    SlotSnapshot s;
    s.bq_ok = i2c_present(ADDR_BQ24195);
    if (s.bq_ok) {
      count++;
      read_bq_key_regs(s);
    }
    ESP_LOGI(TAG_TCA, "slot=%02u ref=%s target_tca=0x%02X ch=%u mask=0x%02X select=%s ctrl70=0x%02X ctrl71=0x%02X BQ=%u r00=0x%02X r08=0x%02X r09b=0x%02X r0A=0x%02X",
             i + 1, tca_ref_for_slot(i), tca, ch, mask, yesno(sel), c70, c71, s.bq_ok ? 1 : 0, s.reg00, s.reg08, s.reg09b, s.reg0a);
    feed();
  }
  disable_tcas();
  ESP_LOGW(TAG_TCA, "strict result: BQ visible count=%d/16", count);
  ESP_LOGW(TAG_TCA, "========== STRICT TCA -> BQ24195 MAP END ==========");
}


static const char *TAG_BQFULL = "mcc_bqfull";
static const char *TAG_TRACE = "mcc_trace";
static const char *TAG_PCATRACE = "mcc_pcatrace";

struct BQFullRegs {
  bool ok{false};
  uint8_t r[11]{0};
};

inline void wait_with_yield(uint32_t ms) {
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < ms) {
    delay(10);
    yield();
  }
}

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

inline void dump_bq_full_all() {
  ESP_LOGW(TAG_BQFULL, "========== BQ24195 FULL REGISTER DUMP ALL SLOTS START ==========");
  ESP_LOGW(TAG_BQFULL, "Read-only except TCA channel select and INA219 config. Reads BQ regs 00..0A for each logical slot.");
  configure_ina219();
  for (uint8_t i = 0; i < 16; i++) {
    SlotSnapshot s;
    BQFullRegs bq;
    capture_slot_full(i, s, bq);
    log_bq_full_line(i, s, bq, "dump", 0);
    feed();
  }
  ESP_LOGW(TAG_BQFULL, "========== BQ24195 FULL REGISTER DUMP ALL SLOTS END ==========");
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

inline void trace_one_slot(uint8_t slot0, uint8_t cycles, uint16_t interval_ms) {
  ESP_LOGW(TAG_TRACE, "========== TRACE ONE LOGICAL SLOT START slot=%02u cycles=%u interval=%ums =========", slot0 + 1, cycles, interval_ms);
  ESP_LOGW(TAG_TRACE, "Use this while inserting/removing a cell or heating one sensor. No BQ/PCA/PCF writes are performed.");
  configure_ina219();
  BQFullRegs prev_bq;
  SlotSnapshot prev_s;
  bool have_prev = false;
  for (uint8_t sample = 0; sample < cycles; sample++) {
    SlotSnapshot s;
    BQFullRegs bq;
    capture_slot_full(slot0, s, bq);
    log_bq_full_line(slot0, s, bq, "trace1", sample);
    if (have_prev) {
      bool regs_changed = false;
      for (uint8_t r = 0; r <= 0x0A; r++) regs_changed |= (bq.r[r] != prev_bq.r[r]);
      float dbus = (s.ina_ok && prev_s.ina_ok) ? (s.bus_v - prev_s.bus_v) * 1000.0f : NAN;
      float dshunt = (s.ina_ok && prev_s.ina_ok) ? s.shunt_mv - prev_s.shunt_mv : NAN;
      ESP_LOGI(TAG_TRACE, "trace1_delta sample=%02u slot=%02u regs_changed=%u dBUS=%+.1fmV dSHUNT=%+.2fmV dA0=%+d prev_r08=0x%02X now_r08=0x%02X prev_r09=0x%02X now_r09=0x%02X",
               sample, slot0 + 1, regs_changed ? 1 : 0, dbus, dshunt, s.a0_raw - prev_s.a0_raw, prev_bq.r[8], bq.r[8], prev_s.reg09b, s.reg09b);
    }
    prev_bq = bq;
    prev_s = s;
    have_prev = true;
    if (sample + 1 < cycles) wait_with_yield(interval_ms);
  }
  ESP_LOGW(TAG_TRACE, "========== TRACE ONE LOGICAL SLOT END slot=%02u =========", slot0 + 1);
}

inline void trace_half_slots(uint8_t start_slot0, uint8_t count, uint8_t cycles, uint16_t interval_ms) {
  ESP_LOGW(TAG_TRACE, "========== TRACE SLOTS %02u-%02u START cycles=%u interval=%ums =========", start_slot0 + 1, start_slot0 + count, cycles, interval_ms);
  ESP_LOGW(TAG_TRACE, "This follows the original MCC limitation idea: trace max 8 logical slots per run.");
  configure_ina219();
  uint8_t prev_r08[16] = {0};
  uint8_t prev_r09[16] = {0};
  float prev_bus[16] = {0};
  int prev_a0[16] = {0};
  bool have_prev = false;
  for (uint8_t sample = 0; sample < cycles; sample++) {
    ESP_LOGW(TAG_TRACE, "-- trace_half sample=%02u slots=%02u-%02u --", sample, start_slot0 + 1, start_slot0 + count);
    for (uint8_t j = 0; j < count; j++) {
      uint8_t slot0 = start_slot0 + j;
      SlotSnapshot s;
      BQFullRegs bq;
      capture_slot_full(slot0, s, bq);
      float dbus = have_prev && s.ina_ok ? (s.bus_v - prev_bus[slot0]) * 1000.0f : 0.0f;
      int da0 = have_prev ? s.a0_raw - prev_a0[slot0] : 0;
      bool bq_changed = have_prev ? ((bq.r[8] != prev_r08[slot0]) || (s.reg09b != prev_r09[slot0])) : false;
      ESP_LOGI(TAG_TRACE,
               "sample=%02u slot=%02u r00=0x%02X r01=0x%02X r02=0x%02X r03=0x%02X r04=0x%02X r05=0x%02X r06=0x%02X r07=0x%02X r08=0x%02X/%s r09=0x%02X/%s r0A=0x%02X bus=%.3fV dBUS=%+.1fmV shunt=%.2fmV A0=%d dA0=%+d changed=%u",
               sample, slot0 + 1,
               bq.r[0], bq.r[1], bq.r[2], bq.r[3], bq.r[4], bq.r[5], bq.r[6], bq.r[7], bq.r[8], bq_chrg_str(bq.r[8]), s.reg09b, bq_fault_str(s.reg09b), bq.r[10],
               s.bus_v, dbus, s.shunt_mv, s.a0_raw, da0, bq_changed ? 1 : 0);
      prev_r08[slot0] = bq.r[8];
      prev_r09[slot0] = s.reg09b;
      prev_bus[slot0] = s.bus_v;
      prev_a0[slot0] = s.a0_raw;
      feed();
    }
    have_prev = true;
    if (sample + 1 < cycles) wait_with_yield(interval_ms);
  }
  ESP_LOGW(TAG_TRACE, "========== TRACE SLOTS %02u-%02u END =========", start_slot0 + 1, start_slot0 + count);
}

inline int pca_channel_duty_from_bytes(const uint8_t b[4]) {
  uint16_t on = ((uint16_t)(b[1] & 0x0F) << 8) | b[0];
  uint16_t off = ((uint16_t)(b[3] & 0x0F) << 8) | b[2];
  bool full_on = b[1] & 0x10;
  bool full_off = b[3] & 0x10;
  if (full_on) return 4096;
  if (full_off) return 0;
  return (int)((off + 4096 - on) & 0x0FFF);
}

inline void trace_pca(uint8_t cycles, uint16_t interval_ms) {
  ESP_LOGW(TAG_PCATRACE, "========== PCA9685 LED/PWM TRACE START cycles=%u interval=%ums =========", cycles, interval_ms);
  ESP_LOGW(TAG_PCATRACE, "Read-only. Use this while watching green/red LEDs to correlate duty changes with visible LED states.");
  int prev_duty[16];
  for (uint8_t i = 0; i < 16; i++) prev_duty[i] = -1;
  for (uint8_t sample = 0; sample < cycles; sample++) {
    uint8_t mode1 = 0, mode2 = 0;
    i2c_read_reg8(ADDR_PCA9685, 0x00, mode1);
    i2c_read_reg8(ADDR_PCA9685, 0x01, mode2);
    char line[360];
    int pos = snprintf(line, sizeof(line), "sample=%02u MODE1=0x%02X MODE2=0x%02X duties=", sample, mode1, mode2);
    int changes = 0;
    for (uint8_t ch = 0; ch < 16; ch++) {
      uint8_t b[4] = {0};
      bool ok = i2c_read_block(ADDR_PCA9685, (uint8_t)(0x06 + 4 * ch), b, 4);
      int duty = ok ? pca_channel_duty_from_bytes(b) : -1;
      if (prev_duty[ch] >= 0 && duty != prev_duty[ch]) changes++;
      prev_duty[ch] = duty;
      if (pos < (int)sizeof(line) - 16) {
        pos += snprintf(line + pos, sizeof(line) - pos, "%02u:%04d%s", ch, duty, ch == 15 ? "" : " ");
      }
      feed();
    }
    ESP_LOGI(TAG_PCATRACE, "%s changes=%d", line, changes);
    if (sample + 1 < cycles) wait_with_yield(interval_ms);
  }
  ESP_LOGW(TAG_PCATRACE, "========== PCA9685 LED/PWM TRACE END =========");
}

}  // namespace mccdiag

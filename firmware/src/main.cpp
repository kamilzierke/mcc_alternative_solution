#include <Arduino.h>
#include <Wire.h>

#include "mcc_diag_contract.h"
#include "mcc_bq_probe_plan.h"
#include "mcc_firmware_core.h"
#include "mcc_i2c_probe_plan.h"

namespace {

constexpr uint8_t kMuxS0 = 13;
constexpr uint8_t kMuxS1 = 12;
constexpr uint8_t kMuxS2 = 14;
constexpr uint8_t kMuxS3 = 16;
constexpr uint8_t kI2cSda = 4;
constexpr uint8_t kI2cScl = 5;
constexpr uint32_t kStatusIntervalMs = 5000;

uint32_t last_status_ms = 0;

#ifndef MCC_ENABLE_MAIN_BUS_PROBE
#define MCC_ENABLE_MAIN_BUS_PROBE 0
#endif

#ifndef MCC_ENABLE_TCA_STATE_PROBE
#define MCC_ENABLE_TCA_STATE_PROBE 0
#endif

#ifndef MCC_ENABLE_BQ_C01_IDENTITY_PROBE
#define MCC_ENABLE_BQ_C01_IDENTITY_PROBE 0
#endif

#ifndef MCC_ENABLE_BQ_C01_SNAPSHOT_PROBE
#define MCC_ENABLE_BQ_C01_SNAPSHOT_PROBE 0
#endif

#if MCC_ENABLE_MAIN_BUS_PROBE
uint8_t main_bus_probe_results[mccfw::kMainBusProbeTargetCount]{};
#endif

#if MCC_ENABLE_TCA_STATE_PROBE
uint8_t tca_control_register_values[mccfw::kTcaControlRegisterProbeTargetCount]{};
uint8_t tca_control_register_bytes_read[mccfw::kTcaControlRegisterProbeTargetCount]{};
#endif

#if MCC_ENABLE_BQ_C01_IDENTITY_PROBE
uint8_t bq_c01_identity_value = 0xFF;
uint8_t bq_c01_identity_bytes_read = 0;
uint8_t bq_c01_pointer_result = 0xFF;
uint8_t bq_c01_select_result = 0xFF;
uint8_t bq_c01_release_tca0_result = 0xFF;
uint8_t bq_c01_release_tca1_result = 0xFF;
#endif

#if MCC_ENABLE_BQ_C01_SNAPSHOT_PROBE
uint8_t bq_c01_register_values[0x0B]{};
uint8_t bq_c01_register_read_counts[0x0B]{};
uint8_t bq_c01_snapshot_select_result = 0xFF;
uint8_t bq_c01_snapshot_release_tca0_result = 0xFF;
uint8_t bq_c01_snapshot_release_tca1_result = 0xFF;
#endif

void emit_event(const char *level, const char *component, const char *message) {
  Serial.printf("MCC|EVENT|%s|%s|%s\n", level, component, message);
}

void emit_component(const mccfw::ComponentStatus &component) {
  const char *observed = mccfw::state_name(component.state);
  Serial.printf("MCC|STATUS|%s|%s|%s|reported|%s|%s\n", component.name, observed, observed,
                mccfw::comparison_name(mccfw::ComparisonState::Match), component.detail);
}

void emit_slot_status(uint8_t slot0) {
  Serial.printf("MCC|SLOT|C%02u|not-sampled|read-only|no I2C read scheduled|awaiting|no I2C read scheduled\n", slot0 + 1);
}

#if MCC_ENABLE_MAIN_BUS_PROBE
void emit_main_bus_probe_status(const mccfw::I2cProbeTarget &target, uint8_t wire_result) {
  const bool acknowledged = mccfw::main_bus_probe_acknowledged(wire_result);
  Serial.printf("MCC|STATUS|%s|%s|address-only probe|%s|%s|I2C address 0x%02X returned Wire code %u\n",
                target.component, acknowledged ? "ack" : "no-ack", acknowledged ? "detected" : "unavailable",
                acknowledged ? "match" : "mismatch", target.address, wire_result);
}

void probe_confirmed_main_bus_targets() {
  for (size_t index = 0; index < mccfw::kMainBusProbeTargetCount; index++) {
    const auto &target = mccfw::kMainBusProbeTargets[index];
    Wire.beginTransmission(target.address);
    main_bus_probe_results[index] = Wire.endTransmission(true);
    emit_main_bus_probe_status(target, main_bus_probe_results[index]);
  }
  emit_event("INFO", "i2c", "address-only main-bus probe completed; no I2C data bytes were written");
}

void emit_cached_main_bus_probe_results() {
  for (size_t index = 0; index < mccfw::kMainBusProbeTargetCount; index++) {
    emit_main_bus_probe_status(mccfw::kMainBusProbeTargets[index], main_bus_probe_results[index]);
  }
}
#endif

#if MCC_ENABLE_BQ_C01_SNAPSHOT_PROBE
uint8_t write_tca_snapshot_selection(uint8_t address, uint8_t selection) {
  Wire.beginTransmission(address);
  Wire.write(selection);
  return Wire.endTransmission(true);
}

void release_all_tca_snapshot_channels() {
  bq_c01_snapshot_release_tca0_result = write_tca_snapshot_selection(0x70, 0x00);
  bq_c01_snapshot_release_tca1_result = write_tca_snapshot_selection(0x71, 0x00);
}

void emit_bq_c01_snapshot_status() {
  bool registers_ok = true;
  for (uint8_t reg = mccfw::kBqReadableFirstRegister; reg <= mccfw::kBqReadableLastRegister; reg++) {
    registers_ok = registers_ok && bq_c01_register_read_counts[reg] == 1;
    Serial.printf("MCC|STATUS|BQ24195 C01 REG%02X|0x%02X|read-only|register read|%s|one-byte read through TCA 0x70 channel 0\n",
                  reg, bq_c01_register_values[reg], bq_c01_register_read_counts[reg] == 1 ? "match" : "mismatch");
  }
  const bool released = bq_c01_snapshot_release_tca0_result == 0 && bq_c01_snapshot_release_tca1_result == 0;
  const char *match = registers_ok && bq_c01_snapshot_select_result == 0 && released ? "match" : "mismatch";
  char summary[240];
  mccdiag::bq_snapshot_summary(bq_c01_register_values, summary, sizeof(summary));
  Serial.printf("MCC|SLOT|C01|BQ REG00..REG0A|read-only|register snapshot|%s|%s; select=%u release70=%u release71=%u\n",
                match, summary, bq_c01_snapshot_select_result, bq_c01_snapshot_release_tca0_result, bq_c01_snapshot_release_tca1_result);
}

void probe_bq_c01_snapshot() {
  release_all_tca_snapshot_channels();
  bq_c01_snapshot_select_result = write_tca_snapshot_selection(mccfw::kBqC01IdentityProbe.tca_address,
                                                                mccfw::kBqC01IdentityProbe.tca_channel_mask);
  if (bq_c01_snapshot_select_result == 0) {
    for (uint8_t reg = mccfw::kBqReadableFirstRegister; reg <= mccfw::kBqReadableLastRegister; reg++) {
      Wire.beginTransmission(mccfw::kBqC01IdentityProbe.bq_address);
      Wire.write(reg);
      const uint8_t pointer_result = Wire.endTransmission(false);
      if (pointer_result == 0) {
        bq_c01_register_read_counts[reg] = Wire.requestFrom(static_cast<int>(mccfw::kBqC01IdentityProbe.bq_address), 1);
        bq_c01_register_values[reg] = bq_c01_register_read_counts[reg] == 1 ? Wire.read() : 0xFF;
      }
    }
  }
  release_all_tca_snapshot_channels();
  emit_bq_c01_snapshot_status();
  emit_event("INFO", "BQ24195 C01", "REG00..REG0A read-only snapshot completed; both TCA switches released");
}
#endif

#if MCC_ENABLE_TCA_STATE_PROBE
void emit_tca_control_register_status(const mccfw::I2cProbeTarget &target, uint8_t bytes_read, uint8_t value) {
  const bool idle = bytes_read == 1 && mccfw::tca_control_register_is_idle(value);
  Serial.printf("MCC|STATUS|%s|0x%02X|0x00|control register read|%s|read %u byte; no channel-selection write issued\n",
                target.component, value, idle ? "match" : "mismatch", bytes_read);
}

void read_tca_control_registers() {
  for (size_t index = 0; index < mccfw::kTcaControlRegisterProbeTargetCount; index++) {
    const auto &target = mccfw::kTcaControlRegisterProbeTargets[index];
    tca_control_register_bytes_read[index] = Wire.requestFrom(static_cast<int>(target.address), 1);
    tca_control_register_values[index] = tca_control_register_bytes_read[index] == 1 ? Wire.read() : 0xFF;
    emit_tca_control_register_status(target, tca_control_register_bytes_read[index], tca_control_register_values[index]);
  }
  emit_event("INFO", "i2c", "TCA control-register reads completed; no I2C write was issued");
}

void emit_cached_tca_control_register_results() {
  for (size_t index = 0; index < mccfw::kTcaControlRegisterProbeTargetCount; index++) {
    emit_tca_control_register_status(mccfw::kTcaControlRegisterProbeTargets[index], tca_control_register_bytes_read[index],
                                     tca_control_register_values[index]);
  }
}
#endif

#if MCC_ENABLE_BQ_C01_IDENTITY_PROBE
uint8_t write_tca_selection(uint8_t address, uint8_t selection) {
  Wire.beginTransmission(address);
  Wire.write(selection);
  return Wire.endTransmission(true);
}

void release_all_tca_channels() {
  bq_c01_release_tca0_result = write_tca_selection(0x70, 0x00);
  bq_c01_release_tca1_result = write_tca_selection(0x71, 0x00);
}

void emit_bq_c01_identity_status() {
  const bool read_ok = bq_c01_select_result == 0 && bq_c01_pointer_result == 0 && bq_c01_identity_bytes_read == 1;
  const bool release_ok = bq_c01_release_tca0_result == 0 && bq_c01_release_tca1_result == 0;
  const char *match = read_ok && release_ok ? "match" : "mismatch";
  Serial.printf("MCC|STATUS|BQ24195 C01|REG0A=0x%02X|read-only REG0A|identity read|%s|select=%u pointer=%u bytes=%u release70=%u release71=%u\n",
                bq_c01_identity_value, match, bq_c01_select_result, bq_c01_pointer_result, bq_c01_identity_bytes_read,
                bq_c01_release_tca0_result, bq_c01_release_tca1_result);
  Serial.printf("MCC|SLOT|C01|REG0A=0x%02X|read-only|BQ identity probe|%s|TCA 0x70 channel 0 selected only during read; both TCA switches released\n",
                bq_c01_identity_value, match);
}

void probe_bq_c01_identity() {
  release_all_tca_channels();
  bq_c01_select_result = write_tca_selection(mccfw::kBqC01IdentityProbe.tca_address,
                                               mccfw::kBqC01IdentityProbe.tca_channel_mask);
  if (bq_c01_select_result == 0) {
    Wire.beginTransmission(mccfw::kBqC01IdentityProbe.bq_address);
    Wire.write(mccfw::kBqC01IdentityProbe.register_address);
    bq_c01_pointer_result = Wire.endTransmission(false);
    if (bq_c01_pointer_result == 0) {
      bq_c01_identity_bytes_read = Wire.requestFrom(static_cast<int>(mccfw::kBqC01IdentityProbe.bq_address), 1);
      bq_c01_identity_value = bq_c01_identity_bytes_read == 1 ? Wire.read() : 0xFF;
    }
  }
  release_all_tca_channels();
  emit_bq_c01_identity_status();
  emit_event("INFO", "BQ24195 C01", "REG0A identity read completed; both TCA switches released");
}
#endif

void initialize_safe_pins() {
  pinMode(kMuxS0, OUTPUT);
  pinMode(kMuxS1, OUTPUT);
  pinMode(kMuxS2, OUTPUT);
  pinMode(kMuxS3, OUTPUT);
  digitalWrite(kMuxS0, LOW);
  digitalWrite(kMuxS1, LOW);
  digitalWrite(kMuxS2, LOW);
  digitalWrite(kMuxS3, LOW);
}

void emit_status() {
  const mccfw::ComponentStatus components[] = {
      {"firmware", mccfw::ComponentState::ReadOnly, "diagnostic mode"},
      {"i2c", mccfw::ComponentState::Ready, "initialized; no probe scheduled"},
      {"mux-select", mccfw::ComponentState::ReadOnly, "S0-S3 held at channel 0"},
      {"outputs", mccfw::ComponentState::ReadOnly, "output commands disabled"},
  };
  for (const auto &component : components) emit_component(component);
#if MCC_ENABLE_MAIN_BUS_PROBE
  emit_cached_main_bus_probe_results();
#endif
#if MCC_ENABLE_TCA_STATE_PROBE
  emit_cached_tca_control_register_results();
#endif
#if MCC_ENABLE_BQ_C01_IDENTITY_PROBE
  emit_bq_c01_identity_status();
#endif
#if MCC_ENABLE_BQ_C01_SNAPSHOT_PROBE
  emit_bq_c01_snapshot_status();
#endif
  for (uint8_t slot0 = 0; slot0 < 16; slot0++) {
#if MCC_ENABLE_BQ_C01_SNAPSHOT_PROBE
    if (slot0 == 0) continue;
#endif
    emit_slot_status(slot0);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  initialize_safe_pins();
  Wire.begin(kI2cSda, kI2cScl);
  Wire.setClock(100000);
  Serial.printf("MCC|HELLO|%s|mode=read-only\n", mccfw::protocol_banner(MCC_FIRMWARE_VERSION));
  emit_event("INFO", "boot", "safe pins initialized; output commands disabled");
#if MCC_ENABLE_MAIN_BUS_PROBE
  probe_confirmed_main_bus_targets();
#elif MCC_ENABLE_TCA_STATE_PROBE
  read_tca_control_registers();
#elif MCC_ENABLE_BQ_C01_IDENTITY_PROBE
  probe_bq_c01_identity();
#elif MCC_ENABLE_BQ_C01_SNAPSHOT_PROBE
  probe_bq_c01_snapshot();
#else
  emit_event("INFO", "i2c", "main-bus probe disabled in this firmware build");
#endif
  emit_status();
}

void loop() {
  const uint32_t now = millis();
  if (now - last_status_ms >= kStatusIntervalMs) {
    last_status_ms = now;
    emit_status();
  }
}
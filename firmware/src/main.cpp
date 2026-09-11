#include <Arduino.h>
#include <Wire.h>

#include "mcc_diag_contract.h"
#include "mcc_firmware_core.h"

namespace {

constexpr uint8_t kMuxS0 = 13;
constexpr uint8_t kMuxS1 = 12;
constexpr uint8_t kMuxS2 = 14;
constexpr uint8_t kMuxS3 = 16;
constexpr uint8_t kI2cSda = 4;
constexpr uint8_t kI2cScl = 5;
constexpr uint32_t kStatusIntervalMs = 5000;

uint32_t last_status_ms = 0;

void emit_event(const char *level, const char *component, const char *message) {
  Serial.printf("MCC|EVENT|%s|%s|%s\n", level, component, message);
}

void emit_component(const mccfw::ComponentStatus &component) {
  Serial.printf("MCC|STATUS|%s|%s|%s\n", component.name, mccfw::state_name(component.state), component.detail);
}

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
}

}  // namespace

void setup() {
  Serial.begin(115200);
  initialize_safe_pins();
  Wire.begin(kI2cSda, kI2cScl);
  Wire.setClock(100000);
  Serial.printf("MCC|HELLO|%s|mode=read-only\n", mccfw::protocol_banner(MCC_FIRMWARE_VERSION));
  emit_event("INFO", "boot", "safe pins initialized; output commands disabled");
  emit_status();
}

void loop() {
  const uint32_t now = millis();
  if (now - last_status_ms >= kStatusIntervalMs) {
    last_status_ms = now;
    emit_status();
  }
}
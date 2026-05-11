# OLED Status Display Design

## Context

`esphome/mcc-pro.yaml` already configures I2C on `GPIO4`/`GPIO5` and the boot scan expects an SSD1306-compatible device at address `0x3C`. The board publishes 16 TC1047 temperature sensors with IDs `c1_tc1047_temperature` through `c16_tc1047_temperature`.

The display hardware is an SSD1306 OLED, `128x64`, I2C address `0x3C`.

## Goal

Add an onboard status display to the current ESPHome configuration. The display should show:

- Device/network status.
- Internet/Wi-Fi connection state, implemented as Wi-Fi/IP/API status rather than a separate external reachability probe.
- IP address.
- Wi-Fi signal strength.
- Temperatures for all 16 cell slots.

Because `128x64` is too small for all of this to be readable on one screen, use a three-page automatic carousel.

## Display Layout

Page 1: status

- Header: `MCC Pro` and page indicator `P1/3`.
- Wi-Fi state: connected or disconnected.
- IP address, or `no ip` when unavailable.
- RSSI in dBm.
- API status if ESPHome exposes it cleanly through a binary sensor.
- Last temperature scan cadence/status, using the configured scan interval text where practical.

The status page does not perform an outbound internet ping or DNS lookup. A connected Wi-Fi state with an IP address is treated as the device's local network/internet readiness signal.

Page 2: temperatures `C01-C08`

- Header: `Temps C01-C08` and `P2/3`.
- Four rows, two cell temperatures per row.
- Unknown or not-yet-published values display as `--.-C`.

Page 3: temperatures `C09-C16`

- Header: `Temps C09-C16` and `P3/3`.
- Four rows, two cell temperatures per row.
- Unknown or not-yet-published values display as `--.-C`.

## ESPHome Components

Add:

- `font` using a small bitmap or ESPHome bundled font suitable for a 128x64 OLED.
- `display` with `platform: ssd1306_i2c`, `model: "SSD1306 128x64"`, `address: 0x3C`, and a lambda renderer.
- `globals` with an integer carousel page index.
- An additional `interval` entry that advances the page every 5 seconds.
- Wi-Fi diagnostics:
  - `wifi_signal` sensor for RSSI.
  - `wifi_info` text sensor for IP address.
- Optional API connection binary sensor only if available in the local ESPHome version without adding fragile custom code.

The existing TC1047 scan script and temperature sensor publishing remain unchanged except for being read by the display lambda.

## Data Handling

The display lambda reads existing sensor state through `id(...)`. It must check `has_state()` before printing sensor values so boot-time and failed-read states render cleanly.

Temperature formatting uses one decimal place. Labels use fixed-width slot names (`C01`, `C02`, etc.) so the two-column layout stays stable.

## Error Handling

If Wi-Fi is disconnected, Page 1 shows `WiFi: down` and `IP: no ip`.

If RSSI is unavailable, Page 1 shows `RSSI: -- dBm`.

If a temperature has not been published yet, the corresponding cell shows `--.-C`.

## Verification

Verification should include:

- ESPHome config validation or compile for `esphome/mcc-pro.yaml`.
- Review of generated YAML sections to confirm the OLED uses the existing I2C bus and does not change TC1047 scan behavior.
- On-device smoke test after flashing: confirm the display cycles through all three pages and that I2C scan still sees expected devices including `0x3C`.

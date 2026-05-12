# ESPHome native diagnostics for MCC Pro

Date: 2026-05-12

## Status

`esphome/mcc-pro-native.yaml` is the current working diagnostic configuration.

User testing confirmed:

- Reading one selected slot works.
- Queueing and reading all 16 slots works.
- Slots above C08 work through the second TCA9548A path.

This supersedes the earlier direct-Wire web-table experiment for BQ diagnostics.

## Coverage

The current native diagnostic firmware directly handles 21 I2C/control ICs:

- 16x BQ24195 charger ICs, one per slot.
- 2x TCA9548A I2C switches at `0x70` and `0x71`.
- 1x PCF8574 expander at `0x27` for TC1047 mux enable.
- 1x INA219 U47 at `0x41`.
- 1x SSD1306 OLED at `0x3C`.

It also reads all 16 TC1047 temperature sensors through the shared 74HC4067/ADC path.
U34 INA219, PCA9685 and the remaining 74HC4067 devices are identified in the hardware notes, but they are not part of the current native slot-read workflow.

## Architecture

The native config moves the known board topology into ESPHome components:

| Function | ESPHome component | Board device |
| --- | --- | --- |
| Main I2C bus | `i2c` `main_i2c` | ESP8266 GPIO4 SDA / GPIO5 SCL |
| C01..C08 BQ branches | `tca9548a` `tca0` | address `0x70`, channels 0..7 |
| C09..C16 BQ branches | `tca9548a` `tca1` | U38, address `0x71`, channels 0..7 |
| TC1047 mux enable | `pcf8574` + `switch.gpio` | IC25 `0x27`, P0 active-low |
| BQ access | `i2c_device` | `0x6B` on each TCA channel |
| INA diagnostic access | `i2c_device` | U47 INA219 at `0x41` |
| OLED | `ssd1306_i2c` | `0x3C` on main I2C |

The helper file `mcc_diag_helpers_native.h` keeps only board-specific logic that ESPHome does not model directly:

- 74HC4067 select GPIO handling.
- TC1047 ADC averaging and conversion.
- BQ24195 status register reads.
- `/bq` queue/cache web table.
- C16 Home Assistant diagnostic entity publishing.

## Slot mapping

The working native mapping is:

| Slots | TCA address | Channels | Downstream BQ address |
| --- | ---: | --- | ---: |
| C01..C08 | `0x70` | 0..7 | `0x6B` |
| C09..C16 | `0x71` / U38 | 0..7 | `0x6B` |

Confirmed U38 details:

- A0 = 5 V / high.
- A1 = GND / low.
- A2 = GND / low.
- RESET = 5 V / high.
- SC7/SD7 route to C16 BQ24195 SCL/SDA.

## Web interface

Endpoint: `/bq`

Supported commands:

- `/bq` - show cached table, no I2C transaction.
- `/bq?slot=N` - queue one slot read, where `N` is `1..16`.
- `/bq?read_all=1` - queue all 16 slot reads.
- `/bq?clear=1` - clear pending queue.

Important implementation detail: HTTP handlers never perform I2C reads. They only update the queue and return HTML. The 250 ms ESPHome interval calls `mccdiag::process_bq_web_read_request()` from the main loop, reads one queued slot, updates cache, then returns. This avoids the ESP8266 crash seen when I2C was performed from the async webserver `ctx: sys` context.

## Per-slot read contents

Each queued slot read currently captures:

- TC1047 temperature for the selected slot.
- BQ24195 `REG00`.
- BQ24195 `REG08` charge/system status.
- BQ24195 `REG09` fault status, read twice to observe latched/cleared behavior.
- BQ24195 `REG0A` vendor/part/revision.
- U47 INA219 bus and shunt diagnostic readings.

The web table reports cached values and age. Read-all is intentionally serialized one slot per interval cycle to keep ESP8266 responsive.

## Boot behavior

The config is network-first:

- Main I2C scan is disabled by default.
- OLED update interval is `never`; updates start after WiFi and API are connected.
- BQ and TC1047 diagnostics are on demand.
- `/bq` remains lightweight at boot because it does not read I2C in the request handler.

## Safety limits

The current native diagnostics are read-oriented:

- BQ24195 writes are not used for charger control.
- INA219 write is limited to diagnostic configuration `0x399F`.
- PCF8574 control is limited to the known TC1047 mux enable P0 through the ESPHome switch.
- PCA9685 outputs remain unmapped and should not be driven.
- Active charge/discharge control remains out of scope until output mapping and a verified stop-all-outputs routine exist.

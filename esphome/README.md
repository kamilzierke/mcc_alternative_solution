# ESPHome MCC Pro diagnostics

This folder contains ESPHome configurations used during MCC Pro reverse engineering.

## Current recommended config

Use `mcc-pro-native.yaml`.

This is the current working diagnostic variant. It uses ESPHome-native components for the confirmed I2C topology:

- `tca9548a` at `0x70`: BQ24195 branches for C01..C08.
- `tca9548a` at `0x71` / U38: BQ24195 branches for C09..C16.
- `pcf8574` at `0x27` / IC25: active-low HC4067 enables are written as full byte masks.
- `i2c_device`: BQ24195 devices at `0x6B` behind each TCA channel, external INA219 at `0x41` and internal INA219 at `0x4F`.
- `ssd1306_i2c` at `0x3C`: local status display.

Confirmed behavior:

- `/bq` serves a lightweight table only; it does not perform I2C reads inside the async web request handler.
- `/bq?slot=N` queues one slot read.
- `/bq?read_all=1` queues all 16 slots.
- `/status` serves a compact status table and accepts read-all.
- `/c16` serves raw C16 diagnostics and manual HC4067 hold controls.
- Every row also has queued BQ action columns before the BQ data columns: `ON`, `OFF`, `WD`, four input-current presets and four charge-current presets.
- Input-current presets write BQ24195 `REG00[2:0] IINLIM`: 500 mA, 900 mA, 1.5 A or 3.0 A.
- Charge-current presets write BQ24195 `REG02[7:2] ICHG`: 512 mA, 1024 mA, 2048 mA or 3008 mA.
- Read/action URLs redirect back to plain `/bq` after queueing, so refresh does not repeat a command.
- `Charge ON` and `Charge OFF` disable the BQ24195 watchdog before changing `CHG_CONFIG`, preventing the charger IC from restoring default charge-control bits after watchdog timeout.
- The `/bq` table uses two header rows and decodes BQ24195 registers, including `REG02` charge current, into short human-readable values with raw hex kept in parentheses.
- A 250 ms ESPHome interval drains queued hardware work from the main loop.
- Each full slot read captures internal TC1047 temperature, external temperature, internal INA219, external INA219 and BQ24195 `REG00..REG0A`.
- A lightweight auto poll queues C01..C16 every 10 seconds after WiFi/API readiness and updates internal TC1047 plus internal INA219 values.
- User testing confirmed that single-slot reads and read-all reads work.

Current captures:

- [`/bq` full slot diagnostics](../docs/screenshots/endpoint-bq-full-slot-table.png)
- [`/status` compact status table](../docs/screenshots/endpoint-status-compact-table.png)
- [`/c16` raw diagnostics](../docs/screenshots/endpoint-c16-raw-diagnostics.png)
- [Home Assistant MCC dashboard](../docs/screenshots/home-assistant-mcc-dashboard.png)

Home Assistant dashboard YAML: [docs/home-assistant/mcc-dashboard.yaml](../docs/home-assistant/mcc-dashboard.yaml).

## ESP-12F pin use

| Pin | Direction | Use |
| --- | --- | --- |
| GPIO4 | I/O | `main_i2c` SDA |
| GPIO5 | I/O | `main_i2c` SCL |
| GPIO13 | OUT | Shared HC4067 `S0`, channel bit 0 |
| GPIO12 | OUT | Shared HC4067 `S1`, channel bit 1 |
| GPIO14 | OUT | Shared HC4067 `S2`, channel bit 2 |
| GPIO16 | OUT | Shared HC4067 `S3`, channel bit 3 |
| A0 / ADC0 | IN | Analog input from U10 and U10E |
| GPIO1 / TX0 | OUT | UART logger at 115200 baud |
| GPIO3 / RX0 | IN | UART0/programming path, not application logic |

The HC4067 channel is selected with `mux_select(slot0 & 0x0F)`, so C01 maps to channel 0 and C16 maps to channel 15.

## PCF8574 mux-enable masks

The helper intentionally writes complete PCF8574 bytes with inactive lines high. Do not mix arbitrary ESPHome read-modify-write behavior with these masks.

| PCF pin | Active state | Board path | Helper mask |
| --- | --- | --- | --- |
| P0 | LOW | U10 internal TC1047 mux to A0 | `0xFE` |
| P1 | LOW | U10E external temperature mux to A0 | `0xFD` |
| P2 | LOW | U34 shunt mux for internal INA219 | `0xFB` |
| P3 | LOW | U3 external INA219 path to `0x41` | `0xF7` |
| P4 | LOW | U2 internal INA219 BAT+ path to `0x4F` | `0xEF` |
| P2+P4 | LOW | U34+U2 internal INA219 path | `0xEB` |

P5..P7 are not used by this configuration.

## C16 hardware tests

The `/bq` table exposes C16 Q34/Q35 test controls, and `/c16` exposes raw C16 diagnostics:

- Q34 OFF, 1%, 5%, 25%, 100% and 5% pulse for 1 second.
- Q35 probe does not drive Q35 directly; it forces Q34 OFF and queues a C16 read for external probing.
- Hold U2+U34/internal INA C16, hold U3/external INA C16 and release hold.

These are bring-up tools. The helper defines PCA9685 access at `0x4F`, which is also the configured internal INA219 address, so Q34 PWM tests should be treated as experimental until the address conflict is resolved on hardware.

## Component notes

Local datasheet summaries:

- [BQ24195](../docs/components/bq24195.md)
- [HC4067](../docs/components/hc4067.md)
- [INA219](../docs/components/ina219.md)
- [PCA9685](../docs/components/pca9685.md)

Screenshot index: [docs/screenshots](../docs/screenshots/README.md).

## Legacy config

The earlier direct-Wire diagnostic config and helper were moved to the local ignored `archive/legacy-esphome/` folder. They are historical reference only and are not synchronized to GitHub. The native variant is the current reference path.

## TC1047 core facts

- U10 = 74HC4067 temperature mux.
- IC25 = PCF8574 at 0x27.
- IC25 pin 4 / P0 controls U10 E active-low.
- R4 = 10k pull-up from U10 E to VCC.
- ADC effective full scale = 3.02 V.

The YAML exposes `C1 TC1047 Temperature` ... `C16 TC1047 Temperature`.

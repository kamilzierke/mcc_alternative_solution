# MegaCell Charger Alternative Solution

Open-source reverse engineering and replacement firmware notes for MegaCell Charger / MCC Pro.

Current confirmed milestone: the native ESPHome diagnostic variant can read one selected slot, queue/read all 16 slots, poll internal temperature/current diagnostics and perform limited queued BQ24195 host-control actions. It uses ESPHome-native `tca9548a`, `pcf8574` and `i2c_device` components for the confirmed I2C topology, while custom helper code keeps I2C work on the main ESPHome loop and outside the async web request context.

Confirmed control and read paths:

- 16x BQ24195 charger ICs behind two TCA9548A switches, with per-slot queued reads and limited queued writes.
- TC1047 C1..C16 internal temperature sensors through U10 74HC4067, IC25 PCF8574 P0 enable and ESP8266 A0.
- External temperature path through U10E 74HC4067, IC25 PCF8574 P1 enable and ESP8266 A0.
- Internal INA219 path through U2+U34 HC4067, IC25 PCF8574 P2+P4 enable mask `0xEB` and INA219 at `0x4F`.
- External INA219 path through U3 HC4067, IC25 PCF8574 P3 enable and INA219 at `0x41`.
- BQ24195 C1..C8 through TCA9548A at `0x70`, channels 0..7.
- BQ24195 C9..C16 through U38 TCA9548A at `0x71`, channels 0..7.
- SSD1306 OLED status display at `0x3C`.

Exposed device-control features:

- `/bq` full slot table with queued read, charge on/off, watchdog reset, input-current presets and charge-current presets.
- `/status` compact status table with read-all.
- `/c16` raw C16 diagnostics plus manual HC4067 hold modes for probing U2+U34/internal INA and U3/external INA.
- Experimental C16 Q34 PWM test actions through PCA9685-style writes to channel 15 at `0x4F`; this address currently overlaps the internal INA219 address in the helper and must be treated as a hardware-risk area until the bus conflict is resolved.

Current screenshots:

- [`/bq` full slot diagnostics](docs/screenshots/endpoint-bq-full-slot-table.png)
- [`/status` compact status table](docs/screenshots/endpoint-status-compact-table.png)
- [`/c16` raw diagnostics](docs/screenshots/endpoint-c16-raw-diagnostics.png)
- [Home Assistant MCC dashboard](docs/screenshots/home-assistant-mcc-dashboard.png)

Home Assistant dashboard YAML: [`docs/home-assistant/mcc-dashboard.yaml`](docs/home-assistant/mcc-dashboard.yaml).

ESP-12F pins used by the current native firmware:

| Pin | Direction | Firmware role |
| --- | --- | --- |
| GPIO4 | I/O | `main_i2c` SDA |
| GPIO5 | I/O | `main_i2c` SCL |
| GPIO13 | OUT | Shared HC4067 `S0` |
| GPIO12 | OUT | Shared HC4067 `S1` |
| GPIO14 | OUT | Shared HC4067 `S2` |
| GPIO16 | OUT | Shared HC4067 `S3` |
| A0 / ADC0 | IN | Analog input from U10 and U10E HC4067 paths |
| GPIO1 / TX0 | OUT | UART logger at 115200 baud |
| GPIO3 / RX0 | IN | UART0/programming path, not application logic |

The active firmware source is `esphome/mcc-pro-native.yaml` with `esphome/mcc_diag_helpers_native.h`.

## Configuration and safety

Firmware validation may use the local ESPHome configuration and compiler. Do not flash firmware, invoke OTA or perform live hardware probing until the relevant hardware procedure has been reviewed.

Create a local `esphome/secrets.yaml` from [esphome/secrets.example.yaml](esphome/secrets.example.yaml) before running ESPHome. The local secrets file is ignored and must never be committed. The active configuration can be checked without connecting to the charger:

```powershell
esphome config esphome\mcc-pro-native.yaml
esphome compile esphome\mcc-pro-native.yaml
& .\scripts\verify-repository.ps1
```

## Documentation and project policy

- [Product roadmap](ROADMAP.md): planned milestones, safety gates and completion criteria.
- [Firmware status](docs/firmware-status-and-roadmap.md): current technical scope and experimental boundaries.
- [ESPHome diagnostics](docs/esphome-native-diagnostics.md) and [component control reference](docs/component-control-reference.md): current implementation and board evidence.
- [Temperature readout](docs/temperature-readout-tc1047.md) and [slot temperature map](hardware/temperature_sensor_map.csv): confirmed TC1047 path evidence.
- [Contributing](CONTRIBUTING.md), [security policy](SECURITY.md) and [changelog](CHANGELOG.md): collaboration, private reporting and published change history.

Component notes from the local datasheets are in `docs/components/`; renamed source PDFs are in `docs/datasheets/`; current UI captures are in `docs/screenshots/`; hardware photos and diagrams are in `docs/hardware/`.

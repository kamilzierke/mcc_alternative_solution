# MegaCell Charger / MCC Pro technical spec v0.3

## Confirmed hardware blocks

- MCU: ESP-12F / ESP8266.
- 16 cell slots.
- Analog mux select lines: GPIO13=S0, GPIO12=S1, GPIO14=S2, GPIO16=S3.
- I2C bus in current working ESPHome config: GPIO4=SDA, GPIO5=SCL.
- I2C devices observed/identified on MCC Pro: 0x27 PCF8574, 0x3C OLED, 0x41 INA219 U47, 0x45 INA219 U34 by pin trace, 0x4F PCA9685, 0x70/0x71 TCA9548A.
- 16x BQ24195 found behind TCA9548A channel selection at address 0x6B.
- Native ESPHome mapping now works for both single-slot reads and all-slot reads: C1..C8 use TCA `0x70` channels 0..7, C9..C16 use U38/TCA `0x71` channels 0..7.
- Temperature sensors C1..C16 are TC1047 and are fully readable through U10.

## Confirmed temperature path v0.3

```text
TC1047 C1..C16 -> U10 74HC4067PW Y0..Y15
U10 Z          -> ESP8266 A0 ADC path
U10 E          -> IC25 PCF8574 pin 4 / P0
R4 10k         -> pull-up U10 E to VCC
```

U10 is normally off. The current native ESPHome config models IC25 as `pcf8574` and exposes P0 as an inverted internal switch named `tc1047_mux_enable`. Turning the switch on drives P0 low, enables U10, selects mux channel, reads A0, then turns the switch off.

The earlier direct-Wire sequence is equivalent: write PCF8574 `0x27` with bit 0 cleared, select mux channel, read A0, then restore previous PCF byte.

ADC effective full-scale for this path: `3.02 V`.

## Confirmed BQ path v0.4 native diagnostics

```text
ESP8266 GPIO4/GPIO5 main I2C
  -> TCA9548A 0x70 channels 0..7 -> BQ24195 C1..C8 at 0x6B
  -> TCA9548A 0x71 channels 0..7 -> BQ24195 C9..C16 at 0x6B
```

U38 is the `0x71` TCA9548A:

- A0 = 5 V / high.
- A1 = GND / low.
- A2 = GND / low.
- RESET = 5 V / high.
- SC7/SD7 route to C16 BQ24195 SCL/SDA.

`esphome/mcc-pro-native.yaml` uses ESPHome `tca9548a` to expose every downstream BQ branch as a separate virtual I2C bus, then attaches one `i2c_device` at `0x6B` per slot.

The `/bq` web endpoint is queue/cache based. The HTTP handler only queues reads and returns HTML; the main ESPHome loop drains the queue through `process_bq_web_read_request()`. This avoids doing I2C from the async webserver context on ESP8266.

Per-slot diagnostics currently read:

- TC1047 temperature.
- BQ24195 `REG00`, `REG08`, `REG09` twice, and `REG0A`.
- U47 INA219 bus/shunt diagnostic values.

User testing on 2026-05-12 confirmed that one-slot reads and all-slot reads work.

## Temperature sensor PCB refs

C1=U5, C2=U6, C3=U7, C4=U8, C5=U16, C6=U17, C7=U18, C8=U19, C9=U24, C10=U25, C11=U26, C12=U27, C13=U32, C14=U33, C15=U34? (conflicts with later U34 INA219 trace), C16=U35.

## Known unresolved items

- Voltage measurement path for cell terminals.
- PCA9685 channel purpose.
- Full PCF8574 bit map beyond IC25/P0 for U10 enable.
- MOSFET/AP3020 gate mapping.
- GPIO15 conflict: FUNC2 button vs discharge PWM.
- Safe `stop_all_outputs()`.
- C15 temperature sensor PCB ref: earlier map says U34, but later pin trace identifies U34 as INA219 at 0x45.

## Safety rule

Do not implement active charge/discharge control until a confirmed output map and `stop_all_outputs()` exist.

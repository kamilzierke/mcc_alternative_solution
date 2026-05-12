# MegaCell Charger Alternative Solution

Open-source reverse engineering and replacement firmware notes for MegaCell Charger / MCC Pro.

Current confirmed milestone: the native ESPHome diagnostic variant can read one selected slot and can queue/read all 16 slots. It uses ESPHome-native `tca9548a`, `pcf8574` and `i2c_device` components for the confirmed I2C topology, while custom helper code keeps BQ/TC1047 reads on demand and outside the async web request context.

Confirmed read paths:

- 21 I2C/control ICs handled by the native diagnostic firmware: 16x BQ24195, 2x TCA9548A, PCF8574, INA219 U47 and SSD1306 OLED.
- TC1047 C1..C16 temperature sensors through U10 74HC4067, IC25 PCF8574 P0 enable and ESP8266 A0.
- BQ24195 C1..C8 through TCA9548A at `0x70`, channels 0..7.
- BQ24195 C9..C16 through U38 TCA9548A at `0x71`, channels 0..7.
- U47 INA219 diagnostic readout at `0x41`.

Start with `esphome/mcc-pro-native.yaml`.

See `docs/esphome-native-diagnostics.md`, `docs/temperature-readout-tc1047.md` and `hardware/temperature_sensor_map.csv`.

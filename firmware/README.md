# MCC OSS read-only firmware

Minimal diagnostic firmware for ESP8266 / PlatformIO.

## Confirmed pins

- GPIO13 = mux S0
- GPIO12 = mux S1
- GPIO14 = mux S2
- GPIO16 = mux S3
- GPIO4 = I2C SDA in working ESPHome config
- GPIO5 = I2C SCL in working ESPHome config

## Confirmed TC1047 temperature path

```text
TC1047 C1..C16 -> U10 74HC4067 Y0..Y15 -> U10 Z -> A0
U10 E -> IC25 PCF8574 pin 4 / P0
R4 10k -> pull-up U10 E to VCC
```

Effective ADC full-scale for TC1047 path: `3.02 V`.

## Build

```bash
cd firmware
pio run
```

## Safety

Firmware must remain read-only for BQ/PCA/MOSFET power outputs until the board output map is fully traced.

# MegaCell Charger / MCC Pro technical spec v0.3

## Confirmed hardware blocks

- MCU: ESP-12F / ESP8266.
- 16 cell slots.
- Analog mux select lines: GPIO13=S0, GPIO12=S1, GPIO14=S2, GPIO16=S3.
- I2C bus in current working ESPHome config: GPIO4=SDA, GPIO5=SCL.
- I2C devices observed on MCC Pro: 0x27 PCF8574, 0x3C OLED, 0x41 INA219, 0x4F PCA9685, 0x70/0x71 TCA9548A.
- 16x BQ24195 found behind TCA9548A channel selection at address 0x6B.
- Temperature sensors C1..C16 are TC1047 and are fully readable through U10.

## Confirmed temperature path v0.3

```text
TC1047 C1..C16 -> U10 74HC4067PW Y0..Y15
U10 Z          -> ESP8266 A0 ADC path
U10 E          -> IC25 PCF8574 pin 4 / P0
R4 10k         -> pull-up U10 E to VCC
```

U10 is normally off. To read temperature, write PCF8574 `0x27` with bit 0 cleared, select mux channel, read A0, then restore previous PCF byte.

ADC effective full-scale for this path: `3.02 V`.

## Temperature sensor PCB refs

C1=U5, C2=U6, C3=U7, C4=U8, C5=U16, C6=U17, C7=U18, C8=U19, C9=U24, C10=U25, C11=U26, C12=U27, C13=U32, C14=U33, C15=U34, C16=U35.

## Known unresolved items

- Physical slot to BQ/TCA channel mapping.
- Voltage measurement path for cell terminals.
- PCA9685 channel purpose.
- Full PCF8574 bit map beyond IC25/P0 for U10 enable.
- MOSFET/AP3020 gate mapping.
- GPIO15 conflict: FUNC2 button vs discharge PWM.
- Safe `stop_all_outputs()`.

## Safety rule

Do not implement active charge/discharge control until a confirmed output map and `stop_all_outputs()` exist.

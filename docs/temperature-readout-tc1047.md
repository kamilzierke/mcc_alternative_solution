# MCC Pro temperature readout: TC1047 C1..C16

## Confirmed path

```text
TC1047 VOUT -> U10 74HC4067PW Y0..Y15
U10 Z       -> ESP8266 A0 ADC path
U10 E       -> IC25 PCF8574 pin 4 / P0
R4 10k      -> pull-up from U10 E to VCC
```

`U10` is disabled by default because `R4` pulls `E` high. `74HC4067 E` is active-low. To read temperature, firmware temporarily drives `IC25/P0` low through PCF8574.

## Control lines

| Signal | Connection | Status |
|---|---|---|
| S0 | ESP8266 GPIO13 | confirmed |
| S1 | ESP8266 GPIO12 | confirmed |
| S2 | ESP8266 GPIO14 | confirmed |
| S3 | ESP8266 GPIO16 | confirmed |
| E | IC25 PCF8574 P0, pin 4 | confirmed |
| Z | ESP8266 A0 ADC path | functionally confirmed |

## PCF8574 logic

```text
0xFF -> P0 high/released -> U10 E high -> mux disabled
0xFE -> P0 low           -> U10 E low  -> mux enabled
```

Safe sequence:

1. Read current PCF8574 byte from `0x27`.
2. Write `pcf_before & 0xFE` to pull P0 low.
3. Select U10 channel Y0..Y15 via S0..S3.
4. Read averaged A0.
5. Convert to voltage and temperature.
6. Restore previous PCF8574 byte.

## Slot mapping

| Cell | TC1047 PCB ref | U10 input | Channel |
|---:|---|---:|---:|
| C1 | U5 | Y0 | 0 |
| C2 | U6 | Y1 | 1 |
| C3 | U7 | Y2 | 2 |
| C4 | U8 | Y3 | 3 |
| C5 | U16 | Y4 | 4 |
| C6 | U17 | Y5 | 5 |
| C7 | U18 | Y6 | 6 |
| C8 | U19 | Y7 | 7 |
| C9 | U24 | Y8 | 8 |
| C10 | U25 | Y9 | 9 |
| C11 | U26 | Y10 | 10 |
| C12 | U27 | Y11 | 11 |
| C13 | U32 | Y12 | 12 |
| C14 | U33 | Y13 | 13 |
| C15 | U34 | Y14 | 14 |
| C16 | U35 | Y15 | 15 |

## TC1047 conversion

```text
VOUT = 0.500 V + 0.010 V/C * T
T[C] = (VOUT - 0.500 V) / 0.010 V
```

ADC effective full-scale for this board/path is calibrated to about `3.02 V`.

```text
VOUT = raw / 1023 * 3.02
T = (VOUT - 0.500) / 0.010
```

Calibration evidence: ESPHome voltage calculated with 3.30 V scale was 0.8761 V while multimeter showed 0.801 V. Corrected full-scale is `3.30 * 0.801 / 0.8761 = 3.017 V`.

## Why earlier reads returned raw 0/1

Earlier tests selected S0..S3 only. They did not enable U10. Since R4 keeps E high, U10 was off and Yx was not connected to Z. Pulling IC25/P0 low solved the issue.

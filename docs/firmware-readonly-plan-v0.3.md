# Firmware OSS read-only plan v0.3

## Goal

Run MCC Pro with replacement firmware without activating power outputs. Current safe scope:

- I2C scan and diagnostics.
- TCA9548A channel scan.
- BQ24195 register read-only diagnostics.
- PCA9685 register read-only diagnostics.
- PCF8574 diagnostics.
- Confirmed TC1047 temperature readout for all 16 slots.

## Temperature readout algorithm

```text
1. Read PCF8574 0x27 current byte.
2. Write current_byte & 0xFE to pull IC25/P0 low.
3. Select U10 channel Y0..Y15 using GPIO13/12/14/16.
4. Wait for mux settling.
5. Average A0 samples.
6. Convert: V = raw/1023*3.02; T = (V-0.500)/0.010.
7. Restore previous PCF8574 byte.
```

## Do not yet implement

- BQ24195 configuration writes.
- PCA9685 output writes.
- Random PCF8574 bit sweeps with cells installed.
- Charge/discharge commands.
- GPIO15 PWM.

First implement and verify `stop_all_outputs()` by measurement.

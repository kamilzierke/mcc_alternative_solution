# ESPHome MCC Pro diagnostics

This folder contains ESPHome configurations used during MCC Pro reverse engineering.

## Current recommended config

Use `mcc-pro-native.yaml`.

This is the current working diagnostic variant. It uses ESPHome-native components for the confirmed I2C topology:

- `tca9548a` at `0x70`: BQ24195 branches for C01..C08.
- `tca9548a` at `0x71` / U38: BQ24195 branches for C09..C16.
- `pcf8574` at `0x27` / IC25: P0 controls U10 TC1047 analog mux enable.
- `i2c_device`: BQ24195 devices at `0x6B` behind each TCA channel and U47 INA219 at `0x41`.

Confirmed behavior:

- `/bq` serves a lightweight table only; it does not perform I2C reads inside the async web request handler.
- `/bq?slot=N` queues one slot read.
- `/bq?read_all=1` queues all 16 slots.
- A 250 ms ESPHome interval drains the queue from the main loop.
- Each slot read captures TC1047 temperature, selected BQ24195 status registers and U47 INA219 diagnostic values.
- User testing confirmed that single-slot reads and read-all reads work.

## Legacy config

`mcc-pro.yaml` is the earlier direct-Wire diagnostic config. It is retained for history and comparison, but the native variant is the current reference path.

## TC1047 core facts

Core facts:

- U10 = 74HC4067 temperature mux.
- IC25 = PCF8574 at 0x27.
- IC25 pin 4 / P0 controls U10 E active-low.
- R4 = 10k pull-up from U10 E to VCC.
- ADC effective full scale = 3.02 V.

The YAML exposes `C1 TC1047 Temperature` ... `C16 TC1047 Temperature`.

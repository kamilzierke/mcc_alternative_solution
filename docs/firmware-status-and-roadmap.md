# Firmware status and roadmap

## Current active firmware path

The current working MCC Pro firmware path is ESPHome native:

- `esphome/mcc-pro-native.yaml`
- `esphome/mcc_diag_helpers_native.h`

This variant uses ESPHome `tca9548a`, `pcf8574`, `i2c_device` and `ssd1306_i2c` components for the confirmed board topology. Board-specific logic stays in the native helper and is executed from the ESPHome main loop, not from the async web request context.

Current confirmed scope:

- Read one selected slot.
- Queue/read all 16 slots.
- Lightweight auto polling of internal TC1047 temperature and internal INA219 values.
- Full slot reads with internal TC1047, external temperature, internal INA219, external INA219 and BQ24195 `REG00..REG0A`.
- Limited queued BQ24195 host-control writes: charge on/off, watchdog reset/disable, input-current presets and charge-current presets.
- OLED status display.
- Web endpoints `/bq`, `/status` and `/c16`.

## Current safety boundaries

Still unresolved or experimental:

- PCA9685/Q34 tests share the unresolved `0x4F` address risk with the internal INA219 path.
- GPIO15 remains high-risk because saved material mentions a possible discharge PWM role on related hardware.
- Full charge/discharge production control is not implemented.
- A verified all-output safe-off routine is still required before broader output control.
- C15 TC1047 PCB reference is marked for recheck to avoid repeating the earlier U34 confusion.

## Roadmap

1. Resolve the `0x4F` internal INA219 versus PCA9685 conflict with direct hardware probing.
2. Confirm PCA9685 address straps, OE behavior and every output-to-load mapping before any production output control.
3. Trace GPIO15 and decide whether it is only a boot/function input or also a discharge PWM path on MCC Pro.
4. Define and measure a `stop_all_outputs()` routine that leaves charger/discharge outputs in a safe state.
5. Only after the safe-off routine is verified, consider moving Q34/PCA9685 actions out of experimental C16 bring-up tooling.
6. Keep BQ24195 writes limited to the currently documented host-control fields until charger/discharge power paths are fully mapped.

## Validation commands

Use the mapped `Z:` path on Windows for compilation. PlatformIO can fail from a UNC working directory because `cmd.exe` does not support UNC current directories.

```powershell
esphome config esphome\mcc-pro-native.yaml
esphome compile esphome\mcc-pro-native.yaml
```

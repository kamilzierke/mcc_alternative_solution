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

## Confirmed low-level main-bus probe

The PlatformIO `mcc_pro_main_bus_probe` firmware was verified on the MCC Pro bench through the local live monitor. One address-only I2C transaction (address plus STOP, no data bytes) was issued at boot for each approved main-bus target. The 60-second UART observation completed with `PASS`, all 16 slot status frames and no invalid protocol frames.

| Address | Component | Result | Evidence boundary |
| --- | --- | --- | --- |
| `0x27` | PCF8574 / IC25 | ACK | Confirms main-bus reachability only; no output or mux state was changed. |
| `0x3C` | SSD1306 | ACK | Confirms main-bus reachability only; no display data was written. |
| `0x70` | TCA9548A | ACK | Confirms main-bus reachability only; no downstream channel was selected. |
| `0x71` | TCA9548A / U38 | ACK | Confirms main-bus reachability only; no downstream channel was selected. |

This result does not confirm register contents, attached devices, analog paths or power-output behavior. The next hardware test must remain read-only and target one documented register or identity read at a time.

## Confirmed TCA idle state

The PlatformIO `mcc_pro_tca_state_probe` firmware read one byte from the control register of each confirmed TCA9548A after reset. The 60-second GUI observation completed with `PASS`, all 16 slot status frames and no invalid protocol frames.

| Address | Observed register | Expected register | Result |
| --- | ---: | ---: | --- |
| `0x70` | `0x00` | `0x00` | match; no BQ channel selected |
| `0x71` | `0x00` | `0x00` | match; no BQ channel selected |

The firmware used two one-byte read transactions and did not send any TCA channel-selection byte. This confirms the expected idle selection state at the time of test, not the behavior of downstream BQ24195 devices.

## Confirmed C01 BQ24195 identity read

The PlatformIO `mcc_pro_bq_c01_identity_probe` selected only C01 through TCA9548A `0x70` channel 0 and read BQ24195 `REG0A`. The cached GUI evidence reported `REG0A = 0x23`, with successful TCA selection, successful BQ read-pointer write, one response byte and successful final release of both TCA devices.

| Step | Result |
| --- | --- |
| Release TCA `0x70` and `0x71` before selection | success |
| Select `0x70 = 0x01` for C01 only | success |
| Set BQ24195 read pointer to `REG0A` | success |
| Read BQ24195 `REG0A` | `0x23`, one byte |
| Release TCA `0x70` and `0x71` after read | success |

No BQ24195 configuration register was written. This confirms the C01 route and one read-only BQ register; it does not yet establish a complete per-slot register or power-path model.

## Confirmed C01 BQ24195 register snapshot

The PlatformIO `mcc_pro_bq_c01_snapshot_probe` firmware completed one read-only C01 snapshot of `REG00..REG0A`. The 60-second GUI observation completed with `PASS`, all 16 slot status frames and no invalid protocol frames. The TCA selection result was successful and both TCA devices were released successfully after the snapshot.

| Register | Observed value |
| --- | ---: |
| `REG00` | `0x30` |
| `REG01` | `0x1B` |
| `REG02` | `0x60` |
| `REG03` | `0x11` |
| `REG04` | `0xB2` |
| `REG05` | `0x9A` |
| `REG06` | `0x03` |
| `REG07` | `0x4B` |
| `REG08` | `0x24` |
| `REG09` | `0x80` |
| `REG0A` | `0x23` |

No BQ24195 configuration register was written. The values document one C01 bench observation and should not be treated as a general slot profile until the same read-only procedure is repeated on the other routes.

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

# MCC Pro low-level firmware

This PlatformIO project is the canonical firmware path for new MCC Pro hardware work. It starts in a strict read-only diagnostic mode: it initializes the confirmed UART, I2C and HC4067 select pins, but does not scan I2C addresses, enable analog muxes or issue BQ24195, PCF8574, PCA9685 or output-control writes.

ESPHome remains a reference and Home Assistant integration path. Update it only after a behavior is confirmed by the low-level firmware and documented hardware evidence.

## Build

```powershell
pio run --project-dir .\firmware
```

The generated binary is local build output under `firmware/.pio/`; it is ignored by Git. Do not upload a binary until the intended test tier has been reviewed under [the hardware test protocol](../docs/hardware-test-protocol.md).

## Approved main-bus probe build

`mcc_pro_main_bus_probe` is a separate build environment for the first bench read. It sends one address-only I2C transaction to each of the confirmed main-bus addresses `0x27`, `0x3C`, `0x70` and `0x71`, then reports the cached ACK result over UART. It does not send I2C data bytes, scan address ranges, select TCA channels, write PCF8574 masks, enable HC4067 muxes or access `0x41`, `0x4F` and `0x6B`. The address transactions occur once at boot; only their saved results are emitted again in periodic status frames.

```powershell
pio run --project-dir .\firmware --environment mcc_pro_main_bus_probe
```

Uploading this environment and opening its serial monitor remain hardware operations that require explicit approval.

## C01 BQ24195 identity probe build

`mcc_pro_bq_c01_identity_probe` is a separate read-only environment for one BQ24195 identity read. It releases both TCA9548A selectors to `0x00`, selects only C01 with `0x70 = 0x01`, writes the BQ24195 read pointer `REG0A` (`0x0A`) and reads one byte from `0x6B`. It then releases both TCA9548A selectors to `0x00` again, including after a failed BQ read. No BQ24195 configuration register is written.

```powershell
pio run --project-dir .\firmware --environment mcc_pro_bq_c01_identity_probe
```

The test is a controlled mux action and BQ read, so upload and observation require explicit approval and the bench conditions in the hardware test protocol.

## Serial protocol

The read-only firmware emits newline-delimited status frames at `115200 8N1`:

```text
MCC|HELLO|<version>|mode=read-only
MCC|EVENT|<level>|<component>|<message>
MCC|STATUS|<component>|<observed>|<requested>|<status>|<match>|<detail>
MCC|SLOT|<slot>|<observed>|<requested>|<status>|<match>|<detail>
```

`read-only` is an enforced capability boundary. The protocol is designed for the local live monitor and future automated bench runners; it does not provide output-control commands. The current firmware deliberately reports every slot as `not-sampled` until a read-only I2C measurement phase is approved.

## Automated checks

```powershell
& .\scripts\test-synthetic.ps1
pio run --project-dir .\firmware
```

The host tests do not require PlatformIO, ESPHome or hardware. A successful build does not prove board behavior.
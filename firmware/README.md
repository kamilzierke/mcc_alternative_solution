# MCC Pro low-level firmware

This PlatformIO project is the canonical firmware path for new MCC Pro hardware work. It starts in a strict read-only diagnostic mode: it initializes the confirmed UART, I2C and HC4067 select pins, but does not scan I2C addresses, enable analog muxes or issue BQ24195, PCF8574, PCA9685 or output-control writes.

ESPHome remains a reference and Home Assistant integration path. Update it only after a behavior is confirmed by the low-level firmware and documented hardware evidence.

## Build

```powershell
pio run --project-dir .\firmware
```

The generated binary is local build output under `firmware/.pio/`; it is ignored by Git. Do not upload a binary until the intended test tier has been reviewed under [the hardware test protocol](../docs/hardware-test-protocol.md).

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
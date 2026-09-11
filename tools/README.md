# Local diagnostic tools

`mcc-live-monitor.ps1` is a dependency-free Windows GUI for observing UART output from MCC Pro firmware. It opens the selected port with DTR and RTS disabled, sends no data and writes a local session log beneath `artifacts/live-monitor/`.

```powershell
& .\tools\mcc-live-monitor.ps1
```

The Live Overview tab lists the firmware, I2C, TCA9548A, PCF8574, HC4067 muxes, INA219 paths, BQ24195 slots, SSD1306, PCA9685 and outputs. Its columns distinguish the observed state, requested state, reported status, comparison result and most recent evidence.

The Slots C01-C16 tab provides the same fields for every slot and includes its known BQ route. The overview includes a 4x4 MCC slot mirror: green means a firmware-reported match, amber means an awaiting measurement, orange means a mismatch, and red means a fault. The mirror reflects only serial frames; it does not infer physical behavior.

The UART Log tab displays every received line. Unrecognized output remains visible as raw evidence and is not misrepresented as a validated component state. The current read-only firmware does not probe I2C, so unmeasured components and slots are deliberately shown as `not sampled` or `awaiting`.

The GUI can observe a connected device, but it cannot verify physical effects such as LEDs, relays, temperatures or current flow. Record those observations through [the hardware test protocol](../docs/hardware-test-protocol.md).
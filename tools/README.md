# Local diagnostic tools

`mcc-live-monitor.ps1` is a dependency-free Windows GUI for observing UART output from MCC Pro firmware. It opens the selected port with DTR and RTS disabled, sends no data and writes a local session log beneath `artifacts/live-monitor/`.

```powershell
& .\tools\mcc-live-monitor.ps1
```

After an approved read-only upload, use the visible 60-second verification flow:

```powershell
& .\scripts\upload-and-observe.ps1 -Port COM7 -ObservationSeconds 60
```

It opens the GUI after the upload, connects without sending UART data, shows elapsed time and received frame counts, then displays `PASS` only when it receives valid frames for the four current firmware components and all 16 slots. It disconnects automatically when the observation ends; the window remains open for inspection.

The monitor buffers incomplete UART fragments until a full CRLF-terminated frame arrives. The host-only regression check is included in `scripts/test-synthetic.ps1`.

For an explicitly approved address-only main-bus probe, use:

```powershell
& .\scripts\probe-main-bus-and-observe.ps1 -Port COM7 -ObservationSeconds 60
```

The probe build reports ACK or no-ACK only for PCF8574 `0x27`, SSD1306 `0x3C`, TCA9548A `0x70` and TCA9548A `0x71`. It does not scan, write I2C data, select channels or access measurement and charger addresses.

For an explicitly approved C01 BQ24195 identity read, use:

```powershell
& .\scripts\probe-bq-c01-identity-and-observe.ps1 -Port COM7 -ObservationSeconds 60
```

The probe temporarily selects only `TCA 0x70` channel 0, reads BQ24195 `REG0A` and releases both TCA devices. The monitor displays the register value and all TCA selection/release result codes as live evidence.

The Live Overview tab lists the firmware, I2C, TCA9548A, PCF8574, HC4067 muxes, INA219 paths, BQ24195 slots, SSD1306, PCA9685 and outputs. Its columns distinguish the observed state, requested state, reported status, comparison result and most recent evidence.

The Slots C01-C16 tab provides the same fields for every slot and includes its known BQ route. The overview includes a 4x4 MCC slot mirror: green means a firmware-reported match, amber means an awaiting measurement, orange means a mismatch, and red means a fault. The mirror reflects only serial frames; it does not infer physical behavior.

The UART Log tab displays every received line. Unrecognized output remains visible as raw evidence and is not misrepresented as a validated component state. The current read-only firmware does not probe I2C, so unmeasured components and slots are deliberately shown as `not sampled` or `awaiting`.

The GUI can observe a connected device, but it cannot verify physical effects such as LEDs, relays, temperatures or current flow. Record those observations through [the hardware test protocol](../docs/hardware-test-protocol.md).
# Local diagnostic tools

`mcc-live-monitor.ps1` is a dependency-free Windows GUI for observing UART output from MCC Pro firmware. It opens the selected port with DTR and RTS disabled, sends no data and writes a local session log beneath `artifacts/live-monitor/`.

Its layout target is documented visually in [`docs/gui-design-reference.png`](../docs/gui-design-reference.png); the current build uses system-styled (unthemed) native controls and does not yet match that reference pixel-for-pixel.

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

The window is a single persistent screen, not tabs. The left panel lists the firmware, I2C, TCA9548A, PCF8574, HC4067 muxes, INA219 paths, BQ24195 slots, SSD1306, PCA9685 and outputs, with columns for the observed state, requested state, reported status, comparison result and most recent evidence.

The center table gives every slot its own row with a column per decoded BQ24195 field (HIZ, IIN, CHG, OTG, ICHG, WD, timer, charge stage, DPM, PG, VSYS, fault, part/revision) alongside its known TCA route and snapshot/match state; columns are manually resizable and horizontally scrollable, with an `Auto-fit` action that fits column width to content within a fixed minimum/maximum. The right panel mirrors C01-C16 as 8 slots, a physical gap, then 8 more slots (not a grid): green means a firmware-reported match, amber means an awaiting measurement, orange means a mismatch, and red means a fault. Clicking a slot selects its row in the center table and the details panel at the bottom of the window. The mirror reflects only serial frames; it does not infer physical behavior.

The Live Log panel toggles between a leveled `Events` view (DEBUG/INFO/OK/WARN/ERROR, colored, with auto-scroll and clear) and a `Raw UART` view showing every received line verbatim. Unrecognized output remains visible as raw evidence in the Raw UART view and is not misrepresented as a validated component state. The current read-only firmware does not probe I2C, so unmeasured components and slots are deliberately shown as `not sampled` or `awaiting`.

The GUI can observe a connected device, but it cannot verify physical effects such as LEDs, relays, temperatures or current flow. Record those observations through [the hardware test protocol](../docs/hardware-test-protocol.md).
# Local diagnostic tools

`mcc-live-monitor.ps1` is a dependency-free Windows GUI for observing UART output from MCC Pro firmware. It opens the selected port with DTR and RTS disabled, sends no data and writes a local session log beneath `artifacts/live-monitor/`.

```powershell
& .\tools\mcc-live-monitor.ps1
```

The top table compares the expected read-only firmware state with structured `MCC|STATUS` frames received from the device. The log panel displays every received UART line. Unrecognized output remains visible as raw evidence and is not misrepresented as a validated component state.

The GUI can observe a connected device, but it cannot verify physical effects such as LEDs, relays, temperatures or current flow. Record those observations through [the hardware test protocol](../docs/hardware-test-protocol.md).
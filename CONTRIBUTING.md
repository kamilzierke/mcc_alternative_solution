# Contributing

Thank you for improving the MegaCell Charger Pro replacement firmware and its evidence base.

## Before proposing a change

- Keep a change focused on one documented behavior, board path or compatibility contract.
- Preserve existing diagnostic behavior unless the migration and user-visible effect are documented.
- Update the relevant technical evidence and `CHANGELOG.md` for user-visible changes.
- Do not commit Wi-Fi credentials, serial numbers, full hardware inventories, raw diagnostic logs, captured network data or local build artifacts.

## Validation

Run the available hardware-independent check before submitting a firmware or configuration change:

```powershell
esphome config esphome\mcc-pro-native.yaml
& .\scripts\verify-repository.ps1
& .\scripts\test-synthetic.ps1
pio run --project-dir .\firmware
git diff --check
```

The PlatformIO build validates the new read-only ESP8266 firmware and does not upload it. Run `esphome compile esphome\mcc-pro-native.yaml` when the local toolchain and all required dependencies are already available. Compilation is not evidence of live charger behavior.

`scripts/verify-repository.ps1` uses only Git and PowerShell. It checks that private inputs and generated artifacts are not tracked, that public documentation targets exist and that the working patch has no whitespace errors. `scripts/test-synthetic.ps1` compiles and runs the deterministic INA219, TC1047 and BQ24195 diagnostic contract tests with a local C++ compiler.

## Hardware evidence

Do not perform untraced PCF8574, PCA9685, BQ24195 or GPIO output writes with Li-Ion cells installed. Hardware reports must state:

- MCC board revision or identifiable board variant;
- power supply and current-limit conditions;
- whether cells or loads were attached;
- exact procedure and observed result;
- final output state and cleanup result.

Keep experimental hardware work separate from synthetic validation. A hardware-dependent claim is not complete until the relevant measurement and safe-off behavior are documented.
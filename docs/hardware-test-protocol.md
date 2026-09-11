# MCC Pro hardware test protocol

This protocol records evidence for a hardware-dependent claim. It does not authorize a test by itself, and it does not make experimental output control production-safe.

## Test tiers

| Tier | Scope | Allowed evidence |
| --- | --- | --- |
| Synthetic | Pure conversions, decoders, configuration and repository checks | Deterministic local or CI result; no hardware, network, credentials or elevated privileges |
| Bench, unpowered | Continuity, passive identification and visual board inspection | Board variant, probe points and observed trace |
| Bench, current-limited | Controlled read-only measurement or a mapped output path without cells | Supply voltage/current limit, procedure, measurement and cleanup result |
| Cell or load attached | A previously mapped and safe-off-protected path only | Approved procedure, cell/load state, safeguards, measurements and final output state |

## Preconditions

- Identify the board variant without publishing serial numbers or private inventory data.
- Use the active firmware revision and record its Git commit locally.
- Confirm whether cells and external loads are absent. They remain absent for unknown PCF8574, PCA9685, BQ24195 and GPIO output paths.
- For powered work, set and record the current limit before connecting the board.
- Define the expected idle state and the stop condition before the first command.
- Keep a practical way to remove power immediately.

## Required report

Store raw logs and photos locally. A shareable summary must include:

```text
Board variant:
Firmware commit/version:
Test tier:
Power source and current limit:
Cells or loads attached:
Target path and expected inactive state:
Procedure:
Measurements and observed result:
Error or timeout behavior:
Final output state and cleanup result:
```

Do not include Wi-Fi credentials, serial numbers, full private inventories or raw logs in public reports.

## Stop conditions

Stop the test and remove power when an unexpected output changes state, measured current exceeds the defined limit, I2C control is not deterministic, an error path cannot restore the expected idle state or the target path cannot be traced confidently. Record the stop condition as evidence; do not continue by trying additional unknown output commands.

## Current MCC Pro restriction

Until the `0x4F` device conflict, GPIO15 role and `stop_all_outputs()` sequence are verified, broader power-output control remains outside the permitted test scope. The active work is evidence-led diagnostics and board mapping.
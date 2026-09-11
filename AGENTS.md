# MCC Alternative Solution Agent Instructions

Follow [AI_AGENT_WORKFLOW_STANDARD.md](AI_AGENT_WORKFLOW_STANDARD.md) for all work in this repository. This file records the repository-specific application of that standard.

## Scope and safety

- The active implementation is `esphome/mcc-pro-native.yaml` with `esphome/mcc_diag_helpers_native.h`.
- Treat all charger, PWM, mux and I2C control as hardware-affecting. Do not access serial/USB devices, flash firmware, invoke OTA, run live probes or make output-control changes without explicit approval.
- Do not read, print, add or modify real credentials. Keep `esphome/secrets.yaml` local and use a redacted example file for documentation.
- Preserve archived material under `archive/`; it is local historical reference and outside the active firmware path.
- Do not fetch, pull, push, commit, change branches, install dependencies or use external services without approval.

## Required task record

Before an edit, record in the task update:

```text
Repository:
Branch/commit:
Task anchor:
Current behavior:
Expected behavior:
Hypothesis:
Cheapest check that can disconfirm it:
Planned first edit:
```

After each phase, report changed files, evidence, commands/results, known risks, approval-gated operations and next step.

## Validation

Synthetic validation for the active firmware uses:

```powershell
esphome config esphome\mcc-pro-native.yaml
esphome compile esphome\mcc-pro-native.yaml
```

Run `config` after the first firmware or configuration edit. Run `compile` for changes to the active YAML or native helper when the local ESPHome toolchain is available. These checks do not validate live behavior or safety on the charger.

## Current hardware risk

Do not extend production control while the PCA9685/internal INA219 address conflict at `0x4F`, GPIO15 role and verified all-output safe-off sequence remain unresolved. The status and next safe work items are in `docs/firmware-status-and-roadmap.md`.
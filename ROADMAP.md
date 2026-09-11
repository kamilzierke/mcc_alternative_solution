# MegaCell Charger Pro roadmap

This roadmap describes the intended direction for the MCC Pro replacement firmware. It separates confirmed behavior from planned work; an item is not a delivery commitment until its safety evidence is complete.

## Current baseline

The native ESPHome diagnostic firmware can read one slot or queue reads for all 16 slots. It exposes TC1047 temperature, internal and external INA219 diagnostics, BQ24195 registers and limited queued BQ host-control actions. The active implementation is documented in `esphome/mcc-pro-native.yaml` and `docs/firmware-status-and-roadmap.md`.

The following remain unverified and block broader output control:

- the `0x4F` internal INA219/PCA9685 address conflict;
- the MCC Pro role of GPIO15;
- PCA9685 address straps, OE state and output-to-load mapping;
- a measured all-output safe-off sequence.

## Milestone 1: Evidence-led board map

**Goal:** establish a traceable map of the remaining control and measurement paths without enabling unknown power outputs.

- Confirm the common (`Z`) and enable paths for U10E, U2, U3 and U34.
- Confirm every PCF8574 bit and its inactive electrical state.
- Resolve whether `0x4F` selects an INA219, PCA9685 or different devices on separate paths.
- Trace GPIO15 from ESP-12F to its board-level destination.
- Record the slot-to-TCA-channel mapping and all measurement calibration evidence.

**Complete when:** each conclusion cites a board observation or repeatable measurement, unknown control paths remain disabled, and the component reference reflects the result.

## Milestone 2: Read-only diagnostic contract

**Goal:** make diagnostic information predictable and useful without depending on live output control.

- Keep slot reads queued on the ESPHome main loop rather than HTTP callbacks.
- Define stable output for `/bq`, `/status`, `/c16` and the documented compatibility endpoints.
- Add low-memory and unavailable-path behavior that returns a clear, bounded response.
- Preserve confirmed legacy endpoint names and request limits where compatibility evidence exists.
- Add configuration-level checks for register decoding, slot mapping and response formatting where they can run without the charger.

**Complete when:** the diagnostic contract is documented, synthetic checks cover its supported states, and live claims are explicitly marked with their hardware evidence.

## Milestone 3: Verified safe-off foundation

**Goal:** create and prove a `stop_all_outputs()` routine before adding any broader charger or discharge control.

- Define the required inactive state for every confirmed BQ, PCF8574, PCA9685 and GPIO control path.
- Validate the sequence first with cells removed and a current-limited power supply.
- Measure output state before, during and after normal shutdown, restart and interrupted operations.
- Ensure failed reads, queue timeouts and web/API errors leave outputs in the defined safe state.

**Complete when:** the routine is documented, repeatable hardware evidence records the board revision, supply conditions and cleanup result, and no untraced output is driven.

## Milestone 4: Deliberate control enablement

**Goal:** introduce only the charger and output controls backed by the board map and safe-off evidence.

- Keep one-slot-at-a-time operations and explicit rate limits.
- Expose state, rejection reasons and error conditions clearly in the web and Home Assistant surfaces.
- Add each output control independently with a documented power-path dependency.
- Retain a safe, read-only mode when a control path is unavailable or unverified.

**Complete when:** each exposed control has a documented electrical mapping, failure behavior, test procedure and rollback to `stop_all_outputs()`.

## Milestone 5: Usable and maintainable integration

**Goal:** make the verified firmware practical to operate and safe to evolve.

- Keep Home Assistant entities and the diagnostic web views aligned with the documented contract.
- Provide redacted example configuration and never commit local Wi-Fi credentials, device identifiers or raw diagnostic logs.
- Publish release notes that distinguish diagnostic, experimental and production-safe capabilities.
- Add a reproducible build and validation path before distributing firmware beyond controlled testing.

**Complete when:** a new contributor can configure, validate and understand the firmware limitations without access to private hardware data.

## Engineering guarantees

Future work must preserve these rules:

- Synthetic validation must not require network access, credentials, physical hardware or elevated privileges.
- Hardware results must identify the board revision, power conditions, attached cells/load state, procedure and cleanup outcome.
- No power-output claim is production-safe until its electrical path and safe-off behavior are verified.
- Local secrets, full hardware inventories and raw diagnostic logs remain outside version control.
- Public documentation describes product behavior, limits and evidence, not internal development tooling.
- Existing documented diagnostic behavior remains compatible unless a user-visible migration is documented.
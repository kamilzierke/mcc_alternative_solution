# Changelog

All notable user-visible changes and material evidence updates are documented here.

## Unreleased

### Documentation and project safety

- Added an outcome-oriented public roadmap with safety gates for diagnostic and control work.
- Added contributor and security guidance for private configuration, synthetic validation and hardware evidence.
- Added ignored local artifact paths for diagnostic logs and reports.
- Removed public references to local archive structure and historical workspace material.
- Added a hardware-independent repository quality gate for private-file tracking, public documentation targets and patch formatting.
- Added deterministic C++ tests for INA219, TC1047 and BQ24195 diagnostic conversions and decoding.
- Added documented hardware-test evidence and pre-release requirements for the diagnostic firmware stage.
- Established PlatformIO read-only firmware as the starting point for new hardware experiments, with ESPHome retained as the verified integration reference.
- Added a local UART live monitor with structured component statuses, expected-versus-live state comparison and ignored session logs.
- Redesigned the live monitor as a single persistent screen with a component-diagnostics table, a per-field BQ24195 cell table, an 8-gap-8 physical slot mirror, and a leveled/raw live log.

### Current diagnostic baseline

- Native ESPHome diagnostics support queued single-slot and all-slot reads across the 16 BQ24195 channels.
- Diagnostics expose TC1047 internal temperature, internal/external INA219 paths, BQ24195 registers, OLED status and documented web endpoints.
- Broader output control remains blocked pending resolution of the `0x4F` conflict, GPIO15 tracing and verified `stop_all_outputs()` behavior.

### Bench evidence

- Confirmed address-only I2C ACK responses for PCF8574 `0x27`, SSD1306 `0x3C` and TCA9548A `0x70`/`0x71` through the read-only PlatformIO probe.
- Confirmed that both TCA9548A control registers read `0x00` after reset, with no BQ channel selected.
- Confirmed read-only BQ24195 C01 `REG0A = 0x23` through TCA9548A `0x70` channel 0, including successful release of both TCA selectors.
- Confirmed a read-only BQ24195 C01 `REG00..REG0A` snapshot with successful TCA selection and final release.
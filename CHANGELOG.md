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

### Current diagnostic baseline

- Native ESPHome diagnostics support queued single-slot and all-slot reads across the 16 BQ24195 channels.
- Diagnostics expose TC1047 internal temperature, internal/external INA219 paths, BQ24195 registers, OLED status and documented web endpoints.
- Broader output control remains blocked pending resolution of the `0x4F` conflict, GPIO15 tracing and verified `stop_all_outputs()` behavior.
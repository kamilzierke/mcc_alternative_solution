# Changelog

All notable user-visible changes and material evidence updates are documented here.

## Unreleased

### Documentation and project safety

- Added an outcome-oriented public roadmap with safety gates for diagnostic and control work.
- Added contributor and security guidance for private configuration, synthetic validation and hardware evidence.
- Added ignored local artifact paths for diagnostic logs and reports.

### Current diagnostic baseline

- Native ESPHome diagnostics support queued single-slot and all-slot reads across the 16 BQ24195 channels.
- Diagnostics expose TC1047 internal temperature, internal/external INA219 paths, BQ24195 registers, OLED status and documented web endpoints.
- Broader output control remains blocked pending resolution of the `0x4F` conflict, GPIO15 tracing and verified `stop_all_outputs()` behavior.
# Reverse engineering next steps

## Already solved

- TC1047 temperature readout for all 16 slots.
- U10 temperature mux enable path via IC25/P0.
- ADC effective scale for TC1047 path: 3.02 V.

## Next priorities

1. Trace U10 Z physically to confirm the full ADC path.
2. Trace remaining 74HC4067 muxes: enable pins, Z/common pins and input domains.
3. Map physical slot number to BQ24195 TCA/channel.
4. Decode PCA9685 outputs by measuring LED/MOSFET behavior without cells.
5. Decode remaining IC25 PCF8574 bits.
6. Resolve GPIO15 conflict.
7. Build `stop_all_outputs()` and verify with multimeter/current-limited PSU.

## Archive-derived compatibility priorities

- Keep the replacement HTTP API memory-safe: poll cells as 1-8 and 9-16, not all 16 in one large payload.
- Keep command endpoints disabled/read-only until power outputs are mapped, but preserve endpoint names and return predictable read-only errors.
- When command endpoints are eventually enabled, mirror original client constraints: at most 2 cells per command request and at least 250 ms between requests.
- Add low-memory behavior to the API design: original/reference firmware may return HTTP 503 with body `low memory`.
- Preserve `/api/who_am_i` compatibility for both newer MCCPro/MCCReg field names and older classic MCC field names.

## Archive-derived hardware cautions

- Saved forum material for regular MCC mentions global discharge PWM on ESP12F GPIO15 at 100 Hz. This is not yet confirmed for MCC Pro, but it makes the existing GPIO15 conflict a high-priority trace item.
- Saved regular-MCC material associates OLED `0x3C` presence with charger-type detection in at least one troubleshooting thread. Keep OLED behavior conservative and avoid assuming the display is optional in compatibility tests.
- Newly catalogued likely power/control parts from the archive sheets: IR4427SPBF gate drivers, SD8942 buck converter, LM1117MP-3.3 regulator and BC849C transistors.

## Safety

Do not test PCF/PCA/BQ output writes with Li-Ion cells installed unless the target line is already traced and the power path is understood.

# Archive analysis: MegaCell Charger source material

Date: 2026-05-11

Analyzed local archives:

- `c:\Users\Kamil\Downloads\Downloads.zip`
- `c:\Users\Kamil\Downloads\megacnc-main.zip`

Temporary extraction path used for analysis:

- `C:\Users\Kamil\AppData\Local\Temp\mcc-archive-analysis-1778504485`

## Scope

The archives contain three useful classes of material:

- saved original/user-facing MegaCell Charger pages and forum discussions;
- user reverse-engineering CSV sheets;
- a MegaCNC Django project with an MCC/MCCPro HTTP client and API reference.

No firmware binaries or saved HTML dumps were copied into this repository. The imported value is distilled into docs and inventory tables.

## High-value API findings

`megacnc-main.zip` contains `API_README.md`, `mccprolib/api.py` and MegaCNC tasks that materially improve the API compatibility notes:

- MCC Pro polling should use `POST /api/get_cells_info` in two batches: `start=1,end=8` and `start=9,end=16`.
- Request ranges are 1-based and inclusive; cell ids in returned objects are 0-based.
- ESP8266 memory pressure is a real API compatibility constraint: avoid faster-than-250 ms polling and batch commands by at most 2 cells.
- Low-memory responses may be HTTP 503 with body `low memory`; clients should retry after a delay.
- `GET /api/dash_data` has a compact array format for cell rows.
- `GET /api/who_am_i` has newer MCC Pro / MCC Regular fields (`ChT`, `FwV`, `McA`, `CeC`) and older classic MCC fields (`McC`, `McA`, `ByC`).
- `api/get_chemistry` is treated by MegaCNC as raw data, not normal decoded text.
- `api/reset_charger` uses secret `20200104` in saved legacy notes and MegaCNC client code.

Imported to:

- `docs/api-compatibility.md`

## Firmware reference findings

`megacnc-main.zip` contains a `Megacell Firmwares` directory with archived binary artifacts and v1.1.0 changelog.

Important compatibility facts:

- v1.1.0 changelog date: 2026-03-20.
- v1.1.0 adds dashboard, OTA web upload, config backup/restore and grouping support.
- The changelog claims no breaking changes to MegaCNC endpoints.
- It explicitly notes ESP8266 heap pressure with 16 cells active.
- `GET /api/get_config_info` is documented as returning an empty response in v1.1.0 because the original handler was commented out.
- The firmware moved toward streaming/zero-heap API responses and compact dashboard JSON.

Imported to:

- `firmware/README.md`
- `docs/api-compatibility.md`

## Hardware findings

`Downloads.zip` contains `Megacellcharger Pro - Arkusz1.csv`, which overlaps current board findings and adds useful component identifications:

- IR4427SPBF dual gate drivers, likely related to fan/resistor or power-path drive control.
- LM1117MP-3.3/TR 3.3 V regulator.
- BC849C small-signal NPN transistors.
- SD8942 buck converter.
- Existing confirmed/relevant entries are reinforced: PCF8574 at `0x27`, SSD1306 OLED at `0x3C`, INA219 at `0x41`, PCA9685 at `0x4F`, TCA9548A at `0x70/0x71`, BQ24195 behind TCA switches, TC1047 sensors and HC4067 muxes.

Imported to:

- `hardware/component_inventory.csv`
- `docs/reverse-engineering-next-steps.md`

## Reverse-engineering notes from saved pages

Saved forum/page material contains useful but lower-confidence context:

- Older regular MCC notes mention OLED `0x3C` and PCA9685 `0x4F`; in one saved discussion, missing OLED detection was associated with unknown charger type.
- Forum notes mention a regular-MCC discharge PWM behavior on ESP12F GPIO15 at 100 Hz affecting all discharge MOSFETs. This is not proof for MCC Pro, but it is relevant to the unresolved GPIO15 conflict.
- Legacy pages show classic MCC API shapes and flashing examples, useful for compatibility but not primary MCC Pro behavior.

Imported as cautions rather than confirmed board facts:

- `docs/reverse-engineering-next-steps.md`
- `docs/api-compatibility.md`

## Archive-only material not imported

These items were found but not imported into repo docs for now:

- `CellTypes.htm` and related images: useful as a battery/cell database, not directly useful for current hardware bring-up.
- Saved installation/news/release pages: mostly user documentation, lower value than API/client source.
- Firmware binaries and Windows flasher executable: catalogued with hashes only; not committed.

# MegaCell Charger / MegaCNC API compatibility

Replacement firmware should eventually expose MegaCell/MegaCNC-like HTTP endpoints while remaining safe during reverse engineering.

Sources consolidated here:

- `Downloads.zip`: saved MCC pages, forum notes and user CSV sheets.
- `megacnc-main.zip`: MegaCNC Django project, `mccprolib/api.py`, `megacellcnc/tasks.py`, `API_README.md`.

## General constraints

- HTTP API runs on port 80.
- ESP8266 free heap is tight during 16-slot operation; archived firmware notes mention roughly 4-6 KB free heap.
- Do not request faster than every 250 ms.
- Batch write commands in groups of at most 2 cells.
- Low-memory behavior in newer firmware: HTTP 503 with body `low memory`; client should retry after 1-2 s.
- Use `Connection: close` style behavior; avoid assuming persistent connections.
- Read-only replacement firmware must stub all command endpoints until output paths are traced.

## Device identity

`GET /api/who_am_i`

MCC Pro / MCC Regular newer shape:

| Field | Meaning |
| --- | --- |
| `ChT` | charger type, e.g. MCCPro / MCCReg |
| `FwV` | firmware version |
| `McA` | MAC address |
| `CeC` | cell/group count |

Classic MCC shape observed in older tooling:

| Field | Meaning |
| --- | --- |
| `McC` | classic firmware / charger marker |
| `McA` | MAC address |
| `ByC` | bay count |

MegaCNC generates display names from charger type plus the last three MAC octets.

## Cell polling

`POST /api/get_cells_info`

MCC Pro request body uses 1-based inclusive slot ranges:

```json
{"start": 1, "end": 8}
```

Poll 1-8 and 9-16 separately, then combine `cells`. The archived MegaCNC client does this by default. `start`/`end` are 1-based, but `CiD` in responses is 0-based.

Verbose response fields:

| Field | Meaning |
| --- | --- |
| `CiD` | internal cell id, 0-based |
| `VlT` | voltage |
| `AmP` | current |
| `CaP` | charge capacity |
| `CCa` | cycle charge capacity |
| `StS` | state id |
| `esr` | ESR |
| `AcL` | action label / action code |
| `DiC` | discharge capacity |
| `CoC` | cycle count / completion counter |
| `TmP` | temperature |
| `ChC` | charge current command/config |
| `CcO` | charge cutoff/config |
| `DcO` | discharge cutoff/config |
| `MaV` | maximum voltage |
| `StV` | store voltage |
| `MiV` | minimum voltage |
| `GiD` | group id |
| `StT` | state time / start timestamp |

Legacy classic MCC pages also show an older request shape based on `settings:[{"charger_id":1}]` and longer JSON field names. Treat that as a compatibility target for classic MCC tooling, not as the primary MCC Pro shape.

## Dashboard data

`GET /api/dash_data`

System fields observed in the archived v1.1.0 reference:

| Field | Meaning |
| --- | --- |
| `heap` | free heap |
| `frag` | heap fragmentation |
| `rssi` | Wi-Fi RSSI |
| `fan` | fan value/status |
| `temp` | board/system temperature |
| `chem` | active chemistry |
| `act` | active action count |
| `cg` | current group |
| `mcg` | max/current group limit |
| `gc` | group count |

Compact cell array format:

```text
[state, voltage_mV, current_mA, charge_cap_mAh, discharge_cap_mAh, group_id, temperature_C]
```

The archived firmware changelog says compact dashboard JSON reduced response size from about 5 KB to about 540 B.

## Commands

`POST /api/set_cell`

Known command codes:

| Code | Meaning |
| --- | --- |
| `ach` | action charge |
| `adc` | action discharge |
| `sc` | stop cell |
| `osc` | override stop charge |
| `odc` | override stop discharge |
| `esr` | ESR read |
| `dsp` | dispose |
| `dps` | dispose stop / dispose pause |
| `cdc` | clear discharge capacity |
| `ccc` | clear charge capacity |
| `asc` | apply slot chemistry/config |

`POST /api/set_cell_macro`

Known macro command codes:

| Code | Meaning |
| --- | --- |
| `mCap` | measure capacity macro |
| `stop` | stop macro/action |

The archived client batches commands in groups of 2 cells because of ESP8266 memory limits.

## Chemistry and settings

Known endpoints:

- `GET /api/chemistries`
- `POST /api/apply_chem`
- `POST /api/set_chemistry`
- `GET /api/get_chemistry`
- `GET /api/settings`
- `POST /api/save_hw`
- `POST /api/save_menu`
- `POST /api/set_hw_conf`
- `GET /api/get_config_info`
- `POST /api/set_config_info`
- `POST /api/set_pid`

Chemistry/config fields observed in MegaCNC and the API reference:

| Field | Meaning |
| --- | --- |
| `maxV` | maximum voltage |
| `minV` | minimum voltage |
| `stoV` | storage voltage |
| `maxCap` | maximum capacity |
| `chgI` | charge current |
| `preI` | precharge current |
| `trmI` | termination current |
| `dchI` | discharge current |
| `dchR` | discharge resistor level |
| `dchM` | discharge mode |
| `maxT` | maximum temperature |
| `lvT` | low-voltage threshold/timer field in tooling |
| `mcT` | max-capacity timer/threshold field in tooling |
| `cyc` | cycles |

MegaCNC treats `api/get_chemistry` as raw/binary-ish data and base64-encodes it before storing. Do not parse it through a normal text response path unless the firmware format is known.

Archived v1.1.0 notes say `GET /api/get_config_info` returns an empty response in that firmware because the handler was commented out in the original firmware.

## System and OTA

Known endpoints:

- `POST /api/reset_charger`
- `POST /api/reboot`
- `POST /api/do_factory_reset`
- `POST /api/update`
- `POST /api/update_spiffs`

`api/reset_charger` was observed with body:

```json
{"secret": 20200104}
```

Saved legacy notes mention the response may report failure even when the device actually reboots.

## Cell state ids

| Id | State |
| --- | --- |
| 0 | `NOT_INSERTED` |
| 1 | `CELL_INSERTED` |
| 2 | `CHECK_CHEMISTRY` |
| 3 | `LVC_CELL` |
| 4 | `REGULAR_CELL` |
| 5 | `BAD_VC_READING` |
| 6 | `TOO_COLD` |
| 7 | `LVC_CHARGE` |
| 8 | `REGULAR_CHARGE` |
| 9 | `COOLDOWN` |
| 10 | `CHARGING_FAILED` |
| 11 | `CHARGED_CHECK` |
| 12 | `VOLT_DROP_CHECK` |
| 13 | `CHARGED` |
| 14 | `ANORMAL_CHARGED` |
| 15 | `BAD_CELL` |
| 16 | `DISCHARGE` |
| 17 | `DISCHARGED` |
| 18 | `DISCHARGE_FAILED` |
| 19 | `ESR_READ` |
| 20 | `ESR_READ_COMPLETED` |
| 21 | `ESR_READ_FAILED` |
| 22 | `RESTING` |
| 23 | `RESTED` |
| 24 | `CHECK_STORE_ACTION` |
| 25 | `STORE_CHARGING` |
| 26 | `STORE_DISCHARGING` |
| 27 | `STORE_CHECK` |
| 28 | `STORED` |
| 29 | `FAILED_STORE` |
| 30 | `DISPOSE` |
| 31 | `DISPOSED` |
| 32 | `ERROR` |

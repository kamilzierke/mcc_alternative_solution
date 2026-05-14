# ESPHome native diagnostics for MCC Pro

Date: 2026-05-12

## Status

`esphome/mcc-pro-native.yaml` is the current working diagnostic configuration.

User testing confirmed:

- Reading one selected slot works.
- Queueing and reading all 16 slots works.
- Slots above C08 work through the second TCA9548A path.

This supersedes the earlier direct-Wire web-table experiment for BQ diagnostics.

## Coverage

The current native diagnostic firmware directly handles the confirmed I2C/control topology:

- 16x BQ24195 charger ICs, one per slot.
- 2x TCA9548A I2C switches at `0x70` and `0x71`.
- 1x PCF8574 expander at `0x27` for active-low HC4067 mux enables.
- INA219 external path at `0x41`.
- INA219 internal path at `0x4F`.
- 1x SSD1306 OLED at `0x3C`.

It also reads all 16 internal TC1047 temperature sensors, the external temperature mux and per-slot INA219 values through shared 74HC4067 select lines.
PCA9685-style writes for C16/Q34 exist as bring-up tests, but `0x4F` is also used for the internal INA219 path in this helper, so this remains a hardware-risk area until the address conflict is resolved.

## Architecture

The native config moves the known board topology into ESPHome components:

| Function | ESPHome component | Board device |
| --- | --- | --- |
| Main I2C bus | `i2c` `main_i2c` | ESP8266 GPIO4 SDA / GPIO5 SCL |
| C01..C08 BQ branches | `tca9548a` `tca0` | address `0x70`, channels 0..7 |
| C09..C16 BQ branches | `tca9548a` `tca1` | U38, address `0x71`, channels 0..7 |
| HC4067 mux enables | helper full-byte PCF8574 writes | IC25 `0x27`, P0..P4 active-low |
| BQ access | `i2c_device` | `0x6B` on each TCA channel |
| External INA diagnostic access | `i2c_device` | U3 path INA219 at `0x41` |
| Internal INA diagnostic access | `i2c_device` | U2 path INA219 at `0x4F` |
| OLED | `ssd1306_i2c` | `0x3C` on main I2C |

The helper file `mcc_diag_helpers_native.h` keeps only board-specific logic that ESPHome does not model directly:

- 74HC4067 select GPIO handling.
- TC1047 ADC averaging and conversion.
- External temperature diagnostics.
- INA219 reads through U2+U34 and U3 mux paths.
- BQ24195 register reads and limited host-control writes.
- `/bq`, `/status` and `/c16` queue/cache web tables.
- C16 Home Assistant diagnostic entity publishing.

## ESP-12F pin use

Direct ESP-12F / ESP8266 pins used by the current native files:

| Pin | Direction | Role |
| --- | --- | --- |
| GPIO4 | I/O | `main_i2c` SDA for PCF8574, OLED, INA219 and TCA9548A |
| GPIO5 | I/O | `main_i2c` SCL at 100 kHz |
| GPIO13 | OUT | shared HC4067 `S0`, channel bit 0 |
| GPIO12 | OUT | shared HC4067 `S1`, channel bit 1 |
| GPIO14 | OUT | shared HC4067 `S2`, channel bit 2 |
| GPIO16 | OUT | shared HC4067 `S3`, channel bit 3 |
| A0 / ADC0 | IN | analog input from U10 and U10E HC4067 outputs |
| GPIO1 / TX0 | OUT | UART logger at 115200 baud |
| GPIO3 / RX0 | IN | UART0/programming path, not application logic |

GPIO0, GPIO2 and GPIO15 are not used by the current YAML/helper application logic. GPIO6..GPIO11 are the ESP8266 flash SPI pins and should not be used.

All HC4067 devices share the same select lines. The helper selects the slot channel with `mux_select(slot0 & 0x0F)`:

| Slot | HC4067 channel | S3 S2 S1 S0 |
| --- | ---: | --- |
| C01 | 0 | 0000 |
| C02 | 1 | 0001 |
| C03 | 2 | 0010 |
| C04 | 3 | 0011 |
| C05 | 4 | 0100 |
| C06 | 5 | 0101 |
| C07 | 6 | 0110 |
| C08 | 7 | 0111 |
| C09 | 8 | 1000 |
| C10 | 9 | 1001 |
| C11 | 10 | 1010 |
| C12 | 11 | 1011 |
| C13 | 12 | 1100 |
| C14 | 13 | 1101 |
| C15 | 14 | 1110 |
| C16 | 15 | 1111 |

## PCF8574 HC4067 enables

The ESP does not directly drive HC4067 enable pins. It writes full byte masks to PCF8574 `0x27`; active mux enable outputs are LOW and all inactive outputs are kept HIGH.

| PCF pin | Active state | Controlled path | Helper mask |
| --- | --- | --- | --- |
| P0 | LOW | U10 internal TC1047 temperature mux to A0 | `0xFE` |
| P1 | LOW | U10E external temperature mux to A0 | `0xFD` |
| P2 | LOW | U34 shunt mux for internal INA219 | `0xFB` |
| P3 | LOW | U3 external-cell path to INA219 `0x41` | `0xF7` |
| P4 | LOW | U2 internal BAT+ path to INA219 `0x4F` | `0xEF` |
| P2+P4 | LOW | U34+U2 internal INA219 path | `0xEB` |

P5..P7 are not used by the current configuration.

## Slot mapping

The working native mapping is:

| Slots | TCA address | Channels | Downstream BQ address |
| --- | ---: | --- | ---: |
| C01..C08 | `0x70` | 0..7 | `0x6B` |
| C09..C16 | `0x71` / U38 | 0..7 | `0x6B` |

Confirmed U38 details:

- A0 = 5 V / high.
- A1 = GND / low.
- A2 = GND / low.
- RESET = 5 V / high.
- SC7/SD7 route to C16 BQ24195 SCL/SDA.

## Web interface

Current captured views:

- [`/bq` full slot diagnostics](screenshots/endpoint-bq-full-slot-table.png)
- [`/status` compact status table](screenshots/endpoint-status-compact-table.png)
- [`/c16` raw diagnostics](screenshots/endpoint-c16-raw-diagnostics.png)
- [Home Assistant MCC dashboard](screenshots/home-assistant-mcc-dashboard.png)

Endpoint: `/bq`

Supported commands:

- `/bq` - show cached table, no I2C transaction.
- `/bq?slot=N` - queue one slot read, where `N` is `1..16`.
- `/bq?read_all=1` - queue all 16 slot reads.
- `/bq?clear=1` - clear pending queue.
- `/bq?slot=N&action=charge_on` - queue a BQ24195 charge-enable action for one slot.
- `/bq?slot=N&action=charge_off` - queue a BQ24195 charge-disable action for one slot.
- `/bq?slot=N&action=wd_reset` - queue a BQ24195 watchdog-reset action for one slot.
- `/bq?slot=N&action=iin_500`, `/bq?slot=N&action=iin_900`, `/bq?slot=N&action=iin_1500`, `/bq?slot=N&action=iin_3000` - queue a BQ24195 input-current-limit change for one slot.
- `/bq?slot=N&action=ichg_512`, `/bq?slot=N&action=ichg_1024`, `/bq?slot=N&action=ichg_2048`, `/bq?slot=N&action=ichg_3008` - queue a BQ24195 charge-current change for one slot.

The endpoint also accepts compact aliases used by the ESP8266 HTML table to keep the response below the small stream-buffer limit:

- `/bq?s=N` - same as `/bq?slot=N`.
- `/bq?ra=1` - same as `/bq?read_all=1`.
- `/bq?s=N&a=1`, `/bq?s=N&a=0`, `/bq?s=N&a=w` - charge on, charge off and watchdog reset.
- `/bq?s=N&a=i2`, `/bq?s=N&a=i3`, `/bq?s=N&a=i5`, `/bq?s=N&a=i7` - input current limit: 500 mA, 900 mA, 1.5 A or 3.0 A.
- `/bq?s=N&a=c0`, `/bq?s=N&a=c8`, `/bq?s=N&a=c24`, `/bq?s=N&a=c39` - charge current: 512 mA, 1024 mA, 2048 mA or 3008 mA.
- `/bq?all=1&action=...` or `/bq?all=1&a=...` - queue the selected BQ action for all 16 slots.
- `/bq?c16hw=q34_off`, `q34_1`, `q34_5`, `q34_25`, `q34_100`, `q34_pulse5` - queue experimental C16 Q34 PCA9685-style output tests.
- `/bq?c16hw=q35_probe` - force Q34 OFF and queue C16 read for external Q35 probing.

Endpoint: `/status`

- `/status` - compact cached status table.
- `/status?read_all=1` or `/status?ra=1` - queue all slot reads and return to `/status`.

Endpoint: `/c16`

- `/c16` - chunked raw C16 diagnostics page.
- `/c16?read=1` - queue C16 read.
- `/c16?c16hw=hold_u2_ina` - hold U2+U34/internal INA path on C16 for DMM/scope probing.
- `/c16?c16hw=hold_u3_ina` - hold U3/external INA path on C16 for DMM/scope probing.
- `/c16?c16hw=hold_release` - release manual HC4067 hold and return PCF8574 to idle.

Important implementation detail: HTTP handlers never perform I2C reads. They only update the queue and return HTML. The 250 ms ESPHome interval calls `mccdiag::process_bq_web_read_request()` from the main loop, reads one queued slot, updates cache, then returns. This avoids the ESP8266 crash seen when I2C was performed from the async webserver `ctx: sys` context.

The per-slot action buttons use the same queue discipline. The HTTP handler only records the requested action; the main loop performs one BQ I2C write operation at a time and queues a follow-up read for that slot.

After any queued read or action request, the handler redirects the browser back to plain `/bq`. This keeps refresh/back navigation from submitting the same command again and makes the visible page URL stable.

The table keeps BQ controls in separate columns before BQ data. `I.5`, `I.9`, `I1.5` and `I3` are input-current-limit presets in amps. `C.5`, `C1`, `C2` and `C3` are charge-current presets in amps. The table is wrapped in a horizontal-scroll container because these per-action columns are intentionally explicit.

Current write actions:

- Charge ON: clear `REG00[7] EN_HIZ`, disable watchdog with `REG05[5:4] WATCHDOG = 00`, then set `REG01[5:4] CHG_CONFIG` to `01`.
- Charge OFF: disable watchdog with `REG05[5:4] WATCHDOG = 00`, then set `REG01[5:4] CHG_CONFIG` to `00`.
- WD Reset: set `REG01[6] WDT_RESET` to `1`.
- IIN presets: set `REG00[2:0] IINLIM` to one of the exposed input current limits.
- ICHG presets: set `REG02[7:2] ICHG` to one of the exposed charge current limits.

The watchdog is disabled for ON/OFF because an active BQ24195 watchdog can reset host-written charge-control registers after its timeout and make a charger return to its previous/default charging behavior.

## Per-slot read contents

Each queued slot read currently captures:

- TC1047 temperature for the selected slot.
- External temperature mux reading for the selected slot.
- Internal INA219 bus voltage, shunt voltage and derived current.
- External INA219 bus voltage, shunt voltage and derived current.
- BQ24195 `REG00`.
- BQ24195 `REG01` charge-control state.
- BQ24195 `REG02` charge-current state.
- BQ24195 `REG05` watchdog/timer state.
- BQ24195 `REG08` charge/system status.
- BQ24195 `REG09` fault status, read twice to observe latched/cleared behavior.
- BQ24195 `REG0A` vendor/part/revision.

The web table reports cached values and age. Read-all is intentionally serialized one slot per interval cycle to keep ESP8266 responsive.

The `/bq` table uses two compact header rows. The first row identifies the device/register, and the second row gives the human label. BQ register cells show a decoded summary plus the raw hex value in parentheses:

- `BQ24195 REG00 / Input source` - `EN_HIZ` state and input current limit.
- `BQ24195 REG01 / Power config` - charge enable state and OTG state.
- `BQ24195 REG02 / Charge current` - decoded charge current setpoint and low-current flag.
- `BQ24195 REG05 / Watchdog/timer` - watchdog timeout and charge timer enable state.
- `BQ24195 REG08 / System status` - charge status plus DPM, Power Good and VSYS flags.
- `BQ24195 REG09 / Fault status` - decoded charger fault state. `input-fault` means the charger IC reports an input supply problem, typically missing/invalid VBUS, input undervoltage, overload/current-limit collapse, or a bad slot power path.
- `BQ24195 REG0A / Part/rev` - part and revision fields.

## Boot behavior

The config is network-first:

- Main I2C scan is disabled by default.
- OLED update interval is `never`; updates start after WiFi and API are connected.
- Full BQ diagnostics are on demand.
- Lightweight auto polling queues all slots every 10 seconds after WiFi/API readiness and reads internal TC1047 plus internal INA219 only.
- `/bq` remains lightweight at boot because it does not read I2C in the request handler.

## Safety limits

The current native diagnostics are limited to known-safe BQ24195 host-control writes:

- BQ24195 writes are limited to `EN_HIZ`, `CHG_CONFIG`, `IINLIM`, `ICHG`, watchdog disable and watchdog reset.
- INA219 write is limited to diagnostic configuration `0x399F`.
- PCF8574 control is full-byte active-low HC4067 mux selection. Do not mix arbitrary read-modify-write PCF8574 behavior with helper masks.
- PCA9685/C16 Q34 tests are experimental and share the unresolved `0x4F` address risk with internal INA219 in the current helper.
- Active charge/discharge control remains out of scope until output mapping and a verified stop-all-outputs routine exist.

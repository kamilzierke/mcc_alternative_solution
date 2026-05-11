# MCC Pro component pinout and control reference

Date: 2026-05-11

This is a firmware-oriented reference for components identified in the MCC Pro board inventory. It captures pin roles, relevant properties and safe control/query patterns. It is not a replacement for datasheets.

## Board-level buses and known addresses

Observed I2C bus on ESP8266:

- SDA: GPIO4
- SCL: GPIO5
- ESPHome currently uses 50 kHz; ESP8266EX I2C is software-driven and the datasheet describes 100 kHz as the maximum I2C clock.

Known MCC Pro addresses:

| Address | Device | Board role |
| --- | --- | --- |
| `0x27` | PCF8574 / IC25 | 8-bit expander; P0 controls U10 temperature mux enable |
| `0x3C` | SSD1306 OLED | 128x64 status display |
| `0x41` | INA219B / U47 | current/power monitor; A0 high, A1 low |
| `0x45` | INA219B / U34 | current/power monitor; A0 high, A1 high; path/slot association still being traced |
| `0x4F` | PCA9685PW | 16-channel PWM controller |
| `0x6B` | BQ24195 | charger IC behind TCA9548A channel switches |
| `0x70` | TCA9548A | I2C mux for 8 BQ24195 devices |
| `0x71` | TCA9548A | second I2C mux for 8 BQ24195 devices |

## ESP8266 / ESP-12F

Relevant package/peripheral facts:

- Operating voltage: 2.5 V to 3.6 V.
- GPIOs are bidirectional/tristate and multiplexed with UART, SPI, I2C, I2S, PWM and IR functions.
- ADC input is `TOUT`; external ADC mode range is 0 V to 1.0 V.
- Boot-sensitive pins include GPIO0, GPIO2 and GPIO15/MTDO.
- ESP8266EX datasheet maps default software I2C examples to GPIO14/GPIO2, but the MCC Pro board uses working ESPHome GPIO4/GPIO5 bit-banged I2C.

MCC Pro use:

- GPIO16/GPIO14/GPIO12/GPIO13 drive 74HC4067 select lines S3/S2/S1/S0.
- A0 reads the selected analog mux output.
- GPIO4/GPIO5 are the board I2C bus.

Firmware cautions:

- Treat GPIO15 as high-risk until the PWM/discharge conflict is resolved.
- Keep ADC scaling board-specific; current TC1047 path uses effective full-scale `3.02 V`, not raw ESP8266 1.0 V.

Source: Espressif ESP8266EX datasheet.

## PCF8574 / IC25 I2C I/O expander

Part function:

- 8-bit I2C GPIO expander, P0..P7.
- Quasi-bidirectional I/O; there is no separate direction register.
- Power-on state of I/O pins is high.
- Supply range in TI datasheet: 2.5 V to 6 V.
- I2C max frequency in TI product data: 100 kHz.

Typical pin roles:

- SCL/SDA: I2C bus.
- A0/A1/A2: address select.
- P0..P7: quasi-bidirectional I/O.
- INT: open-drain interrupt output.
- VCC/GND: supply.

MCC Pro confirmed use:

- Address `0x27`.
- IC25 P0, physical pin 4 in the current trace, drives U10 74HC4067 enable.
- U10 enable is active-low, with R4 pulling enable high by default.

Safe control pattern:

1. Read current PCF byte from `0x27`.
2. Clear bit 0 only: `pcf_enable = pcf_before & 0xFE`.
3. Write the modified byte.
4. Read temperature mux channels.
5. Restore the original byte.

Risk notes:

- Because pins are quasi-bidirectional, writing `1` releases/pulls weakly high and writing `0` actively pulls low.
- Never sweep unknown PCF bits with cells installed; other bits may enable power paths or muxes.

## 74HC4067 / U10 and remaining muxes

Part function:

- 16-channel analog multiplexer/demultiplexer, SP16T.
- Pins: `Y0..Y15` independent analog I/O, `Z` common analog I/O, `S0..S3` select inputs, `E` digital enable.
- `E` high turns all switches off; `E` low enables selected channel.
- Nexperia 74HC4067 supply range: 2 V to 10 V.
- Typical on resistance: 80 ohm at 4.5 V, 70 ohm at 6 V, 60 ohm at 9 V.
- Break-before-make behavior is typical.

MCC Pro confirmed U10 use:

- `Y0..Y15`: TC1047 C1..C16 analog outputs.
- `Z`: ESP8266 A0 path.
- `S0..S3`: GPIO13/GPIO12/GPIO14/GPIO16 respectively in the working firmware.
- `E`: IC25 PCF8574 P0, active-low.

Select logic:

```text
channel = S0 + 2*S1 + 4*S2 + 8*S3
```

Safe read sequence for U10:

1. Enable U10 through PCF8574 P0 low.
2. Set S0..S3 to target channel.
3. Wait for analog settling.
4. Average ADC samples.
5. Disable/restore PCF state.

Open work:

- Remaining 74HC4067 devices are identified but their enable/common pins and signal domains still need tracing.

## TC1047 / TC1047A temperature sensors

Part function:

- Analog temperature-to-voltage sensor.
- 3-pin SOT-23B/TO-236 style package.
- Pins: VDD, VOUT, VSS.
- Output is proportional to temperature.

Key electrical behavior:

- TC1047 supply range: 2.7 V to 4.4 V.
- TC1047A supply range: 2.5 V to 5.5 V.
- Temperature range: -40 C to +125 C.
- Typical output points: 100 mV at -40 C, 500 mV at 0 C, 750 mV at +25 C, 1.75 V at +125 C.
- Slope: 10 mV/C typical.
- Typical supply current: 35 uA.

Conversion used in firmware:

```text
temp_C = (voltage_V - 0.500) * 100 + offset_C
```

MCC Pro use:

- 16 sensors, one per cell slot.
- Sensor outputs feed U10 channels Y0..Y15.

## TCA9548A I2C switch

Part function:

- 1-to-8 bidirectional I2C/SMBus switch.
- Upstream SDA/SCL fans out to downstream SD0/SC0 through SD7/SC7.
- Address pins A0..A2 allow up to eight devices.
- Active-low RESET input.
- Power-up/reset state: all channels deselected.
- Supply range: 1.65 V to 5.5 V.
- I2C clock range: 0 to 400 kHz.
- Supports voltage translation by using per-channel pull-up rails.

PW/TSSOP-24 pinout landmarks:

| Pin | Signal | Notes |
| --- | --- | --- |
| 1,2,21 | A0,A1,A2 | Address inputs |
| 3 | RESET | Active-low reset |
| 4/5 .. 19/20 | SD0/SC0 .. SD7/SC7 | Downstream I2C channels |
| 22/23 | SCL/SDA | Upstream controller bus |
| 24 | VCC | Supply |
| 12 | GND | Ground |

Control register:

- Write one byte to the TCA address.
- Bit `n` selects channel `n`.
- Multiple channels can be selected, but MCC Pro diagnostics should select only one BQ channel at a time.
- `0x00` deselects all channels.
- Channel activation takes effect after STOP condition.

MCC Pro use:

- Two devices at `0x70` and `0x71`.
- Each exposes up to eight downstream BQ24195 devices at repeated address `0x6B`.
- U38 is confirmed as the `0x71` device: A0 is tied high to 5 V, A1/A2 are tied low, RESET is tied high, and SC7/SD7 route to the C16 BQ24195 clock/data lines.
- C16 read-only ESPHome diagnostic confirmed BQ presence and cell-state response through U38 channel 7: empty slot logged `REG08=0x1D` and inserted-cell slot logged `REG08=0x2C`.

Safe query pattern:

1. Write `0x00` to both TCA devices.
2. Select exactly one TCA and one channel: `1 << channel`.
3. Probe/read BQ24195 at `0x6B`.
4. Deselect channel before moving to the next slot.

## BQ24195 single-cell charger IC

Part function:

- I2C-controlled single-cell Li-Ion/Li-Poly charger with NVDC power-path management.
- TI product data: 4.5 A max charge current for BQ24195, 3.9 V to 17 V input range, 22 V absolute max VIN, 3.5 V to 4.4 V battery regulation range.
- Integrated synchronous switching MOSFETs, current sensing, battery temperature monitoring and safety timers.
- VQFN-24 package.

Important pins/pin groups:

| Pin/group | Role |
| --- | --- |
| VBUS | charger input voltage |
| PMID | input/boost intermediate node |
| REGN | internal LDO / gate-drive rail, external capacitor required |
| BTST, SW | buck/boost switching bootstrap and switch nodes |
| SYS | system output / power-path node |
| BAT | battery terminal |
| TS1, TS2 | battery temperature sense inputs |
| SCL, SDA | I2C |
| INT | interrupt output to host |
| STAT | charge/fault status output |
| CE | charge enable input, active-low naming in datasheet/package |
| OTG | boost/OTG mode control input |
| ILIM | input current limit set pin |
| D+, D- | USB BC1.2 detection inputs |
| PGND | power ground |

I2C/register behavior:

- Address: `0x6B`.
- Registers `REG00..REG0A`.
- `REG00..REG07`: read/write configuration.
- `REG08..REG0A`: read-only status/fault/vendor-part-revision.
- Reset defaults include `REG00=0x30`, `REG01=0x1B`, `REG02=0x60`, `REG03=0x11`, `REG04=0xB2`, `REG05=0x9A`, `REG06=0x03`, `REG07=0x4B`.

High-value register groups:

| Register | Use |
| --- | --- |
| `REG00` | input source control: Hi-Z, VINDPM, input current limit |
| `REG01` | power-on configuration |
| `REG02` | charge current |
| `REG03` | precharge / termination current |
| `REG04` | charge voltage |
| `REG05` | termination / timer |
| `REG06` | thermal regulation |
| `REG07` | misc operation |
| `REG08` | system status |
| `REG09` | fault status |
| `REG0A` | vendor / part / revision |

MCC Pro safe mode:

- Read-only diagnostics may read `REG00..REG0A`.
- Do not write BQ registers until slot mapping, external MOSFET control and thermal behavior are understood.
- Always select a single TCA channel before accessing `0x6B`.

## INA219B current/power monitor

Part function:

- I2C/SMBus current shunt and power monitor.
- Measures shunt voltage and bus voltage; can report current and power after calibration.
- Bus voltage measurement range: 0 V to 26 V.
- Supply range: 3 V to 5.5 V.
- Supply current: up to 1 mA.
- 16 programmable I2C addresses.
- INA219B grade has 0.5% maximum accuracy over temperature per TI product data.

Typical pins:

- IN+ / IN-: shunt sense inputs.
- VCC/GND: supply.
- SCL/SDA: I2C.
- A0/A1: address pins.

Register map:

| Register | Address | Access | Use |
| --- | ---: | --- | --- |
| Configuration | `0x00` | R/W | reset, bus range, PGA, ADC averaging/mode |
| Shunt voltage | `0x01` | R | signed shunt voltage; 10 uV LSB |
| Bus voltage | `0x02` | R | bus voltage/status |
| Power | `0x03` | R | valid after calibration |
| Current | `0x04` | R | valid after calibration |
| Calibration | `0x05` | R/W | sets current and power scaling |

MCC Pro current diagnostic behavior:

- U47 address: `0x41` from A0 high and A1 low. This is the current tested ESPHome diagnostic path.
- U34 address: `0x45` from A0 high and A1 high. Its path/slot association still needs tracing; it may not appear in the boot scan unless the relevant bus path is selected.
- Current helper config writes `0x399F` to the U47 configuration register, then reads shunt and bus voltage.
- Without knowing shunt value, current/power values should be treated as uncalibrated.

## PCA9685PW 16-channel PWM controller

Part function:

- 16-channel, 12-bit I2C PWM LED/controller IC.
- Supply range: 2.3 V to 5.5 V.
- Inputs/outputs are 5.5 V tolerant.
- Fm+ I2C, up to 1 MHz.
- Internal oscillator around 25 MHz typical.
- Output frequency programmable through `PRE_SCALE`; NXP product page gives typical range around 24 Hz to 1526 Hz.
- Each output can be off, fully on, or controlled by 12-bit on/off timing.
- Outputs can be open-drain or totem-pole; OE pin is active-low global output enable.

Important pins:

- SDA/SCL: I2C.
- A0..A5: address pins.
- LED0..LED15: PWM outputs.
- OE: asynchronous active-low output enable.
- EXTCLK: optional external clock input.
- VDD/GND: logic supply.

Register landmarks:

| Register | Address | Use |
| --- | ---: | --- |
| MODE1 | `0x00` | sleep, auto-increment, restart, all-call/subaddress control |
| MODE2 | `0x01` | output driver behavior |
| LED0_ON_L | `0x06` | first byte of channel 0 timing |
| LEDn block | `0x06 + 4*n` | ON_L, ON_H, OFF_L, OFF_H |
| ALL_LED_* | `0xFA..0xFD` | load all channel timing registers |
| PRE_SCALE | `0xFE` | PWM frequency prescaler |

Safe diagnostics:

- Read-only dump: read `MODE1`, `MODE2`, `PRE_SCALE`, and each 4-byte channel block.
- Do not write PWM registers until each PCA output is physically mapped.
- To change frequency in future, datasheet requires sleep/oscillator sequencing; do not write `PRE_SCALE` while normal mode is active.

MCC Pro observed address:

- `0x4F`.

## SSD1306 OLED controller/display module

Part function:

- 128x64 dot-matrix OLED/PLED segment/common driver with controller.
- Supports I2C, SPI and parallel modes depending on module wiring.
- Common I2C address for board modules: `0x3C`, observed on MCC Pro.

Useful command landmarks:

| Command | Use |
| --- | --- |
| `0xAE` / `0xAF` | display off/on |
| `0x20` | memory addressing mode |
| `0x21` | column address |
| `0x22` | page address |
| `0x40..0x7F` | display start line |
| `0x81` | contrast |
| `0xA0` / `0xA1` | segment remap |
| `0xA6` / `0xA7` | normal/inverse display |
| `0xA8` | multiplex ratio |
| `0xC0` / `0xC8` | COM scan direction |
| `0xD3` | display offset |
| `0xDA` | COM pins hardware config |
| `0x8D` | charge pump setting |

MCC Pro use:

- ESPHome `ssd1306_i2c` at `0x3C`, model `SSD1306 128x64`.
- Display should be treated as shared I2C load only; it does not participate in charger control.

## LM393DT dual comparator

Part function:

- Dual low-power voltage comparator.
- ST product page: single-supply range 2 V to 36 V or split supplies +/-1 V to +/-18 V.
- Output stage is open-collector style in the LM393 family; external pull-up/load network determines logic level.
- Input common-mode range includes ground when single-supplied.

Typical SO-8 pinout:

| Pin | Signal |
| ---: | --- |
| 1 | OUT1 |
| 2 | IN1- |
| 3 | IN1+ |
| 4 | VCC- / GND |
| 5 | IN2+ |
| 6 | IN2- |
| 7 | OUT2 |
| 8 | VCC+ |

MCC Pro status:

- 8 devices identified.
- Threshold/status function still unknown.
- Because outputs are likely open-collector, tracing pull-ups and destinations is more important than measuring unloaded output voltage.

## IR4427S / IR4427 dual low-side gate driver

Part function:

- Dual low-side MOSFET/IGBT gate driver.
- Two channels.
- Input VCC range: 6 V to 20 V.
- Typical output capability from Infineon product page: 2.3 A source, 3.3 A sink.
- Inputs are CMOS Schmitt-triggered.
- Outputs are in phase with inputs for IR4427.
- Also available in 8-lead SOIC.

Typical 8-pin dual-driver pinout family:

| Pin | Signal |
| ---: | --- |
| 1 | NC |
| 2 | INA |
| 3 | GND |
| 4 | INB |
| 5 | OUTB |
| 6 | VS / VCC |
| 7 | OUTA |
| 8 | NC |

MCC Pro status:

- Two devices identified from archive sheet.
- Likely drive fan/resistor cooling or larger MOSFET gate nodes, but net role is not confirmed.

Firmware caution:

- This is not an I2C device. It is controlled by logic inputs. Do not drive suspected inputs until downstream MOSFET/load wiring is traced.

## AP3020 MOSFETs

Part function:

- N-channel MOSFETs identified by marking `683CK 3020`.
- 32 devices on board, likely charge/discharge switching around slots.

Control behavior:

- Gate voltage controls drain-source conduction.
- Actual active polarity depends on whether the MOSFET is used as low-side, high-side, back-to-back switch, or part of a driver stage.

MCC Pro status:

- Gate map is unresolved.
- Treat every PCA/PCF/GPIO signal that reaches these MOSFET gates as safety-critical.

## BC849C small-signal NPN transistors

Part function:

- NPN general-purpose transistor in SOT-23.
- Nexperia data: 100 mA max collector current, 30 V max VCEO, hFE group C 420..800, 250 mW package dissipation.
- Marking family matches `2C`.

Typical SOT-23 BC849 pinout:

| Pin | Signal |
| ---: | --- |
| 1 | base |
| 2 | emitter |
| 3 | collector |

MCC Pro status:

- O9/O10 identified.
- Likely level shifting, low-side switching or signal conditioning. Trace base resistors and collector pull-up/load to determine logic polarity.

## LM1117MP-3.3 / 3.3 V LDO

Part function:

- 3.3 V low-dropout linear regulator family, SOT-223.
- TI LM1117MP-3.3 reference part is an 800 mA LDO.
- Common SOT-223 fixed-regulator pinout: GND/ADJ, VOUT tab, VIN, but verify exact vendor/package before relying on pin numbers.

MCC Pro status:

- Marking `37CJ N05A`, identified from archive sheet as HGSEMI LM1117MP-3.3/TR.
- Likely local 3.3 V rail source.

Firmware relevance:

- No direct control.
- Brownout/noise here affects ESP8266, I2C pull-ups and logic thresholds.

## SD8942 synchronous buck converter

Part function:

- 600 kHz synchronous step-down converter.
- Archive/source datasheet describes 4.5 V to 16 V input range, 2 A output current, 0.6 V feedback reference, internal compensation, soft-start, over-current hiccup and thermal shutdown.
- SOT23-6 package.

Typical pins from reference application:

| Signal | Role |
| --- | --- |
| IN | input supply |
| SW | switching node |
| BS | bootstrap |
| FB | feedback divider input |
| EN | enable |
| GND | ground |

MCC Pro status:

- Marking `A6162y`, likely board power rail converter.
- No firmware control unless EN is tied to MCU/expander, which is not yet known.

## Source URLs

- Espressif ESP8266EX datasheet: https://documentation.espressif.com/0a-esp8266ex_datasheet_en.html
- TI PCF8574 datasheet/product: https://www.ti.com/lit/ds/symlink/pcf8574.pdf
- Nexperia 74HC4067PW product: https://www.nexperia.com/product/74HC4067PW
- Microchip TC1047/TC1047A datasheet: https://ww1.microchip.com/downloads/en/devicedoc/21498d.pdf
- TI TCA9548A datasheet: https://www.ti.com/lit/ds/symlink/tca9548a.pdf
- TI BQ24195 datasheet/product: https://www.ti.com/lit/ds/symlink/bq24195.pdf
- TI INA219 datasheet/product: https://www.ti.com/lit/ds/symlink/ina219.pdf
- NXP PCA9685 product/datasheet: https://www.nxp.com/products/power-drivers/lighting-driver-and-controller-ics/led-drivers/16-channel-12-bit-pwm-fm-plus-ic-bus-led-driver%3APCA9685
- Solomon Systech SSD1306 datasheet mirror: https://www.radiolocman.com/datasheet/pdf.html?di=168297
- ST LM393 product page: https://www.st.com/content/st_com/en/products/amplifiers-and-comparators/comparators/standard-comparators/lm393.html
- Infineon IR4427 product page: https://www.infineon.com/part/IR4427
- Nexperia BC849C product: https://www.nexperia.com/product/BC849C
- TI LM1117MP-3.3 reference datasheet mirror: https://www.alldatasheet.com/datasheet-pdf/pdf/807152/TI1/LM1117MP-3.3.html
- SHOUDING SD8942 datasheet mirror: https://www.alldatasheet.com/datasheet-pdf/pdf/1151658/SHOUDING/SD8942.html

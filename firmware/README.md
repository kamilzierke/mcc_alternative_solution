# MCC OSS read-only firmware

Minimal diagnostic firmware for ESP8266 / PlatformIO.

## Confirmed pins

- GPIO13 = mux S0
- GPIO12 = mux S1
- GPIO14 = mux S2
- GPIO16 = mux S3
- GPIO4 = I2C SDA in working ESPHome config
- GPIO5 = I2C SCL in working ESPHome config

## Confirmed TC1047 temperature path

```text
TC1047 C1..C16 -> U10 74HC4067 Y0..Y15 -> U10 Z -> A0
U10 E -> IC25 PCF8574 pin 4 / P0
R4 10k -> pull-up U10 E to VCC
```

Effective ADC full-scale for TC1047 path: `3.02 V`.

## Build

```bash
cd firmware
pio run
```

## Safety

Firmware must remain read-only for BQ/PCA/MOSFET power outputs until the board output map is fully traced.

## Archived proprietary/reference firmware inventory

The local archive `c:\Users\Kamil\Downloads\megacnc-main.zip` contains proprietary/reference firmware artifacts. They are not copied into this repository; keep only hashes and compatibility notes here.

| Archive path | Size | SHA256 |
| --- | ---: | --- |
| `Megacell Firmwares/CellDoctor.production.ino.1.0.2_MCCRegular.bin` | 494512 | `D8466D35FADCB475D5BAEFE5A05656CDFFE38F160957501C897131079E8FA4AB` |
| `Megacell Firmwares/CellDoctor.production.ino.1.0.4_MCCPro.bin` | 503648 | `35A1FDB7685B8D589DA685D099DB9B63C103430CECE7711E150F96C9F7C93644` |
| `Megacell Firmwares/CellDoctor Production 1.1.0/firmware.bin` | 543328 | `17F569B3C3CAE048DDC1837B4EDFB2A966D45F5B001D3515243A5A753BE8E8CE` |
| `Megacell Firmwares/CellDoctor Production 1.1.0/spiffs.bin` | 1024000 | `17AA54337CBDB317AB4DE05699895741EC82CD5359BD6A8688990AC02B5C39CD` |
| `Megacell Firmwares/CellDoctor Production 1.1.0/MCC16_Flasher.exe` | 14850507 | `95C43EFBE346C04CDDAC8DE2070EE0CF618A1FE91796AE0C7A7255C8B1923C03` |
| `Megacell Firmwares/CellDoctor Production 1.1.0/Changelog.txt` | n/a | `4688CB0A08FB9F2C835B3ED974BBBBBD98349D01B1098147592590E361EF657F` |

Reference changelog notes for compatibility:

- v1.1.0 changelog date: 2026-03-20.
- Web dashboard exposes system stats such as heap, RSSI, fan, temperature, uptime and fragmentation.
- API responses were optimized for low heap; `handle_get_cells_info` is described as streamed/zero-heap.
- Dashboard cell data was compacted from large verbose JSON to a small array payload.
- MegaCNC command compatibility was intended to remain stable.
- ESP8266 heap remains tight with 16 active cells; low-memory API handling is part of compatibility.
- `GET /api/get_config_info` is documented as returning an empty response in v1.1.0 because the original handler was commented out.

Saved legacy page material shows a regular MCC flashing example using offset `0x00000`, flash mode `dio`, and `CellDoctor.production.ino.1.0.2_MCCRegular.bin`. Treat it as historical reference, not as a command to run on MCC Pro.

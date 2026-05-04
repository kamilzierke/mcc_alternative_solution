# MegaCell Charger Open Source Recon Pack v0.3

Ten katalog jest wynikiem eksploracji zaktualizowanego `Downloads.zip` oraz kolejnego reverse engineeringu MCC Pro na działającej płytce.

Wersja v0.3 dodaje potwierdzony tor odczytu temperatury wszystkich 16 slotów: `TC1047 -> U10 74HC4067 -> A0 ESP8266`, z włączaniem muxa przez `IC25 PCF8574 P0` i kalibracją efektywnej pełnej skali ADC do `3.02 V`.

Cel pakietu:

1. zebrać potwierdzone fakty techniczne,
2. rozdzielić wersję regularną od Pro,
3. oddzielić dane potwierdzone od hipotez,
4. przygotować bezpieczny firmware read-only dla ESP8266,
5. dostarczyć endpointy diagnostyczne do dalszego reverse engineeringu: I2C, TCA9548A, PCF8574, PCA9685, BQ24195 i surowy ADC przez mux HC4067,
6. udokumentować potwierdzony odczyt temperatury TC1047 dla slotów C1..C16.

## Najważniejsze ustalenia v0.2

- ESP8266/ESP-12F jest głównym MCU.
- Slotów jest 16.
- Linie wyboru muxa są potwierdzone z arkusza:
  - `GPIO13 = S0`,
  - `GPIO12 = S1`,
  - `GPIO14 = S2`,
  - `GPIO16 = S3`.
- Wersja regularna z logów forum wykrywa I2C: `0x20`, `0x21`, `0x22`, `0x23`, `0x3C`, `0x4F`.
- Wersja Pro z arkusza zawiera m.in. `2× TCA9548A`, `16× BQ24195`, `PCA9685`, `PCF8574`, `2× INA219`, `5× HC4067`, `16× TC1047`, `32× AP3020`, `8× LM393`.
- `GPIO15` jest konfliktem: arkusz opisuje go jako `FUNC2`, forum opisuje globalny PWM rozładowania 100 Hz. Nie używać aktywnie bez tracingu.


## Najważniejsze ustalenia v0.3 — tor temperatur TC1047

- Czujniki temperatury TC1047 dla slotów C1..C16 są potwierdzone i działają.
- PCB refs czujników: C1=U5, C2=U6, C3=U7, C4=U8, C5=U16, C6=U17, C7=U18, C8=U19, C9=U24, C10=U25, C11=U26, C12=U27, C13=U32, C14=U33, C15=U34, C16=U35.
- `U10` to mux temperatur `74HC4067PW`.
- `R4 = 10 kΩ` podciąga wejście `E` muxa U10 do VCC, więc U10 jest domyślnie wyłączony.
- `IC25` to `PCF8574` pod adresem `0x27`; jego pin 4 (`P0`) steruje linią `E` U10 przez ściąganie jej do GND.
- Aktywacja muxa temperatur: zapis do PCF8574 `0xFE` (`P0=LOW`, reszta high/released). Dezaktywacja / stan bezpieczny: `0xFF`.
- Linie wyboru muxa: `GPIO13=S0`, `GPIO12=S1`, `GPIO14=S2`, `GPIO16=S3`.
- Mapowanie wejść U10: `C1->Y0`, `C2->Y1`, ..., `C8->Y7`, `C9->Y8`, ..., `C16->Y15`.
- Efektywna pełna skala toru ADC dla tego pomiaru: `3.02 V`, skalibrowana pomiarem multimetrem (`ESPHome 0.8761 V` vs multimetr `0.801 V`).

Szczegóły są w `docs/temperature-readout-tc1047.md` oraz `hardware/temperature_sensor_map.csv`.

## Zawartość

- `docs/technical-spec-v0.2.md` — aktualna specyfikacja techniczna z poziomami pewności.
- `docs/temperature-readout-tc1047.md` — potwierdzony tor odczytu temperatur C1..C16.
- `docs/firmware-readonly-plan-v0.2.md` — plan firmware read-only i procedura pierwszych testów.
- `docs/api-compatibility.md` — API MegaCell/MegaCNC oraz zgodność firmware MVP.
- `docs/reverse-engineering-next-steps.md` — procedura dojścia do prawdziwego pinoutu i toru pomiarowego.
- `hardware/component_inventory.csv` — komponenty z arkusza użytkownika w formie roboczej.
- `hardware/gpio_map.csv` — ustalone GPIO.
- `hardware/known_i2c_addresses.csv` — adresy I2C z arkusza i logów forum.
- `hardware/pinmap_template.csv` — szablon do dalszego tracingu PCB.
- `hardware/temperature_sensor_map.csv` — potwierdzone mapowanie czujników temperatury, U10/Yx i PCF P0.
- `sources/extracted/` — wyciągnięte źródła tekstowe z paczki.
- `firmware/` — projekt PlatformIO/Arduino ESP8266.

## Stan firmware

Firmware v0.2/v0.3 jest celowo read-only dla torów mocy:

- odczytuje 16 pozycji muxa przez A0,
- potrafi odczytać temperatury TC1047 przez potwierdzony mux U10 po aktywacji IC25/P0,
- skanuje I2C,
- skanuje kanały TCA9548A 0x70/0x71,
- odczytuje PCF8574 bez zapisu,
- odczytuje podstawowe rejestry PCA9685 bez zapisu,
- odczytuje rejestry BQ24195 bez zapisu,
- wystawia API podobne do MegaCell/MegaCNC.

Nie steruje ładowaniem, rozładowaniem, wentylatorami ani MOSFETami. To jest intencjonalne: najpierw musi powstać potwierdzone `stop_all_outputs()`. Bez tego aktywne sterowanie Li-Ion to rosyjska ruletka, tylko zamiast bębna masz thermal runaway.

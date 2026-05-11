# OLED Status Display Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a three-page SSD1306 OLED status carousel to the current MCC Pro ESPHome configuration.

**Architecture:** Keep the feature inside `esphome/mcc-pro.yaml`. Reuse the existing I2C bus and TC1047 template sensors, add lightweight Wi-Fi/IP/RSSI diagnostics, and render the OLED from one display lambda controlled by a global page index.

**Tech Stack:** ESPHome YAML, ESP8266, SSD1306 I2C display at `0x3C`, ESPHome `font`, `display`, `wifi_signal`, `wifi_info`, `globals`, `interval`, and C++ lambdas.

---

## File Structure

- Modify: `esphome/mcc-pro.yaml`
  - Add `id: mcc_wifi` under the existing `wifi:` block.
  - Add `id: api_server` under the existing `api:` block.
  - Add `globals:` for the OLED carousel page.
  - Extend the existing `interval:` block with a 5-second page advance.
  - Extend the existing `sensor:` block with a diagnostic `wifi_signal` RSSI sensor.
  - Add `text_sensor:` with a `wifi_info` IP address sensor.
  - Add `font:` with a small Google Font suitable for a dense 128x64 OLED.
  - Add `display:` with `ssd1306_i2c`, `model: "SSD1306 128x64"`, `address: 0x3C`, and a three-page lambda.

No helper headers or firmware source files should change. The existing TC1047 scan script must remain behaviorally unchanged.

## External References

- ESPHome SSD1306 I2C display docs: https://esphome.io/components/display/ssd1306/
- ESPHome display text/printf docs: https://esphome.io/components/display/
- ESPHome font docs: https://esphome.io/components/font/
- ESPHome Wi-Fi signal sensor docs: https://esphome.io/components/sensor/wifi_signal/
- ESPHome Wi-Fi info text sensor docs: https://esphome.io/components/text_sensor/wifi_info/
- ESPHome native API `api.connected` lambda docs: https://esphome.io/components/api/
- ESPHome Wi-Fi `wifi.connected` lambda docs: https://esphome.io/components/wifi/

---

### Task 1: Add Network IDs, Diagnostics, And Carousel State

**Files:**
- Modify: `esphome/mcc-pro.yaml`

- [ ] **Step 1: Verify the OLED feature is absent before editing**

Run:

```powershell
Select-String -LiteralPath '\\FILESERVER\kamil\Projekty\mcc_alternative_solution\esphome\mcc-pro.yaml' -Pattern 'ssd1306_i2c|oled_page|wifi_signal_db|mcc_ip_address|api_server|mcc_wifi'
```

Expected before this task: no output for `ssd1306_i2c`, `oled_page`, `wifi_signal_db`, `mcc_ip_address`, `api_server`, or `mcc_wifi`.

- [ ] **Step 2: Add component IDs to existing `wifi:` and `api:` blocks**

Change the existing blocks to include these IDs:

```yaml
wifi:
  id: mcc_wifi
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  power_save_mode: none
  min_auth_mode: WPA2
```

```yaml
api:
  id: api_server
  reboot_timeout: 0s
```

- [ ] **Step 3: Add the OLED carousel global after the `i2c:` block**

Insert this block after the existing `i2c:` section:

```yaml
globals:
  - id: oled_page
    type: int
    restore_value: false
    initial_value: "0"
```

- [ ] **Step 4: Extend the existing `interval:` block**

Keep the existing TC1047 scan interval unchanged and add a second interval item:

```yaml
interval:
  - interval: ${tc1047_scan_interval}
    then:
      - script.execute: scan_all_tc1047_temperatures

  - interval: 5s
    then:
      - lambda: |-
          id(oled_page) = (id(oled_page) + 1) % 3;
```

- [ ] **Step 5: Add Wi-Fi RSSI sensor at the top of the existing `sensor:` list**

The first entry under `sensor:` should become:

```yaml
sensor:
  - platform: wifi_signal
    id: wifi_signal_db
    name: "WiFi Signal dB"
    entity_category: diagnostic
    update_interval: 30s

  - platform: template
    id: c1_tc1047_temperature
    name: "C1 TC1047 Temperature"
    icon: "mdi:thermometer"
    device_class: temperature
    state_class: measurement
    unit_of_measurement: "°C"
    accuracy_decimals: 1
    update_interval: never
```

Do not rewrite the remaining `c2_tc1047_temperature` through `c16_tc1047_temperature` entries.

- [ ] **Step 6: Add Wi-Fi IP text sensor before `button:`**

Insert this block between the end of the `sensor:` list and the existing `button:` block:

```yaml
text_sensor:
  - platform: wifi_info
    ip_address:
      id: mcc_ip_address
      name: "IP Address"
      entity_category: diagnostic
```

- [ ] **Step 7: Run ESPHome validation**

Run:

```powershell
esphome config '\\FILESERVER\kamil\Projekty\mcc_alternative_solution\esphome\mcc-pro.yaml'
```

Expected: validation succeeds. If the command is not installed, record the exact error and use Task 3's compile command later if the project uses another ESPHome entrypoint.

- [ ] **Step 8: Commit Task 1**

Only commit the YAML file if validation succeeds or if the only blocker is a missing local `esphome` executable:

```powershell
git -c safe.directory='*' -C '\\FILESERVER\kamil\Projekty\mcc_alternative_solution' add -- esphome/mcc-pro.yaml
git -c safe.directory='*' -C '\\FILESERVER\kamil\Projekty\mcc_alternative_solution' commit -m 'feat: add OLED status diagnostics'
```

---

### Task 2: Add The SSD1306 Display Renderer

**Files:**
- Modify: `esphome/mcc-pro.yaml`

- [ ] **Step 1: Verify the display renderer is absent before editing**

Run:

```powershell
Select-String -LiteralPath '\\FILESERVER\kamil\Projekty\mcc_alternative_solution\esphome\mcc-pro.yaml' -Pattern 'platform: ssd1306_i2c|id: oled_font|Temps C01-C08|Temps C09-C16'
```

Expected before this task: no output.

- [ ] **Step 2: Add the OLED font after the `text_sensor:` block**

Insert this block after `text_sensor:` and before `button:`:

```yaml
font:
  - file:
      type: gfonts
      family: Roboto Mono
    id: oled_font
    size: 9
    bpp: 1
    glyphs:
      - " !#%()+,-./0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
```

- [ ] **Step 3: Add the display block after the `font:` block**

Insert this block after `font:` and before `button:`:

```yaml
display:
  - platform: ssd1306_i2c
    model: "SSD1306 128x64"
    address: 0x3C
    update_interval: 1s
    lambda: |-
      auto temp_text = [](auto &sensor) -> const char * {
        static char buffers[16][8];
        static uint8_t index = 0;
        char *buffer = buffers[index++ % 16];
        if (sensor.has_state()) {
          snprintf(buffer, 8, "%4.1fC", sensor.state);
        } else {
          snprintf(buffer, 8, "--.-C");
        }
        return buffer;
      };

      it.printf(0, 0, id(oled_font), "MCC Pro");
      it.printf(106, 0, id(oled_font), "P%d/3", id(oled_page) + 1);
      it.line(0, 11, 127, 11);

      if (id(oled_page) == 0) {
        const bool wifi_ok = id(mcc_wifi).is_connected();
        const bool api_ok = id(api_server).is_connected();
        const char *ip = (id(mcc_ip_address).has_state() && !id(mcc_ip_address).state.empty())
                           ? id(mcc_ip_address).state.c_str()
                           : "no ip";

        it.printf(0, 15, id(oled_font), "WiFi: %s", wifi_ok ? "ok" : "down");
        it.printf(0, 25, id(oled_font), "IP: %s", ip);
        if (id(wifi_signal_db).has_state()) {
          it.printf(0, 35, id(oled_font), "RSSI: %.0f dBm", id(wifi_signal_db).state);
        } else {
          it.print(0, 35, id(oled_font), "RSSI: -- dBm");
        }
        it.printf(0, 45, id(oled_font), "API: %s", api_ok ? "ok" : "down");
        it.printf(0, 55, id(oled_font), "Scan: ${tc1047_scan_interval}");
      } else if (id(oled_page) == 1) {
        it.print(0, 15, id(oled_font), "Temps C01-C08");
        it.printf(0, 25, id(oled_font), "C01 %s C02 %s", temp_text(id(c1_tc1047_temperature)), temp_text(id(c2_tc1047_temperature)));
        it.printf(0, 35, id(oled_font), "C03 %s C04 %s", temp_text(id(c3_tc1047_temperature)), temp_text(id(c4_tc1047_temperature)));
        it.printf(0, 45, id(oled_font), "C05 %s C06 %s", temp_text(id(c5_tc1047_temperature)), temp_text(id(c6_tc1047_temperature)));
        it.printf(0, 55, id(oled_font), "C07 %s C08 %s", temp_text(id(c7_tc1047_temperature)), temp_text(id(c8_tc1047_temperature)));
      } else {
        it.print(0, 15, id(oled_font), "Temps C09-C16");
        it.printf(0, 25, id(oled_font), "C09 %s C10 %s", temp_text(id(c9_tc1047_temperature)), temp_text(id(c10_tc1047_temperature)));
        it.printf(0, 35, id(oled_font), "C11 %s C12 %s", temp_text(id(c11_tc1047_temperature)), temp_text(id(c12_tc1047_temperature)));
        it.printf(0, 45, id(oled_font), "C13 %s C14 %s", temp_text(id(c13_tc1047_temperature)), temp_text(id(c14_tc1047_temperature)));
        it.printf(0, 55, id(oled_font), "C15 %s C16 %s", temp_text(id(c15_tc1047_temperature)), temp_text(id(c16_tc1047_temperature)));
      }
```

- [ ] **Step 4: Verify all display Y coordinates are inside the 128x64 canvas**

Run:

```powershell
Select-String -LiteralPath '\\FILESERVER\kamil\Projekty\mcc_alternative_solution\esphome\mcc-pro.yaml' -Pattern ' 64, id\(oled_font\)|, 6[4-9], id\(oled_font\)'
```

Expected: no output. The highest planned text baseline is `55`, leaving room for the 9px font on a 64px display.

- [ ] **Step 5: Run ESPHome validation**

Run:

```powershell
esphome config '\\FILESERVER\kamil\Projekty\mcc_alternative_solution\esphome\mcc-pro.yaml'
```

Expected: validation succeeds and reports the `ssd1306_i2c`, `font`, `wifi_signal`, and `wifi_info` components without YAML schema errors.

- [ ] **Step 6: Commit Task 2**

```powershell
git -c safe.directory='*' -C '\\FILESERVER\kamil\Projekty\mcc_alternative_solution' add -- esphome/mcc-pro.yaml
git -c safe.directory='*' -C '\\FILESERVER\kamil\Projekty\mcc_alternative_solution' commit -m 'feat: render OLED status carousel'
```

---

### Task 3: Compile And On-Device Smoke Test

**Files:**
- Modify only if validation or compile exposes a concrete syntax issue: `esphome/mcc-pro.yaml`

- [ ] **Step 1: Compile the firmware**

Run:

```powershell
esphome compile '\\FILESERVER\kamil\Projekty\mcc_alternative_solution\esphome\mcc-pro.yaml'
```

Expected: compile succeeds. The first compile may download the Google Font declared in `font:` and cache it for later builds.

- [ ] **Step 2: If `esphome compile` is not available, try the project-specific build path**

Run:

```powershell
Get-ChildItem -LiteralPath '\\FILESERVER\kamil\Projekty\mcc_alternative_solution' -Recurse -Filter platformio.ini
```

Expected: if a `platformio.ini` is present, use the matching project build command. If no ESPHome or PlatformIO entrypoint is available locally, record that verification is blocked by missing tooling.

- [ ] **Step 3: Flash or upload using the user's normal ESPHome workflow**

Use the existing local workflow for this project. If the device is reachable over OTA, run:

```powershell
esphome upload '\\FILESERVER\kamil\Projekty\mcc_alternative_solution\esphome\mcc-pro.yaml'
```

Expected: upload succeeds and the device reboots.

- [ ] **Step 4: Watch boot logs for I2C and display setup**

Run:

```powershell
esphome logs '\\FILESERVER\kamil\Projekty\mcc_alternative_solution\esphome\mcc-pro.yaml'
```

Expected:

- I2C scan still includes `0x3C`.
- Existing expected devices such as `0x27`, `0x41`, `0x4F`, `0x70`, and `0x71` are still present when hardware is connected.
- No repeated SSD1306 setup errors.
- Existing TC1047 scan log continues to run every `${tc1047_scan_interval}`.

- [ ] **Step 5: Visually verify all three OLED pages**

Observe the OLED for at least 20 seconds.

Expected:

- Page 1 shows `MCC Pro`, `P1/3`, Wi-Fi state, IP or `no ip`, RSSI or `-- dBm`, API state, and scan interval.
- Page 2 shows `Temps C01-C08` and values or `--.-C` for slots `C01` through `C08`.
- Page 3 shows `Temps C09-C16` and values or `--.-C` for slots `C09` through `C16`.
- Pages advance about every 5 seconds.
- Existing TC1047 values still update in Home Assistant/web server.

- [ ] **Step 6: Commit any compile/smoke-test fix**

If Task 3 required a syntax or layout adjustment, commit it:

```powershell
git -c safe.directory='*' -C '\\FILESERVER\kamil\Projekty\mcc_alternative_solution' add -- esphome/mcc-pro.yaml
git -c safe.directory='*' -C '\\FILESERVER\kamil\Projekty\mcc_alternative_solution' commit -m 'fix: validate OLED display configuration'
```

If Task 3 required no changes, do not create an empty commit.

---

## Self-Review

- Spec coverage: The plan covers SSD1306 `128x64` at `0x3C`, three-page carousel, Wi-Fi/IP/API status, RSSI, all 16 temperature sensors, unknown-value fallbacks, validation, compile, and on-device smoke testing.
- Completion-marker scan: No unfinished markers or unspecified implementation steps remain.
- Type consistency: `mcc_wifi`, `api_server`, `oled_page`, `wifi_signal_db`, `mcc_ip_address`, and `oled_font` are introduced before use in the display lambda.

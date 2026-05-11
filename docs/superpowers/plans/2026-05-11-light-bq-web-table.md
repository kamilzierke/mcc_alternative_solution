# Light BQ Web Table Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the heavy `/bq` endpoint with an ESP8266-friendly table that reads only the slot selected by the user.

**Architecture:** `/bq` renders a 17-row HTML table with one `Read` link per slot and performs no I2C read. `/bq?slot=N` queues a single-slot read in the main loop, then renders the same table with only that slot populated.

**Tech Stack:** ESPHome C++ include helper, ESPAsyncWebServer through ESPHome `web_server_base`, existing TCA/BQ/INA helper functions.

---

### Task 1: Make `/bq` Single-Slot

**Files:**
- Modify: `esphome/mcc_diag_helpers.h`
- Modify: `esphome/mcc-pro.yaml`

- [x] Add a pending slot number next to the pending web request pointer.
- [x] Parse optional `slot` query parameter from `/bq?slot=N`.
- [x] Return the table immediately for `/bq` without any I2C read.
- [x] Queue only the requested slot for `/bq?slot=N`.
- [x] Render placeholder cells for all non-selected slots.
- [x] Keep existing diagnostics buttons unchanged.

### Task 2: Verify

**Files:**
- Validate: `esphome/mcc-pro.yaml`
- Validate: `esphome/mcc_diag_helpers.h`

- [x] Run `git diff --check`.
- [x] Compile ESPHome on a temporary copy with dummy secrets.
- [x] Commit the plan and implementation together.

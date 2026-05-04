# ESPHome MCC Pro TC1047 temperature readout

This folder contains the ESPHome configuration used to confirm temperature readout for all 16 slots.

Core facts:

- U10 = 74HC4067 temperature mux.
- IC25 = PCF8574 at 0x27.
- IC25 pin 4 / P0 controls U10 E active-low.
- R4 = 10k pull-up from U10 E to VCC.
- ADC effective full scale = 3.02 V.

The YAML exposes `C1 TC1047 Temperature` ... `C16 TC1047 Temperature`.

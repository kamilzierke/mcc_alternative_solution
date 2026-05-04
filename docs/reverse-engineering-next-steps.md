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

## Safety

Do not test PCF/PCA/BQ output writes with Li-Ion cells installed unless the target line is already traced and the power path is understood.

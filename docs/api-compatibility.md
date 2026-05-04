# MegaCell Charger / MegaCNC API compatibility

Replacement firmware should eventually expose MegaCell/MegaCNC-like HTTP endpoints while remaining safe during reverse engineering.

Known endpoints from prior recon:

- `/api/who_am_i`
- `/api/get_cells_info`
- `/api/dash_data`
- `/api/get_config_info`
- `/api/set_cell`
- `/api/set_cell_macro`
- `/api/reset_charger`

During read-only development, all command endpoints must be stubs returning read-only / disabled errors.

Important historical constraint: original firmware struggled to return all 16 cells at once. Iterative polling by one cell or by groups 1-8 and 9-16 is safer on ESP8266.

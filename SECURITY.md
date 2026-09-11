# Security policy

## Reporting a vulnerability or safety issue

Report security-sensitive firmware issues and safety-critical control behavior privately through the repository's GitHub Security Advisories feature. Do not include Wi-Fi credentials, API keys, serial numbers, private logs, full hardware inventories or personally identifying device data in a public issue.

For a hardware report, include a redacted description of the affected board variant, firmware revision, safe reproduction conditions and observed outcome. Do not reproduce an issue with Li-Ion cells installed unless the relevant control path and test procedure are already documented as safe.

## Local configuration and artifacts

`esphome/secrets.yaml` contains local Wi-Fi configuration and is intentionally excluded from Git. Use `esphome/secrets.example.yaml` as the public template. Keep generated logs and diagnostic artifacts outside version control.

## Supported scope

This project is currently an experimental diagnostic and replacement-firmware effort. The unresolved `0x4F` device conflict, GPIO15 role and all-output safe-off behavior mean broader power-output control is not considered production-safe.
# Release process

The project is currently in a diagnostic firmware stage. A release must describe whether it is a source snapshot, diagnostic firmware build, experimental bench build or production-safe firmware. Do not imply that a build is safe for unattended charging or discharge control unless the relevant hardware evidence exists.

## Before creating a release candidate

1. Update `VERSION`, `CHANGELOG.md`, README documentation and the relevant technical evidence.
2. Run the repository and diagnostic contract checks:

   ```powershell
   & .\scripts\verify-repository.ps1
   & .\scripts\test-synthetic.ps1
   esphome config esphome\mcc-pro-native.yaml
   ```

3. Run `esphome compile esphome\mcc-pro-native.yaml` only when all required dependencies are already available locally. Record the ESPHome version and result.
4. Review `git diff --check` and confirm that secrets, local logs, private hardware data and generated output are absent.
5. For a hardware claim, attach a report that follows [the hardware test protocol](hardware-test-protocol.md). Synthetic checks do not prove hardware behavior.

## Publication gate

Creating a tag, GitHub Release or distributed firmware artifact requires an explicit review of the release scope. The release tag must match `v` followed by the `VERSION` value. A distributed artifact should include its SHA-256 checksum and a statement of supported board variants and known limitations.

## Current restriction

No production-control release is permitted while the `0x4F` conflict, GPIO15 role and `stop_all_outputs()` behavior remain unresolved. The roadmap's safety gates take precedence over release scheduling.
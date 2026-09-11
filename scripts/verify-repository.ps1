$ErrorActionPreference = 'Stop'

$requiredPaths = @(
    'CHANGELOG.md',
    'CONTRIBUTING.md',
    'ROADMAP.md',
    'SECURITY.md',
    'VERSION',
    'docs/hardware-test-protocol.md',
    'docs/releasing.md',
    'docs/firmware-status-and-roadmap.md',
    'firmware/README.md',
    'firmware/platformio.ini',
    'firmware/include/mcc_firmware_core.h',
    'firmware/include/mcc_i2c_probe_plan.h',
    'firmware/include/mcc_bq_probe_plan.h',
    'firmware/src/main.cpp',
    'esphome/mcc-pro-native.yaml',
    'esphome/mcc_diag_contract.h',
    'esphome/secrets.example.yaml',
    'shared/mcc_diag_contract.h',
    'scripts/test-synthetic.ps1',
    'scripts/probe-main-bus-and-observe.ps1',
    'scripts/probe-bq-c01-identity-and-observe.ps1',
    'scripts/upload-and-observe.ps1',
    'tests/test_mcc_diag_contract.cpp',
    'tests/test_mcc_firmware_core.cpp',
    'tests/test_mcc_i2c_probe_plan.cpp',
    'tests/test_mcc_bq_probe_plan.cpp',
    'tests/test_mcc_uart_frame_buffer.ps1',
    'tools/mcc-live-monitor.ps1',
    'tools/mcc-uart-protocol.ps1',
    'tools/README.md'
)

$missingPaths = $requiredPaths | Where-Object { -not (Test-Path -LiteralPath $_) }
if ($missingPaths) {
    throw "Required public files are missing: $($missingPaths -join ', ')"
}

$version = (Get-Content -LiteralPath 'VERSION' -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$') {
    throw "VERSION must use semantic version syntax: $version"
}

$trackedPrivatePaths = @(
    git ls-files --cached -- 'archive/**' 'firmware_dumps/**' 'esphome/secrets.yaml'
) | Where-Object { $_ }
if ($trackedPrivatePaths) {
    throw "Private local inputs are tracked: $($trackedPrivatePaths -join ', ')"
}

$ignoredPaths = @(
    'archive/private-note.txt',
    'artifacts/diagnostic.log',
    'firmware_dumps/extracted.bin',
    'esphome/secrets.yaml'
)
foreach ($ignoredPath in $ignoredPaths) {
    git check-ignore --no-index -q -- $ignoredPath
    if ($LASTEXITCODE -ne 0) {
        throw "Expected ignored path is not ignored: $ignoredPath"
    }
}

$archiveReferences = @(git grep -n -i -E 'archive/|archived local|archived locally|legacy config' -- '*.md' ':!docs/component-control-reference.md') | Where-Object { $_ }
if ($archiveReferences) {
    throw "Public documentation names local archive material: $($archiveReferences -join '; ')"
}

git diff --check
if ($LASTEXITCODE -ne 0) {
    throw 'Patch formatting check failed.'
}

Write-Host 'Repository quality checks passed.'
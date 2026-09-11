param(
    [string]$Port = 'COM7',
    [int]$ObservationSeconds = 60
)

$ErrorActionPreference = 'Stop'

pio run --project-dir .\firmware --environment mcc_pro_tca_state_probe --target upload --upload-port $Port
if ($LASTEXITCODE -ne 0) {
    throw 'TCA state probe upload failed; live observation was not started.'
}

$monitorPath = Join-Path $PSScriptRoot '..\tools\mcc-live-monitor.ps1'
& $monitorPath -Port $Port -Baud 115200 -ObservationSeconds $ObservationSeconds -AutoConnect
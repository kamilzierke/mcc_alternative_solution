$ErrorActionPreference = 'Stop'

$outputDirectory = Join-Path (Get-Location) 'artifacts/synthetic-tests'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$testExecutable = Join-Path $outputDirectory 'test_mcc_diag_contract.exe'

& g++ -std=c++17 -Wall -Wextra -Werror -I .\shared .\tests\test_mcc_diag_contract.cpp -o $testExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Synthetic diagnostic contract test compilation failed.'
}

& $testExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Synthetic diagnostic contract tests failed.'
}

$coreTestExecutable = Join-Path $outputDirectory 'test_mcc_firmware_core.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I .\firmware\include .\tests\test_mcc_firmware_core.cpp -o $coreTestExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Synthetic firmware core test compilation failed.'
}

& $coreTestExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Synthetic firmware core tests failed.'
}

$probeTestExecutable = Join-Path $outputDirectory 'test_mcc_i2c_probe_plan.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I .\firmware\include .\tests\test_mcc_i2c_probe_plan.cpp -o $probeTestExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Synthetic I2C probe plan test compilation failed.'
}

& $probeTestExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Synthetic I2C probe plan tests failed.'
}

$bqProbeTestExecutable = Join-Path $outputDirectory 'test_mcc_bq_probe_plan.exe'
& g++ -std=c++17 -Wall -Wextra -Werror -I .\firmware\include .\tests\test_mcc_bq_probe_plan.cpp -o $bqProbeTestExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Synthetic BQ probe plan test compilation failed.'
}

& $bqProbeTestExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Synthetic BQ probe plan tests failed.'
}

& .\tests\test_mcc_uart_frame_buffer.ps1
if ($LASTEXITCODE -ne 0) {
    throw 'Synthetic UART frame buffer tests failed.'
}
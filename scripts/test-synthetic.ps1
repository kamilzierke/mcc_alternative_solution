$ErrorActionPreference = 'Stop'

$outputDirectory = Join-Path (Get-Location) 'artifacts/synthetic-tests'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$testExecutable = Join-Path $outputDirectory 'test_mcc_diag_contract.exe'

& g++ -std=c++17 -Wall -Wextra -Werror -I .\esphome .\tests\test_mcc_diag_contract.cpp -o $testExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Synthetic diagnostic contract test compilation failed.'
}

& $testExecutable
if ($LASTEXITCODE -ne 0) {
    throw 'Synthetic diagnostic contract tests failed.'
}
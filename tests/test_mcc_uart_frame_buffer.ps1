$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\tools\mcc-uart-protocol.ps1')

function Expect([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAILED: $Message" }
}

$firstChunk = 'MCC|STATUS|firmware|read-only|read-only|reported|match|diagnostic'
$firstResult = Split-MccUartBuffer -Buffer $firstChunk
Expect ($firstResult.Lines.Count -eq 0) 'partial frame must not be emitted'
Expect ($firstResult.Remainder -eq $firstChunk) 'partial frame must be retained'

$secondChunk = " mode`r`nMCC|SLOT|C01|not-sampled|read-only|no I2C read scheduled|awaiting|no I2C"
$secondResult = Split-MccUartBuffer -Buffer ($firstResult.Remainder + $secondChunk)
Expect ($secondResult.Lines.Count -eq 1) 'completed status frame must be emitted once'
Expect ($secondResult.Lines[0] -eq 'MCC|STATUS|firmware|read-only|read-only|reported|match|diagnostic mode') 'completed status frame must be intact'
Expect ($secondResult.Remainder -like 'MCC|SLOT|C01*') 'partial slot frame must be retained'

$thirdResult = Split-MccUartBuffer -Buffer ($secondResult.Remainder + " read scheduled`r`n")
Expect ($thirdResult.Lines.Count -eq 1) 'completed slot frame must be emitted once'
Expect ($thirdResult.Lines[0] -like 'MCC|SLOT|C01*') 'completed slot frame must be intact'
Expect ($thirdResult.Remainder -eq '') 'remainder must be empty after newline'

Write-Host 'MCC UART frame buffer tests passed.'
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\tools\mcc-gui\models.ps1')

function Expect([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "FAILED: $Message" }
}

# Real, hardware-confirmed BQ24195 C01 decoded snapshot (see
# docs/hardware-test-protocol.md) plus the TCA evidence suffix the
# firmware appends on every SLOT frame.
$confirmedEvidence = 'HIZ off; IIN 100mA; CHG on; OTG off; ICHG 2048mA; WD 40s; timer on; fast-charge; DPM no; PG yes; VSYS no; fault normal; part4 rev3; select=0 release70=0 release71=0'

$decoded = ConvertFrom-BQEvidence -Slot 'C01' -Match 'match' -Snapshot 'register snapshot' -Evidence $confirmedEvidence

Expect ($decoded.RawEvidence -eq $confirmedEvidence) 'RawEvidence must be preserved verbatim'
Expect ($decoded.IsDecoded) 'confirmed snapshot must be recognized as decoded'
Expect ($decoded.HizEnabled -eq $false) 'HIZ off must map to HizEnabled=false'
Expect ($decoded.IinMa -eq 100) 'IIN 100mA must map to IinMa=100'
Expect ($decoded.ChargeEnabled -eq $true) 'CHG on must map to ChargeEnabled=true'
Expect ($decoded.OtgEnabled -eq $false) 'OTG off must map to OtgEnabled=false'
Expect ($decoded.IchgMa -eq 2048) 'ICHG 2048mA must map to IchgMa=2048'
Expect ($decoded.WatchdogS -eq 40) 'WD 40s must map to WatchdogS=40'
Expect ($decoded.TimerEnabled -eq $true) 'timer on must map to TimerEnabled=true'
Expect ($decoded.ChargeStage -eq 'fast-charge') 'fast-charge token must map to ChargeStage'
Expect ($decoded.DpmActive -eq $false) 'DPM no must map to DpmActive=false'
Expect ($decoded.PowerGood -eq $true) 'PG yes must map to PowerGood=true'
Expect ($decoded.VsysActive -eq $false) 'VSYS no must map to VsysActive=false'
Expect ($decoded.Fault -eq 'normal') 'fault normal must map to Fault=normal'
Expect ($decoded.PartNumber -eq 4) 'part4 must map to PartNumber=4'
Expect ($decoded.PartRevision -eq 3) 'rev3 must map to PartRevision=3'
Expect ($decoded.TcaEvidence -eq 'select=0 release70=0 release71=0') 'trailing TCA evidence must be extracted'
Expect ($decoded.Unparsed.Count -eq 0) 'every token in the confirmed string must be recognized'

# A slot that has never been sampled: firmware sends a plain status
# phrase, not a semicolon-joined decoded snapshot.
$notSampled = ConvertFrom-BQEvidence -Slot 'C02' -Match 'awaiting' -Snapshot 'no I2C read scheduled' -Evidence 'no I2C read scheduled'
Expect (-not $notSampled.IsDecoded) 'plain status text must not be treated as decoded'
Expect ($null -eq $notSampled.IinMa) 'undecoded evidence must leave typed fields null'
Expect ($notSampled.RawEvidence -eq 'no I2C read scheduled') 'undecoded RawEvidence must still be preserved verbatim'

# Empty evidence must not throw and must still preserve slot/match.
$empty = ConvertFrom-BQEvidence -Slot 'C03' -Match 'awaiting' -Snapshot 'no I2C read scheduled' -Evidence ''
Expect (-not $empty.IsDecoded) 'empty evidence must not be treated as decoded'
Expect ($empty.RawEvidence -eq '') 'empty evidence must round-trip as empty string'

# An unrecognized token must be preserved, not dropped or thrown on -
# firmware wording can change without this parser silently losing data.
$withUnknownToken = ConvertFrom-BQEvidence -Slot 'C04' -Match 'match' -Snapshot 'register snapshot' -Evidence 'HIZ off; IIN 100mA; mystery-flag; fault normal'
Expect ($withUnknownToken.HizEnabled -eq $false) 'known tokens must still parse alongside an unknown one'
Expect ($withUnknownToken.Unparsed.Count -eq 1) 'unrecognized token must be captured, not dropped'
Expect ($withUnknownToken.Unparsed[0] -eq 'mystery-flag') 'unrecognized token text must be preserved exactly'

# Format-CellDetails must not throw for either decoded or undecoded state.
$decodedText = Format-CellDetails $decoded
Expect ($decodedText -like '*ICHG: 2048mA*') 'decoded details text must include ICHG'
$notSampledText = Format-CellDetails $notSampled
Expect ($notSampledText -like '*no I2C read scheduled*') 'undecoded details text must surface the raw status'

Write-Host 'MCC BQ evidence parser tests passed.'

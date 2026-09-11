# Domain model for C01-C16 cell/BQ state.
#
# Deliberately NOT a PowerShell 5.1 class - $script:CellStates (defined in
# mcc-live-monitor.ps1) is a plain hashtable of slot name -> PSCustomObject
# built by ConvertFrom-BQEvidence below.
#
# ConvertFrom-BQEvidence is the ONLY function allowed to parse the
# firmware's flattened decoded-evidence string (the semicolon-joined
# "HIZ off; IIN 100mA; ..." text already produced upstream by firmware).
# Every other function reads the structured object this returns instead
# of touching the raw string again.
#
# Pure data/string functions only - no System.Windows.Forms dependency,
# so this file can be dot-sourced and tested without loading WinForms.

$script:BQKnownChargeStages = @('fast-charge', 'pre-charge', 'charge-done')

function ConvertFrom-BQEvidence {
    param(
        [Parameter(Mandatory)][string]$Slot,
        [Parameter(Mandatory)][string]$Match,
        [Parameter(Mandatory)][string]$Snapshot,
        [Parameter(Mandatory)][AllowEmptyString()][string]$Evidence
    )

    $cell = [PSCustomObject]@{
        Slot          = $Slot
        Match         = $Match
        Snapshot      = $Snapshot
        RawEvidence   = $Evidence
        IsDecoded     = $false
        HizEnabled    = $null
        IinMa         = $null
        ChargeEnabled = $null
        OtgEnabled    = $null
        IchgMa        = $null
        WatchdogS     = $null
        TimerEnabled  = $null
        ChargeStage   = $null
        DpmActive     = $null
        PowerGood     = $null
        VsysActive    = $null
        Fault         = $null
        PartNumber    = $null
        PartRevision  = $null
        TcaEvidence   = $null
        Unparsed      = @()
    }

    if ([string]::IsNullOrWhiteSpace($Evidence)) {
        return $cell
    }

    $decodedText = $Evidence
    if ($Evidence -match '^(?<decoded>.*); (?<tca>select=\d+ release70=\d+ release71=\d+)$') {
        $decodedText = $Matches['decoded']
        $cell.TcaEvidence = $Matches['tca']
    }

    $unparsed = New-Object System.Collections.Generic.List[string]
    foreach ($rawToken in ($decodedText -split '; ')) {
        $token = $rawToken.Trim()
        if (-not $token) { continue }
        switch -Regex ($token) {
            '^HIZ (on|off)$' { $cell.HizEnabled = ($Matches[1] -eq 'on'); $cell.IsDecoded = $true; break }
            '^IIN (\d+)mA$' { $cell.IinMa = [int]$Matches[1]; $cell.IsDecoded = $true; break }
            '^CHG (on|off)$' { $cell.ChargeEnabled = ($Matches[1] -eq 'on'); $cell.IsDecoded = $true; break }
            '^OTG (on|off)$' { $cell.OtgEnabled = ($Matches[1] -eq 'on'); $cell.IsDecoded = $true; break }
            '^ICHG (\d+)mA$' { $cell.IchgMa = [int]$Matches[1]; $cell.IsDecoded = $true; break }
            '^WD (\d+)s$' { $cell.WatchdogS = [int]$Matches[1]; $cell.IsDecoded = $true; break }
            '^timer (on|off)$' { $cell.TimerEnabled = ($Matches[1] -eq 'on'); $cell.IsDecoded = $true; break }
            '^DPM (yes|no)$' { $cell.DpmActive = ($Matches[1] -eq 'yes'); $cell.IsDecoded = $true; break }
            '^PG (yes|no)$' { $cell.PowerGood = ($Matches[1] -eq 'yes'); $cell.IsDecoded = $true; break }
            '^VSYS (yes|no)$' { $cell.VsysActive = ($Matches[1] -eq 'yes'); $cell.IsDecoded = $true; break }
            '^fault (.+)$' { $cell.Fault = $Matches[1]; $cell.IsDecoded = $true; break }
            '^part(\d+) rev(\d+)$' { $cell.PartNumber = [int]$Matches[1]; $cell.PartRevision = [int]$Matches[2]; $cell.IsDecoded = $true; break }
            default {
                if ($script:BQKnownChargeStages -contains $token) {
                    $cell.ChargeStage = $token
                    $cell.IsDecoded = $true
                } else {
                    $unparsed.Add($token)
                }
            }
        }
    }
    $cell.Unparsed = $unparsed.ToArray()
    return $cell
}

function Format-CellDetails($CellState) {
    if (-not $CellState) { return '' }
    if (-not $CellState.IsDecoded) {
        return "Selected slot: $($CellState.Slot)`r`n`r`n$($CellState.RawEvidence)"
    }
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("Selected slot: $($CellState.Slot)")
    $lines.Add('')
    $lines.Add("HIZ: $(if ($CellState.HizEnabled) { 'on' } else { 'off' })")
    $lines.Add("IIN: $($CellState.IinMa)mA")
    $lines.Add("CHG: $(if ($CellState.ChargeEnabled) { 'on' } else { 'off' })")
    $lines.Add("OTG: $(if ($CellState.OtgEnabled) { 'on' } else { 'off' })")
    $lines.Add("ICHG: $($CellState.IchgMa)mA")
    $lines.Add("WD: $($CellState.WatchdogS)s")
    $lines.Add("Timer: $(if ($CellState.TimerEnabled) { 'on' } else { 'off' })")
    $lines.Add("Charge stage: $($CellState.ChargeStage)")
    $lines.Add("DPM: $(if ($CellState.DpmActive) { 'yes' } else { 'no' })")
    $lines.Add("PG: $(if ($CellState.PowerGood) { 'yes' } else { 'no' })")
    $lines.Add("VSYS: $(if ($CellState.VsysActive) { 'yes' } else { 'no' })")
    $lines.Add("Fault: $($CellState.Fault)")
    $lines.Add("Part/Rev: $($CellState.PartNumber)/$($CellState.PartRevision)")
    if ($CellState.TcaEvidence) {
        $lines.Add('')
        $lines.Add('TCA Evidence:')
        $lines.Add($CellState.TcaEvidence)
    }
    if ($CellState.Unparsed.Count -gt 0) {
        $lines.Add('')
        $lines.Add("Unparsed: $($CellState.Unparsed -join '; ')")
    }
    return ($lines -join "`r`n")
}

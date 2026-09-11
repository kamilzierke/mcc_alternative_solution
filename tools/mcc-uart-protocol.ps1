function Split-MccUartBuffer {
    param([string]$Buffer)

    $parts = [System.Text.RegularExpressions.Regex]::Split($Buffer, "`r?`n")
    if ($Buffer.EndsWith("`n")) {
        return [PSCustomObject]@{ Lines = @($parts | Where-Object { $_ }); Remainder = '' }
    }
    if ($parts.Count -le 1) {
        return [PSCustomObject]@{ Lines = @(); Remainder = $Buffer }
    }
    return [PSCustomObject]@{ Lines = @($parts[0..($parts.Count - 2)] | Where-Object { $_ }); Remainder = $parts[-1] }
}
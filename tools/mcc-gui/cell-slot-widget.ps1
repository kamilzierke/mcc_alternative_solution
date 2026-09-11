# CellSlotWidget: a composite WinForms control representing one physical
# MCC slot (C01-C16) in the ChargerOverview strip.
#
# This file holds ONLY presentation: building the widget, updating its
# displayed text/colors, and toggling its selected state. It never reads
# serial/I2C data itself - callers (mcc-live-monitor.ps1) push state in
# through Update-CellSlotWidget and wire clicks through
# Register-CellSlotWidgetClick.

function Get-SlotAccentColor([string]$match) {
    switch ($match) {
        'match' { return $script:Theme.Success }
        'fault' { return $script:Theme.Error }
        'mismatch' { return $script:Theme.Error }
        'awaiting' { return $script:Theme.Warning }
        default { return $script:Theme.TextMuted }
    }
}

function New-CellSlotWidget([string]$SlotName) {
    $border = New-Object System.Windows.Forms.Panel
    $border.Dock = 'Fill'
    $border.Margin = New-Object System.Windows.Forms.Padding(3)
    $border.Padding = New-Object System.Windows.Forms.Padding(2)
    $border.BackColor = $script:Theme.Border
    $border.Tag = $SlotName
    $border.Cursor = [System.Windows.Forms.Cursors]::Hand

    $body = New-Object System.Windows.Forms.Panel
    $body.Dock = 'Fill'
    $body.BackColor = $script:Theme.PanelAlt
    $body.Tag = $SlotName
    $body.Cursor = [System.Windows.Forms.Cursors]::Hand
    $border.Controls.Add($body)

    $accentStrip = New-Object System.Windows.Forms.Panel
    $accentStrip.Dock = 'Bottom'
    $accentStrip.Height = 4
    $accentStrip.BackColor = $script:Theme.TextMuted
    $body.Controls.Add($accentStrip)

    $secondaryLabel = New-Object System.Windows.Forms.Label
    $secondaryLabel.Dock = 'Bottom'
    $secondaryLabel.Height = 18
    $secondaryLabel.TextAlign = 'MiddleCenter'
    $secondaryLabel.Font = New-Object System.Drawing.Font('Segoe UI', 8)
    $secondaryLabel.ForeColor = $script:Theme.TextMuted
    $secondaryLabel.Text = 'awaiting'
    $secondaryLabel.Tag = $SlotName
    $body.Controls.Add($secondaryLabel)

    $primaryLabel = New-Object System.Windows.Forms.Label
    $primaryLabel.Dock = 'Fill'
    $primaryLabel.TextAlign = 'MiddleCenter'
    $primaryLabel.Font = New-Object System.Drawing.Font('Segoe UI', 8)
    $primaryLabel.ForeColor = $script:Theme.TextMuted
    $primaryLabel.Text = 'not sampled'
    $primaryLabel.Tag = $SlotName
    $body.Controls.Add($primaryLabel)

    $idLabel = New-Object System.Windows.Forms.Label
    $idLabel.Dock = 'Top'
    $idLabel.Height = 22
    $idLabel.TextAlign = 'MiddleCenter'
    $idLabel.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 10)
    $idLabel.ForeColor = $script:Theme.Text
    $idLabel.Text = $SlotName
    $idLabel.Tag = $SlotName
    $body.Controls.Add($idLabel)

    $parts = [PSCustomObject]@{
        Border    = $border
        Body      = $body
        IdLabel   = $idLabel
        Primary   = $primaryLabel
        Secondary = $secondaryLabel
        Accent    = $accentStrip
    }
    $border | Add-Member -NotePropertyName Widgets -NotePropertyValue $parts

    return $border
}

function Update-CellSlotWidget($Widget, [string]$Observed, [string]$Match) {
    $accentColor = Get-SlotAccentColor $Match
    $Widget.Widgets.Primary.Text = $Observed
    $Widget.Widgets.Secondary.Text = $Match
    $Widget.Widgets.Secondary.ForeColor = $accentColor
    $Widget.Widgets.Accent.BackColor = $accentColor
}

function Set-CellSlotWidgetSelected($Widget, [bool]$Selected) {
    $Widget.BackColor = if ($Selected) { $script:Theme.Accent } else { $script:Theme.Border }
}

function Register-CellSlotWidgetClick($Widget, [scriptblock]$OnClick) {
    foreach ($control in @($Widget, $Widget.Widgets.Body, $Widget.Widgets.IdLabel, $Widget.Widgets.Primary, $Widget.Widgets.Secondary)) {
        $control.Add_Click($OnClick)
    }
}

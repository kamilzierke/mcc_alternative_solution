param(
    [string]$Port = 'COM7',
    [int]$Baud = 115200,
    [int]$ObservationSeconds = 0,
    [switch]$AutoConnect,
    [switch]$RequireMainBusProbe
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
. (Join-Path $PSScriptRoot 'mcc-uart-protocol.ps1')
. (Join-Path $PSScriptRoot 'mcc-gui\theme.ps1')
. (Join-Path $PSScriptRoot 'mcc-gui\models.ps1')
. (Join-Path $PSScriptRoot 'mcc-gui\cell-slot-widget.ps1')

Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class ClaudeCodeWindowFix
{
    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
}
'@

$ErrorActionPreference = 'Stop'
$script:serialPort = $null
$script:logWriter = $null
$script:observationWatch = $null
$script:componentFrames = 0
$script:slotFrames = 0
$script:invalidFrames = 0
$script:observedComponents = [System.Collections.Generic.HashSet[string]]::new()
$script:observedSlots = [System.Collections.Generic.HashSet[string]]::new()
$script:observedProbeTargets = [System.Collections.Generic.HashSet[string]]::new()
$script:receiveBuffer = ''
$script:logLineCap = 10000
$script:CellStates = @{}

$form = New-Object System.Windows.Forms.Form
$form.Text = 'MCC Pro Live Monitor'
$form.StartPosition = 'CenterScreen'
$form.MinimumSize = New-Object System.Drawing.Size(1280, 720)
$form.Size = New-Object System.Drawing.Size(1440, 900)
$form.Font = New-Object System.Drawing.Font('Segoe UI', 9)
$form.BackColor = $script:Theme.Window

$form.Add_Shown({
    $form.PerformLayout()
    [void][ClaudeCodeWindowFix]::ShowWindow($form.Handle, 5)
    $form.Refresh()
})

# --- Toolbar -----------------------------------------------------------
# TableLayoutPanel instead of FlowLayoutPanel: explicit columns, no
# implicit reflow, easier to keep aligned once a status indicator is
# added in a later phase.
$topPanel = New-Object System.Windows.Forms.TableLayoutPanel
$topPanel.Dock = 'Top'
$topPanel.Height = 46
$topPanel.BackColor = $script:Theme.Panel
$topPanel.Padding = New-Object System.Windows.Forms.Padding(10, 8, 10, 8)
$topPanel.RowCount = 1
$topPanel.ColumnCount = 8
1..7 | ForEach-Object { [void]$topPanel.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::AutoSize))) }
[void]$topPanel.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::Percent, 100)))

$portLabel = New-Object System.Windows.Forms.Label
$portLabel.Text = 'Port'
$portLabel.AutoSize = $true
$portLabel.ForeColor = $script:Theme.Text
$portLabel.Margin = '0,7,6,0'
$topPanel.Controls.Add($portLabel, 0, 0)

$portSelector = New-Object System.Windows.Forms.ComboBox
$portSelector.Width = 100
$portSelector.Margin = '0,3,0,0'
$portSelector.DropDownStyle = 'DropDownList'
$availablePorts = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
[void]$portSelector.Items.AddRange($availablePorts)
if ($portSelector.Items.Contains($Port)) { $portSelector.SelectedItem = $Port } elseif ($portSelector.Items.Count -gt 0) { $portSelector.SelectedIndex = 0 }
$topPanel.Controls.Add($portSelector, 1, 0)

$baudLabel = New-Object System.Windows.Forms.Label
$baudLabel.Text = 'Baud'
$baudLabel.AutoSize = $true
$baudLabel.ForeColor = $script:Theme.Text
$baudLabel.Margin = '14,7,6,0'
$topPanel.Controls.Add($baudLabel, 2, 0)

$baudSelector = New-Object System.Windows.Forms.ComboBox
$baudSelector.Width = 100
$baudSelector.Margin = '0,3,0,0'
$baudSelector.DropDownStyle = 'DropDownList'
[void]$baudSelector.Items.AddRange(@('115200', '74880', '9600'))
$baudSelector.SelectedItem = $Baud.ToString()
if ($baudSelector.SelectedIndex -lt 0) { $baudSelector.SelectedItem = '115200' }
$topPanel.Controls.Add($baudSelector, 3, 0)

$connectButton = New-Object System.Windows.Forms.Button
$connectButton.Text = 'Connect Read-Only'
$connectButton.AutoSize = $true
$connectButton.Margin = '14,2,0,0'
$topPanel.Controls.Add($connectButton, 4, 0)

$connectionLabel = New-Object System.Windows.Forms.Label
$connectionLabel.Text = 'Disconnected'
$connectionLabel.AutoSize = $true
$connectionLabel.ForeColor = [System.Drawing.Color]::Firebrick
$connectionLabel.Margin = '16,7,0,0'
$topPanel.Controls.Add($connectionLabel, 5, 0)

$testLabel = New-Object System.Windows.Forms.Label
$testLabel.Text = if ($ObservationSeconds -gt 0) { "Test pending: $ObservationSeconds s" } else { 'Live view: manual' }
$testLabel.AutoSize = $true
$testLabel.ForeColor = [System.Drawing.Color]::DarkSlateBlue
$testLabel.Margin = '18,7,0,0'
$topPanel.Controls.Add($testLabel, 6, 0)

# --- Main layout ---------------------------------------------------------
# mainLayout: row0 = rootSplit (fills remaining space), row1 = fixed-height
# SelectedCellPanel spanning the full width, per spec section 1/9.
$mainLayout = New-Object System.Windows.Forms.TableLayoutPanel
$mainLayout.Dock = 'Fill'
$mainLayout.BackColor = $script:Theme.Window
$mainLayout.ColumnCount = 1
$mainLayout.RowCount = 2
[void]$mainLayout.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::Percent, 100)))
[void]$mainLayout.RowStyles.Add((New-Object System.Windows.Forms.RowStyle([System.Windows.Forms.SizeType]::Percent, 100)))
[void]$mainLayout.RowStyles.Add((New-Object System.Windows.Forms.RowStyle([System.Windows.Forms.SizeType]::Absolute, 110)))
$form.Controls.Add($mainLayout)
$form.Controls.Add($topPanel)

# rootSplit: UpperArea (top) / LowerArea (bottom).
$rootSplit = New-Object System.Windows.Forms.SplitContainer
$rootSplit.Size = New-Object System.Drawing.Size(1200, 760)
$rootSplit.Dock = 'Fill'
$rootSplit.Orientation = 'Horizontal'
$rootSplit.BackColor = $script:Theme.Border
$rootSplit.Panel1MinSize = 260
$rootSplit.Panel2MinSize = 180
$rootSplit.SplitterDistance = 480
$mainLayout.Controls.Add($rootSplit, 0, 0)

# upperSplit: ChargerOverview (left, ~72%) / LogConsole (right, ~28%).
$upperSplit = New-Object System.Windows.Forms.SplitContainer
$upperSplit.Size = New-Object System.Drawing.Size(1200, 480)
$upperSplit.Dock = 'Fill'
$upperSplit.Orientation = 'Vertical'
$upperSplit.BackColor = $script:Theme.Border
$upperSplit.Panel1MinSize = 400
$upperSplit.Panel2MinSize = 220
$upperSplit.SplitterDistance = 860
$rootSplit.Panel1.Controls.Add($upperSplit)

# lowerSplit: ModuleDiagnostics (left, ~35%) / CellBQTable (right, ~65%).
$lowerSplit = New-Object System.Windows.Forms.SplitContainer
$lowerSplit.Size = New-Object System.Drawing.Size(1200, 260)
$lowerSplit.Dock = 'Fill'
$lowerSplit.Orientation = 'Vertical'
$lowerSplit.BackColor = $script:Theme.Border
$lowerSplit.Panel1MinSize = 280
$lowerSplit.Panel2MinSize = 320
$lowerSplit.SplitterDistance = 420
$rootSplit.Panel2.Controls.Add($lowerSplit)

# --- Module Diagnostics table (unchanged data path, re-parented only) ---
$statusGrid = New-Object System.Windows.Forms.DataGridView
$statusGrid.Dock = 'Fill'
$statusGrid.BackgroundColor = $script:Theme.Panel
$statusGrid.GridColor = $script:Theme.Border
$statusGrid.ReadOnly = $true
$statusGrid.AllowUserToAddRows = $false
$statusGrid.AllowUserToDeleteRows = $false
$statusGrid.AllowUserToResizeRows = $false
$statusGrid.RowHeadersVisible = $false
$statusGrid.ColumnHeadersVisible = $true
$statusGrid.ColumnHeadersHeightSizeMode = 'AutoSize'
$statusGrid.EnableHeadersVisualStyles = $false
$statusGrid.ColumnHeadersDefaultCellStyle.BackColor = $script:Theme.PanelAlt
$statusGrid.ColumnHeadersDefaultCellStyle.ForeColor = $script:Theme.Text
$statusGrid.ColumnHeadersDefaultCellStyle.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 9)
$statusGrid.DefaultCellStyle.BackColor = $script:Theme.Panel
$statusGrid.DefaultCellStyle.ForeColor = $script:Theme.Text
$statusGrid.DefaultCellStyle.SelectionBackColor = $script:Theme.Selection
$statusGrid.DefaultCellStyle.SelectionForeColor = $script:Theme.Text
$statusGrid.AutoSizeColumnsMode = 'Fill'
[void]$statusGrid.Columns.Add('Component', 'Component')
[void]$statusGrid.Columns.Add('Observed', 'Observed')
[void]$statusGrid.Columns.Add('Requested', 'Requested')
[void]$statusGrid.Columns.Add('Status', 'Status')
[void]$statusGrid.Columns.Add('Match', 'Match')
[void]$statusGrid.Columns.Add('Evidence', 'Latest evidence')
$statusGrid.Columns['Evidence'].FillWeight = 220
$lowerSplit.Panel1.Controls.Add($statusGrid)

# --- Cell/BQ table: manually-sized, resizable, horizontally scrollable
# columns clamped to [MinColumnWidth, MaxColumnWidth]; [Auto-fit] does a
# one-shot resize-to-content pass within the same clamp. ------------------
$script:SlotTableMinColumnWidth = 55
$script:SlotTableMaxColumnWidth = 320

$cellBqPanel = New-Object System.Windows.Forms.Panel
$cellBqPanel.Dock = 'Fill'
$cellBqPanel.BackColor = $script:Theme.Panel
$lowerSplit.Panel2.Controls.Add($cellBqPanel)

$cellBqToolbar = New-Object System.Windows.Forms.FlowLayoutPanel
$cellBqToolbar.Dock = 'Top'
$cellBqToolbar.Height = 32
$cellBqToolbar.BackColor = $script:Theme.Panel
$cellBqToolbar.Padding = New-Object System.Windows.Forms.Padding(4)
$cellBqPanel.Controls.Add($cellBqToolbar)

$autoFitButton = New-Object System.Windows.Forms.Button
$autoFitButton.Text = 'Auto-fit'
$autoFitButton.AutoSize = $true
$cellBqToolbar.Controls.Add($autoFitButton)

$slotGrid = New-Object System.Windows.Forms.DataGridView
$slotGrid.Dock = 'Fill'
$slotGrid.BackgroundColor = $script:Theme.Panel
$slotGrid.GridColor = $script:Theme.Border
$slotGrid.ReadOnly = $true
$slotGrid.AllowUserToAddRows = $false
$slotGrid.AllowUserToDeleteRows = $false
$slotGrid.AllowUserToResizeRows = $false
$slotGrid.AllowUserToResizeColumns = $true
$slotGrid.ScrollBars = 'Both'
$slotGrid.RowHeadersVisible = $false
$slotGrid.SelectionMode = 'FullRowSelect'
$slotGrid.ColumnHeadersVisible = $true
$slotGrid.ColumnHeadersHeightSizeMode = 'AutoSize'
$slotGrid.EnableHeadersVisualStyles = $false
$slotGrid.ColumnHeadersDefaultCellStyle.BackColor = $script:Theme.PanelAlt
$slotGrid.ColumnHeadersDefaultCellStyle.ForeColor = $script:Theme.Text
$slotGrid.ColumnHeadersDefaultCellStyle.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 9)
$slotGrid.DefaultCellStyle.BackColor = $script:Theme.Panel
$slotGrid.DefaultCellStyle.ForeColor = $script:Theme.Text
$slotGrid.DefaultCellStyle.SelectionBackColor = $script:Theme.Selection
$slotGrid.DefaultCellStyle.SelectionForeColor = $script:Theme.Text
$slotGrid.AutoSizeColumnsMode = 'None'
[void]$slotGrid.Columns.Add('Slot', 'Slot')
[void]$slotGrid.Columns.Add('Route', 'BQ route')
[void]$slotGrid.Columns.Add('Snapshot', 'Snapshot')
[void]$slotGrid.Columns.Add('Match', 'Match')
[void]$slotGrid.Columns.Add('Hiz', 'HIZ')
[void]$slotGrid.Columns.Add('Iin', 'IIN')
[void]$slotGrid.Columns.Add('Chg', 'CHG')
[void]$slotGrid.Columns.Add('Otg', 'OTG')
[void]$slotGrid.Columns.Add('Ichg', 'ICHG')
[void]$slotGrid.Columns.Add('Wd', 'WD')
[void]$slotGrid.Columns.Add('Timer', 'Timer')
[void]$slotGrid.Columns.Add('Stage', 'Stage')
[void]$slotGrid.Columns.Add('Dpm', 'DPM')
[void]$slotGrid.Columns.Add('Pg', 'PG')
[void]$slotGrid.Columns.Add('Vsys', 'VSYS')
[void]$slotGrid.Columns.Add('Fault', 'Fault')
[void]$slotGrid.Columns.Add('PartRev', 'Part/Rev')
[void]$slotGrid.Columns.Add('Notes', 'Notes')

$script:SlotTableDefaultColumnWidths = @{
    Slot = 55; Route = 130; Snapshot = 140; Match = 75
    Hiz = 55; Iin = 70; Chg = 55; Otg = 55; Ichg = 80; Wd = 60
    Timer = 60; Stage = 95; Dpm = 55; Pg = 55; Vsys = 60
    Fault = 90; PartRev = 75; Notes = 260
}
foreach ($columnName in $script:SlotTableDefaultColumnWidths.Keys) {
    $slotGrid.Columns[$columnName].MinimumWidth = $script:SlotTableMinColumnWidth
    $slotGrid.Columns[$columnName].Width = $script:SlotTableDefaultColumnWidths[$columnName]
}

$script:columnLayoutPath = Join-Path $PSScriptRoot '..\artifacts\live-monitor\column-layout.json'

function Save-ColumnLayout {
    try {
        $widths = @{}
        foreach ($column in $slotGrid.Columns) { $widths[$column.Name] = $column.Width }
        $directory = Split-Path $script:columnLayoutPath -Parent
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
        $widths | ConvertTo-Json | Out-File -FilePath $script:columnLayoutPath -Encoding utf8
    } catch {
        # Best-effort only; a failed save must not block closing the window.
    }
}

function Restore-ColumnLayout {
    if (-not (Test-Path $script:columnLayoutPath)) { return }
    try {
        $saved = Get-Content $script:columnLayoutPath -Raw | ConvertFrom-Json
        foreach ($property in $saved.PSObject.Properties) {
            if (-not $slotGrid.Columns.Contains($property.Name)) { continue }
            $width = [int]$property.Value
            if ($width -ge $script:SlotTableMinColumnWidth -and $width -le $script:SlotTableMaxColumnWidth) {
                $slotGrid.Columns[$property.Name].Width = $width
            }
        }
    } catch {
        # Corrupt or unreadable preferences file: keep the default widths.
    }
}

Restore-ColumnLayout
$cellBqPanel.Controls.Add($slotGrid)
$slotGrid.BringToFront()

$autoFitButton.Add_Click({
    $slotGrid.AutoResizeColumns([System.Windows.Forms.DataGridViewAutoSizeColumnsMode]::AllCells)
    foreach ($column in $slotGrid.Columns) {
        if ($column.Width -lt $script:SlotTableMinColumnWidth) { $column.Width = $script:SlotTableMinColumnWidth }
        elseif ($column.Width -gt $script:SlotTableMaxColumnWidth) { $column.Width = $script:SlotTableMaxColumnWidth }
    }
})

# --- ChargerOverview placeholder (today's 4x4 mirror; Phase 2 replaces
# this with the 8 + gap + 8 CellSlotWidget row) -------------------------
$mirrorPanel = New-Object System.Windows.Forms.Panel
$mirrorPanel.Dock = 'Fill'
$mirrorPanel.Padding = New-Object System.Windows.Forms.Padding(12)
$mirrorPanel.BackColor = $script:Theme.PanelAlt
$upperSplit.Panel1.Controls.Add($mirrorPanel)

$mirrorTitle = New-Object System.Windows.Forms.Label
$mirrorTitle.Text = 'Charger Overview - C01-C16 (click a slot)'
$mirrorTitle.ForeColor = $script:Theme.Text
$mirrorTitle.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 11)
$mirrorTitle.Dock = 'Top'
$mirrorTitle.Height = 30
$mirrorPanel.Controls.Add($mirrorTitle)

# 8 slots, physical gap, 8 slots - not a 4x4 grid. Column 8 is the gap.
$mirrorGrid = New-Object System.Windows.Forms.TableLayoutPanel
$mirrorGrid.Dock = 'Fill'
$mirrorGrid.ColumnCount = 17
$mirrorGrid.RowCount = 1
for ($index = 0; $index -lt 8; $index++) {
    [void]$mirrorGrid.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::Percent, 1)))
}
[void]$mirrorGrid.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::Absolute, 28)))
for ($index = 0; $index -lt 8; $index++) {
    [void]$mirrorGrid.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::Percent, 1)))
}
[void]$mirrorGrid.RowStyles.Add((New-Object System.Windows.Forms.RowStyle([System.Windows.Forms.SizeType]::Percent, 100)))
$mirrorPanel.Controls.Add($mirrorGrid)

$gapPanel = New-Object System.Windows.Forms.Panel
$gapPanel.Dock = 'Fill'
$gapPanel.BackColor = $script:Theme.PanelAlt
$mirrorGrid.Controls.Add($gapPanel, 8, 0)

$script:selectedSlotName = $null

function Set-SelectedSlot([string]$slotName) {
    if ($script:selectedSlotName -and $script:selectedSlotName -ne $slotName -and $slotTiles.ContainsKey($script:selectedSlotName)) {
        Set-CellSlotWidgetSelected $slotTiles[$script:selectedSlotName] $false
    }
    $script:selectedSlotName = $slotName
    if ($slotTiles.ContainsKey($slotName)) {
        Set-CellSlotWidgetSelected $slotTiles[$slotName] $true
    }
    if ($script:CellStates.ContainsKey($slotName)) {
        $selectedSlotSummary.Text = Format-CellDetails $script:CellStates[$slotName]
    }
}

function Select-SlotRow([string]$slotName) {
    Set-SelectedSlot $slotName
    foreach ($row in $slotGrid.Rows) {
        if ($row.Cells['Slot'].Value -eq $slotName) {
            $slotGrid.ClearSelection()
            $row.Selected = $true
            $slotGrid.CurrentCell = $row.Cells['Slot']
            $slotGrid.FirstDisplayedScrollingRowIndex = [Math]::Max(0, $row.Index - 3)
            break
        }
    }
}

$slotTiles = @{}
for ($slotNumber = 1; $slotNumber -le 16; $slotNumber++) {
    $slotName = 'C{0:d2}' -f $slotNumber
    $columnIndex = if ($slotNumber -le 8) { $slotNumber - 1 } else { $slotNumber }
    $widget = New-CellSlotWidget $slotName
    Register-CellSlotWidgetClick $widget { param($senderObj, $eventArgs) Select-SlotRow $senderObj.Tag }
    $slotTiles[$slotName] = $widget
    $mirrorGrid.Controls.Add($widget, $columnIndex, 0)
}

# --- LogConsole (raw UART view; Events/Raw toggle arrives in Phase 5) ---
$logConsolePanel = New-Object System.Windows.Forms.Panel
$logConsolePanel.Dock = 'Fill'
$logConsolePanel.BackColor = $script:Theme.Window
$upperSplit.Panel2.Controls.Add($logConsolePanel)

# Same recipe as $cellBqToolbar below (proven to render correctly):
# FlowLayoutPanel, Dock=Top, fixed Height, panel-level Padding instead of
# per-control Margin.
$logHeaderPanel = New-Object System.Windows.Forms.FlowLayoutPanel
$logHeaderPanel.Dock = 'Top'
$logHeaderPanel.Height = 32
$logHeaderPanel.BackColor = $script:Theme.Window
$logHeaderPanel.Padding = New-Object System.Windows.Forms.Padding(4)
$logConsolePanel.Controls.Add($logHeaderPanel)

$logTitleLabel = New-Object System.Windows.Forms.Label
$logTitleLabel.Text = 'Live Log'
$logTitleLabel.AutoSize = $true
$logTitleLabel.ForeColor = $script:Theme.Text
$logHeaderPanel.Controls.Add($logTitleLabel)

$eventsViewRadio = New-Object System.Windows.Forms.RadioButton
$eventsViewRadio.Text = 'Events'
$eventsViewRadio.AutoSize = $true
$eventsViewRadio.Checked = $true
$eventsViewRadio.ForeColor = $script:Theme.Text
$logHeaderPanel.Controls.Add($eventsViewRadio)

$rawViewRadio = New-Object System.Windows.Forms.RadioButton
$rawViewRadio.Text = 'Raw UART'
$rawViewRadio.AutoSize = $true
$rawViewRadio.ForeColor = $script:Theme.Text
$logHeaderPanel.Controls.Add($rawViewRadio)

$autoScrollCheckbox = New-Object System.Windows.Forms.CheckBox
$autoScrollCheckbox.Text = 'Auto-scroll'
$autoScrollCheckbox.AutoSize = $true
$autoScrollCheckbox.Checked = $true
$autoScrollCheckbox.ForeColor = $script:Theme.Text
$logHeaderPanel.Controls.Add($autoScrollCheckbox)

$clearLogButton = New-Object System.Windows.Forms.Button
$clearLogButton.Text = 'Clear'
$clearLogButton.AutoSize = $true
$logHeaderPanel.Controls.Add($clearLogButton)

# A single Fill host below the header, so the header has exactly one
# Fill sibling (matching the proven working pattern elsewhere in this
# file) - the two toggleable log views live inside this host instead.
$logContentHost = New-Object System.Windows.Forms.Panel
$logContentHost.Dock = 'Fill'
$logContentHost.BackColor = $script:Theme.Window
$logConsolePanel.Controls.Add($logContentHost)

$eventsLogBox = New-Object System.Windows.Forms.RichTextBox
$eventsLogBox.Dock = 'Fill'
$eventsLogBox.ReadOnly = $true
$eventsLogBox.BackColor = $script:Theme.Window
$eventsLogBox.ForeColor = $script:Theme.Text
$eventsLogBox.BorderStyle = 'None'
$eventsLogBox.Font = New-Object System.Drawing.Font('Cascadia Mono', 9)
$logContentHost.Controls.Add($eventsLogBox)

$logBox = New-Object System.Windows.Forms.RichTextBox
$logBox.Dock = 'Fill'
$logBox.ReadOnly = $true
$logBox.BackColor = $script:Theme.Window
$logBox.ForeColor = $script:Theme.Text
$logBox.BorderStyle = 'None'
$logBox.Font = New-Object System.Drawing.Font('Cascadia Mono', 9)
$logBox.Visible = $false
$logContentHost.Controls.Add($logBox)

$eventsViewRadio.Add_CheckedChanged({
    if ($eventsViewRadio.Checked) {
        $eventsLogBox.Visible = $true
        $logBox.Visible = $false
    }
})
$rawViewRadio.Add_CheckedChanged({
    if ($rawViewRadio.Checked) {
        $logBox.Visible = $true
        $eventsLogBox.Visible = $false
    }
})
$clearLogButton.Add_Click({
    $eventsLogBox.Clear()
    $logBox.Clear()
})

# --- SelectedCellPanel: full-width strip under both lower columns ------
$selectedCellPanel = New-Object System.Windows.Forms.GroupBox
$selectedCellPanel.Text = 'Selected slot - BQ decoded'
$selectedCellPanel.Dock = 'Fill'
$selectedCellPanel.BackColor = $script:Theme.Panel
$selectedCellPanel.ForeColor = $script:Theme.Text
$mainLayout.Controls.Add($selectedCellPanel, 0, 1)

$selectedSlotSummary = New-Object System.Windows.Forms.TextBox
$selectedSlotSummary.Multiline = $true
$selectedSlotSummary.ReadOnly = $true
$selectedSlotSummary.ScrollBars = 'Vertical'
$selectedSlotSummary.Dock = 'Fill'
$selectedSlotSummary.BorderStyle = 'None'
$selectedSlotSummary.BackColor = $script:Theme.PanelAlt
$selectedSlotSummary.ForeColor = $script:Theme.Text
$selectedSlotSummary.Font = New-Object System.Drawing.Font('Cascadia Mono', 9)
$selectedSlotSummary.Text = "Selected slot: awaiting BQ snapshot`r`nDecoded BQ values will appear here."
$selectedCellPanel.Controls.Add($selectedSlotSummary)

function Get-StateColor([string]$match) {
    switch ($match) {
        'match' { return [System.Drawing.Color]::FromArgb(0x16, 0x33, 0x26) }
        'awaiting' { return [System.Drawing.Color]::FromArgb(0x3A, 0x33, 0x18) }
        default { return [System.Drawing.Color]::FromArgb(0x3A, 0x1C, 0x1E) }
    }
}

function Set-ComponentStatus([string]$name, [string]$observed, [string]$requested, [string]$status, [string]$match, [string]$evidence) {
    $row = $null
    foreach ($candidate in $statusGrid.Rows) {
        if ($candidate.Cells['Component'].Value -eq $name) { $row = $candidate; break }
    }
    if ($null -eq $row) { $row = $statusGrid.Rows[$statusGrid.Rows.Add()] }
    $row.Cells['Component'].Value = $name
    $row.Cells['Observed'].Value = $observed
    $row.Cells['Requested'].Value = $requested
    $row.Cells['Status'].Value = $status
    $row.Cells['Match'].Value = $match
    $row.Cells['Evidence'].Value = $evidence
    $row.DefaultCellStyle.BackColor = Get-StateColor $match
}

function Set-SlotStatus([string]$slot, [string]$observed, [string]$requested, [string]$status, [string]$match, [string]$evidence) {
    $row = $null
    foreach ($candidate in $slotGrid.Rows) {
        if ($candidate.Cells['Slot'].Value -eq $slot) { $row = $candidate; break }
    }
    if ($null -eq $row) { $row = $slotGrid.Rows[$slotGrid.Rows.Add()] }
    $slotNumber = [int]$slot.Substring(1)
    $route = if ($slotNumber -le 8) { 'TCA 0x70 / ch {0}' -f ($slotNumber - 1) } else { 'TCA 0x71 / ch {0}' -f ($slotNumber - 9) }

    $cellState = ConvertFrom-BQEvidence -Slot $slot -Match $match -Snapshot $status -Evidence $evidence
    $script:CellStates[$slot] = $cellState

    $row.Cells['Slot'].Value = $slot
    $row.Cells['Route'].Value = $route
    $row.Cells['Snapshot'].Value = $status
    $row.Cells['Match'].Value = $match
    $row.Cells['Hiz'].Value = if ($null -ne $cellState.HizEnabled) { if ($cellState.HizEnabled) { 'on' } else { 'off' } } else { '' }
    $row.Cells['Iin'].Value = if ($null -ne $cellState.IinMa) { "$($cellState.IinMa)mA" } else { '' }
    $row.Cells['Chg'].Value = if ($null -ne $cellState.ChargeEnabled) { if ($cellState.ChargeEnabled) { 'on' } else { 'off' } } else { '' }
    $row.Cells['Otg'].Value = if ($null -ne $cellState.OtgEnabled) { if ($cellState.OtgEnabled) { 'on' } else { 'off' } } else { '' }
    $row.Cells['Ichg'].Value = if ($null -ne $cellState.IchgMa) { "$($cellState.IchgMa)mA" } else { '' }
    $row.Cells['Wd'].Value = if ($null -ne $cellState.WatchdogS) { "$($cellState.WatchdogS)s" } else { '' }
    $row.Cells['Timer'].Value = if ($null -ne $cellState.TimerEnabled) { if ($cellState.TimerEnabled) { 'on' } else { 'off' } } else { '' }
    $row.Cells['Stage'].Value = $cellState.ChargeStage
    $row.Cells['Dpm'].Value = if ($null -ne $cellState.DpmActive) { if ($cellState.DpmActive) { 'yes' } else { 'no' } } else { '' }
    $row.Cells['Pg'].Value = if ($null -ne $cellState.PowerGood) { if ($cellState.PowerGood) { 'yes' } else { 'no' } } else { '' }
    $row.Cells['Vsys'].Value = if ($null -ne $cellState.VsysActive) { if ($cellState.VsysActive) { 'yes' } else { 'no' } } else { '' }
    $row.Cells['Fault'].Value = $cellState.Fault
    $row.Cells['PartRev'].Value = if ($null -ne $cellState.PartNumber) { "$($cellState.PartNumber)/$($cellState.PartRevision)" } else { '' }
    $row.Cells['Notes'].Value = if ($cellState.IsDecoded) { $cellState.TcaEvidence } else { $cellState.RawEvidence }
    $row.DefaultCellStyle.BackColor = Get-StateColor $match

    if ($cellState.IsDecoded) {
        Set-ComponentStatus "BQ24195 $slot decoded" $observed $requested $status $match $cellState.RawEvidence
    }

    if ($script:selectedSlotName -eq $slot) {
        $selectedSlotSummary.Text = Format-CellDetails $cellState
    }

    Update-CellSlotWidget $slotTiles[$slot] $observed $match
}

$slotGrid.Add_SelectionChanged({
    if ($slotGrid.SelectedRows.Count -eq 1) {
        Set-SelectedSlot $slotGrid.SelectedRows[0].Cells['Slot'].Value
    }
})

function Limit-RichTextBoxLines([System.Windows.Forms.RichTextBox]$TextBox) {
    if ($TextBox.Lines.Count -gt $script:logLineCap) {
        $overflow = $TextBox.Lines.Count - $script:logLineCap
        $lines = $TextBox.Lines
        $TextBox.Lines = $lines[$overflow..($lines.Length - 1)]
    }
}

function Get-LogLevelForMatch([string]$match) {
    switch ($match) {
        'match' { return 'OK' }
        'awaiting' { return 'INFO' }
        'mismatch' { return 'WARN' }
        'fault' { return 'ERROR' }
        default { return 'INFO' }
    }
}

function Get-LogLevelColor([string]$level) {
    switch ($level) {
        'DEBUG' { return $script:Theme.TextMuted }
        'INFO' { return $script:Theme.Accent }
        'OK' { return $script:Theme.Success }
        'WARN' { return $script:Theme.Warning }
        'ERROR' { return $script:Theme.Error }
        default { return $script:Theme.Text }
    }
}

function Add-EventLine([string]$message, [string]$level = 'INFO') {
    $timestamp = Get-Date -Format 'HH:mm:ss'
    $eventsLogBox.SelectionStart = $eventsLogBox.TextLength
    $eventsLogBox.SelectionLength = 0
    $eventsLogBox.SelectionColor = Get-LogLevelColor $level
    $eventsLogBox.AppendText("[$timestamp] $level`t$message" + [Environment]::NewLine)
    Limit-RichTextBoxLines $eventsLogBox
    if ($autoScrollCheckbox.Checked) {
        $eventsLogBox.SelectionStart = $eventsLogBox.TextLength
        $eventsLogBox.ScrollToCaret()
    }
}

function Add-LogLine([string]$line) {
    $timestamped = "$(Get-Date -Format 'HH:mm:ss.fff')  $line"
    $logBox.AppendText($timestamped + [Environment]::NewLine)
    Limit-RichTextBoxLines $logBox
    if ($autoScrollCheckbox.Checked) {
        $logBox.SelectionStart = $logBox.TextLength
        $logBox.ScrollToCaret()
    }
    if ($script:logWriter) { $script:logWriter.WriteLine($timestamped); $script:logWriter.Flush() }
}

function Disconnect-Monitor {
    $timer.Stop()
    if ($script:serialPort -and $script:serialPort.IsOpen) {
        $script:serialPort.Close()
        $script:serialPort.Dispose()
    }
    $script:serialPort = $null
    if ($script:logWriter) { $script:logWriter.Dispose(); $script:logWriter = $null }
    $connectButton.Text = 'Connect Read-Only'
    $connectionLabel.Text = 'Disconnected'
    $connectionLabel.ForeColor = [System.Drawing.Color]::Firebrick
}

function Complete-Observation {
    $script:observationWatch.Stop()
    $expectedComponents = @('firmware', 'i2c', 'mux-select', 'outputs')
    $expectedProbeTargets = @('PCF8574 0x27', 'SSD1306 0x3C', 'TCA9548A 0x70', 'TCA9548A 0x71')
    $missingComponents = @($expectedComponents | Where-Object { -not $script:observedComponents.Contains($_) })
    $missingProbeTargets = @($expectedProbeTargets | Where-Object { -not $script:observedProbeTargets.Contains($_) })
    $passed = $script:invalidFrames -eq 0 -and $script:observedSlots.Count -eq 16 -and $missingComponents.Count -eq 0 -and (!$RequireMainBusProbe -or $missingProbeTargets.Count -eq 0)
    $result = if ($passed) { 'PASS' } else { 'FAIL' }
    $testLabel.Text = "${result}: components=$($script:observedComponents.Count)/4 slots=$($script:observedSlots.Count)/16 probe=$($script:observedProbeTargets.Count)/4 invalid=$($script:invalidFrames)"
    $testLabel.ForeColor = if ($passed) { [System.Drawing.Color]::ForestGreen } else { [System.Drawing.Color]::Firebrick }
    Add-LogLine "Observation $result after $ObservationSeconds s: component frames=$($script:componentFrames), slot frames=$($script:slotFrames), unique slots=$($script:observedSlots.Count), invalid=$($script:invalidFrames)."
    Disconnect-Monitor
    $script:observationWatch = $null
}

function Parse-MccLine([string]$line) {
    $fields = $line -split '\|', 8
    if ($fields.Count -lt 2 -or $fields[0] -ne 'MCC') {
        Set-ComponentStatus 'protocol' 'unrecognized' 'MCC frames' 'unrecognized UART text' 'awaiting' 'The line remains raw evidence in the Live Log panel.'
        Add-EventLine 'Unrecognized UART text (see Raw UART view)' 'WARN'
        return
    }
    switch ($fields[1]) {
        'HELLO' {
            Set-ComponentStatus 'firmware' 'read-only' 'read-only' 'banner received' 'match' (($fields | Select-Object -Skip 2) -join ' ')
            Add-EventLine 'Firmware banner received' 'OK'
        }
        'STATUS' {
            if ($fields.Count -ge 8) {
                $script:componentFrames++
                [void]$script:observedComponents.Add($fields[2])
                if ($fields[4] -eq 'address-only probe') { [void]$script:observedProbeTargets.Add($fields[2]) }
                Set-ComponentStatus $fields[2] $fields[3] $fields[4] $fields[5] $fields[6] $fields[7]
                Add-EventLine "$($fields[2]): $($fields[5]) ($($fields[6]))" (Get-LogLevelForMatch $fields[6])
            }
            elseif ($fields.Count -ge 5) {
                Set-ComponentStatus $fields[2] $fields[3] 'unknown' $fields[4] 'awaiting' 'Legacy status frame without comparison fields.'
                Add-EventLine "$($fields[2]): $($fields[4])" 'INFO'
            }
        }
        'SLOT' {
            if ($fields.Count -ge 8) {
                $script:slotFrames++
                [void]$script:observedSlots.Add($fields[2])
                Set-SlotStatus $fields[2] $fields[3] $fields[4] $fields[5] $fields[6] $fields[7]
                Add-EventLine "$($fields[2]): $($fields[5]) ($($fields[6]))" (Get-LogLevelForMatch $fields[6])
            }
        }
        'EVENT' {
            if ($fields.Count -ge 5) {
                Set-ComponentStatus $fields[3] $fields[2].ToLowerInvariant() 'ready' $fields[4] 'awaiting' $fields[4]
                Add-EventLine "$($fields[3]): $($fields[4])" 'INFO'
            }
        }
    }
}

$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 100
$timer.Add_Tick({
    if ($script:serialPort -and $script:serialPort.IsOpen) {
        $data = $script:serialPort.ReadExisting()
        $frames = Split-MccUartBuffer -Buffer ($script:receiveBuffer + $data)
        $script:receiveBuffer = $frames.Remainder
        foreach ($line in $frames.Lines) {
            if ($line) { Add-LogLine $line; Parse-MccLine $line }
        }
        if ($script:observationWatch) {
            $elapsedSeconds = [math]::Floor($script:observationWatch.Elapsed.TotalSeconds)
            if ($elapsedSeconds -ge $ObservationSeconds) { Complete-Observation }
            else { $testLabel.Text = "Testing: $elapsedSeconds/$ObservationSeconds s, components=$($script:observedComponents.Count)/4, slots=$($script:observedSlots.Count)/16" }
        }
    }
})

$connectButton.Add_Click({
    if ($script:serialPort -and $script:serialPort.IsOpen) {
        Disconnect-Monitor
        return
    }
    try {
        $directory = Join-Path $PSScriptRoot '..\artifacts\live-monitor'
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
        $logPath = Join-Path $directory ("uart-{0}-{1}.log" -f $portSelector.SelectedItem, (Get-Date -Format 'yyyyMMdd-HHmmss'))
        $script:logWriter = [System.IO.StreamWriter]::new($logPath, $false, [System.Text.UTF8Encoding]::new($false))
        $script:serialPort = [System.IO.Ports.SerialPort]::new($portSelector.SelectedItem, [int]$baudSelector.SelectedItem)
        $script:serialPort.DtrEnable = $false
        $script:serialPort.RtsEnable = $false
        $script:serialPort.Handshake = [System.IO.Ports.Handshake]::None
        $script:serialPort.Open()
        Set-ComponentStatus 'protocol' 'connected' 'MCC frames' 'waiting for frames' 'awaiting' 'DTR and RTS are disabled; the monitor does not send UART data.'
        $connectButton.Text = 'Disconnect'
        $connectionLabel.Text = "Connected read-only: $($portSelector.SelectedItem)"
        $connectionLabel.ForeColor = [System.Drawing.Color]::ForestGreen
        Add-LogLine "Monitor connected with DTR/RTS disabled; log stored locally in artifacts."
        if ($ObservationSeconds -gt 0) {
            $script:componentFrames = 0
            $script:slotFrames = 0
            $script:invalidFrames = 0
            $script:observedComponents.Clear()
            $script:observedSlots.Clear()
            $script:observedProbeTargets.Clear()
            $script:receiveBuffer = ''
            $script:observationWatch = [System.Diagnostics.Stopwatch]::StartNew()
        }
        $timer.Start()
    } catch {
        if ($script:logWriter) { $script:logWriter.Dispose(); $script:logWriter = $null }
        $connectionLabel.Text = "Connection error: $($_.Exception.Message)"
        $connectionLabel.ForeColor = [System.Drawing.Color]::Firebrick
    }
})

$form.Add_FormClosing({
    Disconnect-Monitor
    Save-ColumnLayout
})

Set-ComponentStatus 'firmware' 'awaiting' 'read-only' 'waiting for status' 'awaiting' 'Connect to receive firmware status.'
Set-ComponentStatus 'i2c' 'not sampled' 'initialized' 'no probe scheduled' 'awaiting' 'Current firmware does not probe the I2C bus.'
Set-ComponentStatus 'TCA9548A 0x70' 'not sampled' 'not touched' 'no probe scheduled' 'awaiting' 'C01-C08 BQ switch remains untouched.'
Set-ComponentStatus 'TCA9548A 0x71' 'not sampled' 'not touched' 'no probe scheduled' 'awaiting' 'C09-C16 BQ switch remains untouched.'
Set-ComponentStatus 'PCF8574 0x27' 'not sampled' 'safe idle' 'no write scheduled' 'awaiting' 'Mux enable expander is untouched.'
Set-ComponentStatus 'U10 TC1047 mux' 'not sampled' 'disabled' 'no read scheduled' 'awaiting' 'Internal temperature mux remains disabled.'
Set-ComponentStatus 'U10E temperature mux' 'not sampled' 'disabled' 'no read scheduled' 'awaiting' 'External temperature mux remains disabled.'
Set-ComponentStatus 'U34 shunt mux' 'not sampled' 'disabled' 'no read scheduled' 'awaiting' 'Internal INA shunt mux remains disabled.'
Set-ComponentStatus 'U3 external INA mux' 'not sampled' 'disabled' 'no read scheduled' 'awaiting' 'External INA mux remains disabled.'
Set-ComponentStatus 'U2 internal INA mux' 'not sampled' 'disabled' 'no read scheduled' 'awaiting' 'Internal INA mux remains disabled.'
Set-ComponentStatus 'INA219 0x41' 'not sampled' 'read-only' 'no read scheduled' 'awaiting' 'External measurement path is untouched.'
Set-ComponentStatus 'INA219 0x4F' 'not sampled' 'read-only' 'no read scheduled' 'awaiting' 'Internal measurement path/address conflict remains unresolved.'
Set-ComponentStatus 'BQ24195 C01-C16' 'not sampled' 'read-only' 'no read scheduled' 'awaiting' 'No charger register reads are scheduled.'
Set-ComponentStatus 'SSD1306 0x3C' 'not sampled' 'not touched' 'no probe scheduled' 'awaiting' 'OLED has not been queried.'
Set-ComponentStatus 'PCA9685 0x4F' 'unverified' 'disabled' 'no access scheduled' 'awaiting' 'Address conflict remains unresolved.'
Set-ComponentStatus 'outputs' 'awaiting' 'read-only' 'waiting for status' 'awaiting' 'Output commands are unavailable in this firmware.'
for ($slotNumber = 1; $slotNumber -le 16; $slotNumber++) {
    Set-SlotStatus ('C{0:d2}' -f $slotNumber) 'not sampled' 'read-only' 'no I2C read scheduled' 'awaiting' 'Awaiting a future approved read-only I2C phase.'
}
$form.Add_Shown({ if ($AutoConnect) { $connectButton.PerformClick() } })
[void]$form.ShowDialog()

param(
    [string]$Port = 'COM7',
    [int]$Baud = 115200,
    [int]$ObservationSeconds = 0,
    [switch]$AutoConnect
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
. (Join-Path $PSScriptRoot 'mcc-uart-protocol.ps1')

$ErrorActionPreference = 'Stop'
$script:serialPort = $null
$script:logWriter = $null
$script:observationWatch = $null
$script:componentFrames = 0
$script:slotFrames = 0
$script:invalidFrames = 0
$script:observedComponents = [System.Collections.Generic.HashSet[string]]::new()
$script:observedSlots = [System.Collections.Generic.HashSet[string]]::new()
$script:receiveBuffer = ''

$form = New-Object System.Windows.Forms.Form
$form.Text = 'MCC Pro Live Monitor'
$form.StartPosition = 'CenterScreen'
$form.MinimumSize = New-Object System.Drawing.Size(1180, 760)
$form.Size = New-Object System.Drawing.Size(1440, 900)
$form.Font = New-Object System.Drawing.Font('Segoe UI', 9)

$topPanel = New-Object System.Windows.Forms.FlowLayoutPanel
$topPanel.Dock = 'Top'
$topPanel.Height = 46
$topPanel.Padding = New-Object System.Windows.Forms.Padding(10, 8, 10, 8)
$topPanel.WrapContents = $false
$form.Controls.Add($topPanel)

$portLabel = New-Object System.Windows.Forms.Label
$portLabel.Text = 'Port'
$portLabel.AutoSize = $true
$portLabel.Margin = '0,7,6,0'
$topPanel.Controls.Add($portLabel)
$portSelector = New-Object System.Windows.Forms.ComboBox
$portSelector.Width = 100
$portSelector.DropDownStyle = 'DropDownList'
$availablePorts = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
[void]$portSelector.Items.AddRange($availablePorts)
if ($portSelector.Items.Contains($Port)) { $portSelector.SelectedItem = $Port } elseif ($portSelector.Items.Count -gt 0) { $portSelector.SelectedIndex = 0 }
$topPanel.Controls.Add($portSelector)

$baudLabel = New-Object System.Windows.Forms.Label
$baudLabel.Text = 'Baud'
$baudLabel.AutoSize = $true
$baudLabel.Margin = '14,7,6,0'
$topPanel.Controls.Add($baudLabel)
$baudSelector = New-Object System.Windows.Forms.ComboBox
$baudSelector.Width = 100
$baudSelector.DropDownStyle = 'DropDownList'
[void]$baudSelector.Items.AddRange(@('115200', '74880', '9600'))
$baudSelector.SelectedItem = $Baud.ToString()
if ($baudSelector.SelectedIndex -lt 0) { $baudSelector.SelectedItem = '115200' }
$topPanel.Controls.Add($baudSelector)

$connectButton = New-Object System.Windows.Forms.Button
$connectButton.Text = 'Connect Read-Only'
$connectButton.AutoSize = $true
$connectButton.Margin = '14,2,0,0'
$topPanel.Controls.Add($connectButton)

$connectionLabel = New-Object System.Windows.Forms.Label
$connectionLabel.Text = 'Disconnected'
$connectionLabel.AutoSize = $true
$connectionLabel.ForeColor = [System.Drawing.Color]::Firebrick
$connectionLabel.Margin = '16,7,0,0'
$topPanel.Controls.Add($connectionLabel)

$testLabel = New-Object System.Windows.Forms.Label
$testLabel.Text = if ($ObservationSeconds -gt 0) { "Test pending: $ObservationSeconds s" } else { 'Live view: manual' }
$testLabel.AutoSize = $true
$testLabel.ForeColor = [System.Drawing.Color]::DarkSlateBlue
$testLabel.Margin = '18,7,0,0'
$topPanel.Controls.Add($testLabel)

$viewTabs = New-Object System.Windows.Forms.TabControl
$viewTabs.Dock = 'Fill'
$form.Controls.Add($viewTabs)

$overviewTab = New-Object System.Windows.Forms.TabPage
$overviewTab.Text = 'Live Overview'
$viewTabs.TabPages.Add($overviewTab)
$slotsTab = New-Object System.Windows.Forms.TabPage
$slotsTab.Text = 'Slots C01-C16'
$viewTabs.TabPages.Add($slotsTab)
$logTab = New-Object System.Windows.Forms.TabPage
$logTab.Text = 'UART Log'
$viewTabs.TabPages.Add($logTab)

$overviewSplit = New-Object System.Windows.Forms.SplitContainer
$overviewSplit.Dock = 'Fill'
$overviewSplit.Orientation = 'Vertical'
$overviewSplit.SplitterDistance = 900
$overviewTab.Controls.Add($overviewSplit)

$statusGrid = New-Object System.Windows.Forms.DataGridView
$statusGrid.Dock = 'Fill'
$statusGrid.ReadOnly = $true
$statusGrid.AllowUserToAddRows = $false
$statusGrid.AllowUserToDeleteRows = $false
$statusGrid.AllowUserToResizeRows = $false
$statusGrid.RowHeadersVisible = $false
$statusGrid.AutoSizeColumnsMode = 'Fill'
[void]$statusGrid.Columns.Add('Component', 'Component')
[void]$statusGrid.Columns.Add('Observed', 'Observed')
[void]$statusGrid.Columns.Add('Requested', 'Requested')
[void]$statusGrid.Columns.Add('Status', 'Status')
[void]$statusGrid.Columns.Add('Match', 'Match')
[void]$statusGrid.Columns.Add('Evidence', 'Latest evidence')
$statusGrid.Columns['Evidence'].FillWeight = 220
$overviewSplit.Panel1.Controls.Add($statusGrid)

$mirrorPanel = New-Object System.Windows.Forms.Panel
$mirrorPanel.Dock = 'Fill'
$mirrorPanel.Padding = New-Object System.Windows.Forms.Padding(12)
$mirrorPanel.BackColor = [System.Drawing.Color]::FromArgb(35, 39, 46)
$overviewSplit.Panel2.Controls.Add($mirrorPanel)

$mirrorTitle = New-Object System.Windows.Forms.Label
$mirrorTitle.Text = 'MCC slot mirror'
$mirrorTitle.ForeColor = [System.Drawing.Color]::WhiteSmoke
$mirrorTitle.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 11)
$mirrorTitle.Dock = 'Top'
$mirrorTitle.Height = 30
$mirrorPanel.Controls.Add($mirrorTitle)

$mirrorGrid = New-Object System.Windows.Forms.TableLayoutPanel
$mirrorGrid.Dock = 'Fill'
$mirrorGrid.ColumnCount = 4
$mirrorGrid.RowCount = 4
for ($index = 0; $index -lt 4; $index++) {
    $mirrorGrid.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle([System.Windows.Forms.SizeType]::Percent, 25)))
    $mirrorGrid.RowStyles.Add((New-Object System.Windows.Forms.RowStyle([System.Windows.Forms.SizeType]::Percent, 25)))
}
$mirrorPanel.Controls.Add($mirrorGrid)

$slotTiles = @{}
for ($slotNumber = 1; $slotNumber -le 16; $slotNumber++) {
    $slotName = 'C{0:d2}' -f $slotNumber
    $tile = New-Object System.Windows.Forms.Label
    $tile.Text = "$slotName`nawaiting"
    $tile.TextAlign = 'MiddleCenter'
    $tile.Dock = 'Fill'
    $tile.Margin = New-Object System.Windows.Forms.Padding(4)
    $tile.BorderStyle = 'FixedSingle'
    $tile.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 10)
    $tile.BackColor = [System.Drawing.Color]::FromArgb(92, 78, 35)
    $tile.ForeColor = [System.Drawing.Color]::WhiteSmoke
    $slotTiles[$slotName] = $tile
    $mirrorGrid.Controls.Add($tile, (($slotNumber - 1) % 4), [math]::Floor(($slotNumber - 1) / 4))
}

$slotGrid = New-Object System.Windows.Forms.DataGridView
$slotGrid.Dock = 'Fill'
$slotGrid.ReadOnly = $true
$slotGrid.AllowUserToAddRows = $false
$slotGrid.AllowUserToDeleteRows = $false
$slotGrid.AllowUserToResizeRows = $false
$slotGrid.RowHeadersVisible = $false
$slotGrid.AutoSizeColumnsMode = 'Fill'
[void]$slotGrid.Columns.Add('Slot', 'Slot')
[void]$slotGrid.Columns.Add('Route', 'BQ route')
[void]$slotGrid.Columns.Add('Observed', 'Observed')
[void]$slotGrid.Columns.Add('Requested', 'Requested')
[void]$slotGrid.Columns.Add('Status', 'Status')
[void]$slotGrid.Columns.Add('Match', 'Match')
[void]$slotGrid.Columns.Add('Evidence', 'Latest evidence')
$slotGrid.Columns['Evidence'].FillWeight = 180
$slotsTab.Controls.Add($slotGrid)

$logBox = New-Object System.Windows.Forms.RichTextBox
$logBox.Dock = 'Fill'
$logBox.ReadOnly = $true
$logBox.BackColor = [System.Drawing.Color]::FromArgb(25, 29, 35)
$logBox.ForeColor = [System.Drawing.Color]::Gainsboro
$logBox.Font = New-Object System.Drawing.Font('Cascadia Mono', 9)
$logTab.Controls.Add($logBox)

function Get-StateColor([string]$match) {
    switch ($match) {
        'match' { return [System.Drawing.Color]::Honeydew }
        'awaiting' { return [System.Drawing.Color]::LemonChiffon }
        default { return [System.Drawing.Color]::MistyRose }
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
    $row.Cells['Slot'].Value = $slot
    $row.Cells['Route'].Value = $route
    $row.Cells['Observed'].Value = $observed
    $row.Cells['Requested'].Value = $requested
    $row.Cells['Status'].Value = $status
    $row.Cells['Match'].Value = $match
    $row.Cells['Evidence'].Value = $evidence
    $row.DefaultCellStyle.BackColor = Get-StateColor $match
    $slotTiles[$slot].Text = "$slot`n$observed`n$match"
    $slotTiles[$slot].BackColor = switch ($match) {
        'match' { [System.Drawing.Color]::FromArgb(45, 106, 66) }
        'fault' { [System.Drawing.Color]::FromArgb(137, 47, 47) }
        'mismatch' { [System.Drawing.Color]::FromArgb(137, 72, 35) }
        default { [System.Drawing.Color]::FromArgb(92, 78, 35) }
    }
}

function Add-LogLine([string]$line) {
    $timestamped = "$(Get-Date -Format 'HH:mm:ss.fff')  $line"
    $logBox.AppendText($timestamped + [Environment]::NewLine)
    $logBox.SelectionStart = $logBox.TextLength
    $logBox.ScrollToCaret()
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
    $missingComponents = @($expectedComponents | Where-Object { -not $script:observedComponents.Contains($_) })
    $passed = $script:invalidFrames -eq 0 -and $script:observedSlots.Count -eq 16 -and $missingComponents.Count -eq 0
    $result = if ($passed) { 'PASS' } else { 'FAIL' }
    $testLabel.Text = "${result}: components=$($script:observedComponents.Count)/4 slots=$($script:observedSlots.Count)/16 invalid=$($script:invalidFrames)"
    $testLabel.ForeColor = if ($passed) { [System.Drawing.Color]::ForestGreen } else { [System.Drawing.Color]::Firebrick }
    Add-LogLine "Observation $result after $ObservationSeconds s: component frames=$($script:componentFrames), slot frames=$($script:slotFrames), unique slots=$($script:observedSlots.Count), invalid=$($script:invalidFrames)."
    Disconnect-Monitor
    $script:observationWatch = $null
}

function Parse-MccLine([string]$line) {
    $fields = $line -split '\|', 8
    if ($fields.Count -lt 2 -or $fields[0] -ne 'MCC') {
        Set-ComponentStatus 'protocol' 'unrecognized' 'MCC frames' 'unrecognized UART text' 'awaiting' 'The line remains raw evidence in the UART Log tab.'
        return
    }
    switch ($fields[1]) {
        'HELLO' {
            Set-ComponentStatus 'firmware' 'read-only' 'read-only' 'banner received' 'match' (($fields | Select-Object -Skip 2) -join ' ')
        }
        'STATUS' {
            if ($fields.Count -ge 8) {
                $script:componentFrames++
                [void]$script:observedComponents.Add($fields[2])
                Set-ComponentStatus $fields[2] $fields[3] $fields[4] $fields[5] $fields[6] $fields[7]
            }
            elseif ($fields.Count -ge 5) { Set-ComponentStatus $fields[2] $fields[3] 'unknown' $fields[4] 'awaiting' 'Legacy status frame without comparison fields.' }
        }
        'SLOT' {
            if ($fields.Count -ge 8) {
                $script:slotFrames++
                [void]$script:observedSlots.Add($fields[2])
                Set-SlotStatus $fields[2] $fields[3] $fields[4] $fields[5] $fields[6] $fields[7]
            }
        }
        'EVENT' {
            if ($fields.Count -ge 5) { Set-ComponentStatus $fields[3] $fields[2].ToLowerInvariant() 'ready' $fields[4] 'awaiting' $fields[4] }
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
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = 'Stop'
$script:serialPort = $null
$script:logWriter = $null

$form = New-Object System.Windows.Forms.Form
$form.Text = 'MCC Pro Live Monitor'
$form.StartPosition = 'CenterScreen'
$form.MinimumSize = New-Object System.Drawing.Size(980, 650)
$form.Size = New-Object System.Drawing.Size(1200, 780)
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
if ($portSelector.Items.Contains('COM7')) { $portSelector.SelectedItem = 'COM7' } elseif ($portSelector.Items.Count -gt 0) { $portSelector.SelectedIndex = 0 }
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
$baudSelector.SelectedItem = '115200'
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

$split = New-Object System.Windows.Forms.SplitContainer
$split.Dock = 'Fill'
$split.Orientation = 'Horizontal'
$split.SplitterDistance = 270
$form.Controls.Add($split)

$statusGrid = New-Object System.Windows.Forms.DataGridView
$statusGrid.Dock = 'Fill'
$statusGrid.ReadOnly = $true
$statusGrid.AllowUserToAddRows = $false
$statusGrid.AllowUserToDeleteRows = $false
$statusGrid.AllowUserToResizeRows = $false
$statusGrid.RowHeadersVisible = $false
$statusGrid.AutoSizeColumnsMode = 'Fill'
[void]$statusGrid.Columns.Add('Component', 'Component')
[void]$statusGrid.Columns.Add('Expected', 'Expected mode')
[void]$statusGrid.Columns.Add('Live', 'Live state')
[void]$statusGrid.Columns.Add('Evidence', 'Latest evidence')
$statusGrid.Columns['Evidence'].FillWeight = 220
$split.Panel1.Controls.Add($statusGrid)

$logBox = New-Object System.Windows.Forms.RichTextBox
$logBox.Dock = 'Fill'
$logBox.ReadOnly = $true
$logBox.BackColor = [System.Drawing.Color]::FromArgb(25, 29, 35)
$logBox.ForeColor = [System.Drawing.Color]::Gainsboro
$logBox.Font = New-Object System.Drawing.Font('Cascadia Mono', 9)
$split.Panel2.Controls.Add($logBox)

function Set-ComponentStatus([string]$name, [string]$expected, [string]$live, [string]$evidence) {
    $row = $null
    foreach ($candidate in $statusGrid.Rows) {
        if ($candidate.Cells['Component'].Value -eq $name) { $row = $candidate; break }
    }
    if ($null -eq $row) { $row = $statusGrid.Rows[$statusGrid.Rows.Add()] }
    $row.Cells['Component'].Value = $name
    $row.Cells['Expected'].Value = $expected
    $row.Cells['Live'].Value = $live
    $row.Cells['Evidence'].Value = $evidence
    $row.DefaultCellStyle.BackColor = if ($live -eq $expected) { [System.Drawing.Color]::Honeydew } elseif ($live -eq 'awaiting') { [System.Drawing.Color]::LemonChiffon } else { [System.Drawing.Color]::MistyRose }
}

function Add-LogLine([string]$line) {
    $timestamped = "$(Get-Date -Format 'HH:mm:ss.fff')  $line"
    $logBox.AppendText($timestamped + [Environment]::NewLine)
    $logBox.SelectionStart = $logBox.TextLength
    $logBox.ScrollToCaret()
    if ($script:logWriter) { $script:logWriter.WriteLine($timestamped); $script:logWriter.Flush() }
}

function Parse-MccLine([string]$line) {
    $fields = $line -split '\|', 5
    if ($fields.Count -lt 2 -or $fields[0] -ne 'MCC') {
        Set-ComponentStatus 'protocol' 'MCC structured status' 'awaiting' 'Received legacy or unrecognized UART text.'
        return
    }
    switch ($fields[1]) {
        'HELLO' {
            Set-ComponentStatus 'firmware' 'read-only' 'read-only' ($fields | Select-Object -Skip 2) -join ' '
        }
        'STATUS' {
            if ($fields.Count -ge 5) {
                $expected = if ($fields[2] -in @('outputs', 'mux-select', 'firmware')) { 'read-only' } else { 'ready' }
                Set-ComponentStatus $fields[2] $expected $fields[3] $fields[4]
            }
        }
        'EVENT' {
            if ($fields.Count -ge 5) { Set-ComponentStatus $fields[3] 'ready' $fields[2].ToLowerInvariant() $fields[4] }
        }
    }
}

$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 100
$timer.Add_Tick({
    if ($script:serialPort -and $script:serialPort.IsOpen) {
        $data = $script:serialPort.ReadExisting()
        foreach ($line in ($data -split "`r?`n")) {
            if ($line) { Add-LogLine $line; Parse-MccLine $line }
        }
    }
})

$connectButton.Add_Click({
    if ($script:serialPort -and $script:serialPort.IsOpen) {
        $timer.Stop()
        $script:serialPort.Close()
        $script:serialPort.Dispose()
        $script:serialPort = $null
        if ($script:logWriter) { $script:logWriter.Dispose(); $script:logWriter = $null }
        $connectButton.Text = 'Connect Read-Only'
        $connectionLabel.Text = 'Disconnected'
        $connectionLabel.ForeColor = [System.Drawing.Color]::Firebrick
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
        Set-ComponentStatus 'protocol' 'MCC structured status' 'awaiting' 'Waiting for MCC firmware status frames.'
        $connectButton.Text = 'Disconnect'
        $connectionLabel.Text = "Connected read-only: $($portSelector.SelectedItem)"
        $connectionLabel.ForeColor = [System.Drawing.Color]::ForestGreen
        Add-LogLine "Monitor connected with DTR/RTS disabled; log stored locally in artifacts."
        $timer.Start()
    } catch {
        if ($script:logWriter) { $script:logWriter.Dispose(); $script:logWriter = $null }
        $connectionLabel.Text = "Connection error: $($_.Exception.Message)"
        $connectionLabel.ForeColor = [System.Drawing.Color]::Firebrick
    }
})

$form.Add_FormClosing({
    $timer.Stop()
    if ($script:serialPort -and $script:serialPort.IsOpen) { $script:serialPort.Close(); $script:serialPort.Dispose() }
    if ($script:logWriter) { $script:logWriter.Dispose() }
})

Set-ComponentStatus 'firmware' 'read-only' 'awaiting' 'Connect to receive live status.'
Set-ComponentStatus 'outputs' 'read-only' 'awaiting' 'No output command is available in the monitor.'
[void]$form.ShowDialog()
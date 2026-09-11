# Shared dark palette for MCC Pro Live Monitor.
# Applied to content-area controls only (panels, grids, log, labels).
# Native chrome (ComboBox/scrollbar rendering) is intentionally left system-styled.
$script:Theme = @{
    Window    = [System.Drawing.Color]::FromArgb(0x0B, 0x14, 0x20)
    Panel     = [System.Drawing.Color]::FromArgb(0x10, 0x1D, 0x2B)
    PanelAlt  = [System.Drawing.Color]::FromArgb(0x14, 0x23, 0x34)
    Border    = [System.Drawing.Color]::FromArgb(0x29, 0x40, 0x56)
    Text      = [System.Drawing.Color]::FromArgb(0xE7, 0xED, 0xF4)
    TextMuted = [System.Drawing.Color]::FromArgb(0x95, 0xA6, 0xB8)
    Accent    = [System.Drawing.Color]::FromArgb(0x35, 0xA7, 0xFF)
    Success   = [System.Drawing.Color]::FromArgb(0x34, 0xD1, 0x7B)
    Warning   = [System.Drawing.Color]::FromArgb(0xE9, 0xB9, 0x49)
    Error     = [System.Drawing.Color]::FromArgb(0xF0, 0x5D, 0x5E)
    Selection = [System.Drawing.Color]::FromArgb(0x19, 0x3B, 0x55)
}

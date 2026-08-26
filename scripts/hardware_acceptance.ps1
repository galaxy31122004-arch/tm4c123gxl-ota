param(
    [string]$OtaPort,
    [string]$DebugPort,
    [string]$TivaWareRoot = 'C:\ti\TivaWare_C_Series-2.2.0.295',
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$steps = @(
    'Build images and validate map addresses.',
    'Flash bootloader and confirming Slot A v1.0.0 through ICDI.',
    'Require GET_INFO to report Slot A ACTIVE.',
    'OTA confirming Slot B v1.0.1 and require Slot B ACTIVE.',
    'OTA non-confirming Slot A and require rollback after three attempts.',
    'Interrupt UART during DATA and verify the active image boots.',
    'Interrupt power during update and verify the active image boots.'
)
if (!(Test-Path -LiteralPath $TivaWareRoot)) { throw "TivaWare root not found: $TivaWareRoot" }
if ($DryRun) {
    Write-Host 'Phase 1 hardware acceptance dry run:'
    for ($i = 0; $i -lt $steps.Count; $i++) { Write-Host ("{0}. {1}" -f ($i + 1), $steps[$i]) }
    exit 0
}
if ([string]::IsNullOrWhiteSpace($OtaPort) -or [string]::IsNullOrWhiteSpace($DebugPort)) { throw 'OtaPort and DebugPort are required.' }
$ports = [System.IO.Ports.SerialPort]::GetPortNames()
if ($OtaPort -notin $ports) { throw "OTA port not found: $OtaPort" }
if ($DebugPort -notin $ports) { throw "Debug port not found: $DebugPort" }
$logDir = Join-Path $root ('artifacts\hardware\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
& (Join-Path $PSScriptRoot 'build_ccs.ps1') -TivaWareRoot $TivaWareRoot 2>&1 | Tee-Object -FilePath (Join-Path $logDir 'build.log')
Write-Host 'Build complete. Follow docs/HARDWARE-TEST.md and save logs in:' $logDir
for ($i = 1; $i -lt $steps.Count; $i++) { Write-Host ("{0}. {1}" -f ($i + 1), $steps[$i]) }

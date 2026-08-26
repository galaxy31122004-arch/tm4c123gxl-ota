param([Parameter(Mandatory=$true)][string]$Port)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$tool = Join-Path $root '.venv\Scripts\tm4c-ota.exe'
if (!(Test-Path $tool)) { throw 'PC tool is not installed. Run scripts\setup_pc_tool.ps1 first.' }
$image = Join-Path $root 'artifacts\ota_slot_b_v1.0.1.bin'
if (!(Test-Path $image)) { throw 'OTA image is missing. Run scripts\build_ccs.ps1 first.' }
Write-Host 'ESP32 simulation: GET_INFO'
& $tool --port $Port info
Write-Host 'ESP32 simulation: START_UPDATE + DATA + END_UPDATE'
& $tool --port $Port update $image
Write-Host 'ESP32 simulation: RESET'
& $tool --port $Port reset
Write-Host 'OTA transfer completed; the application will confirm itself after boot.'

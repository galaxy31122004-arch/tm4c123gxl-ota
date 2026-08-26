param([switch]$NoBuild)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    if (!$NoBuild) { & "$PSScriptRoot\build_ccs.ps1" -TivaWareRoot 'C:\ti\TivaWare_C_Series-2.2.0.295' }
    $uniflash = 'C:\ti\ccs2100\ccs\ccs_base\scripting\examples\uniflash\cmdLine\uniflash.bat'
    if (!(Test-Path $uniflash)) { throw 'CCS UniFlash command line tool was not found.' }
    $env:TI_APPDATA_DIR = "$root\build\ti-appdata"
    New-Item -ItemType Directory -Force $env:TI_APPDATA_DIR | Out-Null
    & $uniflash -ccxml 'config\tm4c123gxl.ccxml' -core 'CORTEX_M4_0' `
        -programBin 'artifacts\bootloader.bin' '0x00000000' 'artifacts\factory_slot_a_v1.0.0.bin' '0x00008000' `
        -verifyBin 'artifacts\bootloader.bin' '0x00000000' 'artifacts\factory_slot_a_v1.0.0.bin' '0x00008000' `
        -targetOp reset run
    if ($LASTEXITCODE) { throw 'ICDI flash/verify failed. Check the DEBUG USB cable and board power switch.' }
} finally { Pop-Location }

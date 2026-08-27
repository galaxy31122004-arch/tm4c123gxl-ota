param([switch]$NoBuild)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    if (!$NoBuild) { & "$PSScriptRoot\build_ccs_actual.ps1" -TivaWareRoot 'C:\ti\TivaWare_C_Series-2.2.0.295' }
    $uniflash = 'C:\ti\ccs2100\ccs\ccs_base\scripting\examples\uniflash\cmdLine\uniflash.bat'
    if (!(Test-Path $uniflash)) { throw 'CCS UniFlash command line tool was not found.' }
    $env:TI_APPDATA_DIR = "$root\build\ti-appdata"
    New-Item -ItemType Directory -Force $env:TI_APPDATA_DIR | Out-Null
    $ccxml = (Resolve-Path 'config\tm4c123gxl.ccxml').Path.Replace('\','/')
    $bootloader = (Resolve-Path 'artifacts\bootloader.bin').Path.Replace('\','/')
    $factory = (Resolve-Path 'artifacts\factory_slot_a_v1.0.0.bin').Path.Replace('\','/')
    & $uniflash -ccxml $ccxml -core 'CORTEX_M4_0' `
        -programBin $bootloader '0x00000000' $factory '0x00008000' `
        -verifyBin $bootloader '0x00000000' $factory '0x00008000' `
        -targetOp reset run
    if ($LASTEXITCODE) { throw 'ICDI flash/verify failed. Check the DEBUG USB cable and board power switch.' }
} finally { Pop-Location }

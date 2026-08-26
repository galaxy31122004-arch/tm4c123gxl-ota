param([Parameter(Mandatory=$true)][string]$TivaWareRoot)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$cc = 'C:\ti\ccs2100\ccs\tools\compiler\ti-cgt-armllvm_5.1.1.LTS\bin\tiarmclang.exe'
$objcopy = 'C:\ti\ccs2100\ccs\tools\compiler\ti-cgt-armllvm_5.1.1.LTS\bin\tiarmobjcopy.exe'
if (!(Test-Path $cc) -or !(Test-Path $objcopy)) { throw 'CCS TI Arm Clang 5.1.1.LTS was not found under C:\ti\ccs2100.' }
if (!(Test-Path $TivaWareRoot)) { throw "TivaWare root not found: $TivaWareRoot" }
$artifacts = Join-Path $root 'artifacts'; $objects = Join-Path $artifacts 'obj'
New-Item -ItemType Directory -Force $objects | Out-Null
$base = @('--target=arm-ti-none-eabi','-mcpu=cortex-m4','-mthumb','-Oz','-fshort-wchar','-std=c11','-D__TI_ARM__','-DPART_TM4C123GH6PM')
$includes = @("-I$root\tm4c123gxl\common\inc","-I$root\tm4c123gxl\bootloader\inc","-I$root\tm4c123gxl\application\inc","-I$TivaWareRoot")
function Compile([string]$Source,[string]$Output,[string[]]$Defines=@()) {
    & $cc @base @includes @Defines -c $Source -o $Output
    if ($LASTEXITCODE) { throw "Compile failed: $Source" }
}
function Link([string]$Output,[string]$Map,[string]$Linker,[string[]]$Inputs) {
    & $cc '--target=arm-ti-none-eabi' '-mcpu=cortex-m4' '-mthumb' '-fshort-wchar' "-Wl,-m,$Map" -o $Output @Inputs $Linker
    if ($LASTEXITCODE) { throw "Link failed: $Output" }
    & $objcopy -O binary $Output ([IO.Path]::ChangeExtension($Output,'.bin'))
    if ($LASTEXITCODE) { throw "Binary conversion failed: $Output" }
}
Push-Location $root
try {
    Compile 'tm4c123gxl\common\src\ota_crc32.c' "$objects\app_crc.o"
    Compile 'tm4c123gxl\application\src\boot_confirm.c' "$objects\app_confirm.o"
    Compile 'tm4c123gxl\application\startup_tm4c123.c' "$objects\app_startup.o"
    Compile 'tm4c123gxl\application\src\app_main.c' "$objects\app_a_main.o" @('-DOTA_APP_SLOT=0','-DAPP_VERSION_MAJOR=1','-DAPP_VERSION_MINOR=0','-DAPP_VERSION_PATCH=0')
    Compile 'tm4c123gxl\application\src\app_main.c' "$objects\app_b_main.o" @('-DOTA_APP_SLOT=1','-DAPP_VERSION_MAJOR=1','-DAPP_VERSION_MINOR=0','-DAPP_VERSION_PATCH=1')
    Link "$artifacts\application_SlotA.out" "$artifacts\application_SlotA.map" 'tm4c123gxl\application\linker\slot_a.cmd' @("$objects\app_crc.o","$objects\app_confirm.o","$objects\app_a_main.o","$objects\app_startup.o")
    Link "$artifacts\application_SlotB.out" "$artifacts\application_SlotB.map" 'tm4c123gxl\application\linker\slot_b.cmd' @("$objects\app_crc.o","$objects\app_confirm.o","$objects\app_b_main.o","$objects\app_startup.o")
    $bootObjects = @()
    foreach ($name in @('ota_crc32','ota_protocol','ota_metadata','ota_boot','ota_image')) { $out="$objects\bl_$name.o"; Compile "tm4c123gxl\common\src\$name.c" $out; $bootObjects += $out }
    foreach ($name in @('bl_hal_tm4c','bl_update','bl_jump','bl_main')) { $out="$objects\$name.o"; Compile "tm4c123gxl\bootloader\src\$name.c" $out @('-DTARGET_IS_TM4C123_RB1'); $bootObjects += $out }
    Compile 'tm4c123gxl\application\src\boot_confirm.c' "$objects\bl_confirm.o"; $bootObjects += "$objects\bl_confirm.o"
    Compile 'tm4c123gxl\bootloader\startup_tm4c123.c' "$objects\bl_startup.o"; $bootObjects += "$objects\bl_startup.o"
    $bootObjects += "$TivaWareRoot\driverlib\ccs\Debug\driverlib.lib"
    Link "$artifacts\bootloader.out" "$artifacts\bootloader.map" 'tm4c123gxl\bootloader\linker\bootloader.cmd' $bootObjects
    py -3.10 scripts\package_image.py --slot A --version 1.0.0 --input artifacts\application_SlotA.bin --output artifacts\factory_slot_a_v1.0.0.bin
    py -3.10 scripts\package_image.py --slot B --version 1.0.1 --input artifacts\application_SlotB.bin --output artifacts\ota_slot_b_v1.0.1.bin
    $checks=@(@('bootloader.map','00000000'),@('application_SlotA.map','00008400'),@('application_SlotB.map','00024000'))
    foreach($check in $checks) { if ((Get-Content -Raw (Join-Path $artifacts $check[0])) -notmatch "\.intvecs\s+0\s+$($check[1])") { throw "Bad vector address in $($check[0])" } }
    if ((Get-Item "$artifacts\bootloader.bin").Length -gt 32768) { throw 'Bootloader exceeds 32 KB.' }
    Write-Host 'Build passed. Generated bootloader.bin, factory_slot_a_v1.0.0.bin, and ota_slot_b_v1.0.1.bin.'
} finally { Pop-Location }

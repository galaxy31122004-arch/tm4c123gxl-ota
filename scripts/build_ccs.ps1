param([Parameter(Mandatory=$true)][string]$TivaWareRoot)
$ErrorActionPreference = 'Stop'
if (!(Test-Path -LiteralPath $TivaWareRoot)) { throw "TivaWare root not found: $TivaWareRoot" }
$root = (Resolve-Path .).Path
$projects = @(@{Name='bootloader'; Map='artifacts/bootloader.map'; Vector='0x00000000'; Limit=32768}, @{Name='application_SlotA'; Map='artifacts/application_SlotA.map'; Vector='0x00008400'; Limit=112640}, @{Name='application_SlotB'; Map='artifacts/application_SlotB.map'; Vector='0x00024000'; Limit=112640})
New-Item -ItemType Directory -Force artifacts | Out-Null
foreach ($p in $projects) {
  $map = Join-Path $root $p.Map
  if (!(Test-Path -LiteralPath $map)) { Write-Error "Missing CCS map for $($p.Name): $map"; exit 2 }
  $text = Get-Content -Raw -LiteralPath $map
  if ($text -notmatch [regex]::Escape($p.Vector)) { throw " $($p.Name): .intvecs address $($p.Vector) not found" }
  $bin = [IO.Path]::ChangeExtension($map,'.bin'); if (Test-Path $bin) { if ((Get-Item $bin).Length -gt ($p.Limit + 1024)) { throw "$($p.Name) image exceeds configured limit" } }
}
Write-Host 'CCS layout validation passed.'

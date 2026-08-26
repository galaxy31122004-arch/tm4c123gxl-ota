param([Parameter(Mandatory=$true)][string]$TivaWareRoot)
& (Join-Path $PSScriptRoot 'build_ccs_actual.ps1') -TivaWareRoot $TivaWareRoot
exit $LASTEXITCODE

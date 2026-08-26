$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    py -3.10 -m venv .venv
    & '.\.venv\Scripts\python.exe' -m pip install --upgrade pip setuptools
    & '.\.venv\Scripts\python.exe' -m pip install -e '.\pc_tool[test]'
} finally { Pop-Location }

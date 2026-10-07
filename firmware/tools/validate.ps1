param([ValidateSet('--all', '--static', '--firmware')][string]$Mode = '--all')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
Push-Location $repoRoot
try {
    $env:PYTHONUTF8 = '1'
    $env:PYTHONDONTWRITEBYTECODE = '1'
    $env:PYTHON = (Get-Command python -ErrorAction Stop).Source.Replace('\', '/')
    if ($Mode -ne '--firmware') {
        $bash = Get-Command bash -ErrorAction SilentlyContinue
        $bashPath = if ($bash) { $bash.Source } else { Join-Path $env:ProgramFiles 'Git/bin/bash.exe' }
        & $bashPath tools/validate.sh --static
        if ($LASTEXITCODE -ne 0) { throw "Static gate failed ($LASTEXITCODE)" }
    }
    if ($Mode -ne '--static') {
        & $env:PYTHON tools/validate_firmware.py
        if ($LASTEXITCODE -ne 0) { throw "Firmware gate failed ($LASTEXITCODE)" }
    }
    if ($Mode -eq '--all') {
        & $env:PYTHON tools/run_cast_tests.py --font-runtime
        if ($LASTEXITCODE -ne 0) { throw "LVGL font runtime test failed ($LASTEXITCODE)" }
    }
} finally {
    Pop-Location
}

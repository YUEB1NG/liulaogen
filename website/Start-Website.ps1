param(
    [string]$BindAddress = '127.0.0.1',
    [int]$Port = 8766,
    [string]$PublicOrigin = '',
    [string]$DataDirectory = (Join-Path $PSScriptRoot 'runtime')
)
$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$env:PYTHONDONTWRITEBYTECODE = '1'
$arguments = @((Join-Path $PSScriptRoot 'server.py'), '--host', $BindAddress, '--port', "$Port", '--data-dir', $DataDirectory)
if ($PublicOrigin) { $arguments += @('--public-origin', $PublicOrigin) }
& python @arguments
exit $LASTEXITCODE

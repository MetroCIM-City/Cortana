#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
$out = Join-Path $PSScriptRoot 'out'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$dotnet8 = Join-Path $env:LOCALAPPDATA 'Microsoft\dotnet-8\dotnet.exe'
& $dotnet8 run --project (Join-Path $PSScriptRoot '..\spike\CfbSpike\CfbSpike.csproj') -c Release -- $out
if ($LASTEXITCODE -ne 0) { throw "fixture generation failed: $LASTEXITCODE" }
Write-Host "Synthetic compound files are in $out (gitignored)."

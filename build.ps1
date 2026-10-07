#Requires -Version 5.1
param(
    [string]$Version = '1.2.0',
    [switch]$SkipNative,
    [switch]$SkipInstaller
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$dotnet8 = Join-Path $env:LOCALAPPDATA 'Microsoft\dotnet-8\dotnet.exe'
$dotnet10 = (Get-Command dotnet -ErrorAction Stop).Source
$artifacts = Join-Path $root 'artifacts\native'
$stage = Join-Path $root 'artifacts\stage'
$dist = Join-Path $root 'dist'

function Invoke-Step([string]$Name, [scriptblock]$Action) {
    Write-Host "== $Name"
    & $Action
    if ($LASTEXITCODE -ne $null -and $LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

function Find-MsBuild {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw 'vswhere.exe was not found.' }
    $root = & $vswhere -prerelease -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if (-not $root) { throw 'Visual Studio with MSBuild was not found.' }
    $msbuild = Join-Path $root 'MSBuild\Current\Bin\MSBuild.exe'
    if (-not (Test-Path $msbuild)) { throw "MSBuild was not found at $msbuild" }
    return $msbuild
}

if (-not (Test-Path $dotnet8)) { throw "The .NET 8 SDK is not at $dotnet8" }

if (-not $SkipNative) {
    $msbuild = Find-MsBuild
    $native = @(
        'src\ShellHandler\ShellHandler.vcxproj',
        'src\Store.Abi\Store.Abi.vcxproj',
        'tests\Store.Tests\Store.Tests.vcxproj',
        'tests\ShellHandler.Tests\ShellHandler.Tests.vcxproj'
    )
    foreach ($project in $native) {
        Write-Host "== $project"
        & $msbuild (Join-Path $root $project) /p:Configuration=Release /p:Platform=x64 /m /v:minimal
        if ($LASTEXITCODE -ne 0) { throw "$project failed with exit code $LASTEXITCODE" }
    }
    Invoke-Step 'Store.Tests' { & (Join-Path $artifacts 'Store.Tests.exe') }
    Invoke-Step 'Store.Tests perf' { & (Join-Path $artifacts 'Store.Tests.exe') --perf }
    Invoke-Step 'ShellHandler.Tests' { & (Join-Path $artifacts 'ShellHandler.Tests.exe') }
}

Invoke-Step 'Store.Net' { & $dotnet8 build (Join-Path $root 'src\Store.Net\RvtFileInfo.Store.Net.csproj') -c Release }
Invoke-Step 'Store.Net10' { & $dotnet10 build (Join-Path $root 'src\Store.Net\RvtFileInfo.Store.Net10.csproj') -c Release }
Invoke-Step 'Revit 2025 add-in' { & $dotnet8 build (Join-Path $root 'src\RevitAddin\RevitAddin.csproj') -c R25 }
Invoke-Step 'Revit 2026 add-in' { & $dotnet8 build (Join-Path $root 'src\RevitAddin\RevitAddin.csproj') -c R26 }
Invoke-Step 'Revit 2027 add-in' { & $dotnet10 build (Join-Path $root 'src\RevitAddin\RevitAddin.csproj') -c R27 }
Invoke-Step 'Addin.Tests' { & $dotnet8 test (Join-Path $root 'tests\Addin.Tests\Addin.Tests.csproj') -c Release --nologo }

if ($SkipInstaller) { return }

$stageShell = Join-Path $root 'artifacts\stage\shell'
$stageAddin = Join-Path $root 'artifacts\stage\addin'
$stageManifests = Join-Path $root 'artifacts\stage\manifests'
foreach ($dir in @($stage, $stageShell, $stageAddin, $stageManifests, $dist)) {
    if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

Invoke-Step 'publish setup (shell)' { & $dotnet8 publish (Join-Path $root 'src\Setup\RvtFileInfo.Setup.csproj') -c Release -r win-x64 --self-contained true -o $stageShell }
Invoke-Step 'publish cli' { & $dotnet8 publish (Join-Path $root 'src\EditorCli\RvtFileInfo.EditorCli.csproj') -c Release -r win-x64 --self-contained true -o (Join-Path $stageShell 'cli') }
Invoke-Step 'publish setup (manifests)' { & $dotnet8 publish (Join-Path $root 'src\Setup\RvtFileInfo.Setup.csproj') -c Release -r win-x64 --self-contained true -o $stageManifests }

Copy-Item (Join-Path $artifacts 'RvtFileInfo.Store.dll') $stageShell -Force
Copy-Item (Join-Path $artifacts 'RvtFileInfo.ShellHandler.dll') $stageShell -Force
Copy-Item (Join-Path $artifacts 'RvtFileInfo.Store.dll') (Join-Path $stageShell 'cli') -Force
Copy-Item (Join-Path $root 'src\ShellHandler\RvtFileInfo.propdesc') $stageShell -Force

foreach ($year in 2025, 2026, 2027) {
    $config = switch ($year) { 2025 { 'R25' } 2026 { 'R26' } 2027 { 'R27' } }
    $from = Join-Path $root "src\RevitAddin\bin\$config"
    $to = Join-Path $stageAddin "$year"
    New-Item -ItemType Directory -Force -Path $to | Out-Null
    Copy-Item (Join-Path $from 'RvtFileInfo.RevitAddin.dll') $to -Force
    Copy-Item (Join-Path $from 'RvtFileInfo.RevitAddin.deps.json') $to -Force
    Copy-Item (Join-Path $from 'RvtFileInfo.Store.Net.dll') $to -Force
    Copy-Item (Join-Path $artifacts 'RvtFileInfo.Store.dll') $to -Force
    Get-ChildItem $from -Filter 'RvtFileInfo.RevitAddin.pdb' -ErrorAction SilentlyContinue | Copy-Item -Destination $to -Force
}

$wix = Join-Path $root '.tools\wix.exe'
$license = Join-Path $root 'src\Installer\License.rtf'
$packages = @(
    @{ Name = 'shell'; Wxs = 'Package.wxs'; Stage = $stageShell; Out = "RvtFileInfo-$Version-x64.msi"; Extra = @() }
    @{ Name = 'addin'; Wxs = 'Package.Addin.wxs'; Stage = $stageAddin; Out = "RvtFileInfo.RevitAddin-$Version-x64.msi"; Extra = @("-d", "PicklistFile=$(Join-Path $root 'src\Setup\picklists.json')") }
    @{ Name = 'manifests'; Wxs = 'Package.Manifests.wxs'; Stage = $stageManifests; Out = "RvtFileInfo.Manifests-$Version-x64.msi"; Extra = @() }
)

$sums = @()
foreach ($package in $packages) {
    $msi = Join-Path $dist $package.Out
    Invoke-Step $package.Name {
        $wixArgs = @(
            'build', (Join-Path $root "src\Installer\$($package.Wxs)"),
            '-ext', 'WixToolset.Util.wixext/5.0.2',
            '-ext', 'WixToolset.UI.wixext/5.0.2',
            '-d', "StageDir=$($package.Stage)",
            '-d', "LicenseRtf=$license",
            '-arch', 'x64',
            '-o', $msi
        ) + $package.Extra
        & $wix @wixArgs
    }
    if ($env:SIGN_CERT_THUMBPRINT) {
        $signtool = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe' -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
        if (-not $signtool) { throw 'SIGN_CERT_THUMBPRINT is set but signtool.exe was not found.' }
        Invoke-Step "sign $($package.Name)" { & $signtool.FullName sign /fd SHA256 /sha1 $env:SIGN_CERT_THUMBPRINT $msi }
    }
    $hash = Get-FileHash $msi -Algorithm SHA256
    $sums += "$($hash.Hash)  $($package.Out)"
    Write-Host "Package: $msi"
    Write-Host "SHA256: $($hash.Hash)"
}

$sums | Set-Content -Path (Join-Path $dist 'SHA256SUMS.txt') -Encoding ascii

#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
$out = Join-Path $PSScriptRoot 'out'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$keys = @(
    'HKCR\.rvt',
    'HKCR\.rfa',
    'HKCR\.dwg',
    'HKCR\.nwd',
    'HKCR\.nwf',
    'HKCR\.nwc',
    'HKCR\.pdf',
    'HKCR\Revit.Project',
    'HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.rvt',
    'HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.rfa',
    'HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.dwg',
    'HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.nwd',
    'HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.nwf',
    'HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.nwc',
    'HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.pdf',
    'HKLM\SOFTWARE\RvtFileInfo',
    'HKCR\CLSID\{C4A91E72-5D38-4F0B-9E16-2B7A6C8D4E50}'
)
$label = if ($args.Count -gt 0) { $args[0] } else { 'snapshot' }
$path = Join-Path $out "$label.txt"
$builder = New-Object System.Text.StringBuilder
foreach ($key in $keys) {
    [void]$builder.AppendLine("===== $key =====")
    $query = & reg query $key /s 2>&1
    [void]$builder.AppendLine(($query | Out-String))
}
[System.IO.File]::WriteAllText($path, $builder.ToString())
Write-Host $path

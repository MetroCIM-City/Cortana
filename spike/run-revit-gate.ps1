#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
$root = 'C:\Users\SIGNAL\AppData\Local\Temp\rvtfileinfo-gate'
$source = 'C:\Users\SIGNAL\Desktop\Construction_Ceilling_Knauf_D152-DE_wood-beamed-ceiling-system_metal-grid-CD-60x27_2016.rvt'
$spike = 'C:\Users\SIGNAL\.cursor\projects\Cortana\Cortana\spike\CfbSpike\bin\Release\net8.0\CfbSpike.exe'
$revit = 'C:\Program Files\Autodesk\Revit 2027\Revit.exe'
$dotnet8 = Join-Path $env:LOCALAPPDATA 'Microsoft\dotnet-8\dotnet.exe'
New-Item -ItemType Directory -Force -Path $root | Out-Null
& $dotnet8 build 'C:\Users\SIGNAL\.cursor\projects\Cortana\Cortana\spike\CfbSpike\CfbSpike.csproj' -c Release --nologo | Out-Null
$seed = Join-Path $root 'seed.rvt'
Copy-Item $source $seed -Force
& $spike (Join-Path $root 'prep') $seed | Out-Null
$gate = Join-Path $root 'gate.rvt'
Copy-Item (Join-Path $root 'prep\sample-copy.rvt') $gate -Force
$before = & $spike streams $gate
$beforeStamp = (Get-Item $gate).LastWriteTimeUtc
$stamp = Get-Date -Format 'dd-MMM-yyyy HH:mm:ss.fff'
$journal = @"
'C $stamp;
Dim Jrn
Set Jrn = CrsJournalScript
Jrn.Directive "DebugMode", "PerformAutomaticActionInErrorDialog", 1
Jrn.Directive "DebugMode", "PermissiveJournal", 1
Jrn.Command "Ribbon" , "Open an existing project , ID_REVIT_FILE_OPEN"
Jrn.Data "FileOpenSubDialog" , "AuditCheckBox", "False"
Jrn.Data "File Name" , "IDOK", "$gate"
Jrn.Data "WorksetConfig" , "Custom", 0
Jrn.Data "TaskDialogResult" , "Revit could not find or read 1 references. What do you want to do?", "Ignore and continue opening the project", "1002"
Jrn.Directive "DocSymbol" , "[]"
Jrn.Command "Ribbon" , "Save the active project , ID_REVIT_FILE_SAVE"
Jrn.Command "SystemMenu" , "Quit the application; prompts to save projects , ID_APP_EXIT"
Jrn.Data "TaskDialogResult" , "Do you want to save changes to Untitled?", "No", "IDNO"
"@
$journalPath = Join-Path $root 'gate.txt'
Set-Content -Path $journalPath -Value $journal -Encoding ASCII
if (Get-Process Revit -ErrorAction SilentlyContinue) {
    'G2 UNTESTED Revit already running'
    'G3 UNTESTED Revit already running'
    'G4 UNTESTED Revit already running'
    'G5 UNTESTED Revit already running'
    exit 0
}
$proc = Start-Process -FilePath $revit -ArgumentList "`"$journalPath`"" -PassThru
$finished = $proc.WaitForExit(180000)
if (-not $finished) {
    try { Stop-Process -Id $proc.Id -Force } catch {}
    Start-Sleep -Seconds 2
}
$afterStamp = (Get-Item $gate).LastWriteTimeUtc
$after = @()
try { $after = & $spike streams $gate } catch { $after = @('READ_FAILED') }
$kept = $after -contains 'RvtFileInfo'
"finished=$finished exit=$($proc.ExitCode) stampChanged=$($afterStamp -ne $beforeStamp) streamKept=$kept"
'BEFORE'
$before
'AFTER'
$after

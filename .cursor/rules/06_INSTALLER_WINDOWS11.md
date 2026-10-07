# 06 — Windows 11 Installer

## Technology
**WiX Toolset v5 (MSI)**, per-machine, x64, with a small .NET (or C++) custom-action helper. Alternative allowed only if documented in an ADR: Inno Setup. No MSIX (shell extension + HKLM schema registration is not a good fit).

## Install layout
```
%ProgramFiles%\RvtFileInfo\
  RvtFileInfo.ShellHandler.dll
  RvtFileInfo.Store.dll
  RvtFileInfo.propdesc
  rvtinfo.exe                       # CLI tool
  RegisterSchema.exe                # helper (or MSI custom action)
  RevitAddin\2025\*  RevitAddin\2026\*  RevitAddin\2027\*   # Track B only
%ProgramData%\RvtFileInfo\
  picklists.json  (sample, preserved on upgrade)  logs\
%ProgramData%\Autodesk\Revit\Addins\<year>\RvtFileInfo.addin   # Track B only
```

## Install sequence (all elevated)
1. Copy files.
2. **Backup**: write `HKLM\SOFTWARE\RvtFileInfo\OriginalPropertyHandler` (existing `.rvt` handler CLSID, if any) and the original `FullDetails/PreviewDetails/InfoTip` strings.
3. Register COM class (`InprocServer32`, Approved list).
4. `PSRegisterPropertySchema("...\RvtFileInfo.propdesc")` → `PSRefreshPropertySchema()`.
5. Register `.rvt` property handler + append our properties to `FullDetails`, `PreviewDetails`, `InfoTip`.
6. Track B: detect Revit 2025/2026/2027 and write `.addin` manifests (+ `INSTALLALLYEARS` option).
7. `SHChangeNotify(SHCNE_ASSOCCHANGED, ...)`; offer to restart Explorer (`taskkill /f /im explorer.exe` + relaunch) — **opt-in checkbox**, default on, never in silent mode unless `RESTARTEXPLORER=1`.

## Uninstall sequence
Reverse order: remove manifests → restore original `.rvt` handler and detail strings → `PSUnregisterPropertySchema` → unregister COM → delete files; leave `%ProgramData%\RvtFileInfo\picklists.json` only if the user changed it (otherwise remove). Verify no leftovers (see tests).

## Upgrade / repair
- Major upgrade by `UpgradeCode`; if the handler DLL is in use by Explorer/prophost, schedule replacement at next Explorer restart (use Restart Manager via WiX `FilesInUse` handling) — **no reboot** needed in the normal path.
- Repair re-runs registration steps idempotently.

## UI & properties
- Minimal wizard: licence (optional), install folder (default fixed), options: ☐ Install Revit add-in (Track B, default on), ☐ Restart Explorer, ☐ Install for all detected Revit years.
- Public properties for silent installs: `INSTALLADDIN=0|1`, `INSTALLALLYEARS=0|1`, `RESTARTEXPLORER=0|1`.
- Logging: `msiexec /i RvtFileInfo.msi /l*v install.log`.

## Build & signing
- `build.ps1 -Configuration Release` outputs `dist\RvtFileInfo-<version>-x64.msi` and a `dist\SHA256SUMS.txt`.
- Sign DLLs and MSI with `signtool` if `$env:SIGN_CERT_THUMBPRINT` is set; otherwise skip with a warning.

## Pre-flight checks (installer must refuse politely)
- Windows 11 (build ≥ 22000), x64. Admin rights.
- Warn (not block) if no Revit 2025–2027 is detected in Track B (add-in manifests can still be installed with `INSTALLALLYEARS=1`).

## Done when
- Clean Windows 11 VM: install → columns available; uninstall → registry/file diff vs baseline is empty (except logs the user opted to keep).

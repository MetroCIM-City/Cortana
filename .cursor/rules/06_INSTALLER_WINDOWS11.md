# 06 — Windows 11 Installers (three MSIs)

WiX v5, per-machine, x64. Custom actions call `RvtFileInfo.Setup.exe` (`WixQuietExec` / `Wix4UtilCA_X64`). No Inno, no MSIX. UI: `WixUI_Minimal` + a small custom dialog. License `src/Installer/License.rtf`. Manufacturer **Metropolitan CIM**. Version **1.2.0.0**.

WiX 5: do not put `Win64` on `RegistrySearch`. `NeverOverwrite` belongs on `Component`, not `File`.

## Products

| MSI | Name | UpgradeCode | Setup verb |
|-----|------|-------------|------------|
| `RvtFileInfo-1.2.0-x64.msi` | RvtFileInfo | `{D1F4A8C3-2E67-4B90-8A15-6C3E9F0B2D47}` | `shell install\|uninstall` |
| `RvtFileInfo.RevitAddin-1.2.0-x64.msi` | RvtFileInfo Revit Add-in | `{E2A5B9D4-3F78-4C01-9B26-7D4E0A1C3E58}` | none (files + registry) |
| `RvtFileInfo.Manifests-1.2.0-x64.msi` | RvtFileInfo Revit Manifests | `{F3B6C0E5-4089-4D12-8C37-8E5F1B2D4F69}` | `manifests install\|uninstall` |

WXS: `Package.wxs`, `Package.Addin.wxs`, `Package.Manifests.wxs`. `build.ps1` stages `artifacts\stage\shell`, `addin`, `manifests`.

Older 1.0/1.1 combined MSI: Explorer 1.2.0 major-upgrades it via the Explorer UpgradeCode. Then install add-in + manifests.

## Layout

Explorer (`%ProgramFiles%\RvtFileInfo\`):
```
RvtFileInfo.Setup.exe          # self-contained net8 win-x64
RvtFileInfo.ShellHandler.dll
RvtFileInfo.Store.dll
RvtFileInfo.propdesc
cli\rvtinfo.exe                # + Store.dll
```

Add-in:
```
%ProgramFiles%\RvtFileInfo\RevitAddin\2025|2026|2027\
  RvtFileInfo.RevitAddin.dll, .deps.json, RvtFileInfo.Store.dll, RvtFileInfo.Store.Net.dll
%ProgramData%\RvtFileInfo\picklists.json   # Component NeverOverwrite=yes
HKLM\SOFTWARE\RvtFileInfo.Addin\InstallDir = [ADDINFOLDER]
```

Manifests:
```
%ProgramFiles%\RvtFileInfo\Manifests\RvtFileInfo.Setup.exe
→ writes %ProgramData%\Autodesk\Revit\Addins\<year>\RvtFileInfo.addin
```

Logs: `%ProgramData%\RvtFileInfo\logs\setup.log` and `addin.log`.

## Explorer install (`shell install [--restart 0|1]`)
1. Refuse if not Windows 11 x64 build ≥ 22000.
2. Backup previous property handlers per extension.
3. Register COM, PropertyHandlers, Approved, schema, FullDetails/PreviewDetails/InfoTip (extension + SFA + ProgIDs).
4. `SHChangeNotify(SHCNE_ASSOCCHANGED)`. Restart Explorer only if `--restart 1`.
5. Do **not** write `.addin` files. Do **not** delete `SOFTWARE\RvtFileInfo.Manifests`.

UI: Restart Explorer checkbox. Default `RESTARTEXPLORER=1` in UI (`DefaultRestart` after AppSearch); silent default `0` unless property set.

## Manifests install (`manifests install [--allyears 0|1] [--addindir PATH]`)
`ADDINDIR` from MSI property, else registry `SOFTWARE\RvtFileInfo.Addin\InstallDir`, else `Program Files\RvtFileInfo\RevitAddin`. Skip year if DLL missing. Uninstall deletes listed `.addin` files and `SOFTWARE\RvtFileInfo.Manifests`.

## Public properties
| Property | Package | Silent default | Meaning |
|----------|---------|----------------|---------|
| `RESTARTEXPLORER` | Explorer | `0` | `1` restart Explorer |
| `INSTALLALLYEARS` | Manifests | `0` | `1` = 2025–2027 (if DLLs exist) |
| `ADDINDIR` | Manifests | registry / Program Files | folder containing `2025` `2026` `2027` |

There is **no** `INSTALLADDIN`. Add-in is its own MSI.

## Uninstall order
Manifests → add-in → Explorer. Explorer uninstall restores handlers, unregisters schema, deletes `SOFTWARE\RvtFileInfo`, leaves streams in documents.

## Build
`.\build.ps1` → three MSIs + `dist\SHA256SUMS.txt`. Sign if `SIGN_CERT_THUMBPRINT` is set.

## Silent
```
msiexec /i RvtFileInfo-1.2.0-x64.msi /qn RESTARTEXPLORER=1
msiexec /i RvtFileInfo.RevitAddin-1.2.0-x64.msi /qn
msiexec /i RvtFileInfo.Manifests-1.2.0-x64.msi /qn
```

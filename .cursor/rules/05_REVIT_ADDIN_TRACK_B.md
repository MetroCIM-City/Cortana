# 05 — Revit Add-in (Track B — shipping)

Always build this. Track A was not chosen.

## Targets
| Revit | Configuration | TFM | Output |
|-------|---------------|-----|--------|
| 2025 | `R25` | `net8.0-windows` | `src/RevitAddin/bin/R25/` |
| 2026 | `R26` | `net8.0-windows` | `src/RevitAddin/bin/R26/` |
| 2027 | `R27` | `net10.0-windows` | `src/RevitAddin/bin/R27/` |

NuGet Nice3point Revit API. `Private=false` / no Revit DLLs in output. Store.Net: net8 for 2025/2026, `RvtFileInfo.Store.Net10` for 2027. Ship `RvtFileInfo.Store.dll` next to each year’s add-in DLL.

## Layout (as in the repo)
```
src/RevitAddin/
  RevitAddin.csproj
  App.cs                         IExternalApplication
  Commands/EditFileInfoCommand.cs
  Services/ProjectInfoParameters.cs
  Services/FileStoreSync.cs
  UI/FileInfoWindow.cs           WPF in code (not XAML)
  AddinLog.cs                    %ProgramData%\RvtFileInfo\logs\addin.log
  Resources/icon16.png, icon32.png
```

Manifests are **not** in this project. The Manifests MSI runs `RvtFileInfo.Setup.exe manifests install`.

## App
- Ribbon tab **File Info**, panel **Metadata**, button **Edit File Info**.
- Events on `ControlledApplication`: `DocumentOpened`, `DocumentSaved`, `DocumentSavedAs`, `DocumentSynchronizedWithCentral`. Unregister in `OnShutdown`.
- No Revit API on background threads. File write retries: `ExternalEvent` + timer, max 3, then TaskDialog and log.

## Project Information parameters (fixed GUIDs — do not regenerate)
| Name | GUID | Visible |
|------|------|---------|
| `RvtFileInfo_Discipline` | `{A11D0001-7C4E-4B2A-9F10-6E8D3C5B1A01}` | yes |
| `RvtFileInfo_Location` | `{A11D0002-7C4E-4B2A-9F10-6E8D3C5B1A02}` | yes |
| `RvtFileInfo_Originator` | `{A11D0003-7C4E-4B2A-9F10-6E8D3C5B1A03}` | yes |
| `RvtFileInfo_SubDiscipline` | `{A11D0004-7C4E-4B2A-9F10-6E8D3C5B1A04}` | yes |
| `RvtFileInfo_DocumentType` | `{A11D0005-7C4E-4B2A-9F10-6E8D3C5B1A05}` | yes |
| `RvtFileInfo_Program` | `{A11D0006-7C4E-4B2A-9F10-6E8D3C5B1A06}` | yes |
| `RvtFileInfo_SubProgram` | `{A11D0007-7C4E-4B2A-9F10-6E8D3C5B1A07}` | yes |
| `RvtFileInfo_ModifiedUtc` | `{A11D0008-7C4E-4B2A-9F10-6E8D3C5B1A08}` | no |

Temporary shared-parameter file; never edit the user’s. `SpecTypeId.String.Text`, instance binding `OST_ProjectInformation`, `GroupTypeId.IdentityData`. Transaction `"RvtFileInfo: update"`. If Project Information is owned by another user, show the owner and skip.

Dialog labels must match Explorer display names. Pick lists: `%ProgramData%\RvtFileInfo\picklists.json`, editable ComboBox, free text allowed.

## `.addin` (written by Manifests installer)
Path: `%ProgramData%\Autodesk\Revit\Addins\<year>\RvtFileInfo.addin`

```xml
<?xml version="1.0" encoding="utf-8"?>
<RevitAddIns>
  <AddIn Type="Application">
    <Name>RvtFileInfo</Name>
    <Assembly>C:\Program Files\RvtFileInfo\RevitAddin\2025\RvtFileInfo.RevitAddin.dll</Assembly>
    <AddInId>{8E2B7C41-6A95-4D13-B8F0-3C5D9A1E7B24}</AddInId>
    <FullClassName>RvtFileInfo.RevitAddin.App</FullClassName>
    <VendorId>RFI</VendorId>
    <VendorDescription>RvtFileInfo</VendorDescription>
  </AddIn>
</RevitAddIns>
```

Same AddInId for 2025/2026/2027. Assembly path uses `--addindir` or `HKLM\SOFTWARE\RvtFileInfo.Addin\InstallDir`. Skip a year if that year’s DLL is missing. Default years: those with `Revit.exe` under Program Files or `D:\Program Files\Autodesk`. `INSTALLALLYEARS=1` / `--allyears 1` writes 2025–2027 when DLLs exist.

Manifest list is stored in `HKLM\SOFTWARE\RvtFileInfo.Manifests` (not under `SOFTWARE\RvtFileInfo`).

## Native store
C ABI `rfi_read` / `rfi_write`. `NativeLibrary.SetDllImportResolver` loads `RvtFileInfo.Store.dll` from the add-in folder.

## Tests
`tests/Addin.Tests` — sync decision only (no Revit process). In-Revit behaviour is **Not verified** unless a tester runs it. Do not start or kill Revit from the agent.

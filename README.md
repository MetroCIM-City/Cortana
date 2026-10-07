# RvtFileInfo

Windows 11 File Explorer extra properties for BIM files.

After install, these seven text fields appear on **Properties → Details**, in the Details pane, and as Explorer columns:

| Field | Canonical name |
| --- | --- |
| Discipline | `RvtFileInfo.Discipline` |
| Location | `RvtFileInfo.Location` |
| Originator | `RvtFileInfo.Originator` |
| Sub Discipline | `RvtFileInfo.SubDiscipline` |
| Document Type | `RvtFileInfo.DocumentType` |
| Program | `RvtFileInfo.Program` |
| Sub Program | `RvtFileInfo.SubProgram` |

Supported extensions: **`.rvt` `.rfa` `.dwg` `.nwd` `.nwf` `.nwc` `.pdf`**.

## Install

There are **three** per-machine x64 MSIs (Windows 11 build 22000+). Install as administrator, in this order if you use Revit:

| Package | File | What it does |
| --- | --- | --- |
| Explorer | `dist\RvtFileInfo-1.2.0-x64.msi` | Property handler, Details columns, `rvtinfo` |
| Revit add-in | `dist\RvtFileInfo.RevitAddin-1.2.0-x64.msi` | Add-in DLLs under `Program Files\RvtFileInfo\RevitAddin\<year>` |
| Revit manifests | `dist\RvtFileInfo.Manifests-1.2.0-x64.msi` | Writes `%ProgramData%\Autodesk\Revit\Addins\<year>\RvtFileInfo.addin` |

Install them separately:

1. **Explorer** — File Explorer Details only. No Revit.
2. **Revit add-in** — copies `RvtFileInfo.RevitAddin.dll` (and `RvtFileInfo.Store.dll`) for 2025, 2026, and 2027. Does **not** write `.addin` files, so Revit will not load the add-in yet.
3. **Revit manifests** — writes `%ProgramData%\Autodesk\Revit\Addins\<year>\RvtFileInfo.addin` pointing at the add-in folder. Does **not** copy DLLs. Install the add-in MSI first.

```text
msiexec /i RvtFileInfo-1.2.0-x64.msi /qn RESTARTEXPLORER=1
msiexec /i RvtFileInfo.RevitAddin-1.2.0-x64.msi /qn
msiexec /i RvtFileInfo.Manifests-1.2.0-x64.msi /qn
msiexec /i RvtFileInfo.Manifests-1.2.0-x64.msi /qn INSTALLALLYEARS=1
msiexec /i RvtFileInfo.Manifests-1.2.0-x64.msi /qn ADDINDIR="C:\Program Files\RvtFileInfo\RevitAddin"
```

| Property | Package | Default | Meaning |
| --- | --- | --- | --- |
| `RESTARTEXPLORER` | Explorer | `0` silent / `1` UI | Restart Explorer so Details appear immediately |
| `INSTALLALLYEARS` | Manifests | `0` | `0` = years that have `Revit.exe`; `1` = 2025, 2026, and 2027 |
| `ADDINDIR` | Manifests | `Program Files\RvtFileInfo\RevitAddin` | Folder that contains `2025`, `2026`, and `2027` |

If an older combined 1.0 / 1.1 MSI is installed, install Explorer 1.2.0 to replace it, then install the add-in and manifest packages.

## Use in File Explorer

1. Right-click a file → **Properties** → **Details**.
2. Click the **empty cell to the right of the name** (Discipline, Originator, …), type a value, **Apply**.
3. To show columns: in a folder, right-click a column header → **More…** → turn on the seven names.

Each value is at most 256 characters. Sorting and grouping work on the columns.

The **Custom** tab (Checked By, Client, Typist, …) is the old OLE list. These seven fields are on **Details**, not Custom.

## Where values are stored

| Files | Store |
| --- | --- |
| `.rvt` `.rfa` | `RvtFileInfo` stream inside the compound file. Survives copy, rename, and zip. |
| `.dwg` `.nwd` `.nwf` `.nwc` `.pdf` | NTFS alternate data stream `filename.ext:RvtFileInfo`. If ADS cannot be written, sidecar `filename.ext.fileinfo.json`. The original file bytes are not rewritten. Zip / email copies drop ADS. |

Cloud placeholders (OneDrive files not on disk) are read-only.

## Revit add-in

Ribbon: **File Info** → **Metadata** → **Edit File Info**.

Values are stored on Project Information. After Save, Save As, and Synchronize with Central, they are written back to the file stream. If Explorer wrote a newer timestamp, it is imported when the model opens.

Built for Revit **2025** and **2026** (`net8.0-windows`) and **2027** (`net10.0-windows`). Optional pick lists: `%ProgramData%\RvtFileInfo\picklists.json`.

## Command line

`C:\Program Files\RvtFileInfo\cli\rvtinfo.exe`

```text
rvtinfo get "C:\path\file.rvt"
rvtinfo set "C:\path\file.rvt" Discipline=ARC Location=L01 Originator=ABC "Sub Discipline"=Facade "Document Type"=Model Program=Housing "Sub Program"=Phase1
rvtinfo dump "C:\path\file.rvt" --streams
```

Same commands work with `.rfa`, `.dwg`, `.nwd`, `.nwf`, `.nwc`, and `.pdf`.

## Uninstall

Uninstall each package from Settings → Apps, or:

```text
msiexec /x RvtFileInfo.Manifests-1.2.0-x64.msi /qn
msiexec /x RvtFileInfo.RevitAddin-1.2.0-x64.msi /qn
msiexec /x RvtFileInfo-1.2.0-x64.msi /qn
```

Removing manifests unloads the add-in from Revit and leaves the DLLs. Removing the add-in removes the DLLs. Removing Explorer restores a previous property handler (DWG/PDF) if one was backed up. Thumbnails are not changed. Streams already written into documents stay.

## Troubleshooting

| Symptom | What to do |
| --- | --- |
| Details missing on DWG / RFA / NWD / PDF | Install Explorer **1.2.0**. Restart Explorer. |
| Revit has no File Info tab | Install the add-in MSI, then the manifest MSI. |
| Names show but you cannot type | Click the **value** cell, not the name. Close the file in Revit, AutoCAD, Navisworks, or the PDF viewer. Restart Explorer after upgrade. |
| Columns missing after install | Restart Explorer, or sign out. Silent install needs `RESTARTEXPLORER=1`. |
| Values vanish after Revit Save | Load the add-in (File Info tab). Without it, Revit can drop the Explorer stream. |
| Search `Discipline:ARC` finds nothing | Rebuild the Windows Search index. Columns and Details do not depend on Search. |
| Thumbnails changed | They should not. Dump `HKCR\.rvt\shellex` — see `docs\SUPPORT.md`. |

Logs:

- `%ProgramData%\RvtFileInfo\logs\setup.log`
- `%ProgramData%\RvtFileInfo\logs\addin.log`

## Build from source

```text
.\build.ps1
```

Output: three MSIs in `dist\` plus `SHA256SUMS.txt`.

Needs MSVC (ATL, Windows SDK), .NET 8 SDK (2025/2026 add-in and Setup), .NET 10 SDK (2027 add-in), WiX Toolset 5.

## License

MIT. Copyright (c) 2026 Metropolitan CIM.

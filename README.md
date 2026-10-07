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

Use **version 1.1.0**. Version 1.0.0 only registered `.rvt`.

1. Close File Explorer property windows and any open Revit / AutoCAD / Navisworks / PDF files you care about.
2. Right-click `dist\RvtFileInfo-1.1.0-x64.msi` and install as administrator.
3. Leave **Restart File Explorer** checked (default in the UI).
4. Confirm the Revit add-in option if you use Revit 2025–2027.

Silent install:

```text
msiexec /i RvtFileInfo-1.1.0-x64.msi /qn RESTARTEXPLORER=1
msiexec /i RvtFileInfo-1.1.0-x64.msi /qn INSTALLADDIN=0
msiexec /i RvtFileInfo-1.1.0-x64.msi /qn INSTALLALLYEARS=1
```

| Property | Default | Meaning |
| --- | --- | --- |
| `INSTALLADDIN` | `1` | Install the Revit add-in |
| `INSTALLALLYEARS` | `0` | `0` = manifest only for years that have `Revit.exe`; `1` = 2025, 2026, and 2027 |
| `RESTARTEXPLORER` | `0` (silent) / `1` (UI) | Restart Explorer so Details appear immediately |

Requires Windows 11 x64 (build 22000 or later). Per-machine install.

If 1.0.0 is already installed, install 1.1.0 on top of it. Do not keep using `RvtFileInfo-1.0.0-x64.msi`.

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

Settings → Apps → **RvtFileInfo**, or:

```text
msiexec /x RvtFileInfo-1.1.0-x64.msi /qn
```

Uninstall restores a previous Explorer property handler (for example DWG or PDF) if one was backed up. Autodesk thumbnail handlers are not changed. Streams and sidecar files already written into documents are left in place.

## Troubleshooting

| Symptom | What to do |
| --- | --- |
| Details missing on DWG / RFA / NWD / PDF | Install **1.1.0**, not 1.0.0. Restart Explorer. |
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

Output: `dist\RvtFileInfo-<version>-x64.msi` and `dist\SHA256SUMS.txt`.

Needs MSVC (ATL, Windows SDK), .NET 8 SDK (2025/2026 add-in and Setup), .NET 10 SDK (2027 add-in), WiX Toolset 5.

## License

MIT. Copyright (c) 2026 Metropolitan CIM.

# 01 — Requirements

## Goal
Seven custom text fields visible and editable in Windows 11 File Explorer for **`.rvt` `.rfa` `.dwg` `.nwd` `.nwf` `.nwc` `.pdf`**, searchable/sortable/groupable. Explorer, Revit add-in binaries, and Revit `.addin` registration are **three separate installers**.

## Functional requirements
- **FR1** Explorer "More…" lists: Discipline, Location, Originator, Sub Discipline, Document Type, Program, Sub Program.
- **FR2** Same values in the Details pane, **Properties → Details**, and InfoTip. Not the **Custom** tab (OLE Summary Information).
- **FR3** Editable in Details / Properties → Details when the file is not locked and is not a cloud placeholder. Locked: sharing violation, no hang, no partial write. Cloud placeholder (`FILE_ATTRIBUTE_RECALL_ON_*`): read-only.
- **FR4** Sortable / groupable; searchable (`Discipline:ARC`) after the Windows Search index is rebuilt. Indexing is not required for columns or Details.
- **FR5** Survival:
  - `.rvt` `.rfa`: S1 stream inside CFB — copy, rename, zip.
  - `.dwg` `.nwd` `.nwf` `.nwc` `.pdf`: appended OLE trailer — copy, USB (any filesystem), zip, email. Native Save in AutoCAD / Navisworks / Acrobat can drop the trailer.
  - Revit Save / Save As / Sync: add-in rewrites S1 after those events. Without the add-in loaded, Revit may drop the stream (UNTESTED; treat as risk).
- **FR6** Free text, max 256 chars, control characters stripped, UTF-8 JSON payload ≤ 4 KB. Optional pick-lists: `%ProgramData%\RvtFileInfo\picklists.json` (add-in MSI; `NeverOverwrite`).
- **FR7** Do not break Autodesk thumbnails, preview, Title/Author, double-click, or native app open. Do not wrap DWG/PDF/Navisworks as a leading OLE compound file (that changes magic bytes and breaks those apps).
- **FR8** Track B: Revit 2025, 2026, 2027 ribbon **File Info → Metadata → Edit File Info**. Values on Project Information; mirrored to the file store on Save / Save As / Sync.
- **FR9** Uninstall each MSI independently. Manifests first unloads Revit. Add-in removes DLLs. Explorer restores backed-up property handlers (DWG/PDF) and unregisters the schema. Do not wipe `SOFTWARE\RvtFileInfo.Manifests` when uninstalling Explorer (`SOFTWARE\RvtFileInfo` only).

## Non-functional
- Windows 11 x64, build ≥ 22000. Per-machine. Silent `msiexec /qn`.
- Handler read of a 500 MB CFB: under 50 ms; do not load the whole file.
- No telemetry, no network.
- COM boundary catches all C++ exceptions; never abort the host.
- UTF-8 on disk, UTF-16 in the property system.
- Repair/upgrade idempotent. Reboot not required. Explorer restart is opt-in (`RESTARTEXPLORER`).

## Constraints
- `.rvt` / `.rfa` are CFB (`D0 CF 11 E0 …`). Store decision is **magic-byte**, not extension: `IsCompoundFile` → S1.
- Non-CFB hosts get a trailer CFB + 16-byte footer (`RFICFB01`); host prefix bytes must hash-match.
- Explorer columns use the Windows Property System (`.propdesc` + handler). `IColumnProvider` is obsolete.
- Handler is native C++ x64 (`/sdl`, `/GS`, `/guard:cf`). Not managed, not in-proc isolation disabled.
- Revit 2025/2026: `net8.0-windows`. Revit 2027: `net10.0-windows`.

## Out of scope
- Revit Cloud / ACC / BIM 360 / Revit Server metadata.
- `.rte` / `.rft` unless added as a registry + handler extension list change.
- macOS/Linux.
- Turning DWG/PDF/NWD into leading OLE documents.
- Combining the three MSIs back into one product.

## Acceptance
1. Explorer MSI → Details for all seven extensions after Explorer restart.
2. Edit Details → persist; `.rvt` still opens in Revit (when available).
3. Copy a `.dwg`/`.pdf` after write → Details still readable on the copy without ADS/sidecar.
4. Add-in MSI then manifests MSI → File Info tab; uninstall manifests leaves DLLs.
5. Automated tests in file 07 pass. Anything not run is **Not verified**.

# 01 — Requirements

## Goal
Make seven custom text fields visible and editable for `.rvt` files in Windows 11 File Explorer, searchable, sortable and groupable, installed via a single installer.

## Functional requirements
- **FR1** Explorer "More…" column chooser lists: Discipline, Location, Originator, Sub Discipline, Document Type, Program, Sub Program.
- **FR2** The same values show in the **Details pane**, the **Properties > Details** tab, and tooltips (InfoTip) for `.rvt`.
- **FR3** Values are **editable** in Details pane / Properties > Details (writable property handler), when the file is not locked by Revit. If locked, show read-only and fail gracefully (no hang, no crash).
- **FR4** Values are **sortable / groupable** by column; **searchable** (`Discipline:Arch` in Explorer search once indexed).
- **FR5** Values survive: copy, move, rename, zip → unzip (if embedded), OneDrive/SharePoint sync (if embedded), and **Revit Save, Save As, Save as Central, Synchronize with Central** (verified by gates — see file 02).
- **FR6** All seven values are free **text** (max 256 chars each). Optional pick-lists (suggestions only) are loaded from `%ProgramData%\RvtFileInfo\picklists.json`.
- **FR7** Does **not** break existing Autodesk behaviour: thumbnails, preview, Title/Author properties for `.rvt`, Revit opening files, double-click.
- **FR8** Track B only: Revit 2025, 2026, 2027 ribbon button "File Info" to edit the seven values for the open model; values stored in the model (Project Information) and mirrored to the file store on save.
- **FR9** Clean uninstall removes all registrations, schemas, add-in manifests; restores any backed-up `.rvt` handler.

## Non-functional requirements
- Windows 11 (22H2+), x64. Per-machine install (admin/UAC). Silent install supported (`msiexec /i ... /qn`).
- Handler read time < 50 ms for a 500 MB `.rvt`; must read only the CFB directory + one small stream (never load the file into memory).
- No telemetry, no network access.
- Handler must be crash-safe: all exceptions caught at the COM boundary; return `E_FAIL`-class HRESULTs, never terminate the host.
- Unicode safe (UTF-8 storage, UTF-16 in the property system).
- Idempotent install/repair; reboot not required (Explorer restart allowed).

## Constraints / facts to respect
- `.rvt` files are OLE **Compound File Binary (CFB)** containers (streams such as `BasicFileInfo`, `Global\...`). Verify on real sample files; large files may use 4096-byte sectors (CFB v4).
- Explorer **column handlers (IColumnProvider) are obsolete**; use the **Windows Property System** (property schema `.propdesc` + property handler).
- Shell extensions must not be written in managed code (in-proc in Explorer / prophost).
- Revit 2025 and 2026 target **net8.0-windows**; Revit 2027 targets **net10.0-windows** (verify against installed SDK/NuGet before building).

## Out of scope
- Revit Cloud / ACC / BIM 360 server-side metadata, Revit Server.
- `.rfa` / `.rte` / `.rft` (design the code so adding extensions later is a registry-only change).
- macOS/Linux.

## Acceptance criteria (summary)
1. Fresh Windows 11 VM: run installer → open Explorer in a folder with `.rvt` → add the 7 columns → values display for a prepared test file.
2. Edit a value in Details pane → close/reopen Explorer → value persists; the `.rvt` still opens in Revit with no repair prompt (if Revit available).
3. Gate results documented; Track decision justified.
4. Uninstall leaves no `RvtFileInfo` keys, files, or schema.
5. All automated tests in file 07 pass.

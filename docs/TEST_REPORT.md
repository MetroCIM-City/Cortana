# RvtFileInfo test report

Environment: Windows 11 build 26200, x64. MSVC 14.51 (platform toolset v145), Windows SDK 10.0.22621.0, .NET SDK 8.0.425 and 10.0.400-preview, WiX Toolset 5.0.2. The session that ran these tests was not elevated.

Track: B. See [decisions/ADR-001-track-decision.md](decisions/ADR-001-track-decision.md).

## Automated

| Suite | Result |
|-------|--------|
| `Store.Tests` | PASS, 0 failed. Unicode round-trip (Finnish, Arabic, CJK, emoji), 256-character limit, control-character stripping, other streams byte-identical, failpoint leaves the original file, locked file returns `0x80070020`, corrupt input fails closed, ADS and sidecar round-trip, copy/rename keeps the `RvtFileInfo` stream. |
| `Store.Tests --perf` | PASS. Worst of 20 reads of a compound file with a 500 MB `Bulk` stream: **0.40 ms** (limit 50 ms). |
| `ShellHandler.Tests` | PASS, 0 failed. `IPropertyStore` read/write/commit, read-only stream rejects `SetValue` with access denied, and a mock delegate returns `System.Title` = `FromMock` while Discipline stays on the file store. |
| `Addin.Tests` | Passed 4, failed 0. File-newer imports, equal timestamps keep the model, empty model timestamp imports, empty file timestamp skips. |
| `rvtinfo` | PASS on a temp compound file: `set` of all seven fields, `get` read-back, `dump --streams` lists `RvtFileInfo`. |

`PSRegisterPropertySchema` returned `0x80070005` (`ERROR_ACCESS_DENIED`). `PSGetPropertyDescriptionByName(L"RvtFileInfo.SubDiscipline")` was not called. The schema was checked against the Windows property description schema: no `description` element, and `canBePurged="false"` is omitted because a writable property cannot set that flag.

## Phase 0 gates

| Gate | Result |
|------|--------|
| G1 Unicode round-trip | PASS for S1 (synthetic compound file and a temp copy of a 1.2 MB `.rvt`, 8 original streams kept), S2, and S3. |
| G2 Open in Revit | UNTESTED. Revit 2027 was already running and was not closed. Revit 2025 has no `Revit.exe`. Revit 2026 is not installed. |
| G3 Save | UNTESTED, same reason. |
| G4 Save As | UNTESTED. |
| G5 Sync with Central | UNTESTED. |
| G6 Copy, rename, zip | PASS for S1. S2 survives an NTFS copy and is dropped by zip. S3 is a separate file. OneDrive copy was not run. |
| G7 Edit while Revit has the file open | Explorer UI UNTESTED. The store returns sharing violation; `Store.Tests` covers that path. |
| G8 Thumbnail / preview | UNTESTED visually. The installer does not write the thumbnail shellex key. |

## Not verified

- Properties > Details, the Details pane, column chooser (More…), sort, and Group by in Explorer.
- `SHGetPropertyStoreFromParsingName` and `IShellItem2` after COM registration.
- Property schema registration and the display name "Sub Discipline" from `PSGetPropertyDescriptionByName`.
- MSI install, repair, upgrade, and uninstall, including `tests/InstallerDiff.ps1` before/after. The package was built; it was not installed.
- A folder of 1000 files, and Windows Search (`Discipline:ARC`).
- Locked-by-Revit behavior in the Explorer UI.
- Thumbnails and preview, visually.
- Explorer Details editing for `.rfa`, `.dwg`, `.nwd`, `.nwf`, `.nwc`, and `.pdf`. Unit tests cover a dummy DWG via ADS; the MSI was not reinstalled in Explorer after adding those extensions.
- Revit 2027 open, save, save as, sync, import of a newer file store, and close without save. The add-in is built for `net10.0-windows` and was not loaded in Revit.
- Revit 2025 runtime. The add-in is built for `net8.0-windows`. `Revit.exe` is not present.
- Revit 2026 runtime. The add-in is built for `net8.0-windows` and was not run inside Revit.
- OneDrive or cloud-placeholder copy.

## Known limitations

- Values written only by Explorer stay on the file if Revit preserves the `RvtFileInfo` stream. That was not measured. The add-in writes the stream again after Save, Save As, and Sync with Central.
- S1 travels with copy, rename, and zip. S2 and S3 are not the shipping write path.
- Windows Search may need a re-index, and `.rvt` has no persistent handler, so `Discipline:ARC` may not hit these properties. Columns and Properties > Details do not depend on that index.
- Cloud placeholders are read-only.
- The 2027 add-in is built against the installed Revit 2027 API target (`net10.0-windows`). It was not run inside Revit 2027.

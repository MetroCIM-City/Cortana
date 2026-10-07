# 07 — Testing & Verification

## Pyramid
1. Unit: Store.Core (JSON, S1, S4, S2/S3, locks, failpoint, corrupt).
2. Integration: COM handler host (`ShellHandler.Tests`); installer diff script exists (`tests/InstallerDiff.ps1`) — MSI install on a VM is still **Not verified** unless run.
3. Manual: Explorer + Revit — record in `docs/TEST_REPORT.md`. Do not claim pass if not run.

## Fixtures
Do not commit real `.rvt` / client files. Synthetic CFB in tests (`CreateSample`). Dummy DWG `AC1032…`, dummy PDF `%PDF-…`. `tests/Fixtures/local.json` is git-ignored.

## Unit tests that must keep passing
- Seven-field Unicode round-trip (Finnish, Arabic, CJK, emoji), empty, 256 limit, oversize reject, controls stripped, unknown JSON keys kept.
- S1 write: every other stream hash-identical.
- Failpoint before `ReplaceFileW` leaves the original file.
- Locked file → `ERROR_SHARING_VIOLATION`.
- Empty / non-CFB text: `ReadFile` S_OK, `found=false`. Truncated CFB magic: fail closed.
- S2/S3 direct APIs round-trip.
- **S4:** dummy `.dwg` and `.pdf` `WriteFile` → not `IsCompoundFile`; host prefix unchanged; `CopyFile` still `ReadFile`s values without ADS/sidecar; second write does not grow the host prefix.

## Handler tests
`IPropertyStore` read/write/commit on a CFB and a dummy DWG via `IInitializeWithFile`. Isolated stream init. Mock inner handler for `System.Title`. Schema registration may return `0x80070005` without elevation — do not treat that as a store failure.

## Add-in tests
`Addin.Tests`: file-newer imports, equal timestamps keep model, empty model timestamp imports, empty file timestamp skips. Four facts, no Revit host.

## Commands
```
.\build.ps1
.\build.ps1 -SkipNative
.\build.ps1 -SkipInstaller
rvtinfo get  "C:\path\file.rvt"
rvtinfo set  "C:\path\file.dwg" Discipline=ARC Location=L01 Originator=ABC "Sub Discipline"=Facade "Document Type"=Model Program=Housing "Sub Program"=Phase1
rvtinfo dump "C:\path\file.rvt" --streams
```

`dump --streams` lists CFB streams for RVT/RFA; for a trailer file it reports `RvtFileInfo` as trailer.

## Manual Explorer (when a human runs it)
- [ ] More… shows all seven names on each supported extension.
- [ ] Details pane and Properties → Details edit (click the **value** cell).
- [ ] Sort / Group by.
- [ ] Copy DWG/PDF to USB / another folder → Details remain (handler installed on that PC).
- [ ] Thumbnails unchanged.

## Manual Revit (when a human runs it; agent must not kill Revit)
- [ ] Add-in MSI then manifests MSI → File Info tab.
- [ ] Edit → Save → Explorer.
- [ ] Save As / Sync.
- [ ] Uninstall manifests → Revit no longer loads the add-in; DLLs remain.

## Reporting
Every skipped test goes under **Not verified** in `docs/TEST_REPORT.md`. Never mark G2–G5/G8 as passed without in-Revit evidence.

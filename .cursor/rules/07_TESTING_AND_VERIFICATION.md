# 07 — Testing & Verification

## Test pyramid
1. **Unit (automated)** — Store.Core, JSON codec, validation, CFB reader/writer.
2. **Integration (automated, on Windows runner/VM)** — COM handler via `IPropertyStore` from a test host; registration/unregistration; installer diff.
3. **Manual (checklist)** — Explorer UI, Revit 2025/2026/2027.

## Fixtures
- Do **not** commit real client `.rvt` files. Provide `tests/Fixtures/make_fixtures.ps1` / `.py` that generates **synthetic CFB files** resembling `.rvt` (streams `BasicFileInfo`, `Global\Latest` with random data, ~1 MB, ~600 MB sparse for perf).
- If the user supplies real sample `.rvt` files, store paths in `tests/Fixtures/local.json` (git-ignored) and run extra tests on **copies**.
- Corrupt-file fixtures: truncated, cyclic FAT chain, oversized directory, zero-length, non-CFB.

## Unit tests (must exist)
- Round-trip all 7 values, Unicode (Finnish ä/ö, Arabic, CJK, emoji), empty, 256-char limit, >256 rejection, control chars stripped.
- Writing our stream leaves **every other stream byte-identical** (hash compare).
- Interrupted write (kill mid-commit) leaves original file intact.
- Locked file → `ERROR_SHARING_VIOLATION`, no partial write.
- Corrupt fixtures → clean failure, no crash/leak (run under Application Verifier / ASan build).

## Integration tests
- Host `CoCreateInstance(CLSID)` → `IInitializeWithStream` → read/write via `IPropertyStore`; also via `SHGetPropertyStoreFromParsingName` and `IShellItem2::GetString(PKEY)`.
- Property schema: `PSGetPropertyDescriptionByName(L"RvtFileInfo.SubDiscipline")` returns the display name "Sub Discipline".
- Coexistence: if a mock "original" handler is registered, non-RvtFileInfo keys are forwarded; thumbnails unaffected.
- Installer: capture registry + file listing before/after install/uninstall (`tests/InstallerDiff.ps1`); diff must be empty after uninstall.
- Performance: read of 500 MB synthetic file < 50 ms (p95), 1000 files in a folder listing without UI stall.

## Manual checklist (Explorer)
- [ ] Folder view → right-click column header → **More…** → all 7 names present.
- [ ] Values appear; sorting and "Group by" work.
- [ ] Details pane shows & edits values; Properties > Details tab edits and saves.
- [ ] File open in Revit → edit attempt shows a friendly read-only message, no hang.
- [ ] Search box: `Discipline:ARC` returns the file after indexing (if indexing implemented).
- [ ] Thumbnails/preview of `.rvt` unchanged.

## Manual checklist (Revit, each installed year 2025 / 2026 / 2027) — Track B
- [ ] Add-in loads; ribbon button visible; no errors in journal.
- [ ] Edit File Info → values saved to Project Information.
- [ ] Save → Explorer shows values within 5 s of refresh.
- [ ] Save As → new file has values; original unchanged.
- [ ] Workshared: create central, make local, edit values, Synchronize → local file shows values.
- [ ] Open a file whose Explorer-edited values are newer → imported into model.
- [ ] Close without saving → file store unchanged.
- [ ] Uninstall add-in → Revit starts cleanly; model parameters remain (documented).

## Commands the agent should provide
```powershell
./build.ps1 -Configuration Release        # build + unit tests + package
./build.ps1 -Test Integration             # needs admin + Windows 11
rvtinfo get  "C:\path\file.rvt"
rvtinfo set  "C:\path\file.rvt" Discipline=ARC Location=L01 Originator=ABC "Sub Discipline"=Facade "Document Type"=Model Program=Housing "Sub Program"=Phase1
rvtinfo dump "C:\path\file.rvt" --streams
```

## Reporting
Record every executed test with result and environment. Anything not run (e.g. Revit not available) must be listed under **Not verified** — never claim it passed.

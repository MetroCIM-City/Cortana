# 02 — Track decision (locked)

Phase 0 is done. Do not re-run the spike as a prerequisite for shipping changes. Results: `docs/decisions/ADR-001-track-decision.md`. Registry dump: `docs/decisions/rvt-registry-baseline.txt`.

## Decision
**Track B.** Mandatory Revit gates G2, G3, G4, G5, and G8 were UNTESTED (Revit 2027 was already running and was not closed; 2025 has no `Revit.exe`; 2026 is not installed). UNTESTED counts as fail.

Whether Revit drops an unknown `RvtFileInfo` stream on Save was **not measured**. Track B does not depend on that: the add-in stores values on Project Information and writes the file store after Save, Save As, and Sync with Central.

## Stores

| ID | What | Shipping write | Shipping read |
|----|------|----------------|---------------|
| **S1** | Root CFB stream `RvtFileInfo` | Yes, when file magic is CFB (`.rvt` `.rfa`) | First if CFB |
| **S4** | Appended OLE CFB + footer `RFICFB01` after host bytes | Yes, when not CFB (`.dwg` `.nwd` `.nwf` `.nwc` `.pdf`) | After S1 miss / for non-CFB |
| **S2** | NTFS ADS `file:RvtFileInfo` | No (legacy). Deleted after a successful S4 write | After S4 miss |
| **S3** | Sidecar `file.fileinfo.json` | No (legacy). Deleted after a successful S4 write | After S2 miss |

S2/S3 remain in `Store.Core` (`ReadAds`/`WriteAds`/`ReadSidecar`/`WriteSidecar`) for tests and old files. `WriteFile` does not use them for new writes.

## S4 rules (do not violate)
- Do **not** call `StgOpenStorage` on the whole DWG/PDF/NWD — those are not compound files.
- Do **not** wrap the host as a leading OLE package (would change the first 8 bytes).
- Host prefix (bytes before the trailer) must be byte-identical after write (SHA-256).
- Footer (little-endian, packed 16 bytes): `uint32 cfbSize`, `uint32 reserved=0`, magic 8 bytes `'R','F','I','C','F','B','0','1'`. CFB blob size 512–65536. CFB at `hostSize` must start with `D0 CF 11 E0 …`.
- Rewrite strips a previous trailer, then appends a new CFB + footer (host size stays stable).

## Gates (historical; ADR-001)
| Gate | S1 | S2 | S3 |
|------|----|----|-----|
| G1 Unicode | PASS | PASS | PASS |
| G2 Open in Revit | UNTESTED | UNTESTED | UNTESTED |
| G3 Save | UNTESTED | UNTESTED | UNTESTED |
| G4 Save As | UNTESTED | UNTESTED | UNTESTED |
| G5 Sync | UNTESTED | UNTESTED | UNTESTED |
| G6 Copy/rename/zip | PASS (incl. zip for S1) | NTFS copy yes; zip drops | Sidecar separate |
| G7 Edit while Revit open | Store: sharing violation | n/a | n/a |
| G8 Thumbnail | Installer does not write thumbnail shellex | n/a | n/a |

S4 G6 (copy of dummy DWG/PDF) is covered by `Store.Tests` (`TestNonCompoundWrite`). Zip of real DWG/PDF with trailer: not re-run as a Revit gate.

## Baseline
`.rvt` had **no** property handler and **no** PersistentHandler. Thumbnail CLSID `{2E559A13-E91E-4DEB-8996-3B594FC11AC4}` (`Revit.Thumbnail20.dll`) must remain. Do not register `HKCR\.rvt\PersistentHandler` unless a later ADR says so (Search columns work without it).

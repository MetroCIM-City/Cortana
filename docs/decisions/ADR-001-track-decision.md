# ADR-001: Track B, primary store S1

## Status

Accepted. Track B.

## Environment

- Windows 11 build 26200 (registry ProductName still reports Windows 10 Pro, DisplayVersion 25H2).
- Revit 2027 is installed at `C:\Program Files\Autodesk\Revit 2027`. `Revit.exe` has been running since 2026-10-03 as process 3040. That session was not closed.
- `D:\Program Files\Autodesk\Revit 2025` exists and does not contain `Revit.exe` or `RevitAPI.dll`.
- Revit 2026 is not installed.
- Shell baseline: [rvt-registry-baseline.txt](rvt-registry-baseline.txt). No property handler and no persistent handler are registered for `.rvt`. The thumbnail extractor is Autodesk `Revit.Thumbnail20.dll`.

## Method

Prototype writer: `spike/CfbSpike` (OpenMcdf, throwaway). It writes a UTF-8 `RvtFileInfo` stream into a compound file. Real-file tests used a temp copy of `Construction_Ceilling_Knauf_D152-DE_wood-beamed-ceiling-system_metal-grid-CD-60x27_2016.rvt`. The Desktop original was not modified. Results were printed by `spike/CfbSpike.exe` and `spike/run-revit-gate.ps1`.

## Results

| Gate | S1 extra CFB stream | S2 NTFS ADS `file.rvt:RvtFileInfo` | S3 sidecar `file.rvt.fileinfo.json` |
|------|---------------------|------------------------------------|-------------------------------------|
| G1 Unicode round-trip | PASS on a synthetic compound file and on the temp `.rvt` copy (8 original streams kept) | PASS read-back of `Architectural` / `L01` | PASS file written beside the copy |
| G2 Open in Revit | UNTESTED. Revit 2027 was already running and was not interrupted. Revit 2025 and 2026 cannot be launched. | UNTESTED | UNTESTED |
| G3 Save | UNTESTED, same reason | UNTESTED | UNTESTED |
| G4 Save As | UNTESTED | UNTESTED | UNTESTED |
| G5 Sync with Central | UNTESTED | UNTESTED | UNTESTED |
| G6 Copy, rename, zip | PASS. Copy, rename, zip, and unzip kept the stream on the synthetic file and on the temp `.rvt` copy. OneDrive copy was not run. | Copy on the same NTFS volume kept the stream. Zip/unzip dropped it. | The sidecar is a separate file. A manual copy of the sidecar works. Zip of the `.rvt` alone does not include it. |
| G7 Edit while Revit has the file open | UNTESTED in Explorer. The store returns sharing-violation when the file cannot be opened exclusively; that path is covered by `Store.Tests` once the native build exists. | n/a | n/a |
| G8 Thumbnail / preview | UNTESTED visually. The installer does not write the thumbnail shellex key `{BB2E617C-0920-11d1-9A0B-00C04FC2D6C1}`. | n/a | n/a |

The journal attempt in `spike/run-revit-gate.ps1` did not start Revit, because `Revit.exe` was already running.

## Decision

Mandatory gates G2, G3, G4, G5, and G8 are UNTESTED. The Phase 0 rule treats UNTESTED as fail, so the product is **Track B**.

Whether Revit drops an unknown `RvtFileInfo` stream on Save was not measured. Track B does not depend on that measurement: the add-in stores the values on Project Information and writes the stream again after Save, Save As, and Sync with Central.

## Primary store

**S1**, the root stream `RvtFileInfo`, remains the store Explorer reads. It passed G1 and G6, so a copy, rename, or zip keeps the values. S2 and S3 are implemented behind the store API for tests and are not the shipping write path.

## Consequences

- Ship the shell property handler, schema, and installer.
- Ship the Revit add-in for 2025 (`net8.0-windows`), 2026 (`net8.0-windows`), and 2027 (`net10.0-windows`).
- 2026 is built and is not run inside Revit on this machine.
- 2025 is built and is not run inside Revit, because `Revit.exe` is absent.

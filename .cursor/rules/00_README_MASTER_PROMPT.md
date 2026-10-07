# MASTER PROMPT — RvtFileInfo (Windows 11 Explorer + Revit)

This product is **implemented**. These files are the spec the code must keep matching. Do not regenerate GUIDs, UpgradeCodes, or AddInIds. Do not recreate Phase 0. Do not kill a running Revit process.

## Role
Maintain the Windows 11 shell property handler and Track B Revit add-in that add seven **text** properties to BIM files in File Explorer (columns, Details pane, Properties → Details). Manufacturer: Metropolitan CIM. License: MIT (2026). Version: **1.2.0**. No telemetry. Sign binaries only when `SIGN_CERT_THUMBPRINT` is set.

## The seven properties (all Text / String)
| # | Display name   | Canonical name | FMTID PID |
|---|----------------|----------------|-----------|
| 1 | Discipline     | `RvtFileInfo.Discipline`     | 2 |
| 2 | Location       | `RvtFileInfo.Location`       | 3 |
| 3 | Originator     | `RvtFileInfo.Originator`     | 4 |
| 4 | Sub Discipline | `RvtFileInfo.SubDiscipline`  | 5 |
| 5 | Document Type  | `RvtFileInfo.DocumentType`   | 6 |
| 6 | Program        | `RvtFileInfo.Program`        | 7 |
| 7 | Sub Program    | `RvtFileInfo.SubProgram`     | 8 |

Group property `RvtFileInfo.FileInfo` (PID 100, innate, label **File Info**) exists only so Details can group the seven fields.

## Track (locked)
**Track B** — ADR-001: gates G2–G5/G8 were UNTESTED, so the Revit add-in ships. Primary file store is S1 for CFB files. Non-CFB types use an appended OLE trailer (not NTFS ADS as the write path).

## Supported extensions
`.rvt` `.rfa` `.dwg` `.nwd` `.nwf` `.nwc` `.pdf`

## Stable identifiers (do not change)
| Name | Value |
|------|--------|
| Handler CLSID | `{C4A91E72-5D38-4F0B-9E16-2B7A6C8D4E50}` |
| FMTID | `{6F3C2A91-8B14-4D5E-A7C2-19E4B8D07F31}` |
| Revit AddInId (all years) | `{8E2B7C41-6A95-4D13-B8F0-3C5D9A1E7B24}` |
| VendorId | `RFI` |
| Explorer MSI UpgradeCode | `{D1F4A8C3-2E67-4B90-8A15-6C3E9F0B2D47}` |
| Add-in MSI UpgradeCode | `{E2A5B9D4-3F78-4C01-9B26-7D4E0A1C3E58}` |
| Manifests MSI UpgradeCode | `{F3B6C0E5-4089-4D12-8C37-8E5F1B2D4F69}` |
| CFB / trailer stream name | `RvtFileInfo` |

## Packaging
**Three** per-machine x64 MSIs — not one combined installer:

| Package | File | Role |
|---------|------|------|
| Explorer | `dist\RvtFileInfo-1.2.0-x64.msi` | Handler, schema, Details, `rvtinfo` |
| Revit add-in | `dist\RvtFileInfo.RevitAddin-1.2.0-x64.msi` | DLLs under `Program Files\RvtFileInfo\RevitAddin\<year>` only |
| Revit manifests | `dist\RvtFileInfo.Manifests-1.2.0-x64.msi` | Writes `%ProgramData%\Autodesk\Revit\Addins\<year>\RvtFileInfo.addin` only |

## Reading order
| File | Purpose |
|------|---------|
| `01_REQUIREMENTS.md` | Goals, constraints, acceptance |
| `02_FEASIBILITY_AND_DECISION.md` | Locked Track B + store choice |
| `03_ARCHITECTURE.md` | Components, stores, layout |
| `04_SHELL_PROPERTY_HANDLER.md` | Handler + `.propdesc` as shipped |
| `05_REVIT_ADDIN_TRACK_B.md` | Add-in as shipped |
| `06_INSTALLER_WINDOWS11.md` | Three WiX packages + Setup.exe |
| `07_TESTING_AND_VERIFICATION.md` | Tests |
| `08_DELIVERABLES_AND_REPORT.md` | Done / reports |

## Operating rules
- **Never corrupt host files.** CFB: temp copy → transacted storage → hash all streams except `RvtFileInfo` → `ReplaceFileW`. Non-CFB: never rewrite leading magic; append/replace the OLE trailer only; hash host prefix before/after.
- **Never overwrite Autodesk thumbnail/preview** (`HKCR\.ext\shellex\{BB2E617C-…}`).
- **Never kill running Revit.**
- No real `.rvt` (or client BIM files) in git. Fixtures are synthetic or git-ignored copies.
- Shell handler is native C++ x64 only. .NET is for Setup, CLI, Store.Net, and the Revit add-in.
- One `build.ps1` builds, tests, and packages. Default `Version` is `1.2.0`.
- Details are on the **Details** tab, not OLE Custom. Editing needs `FullDetails` on the ProgID / SystemFileAssociations as well as `PropertyHandlers\.ext`.

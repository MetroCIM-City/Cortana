# 08 — Deliverables & Final Report

## Deliverables
1. Source in the layout from file 03.
2. Three MSIs in `dist\` plus `SHA256SUMS.txt` (binaries git-ignored; build locally).
3. `docs/decisions/ADR-001-track-decision.md` and `rvt-registry-baseline.txt`.
4. `README.md`: three packages, silent properties, Details vs Custom, S1 vs S4, uninstall order, troubleshooting.
5. `docs/SUPPORT.md`: logs, registry dump, `rvtinfo`, three installers.
6. `docs/TEST_REPORT.md` with **Not verified**.
7. Known limitations.

## Definition of done (maintenance)
- [ ] Do not regenerate committed GUIDs / UpgradeCodes / AddInId / parameter GUIDs.
- [ ] Seven properties on Explorer Details for `.rvt` `.rfa` `.dwg` `.nwd` `.nwf` `.nwc` `.pdf`.
- [ ] S4 trailer for non-CFB; S1 for CFB; no leading-OLE wrap of DWG/PDF/NWD.
- [ ] Three MSIs remain separate.
- [ ] Autodesk thumbnail shellex untouched.
- [ ] Unit tests pass; unrun Explorer/Revit UI listed as Not verified.
- [ ] No client files or secrets in git.

## Build report format
```
# RvtFileInfo — Build Report
Track: B (ADR-001, UNTESTED Revit gates)
Stores: S1 CFB stream; S4 appended OLE trailer; S2/S3 read fallback
Extensions: .rvt .rfa .dwg .nwd .nwf .nwc .pdf
Installers: Explorer / RevitAddin / Manifests 1.2.0 x64 + SHA256
Tests: Store.Tests / ShellHandler.Tests / Addin.Tests
Not verified: ...
Known limitations: ...
```

## Known limitations (keep honest)
- Revit Save without the add-in loaded may drop S1 (G3 UNTESTED).
- AutoCAD / Navisworks / Acrobat Save may drop the S4 trailer.
- Details appear on another PC only if that PC has the Explorer MSI.
- Cloud placeholders are read-only.
- Windows Search may need a re-index.
- `PSRegisterPropertySchema` needs elevation; tests may see `0x80070005`.
- Revit 2025/2026/2027 in-process behaviour is Not verified unless a tester runs it.

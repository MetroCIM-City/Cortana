# 08 — Deliverables & Final Report

## Deliverables
1. Source repository in the layout from file 03 (only the Track-relevant projects; remove unused ones).
2. `dist/RvtFileInfo-<version>-x64.msi` + `SHA256SUMS.txt`.
3. `docs/decisions/ADR-001-track-decision.md` (+ any other ADRs) and `rvt-registry-baseline.txt`.
4. `README.md` (user): install, silent install, using columns, editing values, uninstall, troubleshooting (Explorer restart, rebuild search index, "values missing after Revit save").
5. `docs/SUPPORT.md`: log locations, how to dump registry state, how to run `rvtinfo`.
6. Test results summary (`docs/TEST_REPORT.md`) with "Not verified" section.
7. Known limitations list.

## Definition of done
- [ ] Phase 0 gates executed or explicitly marked UNTESTED with reason; Track decision recorded.
- [ ] 7 properties visible in Explorer columns, Details pane, Properties > Details.
- [ ] Editable where safe; locked-file behaviour graceful.
- [ ] Existing `.rvt` Explorer behaviour unchanged (before/after property dump attached).
- [ ] Track B: add-in built for 2025, 2026, 2027 with correct target frameworks; manifests install correctly.
- [ ] Installer installs, repairs, upgrades, uninstalls cleanly (diff test).
- [ ] All unit + integration tests pass; manual checklist results recorded.
- [ ] No real client files in the repo; no secrets.

## Final report format (print at the end of the run)
```
# RVT File Info — Build Report
Track chosen: A | B   (reason in 1–2 lines)
Gate results: G1..G8 table
Components built: ...
Revit versions: 2025 [built/tested], 2026 [...], 2027 [...]
Installer: path, size, SHA256
Tests: passed/failed counts
Not verified: ...
Known limitations: ...
Next steps / recommendations: ...
```

## Likely limitations to state honestly
- Values set outside Revit are only durable if Revit preserves them (Phase 0 result).
- Cloud / zip portability depends on the chosen store (S1 portable; S2/S3 not).
- Windows Search indexing of custom properties may need a re-index after install.
- Revit 2027 API/target framework verified against the installed SDK at build time only.

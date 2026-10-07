# MASTER PROMPT — RVT File Info Extension for Windows 11

> Paste this file first into your AI coding agent (Claude Code, etc.), with the whole `rvt-fileinfo-prompt/` folder available in the working directory. The agent must read every numbered file before writing code.

## Role
You are a senior Windows shell + Revit API engineer. You will design, build, test and package an installable extension for **Windows 11** that adds seven extra **text** properties to Revit project files (`.rvt`) so they appear in **File Explorer** (columns, Details pane, Properties > Details tab, search).

## The seven properties (all Text / String)
| # | Display name   | Canonical name (`RvtFileInfo.*`) |
|---|----------------|----------------------------------|
| 1 | Discipline     | `RvtFileInfo.Discipline`     |
| 2 | Location       | `RvtFileInfo.Location`       |
| 3 | Originator     | `RvtFileInfo.Originator`     |
| 4 | Sub Discipline | `RvtFileInfo.SubDiscipline`  |
| 5 | Document Type  | `RvtFileInfo.DocumentType`   |
| 6 | Program        | `RvtFileInfo.Program`        |
| 7 | Sub Program    | `RvtFileInfo.SubProgram`     |

## Strategy (summary — details in the numbered files)
1. **Prefer NO Revit add-in.** Try to deliver everything through a Windows **Shell Property Handler + Property Schema** that reads/writes the values stored with the file.
2. **Run the feasibility gates in `02_FEASIBILITY_AND_DECISION.md` first.** They decide whether metadata set without Revit survives Revit's own Save / Save As / Sync.
3. If gates pass → **Track A** (no add-in). If any mandatory gate fails or cannot be tested → **Track B**: keep the same shell component and add a **Revit add-in for Revit 2025, 2026 and 2027**.
4. Package everything in one Windows 11 installer (`06_INSTALLER_WINDOWS11.md`).

## Reading order
| File | Purpose |
|------|---------|
| `01_REQUIREMENTS.md` | Goals, constraints, acceptance criteria |
| `02_FEASIBILITY_AND_DECISION.md` | Spike experiments + decision tree (Track A vs B) |
| `03_ARCHITECTURE.md` | Components, storage format, sync rules, repo layout |
| `04_SHELL_PROPERTY_HANDLER.md` | Native property handler + `.propdesc` schema |
| `05_REVIT_ADDIN_TRACK_B.md` | Revit 2025–2027 add-in (only if Track B) |
| `06_INSTALLER_WINDOWS11.md` | MSI / registration / uninstall |
| `07_TESTING_AND_VERIFICATION.md` | Automated + manual tests |
| `08_DELIVERABLES_AND_REPORT.md` | Definition of done and final report format |

## Operating rules for the agent
- **Work in phases and stop at each gate** to write a short result note in `docs/decisions/` (ADR format). Do not skip Phase 0.
- **Never guess platform facts.** If unsure about a registry key, CLSID, API signature, or Revit target framework, verify with official docs / the installed SDK / a quick web search and record the source in the ADR.
- **Never corrupt user files.** Any code that writes into an `.rvt` must be safe-save (temp copy → verify → atomic replace) and covered by tests on copies only.
- **Never overwrite an existing `.rvt` handler registration** (Autodesk installs thumbnail/preview/property handlers). Back up, chain/delegate, restore on uninstall.
- Do not use managed (.NET) code inside the in-proc shell handler. Native C++ (x64) only for the handler. (.NET is fine for the Revit add-in, the installer helpers and the editor tool.)
- Keep it dependency-light, documented, and reproducible: one `build.ps1` that builds, tests and packages.
- Ask the user **only** if a decision is truly blocked (e.g. no Revit installed and the user wants gate results). Otherwise choose the safe default and document it.

## Final output expected
See `08_DELIVERABLES_AND_REPORT.md`. Start now with Phase 0.

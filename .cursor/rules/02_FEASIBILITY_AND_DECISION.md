# 02 — Phase 0: Feasibility Spike & Decision (Track A vs Track B)

**Goal:** decide, with evidence, whether the extension can work **without a Revit add-in**.
Write results to `docs/decisions/ADR-001-track-decision.md`.

## Why this needs a spike
Explorer can only show what a property handler can read from the file (or from something attached to it). Revit rewrites the whole `.rvt` on Save. If Revit drops our data on save, a no-add-in solution loses values every time the user saves — which is unacceptable. We must **measure**, not assume.

## Storage candidates to test (in order of preference)
| ID | Candidate | Pros | Cons |
|----|-----------|------|------|
| S1 | **Extra stream inside the CFB** (e.g. `RvtFileInfo`) | Travels with the file (copy, zip, cloud) | Revit may drop it on save; external write into a proprietary file must be safe |
| S2 | **NTFS Alternate Data Stream** `file.rvt:RvtFileInfo` | Never touches `.rvt` bytes | Lost on zip/email/cloud/non-NTFS; may be lost if Revit replaces the file on save |
| S3 | **Sidecar** `file.rvt.fileinfo.json` | Simple, visible | Easily separated from file; clutter |

## Experiments (run on COPIES of real `.rvt` files; if no Revit is installed, mark "UNTESTED" = FAIL)
For each candidate S1–S3:

| Gate | Test | Pass condition |
|------|------|----------------|
| G1 | Write 7 values with the prototype writer; read back with a separate process | Exact round-trip, incl. Unicode |
| G2 | Open the file in Revit 2025/2026/2027 (whichever installed) — no warnings, no "recover" dialog | Opens normally |
| G3 | In Revit: **Save** (workshared local & non-workshared) | Values still present afterwards |
| G4 | In Revit: **Save As** to a new file name | Values present on the **new** file |
| G5 | In Revit: **Synchronize with Central** (local file) | Values still present on local file |
| G6 | Copy to another folder, rename, zip/unzip, copy via OneDrive folder | Present (S1 must pass; S2/S3 documented as non-portable) |
| G7 | Explorer edit (writable handler) while Revit has the file **open** | Graceful read-only failure; no corruption |
| G8 | Existing Autodesk thumbnail / preview still works | Yes |

Mandatory gates for **Track A**: **G1, G2, G3, G4, G5, G8**. G6 is mandatory for S1 if portability is a goal in FR5; otherwise document.

## Decision tree
```
Any candidate passes all mandatory gates?
 ├─ YES → Track A (no add-in). Use the passing candidate as the primary store.
 │         Still ship the shell handler + installer. Do NOT build the add-in.
 └─ NO / UNTESTED → Track B.
           Keep the shell handler + store abstraction (file 04).
           Build Revit add-in 2025/2026/2027 (file 05) that holds the truth in the model
           (Project Information parameters) and re-writes the file store after each save/sync/save-as.
```

## Notes the agent should check and record
- Does Revit save **in place** or write-new-then-replace? (Check file ID / creation time; ADS and sidecar behaviour depends on it.)
- Does Revit preserve **unknown CFB streams** on save? (Expected: probably not; confirm.)
- Does `DocumentSaved` / `DocumentSavedAs` / `DocumentSynchronizedWithCentral` in the Revit API fire after file handles are released so the file can be updated externally? (Only if Track B; test in the add-in spike.)
- Is there an existing Autodesk property handler or persistent handler on `.rvt`? Dump:
  `HKCR\.rvt`, `HKCR\<ProgID>`, `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.rvt`, `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\...\ThumbnailHandler` (via `shellex`). Save to `docs/decisions/rvt-registry-baseline.txt`.

## Expected outcome (hypothesis, to be confirmed)
S1/S2/S3 will probably **fail G3–G5** because Revit rewrites the file → **Track B is likely**. Even so, run the spike (or record "UNTESTED → Track B" with reason). Track B reuses 100% of Track A's shell handler, schema and installer, so no work is wasted.

## Deliverable of Phase 0
- `docs/decisions/ADR-001-track-decision.md` containing: environment (Windows build, Revit versions), results table G1–G8 per candidate, chosen primary store, chosen Track, and rationale.
- Prototype code under `spike/` (throwaway, not shipped).

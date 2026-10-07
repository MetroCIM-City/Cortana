# 03 — Architecture

## Components
```
┌───────────────────────────────┐        ┌────────────────────────────────────┐
│ Windows Explorer / Search     │        │ Revit 2025 / 2026 / 2027 (Track B) │
│  columns, Details pane        │        │  Add-in: ribbon + events           │
└──────────────┬────────────────┘        └───────────────┬────────────────────┘
               │ IPropertyStore                           │ writes on Save/SaveAs/Sync
┌──────────────▼────────────────┐   shared lib   ┌────────▼───────────────────┐
│ RvtFileInfo.ShellHandler (C++)│◄──────────────►│ RvtFileInfo.Store (C++ core │
│ property handler + schema     │  same format   │  + .NET wrapper/P-Invoke)   │
└──────────────┬────────────────┘                └────────┬───────────────────┘
               └──────────────► file store (S1/S2/S3) ◄───┘   (+ model: ProjectInfo params, Track B)
```

## Repository layout
```
rvt-fileinfo/
  build.ps1                      # restore, build, test, package
  docs/decisions/                # ADRs
  spike/                         # Phase 0 throwaway code
  src/
    Store.Core/                  # C++ static lib: CFB read/write, ADS, sidecar, JSON codec
    ShellHandler/                # C++ ATL COM DLL (x64): IPropertyStore, IInitializeWithStream, ...
      RvtFileInfo.propdesc
    Store.Net/                   # .NET wrapper over Store.Core (C ABI) — Track B + tools
    RevitAddin/                  # Track B: multi-target 2025/2026/2027
    EditorCli/                   # rvtinfo.exe: get/set/dump values (testing + support)
    Installer/                   # WiX v5 project + custom actions
  tests/
    Store.Tests/  ShellHandler.Tests/  Addin.Tests/  Fixtures/
```

## Property schema
- Namespace: `RvtFileInfo`; one new **FMTID GUID** (generate once, commit; PIDs 2–8).
- Property names `RvtFileInfo.Discipline`, … (see file 00). Type `String`. Display names exactly as requested (with spaces).
- Registered system-wide with `PSRegisterPropertySchema` from a `.propdesc` installed under `%ProgramFiles%\RvtFileInfo\`.

## Stored payload (UTF-8 JSON, schema v1)
```json
{
  "schema": 1,
  "discipline": "", "location": "", "originator": "", "subDiscipline": "",
  "documentType": "", "program": "", "subProgram": "",
  "modifiedUtc": "2026-01-01T00:00:00Z",
  "source": "explorer|revit|cli"
}
```
Rules: unknown keys preserved; max 256 chars/value; max payload 4 KB; trim control characters.

## Store abstraction
```
interface IFileInfoStore { Read(path|stream) -> FileInfo?; Write(path, FileInfo) -> HRESULT; Name }
```
Implementations: `CfbStreamStore` (S1), `AdsStore` (S2), `SidecarStore` (S3). Only the backends chosen in ADR-001 ship enabled. **Read order:** primary → secondary → none. **Write:** primary only (plus secondary if configured).

## Safe write rules (S1)
1. Never write if the file is locked by another process (probe with exclusive open); return `HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION)`.
2. Operate on a **temporary copy** in the same folder → modify only our stream via structured storage in transacted mode → verify CFB opens and all other streams' sizes/hashes unchanged → atomic replace (`ReplaceFileW`, preserving ACLs, timestamps adjusted only for modified).
3. Preserve file attributes and OneDrive placeholder state (do not hydrate/dehydrate unexpectedly; if file is a cloud placeholder, fall back to the read-only state).
4. Keep a rolling `.bak` only when `HKLM\SOFTWARE\RvtFileInfo\KeepBackups=1`.

## Sync rules (Track B only)
- **Model is the source of truth** for Revit users: seven shared parameters on **Project Information**.
- `DocumentOpened`: if stored `modifiedUtc` (file store) is newer than model's `RvtFileInfo_ModifiedUtc` parameter → import into model (one transaction, named "Import file info"); else leave.
- `DocumentSaved`, `DocumentSavedAs`, `DocumentSynchronizedWithCentral`: export model values → file store (using the Store library). Verify the file is writable at that moment (see ADR-001 notes).
- Conflict: newest `modifiedUtc` wins; ties → model wins.

## Security & robustness
- No elevation required at runtime; installer is the only admin step.
- Property handler runs out-of-proc via the property host (`IInitializeWithStream`); do **not** set `DisableProcessIsolation`.
- Fuzz the CFB reader (truncated, cyclic FAT, huge sizes) — must fail closed.

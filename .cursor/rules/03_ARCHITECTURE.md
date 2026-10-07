# 03 — Architecture

## Components
```
┌───────────────────────────────┐        ┌────────────────────────────────────┐
│ Windows Explorer / Search     │        │ Revit 2025 / 2026 / 2027 (Track B) │
│  columns, Details pane        │        │  Add-in: ribbon + events           │
└──────────────┬────────────────┘        └───────────────┬────────────────────┘
               │ IPropertyStore                           │ writes on Save/SaveAs/Sync
┌──────────────▼────────────────┐   shared lib   ┌────────▼───────────────────┐
│ RvtFileInfo.ShellHandler (C++)│◄──────────────►│ Store.Core + Store.Abi     │
│ + RvtFileInfo.propdesc        │  same JSON     │ + Store.Net P/Invoke       │
└──────────────┬────────────────┘                └────────┬───────────────────┘
               └────────── file store S1 / S4 (S2/S3 read fallback) ─────────┘
```

## Repository layout (this repo)
```
build.ps1
docs/decisions/                  ADR-001, rvt-registry-baseline.txt
docs/SUPPORT.md  docs/TEST_REPORT.md
spike/                           Phase 0 throwaway; not shipped
src/
  Store.Core/                    C++ static lib: Cfb.cpp (S1+S4), AltStores.cpp (S2/S3), Json.cpp
  Store.Abi/                     C ABI → RvtFileInfo.Store.dll
  Store.Net/                     net8 + net10 wrappers
  ShellHandler/                  ATL COM DLL x64, RvtFileInfo.propdesc
  RevitAddin/                    configs R25, R26, R27
  EditorCli/                     rvtinfo.exe
  Setup/                         RvtFileInfo.Setup.exe (shell | manifests)
  Installer/                     Package.wxs, Package.Addin.wxs, Package.Manifests.wxs
  Shared/Guids.h
tests/  Store.Tests  ShellHandler.Tests  Addin.Tests  Fixtures/
```

## Property schema
- Namespace `RvtFileInfo`. FMTID `{6F3C2A91-8B14-4D5E-A7C2-19E4B8D07F31}`. PIDs 2–8 + group 100.
- Installed as `%ProgramFiles%\RvtFileInfo\RvtFileInfo.propdesc`.
- Register with `PSRegisterPropertySchema` from Setup.exe (not a separate RegisterSchema.exe).
- No `<description>` element. Do not set `canBePurged` on writable (`isInnate=false`) properties. Group-by is `groupingRange="Discrete"`. Edit control is `editControl control="Text"`.

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
Unknown keys preserved. Max 256 chars/value. Max payload 4 KB. Control characters stripped.

## Read / write tree (`ReadFile` / `WriteFile`)
```
cloud placeholder → fail (read-only)
Write: exclusive open or ERROR_SHARING_VIOLATION

Read:
  if CFB magic → S1 OpenStream("RvtFileInfo"); if missing → S4 trailer → S2 → S3
  else → S4 trailer → S2 → S3

Write:
  if CFB magic → S1 temp copy, STGM_TRANSACTED, SameExceptPayload, ReplaceFileW
  else → S4 temp copy, truncate to hostSize, append CFB+footer, host prefix hash, ReplaceFileW
        then delete path:RvtFileInfo and path.fileinfo.json
```

`IStream` path: `ReadStream` / `WriteStream` try whole-stream CFB first; if that fails, S4 on the stream.

## Safe write (S1)
1. Exclusive probe; locked → `HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION)`.
2. Temp `*.rfi.tmp` in the same folder → transacted structured storage → verify other streams' SHA-256 → `ReplaceFileW`.
3. Cloud placeholders: no write.
4. `.bak` only if `HKLM\SOFTWARE\RvtFileInfo\KeepBackups=1`.
5. `StgOpenStorage` paths must **not** use the `\\?\` prefix (`StoragePath`).

## Safe write (S4)
Same exclusive probe, temp copy, `ReplaceFileW`, failpoint `SetFailPoint(1)` before replace. Host bytes (everything before the trailer) hashed; mismatch aborts and deletes the temp file.

## Sync rules (Track B)
- Model is source of truth in Revit: shared parameters on Project Information.
- `DocumentOpened`: if file `modifiedUtc` is newer than `RvtFileInfo_ModifiedUtc` → import in transaction `"Import file info"`.
- `DocumentSaved` / `DocumentSavedAs` / `DocumentSynchronizedWithCentral`: export model → `WriteFile`. Retry via `ExternalEvent` + timer, max 3, then `TaskDialog` + `addin.log`.
- Conflict: newer `modifiedUtc` wins; tie → model wins.
- Ignore families, links, documents with no path.

## Security
- Runtime: no elevation. Installers: admin.
- Handler: process isolation on (do **not** set `DisableProcessIsolation`).
- Corrupt CFB (truncated magic, etc.): fail closed.

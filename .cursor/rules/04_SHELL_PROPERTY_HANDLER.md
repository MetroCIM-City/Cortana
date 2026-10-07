# 04 — Shell Property Handler + Property Schema

## Technology
C++20, MSVC ATL, x64 COM DLL `RvtFileInfo.ShellHandler.dll`. Toolset v145 (override `RfiToolset`). `/sdl`, `/GS`, `/guard:cf`. No managed code in the handler.

## COM class
CLSID `{C4A91E72-5D38-4F0B-9E16-2B7A6C8D4E50}` (`CRvtPropertyHandler` in `PropertyHandler.cpp`).

Implements:
- `IInitializeWithStream`
- `IInitializeWithFile`
- `IInitializeWithItem`
- `IPropertyStore`
- `IPropertyStoreCapabilities`

ThreadingModel **Both**. No `DisableProcessIsolation`. No extra AppID/DllSurrogate unless a new ADR requires it.

Process isolation stays on: `IInitializeWithFile` is used when Explorer gives a path; `IInitializeWithStream` is preferred when isolated. Path is also taken from `STATSTG.pwcsName` when it is a real filesystem path. Persist **prefers `WriteFile(path)`** so S1/S4 run; only if that fails **and** the source is a leading CFB does it `WriteStream` / `IDestinationStreamFactory`. Do not `WriteStream` a non-CFB host as if it were a compound file (that would corrupt DWG/PDF).

### Behaviour
- Writable unless the path is a cloud placeholder. Do **not** treat `STGM_READ` as making our seven properties read-only (Details would list them but not accept edits).
- `GetValue`: `VT_LPWSTR` for our PIDs 2–8; `VT_EMPTY` if unset.
- `SetValue`: ≤256 chars, strip controls, stage; `Commit` sets `source=explorer`, `mask=FieldAll`, `modifiedUtc=NowUtc`.
- Delegate non-`RvtFileInfo.*` keys to the previous handler CLSID stored per extension under `HKLM\SOFTWARE\RvtFileInfo\OriginalHandlers` (and legacy `OriginalPropertyHandler` for `.rvt`).

## Coexistence
1. Baseline: `docs/decisions/rvt-registry-baseline.txt`.
2. Backup existing `PropertyHandlers\.ext` into `HKLM\SOFTWARE\RvtFileInfo` before overwrite (`Setup` `Backup`).
3. Register our CLSID as the handler for **all** supported extensions.
4. Never write thumbnail/preview `shellex` keys.

## Registry (Explorer MSI / `RvtFileInfo.Setup.exe shell install`)
```
HKCR\CLSID\{C4A91E72-5D38-4F0B-9E16-2B7A6C8D4E50}\InprocServer32
  (Default) = %ProgramFiles%\RvtFileInfo\RvtFileInfo.ShellHandler.dll
  ThreadingModel = Both
HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.rvt|.rfa|.dwg|.nwd|.nwf|.nwc|.pdf
  (Default) = {C4A91E72-5D38-4F0B-9E16-2B7A6C8D4E50}
HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved
  {C4A91E72-…} = RvtFileInfo Property Handler
```

**Details UI:** append `RvtFileInfo.FileInfo;RvtFileInfo.Discipline;…;RvtFileInfo.SubProgram` to `FullDetails`, `PreviewDetails`, and `InfoTip` on:
- `HKCR\.ext`
- `HKCR\SystemFileAssociations\.ext`
- every ProgID (`(Default)`, `OpenWithProgids`, `CurVer`)

Without ProgID `FullDetails`, Explorer lists the names but they are not editable. For `.nwd` `.nwf` `.nwc` Setup may create ProgID `RvtFileInfo.nwd` (etc.) if none exists.

Do not register PersistentHandler unless documented.

## Property schema (`src/ShellHandler/RvtFileInfo.propdesc`)
Shipped file is the source of truth. Publisher/product `RvtFileInfo`. `formatID` is the FMTID above.

- Group: `RvtFileInfo.FileInfo` PID 100, `isGroup="true"` `isInnate="true"` label **File Info**.
- Seven properties: `searchInfo isColumn="true" columnIndexType="OnDisk"`; `typeInfo isInnate="false" isViewable="true" isQueryable="true" groupingRange="Discrete"`; `labelInfo` display names with spaces; `editControl control="Text"`.
- No `<description>` node (schema rejected it). No `canBePurged`.

Setup: `PSRegisterPropertySchema` then `PSRefreshPropertySchema`. Uninstall: `PSUnregisterPropertySchema`.

## Performance
Do not slurp the host file. S1 hashes other streams on **write** only. S4 hashes the host prefix on write. Read: CFB directory + `RvtFileInfo` or last 16 bytes + small CFB.

COM exports: `DllCanUnloadNow`, `DllGetClassObject`. Registration of COM/schema is Setup.exe, not `DllRegisterServer` as the install path.

## Done when
`rvtinfo set` / Details edit round-trip for `.rvt` and for a dummy `.dwg`/`.pdf` whose leading bytes stay `AC10…` / `%PDF`. Copy of the DWG still `get`s values without ADS.

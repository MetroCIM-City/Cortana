# 04 — Shell Property Handler + Property Schema (always built, both tracks)

## Technology
- **C++20, MSVC, ATL/WRL COM DLL, x64.** (Optional later: ARM64 build — Revit itself is x64, so not required.)
- No managed code in the handler.

## COM class
`RvtFileInfoPropertyHandler` implements:
- `IInitializeWithStream` (preferred; works with the sandboxed property host)
- `IPropertyStore` (GetCount, GetAt, GetValue, SetValue, Commit)
- `IPropertyStoreCapabilities` (`IsPropertyWritable` → true for our 7 properties only)
- `IPropertyStoreCapabilities`-compatible read-only behaviour when the stream is read-only
- Thread model: **Both**. Register with `AppID` + `DllSurrogate` only if the ADR requires it; otherwise default prophost.

### Behaviour
- `Initialize(IStream*, grfMode)`: open the store from the stream. Read **only** what is needed (CFB header → directory → our stream). Cache values in a `PROPVARIANT` map.
- `GetValue(key)`: return `VT_LPWSTR` for our 7 keys; `VT_EMPTY` when unset.
- `SetValue`: validate (≤256 chars, strip control chars), stage in memory.
- `Commit`: use the safe-write rules from file 03 (for S1, via `IDestinationStreamFactory` / transacted storage; for S2/S3 the handler needs a path → see *File-based store note*).
- **File-based store note (S2/S3):** these need a file path. Implement `IInitializeWithFile` only if an ADR shows it is needed, and then **justify disabling process isolation** in the ADR — otherwise prefer `IInitializeWithStream` + resolving the path from `IInitializeWithItem`.
- Must **delegate** to the pre-existing Autodesk handler (if any): see "Coexistence" below.

## Coexistence with Autodesk's `.rvt` registration
1. Read the baseline from `docs/decisions/rvt-registry-baseline.txt`.
2. If **no** property handler is registered for `.rvt` → register ours directly.
3. If one **is** registered → register our handler as the primary and **aggregate**: our `IPropertyStore` forwards all non-`RvtFileInfo.*` keys (`GetCount/GetAt/GetValue`) to the original handler's `IPropertyStore` (CoCreateInstance the original CLSID, initialise with the same stream). Save the original CLSID in `HKLM\SOFTWARE\RvtFileInfo\OriginalPropertyHandler` for uninstall/restore.
4. Never touch thumbnail/preview handler keys.

## Registry entries (per-machine, written by the installer)
```
HKCR\CLSID\{OUR-CLSID}\InprocServer32  (Default) = C:\Program Files\RvtFileInfo\RvtFileInfo.ShellHandler.dll ; ThreadingModel = Both
HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.rvt  (Default) = {OUR-CLSID}
HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved  {OUR-CLSID} = "RvtFileInfo Property Handler"
HKCR\.rvt\ (or its ProgID)\ FullDetails / PreviewDetails / InfoTip = prop:...;RvtFileInfo.Discipline;...  (append, don't replace)
```
Verify each key name against Microsoft docs ("Registering and Distributing a Property Handler"), and for Windows Search ("Developing Property Handlers for Windows Search": `HKCR\.rvt\PersistentHandler`, only if indexing is required — document the decision).

## Property schema (`RvtFileInfo.propdesc`)
One `<propertyDescription>` per property, for example:
```xml
<?xml version="1.0" encoding="utf-8"?>
<schema xmlns="http://schemas.microsoft.com/windows/2006/propertydescription" schemaVersion="1.0">
  <propertyDescriptionList publisher="RvtFileInfo" product="RvtFileInfo">
    <propertyDescription name="RvtFileInfo.Discipline" formatID="{NEW-GUID}" propID="2">
      <description>Discipline of the Revit file</description>
      <searchInfo inInvertedIndex="true" isColumn="true" isColumnSparse="false" columnIndexType="OnDisk" mnemonics="discipline"/>
      <typeInfo type="String" multipleValues="false" isInnate="false" isViewable="true" isQueryable="true" canBePurged="false"/>
      <labelInfo label="Discipline" invitationText="Add Discipline" hideLabel="false"/>
      <displayInfo defaultColumnWidth="16" displayType="String" alignment="Left" relativeDescriptionType="General">
        <editControl><control>TextBox</control></editControl>   <!-- verify exact schema for edit control -->
        <stringFormat formatAs="General"/>
        <drawControl><control>Default</control></drawControl>
      </displayInfo>
      <typeInfo/> <!-- agent: fix to the valid schema order/elements per MS docs -->
    </propertyDescription>
    <!-- repeat for propID 3..8: Location, Originator, SubDiscipline ("Sub Discipline"), DocumentType ("Document Type"), Program, SubProgram ("Sub Program") -->
  </propertyDescriptionList>
</schema>
```
**The snippet is illustrative.** Validate the final `.propdesc` with `PSRegisterPropertySchema` on a test VM and with Microsoft's schema; remove placeholder/duplicate elements. Requirements: `canGroupBy`, `canStackBy`, sortable, `isViewable=true`, `isInnate=false`, `searchInfo isColumn=true`.

## Registration helpers
- `RegisterSchema.exe` (or MSI custom action): calls `PSRegisterPropertySchema(path)`, then `PSRefreshPropertySchema()`. Uninstall: `PSUnregisterPropertySchema(path)`.
- Both elevated; return meaningful HRESULTs to the MSI log.

## Performance & stability checklist
- No heap allocation proportional to file size; use `IStream::Seek/Read` for header, DIFAT/FAT chain walking, and the single target stream.
- Hard limits: ≤ 1 MB read for directory, ≤ 64 KB for our stream. Time limit via cancel flag.
- Catch all C++ exceptions at COM boundary; no `abort()`.
- Add `DllCanUnloadNow`, `DllGetClassObject`, `DllRegisterServer` (registry only; schema is done by installer).
- Build with `/guard:cf`, `/GS`, `/sdl`, signed if a certificate is available.

## Done when
- `rvtinfo.exe set file.rvt Discipline=ARC` → Explorer shows the value in the Discipline column after Explorer restart.
- Editing Discipline in the Details pane writes the value and `rvtinfo.exe get` returns it.
- All non-`RvtFileInfo` properties previously shown for `.rvt` still show (compare before/after dump).

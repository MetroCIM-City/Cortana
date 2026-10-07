# Support

## Logs

| File | Written by |
|------|------------|
| `%ProgramData%\RvtFileInfo\logs\setup.log` | Install and uninstall |
| `%ProgramData%\RvtFileInfo\logs\addin.log` | Revit add-in exceptions |

The install does not delete the log folder.

## Dump the shell registration

From an elevated or normal command prompt:

```text
reg query HKCR\.rvt /s
reg query HKCR\.pdf /s
reg query HKCR\Revit.Project /s
reg query HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.rvt
reg query HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.dwg
reg query HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.pdf
reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertySchema" /s
reg query HKLM\SOFTWARE\RvtFileInfo /s
```

The thumbnail value under `HKCR\.rvt\shellex\{BB2E617C-0920-11d1-9A0B-00C04FC2D6C1}` should stay `{2E559A13-E91E-4DEB-8996-3B594FC11AC4}` on a machine that had the Autodesk thumbnail handler before install.

## Read a file from the command line

```text
"%ProgramFiles%\RvtFileInfo\cli\rvtinfo.exe" get "C:\path\file.rvt"
"%ProgramFiles%\RvtFileInfo\cli\rvtinfo.exe" dump "C:\path\file.rvt" --streams
```

`get` prints the seven fields. `dump --streams` lists compound-file streams and their sizes, then the field values. For DWG, Navisworks, and PDF, `get` / `set` use the NTFS alternate stream `file:RvtFileInfo` (or `file.ext.fileinfo.json` if ADS cannot be created). Zip copies do not keep ADS values.

## Pick lists

`%ProgramData%\RvtFileInfo\picklists.json` is optional. Keys are the display labels (`Discipline`, `Location`, `Originator`, `Sub Discipline`, `Document Type`, `Program`, `Sub Program`). Values are arrays of strings. The Revit dialog shows them in an editable combo box. Uninstall deletes this file only when its contents still match the hash stored at install time.

## Values missing after Revit save

Explorer writes the `RvtFileInfo` stream directly. Revit rewrites the project file on save. The add-in writes the stream again from Project Information after Save, Save As, and Sync with Central. If the add-in is not loaded, a save can drop a value that was typed only in Explorer.

Check:

1. Revit shows the File Info tab.
2. `%ProgramData%\Autodesk\Revit\Addins\<year>\RvtFileInfo.addin` points at `RvtFileInfo.RevitAddin.dll`.
3. `%ProgramData%\RvtFileInfo\logs\addin.log` for a sharing violation that exhausted three retries.

## Locked file

If Revit has the file open, `rvtinfo set` and Explorer edits fail with a sharing violation and do not change the file. Reads still work.

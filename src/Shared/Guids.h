#pragma once

#include <guiddef.h>

// Stable identifiers. Do not regenerate; Explorer, the schema, and the add-in share them.
// FMTID / property PIDs: Microsoft property system (one FMTID, PIDs 2-8).
// CLSID: RvtFileInfo property handler.

namespace rfi {

inline constexpr GUID CLSID_RvtFileInfoPropertyHandler = {
    0xC4A91E72, 0x5D38, 0x4F0B, {0x9E, 0x16, 0x2B, 0x7A, 0x6C, 0x8D, 0x4E, 0x50}};

inline constexpr GUID FMTID_RvtFileInfo = {
    0x6F3C2A91, 0x8B14, 0x4D5E, {0xA7, 0xC2, 0x19, 0xE4, 0xB8, 0xD0, 0x7F, 0x31}};

inline constexpr unsigned long kPidDiscipline = 2;
inline constexpr unsigned long kPidLocation = 3;
inline constexpr unsigned long kPidOriginator = 4;
inline constexpr unsigned long kPidSubDiscipline = 5;
inline constexpr unsigned long kPidDocumentType = 6;
inline constexpr unsigned long kPidProgram = 7;
inline constexpr unsigned long kPidSubProgram = 8;
inline constexpr unsigned long kPidGroup = 100;

inline constexpr wchar_t kStreamName[] = L"RvtFileInfo";
inline constexpr size_t kMaxValueChars = 256;
inline constexpr size_t kMaxPayloadBytes = 4096;

}  // namespace rfi

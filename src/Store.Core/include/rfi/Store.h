#pragma once

#include "Guids.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct IStream;
struct IStorage;

namespace rfi {

enum Field : uint32_t {
    FieldDiscipline = 1u << 0,
    FieldLocation = 1u << 1,
    FieldOriginator = 1u << 2,
    FieldSubDiscipline = 1u << 3,
    FieldDocumentType = 1u << 4,
    FieldProgram = 1u << 5,
    FieldSubProgram = 1u << 6,
    FieldAll = 0x7Fu
};

struct FileInfo {
    int schema = 1;
    uint32_t mask = 0;
    std::wstring discipline;
    std::wstring location;
    std::wstring originator;
    std::wstring subDiscipline;
    std::wstring documentType;
    std::wstring program;
    std::wstring subProgram;
    std::wstring modifiedUtc;
    std::wstring source;
    // Preserved unknown object members: JSON name, raw JSON value text.
    std::vector<std::pair<std::string, std::string>> unknown;
};

struct Status {
    long hr = 0;
    std::wstring message;
};

inline bool Ok(const Status& status) { return status.hr >= 0; }

std::wstring SanitizeField(const std::wstring& value, bool* tooLong);
std::wstring NowUtc();

Status Encode(const FileInfo& info, std::string& utf8);
Status Decode(const std::string& utf8, FileInfo& info);

// Primary store: CFB stream "RvtFileInfo". Safe temp-copy + transacted commit + ReplaceFileW.
Status ReadFile(const std::wstring& path, FileInfo& info, bool& found);
Status WriteFile(const std::wstring& path, const FileInfo& info);

// Alternate backends. Tested, not enabled for shipping writes (see ADR-001).
Status ReadAds(const std::wstring& path, FileInfo& info, bool& found);
Status WriteAds(const std::wstring& path, const FileInfo& info);
Status ReadSidecar(const std::wstring& path, FileInfo& info, bool& found);
Status WriteSidecar(const std::wstring& path, const FileInfo& info);

Status ReadRaw(const std::wstring& path, std::string& json, bool& found);
Status WriteRaw(const std::wstring& path, const std::string& json);
Status ListStreams(const std::wstring& path, std::wstring& report);

// Property-handler path: the stream is the compound file itself.
Status ReadStream(IStream* stream, FileInfo& info, bool& found);
Status WriteStream(IStream* stream, const FileInfo& info);

// 0 = off, 1 = fail after the temp file is verified and before ReplaceFileW.
void SetFailPoint(int phase);
std::wstring LastError();

bool IsCloudPlaceholder(const std::wstring& path);
bool IsCompoundFile(const std::wstring& path);

}  // namespace rfi

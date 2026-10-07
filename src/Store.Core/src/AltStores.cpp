#include "rfi/Store.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace rfi {
namespace {

Status WriteBytes(const std::wstring& path, const std::string& bytes) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Status{HRESULT_FROM_WIN32(GetLastError()), L"could not create the store"};
    DWORD written = 0;
    const BOOL ok = bytes.empty() || ::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(file);
    if (!ok || written != bytes.size()) return Status{E_FAIL, L"could not write the store"};
    return Status{S_OK, {}};
}

Status ReadBytes(const std::wstring& path, std::string& bytes, bool& found) {
    found = false;
    bytes.clear();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return Status{S_OK, {}};
        return Status{HRESULT_FROM_WIN32(GetLastError()), L"could not read the store"};
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart > static_cast<LONGLONG>(kMaxPayloadBytes)) {
        CloseHandle(file);
        return Status{E_INVALIDARG, L"payload exceeds 4 KB"};
    }
    bytes.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL ok = bytes.empty() || ::ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok) return Status{E_FAIL, L"could not read the store"};
    bytes.resize(read);
    found = true;
    return Status{S_OK, {}};
}

Status WriteEncoded(const std::wstring& target, const FileInfo& incoming, const std::wstring& sourceName) {
    FileInfo info = incoming;
    if (info.mask == 0) info.mask = FieldAll;
    bool tooLong = false;
    auto apply = [&](uint32_t bit, std::wstring& field) {
        if ((info.mask & bit) == 0) return;
        field = SanitizeField(field, &tooLong);
    };
    apply(FieldDiscipline, info.discipline);
    apply(FieldLocation, info.location);
    apply(FieldOriginator, info.originator);
    apply(FieldSubDiscipline, info.subDiscipline);
    apply(FieldDocumentType, info.documentType);
    apply(FieldProgram, info.program);
    apply(FieldSubProgram, info.subProgram);
    if (tooLong) return Status{E_INVALIDARG, L"value exceeds 256 characters"};
    info.modifiedUtc = NowUtc();
    info.source = sourceName;
    info.schema = 1;
    std::string json;
    Status encoded = Encode(info, json);
    if (!Ok(encoded)) return encoded;
    if (target.size() >= 3 && target[1] == L':' && target.find(L':', 2) != std::wstring::npos) {
        return WriteBytes(target, json);
    }
    const std::wstring temp = target + L".tmp";
    Status written = WriteBytes(temp, json);
    if (!Ok(written)) return written;
    if (!MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        DeleteFileW(temp.c_str());
        return Status{HRESULT_FROM_WIN32(error), L"could not replace the store"};
    }
    return Status{S_OK, {}};
}

Status ReadEncoded(const std::wstring& target, FileInfo& info, bool& found) {
    std::string json;
    Status read = ReadBytes(target, json, found);
    if (!Ok(read) || !found) return read;
    return Decode(json, info);
}

}  // namespace

Status ReadAds(const std::wstring& path, FileInfo& info, bool& found) {
    return ReadEncoded(path + L":RvtFileInfo", info, found);
}

Status WriteAds(const std::wstring& path, const FileInfo& info) {
    if (IsCloudPlaceholder(path)) return Status{HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), L"cloud placeholder is read-only"};
    return WriteEncoded(path + L":RvtFileInfo", info, info.source.empty() ? L"cli" : info.source);
}

Status ReadSidecar(const std::wstring& path, FileInfo& info, bool& found) {
    return ReadEncoded(path + L".fileinfo.json", info, found);
}

Status WriteSidecar(const std::wstring& path, const FileInfo& info) {
    return WriteEncoded(path + L".fileinfo.json", info, info.source.empty() ? L"cli" : info.source);
}

}  // namespace rfi

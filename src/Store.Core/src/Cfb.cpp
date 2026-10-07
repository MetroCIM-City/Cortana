#include "rfi/Store.h"

#include "ComPtr.h"

#include <cstring>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <ole2.h>

#include <atomic>
#include <map>
#include <mutex>
#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")

#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000
#endif
#ifndef FILE_ATTRIBUTE_RECALL_ON_OPEN
#define FILE_ATTRIBUTE_RECALL_ON_OPEN 0x00040000
#endif

namespace rfi {
namespace {

std::atomic<int> g_failPoint{0};
thread_local std::wstring g_lastError;

struct Digest {
    uint64_t size = 0;
    std::array<uint8_t, 32> hash{};
};

class StreamLockBytes : public ILockBytes {
public:
    explicit StreamLockBytes(IStream* stream) : stream_(stream) { stream_->AddRef(); }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ILockBytes) {
            *object = static_cast<ILockBytes*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG count = static_cast<ULONG>(InterlockedDecrement(&refs_));
        if (count == 0) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE ReadAt(ULARGE_INTEGER offset, void* buffer, ULONG count, ULONG* read) override {
        std::lock_guard<std::mutex> lock(mutex_);
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(offset.QuadPart);
        HRESULT hr = stream_->Seek(position, STREAM_SEEK_SET, nullptr);
        if (FAILED(hr)) return hr;
        return stream_->Read(buffer, count, read);
    }

    HRESULT STDMETHODCALLTYPE WriteAt(ULARGE_INTEGER offset, const void* buffer, ULONG count, ULONG* written) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const ULONGLONG end = offset.QuadPart + count;
        STATSTG stat{};
        HRESULT hr = stream_->Stat(&stat, STATFLAG_NONAME);
        if (FAILED(hr)) return hr;
        if (end > stat.cbSize.QuadPart) {
            ULARGE_INTEGER size{};
            size.QuadPart = end;
            hr = stream_->SetSize(size);
            if (FAILED(hr)) return hr;
        }
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(offset.QuadPart);
        hr = stream_->Seek(position, STREAM_SEEK_SET, nullptr);
        if (FAILED(hr)) return hr;
        return stream_->Write(buffer, count, written);
    }

    HRESULT STDMETHODCALLTYPE Flush() override { return stream_->Commit(STGC_DEFAULT); }

    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER size) override { return stream_->SetSize(size); }

    HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE Stat(STATSTG* stat, DWORD flags) override {
        if (!stat) return E_POINTER;
        STATSTG streamStat{};
        HRESULT hr = stream_->Stat(&streamStat, STATFLAG_NONAME);
        if (FAILED(hr)) return hr;
        ZeroMemory(stat, sizeof(*stat));
        stat->type = STGTY_LOCKBYTES;
        stat->cbSize = streamStat.cbSize;
        stat->grfMode = STGM_READWRITE;
        (void)flags;
        return S_OK;
    }

private:
    ~StreamLockBytes() { stream_->Release(); }

    IStream* stream_;
    long refs_ = 1;
    std::mutex mutex_;
};

Status Fail(long hr, const std::wstring& message) {
    g_lastError = message;
    return Status{hr, message};
}

std::wstring LongPath(const std::wstring& path) {
    if (path.rfind(L"\\\\?\\", 0) == 0) return path;
    if (path.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + path.substr(2);
    if (path.size() >= 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) return L"\\\\?\\" + path;
    return path;
}

// Structured storage returns STG_E_INVALIDFLAG for the \\?\ prefix.
std::wstring StoragePath(const std::wstring& path) {
    if (path.rfind(L"\\\\?\\UNC\\", 0) == 0) return L"\\\\" + path.substr(8);
    if (path.rfind(L"\\\\?\\", 0) == 0) return path.substr(4);
    return path;
}

bool KeepBackups() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\RvtFileInfo", L"KeepBackups", RRF_RT_REG_DWORD,
                                        nullptr, &value, &size);
    return status == ERROR_SUCCESS && value == 1;
}

bool HashStream(IStream* stream, Digest& digest) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    DWORD objectLength = 0;
    DWORD ignored = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength),
                          &ignored, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }
    std::vector<UCHAR> object(objectLength);
    if (BCryptCreateHash(algorithm, &hash, object.data(), objectLength, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }
    LARGE_INTEGER zero{};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    std::vector<unsigned char> buffer(64 * 1024);
    uint64_t total = 0;
    for (;;) {
        ULONG read = 0;
        const HRESULT hr = stream->Read(buffer.data(), static_cast<ULONG>(buffer.size()), &read);
        if (FAILED(hr)) {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return false;
        }
        if (read == 0) break;
        if (BCryptHashData(hash, buffer.data(), read, 0) < 0) {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return false;
        }
        total += read;
    }
    digest.size = total;
    if (BCryptFinishHash(hash, digest.hash.data(), static_cast<ULONG>(digest.hash.size()), 0) < 0) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return true;
}

Status Enumerate(IStorage* storage, const std::wstring& prefix, std::map<std::wstring, Digest>& out) {
    ComPtr<IEnumSTATSTG> enumerator;
    HRESULT hr = storage->EnumElements(0, nullptr, 0, enumerator.Put());
    if (FAILED(hr)) return Fail(hr, L"EnumElements failed");
    for (;;) {
        STATSTG stat{};
        hr = enumerator->Next(1, &stat, nullptr);
        if (hr == S_FALSE) break;
        if (FAILED(hr)) return Fail(hr, L"IEnumSTATSTG::Next failed");
        std::wstring name = stat.pwcsName ? stat.pwcsName : L"";
        CoTaskMemFree(stat.pwcsName);
        const std::wstring full = prefix.empty() ? name : prefix + L"\\" + name;
        if (stat.type == STGTY_STREAM) {
            ComPtr<IStream> stream;
            hr = storage->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, stream.Put());
            if (FAILED(hr)) {
                hr = storage->OpenStream(name.c_str(), nullptr, STGM_READ | STGM_SHARE_DENY_NONE, 0, stream.Put());
            }
            if (FAILED(hr)) return Fail(hr, L"OpenStream failed while hashing " + full);
            Digest digest;
            if (!HashStream(stream.Get(), digest)) return Fail(E_FAIL, L"hash failed for " + full);
            out.emplace(full, digest);
        } else if (stat.type == STGTY_STORAGE) {
            ComPtr<IStorage> child;
            hr = storage->OpenStorage(name.c_str(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, child.Put());
            if (FAILED(hr)) return Fail(hr, L"OpenStorage failed for " + full);
            Status nested = Enumerate(child.Get(), full, out);
            if (!Ok(nested)) return nested;
        }
    }
    return Status{S_OK, {}};
}

Status OpenPath(const std::wstring& path, DWORD mode, IStorage** storage) {
    return Status{StgOpenStorage(StoragePath(path).c_str(), nullptr, mode, nullptr, 0, storage), {}};
}

Status HashFile(const std::wstring& path, std::map<std::wstring, Digest>& out) {
    ComPtr<IStorage> storage;
    const HRESULT hr = StgOpenStorage(StoragePath(path).c_str(), nullptr, STGM_READ | STGM_SHARE_DENY_WRITE, nullptr, 0,
                                      storage.Put());
    if (FAILED(hr)) return Fail(hr, L"could not open compound file to hash streams");
    return Enumerate(storage.Get(), L"", out);
}

bool SameExceptPayload(const std::map<std::wstring, Digest>& before, const std::map<std::wstring, Digest>& after,
                       std::wstring& error) {
    for (const auto& item : before) {
        if (item.first == kStreamName) continue;
        const auto found = after.find(item.first);
        if (found == after.end() || found->second.size != item.second.size || found->second.hash != item.second.hash) {
            error = L"stream changed: " + item.first;
            return false;
        }
    }
    for (const auto& item : after) {
        if (item.first == kStreamName) continue;
        if (before.find(item.first) == before.end()) {
            error = L"unexpected new stream: " + item.first;
            return false;
        }
    }
    return true;
}

Status ReadStorage(IStorage* storage, FileInfo& info, bool& found) {
    found = false;
    ComPtr<IStream> stream;
    HRESULT hr = storage->OpenStream(kStreamName, nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, stream.Put());
    if (hr == STG_E_FILENOTFOUND) return Status{S_OK, {}};
    if (FAILED(hr)) return Fail(hr, L"OpenStream RvtFileInfo failed");
    STATSTG stat{};
    hr = stream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(hr)) return Fail(hr, L"Stat failed");
    if (stat.cbSize.QuadPart > kMaxPayloadBytes) return Fail(E_INVALIDARG, L"payload exceeds 4 KB");
    std::string json(static_cast<size_t>(stat.cbSize.QuadPart), '\0');
    ULONG read = 0;
    if (!json.empty()) {
        hr = stream->Read(json.data(), static_cast<ULONG>(json.size()), &read);
        if (FAILED(hr)) return Fail(hr, L"Read failed");
        json.resize(read);
    }
    Status decoded = Decode(json, info);
    if (!Ok(decoded)) return decoded;
    found = true;
    return Status{S_OK, {}};
}

Status WriteStorage(IStorage* storage, const FileInfo& info) {
    std::string json;
    Status encoded = Encode(info, json);
    if (!Ok(encoded)) return encoded;
    ComPtr<IStream> stream;
    HRESULT hr = storage->CreateStream(kStreamName, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, stream.Put());
    if (FAILED(hr)) return Fail(hr, L"CreateStream RvtFileInfo failed");
    ULONG written = 0;
    hr = stream->Write(json.data(), static_cast<ULONG>(json.size()), &written);
    if (FAILED(hr) || written != json.size()) return Fail(FAILED(hr) ? hr : E_FAIL, L"writing RvtFileInfo failed");
    hr = stream->Commit(STGC_DEFAULT);
    if (FAILED(hr)) return Fail(hr, L"stream commit failed");
    return Status{S_OK, {}};
}

}  // namespace

bool IsCompoundFile(const std::wstring& path) {
    HANDLE file = CreateFileW(LongPath(path).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    unsigned char magic[8]{};
    DWORD read = 0;
    const BOOL ok = ::ReadFile(file, magic, sizeof(magic), &read, nullptr);
    CloseHandle(file);
    const unsigned char signature[8] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    return ok && read == sizeof(magic) && memcmp(magic, signature, sizeof(magic)) == 0;
}

namespace {

Status ProbeExclusive(const std::wstring& path) {
    HANDLE file = CreateFileW(StoragePath(path).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (error == ERROR_SHARING_VIOLATION) return Fail(HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION), L"file is in use");
        return Fail(HRESULT_FROM_WIN32(error), L"could not open the file for write");
    }
    CloseHandle(file);
    return Status{S_OK, {}};
}

Status ApplyField(const FileInfo& incoming, uint32_t bit, const std::wstring& value, std::wstring& destination) {
    if ((incoming.mask & bit) == 0) return Status{S_OK, {}};
    bool tooLong = false;
    destination = SanitizeField(value, &tooLong);
    if (tooLong) return Fail(E_INVALIDARG, L"value exceeds 256 characters");
    return Status{S_OK, {}};
}

Status Merge(const std::wstring& path, const FileInfo& incoming, FileInfo& merged) {
    FileInfo existing;
    bool found = false;
    Status read = ReadFile(path, existing, found);
    if (!Ok(read)) return read;
    merged = found ? existing : FileInfo{};
    Status applied = Status{S_OK, {}};
    applied = ApplyField(incoming, FieldDiscipline, incoming.discipline, merged.discipline);
    if (!Ok(applied)) return applied;
    applied = ApplyField(incoming, FieldLocation, incoming.location, merged.location);
    if (!Ok(applied)) return applied;
    applied = ApplyField(incoming, FieldOriginator, incoming.originator, merged.originator);
    if (!Ok(applied)) return applied;
    applied = ApplyField(incoming, FieldSubDiscipline, incoming.subDiscipline, merged.subDiscipline);
    if (!Ok(applied)) return applied;
    applied = ApplyField(incoming, FieldDocumentType, incoming.documentType, merged.documentType);
    if (!Ok(applied)) return applied;
    applied = ApplyField(incoming, FieldProgram, incoming.program, merged.program);
    if (!Ok(applied)) return applied;
    applied = ApplyField(incoming, FieldSubProgram, incoming.subProgram, merged.subProgram);
    if (!Ok(applied)) return applied;
    merged.modifiedUtc = NowUtc();
    merged.source = incoming.source.empty() ? L"cli" : SanitizeField(incoming.source, nullptr);
    merged.schema = 1;
    merged.mask = FieldAll;
    if (!incoming.unknown.empty()) merged.unknown = incoming.unknown;
    return Status{S_OK, {}};
}

}  // namespace

void SetFailPoint(int phase) { g_failPoint.store(phase); }

std::wstring LastError() { return g_lastError; }

bool IsCloudPlaceholder(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(LongPath(path).c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return false;
    const DWORD recall = FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS | FILE_ATTRIBUTE_RECALL_ON_OPEN;
    return (attributes & recall) != 0;
}

Status ReadStream(IStream* stream, FileInfo& info, bool& found) {
    if (!stream) return Fail(E_POINTER, L"stream is null");
    ComPtr<ILockBytes> bytes(new StreamLockBytes(stream));
    ComPtr<IStorage> storage;
    const HRESULT hr = StgOpenStorageOnILockBytes(bytes.Get(), nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0,
                                                  storage.Put());
    if (FAILED(hr)) return Fail(hr, L"stream is not a readable compound file");
    return ReadStorage(storage.Get(), info, found);
}

Status WriteStream(IStream* stream, const FileInfo& info) {
    if (!stream) return Fail(E_POINTER, L"stream is null");
    ComPtr<ILockBytes> bytes(new StreamLockBytes(stream));
    ComPtr<IStorage> storage;
    HRESULT hr = StgOpenStorageOnILockBytes(
        bytes.Get(), nullptr, STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_TRANSACTED, nullptr, 0, storage.Put());
    if (FAILED(hr)) return Fail(hr, L"could not open compound file for transacted write");
    Status written = WriteStorage(storage.Get(), info);
    if (!Ok(written)) {
        storage->Revert();
        return written;
    }
    hr = storage->Commit(STGC_DEFAULT);
    if (FAILED(hr)) return Fail(hr, L"transacted commit failed");
    bytes->Flush();
    return Status{S_OK, {}};
}

Status ReadAlternate(const std::wstring& path, FileInfo& info, bool& found) {
    Status ads = ReadAds(path, info, found);
    if (!Ok(ads) || found) return ads;
    return ReadSidecar(path, info, found);
}

Status WriteAlternate(const std::wstring& path, const FileInfo& incoming) {
    FileInfo merged;
    Status mergedStatus = Merge(path, incoming, merged);
    if (!Ok(mergedStatus)) return mergedStatus;
    Status ads = WriteAds(path, merged);
    if (Ok(ads)) return ads;
    return WriteSidecar(path, merged);
}

Status ReadFile(const std::wstring& path, FileInfo& info, bool& found) {
    found = false;
    info = FileInfo{};
    if (IsCloudPlaceholder(path)) return Fail(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), L"cloud placeholder is read-only");
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return Fail(HRESULT_FROM_WIN32(GetLastError()), L"file not found");
    }
    if (IsCompoundFile(path)) {
        ComPtr<IStorage> storage;
        const HRESULT hr = StgOpenStorage(StoragePath(path).c_str(), nullptr, STGM_READ | STGM_SHARE_DENY_WRITE, nullptr, 0,
                                          storage.Put());
        if (FAILED(hr)) return Fail(hr, L"could not open compound file");
        Status status = ReadStorage(storage.Get(), info, found);
        if (!Ok(status) || found) return status;
        return ReadAlternate(path, info, found);
    }
    return ReadAlternate(path, info, found);
}

Status WriteFile(const std::wstring& path, const FileInfo& incoming) {
    if (IsCloudPlaceholder(path)) return Fail(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), L"cloud placeholder is read-only");
    Status probe = ProbeExclusive(path);
    if (!Ok(probe)) return probe;
    if (!IsCompoundFile(path)) return WriteAlternate(path, incoming);

    FileInfo merged;
    Status mergedStatus = Merge(path, incoming, merged);
    if (!Ok(mergedStatus)) return mergedStatus;

    std::map<std::wstring, Digest> before;
    Status hashed = HashFile(path, before);
    if (!Ok(hashed)) return hashed;

    std::wstring temp = path + L".rfi.tmp";
    for (int attempt = 0; attempt < 50 && GetFileAttributesW(temp.c_str()) != INVALID_FILE_ATTRIBUTES; ++attempt) {
        temp = path + L".rfi" + std::to_wstring(attempt) + L".tmp";
    }
    const std::wstring tempLong = LongPath(temp);
    if (!CopyFileW(LongPath(path).c_str(), tempLong.c_str(), FALSE)) {
        return Fail(HRESULT_FROM_WIN32(GetLastError()), L"could not copy the file");
    }
    SetFileAttributesW(tempLong.c_str(), FILE_ATTRIBUTE_NORMAL);
    auto cleanup = [&]() { DeleteFileW(tempLong.c_str()); };

    ComPtr<IStorage> storage;
    HRESULT hr = StgOpenStorage(StoragePath(temp).c_str(), nullptr,
                                STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_TRANSACTED, nullptr, 0, storage.Put());
    if (FAILED(hr)) {
        cleanup();
        return Fail(hr, L"could not open the temporary compound file");
    }
    Status written = WriteStorage(storage.Get(), merged);
    if (!Ok(written)) {
        storage->Revert();
        storage.Reset();
        cleanup();
        return written;
    }
    hr = storage->Commit(STGC_DEFAULT);
    storage.Reset();
    if (FAILED(hr)) {
        cleanup();
        return Fail(hr, L"temporary compound file commit failed");
    }

    std::map<std::wstring, Digest> after;
    hashed = HashFile(temp, after);
    if (!Ok(hashed)) {
        cleanup();
        return hashed;
    }
    std::wstring mismatch;
    if (!SameExceptPayload(before, after, mismatch)) {
        cleanup();
        return Fail(E_FAIL, mismatch);
    }
    ComPtr<IStorage> verify;
    hr = StgOpenStorage(StoragePath(temp).c_str(), nullptr, STGM_READ | STGM_SHARE_DENY_WRITE, nullptr, 0, verify.Put());
    if (FAILED(hr)) {
        cleanup();
        return Fail(hr, L"temporary file failed verification");
    }
    verify.Reset();

    if (g_failPoint.load() == 1) {
        cleanup();
        return Fail(E_FAIL, L"failpoint before replace");
    }

    const bool backups = KeepBackups();
    const std::wstring backup = path + L".bak";
    if (!ReplaceFileW(LongPath(path).c_str(), tempLong.c_str(), backups ? LongPath(backup).c_str() : nullptr,
                      REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
        const DWORD error = GetLastError();
        cleanup();
        return Fail(HRESULT_FROM_WIN32(error), L"ReplaceFile failed");
    }
    g_lastError.clear();
    return Status{S_OK, {}};
}

Status ReadRaw(const std::wstring& path, std::string& json, bool& found) {
    FileInfo info;
    Status status = ReadFile(path, info, found);
    if (!Ok(status) || !found) {
        json.clear();
        return status;
    }
    return Encode(info, json);
}

Status WriteRaw(const std::wstring& path, const std::string& json) {
    if (json.size() > kMaxPayloadBytes) return Fail(E_INVALIDARG, L"payload exceeds 4 KB");
    FileInfo info;
    Status decoded = Decode(json, info);
    if (!Ok(decoded)) return decoded;
    info.mask = FieldAll;
    if (info.source.empty()) info.source = L"cli";
    return WriteFile(path, info);
}

Status ListStreams(const std::wstring& path, std::wstring& report) {
    report.clear();
    if (!IsCompoundFile(path)) return Fail(STG_E_INVALIDHEADER, L"not a compound file");
    ComPtr<IStorage> storage;
    const HRESULT hr = StgOpenStorage(StoragePath(path).c_str(), nullptr, STGM_READ | STGM_SHARE_DENY_WRITE, nullptr, 0,
                                      storage.Put());
    if (FAILED(hr)) return Fail(hr, L"could not open compound file");
    std::map<std::wstring, Digest> streams;
    Status status = Enumerate(storage.Get(), L"", streams);
    if (!Ok(status)) return status;
    for (const auto& item : streams) {
        report += item.first;
        report += L"\t";
        report += std::to_wstring(item.second.size);
        report += L"\r\n";
    }
    return Status{S_OK, {}};
}

}  // namespace rfi

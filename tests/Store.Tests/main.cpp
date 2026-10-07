#include "rfi/Store.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <ole2.h>
#include <shlwapi.h>

#include <cstdio>
#include <string>
#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shlwapi.lib")

namespace {

int g_fails = 0;

void Check(bool condition, const wchar_t* text, int line) {
    if (!condition) {
        wprintf(L"FAIL %d: %s\n", line, text);
        ++g_fails;
    }
}

#define CHECK(cond) Check((cond), L#cond, __LINE__)

std::wstring TempPath(const wchar_t* name) {
    wchar_t dir[MAX_PATH]{};
    GetTempPathW(MAX_PATH, dir);
    return std::wstring(dir) + L"rfi-" + name;
}

bool WriteStreamBytes(IStorage* storage, const wchar_t* name, const void* data, ULONG size) {
    IStream* stream = nullptr;
    HRESULT hr = storage->CreateStream(name, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stream);
    if (FAILED(hr)) return false;
    ULONG written = 0;
    hr = stream->Write(data, size, &written);
    stream->Release();
    return SUCCEEDED(hr) && written == size;
}

bool CreateSample(const std::wstring& path) {
    DeleteFileW(path.c_str());
    IStorage* storage = nullptr;
    HRESULT hr = StgCreateStorageEx(path.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, STGFMT_STORAGE, 0,
                                   nullptr, nullptr, IID_IStorage, reinterpret_cast<void**>(&storage));
    if (FAILED(hr)) return false;
    const char basic[] = "BasicFileInfo-sample";
    const bool basicOk = WriteStreamBytes(storage, L"BasicFileInfo", basic, sizeof(basic));
    IStorage* global = nullptr;
    hr = storage->CreateStorage(L"Global", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &global);
    bool latestOk = false;
    if (SUCCEEDED(hr)) {
        const char latest[] = "latest-bytes";
        latestOk = WriteStreamBytes(global, L"Latest", latest, sizeof(latest));
        global->Commit(STGC_DEFAULT);
        global->Release();
    }
    storage->Commit(STGC_DEFAULT);
    storage->Release();
    return basicOk && latestOk;
}

std::string ReadNamed(const std::wstring& path, const wchar_t* storageName, const wchar_t* streamName) {
    IStorage* storage = nullptr;
    if (FAILED(StgOpenStorage(path.c_str(), nullptr, STGM_READ | STGM_SHARE_DENY_NONE, nullptr, 0, &storage))) return {};
    IStorage* current = storage;
    IStorage* child = nullptr;
    if (storageName) {
        if (FAILED(storage->OpenStorage(storageName, nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, nullptr, 0, &child))) {
            storage->Release();
            return {};
        }
        current = child;
    }
    IStream* stream = nullptr;
    std::string data;
    if (SUCCEEDED(current->OpenStream(streamName, nullptr, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stream))) {
        STATSTG stat{};
        stream->Stat(&stat, STATFLAG_NONAME);
        data.resize(static_cast<size_t>(stat.cbSize.QuadPart));
        ULONG read = 0;
        if (!data.empty()) stream->Read(data.data(), static_cast<ULONG>(data.size()), &read);
        data.resize(read);
        stream->Release();
    }
    if (child) child->Release();
    storage->Release();
    return data;
}

std::vector<unsigned char> FileBytes(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size{};
    GetFileSizeEx(file, &size);
    std::vector<unsigned char> data(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    if (!data.empty()) ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &read, nullptr);
    CloseHandle(file);
    data.resize(read);
    return data;
}

rfi::FileInfo Sample() {
    rfi::FileInfo info;
    info.mask = rfi::FieldAll;
    info.discipline = L"Ää Öö العربية 中文 😀";
    info.location = L"L01";
    info.originator = L"ABC";
    info.subDiscipline = L"Facade";
    info.documentType = L"Model";
    info.program = L"Housing";
    info.subProgram = L"Phase1";
    info.source = L"cli";
    return info;
}

void TestRoundTrip() {
    const std::wstring path = TempPath(L"round.cfb");
    CHECK(CreateSample(path));
    const auto basicBefore = ReadNamed(path, nullptr, L"BasicFileInfo");
    const auto latestBefore = ReadNamed(path, L"Global", L"Latest");
    CHECK(rfi::Ok(rfi::WriteFile(path, Sample())));
    rfi::FileInfo read;
    bool found = false;
    CHECK(rfi::Ok(rfi::ReadFile(path, read, found)));
    CHECK(found);
    CHECK(read.discipline == L"Ää Öö العربية 中文 😀");
    CHECK(read.location == L"L01");
    CHECK(read.originator == L"ABC");
    CHECK(read.subDiscipline == L"Facade");
    CHECK(read.documentType == L"Model");
    CHECK(read.program == L"Housing");
    CHECK(read.subProgram == L"Phase1");
    CHECK(read.source == L"cli");
    CHECK(read.modifiedUtc.size() == 20);
    CHECK(ReadNamed(path, nullptr, L"BasicFileInfo") == basicBefore);
    CHECK(ReadNamed(path, L"Global", L"Latest") == latestBefore);
    DeleteFileW(path.c_str());
}

void TestLimits() {
    const std::wstring path = TempPath(L"limits.cfb");
    CHECK(CreateSample(path));
    const auto before = FileBytes(path);
    rfi::FileInfo info;
    info.mask = rfi::FieldDiscipline;
    info.discipline = std::wstring(256, L'A');
    info.source = L"cli";
    CHECK(rfi::Ok(rfi::WriteFile(path, info)));
    rfi::FileInfo read;
    bool found = false;
    CHECK(rfi::Ok(rfi::ReadFile(path, read, found)));
    CHECK(read.discipline.size() == 256);

    info.discipline = std::wstring(257, L'B');
    CHECK(!rfi::Ok(rfi::WriteFile(path, info)));
    CHECK(rfi::Ok(rfi::ReadFile(path, read, found)));
    CHECK(read.discipline.size() == 256);

    info.discipline = L"  A\u0001B\u007F C  ";
    CHECK(rfi::Ok(rfi::WriteFile(path, info)));
    CHECK(rfi::Ok(rfi::ReadFile(path, read, found)));
    CHECK(read.discipline == L"AB C");

    info.discipline.clear();
    info.mask = rfi::FieldAll;
    CHECK(rfi::Ok(rfi::WriteFile(path, info)));
    CHECK(rfi::Ok(rfi::ReadFile(path, read, found)));
    CHECK(read.discipline.empty());
    DeleteFileW(path.c_str());
    (void)before;
}

void TestUnknownPreserved() {
    const std::wstring path = TempPath(L"unknown.cfb");
    CHECK(CreateSample(path));
    const std::string raw =
        "{\"schema\":1,\"discipline\":\"ARC\",\"location\":\"\",\"originator\":\"\",\"subDiscipline\":\"\","
        "\"documentType\":\"\",\"program\":\"\",\"subProgram\":\"\",\"modifiedUtc\":\"2026-01-01T00:00:00Z\","
        "\"source\":\"cli\",\"extra\":{\"kept\":true}}";
    CHECK(rfi::Ok(rfi::WriteRaw(path, raw)));
    rfi::FileInfo update;
    update.mask = rfi::FieldLocation;
    update.location = L"Roof";
    update.source = L"cli";
    CHECK(rfi::Ok(rfi::WriteFile(path, update)));
    rfi::FileInfo read;
    bool found = false;
    CHECK(rfi::Ok(rfi::ReadFile(path, read, found)));
    CHECK(read.discipline == L"ARC");
    CHECK(read.location == L"Roof");
    CHECK(read.unknown.size() == 1);
    if (!read.unknown.empty()) CHECK(read.unknown[0].first == "extra");
    DeleteFileW(path.c_str());
}

void TestFailpointAndLock() {
    const std::wstring path = TempPath(L"safe.cfb");
    CHECK(CreateSample(path));
    const auto before = FileBytes(path);
    rfi::SetFailPoint(1);
    CHECK(!rfi::Ok(rfi::WriteFile(path, Sample())));
    CHECK(FileBytes(path) == before);
    rfi::SetFailPoint(0);

    HANDLE locked = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    CHECK(locked != INVALID_HANDLE_VALUE);
    const rfi::Status denied = rfi::WriteFile(path, Sample());
    CHECK(denied.hr == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION));
    CloseHandle(locked);
    CHECK(FileBytes(path) == before);
    DeleteFileW(path.c_str());
}

void TestCorrupt() {
    const wchar_t* names[] = {L"empty.cfb", L"text.cfb", L"trunc.cfb"};
    for (const wchar_t* name : names) DeleteFileW(TempPath(name).c_str());
    {
        const std::wstring path = TempPath(L"empty.cfb");
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK(file != INVALID_HANDLE_VALUE);
        CloseHandle(file);
        rfi::FileInfo info;
        bool found = false;
        CHECK(rfi::Ok(rfi::ReadFile(path, info, found)));
        CHECK(!found);
        DeleteFileW(path.c_str());
    }
    {
        const std::wstring path = TempPath(L"text.cfb");
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        const char text[] = "not-a-compound-file";
        DWORD written = 0;
        WriteFile(file, text, sizeof(text), &written, nullptr);
        CloseHandle(file);
        rfi::FileInfo info;
        bool found = false;
        CHECK(rfi::Ok(rfi::ReadFile(path, info, found)));
        CHECK(!found);
        DeleteFileW(path.c_str());
    }
    {
        const std::wstring path = TempPath(L"trunc.cfb");
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        const unsigned char magic[8] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
        DWORD written = 0;
        WriteFile(file, magic, sizeof(magic), &written, nullptr);
        CloseHandle(file);
        rfi::FileInfo info;
        bool found = false;
        CHECK(!rfi::Ok(rfi::ReadFile(path, info, found)));
        DeleteFileW(path.c_str());
    }
}

void TestAlternateStores() {
    const std::wstring path = TempPath(L"alt.cfb");
    CHECK(CreateSample(path));
    CHECK(rfi::Ok(rfi::WriteAds(path, Sample())));
    rfi::FileInfo read;
    bool found = false;
    CHECK(rfi::Ok(rfi::ReadAds(path, read, found)));
    CHECK(found);
    CHECK(read.discipline == Sample().discipline);
    CHECK(rfi::Ok(rfi::WriteSidecar(path, Sample())));
    CHECK(rfi::Ok(rfi::ReadSidecar(path, read, found)));
    CHECK(read.originator == L"ABC");
    DeleteFileW((path + L":RvtFileInfo").c_str());
    DeleteFileW((path + L".fileinfo.json").c_str());
    DeleteFileW(path.c_str());
}

void TestNonCompoundWrite() {
    const std::wstring path = TempPath(L"sample.dwg");
    DeleteFileW(path.c_str());
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    const char header[] = "AC1032-dummy";
    DWORD written = 0;
    WriteFile(file, header, sizeof(header), &written, nullptr);
    CloseHandle(file);
    CHECK(!rfi::IsCompoundFile(path));
    CHECK(rfi::Ok(rfi::WriteFile(path, Sample())));
    rfi::FileInfo read;
    bool found = false;
    CHECK(rfi::Ok(rfi::ReadFile(path, read, found)));
    CHECK(found);
    CHECK(read.program == L"Housing");
    CHECK(ReadNamed(path, nullptr, L"BasicFileInfo").empty());
    DeleteFileW((path + L":RvtFileInfo").c_str());
    DeleteFileW((path + L".fileinfo.json").c_str());
    DeleteFileW(path.c_str());
}

void TestCopyRename() {
    const std::wstring path = TempPath(L"copy-src.cfb");
    const std::wstring copied = TempPath(L"copy-dst.cfb");
    CHECK(CreateSample(path));
    CHECK(rfi::Ok(rfi::WriteFile(path, Sample())));
    CHECK(CopyFileW(path.c_str(), copied.c_str(), FALSE));
    MoveFileExW(copied.c_str(), TempPath(L"renamed.cfb").c_str(), MOVEFILE_REPLACE_EXISTING);
    rfi::FileInfo read;
    bool found = false;
    CHECK(rfi::Ok(rfi::ReadFile(TempPath(L"renamed.cfb"), read, found)));
    CHECK(read.program == L"Housing");
    DeleteFileW(path.c_str());
    DeleteFileW(TempPath(L"renamed.cfb").c_str());
}

void TestPerf() {
    const std::wstring path = TempPath(L"perf.cfb");
    DeleteFileW(path.c_str());
    IStorage* storage = nullptr;
    HRESULT hr = StgCreateStorageEx(path.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, STGFMT_STORAGE, 0,
                                   nullptr, nullptr, IID_IStorage, reinterpret_cast<void**>(&storage));
    CHECK(SUCCEEDED(hr));
    IStream* bulk = nullptr;
    hr = storage->CreateStream(L"Bulk", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &bulk);
    CHECK(SUCCEEDED(hr));
    std::vector<unsigned char> block(1024 * 1024, 0x5A);
    for (int i = 0; i < 500; ++i) {
        ULONG written = 0;
        bulk->Write(block.data(), static_cast<ULONG>(block.size()), &written);
    }
    bulk->Release();
    storage->Commit(STGC_DEFAULT);
    storage->Release();
    CHECK(rfi::Ok(rfi::WriteFile(path, Sample())));
    LARGE_INTEGER frequency{};
    LARGE_INTEGER start{};
    LARGE_INTEGER end{};
    QueryPerformanceFrequency(&frequency);
    double worst = 0;
    for (int i = 0; i < 20; ++i) {
        QueryPerformanceCounter(&start);
        rfi::FileInfo read;
        bool found = false;
        CHECK(rfi::Ok(rfi::ReadFile(path, read, found)));
        QueryPerformanceCounter(&end);
        const double ms = (end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
        if (ms > worst) worst = ms;
    }
    wprintf(L"PERF worst read of 500MB file: %.2f ms\n", worst);
    CHECK(worst < 50.0);
    DeleteFileW(path.c_str());
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return 1;
    TestRoundTrip();
    TestLimits();
    TestUnknownPreserved();
    TestFailpointAndLock();
    TestCorrupt();
    TestAlternateStores();
    TestNonCompoundWrite();
    TestCopyRename();
    bool perf = false;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--perf") == 0) perf = true;
    }
    if (perf) TestPerf();
    wprintf(L"%s (%d failed)\n", g_fails == 0 ? L"PASS" : L"FAIL", g_fails);
    CoUninitialize();
    return g_fails == 0 ? 0 : 1;
}

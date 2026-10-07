#include "rfi/Store.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shlwapi.h>
#include <shobjidl.h>

#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "propsys.lib")
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
    return std::wstring(dir) + L"rfi-shell-" + name;
}

bool CreateSample(const std::wstring& path) {
    DeleteFileW(path.c_str());
    IStorage* storage = nullptr;
    HRESULT hr = StgCreateStorageEx(path.c_str(), STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, STGFMT_STORAGE, 0,
                                   nullptr, nullptr, IID_IStorage, reinterpret_cast<void**>(&storage));
    if (FAILED(hr)) return false;
    storage->Commit(STGC_DEFAULT);
    storage->Release();
    return true;
}

class MockStore : public IInitializeWithStream, public IPropertyStore, public IPropertyStoreCapabilities {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IPropertyStore) {
            *object = static_cast<IPropertyStore*>(this);
        } else if (riid == IID_IInitializeWithStream) {
            *object = static_cast<IInitializeWithStream*>(this);
        } else if (riid == IID_IPropertyStoreCapabilities) {
            *object = static_cast<IPropertyStoreCapabilities*>(this);
        } else {
            *object = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG left = InterlockedDecrement(&refs_);
        if (left == 0) delete this;
        return left;
    }
    STDMETHODIMP Initialize(IStream*, DWORD) override { return S_OK; }
    STDMETHODIMP GetCount(DWORD* count) override {
        if (!count) return E_POINTER;
        *count = 1;
        return S_OK;
    }
    STDMETHODIMP GetAt(DWORD index, PROPERTYKEY* key) override {
        if (!key || index != 0) return E_INVALIDARG;
        *key = PKEY_Title;
        return S_OK;
    }
    STDMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT* value) override {
        if (!value) return E_POINTER;
        PropVariantInit(value);
        if (IsEqualGUID(key.fmtid, PKEY_Title.fmtid) && key.pid == PKEY_Title.pid) {
            return InitPropVariantFromString(L"FromMock", value);
        }
        return S_OK;
    }
    STDMETHODIMP SetValue(REFPROPERTYKEY, REFPROPVARIANT) override { return S_OK; }
    STDMETHODIMP Commit() override { return S_OK; }
    STDMETHODIMP IsPropertyWritable(REFPROPERTYKEY) override { return S_FALSE; }

private:
    long refs_ = 1;
};

using DllGetClassObjectFn = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**);
using SetDelegateFn = void(__cdecl*)(IUnknown*);

void TestSchema();

HRESULT CreateHandler(HMODULE library, IPropertyStore** store) {
    auto getClass = reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(library, "DllGetClassObject"));
    if (!getClass) return E_FAIL;
    IClassFactory* factory = nullptr;
    HRESULT hr = getClass(rfi::CLSID_RvtFileInfoPropertyHandler, IID_IClassFactory, reinterpret_cast<void**>(&factory));
    if (FAILED(hr)) return hr;
    hr = factory->CreateInstance(nullptr, IID_IPropertyStore, reinterpret_cast<void**>(store));
    factory->Release();
    return hr;
}

void TestHandler() {
    const std::wstring path = TempPath(L"handler.cfb");
    CHECK(CreateSample(path));
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dll = exe;
    const size_t slash = dll.find_last_of(L"\\/");
    dll = dll.substr(0, slash + 1) + L"RvtFileInfo.ShellHandler.dll";
    HMODULE library = LoadLibraryW(dll.c_str());
    CHECK(library != nullptr);
    if (!library) return;

    IPropertyStore* store = nullptr;
    CHECK(SUCCEEDED(CreateHandler(library, &store)));
    if (!store) return;
    IInitializeWithStream* init = nullptr;
    CHECK(SUCCEEDED(store->QueryInterface(IID_PPV_ARGS(&init))));
    IStream* stream = nullptr;
    CHECK(SUCCEEDED(SHCreateStreamOnFileEx(path.c_str(), STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, FALSE, nullptr, &stream)));
    CHECK(SUCCEEDED(init->Initialize(stream, STGM_READWRITE)));
    DWORD count = 0;
    CHECK(SUCCEEDED(store->GetCount(&count)));
    CHECK(count >= 7);
    PROPERTYKEY key{rfi::FMTID_RvtFileInfo, rfi::kPidDiscipline};
    PROPVARIANT value{};
    CHECK(SUCCEEDED(InitPropVariantFromString(L"ARC", &value)));
    CHECK(SUCCEEDED(store->SetValue(key, value)));
    PropVariantClear(&value);
    CHECK(SUCCEEDED(store->Commit()));
    stream->Release();
    init->Release();
    store->Release();

    rfi::FileInfo read;
    bool found = false;
    CHECK(rfi::Ok(rfi::ReadFile(path, read, found)));
    CHECK(found);
    CHECK(read.discipline == L"ARC");
    CHECK(read.source == L"explorer");

    CHECK(SUCCEEDED(CreateHandler(library, &store)));
    if (!store) return;
    IInitializeWithFile* fileInit = nullptr;
    CHECK(SUCCEEDED(store->QueryInterface(IID_PPV_ARGS(&fileInit))));
    CHECK(SUCCEEDED(fileInit->Initialize(path.c_str(), STGM_READWRITE)));
    CHECK(SUCCEEDED(InitPropVariantFromString(L"L02", &value)));
    PROPERTYKEY locationKey{rfi::FMTID_RvtFileInfo, rfi::kPidLocation};
    CHECK(SUCCEEDED(store->SetValue(locationKey, value)));
    PropVariantClear(&value);
    CHECK(SUCCEEDED(store->Commit()));
    fileInit->Release();
    store->Release();
    store = nullptr;
    CHECK(rfi::Ok(rfi::ReadFile(path, read, found)));
    CHECK(read.location == L"L02");

    const std::wstring dwg = TempPath(L"handler.dwg");
    DeleteFileW(dwg.c_str());
    HANDLE dummy = CreateFileW(dwg.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    const char ac[] = "AC1032";
    DWORD wrote = 0;
    WriteFile(dummy, ac, sizeof(ac), &wrote, nullptr);
    CloseHandle(dummy);
    CHECK(SUCCEEDED(CreateHandler(library, &store)));
    if (store) {
        IInitializeWithFile* dwgInit = nullptr;
        CHECK(SUCCEEDED(store->QueryInterface(IID_PPV_ARGS(&dwgInit))));
        CHECK(SUCCEEDED(dwgInit->Initialize(dwg.c_str(), STGM_READWRITE)));
        CHECK(SUCCEEDED(InitPropVariantFromString(L"ARC", &value)));
        CHECK(SUCCEEDED(store->SetValue(key, value)));
        PropVariantClear(&value);
        CHECK(SUCCEEDED(store->Commit()));
        dwgInit->Release();
        store->Release();
        store = nullptr;
        CHECK(rfi::Ok(rfi::ReadFile(dwg, read, found)));
        CHECK(read.discipline == L"ARC");
    }
    DeleteFileW((dwg + L":RvtFileInfo").c_str());
    DeleteFileW((dwg + L".fileinfo.json").c_str());
    DeleteFileW(dwg.c_str());

    store = nullptr;
    init = nullptr;
    stream = nullptr;
    CHECK(SUCCEEDED(CreateHandler(library, &store)));
    if (!store) return;
    CHECK(SUCCEEDED(store->QueryInterface(IID_PPV_ARGS(&init))));
    CHECK(SUCCEEDED(SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, 0, FALSE, nullptr, &stream)));
    CHECK(SUCCEEDED(init->Initialize(stream, STGM_READ)));
    IPropertyStoreCapabilities* caps = nullptr;
    CHECK(SUCCEEDED(store->QueryInterface(IID_PPV_ARGS(&caps))));
    CHECK(caps->IsPropertyWritable(key) == S_OK);
    CHECK(InitPropVariantFromString(L"MEP", &value) == S_OK);
    CHECK(SUCCEEDED(store->SetValue(key, value)));
    PropVariantClear(&value);
    PropVariantInit(&value);
    CHECK(SUCCEEDED(store->GetValue(key, &value)));
    CHECK(value.vt == VT_LPWSTR);
    CHECK(wcscmp(value.pwszVal, L"MEP") == 0);
    PropVariantClear(&value);
    caps->Release();
    stream->Release();
    init->Release();
    store->Release();

    auto setDelegate = reinterpret_cast<SetDelegateFn>(GetProcAddress(library, "RfiTest_SetDelegate"));
    CHECK(setDelegate != nullptr);
    if (!setDelegate) return;
    MockStore* mock = new MockStore();
    setDelegate(static_cast<IPropertyStore*>(mock));
    store = nullptr;
    CHECK(SUCCEEDED(CreateHandler(library, &store)));
    if (!store) return;
    CHECK(SUCCEEDED(store->QueryInterface(IID_PPV_ARGS(&init))));
    CHECK(SUCCEEDED(SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, 0, FALSE, nullptr, &stream)));
    CHECK(SUCCEEDED(init->Initialize(stream, STGM_READ)));
    PropVariantInit(&value);
    CHECK(SUCCEEDED(store->GetValue(PKEY_Title, &value)));
    CHECK(value.vt == VT_LPWSTR && wcscmp(value.pwszVal, L"FromMock") == 0);
    PropVariantClear(&value);
    stream->Release();
    init->Release();
    store->Release();
    setDelegate(nullptr);
    mock->Release();

    FreeLibrary(library);
    DeleteFileW(path.c_str());
    TestSchema();
}

std::wstring FindPropdesc() {
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir = exe;
    for (int i = 0; i < 8; ++i) {
        const size_t slash = dir.find_last_of(L"\\/");
        if (slash == std::wstring::npos) break;
        dir.resize(slash);
        const std::wstring candidate = dir + L"\\src\\ShellHandler\\RvtFileInfo.propdesc";
        if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) return candidate;
    }
    return {};
}

void TestSchema() {
    const std::wstring path = FindPropdesc();
    CHECK(!path.empty());
    if (path.empty()) return;
    const HRESULT registered = PSRegisterPropertySchema(path.c_str());
    if (registered == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) || registered == HRESULT_FROM_WIN32(ERROR_PRIVILEGE_NOT_HELD)) {
        wprintf(L"SCHEMA NOT VERIFIED: PSRegisterPropertySchema returned 0x%08X\n", registered);
        return;
    }
    if (FAILED(registered)) {
        wprintf(L"FAIL schema register 0x%08X\n", registered);
        ++g_fails;
        return;
    }
    IPropertyDescription* description = nullptr;
    const HRESULT hr = PSGetPropertyDescriptionByName(L"RvtFileInfo.SubDiscipline", IID_PPV_ARGS(&description));
    CHECK(SUCCEEDED(hr));
    if (description) {
        wchar_t* label = nullptr;
        CHECK(SUCCEEDED(description->GetDisplayName(&label)));
        CHECK(label && wcscmp(label, L"Sub Discipline") == 0);
        CoTaskMemFree(label);
        description->Release();
    }
    PSUnregisterPropertySchema(path.c_str());
}

}  // namespace

int wmain() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    CHECK(SUCCEEDED(hr));
    TestHandler();
    CoUninitialize();
    wprintf(L"%s (%d failed)\n", g_fails == 0 ? L"PASS" : L"FAIL", g_fails);
    return g_fails == 0 ? 0 : 1;
}

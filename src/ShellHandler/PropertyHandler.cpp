#include "rfi/Store.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <propsys.h>
#include <atlbase.h>
#include <atlcom.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shlobj.h>

#include <cwctype>
#include <string>
#include <vector>

#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

namespace {

const GUID kHandlerClsid = rfi::CLSID_RvtFileInfoPropertyHandler;

const PROPERTYKEY kKeys[] = {
    {rfi::FMTID_RvtFileInfo, rfi::kPidDiscipline},
    {rfi::FMTID_RvtFileInfo, rfi::kPidLocation},
    {rfi::FMTID_RvtFileInfo, rfi::kPidOriginator},
    {rfi::FMTID_RvtFileInfo, rfi::kPidSubDiscipline},
    {rfi::FMTID_RvtFileInfo, rfi::kPidDocumentType},
    {rfi::FMTID_RvtFileInfo, rfi::kPidProgram},
    {rfi::FMTID_RvtFileInfo, rfi::kPidSubProgram},
};

std::wstring* FieldFor(rfi::FileInfo& info, DWORD pid) {
    switch (pid) {
    case rfi::kPidDiscipline: return &info.discipline;
    case rfi::kPidLocation: return &info.location;
    case rfi::kPidOriginator: return &info.originator;
    case rfi::kPidSubDiscipline: return &info.subDiscipline;
    case rfi::kPidDocumentType: return &info.documentType;
    case rfi::kPidProgram: return &info.program;
    case rfi::kPidSubProgram: return &info.subProgram;
    default: return nullptr;
    }
}

bool IsOurKey(REFPROPERTYKEY key) {
    return IsEqualGUID(key.fmtid, rfi::FMTID_RvtFileInfo) && key.pid >= rfi::kPidDiscipline && key.pid <= rfi::kPidSubProgram;
}

bool SameKey(REFPROPERTYKEY left, REFPROPERTYKEY right) {
    return left.pid == right.pid && IsEqualGUID(left.fmtid, right.fmtid);
}

std::wstring ReadRegistryString(HKEY root, const wchar_t* subkey, const wchar_t* name) {
    DWORD size = 0;
    if (RegGetValueW(root, subkey, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS || size < sizeof(wchar_t)) {
        return {};
    }
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, subkey, name, RRF_RT_REG_SZ, nullptr, value.data(), &size) != ERROR_SUCCESS) return {};
    value.resize(wcsnlen(value.c_str(), value.size()));
    return value;
}

std::wstring ExtensionOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    const size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) return {};
    std::wstring ext = path.substr(dot);
    for (wchar_t& ch : ext) ch = static_cast<wchar_t>(towlower(ch));
    return ext;
}

IUnknown* g_testDelegate = nullptr;

}  // namespace

class ATL_NO_VTABLE CRvtPropertyHandler :
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CRvtPropertyHandler, &kHandlerClsid>,
    public IInitializeWithStream,
    public IInitializeWithFile,
    public IInitializeWithItem,
    public IPropertyStore,
    public IPropertyStoreCapabilities {
public:
    DECLARE_NO_REGISTRY()
    DECLARE_NOT_AGGREGATABLE(CRvtPropertyHandler)

    BEGIN_COM_MAP(CRvtPropertyHandler)
        COM_INTERFACE_ENTRY(IInitializeWithStream)
        COM_INTERFACE_ENTRY(IInitializeWithFile)
        COM_INTERFACE_ENTRY(IInitializeWithItem)
        COM_INTERFACE_ENTRY(IPropertyStore)
        COM_INTERFACE_ENTRY(IPropertyStoreCapabilities)
    END_COM_MAP()

    STDMETHODIMP Initialize(IStream* stream, DWORD mode) override {
        try {
            if (!stream) return E_POINTER;
            stream_ = stream;
            CapturePathFromStream(stream);
            writable_ = !IsReadOnlySource();
            AttachDelegate(stream, mode);
            bool found = false;
            const rfi::Status status = rfi::ReadStream(stream, info_, found);
            compound_ = rfi::Ok(status);
            if (!compound_ || !found) {
                if (!path_.empty()) {
                    rfi::FileInfo fromFile;
                    bool fileFound = false;
                    const rfi::Status fileStatus = rfi::ReadFile(path_, fromFile, fileFound);
                    if (rfi::Ok(fileStatus) && fileFound) {
                        info_ = fromFile;
                        found = true;
                    }
                }
                if (!found) info_ = rfi::FileInfo{};
            }
            CollectKeys();
            return S_OK;
        } catch (...) {
            return E_FAIL;
        }
    }

    STDMETHODIMP Initialize(LPCWSTR path, DWORD) override {
        try {
            if (!path || !*path) return E_INVALIDARG;
            path_ = path;
            stream_.Release();
            compound_ = rfi::IsCompoundFile(path_);
            writable_ = !IsReadOnlySource();
            AttachDelegate(nullptr, STGM_READWRITE);
            bool found = false;
            const rfi::Status status = rfi::ReadFile(path_, info_, found);
            if (!rfi::Ok(status)) return status.hr;
            if (!found) info_ = rfi::FileInfo{};
            CollectKeys();
            return S_OK;
        } catch (...) {
            return E_FAIL;
        }
    }

    STDMETHODIMP Initialize(IShellItem* item, DWORD mode) override {
        try {
            if (!item) return E_POINTER;
            PWSTR name = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name)) && name) {
                path_ = name;
                CoTaskMemFree(name);
            }
            CComPtr<IStream> stream;
            if (SUCCEEDED(item->BindToHandler(nullptr, BHID_Stream, IID_PPV_ARGS(&stream))) && stream) {
                return Initialize(static_cast<IStream*>(stream), mode);
            }
            if (path_.empty()) return E_FAIL;
            return Initialize(path_.c_str(), mode);
        } catch (...) {
            return E_FAIL;
        }
    }

    STDMETHODIMP GetCount(DWORD* count) override {
        try {
            if (!count) return E_POINTER;
            *count = static_cast<DWORD>(keys_.size());
            return S_OK;
        } catch (...) {
            return E_FAIL;
        }
    }

    STDMETHODIMP GetAt(DWORD index, PROPERTYKEY* key) override {
        try {
            if (!key) return E_POINTER;
            if (index >= keys_.size()) return E_INVALIDARG;
            *key = keys_[index];
            return S_OK;
        } catch (...) {
            return E_FAIL;
        }
    }

    STDMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT* value) override {
        try {
            if (!value) return E_POINTER;
            PropVariantInit(value);
            if (IsOurKey(key)) {
                const std::wstring* field = FieldFor(info_, key.pid);
                if (!field || field->empty()) return S_OK;
                return InitPropVariantFromString(field->c_str(), value);
            }
            if (inner_) return inner_->GetValue(key, value);
            return S_OK;
        } catch (...) {
            return E_FAIL;
        }
    }

    STDMETHODIMP SetValue(REFPROPERTYKEY key, REFPROPVARIANT value) override {
        try {
            if (!writable_) return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
            if (!IsOurKey(key)) {
                if (inner_) {
                    const HRESULT hr = inner_->SetValue(key, value);
                    if (SUCCEEDED(hr)) CollectKeys();
                    return hr;
                }
                return E_INVALIDARG;
            }
            std::wstring text;
            if (value.vt == VT_EMPTY || value.vt == VT_NULL) {
                text.clear();
            } else if (value.vt == VT_LPWSTR && value.pwszVal) {
                text = value.pwszVal;
            } else if (value.vt == VT_BSTR && value.bstrVal) {
                text = value.bstrVal;
            } else {
                return E_INVALIDARG;
            }
            bool tooLong = false;
            text = rfi::SanitizeField(text, &tooLong);
            if (tooLong) return E_INVALIDARG;
            std::wstring* field = FieldFor(info_, key.pid);
            if (!field) return E_INVALIDARG;
            *field = text;
            dirty_ = true;
            return S_OK;
        } catch (...) {
            return E_FAIL;
        }
    }

    STDMETHODIMP Commit() override {
        try {
            if (inner_) {
                const HRESULT hr = inner_->Commit();
                if (FAILED(hr)) return hr;
            }
            if (!dirty_) return S_OK;
            if (!writable_) return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
            info_.modifiedUtc = rfi::NowUtc();
            info_.source = L"explorer";
            info_.schema = 1;
            info_.mask = rfi::FieldAll;
            const HRESULT hr = Persist();
            if (FAILED(hr)) return hr;
            dirty_ = false;
            return S_OK;
        } catch (...) {
            return E_FAIL;
        }
    }

    STDMETHODIMP IsPropertyWritable(REFPROPERTYKEY key) override {
        try {
            if (!IsOurKey(key)) {
                CComPtr<IPropertyStoreCapabilities> capabilities;
                if (inner_ && SUCCEEDED(inner_->QueryInterface(IID_PPV_ARGS(&capabilities)))) {
                    return capabilities->IsPropertyWritable(key);
                }
                return S_FALSE;
            }
            return writable_ ? S_OK : S_FALSE;
        } catch (...) {
            return E_FAIL;
        }
    }

private:
    bool IsReadOnlySource() const {
        if (!path_.empty() && rfi::IsCloudPlaceholder(path_)) return true;
        return false;
    }

    void CapturePathFromStream(IStream* stream) {
        if (!path_.empty() || !stream) return;
        STATSTG stat{};
        if (FAILED(stream->Stat(&stat, STATFLAG_DEFAULT)) || !stat.pwcsName) return;
        std::wstring candidate = stat.pwcsName;
        CoTaskMemFree(stat.pwcsName);
        if (candidate.size() >= 2 && candidate[1] == L':' && GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
            path_ = candidate;
        }
    }

    HRESULT Persist() {
        if (!path_.empty()) {
            const rfi::Status status = rfi::WriteFile(path_, info_);
            if (rfi::Ok(status)) return S_OK;
            if (!compound_) return status.hr;
        }
        if (!compound_ || !stream_) return HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED);
        const rfi::Status streamed = rfi::WriteStream(stream_, info_);
        if (rfi::Ok(streamed)) return S_OK;
        CComPtr<IDestinationStreamFactory> factory;
        if (FAILED(stream_->QueryInterface(IID_PPV_ARGS(&factory)))) return streamed.hr;
        CComPtr<IStream> dest;
        HRESULT hr = factory->GetDestinationStream(&dest);
        if (FAILED(hr) || !dest) return FAILED(hr) ? hr : E_FAIL;
        LARGE_INTEGER zero{};
        hr = stream_->Seek(zero, STREAM_SEEK_SET, nullptr);
        if (FAILED(hr)) return hr;
        ULARGE_INTEGER all{};
        all.QuadPart = static_cast<ULONGLONG>(-1);
        hr = stream_->CopyTo(dest, all, nullptr, nullptr);
        if (FAILED(hr)) return hr;
        const rfi::Status copied = rfi::WriteStream(dest, info_);
        if (!rfi::Ok(copied)) return copied.hr;
        return dest->Commit(STGC_DEFAULT);
    }

    std::wstring OriginalHandlerClsid() const {
        const std::wstring ext = ExtensionOf(path_);
        if (!ext.empty()) {
            std::wstring text = ReadRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\RvtFileInfo\\OriginalHandlers", ext.c_str());
            if (text.empty()) text = ReadRegistryString(HKEY_CURRENT_USER, L"Software\\RvtFileInfo\\OriginalHandlers", ext.c_str());
            if (!text.empty()) return text;
        }
        std::wstring text = ReadRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\RvtFileInfo", L"OriginalPropertyHandler");
        if (text.empty()) text = ReadRegistryString(HKEY_CURRENT_USER, L"Software\\RvtFileInfo", L"OriginalPropertyHandler");
        return text;
    }

    void AttachDelegate(IStream* stream, DWORD mode) {
        inner_.Release();
        if (g_testDelegate) {
            CComPtr<IInitializeWithStream> init;
            if (stream && SUCCEEDED(g_testDelegate->QueryInterface(IID_PPV_ARGS(&init)))) {
                CComPtr<IStream> clone;
                IStream* used = stream;
                if (SUCCEEDED(stream->Clone(&clone))) used = clone;
                if (SUCCEEDED(init->Initialize(used, mode))) {
                    g_testDelegate->QueryInterface(IID_PPV_ARGS(&inner_));
                }
            }
            return;
        }
        const std::wstring text = OriginalHandlerClsid();
        if (text.empty()) return;
        CLSID clsid{};
        if (FAILED(CLSIDFromString(text.c_str(), &clsid)) || IsEqualGUID(clsid, kHandlerClsid)) return;
        if (stream) {
            CComPtr<IInitializeWithStream> init;
            if (SUCCEEDED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&init)))) {
                CComPtr<IStream> clone;
                IStream* used = stream;
                if (SUCCEEDED(stream->Clone(&clone))) used = clone;
                if (SUCCEEDED(init->Initialize(used, mode))) {
                    init->QueryInterface(IID_PPV_ARGS(&inner_));
                    return;
                }
            }
        }
        if (path_.empty()) return;
        CComPtr<IInitializeWithFile> file;
        if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&file)))) return;
        if (SUCCEEDED(file->Initialize(path_.c_str(), mode))) file->QueryInterface(IID_PPV_ARGS(&inner_));
    }

    void CollectKeys() {
        keys_.clear();
        keys_.insert(keys_.end(), std::begin(kKeys), std::end(kKeys));
        if (!inner_) return;
        DWORD count = 0;
        if (FAILED(inner_->GetCount(&count))) return;
        for (DWORD index = 0; index < count; ++index) {
            PROPERTYKEY key{};
            if (FAILED(inner_->GetAt(index, &key)) || IsOurKey(key)) continue;
            bool exists = false;
            for (const PROPERTYKEY& have : keys_) {
                if (SameKey(have, key)) exists = true;
            }
            if (!exists) keys_.push_back(key);
        }
    }

    CComPtr<IStream> stream_;
    CComPtr<IPropertyStore> inner_;
    rfi::FileInfo info_;
    std::vector<PROPERTYKEY> keys_;
    std::wstring path_;
    bool writable_ = true;
    bool dirty_ = false;
    bool compound_ = false;
};

void PublishTestDelegate(IUnknown* unknown) {
    if (g_testDelegate) g_testDelegate->Release();
    g_testDelegate = unknown;
    if (g_testDelegate) g_testDelegate->AddRef();
}

OBJECT_ENTRY_AUTO(kHandlerClsid, CRvtPropertyHandler)

extern "C" __declspec(dllexport) void RfiTest_SetDelegate(IUnknown* unknown) {
    PublishTestDelegate(unknown);
}

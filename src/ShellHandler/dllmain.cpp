#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include <strsafe.h>

#include "rfi/Store.h"

namespace {
const GUID kHandlerClsid = rfi::CLSID_RvtFileInfoPropertyHandler;
HMODULE g_module = nullptr;

class CRvtModule : public CAtlDllModuleT<CRvtModule> {};
CRvtModule g_atlModule;

bool WriteString(HKEY root, const wchar_t* key, const wchar_t* name, const wchar_t* value) {
    return RegSetKeyValueW(root, key, name, REG_SZ, value, static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t))) ==
           ERROR_SUCCESS;
}

}  // namespace

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return g_atlModule.DllMain(reason, reserved);
}

STDAPI DllCanUnloadNow() { return g_atlModule.DllCanUnloadNow(); }

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID* object) {
    return g_atlModule.DllGetClassObject(clsid, iid, object);
}

STDAPI DllRegisterServer() {
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(g_module, path, MAX_PATH)) return HRESULT_FROM_WIN32(GetLastError());
    wchar_t clsid[64]{};
    StringFromGUID2(kHandlerClsid, clsid, 64);
    wchar_t key[160]{};
    StringCchPrintfW(key, 160, L"CLSID\\%s", clsid);
    HKEY root = HKEY_LOCAL_MACHINE;
    HKEY classes = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes", 0, nullptr, 0, KEY_SET_VALUE | KEY_CREATE_SUB_KEY, nullptr,
                        &classes, nullptr) != ERROR_SUCCESS) {
        root = HKEY_CURRENT_USER;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes", 0, nullptr, 0, KEY_SET_VALUE | KEY_CREATE_SUB_KEY, nullptr,
                            &classes, nullptr) != ERROR_SUCCESS) {
            return E_ACCESSDENIED;
        }
    }
    wchar_t inproc[180]{};
    StringCchPrintfW(inproc, 180, L"%s\\InprocServer32", key);
    const bool ok = WriteString(classes, key, nullptr, L"RvtFileInfo Property Handler") &&
                    WriteString(classes, inproc, nullptr, path) &&
                    WriteString(classes, inproc, L"ThreadingModel", L"Both");
    RegCloseKey(classes);

    const wchar_t* extensions[] = {L".rvt", L".rfa", L".dwg", L".nwd", L".nwf", L".nwc", L".pdf"};
    for (const wchar_t* ext : extensions) {
        wchar_t handlerKey[160]{};
        StringCchPrintfW(handlerKey, 160, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\%s",
                         ext);
        HKEY handler = nullptr;
        if (RegCreateKeyExW(root, handlerKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &handler, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(handler, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(clsid),
                           static_cast<DWORD>((wcslen(clsid) + 1) * sizeof(wchar_t)));
            RegCloseKey(handler);
        }
    }
    return ok ? S_OK : E_FAIL;
}

STDAPI DllUnregisterServer() {
    wchar_t clsid[64]{};
    StringFromGUID2(kHandlerClsid, clsid, 64);
    wchar_t key[180]{};
    StringCchPrintfW(key, 180, L"Software\\Classes\\CLSID\\%s", clsid);
    RegDeleteTreeW(HKEY_CURRENT_USER, key);
    StringCchPrintfW(key, 180, L"SOFTWARE\\Classes\\CLSID\\%s", clsid);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, key);
    const wchar_t* extensions[] = {L".rvt", L".rfa", L".dwg", L".nwd", L".nwf", L".nwc", L".pdf"};
    for (const wchar_t* ext : extensions) {
        wchar_t handlerKey[160]{};
        StringCchPrintfW(handlerKey, 160, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\%s",
                         ext);
        wchar_t existing[64]{};
        DWORD size = sizeof(existing);
        if (RegGetValueW(HKEY_LOCAL_MACHINE, handlerKey, nullptr, RRF_RT_REG_SZ, nullptr, existing, &size) == ERROR_SUCCESS &&
            _wcsicmp(existing, clsid) == 0) {
            RegDeleteTreeW(HKEY_LOCAL_MACHINE, handlerKey);
        }
    }
    return S_OK;
}

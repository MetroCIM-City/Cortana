#include "rfi/Abi.h"
#include "rfi/Store.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstring>

namespace {

void CopyField(wchar_t* destination, size_t count, const std::wstring& value) {
    wcsncpy_s(destination, count, value.c_str(), _TRUNCATE);
}

void Fill(const rfi::FileInfo& info, RfiPayload* out) {
    ZeroMemory(out, sizeof(*out));
    out->schema = static_cast<uint32_t>(info.schema);
    out->setMask = info.mask;
    CopyField(out->discipline, 257, info.discipline);
    CopyField(out->location, 257, info.location);
    CopyField(out->originator, 257, info.originator);
    CopyField(out->subDiscipline, 257, info.subDiscipline);
    CopyField(out->documentType, 257, info.documentType);
    CopyField(out->program, 257, info.program);
    CopyField(out->subProgram, 257, info.subProgram);
    CopyField(out->modifiedUtc, 40, info.modifiedUtc);
    CopyField(out->source, 16, info.source);
}

rfi::FileInfo FromPayload(const RfiPayload* in) {
    rfi::FileInfo info;
    info.schema = static_cast<int>(in->schema);
    info.mask = in->setMask == 0 ? rfi::FieldAll : in->setMask;
    info.discipline = in->discipline;
    info.location = in->location;
    info.originator = in->originator;
    info.subDiscipline = in->subDiscipline;
    info.documentType = in->documentType;
    info.program = in->program;
    info.subProgram = in->subProgram;
    info.modifiedUtc = in->modifiedUtc;
    info.source = in->source;
    return info;
}

}  // namespace

extern "C" {

int32_t rfi_read(const wchar_t* path, RfiPayload* out) {
    if (!path || !out) return E_INVALIDARG;
    rfi::FileInfo info;
    bool found = false;
    const rfi::Status status = rfi::ReadFile(path, info, found);
    if (!rfi::Ok(status)) return status.hr;
    Fill(info, out);
    return found ? S_OK : S_FALSE;
}

int32_t rfi_write(const wchar_t* path, const RfiPayload* in) {
    if (!path || !in) return E_INVALIDARG;
    return rfi::WriteFile(path, FromPayload(in)).hr;
}

int32_t rfi_list_streams(const wchar_t* path, wchar_t* buffer, uint32_t* count) {
    if (!path || !count) return E_INVALIDARG;
    std::wstring report;
    const rfi::Status status = rfi::ListStreams(path, report);
    if (!rfi::Ok(status)) return status.hr;
    const uint32_t needed = static_cast<uint32_t>(report.size() + 1);
    if (!buffer || *count < needed) {
        *count = needed;
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }
    wcsncpy_s(buffer, *count, report.c_str(), _TRUNCATE);
    *count = needed;
    return S_OK;
}

int32_t rfi_last_error(wchar_t* buffer, uint32_t count) {
    const std::wstring message = rfi::LastError();
    if (!buffer || count == 0) return E_INVALIDARG;
    wcsncpy_s(buffer, count, message.c_str(), _TRUNCATE);
    return S_OK;
}

int32_t rfi_set_failpoint(int32_t phase) {
    rfi::SetFailPoint(phase);
    return S_OK;
}

}  // extern "C"

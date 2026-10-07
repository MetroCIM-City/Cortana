#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 4)
typedef struct RfiPayload {
    uint32_t schema;
    uint32_t setMask;
    wchar_t discipline[257];
    wchar_t location[257];
    wchar_t originator[257];
    wchar_t subDiscipline[257];
    wchar_t documentType[257];
    wchar_t program[257];
    wchar_t subProgram[257];
    wchar_t modifiedUtc[40];
    wchar_t source[16];
} RfiPayload;
#pragma pack(pop)

__declspec(dllexport) int32_t rfi_read(const wchar_t* path, RfiPayload* out);
__declspec(dllexport) int32_t rfi_write(const wchar_t* path, const RfiPayload* in);
__declspec(dllexport) int32_t rfi_list_streams(const wchar_t* path, wchar_t* buffer, uint32_t* count);
__declspec(dllexport) int32_t rfi_last_error(wchar_t* buffer, uint32_t count);
__declspec(dllexport) int32_t rfi_set_failpoint(int32_t phase);

#ifdef __cplusplus
}
#endif

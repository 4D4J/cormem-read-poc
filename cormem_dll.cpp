#include "cordrv.hpp"
#include "batched_read.hpp"

extern "C" {

__declspec(dllexport) void* CorDrv_Create() {
    return new CorDrv();
}

__declspec(dllexport) void CorDrv_Destroy(void* drv) {
    if (drv) {
        delete static_cast<CorDrv*>(drv);
    }
}

__declspec(dllexport) bool CorDrv_Initialize(void* drv) {
    if (!drv) return false;
    return static_cast<CorDrv*>(drv)->Initialize();
}

__declspec(dllexport) uint64_t CorDrv_FindProcessDTB(void* drv, DWORD pid) {
    if (!drv) return 0;
    return static_cast<CorDrv*>(drv)->FindProcessDTB(pid);
}

__declspec(dllexport) bool CorDrv_ReadProcessMemory(void* drv, uint64_t dtb, uint64_t virtualAddress, void* buffer, size_t size) {
    if (!drv) return false;
    return static_cast<CorDrv*>(drv)->ReadProcessMemory(dtb, virtualAddress, buffer, size);
}

__declspec(dllexport) bool CorDrv_WriteProcessMemory(void* drv, uint64_t dtb, uint64_t virtualAddress, const void* buffer, size_t size) {
    if (!drv) return false;
    return static_cast<CorDrv*>(drv)->WriteProcessMemory(dtb, virtualAddress, buffer, size);
}

__declspec(dllexport) void CorDrv_ReadProcessMemoryBatched(void* drv, uint64_t dtb, BatchedReadRequest* requests, size_t count) {
    if (!drv || !requests || count == 0) return;
    CorDrv* cdrv = static_cast<CorDrv*>(drv);
    for (size_t i = 0; i < count; ++i) {
        requests[i].Success = cdrv->ReadProcessMemory(dtb, requests[i].Address, requests[i].Buffer, requests[i].Size);
    }
}

}

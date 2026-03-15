#pragma once

#include <cstdint>

#pragma pack(push, 1)
struct BatchedReadRequest {
    uint64_t Address;
    uint64_t Size;
    void* Buffer;
    bool Success;    // Out
};
#pragma pack(pop)

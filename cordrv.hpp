#pragma once

#include <Windows.h>
#include <cstdint>

#define IOCTL_CORMEM_MAP_POOL                   0x222000
#define IOCTL_CORMEM_MAP_BUFFER                 0x22200C
#define IOCTL_CORMEM_UNMAP_BUFFER               0x222010
#define IOCTL_CORMEM_READ_IO                    0x222014
#define IOCTL_CORMEM_WRITE_IO                   0x222018
#define IOCTL_CORMEM_LINEAR_TO_PHYS             0x22201C
#define IOCTL_CORMEM_FREE_BUFFER                0x222020
#define IOCTL_CORMEM_LOCK_SG_BUFFER             0x222024
#define IOCTL_CORMEM_UNLOCK_SG_BUFFER           0x222028
#define IOCTL_CORMEM_UNLOCK_ALL_SG_BUFFER       0x22202C
#define IOCTL_CORMEM_ALLOC_BUFFER               0x222030
#define IOCTL_CORMEM_ALLOC_MSG                  0x222034
#define IOCTL_CORMEM_GET_MSG_BOUNDARY           0x222038
#define IOCTL_CORMEM_ALLOC_PHYS_MEMORY          0x22203C
#define IOCTL_CORMEM_FREE_PHYS_MEMORY           0x222040
#define IOCTL_CORMEM_MAP_PHYS_MEMORY            0x222044
#define IOCTL_CORMEM_UNMAP_PHYS_MEMORY          0x222048
#define IOCTL_CORMEM_GET_PHYS_MEMORY            0x22204C
#define IOCTL_CORMEM_GET_BUFFER_MEM_STATUS      0x222050
#define IOCTL_CORMEM_GET_MSG_MEM_STATUS         0x222054
#define IOCTL_CORMEM_CREATE_MDL_LOCK            0x222058
#define IOCTL_CORMEM_GET_POOL_BLOCK_COUNT       0x22205C
#define IOCTL_CORMEM_GET_PHYS_MEMORY_64         0x222060
#define IOCTL_CORMEM_ALLOC_BUFFER_64            0x222064
#define IOCTL_CORMEM_GET_BUFFER64_MEM_STATUS    0x222068

#define CORMEM_DEVICE_NAME "\\\\.\\CORMEM"
#define CORMEM_MAX_POOL_BLOCKS 0x101

#define PSB_SIGNATURE_OFFSET    0x000
#define PSB_KERNEL_ENTRY_OFFSET 0x070
#define PSB_PML4_OFFSET         0x0A0
#define PSB_SIGNATURE_MASK      0xffffffffffff00ffULL
#define PSB_SIGNATURE_VALUE     0x00000001000600E9ULL
#define KERNEL_VA_MASK          0xfffff80000000003ULL
#define KERNEL_VA_EXPECTED      0xfffff80000000000ULL
#define PML4_INVALID_BITS_MASK  0xffffff0000000fffULL

#define PAGE_PRESENT    0x1
#define PAGE_LARGE      0x80
#define PAGE_4KB        0x1000ULL
#define PAGE_2MB        0x200000ULL
#define PAGE_1GB        0x40000000ULL

namespace EProcess {
    constexpr uint64_t DirectoryTableBase = 0x028;
    constexpr uint64_t UniqueProcessId = 0x440;
    constexpr uint64_t ActiveProcessLinks = 0x448;
    constexpr uint64_t ImageFileName = 0x5A8;
}

// Offsets within kernel LDR_DATA_TABLE_ENTRY (x64, Windows 10/11)
namespace LdrEntry {
    constexpr uint64_t InLoadOrderFlink  = 0x000; // LIST_ENTRY.Flink
    constexpr uint64_t InLoadOrderBlink  = 0x008; // LIST_ENTRY.Blink
    constexpr uint64_t BaseDllNameLength = 0x058; // UNICODE_STRING.Length (USHORT)
    constexpr uint64_t BaseDllNameBuffer = 0x060; // UNICODE_STRING.Buffer (PWSTR)
}

#define SystemExtendedHandleInformation 0x40

struct SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX
{
    PVOID Object;
    ULONG UniqueProcessId;
    ULONG HandleValue;
    ULONG GrantedAccess;
    USHORT CreatorBackTraceIndex;
    USHORT ObjectTypeIndex;
    ULONG HandleAttributes;
    ULONG Reserved;
};
struct SYSTEM_HANDLE_INFORMATION_EX
{
    ULONG NumberOfHandles;
    ULONG Reserved;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
};

typedef NTSTATUS(NTAPI* PFN_NtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );

#pragma pack(push, 1)

struct CORMEM_ALLOC_BUFFER_IN {
    uint64_t Size;
    uint32_t Alignment;
    uint32_t Flags;
};
static_assert(sizeof(CORMEM_ALLOC_BUFFER_IN) == 0x10);

struct CORMEM_ALLOC_BUFFER_OUT {
    uint64_t PhysicalAddress;
    uint64_t Reserved;
};
static_assert(sizeof(CORMEM_ALLOC_BUFFER_OUT) == 0x10);

struct CORMEM_ALLOC_PHYS_IN {
    uint64_t Param0;
    uint64_t Param1;
    uint64_t Param2;
    uint64_t Param3;
};
static_assert(sizeof(CORMEM_ALLOC_PHYS_IN) == 0x20);

struct CORMEM_ALLOC_PHYS_OUT {
    uint64_t PhysicalAddress;
    uint64_t Param1;
};
static_assert(sizeof(CORMEM_ALLOC_PHYS_OUT) == 0x10);

struct CORMEM_MAP_BUFFER_IN {
    uint64_t Address;
    uint64_t Size;
    uint64_t Param2;
};
static_assert(sizeof(CORMEM_MAP_BUFFER_IN) == 0x18);

struct CORMEM_MAP_POOL_OUT {
    uint64_t UserAddress;
    uint64_t KernelAddress;
    uint64_t PhysicalAddress;
    uint32_t Size;
};
static_assert(sizeof(CORMEM_MAP_POOL_OUT) == 0x1C);

struct CORMEM_READ_IO_IN {
    uint32_t Width;
    uint64_t Address;
};
static_assert(sizeof(CORMEM_READ_IO_IN) == 0x0C);

struct CORMEM_WRITE_IO_IN {
    uint32_t Width;
    uint64_t Address;
    uint32_t Value;
};
static_assert(sizeof(CORMEM_WRITE_IO_IN) == 0x10);

struct CORMEM_UNMAP_PHYS_IN {
    uint64_t MappedAddress;
    uint64_t PhysAddress;
};
static_assert(sizeof(CORMEM_UNMAP_PHYS_IN) == 0x10);

struct CORMEM_UNMAP_BUFFER_IN {
    uint64_t MappedAddress;
    uint64_t Size;
};
static_assert(sizeof(CORMEM_UNMAP_BUFFER_IN) == 0x10);

#pragma pack(pop)

struct PoolBlock {
    uint64_t UserAddress;
    uint64_t KernelAddress;
    uint64_t PhysicalAddress;
    uint64_t Size;
};

class CorDrv {
public:
    CorDrv() = default;
    ~CorDrv();

    CorDrv(const CorDrv&) = delete;
    CorDrv& operator=(const CorDrv&) = delete;
    CorDrv(CorDrv&&) noexcept;
    CorDrv& operator=(CorDrv&&) noexcept;

    bool Initialize();
    void Close();
    bool IsValid() const { return m_Device != INVALID_HANDLE_VALUE; }

    uint64_t MapPhysicalMemory(uint64_t PhysicalAddress);
    bool UnmapPhysicalMemory(uint64_t MappedAddress, uint64_t PhysicalAddress);
    uint64_t LinearToPhys(uint64_t VirtualAddress);

    bool ReadIo(uint32_t Width, uint64_t Address, uint32_t* OutValue);
    bool WriteIo(uint32_t Width, uint64_t Address, uint32_t Value);

    bool AllocBuffer(uint64_t Size, uint32_t Alignment, uint32_t Flags, uint64_t* PhysAddress, uint64_t* UserAddress);
    bool FreeBuffer(uint64_t UserAddress);
    uint64_t MapBuffer(uint64_t Address, uint64_t Size, uint64_t Param);
    bool UnmapBuffer(uint64_t MappedAddress, uint64_t Size);
    bool AllocPhysMemory(uint64_t P0, uint64_t P1, uint64_t P2, uint64_t P3, uint64_t* OutPhys, uint64_t* OutParam);
    bool FreePhysMemory(uint64_t PhysAddress);
    bool GetPoolBlockCount(uint32_t* Count);

    uint64_t MapPhysToUser(uint64_t PhysAddress) const;
    uint64_t MapPhysToKernel(uint64_t PhysAddress) const;
    uint64_t MapUserToPhys(uint64_t UserAddress) const;
    uint64_t MapKernelToPhys(uint64_t KernelAddress) const;
    uint64_t MapKernelToUser(uint64_t KernelAddress) const;
    uint64_t MapUserToKernel(uint64_t UserAddress) const;

    bool ReadPhysicalMemory(uint64_t PhysicalAddress, void* Buffer, size_t Size);
    bool WritePhysicalMemory(uint64_t PhysicalAddress, const void* Buffer, size_t Size);

    template<typename T>
    T ReadPhys(uint64_t PhysicalAddress) {
        T value{};
        ReadPhysicalMemory(PhysicalAddress, &value, sizeof(T));
        return value;
    }
    template<typename T>
    void WritePhys(uint64_t PhysicalAddress, const T& Value) {
        WritePhysicalMemory(PhysicalAddress, &Value, sizeof(T));
    }

    uint64_t FindSystemDTB();
    uint64_t FindProcessDTB(DWORD Pid);
    uint64_t TranslateVirtualAddress(uint64_t DTB, uint64_t VirtualAddress);

    static uint64_t GetSystemEprocessVA();

    bool HideDriver(const wchar_t* DriverBaseName);
    bool RestoreDriver();

    bool ReadProcessMemory(uint64_t DTB, uint64_t VirtualAddress, void* Buffer, size_t Size);
    bool WriteProcessMemory(uint64_t DTB, uint64_t VirtualAddress, const void* Buffer, size_t Size);

    template<typename T>
    T ReadProcess(uint64_t DTB, uint64_t VirtualAddress) {
        T value{};
        ReadProcessMemory(DTB, VirtualAddress, &value, sizeof(T));
        return value;
    }
    template<typename T>
    void WriteProcess(uint64_t DTB, uint64_t VirtualAddress, const T& Value) {
        WriteProcessMemory(DTB, VirtualAddress, &Value, sizeof(T));
    }

    uint64_t GetSystemDTB() const { return m_SystemDTB; }

private:
    bool SendIoctl(DWORD IoControlCode, void* InBuffer, DWORD InSize,
        void* OutBuffer, DWORD OutSize, DWORD* BytesReturned = nullptr);
    bool MapPoolBlock(uint32_t Index);
    static bool TryFindDTBFromLowStub(uint8_t* LowStub1M, uint64_t& OutDTB, uint64_t& OutKernelEntry);
    bool ValidatePML4Page(uint64_t DTB, uint64_t MaxPhysAddr);
    static uint64_t GetNtoskrnlBase(char* OutName = nullptr, size_t NameSize = 0);
    static uint64_t ResolvePsLoadedModuleList(uint64_t NtBase, const char* NtName);

    HANDLE m_Device = INVALID_HANDLE_VALUE;
    uint32_t m_PoolBlockCount = 0;
    PoolBlock m_PoolBlocks[CORMEM_MAX_POOL_BLOCKS] = {};
    uint64_t m_SystemDTB = 0;
    // HideDriver state — used by RestoreDriver
    uint64_t m_HiddenEntryVA    = 0; // VA of CORMEM LDR_DATA_TABLE_ENTRY in kernel
    uint64_t m_HiddenEntryFlink = 0; // original Flink saved before unlinking
    uint64_t m_HiddenEntryBlink = 0; // original Blink saved before unlinking
};
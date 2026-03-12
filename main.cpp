#include "CorDrv.hpp"
#include <cstdio>
#include <cstdlib>

static void HexDump(const uint8_t* Data, size_t Size, uint64_t BaseAddr = 0) {
    for (size_t i = 0; i < Size; i++) {
        if (i % 16 == 0) printf("  %llX: ", (unsigned long long)(BaseAddr + i));
        printf("%02X ", Data[i]);
        if (i % 16 == 15) printf("\n");
    }
    if (Size % 16 != 0) printf("\n");
}

static void PrintUsage(const char* argv0) {
    printf("Usage:\n");
    printf("  %s <pid> <address> [size]\n\n", argv0);
    printf("  pid     : process ID to read from (decimal)\n");
    printf("  address : virtual address to read (hex, e.g. 0x7FF700000000)\n");
    printf("  size    : bytes to dump (optional, default=256)\n\n");
    printf("Example:\n");
    printf("  %s 1234 0x7FF76B400000 512\n", argv0);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        PrintUsage(argv[0]);
        return 1;
    }

    DWORD    targetPid = (DWORD)strtoul(argv[1], nullptr, 10);
    uint64_t targetVA  = strtoull(argv[2], nullptr, 16);
    size_t   dumpSize  = (argc >= 4) ? (size_t)strtoull(argv[3], nullptr, 10) : 256;

    if (targetPid == 0) { printf("[-] Invalid PID.\n"); return 1; }
    if (targetVA  == 0) { printf("[-] Invalid address.\n"); return 1; }
    if (dumpSize  == 0 || dumpSize > 0x100000) { printf("[-] Size must be 1..1048576.\n"); return 1; }

    printf("[*] Target PID     : %u\n",   targetPid);
    printf("[*] Target address : 0x%llX\n", (unsigned long long)targetVA);
    printf("[*] Dump size      : %zu bytes\n\n", dumpSize);

    // --- Init driver ---
    CorDrv drv;
    printf("[*] Initializing CorDrv...\n");
    if (!drv.Initialize()) { printf("[-] Failed. Is CORMEM.SYS loaded?\n"); return 1; }
    printf("[+] Driver initialized.\n\n");

    // --- Find system DTB ---
    printf("[*] Finding system DTB...\n");
    uint64_t sysDTB = drv.FindSystemDTB();
    if (!sysDTB) { printf("[-] Failed to find system DTB.\n"); return 1; }
    printf("[+] System DTB: 0x%llX\n\n", (unsigned long long)sysDTB);

    // --- Find target process DTB ---
    printf("[*] Searching EPROCESS list for PID %u...\n", targetPid);
    uint64_t procDTB = drv.FindProcessDTB(targetPid);
    if (!procDTB) { printf("[-] Failed to find DTB for PID %u.\n", targetPid); return 1; }
    printf("[+] Process DTB: 0x%llX\n\n", (unsigned long long)procDTB);

    // --- Translate VA ---
    uint64_t phys = drv.TranslateVirtualAddress(procDTB, targetVA);
    if (!phys) { printf("[-] Page table walk failed for 0x%llX.\n", (unsigned long long)targetVA); return 1; }
    printf("[+] VA 0x%llX -> PA 0x%llX\n\n", (unsigned long long)targetVA, (unsigned long long)phys);

    // --- Read & dump ---
    uint8_t* buf = new uint8_t[dumpSize]();
    if (!drv.ReadProcessMemory(procDTB, targetVA, buf, dumpSize)) {
        printf("[-] ReadProcessMemory failed.\n");
        delete[] buf;
        return 1;
    }

    printf("Memory dump (0x%llX, %zu bytes):\n", (unsigned long long)targetVA, dumpSize);
    HexDump(buf, dumpSize, targetVA);

    delete[] buf;
    printf("\n[+] Done.\n");
    return 0;
}
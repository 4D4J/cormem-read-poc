#include "CorDrv.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <TlHelp32.h>

//  Process lookup 

static bool FindProcessByName(const char* ProcName, DWORD* OutPid, uint64_t* OutBase) {
    *OutPid = 0;
    *OutBase = 0;

    // Convert name to wide + add .exe if omitted
    wchar_t wname[MAX_PATH] = {};
    MultiByteToWideChar(CP_ACP, 0, ProcName, -1, wname, MAX_PATH);
    if (!wcschr(wname, L'.'))
        wcsncat_s(wname, L".exe", _TRUNCATE);

    // Enumerate processes
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, wname) == 0) {
                *OutPid = pe.th32ProcessID;
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (!found) return false;

    // Get base address (first module = main exe)
    HANDLE msnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, *OutPid);
    if (msnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me = {};
        me.dwSize = sizeof(me);
        if (Module32FirstW(msnap, &me))
            *OutBase = reinterpret_cast<uint64_t>(me.modBaseAddr);
        CloseHandle(msnap);
    }
    return true;
}

//  Hex dump 
static void HexDump(const uint8_t* Data, size_t Size, uint64_t BaseAddr = 0) {
    for (size_t i = 0; i < Size; i++) {
        if (i % 16 == 0) printf("  %llX: ", (unsigned long long)(BaseAddr + i));
        printf("%02X ", Data[i]);
        if (i % 16 == 15) printf("\n");
    }
    if (Size % 16 != 0) printf("\n");
}

//  Helpers 

static void Pause() { printf("\nAppuyez sur une touche pour quitter...\n"); getchar(); }

static void PrintUsage(const char* argv0) {
    printf("Usage:\n");
    printf("  %s <process.exe> [size] [--hide]       auto-mode\n", argv0);
    printf("  %s <pid> <address> [size] [--hide]     manual mode\n\n", argv0);
    printf("  process.exe  : name of the target process\n");
    printf("  pid          : process ID (decimal)\n");
    printf("  address      : virtual address (hex, e.g. 0x7FF700000000)\n");
    printf("  size         : bytes to dump (default 256, max 1048576)\n");
    printf("  Run with no arguments for interactive mode.\n");
}

//  Entry point 

int main(int argc, char* argv[]) {
    DWORD    targetPid  = 0;
    uint64_t targetVA   = 0;
    size_t   dumpSize   = 256;
    bool     doHide     = false;
    char     procName[MAX_PATH] = {};

    //  Parse arguments 
    if (argc == 1) {
        // Interactive mode
        printf("=== cormem-read interactive ===\n\n");
        printf("Process name: ");
        fgets(procName, sizeof(procName), stdin);
        procName[strcspn(procName, "\n")] = 0;

        char sizeStr[32] = {};
        printf("Bytes to dump [256]: ");
        fgets(sizeStr, sizeof(sizeStr), stdin);
        if (sizeStr[0] != '\n' && sizeStr[0] != '\0')
            dumpSize = (size_t)strtoull(sizeStr, nullptr, 10);
        printf("\n");
    }
    else {
        // Scan args
        for (int i = 1; i < argc; i++) {
            if (strncmp(argv[i], "0x", 2) == 0 || strncmp(argv[i], "0X", 2) == 0) {
                // Hex address → manual mode
                targetVA = strtoull(argv[i], nullptr, 16);
            }
            else if (isalpha((unsigned char)argv[i][0]) || strchr(argv[i], '.')) {
                // Process name 
                strncpy_s(procName, argv[i], _TRUNCATE);
            }
            else {
                // Numeric → PID or size
                uint64_t val = strtoull(argv[i], nullptr, 10);
                if (targetPid == 0 && val > 0 && val < 65536)
                    targetPid = (DWORD)val;
                else
                    dumpSize = (size_t)val;
            }
        }

        if (procName[0] == 0 && targetPid == 0 && targetVA == 0) {
            PrintUsage(argv[0]);
            Pause();
            return 1;
        }
    }

    if (dumpSize == 0 || dumpSize > 0x100000) dumpSize = 256;

    //  Resolve process name if needed 
    if (procName[0] != 0) {
        printf("[*] Looking for process: %s\n", procName);
        if (!FindProcessByName(procName, &targetPid, &targetVA)) {
            printf("[-] Process not found. Is it running?\n");
            Pause(); return 1;
        }
        printf("[+] PID   : %u\n", targetPid);
        printf("[+] Base  : 0x%llX\n\n", (unsigned long long)targetVA);
    } // targetVA is populated by FindProcessByName if procName is used


    if (targetPid == 0) { printf("[-] Invalid PID.\n"); Pause(); return 1; }
    if (targetVA  == 0) { 
        printf("[-] Warning: Base address is 0x0. Make sure you are running as Admin.\n"); 
    }

    printf("[*] Target PID     : %u\n",   targetPid);
    printf("[*] Target address : 0x%llX\n", (unsigned long long)targetVA);
    printf("[*] Dump size      : %zu bytes\n\n", dumpSize);

    //  Init driver 
    CorDrv drv;
    printf("[*] Initializing CorDrv...\n");
    if (!drv.Initialize()) {
        printf("[-] Failed. Is CORMEM.SYS loaded?\n");
        Pause(); return 1;
    }
    printf("[+] Driver initialized.\n\n");

    //  Find system DTB 
    printf("[*] Finding system DTB...\n");
    uint64_t sysDTB = drv.FindSystemDTB();
    if (!sysDTB) { printf("[-] Failed to find system DTB.\n"); Pause(); return 1; }
    printf("[+] System DTB: 0x%llX\n\n", (unsigned long long)sysDTB);

    //  DKOM hide (DISABLED - Causes PatchGuard BSOD)
    if (doHide) {
        printf("[*] Hiding CORMEM from PsLoadedModuleList is DISABLED (prevents KPP BSOD).\n");
        printf("[+] Proceeding without hiding.\n\n");
    }

    //  Find process DTB 
    printf("[*] Searching EPROCESS list for PID %u...\n", targetPid);
    uint64_t procDTB = drv.FindProcessDTB(targetPid);
    if (!procDTB) { printf("[-] Failed to find DTB for PID %u.\n", targetPid); Pause(); return 1; }
    printf("[+] Process DTB: 0x%llX\n\n", (unsigned long long)procDTB);

    //  Translate VA 
    uint64_t phys = drv.TranslateVirtualAddress(procDTB, targetVA);
    if (!phys) { printf("[-] Page table walk failed for 0x%llX.\n", (unsigned long long)targetVA); Pause(); return 1; }
    printf("[+] VA 0x%llX -> PA 0x%llX\n\n", (unsigned long long)targetVA, (unsigned long long)phys);

    //  Read & dump 
    uint8_t* buf = new uint8_t[dumpSize]();
    if (!drv.ReadProcessMemory(procDTB, targetVA, buf, dumpSize)) {
        printf("[-] ReadProcessMemory failed.\n");
        delete[] buf; Pause(); return 1;
    }

    printf("Memory dump (0x%llX, %zu bytes):\n", (unsigned long long)targetVA, dumpSize);
    HexDump(buf, dumpSize, targetVA);

    delete[] buf;
    printf("\n[+] Done.\n");
    Pause();
    return 0;
}
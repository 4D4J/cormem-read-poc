#include "dayz_reader.hpp"
#include <cstdio>
#include <iostream>
#include <vector>

#define OFF_Network_Manager     0xF5E190
#define OFF_Network_Client      0x50
#define OFF_Network_Table       0x18
#define OFF_Network_Table_Size  0x1C
#define OFF_Network_Table_ID    0x30
#define OFF_Network_ID          0x6E4
#define OFF_PlayerName          0xF8
#define OFF_playerIsDead        0xE2

template <typename T>
T ReadMem(CorDrv& drv, uint64_t procDTB, uint64_t address) {
    T buffer{};
    if (address == 0) return buffer;
    drv.ReadProcessMemory(procDTB, address, &buffer, sizeof(T));
    return buffer;
}

std::string ReadString(CorDrv& drv, uint64_t procDTB, uint64_t address, size_t maxLength = 64) {
    if (address == 0) return "";
    std::string str(maxLength, '\0');
    if (drv.ReadProcessMemory(procDTB, address, &str[0], maxLength)) {
        str.resize(str.find('\0')); // Couper au premier null byte
        return str;
    }
    return "";
}

void RunDayZReader(CorDrv& drv, uint64_t procDTB, uint64_t baseAddress) {
    printf("[+] Starting DayZ Data Reader (Network Only)...\n");

    uint64_t networkManagerPtr = ReadMem<uint64_t>(drv, procDTB, baseAddress + OFF_Network_Manager);
    if (!networkManagerPtr) {
        printf("[-] NetworkManager not found.\n");
        return;
    }
    printf("    -> NetworkManager: 0x%llX\n", (unsigned long long)networkManagerPtr);

    uint64_t networkClient = ReadMem<uint64_t>(drv, procDTB, networkManagerPtr + OFF_Network_Client);
    if (!networkClient) {
        printf("[-] NetworkClient not found.\n");
        return;
    }
    printf("    -> NetworkClient: 0x%llX\n", (unsigned long long)networkClient);

    uint64_t networkTable = ReadMem<uint64_t>(drv, procDTB, networkClient + OFF_Network_Table);
    uint32_t networkTableSize = ReadMem<uint32_t>(drv, procDTB, networkClient + OFF_Network_Table_Size);
    
    printf("    -> NetworkTable: 0x%llX (Size: %u)\n", (unsigned long long)networkTable, networkTableSize);

    if (networkTable && networkTableSize > 0 && networkTableSize < 2000) { // Check de sécurité sur la taille
        printf("\n[+] Reading Network Table Entries...\n");

        // Exemple minimal d'optimisation par buffer reading de la table entière.
        // La NetworkTable contient les pointeurs (8 bytes) des objets réseau.
        size_t tableByteSize = networkTableSize * sizeof(uint64_t);
        std::vector<uint64_t> tablePointers(networkTableSize);

        if (drv.ReadProcessMemory(procDTB, networkTable, tablePointers.data(), tableByteSize)) {
            for (uint32_t i = 0; i < networkTableSize; i++) {
                uint64_t entityPtr = tablePointers[i];
                if (!entityPtr) continue;

                uint64_t networkID = ReadMem<uint64_t>(drv, procDTB, entityPtr + OFF_Network_ID);
                uint64_t playerNamePtr = ReadMem<uint64_t>(drv, procDTB, entityPtr + OFF_PlayerName); // Souvent un ptr vers une string
                uint8_t isDead = ReadMem<uint8_t>(drv, procDTB, entityPtr + OFF_playerIsDead);

                if (networkID != 0) {
                    std::string playerName = "Unknown";
                    if (playerNamePtr) {
                        playerName = ReadString(drv, procDTB, playerNamePtr);
                    }
                    
                    printf("        [Entity %u] Ptr: 0x%llX | NetID: %llu | Dead: %s | Name: %s\n",
                           i, (unsigned long long)entityPtr, (unsigned long long)networkID,
                           (isDead ? "Yes" : "No"), playerName.c_str());
                }
            }
        } else {
            printf("[-] Failed to read the full NetworkTable buffer.\n");
        }
    }

    printf("[+] DayZ Data Read Complete.\n");
}

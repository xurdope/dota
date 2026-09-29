#include "offset_scanner.h"
#include "skin_changer.h"
#include "pattern_scan.h"
#include "../system/system_utils.h"
#include "../data/item_schema.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <algorithm>

OffsetScanner& OffsetScanner::GetInstance() {
    static OffsetScanner inst;
    return inst;
}

void OffsetScanner::Initialize() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_offsets = ResolvedOffsets{};
    m_logs.clear();
    Log("[OffsetScanner] Initialized. Ready for dynamic runtime scanning.");
}

void OffsetScanner::Log(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    m_logs.push_back(buf);
    if (m_logs.size() > 100) m_logs.erase(m_logs.begin());
    SystemUtils::Log("%s", buf);
}

std::vector<std::string> OffsetScanner::GetScanLogs() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_logs;
}

ResolvedOffsets OffsetScanner::GetOffsets() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_offsets;
}

void OffsetScanner::SetManualOffsetOverride(const std::string& name, uintptr_t val) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (name == "m_hMyWearables")          m_offsets.m_hMyWearables = val;
    else if (name == "m_iItemDefinitionIndex") m_offsets.m_iItemDefinitionIndex = val;
    else if (name == "m_nHeroID")          m_offsets.m_nHeroID = val;
    else if (name == "m_nStyle")           m_offsets.m_nStyle = val;
    else if (name == "m_flFallbackWear")   m_offsets.m_flFallbackWear = val;
    Log("[OffsetScanner] Manual override set for %s = 0x%X", name.c_str(), (uint32_t)val);
}

bool OffsetScanner::ProbeWearableEntity(uintptr_t wearEntityPtr) {
    if (!SafeMemoryOps::IsValid(wearEntityPtr)) return false;

    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_offsets.defIndexResolved) return true;

    // Scan 4-byte aligned offsets in [0x1A0..0x220] for valid defIndex
    for (uintptr_t off = 0x1A0; off <= 0x220; off += 4) {
        uint32_t candidateDef = 0;
        if (SafeMemoryOps::Read32(wearEntityPtr + off, candidateDef) && candidateDef > 0 && candidateDef < 100000u) {
            int slot = GetItemSlotFromDB(candidateDef);
            if (slot >= 0 || candidateDef == 4001 || candidateDef == 125 || candidateDef == 4007 || candidateDef == 7061) {
                m_offsets.m_iItemDefinitionIndex = off;
                m_offsets.m_nStyle               = off + 4;
                m_offsets.m_flFallbackWear       = off + 12;
                m_offsets.defIndexResolved       = true;
                Log("[AutoResolver] SUCCESS: Resolved m_iItemDefinitionIndex = 0x%X (found defIndex #%u)", (uint32_t)off, candidateDef);
                return true;
            }
        }
    }
    return false;
}

bool OffsetScanner::ProbeHeroEntity(uintptr_t heroEntityPtr, uintptr_t chunkBase, int stride, int ptrOff) {
    if (!SafeMemoryOps::IsValid(heroEntityPtr) || !SafeMemoryOps::IsValid(chunkBase)) return false;

    std::lock_guard<std::mutex> lk(m_mutex);

    // 1. Probe m_nHeroID if not resolved
    if (!m_offsets.heroIDResolved) {
        for (uintptr_t hOff = 0x580; hOff <= 0x600; hOff += 4) {
            uint32_t candidateID = 0;
            if (SafeMemoryOps::Read32(heroEntityPtr + hOff, candidateID) && candidateID > 0 && candidateID < 250) {
                m_offsets.m_nHeroID = hOff;
                m_offsets.heroIDResolved = true;
                Log("[AutoResolver] SUCCESS: Resolved m_nHeroID = 0x%X (found HeroID #%u)", (uint32_t)hOff, candidateID);
                break;
            }
        }
    }

    // 2. Probe m_hMyWearables CUtlVector
    if (!m_offsets.hMyWearablesResolved) {
        for (uintptr_t off = 0xDF0; off <= 0xF80; off += 8) {
            uintptr_t wearData = 0;
            uint32_t  wearCount = 0;

            if (SafeMemoryOps::Read64(heroEntityPtr + off, wearData) &&
                SafeMemoryOps::Read32(heroEntityPtr + off + 8, wearCount) &&
                wearCount >= 1 && wearCount <= 32 && SafeMemoryOps::IsValid(wearData))
            {
                uint32_t handleVal = 0xFFFFFFFF;
                if (SafeMemoryOps::Read32(wearData, handleVal) && handleVal != 0xFFFFFFFF) {
                    const int entIdx = (int)(handleVal & 0x7FFF);
                    const int chunk  = entIdx / 512;
                    const int slotInChunk = entIdx % 512;

                    uintptr_t chunkPtr = 0;
                    if (SafeMemoryOps::Read64(chunkBase + (uintptr_t)chunk * 8, chunkPtr) && SafeMemoryOps::IsValid(chunkPtr)) {
                        uintptr_t identAddr = chunkPtr + (uintptr_t)slotInChunk * stride;
                        uintptr_t wearPtr = 0;
                        if (SafeMemoryOps::IsValid(identAddr) && SafeMemoryOps::Read64(identAddr + ptrOff, wearPtr) && SafeMemoryOps::IsValid(wearPtr)) {
                            m_offsets.m_hMyWearables       = off;
                            m_offsets.hMyWearablesResolved = true;
                            Log("[AutoResolver] SUCCESS: Resolved m_hMyWearables = 0x%X (found %u wearables)", (uint32_t)off, wearCount);
                            break;
                        }
                    }
                }
            }
        }
    }

    m_offsets.isFullyResolved = m_offsets.hMyWearablesResolved && m_offsets.defIndexResolved && m_offsets.heroIDResolved;
    if (m_offsets.isFullyResolved) {
        m_offsets.statusText = "Fully Auto-Resolved & Active";
    } else {
        char statusBuf[128];
        snprintf(statusBuf, sizeof(statusBuf), "Scanning... [Wearables:%s | DefIndex:%s | HeroID:%s]",
                 m_offsets.hMyWearablesResolved ? "OK" : "Auto",
                 m_offsets.defIndexResolved ? "OK" : "Auto",
                 m_offsets.heroIDResolved ? "OK" : "Auto");
        m_offsets.statusText = statusBuf;
    }

    return m_offsets.isFullyResolved;
}

bool OffsetScanner::RunAutoScan(uintptr_t pEntitySystem) {
    Log("[OffsetScanner] Starting full memory & pattern auto-scan...");

    // 1. m_hMyWearables pattern scan
    static const char* wearPats[] = {
        "48 8B 8B ?? ?? 00 00 48 8B 01",
        "48 8B 93 ?? ?? 00 00 48 8B 02",
        "48 8B 83 ?? ?? 00 00 48 85 C0 74 ?? 48 8B"
    };
    for (auto pat : wearPats) {
        uintptr_t patAddr = PatternScan::Find("client.dll", pat);
        if (patAddr) {
            uint32_t relOff = 0;
            if (SafeMemoryOps::Read32(patAddr + 3, relOff) && relOff >= 0x200 && relOff <= 0x2000) {
                std::lock_guard<std::mutex> lk(m_mutex);
                m_offsets.m_hMyWearables = relOff;
                m_offsets.hMyWearablesResolved = true;
                Log("[OffsetScanner] Pattern found m_hMyWearables = 0x%X via client.dll signature", relOff);
                break;
            }
        }
    }

    // 2. m_iItemDefinitionIndex pattern scan
    static const char* defPats[] = {
        "0F B7 83 ?? ?? 00 00 66 85 C0",
        "0F B7 8B ?? ?? 00 00 85 C9",
        "8B 8B ?? ?? 00 00 85 C9 74 ?? 48 8B",
        "0F B7 80 ?? ?? 00 00 85 C0"
    };
    for (auto pat : defPats) {
        uintptr_t patAddr = PatternScan::Find("client.dll", pat);
        if (patAddr) {
            uint32_t relOff = 0;
            if (SafeMemoryOps::Read32(patAddr + 3, relOff) && relOff >= 0x100 && relOff <= 0x400) {
                std::lock_guard<std::mutex> lk(m_mutex);
                m_offsets.m_iItemDefinitionIndex = relOff;
                m_offsets.m_nStyle               = relOff + 4;
                m_offsets.m_flFallbackWear       = relOff + 12;
                m_offsets.defIndexResolved       = true;
                Log("[OffsetScanner] Pattern found m_iItemDefinitionIndex = 0x%X via client.dll signature", relOff);
                break;
            }
        }
    }

    // 3. m_nHeroID pattern scan
    static const char* heroPats[] = {
        "0F B7 83 ?? ?? 00 00 3D ?? 00 00 00",
        "8B 8B ?? ?? 00 00 85 C9 74 ?? 48 8B 01",
        "0F B7 8B ?? ?? 00 00 48 8D"
    };
    for (auto pat : heroPats) {
        uintptr_t patAddr = PatternScan::Find("client.dll", pat);
        if (patAddr) {
            uint32_t relOff = 0;
            if (SafeMemoryOps::Read32(patAddr + 3, relOff) && relOff >= 0x400 && relOff <= 0x800) {
                std::lock_guard<std::mutex> lk(m_mutex);
                m_offsets.m_nHeroID = relOff;
                m_offsets.heroIDResolved = true;
                Log("[OffsetScanner] Pattern found m_nHeroID = 0x%X via client.dll signature", relOff);
                break;
            }
        }
    }

    Log("[OffsetScanner] Auto-scan pass complete.");
    return true;
}

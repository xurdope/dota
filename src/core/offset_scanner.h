#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>

// ============================================================
//  Dynamic Offset & Pattern Auto-Resolver for Dota 2 Source 2
// ============================================================

struct ResolvedOffsets {
    uintptr_t dwEntitySystem          = 0;
    uintptr_t m_hMyWearables          = 0xEF8;
    uintptr_t m_iItemDefinitionIndex  = 0x1B8;
    uintptr_t m_nStyle                = 0x1BC;
    uintptr_t m_flFallbackWear        = 0x1C4;
    uintptr_t m_nHeroID               = 0x5AC;
    uintptr_t m_hOwnerEntity          = 0x3C8;
    uintptr_t m_pGameSceneNode        = 0x118;

    bool      hMyWearablesResolved    = false;
    bool      defIndexResolved        = false;
    bool      heroIDResolved          = false;
    bool      isFullyResolved         = false;
    std::string statusText            = "Pending scan...";
};

class OffsetScanner {
public:
    static OffsetScanner& GetInstance();

    void Initialize();
    bool RunAutoScan(uintptr_t pEntitySystem);

    ResolvedOffsets GetOffsets() const;
    void SetManualOffsetOverride(const std::string& name, uintptr_t val);

    // Dynamic prober called during entity scan
    bool ProbeHeroEntity(uintptr_t heroEntityPtr, uintptr_t chunkBase, int stride, int ptrOff);
    bool ProbeWearableEntity(uintptr_t wearEntityPtr);

    std::vector<std::string> GetScanLogs() const;
    void Log(const char* fmt, ...);

private:
    OffsetScanner()  = default;
    ~OffsetScanner() = default;

    mutable std::mutex       m_mutex;
    ResolvedOffsets          m_offsets;
    std::vector<std::string> m_logs;
};

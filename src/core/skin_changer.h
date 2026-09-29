#pragma once
#include <windows.h>
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>
#include <deque>
#include "imgui.h"

// ============================================================
//  SEH Safe Memory Operations
// ============================================================
namespace SafeMemoryOps {
    template<typename T>
    inline bool Read(uintptr_t addr, T& outVal) {
        if (!addr || addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return false;
        __try {
            outVal = *reinterpret_cast<const T*>(addr);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    template<typename T>
    inline bool Write(uintptr_t addr, const T& val) {
        if (!addr || addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return false;
        __try {
            *reinterpret_cast<T*>(addr) = val;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    inline bool Read32(uintptr_t addr, uint32_t& outVal)  { return Read<uint32_t>(addr, outVal); }
    inline bool Read32s(uintptr_t addr, int32_t& outVal)  { return Read<int32_t>(addr, outVal); }
    inline bool Read64(uintptr_t addr, uintptr_t& outVal) { return Read<uintptr_t>(addr, outVal); }
    inline bool ReadFloat(uintptr_t addr, float& outVal)  { return Read<float>(addr, outVal); }
    inline bool ReadBool(uintptr_t addr, bool& outVal)   { return Read<bool>(addr, outVal); }

    inline bool Write32(uintptr_t addr, uint32_t val)    { return Write<uint32_t>(addr, val); }
    inline bool WriteFloat(uintptr_t addr, float val)    { return Write<float>(addr, val); }
    inline bool WriteBool(uintptr_t addr, bool val)      { return Write<bool>(addr, val); }

    inline bool IsValid(uintptr_t addr) {
        if (!addr || addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return false;
        __try {
            volatile uint8_t dummy = *reinterpret_cast<const uint8_t*>(addr);
            (void)dummy;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }
}

// ─── Debug / Diagnostics ─────────────────────────────────────
struct SkinDebugStats {
    uintptr_t entitySystemAddr    = 0;
    bool      isEntitySystemReady = false;
    uint32_t  totalEntitiesScanned= 0;
    uint32_t  econItemsFound      = 0;   // heroes found
    uint32_t  itemsPatched        = 0;
    uint32_t  activeOverridesCount= 0;
    uint32_t  safeExecuteSuccesses= 0;
    uint32_t  safeExecuteFailures = 0;
    uint32_t  lastOriginalDefIndex= 0;
    uint32_t  lastTargetDefIndex  = 0;
    uintptr_t lastPatchedAddr     = 0;
    bool      lastWriteStatus     = false;
    uint32_t  tickCount           = 0;
};

struct SkinDebugLogEntry {
    std::string timestamp;
    std::string message;
    ImVec4      color;
};

// ─── Inventory Data ───────────────────────────────────────────
static constexpr int INV_MAX_SLOTS = 10; // max wearable slots per hero

// Override for a single wearable slot
struct SlotOverride {
    bool     enabled  = false;
    uint32_t defIndex = 0;      // target cosmetic defIndex (0 = disabled)
    uint32_t style    = 0;      // item style variant (0 = default)
    float    wear     = 0.001f; // item wear float (0.001f - 1.0f)
};

// Full per-hero inventory override config
struct HeroInventory {
    bool         enabled = false;
    SlotOverride slots[INV_MAX_SLOTS];
};

// Direct item-definition override rule
struct ItemOverrideRule {
    uint32_t originalDefIndex = 0;
    uint32_t overrideDefIndex = 0;
    uint32_t style            = 0;
    float    wear             = 0.001f;
    bool     enabled          = true;
};

// Detected wearable (for UI display)
struct DetectedWearable {
    uintptr_t addr;
    uint32_t  heroID;
    int       slotIndex;
    uint32_t  originalDefIndex;
    uint32_t  patchedDefIndex; // 0 = not patched
};

// ─── InventoryChanger Singleton ───────────────────────────────
class SkinChanger {
public:
    static SkinChanger& GetInstance();

    void Initialize();
    void Tick();

    // ── Master toggle ────────────────────────────────────────
    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    // ── Override Rule Registration & Direct Binding ──────────
    void AddItemOverride(uint32_t heroID, int slot, uint32_t defIndex, uint32_t style = 0, float wear = 0.001f, bool enabled = true);
    void AddItemOverride(uint32_t originalDefIndex, uint32_t overrideDefIndex, uint32_t style = 0, float wear = 0.001f);
    void RemoveItemOverride(uint32_t heroID, int slot);
    void RemoveItemOverride(uint32_t originalDefIndex);
    bool FindItemOverride(uint32_t origDefIndex, ItemOverrideRule& outRule) const;

    int  GetActiveOverridesCount() const;
    int  GetPatchCount()       const;
    bool IsEntitySystemReady() const;

    // ── Per-hero inventory configuration ─────────────────────
    HeroInventory GetHeroInventoryCopy(uint32_t heroID) const;
    bool          HeroHasOverrides(uint32_t heroID) const;

    void SetHeroEnabled(uint32_t heroID, bool enabled);
    void SetSlotOverride(uint32_t heroID, int slot,
                         uint32_t defIndex, uint32_t style = 0, float wear = 0.001f, bool enabled = true);
    void ClearSlotOverride(uint32_t heroID, int slot);
    void ClearHeroOverrides(uint32_t heroID);
    void ClearAllOverrides();

    // ── UI helpers ────────────────────────────────────────────
    void ForceRescan();
    std::vector<DetectedWearable> GetDetectedWearables() const;

    // ── Debug logging ─────────────────────────────────────────
    void LogDebug(const char* level, const char* fmt, ...);
    void ClearDebugLogs();
    std::vector<SkinDebugLogEntry> GetDebugLogs() const;
    SkinDebugStats GetDebugStats()                 const;

private:
    SkinChanger()  = default;
    ~SkinChanger() = default;
    SkinChanger(const SkinChanger&) = delete;
    SkinChanger& operator=(const SkinChanger&) = delete;

    struct ESLayout {
        uintptr_t listOffset = 0x10;
        bool      isIndirect = false;
        uintptr_t stride     = 0x70;
        uintptr_t ptrOff     = 0x00;
        bool      resolved   = false;
    };

    bool      TryResolveEntitySystem();
    bool      ProbeLayout(uintptr_t es, ESLayout& outLayout);
    void      ApplyPatches();
    uintptr_t ResolveHandle(uintptr_t chunkBase, uint32_t handleVal);
    bool      SafeWrite32(uintptr_t addr, uint32_t val);
    bool      SafeWriteFloat(uintptr_t addr, float val);
    bool      SafeWriteBool(uintptr_t addr, bool val);

    bool      m_enabled       = false;
    bool      m_needsRescan   = false;
    uintptr_t m_pEntitySystem = 0;
    ESLayout  m_esLayout;
    int       m_initAttempts  = 0;
    int       m_patchCount    = 0;

    mutable std::recursive_mutex m_mutex;
    std::unordered_map<uint32_t, HeroInventory>    m_heroInventories;
    std::unordered_map<uint32_t, ItemOverrideRule> m_itemOverrides; // origDefIndex -> rule
    std::vector<DetectedWearable>                  m_detectedWearables;

    mutable std::mutex  m_debugMutex;
    SkinDebugStats      m_debugStats;
    std::deque<SkinDebugLogEntry> m_debugLogs;
};
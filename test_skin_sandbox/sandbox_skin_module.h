// ============================================================================
//  File: test_skin_sandbox/sandbox_skin_module.h
//  Description: Standalone SkinChanger module header for testing item overrides,
//               definition swapping, and SEH safe memory writing mechanisms.
// ============================================================================

#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Test structure representing an item entity in memory (simulating C_EconItemView)
struct TestEconItem {
    uint64_t itemID          = 0;
    uint32_t originalDefIndex = 0;
    uint32_t currentDefIndex  = 0;
    uint32_t style            = 0;
    float    wear             = 0.001f;
    bool     isEquipped       = true;
    char     slotName[32]     = { 0 };
    char     heroName[32]     = { 0 };
};

// Item override rule entry
struct SkinOverrideRule {
    uint32_t targetDefIndex = 0;
    uint32_t targetStyle    = 0;
    float    wearOverride   = -1.0f; // -1.0f means keep original wear
    bool     active         = true;
};

// Log levels for detailed console output
enum class LogLevel {
    Info,
    Success,
    Warning,
    Error,
    SEHGuard
};

// Low-level SEH Memory Protection Wrappers (__try / __except)
namespace SafeMemoryOps {
    bool SafeRead64(const uint64_t* pAddress, uint64_t* pOutValue, DWORD* pOutExceptionCode = nullptr);
    bool SafeRead32(const uint32_t* pAddress, uint32_t* pOutValue, DWORD* pOutExceptionCode = nullptr);
    bool SafeWrite32(uint32_t* pAddress, uint32_t value, DWORD* pOutExceptionCode = nullptr);
    bool SafeReadFloat(const float* pAddress, float* pOutValue, DWORD* pOutExceptionCode = nullptr);
    bool SafeWriteFloat(float* pAddress, float value, DWORD* pOutExceptionCode = nullptr);
}

// Standalone Sandbox SkinChanger Module Class
class SandboxSkinModule {
private:
    std::unordered_map<uint32_t, SkinOverrideRule> m_itemOverrides;
    std::unordered_map<std::string, SkinOverrideRule> m_slotOverrides;
    bool m_sehProtectionEnabled = true;

    void LogStep(LogLevel level, const char* fmt, ...) const;

public:
    SandboxSkinModule() = default;
    ~SandboxSkinModule() = default;

    void SetSEHProtection(bool enabled) { m_sehProtectionEnabled = enabled; }

    // Configuration methods
    void AddItemOverride(uint32_t origDef, uint32_t newDef, uint32_t style = 0, float wear = -1.0f);
    void AddHeroSlotOverride(const std::string& heroName, const std::string& slotName, uint32_t newDef, uint32_t style = 0);
    void ClearOverrides();

    // Query methods
    bool FindOverride(uint32_t origDef, SkinOverrideRule& outRule) const;
    bool FindSlotOverride(const std::string& heroName, const std::string& slotName, SkinOverrideRule& outRule) const;

    // Process item modification safely using SEH
    bool ProcessItemModification(TestEconItem* pItem);

    // Run standalone unit simulation suite
    static void RunSimulationSuite();
};

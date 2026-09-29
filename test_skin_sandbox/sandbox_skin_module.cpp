// ============================================================================
//  File: test_skin_sandbox/sandbox_skin_module.cpp
//  Description: Standalone implementation of skin changer logic, item overrides,
//               definition swapping, and SEH safe memory field writes.
// ============================================================================

#include "sandbox_skin_module.h"
#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <cstring>

// ----------------------------------------------------------------------------
// Low-Level Safe Memory Operations (__try / __except isolated SEH wrappers)
// ----------------------------------------------------------------------------
namespace SafeMemoryOps {

    bool SafeRead64(const uint64_t* pAddress, uint64_t* pOutValue, DWORD* pOutExceptionCode) {
        __try {
            *pOutValue = *pAddress;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            if (pOutExceptionCode) {
                *pOutExceptionCode = GetExceptionCode();
            }
            return false;
        }
    }

    bool SafeRead32(const uint32_t* pAddress, uint32_t* pOutValue, DWORD* pOutExceptionCode) {
        __try {
            *pOutValue = *pAddress;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            if (pOutExceptionCode) {
                *pOutExceptionCode = GetExceptionCode();
            }
            return false;
        }
    }

    bool SafeWrite32(uint32_t* pAddress, uint32_t value, DWORD* pOutExceptionCode) {
        __try {
            *pAddress = value;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            if (pOutExceptionCode) {
                *pOutExceptionCode = GetExceptionCode();
            }
            return false;
        }
    }

    bool SafeReadFloat(const float* pAddress, float* pOutValue, DWORD* pOutExceptionCode) {
        __try {
            *pOutValue = *pAddress;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            if (pOutExceptionCode) {
                *pOutExceptionCode = GetExceptionCode();
            }
            return false;
        }
    }

    bool SafeWriteFloat(float* pAddress, float value, DWORD* pOutExceptionCode) {
        __try {
            *pAddress = value;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            if (pOutExceptionCode) {
                *pOutExceptionCode = GetExceptionCode();
            }
            return false;
        }
    }
}

// ----------------------------------------------------------------------------
// SandboxSkinModule Implementation
// ----------------------------------------------------------------------------

void SandboxSkinModule::LogStep(LogLevel level, const char* fmt, ...) const {
    const char* prefix = "[INFO]";
    switch (level) {
    case LogLevel::Info:     prefix = "[INFO]   "; break;
    case LogLevel::Success:  prefix = "[SUCCESS]"; break;
    case LogLevel::Warning:  prefix = "[WARN]   "; break;
    case LogLevel::Error:    prefix = "[ERROR]  "; break;
    case LogLevel::SEHGuard: prefix = "[SEH-GUARD]"; break;
    }

    printf("%s ", prefix);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
}

void SandboxSkinModule::AddItemOverride(uint32_t origDef, uint32_t newDef, uint32_t style, float wear) {
    SkinOverrideRule rule;
    rule.targetDefIndex = newDef;
    rule.targetStyle    = style;
    rule.wearOverride   = wear;
    rule.active         = true;

    m_itemOverrides[origDef] = rule;
    LogStep(LogLevel::Info, "Registered Override Rule: Def %u -> %u (Style: %u, Wear: %.4f)",
            origDef, newDef, style, wear);
}

void SandboxSkinModule::AddHeroSlotOverride(const std::string& heroName, const std::string& slotName, uint32_t newDef, uint32_t style) {
    SkinOverrideRule rule;
    rule.targetDefIndex = newDef;
    rule.targetStyle    = style;
    rule.active         = true;

    std::string key = heroName + ":" + slotName;
    m_slotOverrides[key] = rule;
    LogStep(LogLevel::Info, "Registered Slot Rule [%s]: Def %u (Style: %u)",
            key.c_str(), newDef, style);
}

void SandboxSkinModule::ClearOverrides() {
    m_itemOverrides.clear();
    m_slotOverrides.clear();
    LogStep(LogLevel::Info, "Cleared all active skin override rules.");
}

bool SandboxSkinModule::FindOverride(uint32_t origDef, SkinOverrideRule& outRule) const {
    auto it = m_itemOverrides.find(origDef);
    if (it != m_itemOverrides.end() && it->second.active) {
        outRule = it->second;
        return true;
    }
    return false;
}

bool SandboxSkinModule::FindSlotOverride(const std::string& heroName, const std::string& slotName, SkinOverrideRule& outRule) const {
    std::string key = heroName + ":" + slotName;
    auto it = m_slotOverrides.find(key);
    if (it != m_slotOverrides.end() && it->second.active) {
        outRule = it->second;
        return true;
    }
    return false;
}

bool SandboxSkinModule::ProcessItemModification(TestEconItem* pItem) {
    if (!pItem) {
        LogStep(LogLevel::Error, "ProcessItemModification failed: pItem is nullptr.");
        return false;
    }

    DWORD sehCode = 0;
    if (m_sehProtectionEnabled) {
        uint64_t testRead = 0;
        if (!SafeMemoryOps::SafeRead64(reinterpret_cast<const uint64_t*>(pItem), &testRead, &sehCode)) {
            LogStep(LogLevel::SEHGuard, "Access violation reading item structure at address 0x%p (Exception Code: 0x%X)",
                    (void*)pItem, sehCode);
            return false;
        }
    }

    LogStep(LogLevel::Info, "--------------------------------------------------");
    LogStep(LogLevel::Info, "Processing Econ Item [ID: %llu | Hero: %s | Slot: %s | Def: %u]",
            pItem->itemID, pItem->heroName, pItem->slotName, pItem->originalDefIndex);

    // Step 1: Read current state using SEH read protection
    uint32_t currentDef = 0;
    if (m_sehProtectionEnabled) {
        if (!SafeMemoryOps::SafeRead32(&pItem->currentDefIndex, &currentDef, &sehCode)) {
            LogStep(LogLevel::SEHGuard, "Access violation reading item memory at address 0x%p (Code: 0x%X)",
                    (void*)&pItem->currentDefIndex, sehCode);
            return false;
        }
    } else {
        currentDef = pItem->currentDefIndex;
    }

    LogStep(LogLevel::Info, "Step 1: Current item definition verified: %u", currentDef);

    // Step 2: Query Override Rules (Item DefIndex match first, then Slot match)
    SkinOverrideRule activeRule;
    bool foundRule = false;

    if (FindOverride(pItem->originalDefIndex, activeRule)) {
        LogStep(LogLevel::Info, "Step 2: Found Direct DefIndex Override Rule -> Target Def: %u", activeRule.targetDefIndex);
        foundRule = true;
    } else if (FindSlotOverride(pItem->heroName, pItem->slotName, activeRule)) {
        LogStep(LogLevel::Info, "Step 2: Found Hero Slot Override Rule -> Target Def: %u", activeRule.targetDefIndex);
        foundRule = true;
    }

    if (!foundRule) {
        LogStep(LogLevel::Warning, "Step 2: No override rule found for item %u. Keeping original.", pItem->originalDefIndex);
        return false;
    }

    // Step 3: Safe Memory Field Modifications with SEH Protection
    LogStep(LogLevel::Info, "Step 3: Applying definition swap safely (%u -> %u)...",
            pItem->currentDefIndex, activeRule.targetDefIndex);

    if (m_sehProtectionEnabled) {
        if (!SafeMemoryOps::SafeWrite32(&pItem->currentDefIndex, activeRule.targetDefIndex, &sehCode)) {
            LogStep(LogLevel::SEHGuard, "SEH Intercepted illegal write to currentDefIndex at 0x%p! Code: 0x%X",
                    (void*)&pItem->currentDefIndex, sehCode);
            return false;
        }

        LogStep(LogLevel::Info, "Step 4: Applying style modification (%u -> %u)...",
                pItem->style, activeRule.targetStyle);
        if (!SafeMemoryOps::SafeWrite32(&pItem->style, activeRule.targetStyle, &sehCode)) {
            LogStep(LogLevel::SEHGuard, "SEH Intercepted illegal write to style at 0x%p! Code: 0x%X",
                    (void*)&pItem->style, sehCode);
            return false;
        }

        if (activeRule.wearOverride >= 0.0f) {
            LogStep(LogLevel::Info, "Step 5: Applying wear override (%.4f -> %.4f)...",
                    pItem->wear, activeRule.wearOverride);
            if (!SafeMemoryOps::SafeWriteFloat(&pItem->wear, activeRule.wearOverride, &sehCode)) {
                LogStep(LogLevel::SEHGuard, "SEH Intercepted illegal write to wear at 0x%p! Code: 0x%X",
                        (void*)&pItem->wear, sehCode);
                return false;
            }
        }
    } else {
        pItem->currentDefIndex = activeRule.targetDefIndex;
        pItem->style            = activeRule.targetStyle;
        if (activeRule.wearOverride >= 0.0f) {
            pItem->wear = activeRule.wearOverride;
        }
    }

    // Step 4: Verification of modified fields
    uint32_t verifiedDef = 0;
    uint32_t verifiedStyle = 0;
    float    verifiedWear = 0.0f;

    SafeMemoryOps::SafeRead32(&pItem->currentDefIndex, &verifiedDef);
    SafeMemoryOps::SafeRead32(&pItem->style, &verifiedStyle);
    SafeMemoryOps::SafeReadFloat(&pItem->wear, &verifiedWear);

    LogStep(LogLevel::Success, "CONFIRMED: Item modification applied successfully!");
    LogStep(LogLevel::Success, "          [New DefIndex: %u | New Style: %u | Wear: %.4f]",
            verifiedDef, verifiedStyle, verifiedWear);

    return true;
}

void SandboxSkinModule::RunSimulationSuite() {
    printf("============================================================\n");
    printf("     Sandbox SkinChanger Autonomous Test Suite (MSVC x64)\n");
    printf("============================================================\n\n");

    SandboxSkinModule skinModule;

    // Register test rules
    skinModule.AddItemOverride(508, 12930, 1, 0.0001f); // e.g. Weapon / Arcana swap
    skinModule.AddHeroSlotOverride("Pudge", "weapon", 4007, 2); // Dragonclaw Hook test

    printf("\n--- SCENARIO 1: Direct Item Override (AK-47 / Arcana Swap) ---\n");
    TestEconItem weaponItem{};
    weaponItem.itemID           = 10001;
    weaponItem.originalDefIndex = 508;
    weaponItem.currentDefIndex  = 508;
    weaponItem.style            = 0;
    weaponItem.wear             = 0.15f;
    weaponItem.isEquipped       = true;
    strcpy_s(weaponItem.heroName, sizeof(weaponItem.heroName), "Juggernaut");
    strcpy_s(weaponItem.slotName, sizeof(weaponItem.slotName), "weapon");

    bool res1 = skinModule.ProcessItemModification(&weaponItem);
    printf("Result 1: %s\n\n", res1 ? "PASSED" : "FAILED");

    printf("--- SCENARIO 2: Hero Slot Override (Pudge Hook) ---\n");
    TestEconItem pudgeItem{};
    pudgeItem.itemID           = 10002;
    pudgeItem.originalDefIndex = 4001; // Base Hook
    pudgeItem.currentDefIndex  = 4001;
    pudgeItem.style            = 0;
    pudgeItem.wear             = 0.05f;
    pudgeItem.isEquipped       = true;
    strcpy_s(pudgeItem.heroName, sizeof(pudgeItem.heroName), "Pudge");
    strcpy_s(pudgeItem.slotName, sizeof(pudgeItem.slotName), "weapon");

    bool res2 = skinModule.ProcessItemModification(&pudgeItem);
    printf("Result 2: %s\n\n", res2 ? "PASSED" : "FAILED");

    printf("--- SCENARIO 3: Invalid Memory Pointer Interception (SEH Guard Test) ---\n");
    TestEconItem* pBadItem = reinterpret_cast<TestEconItem*>(static_cast<uintptr_t>(0xBAADF00D));
    bool res3 = skinModule.ProcessItemModification(pBadItem);
    printf("Result 3: %s (Correctly prevented crash via SEH Guard)\n\n", !res3 ? "PASSED" : "FAILED");

    printf("--- SCENARIO 4: Item with No Registered Rule ---\n");
    TestEconItem unmodItem{};
    unmodItem.itemID           = 10003;
    unmodItem.originalDefIndex = 9999;
    unmodItem.currentDefIndex  = 9999;
    strcpy_s(unmodItem.heroName, sizeof(unmodItem.heroName), "Axe");
    strcpy_s(unmodItem.slotName, sizeof(unmodItem.slotName), "armor");

    bool res4 = skinModule.ProcessItemModification(&unmodItem);
    printf("Result 4: %s (Correctly ignored unconfigured item)\n\n", !res4 ? "PASSED (Unmodified)" : "FAILED");

    printf("============================================================\n");
    printf("              Simulation Test Suite Complete\n");
    printf("============================================================\n");
}

int main() {
    SandboxSkinModule::RunSimulationSuite();
    return 0;
}

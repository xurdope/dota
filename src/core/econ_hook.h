#pragma once
#include <cstdint>

// ============================================================
//  EconHook — C_EconItemView Loadout & Item Definition Hook
// ============================================================

namespace EconHook {
    struct FakeEconItem {
        uint64_t itemID;
        uint32_t defIndex;
        uint32_t heroID;
        uint32_t slot;
        uint32_t style;
        bool     equipped;
    };

    bool Install();
    void Uninstall();
    bool IsInstalled();
    
    // Inject fake item into local inventory cache (SOCache / Panorama)
    void InjectFakeItem(uint32_t heroID, uint32_t slot, uint32_t defIndex, uint32_t style = 0);
    void ClearFakeItems();

    // Перехват обработки предмета C_EconItemView
    uint32_t FilterItemDefinitionIndex(uint32_t originalDefIndex);
}
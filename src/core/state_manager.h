#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// ============================================================
//  StateManager — Заглушка подсистемы состояний
// ============================================================

enum class RenderSlot {
    Primary,
    Secondary,
    Effects,
    Count
};

struct PresetItem {
    uint32_t    id;
    std::string name;
    std::string assetPath;
    float       scale;
};

struct RuntimeStateConfig {
    std::unordered_map<RenderSlot, uint32_t> slotSelections;
};

class StateManager {
public:
    static StateManager& GetInstance() {
        static StateManager instance;
        return instance;
    }

    void Initialize();
    bool ApplyRuntimeState();

    uint32_t GetActivePreset(RenderSlot slot);
    uint32_t GetStagingPreset(RenderSlot slot) const;
    void     SetStagingPreset(RenderSlot slot, uint32_t presetId);
    const std::vector<PresetItem>& GetPresetsForSlot(RenderSlot slot) const;

private:
    StateManager() = default;
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;
};
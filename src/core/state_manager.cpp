#include "state_manager.h"
#include "../system/system_utils.h"

// ============================================================
//  StateManager implementation (stub)
// ============================================================

void StateManager::Initialize() {
    SystemUtils::Log("[SM] StateManager initialized.");
}

bool StateManager::ApplyRuntimeState() {
    return true;
}

uint32_t StateManager::GetActivePreset(RenderSlot) {
    return 0;
}

uint32_t StateManager::GetStagingPreset(RenderSlot) const {
    return 0;
}

void StateManager::SetStagingPreset(RenderSlot, uint32_t) {
}

const std::vector<PresetItem>& StateManager::GetPresetsForSlot(RenderSlot) const {
    static const std::vector<PresetItem> s_empty;
    return s_empty;
}
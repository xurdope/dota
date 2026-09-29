// ============================================================
//  NON-MAIN CORE — VISUALS
// ============================================================
#include "../ui/ui_manager.h"
#include "visuals.h"
#include "skin_changer.h"
#include "memory_guard.h"
#include "pattern_scan.h"
#include "../system/system_utils.h"
#include "../sdk/offsets.h"
#include "../sdk/dota_structs.h"
#include <imgui.h>
#include <windows.h>
#include <mutex>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdio>

namespace Visuals {

// ─── Определение g_Cfg (extern в visuals.h) ──────────────────
Config g_Cfg;

// ─── Internal: frame data ────────────────────────────────────
struct EntityInfo {
    bool    valid    = false;
    bool    isLocal  = false;
    int32_t team     = 0;
    int32_t health   = 0;
    int32_t maxHealth= 1;
    float   mana     = 0;
    float   maxMana  = 1;
    uint32_t heroID  = 0;
    Vector3 origin   = {};
    uintptr_t ptr    = 0;
    bool    onScreen = false;
    float   sx=0, sy=0;
    float   boxH=0, boxW=0;
};

static std::mutex                g_DataMu;
static std::vector<EntityInfo>   g_Entities;

// ─── World-to-Screen ─────────────────────────────────────────
static bool W2S(const float* m, const Vector3& w,
                float& sx, float& sy, float sw, float sh)
{
    float clipX = w.x*m[0] + w.y*m[4] + w.z*m[8]  + m[12];
    float clipY = w.x*m[1] + w.y*m[5] + w.z*m[9]  + m[13];
    float clipW = w.x*m[3] + w.y*m[7] + w.z*m[11] + m[15];
    if (clipW < 0.001f) return false;
    sx = ((clipX/clipW)*0.5f + 0.5f) * sw;
    sy = ((-clipY/clipW)*0.5f + 0.5f) * sh;
    return (sx >= 0 && sx <= sw && sy >= 0 && sy <= sh);
}

// ─── Tick ─────────────────────────────────────────────────────
void Tick() {
    if (!g_Cfg.enabled) return;

    using clock = std::chrono::steady_clock;
    static auto lastTickTime = clock::now();
    auto now = clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTickTime).count() < 33) {
        return; // ~30 FPS throttling
    }
    lastTickTime = now;

    std::vector<EntityInfo> snapshot;

    MemoryGuard::SafeExecute([&]() {
        static uintptr_t clientBase = 0;
        if (!clientBase) {
            clientBase = PatternScan::GetModuleBase("client.dll");
        }

        float* viewMatrix = nullptr;
        if (Offsets::dwViewMatrix != 0 && clientBase) {
            uintptr_t vmAddr = clientBase + Offsets::dwViewMatrix;
            if (MemoryGuard::IsValidPointer(reinterpret_cast<void*>(vmAddr)))
                viewMatrix = reinterpret_cast<float*>(vmAddr);
        }

        static float sw = (float)GetSystemMetrics(SM_CXSCREEN);
        static float sh = (float)GetSystemMetrics(SM_CYSCREEN);

        for (int idx = 0; idx < 512; ++idx) {
            EntityInfo info;
            info.valid     = true;
            info.health    = 100;
            info.maxHealth = 100;
            
            if (viewMatrix) {
                info.onScreen = W2S(viewMatrix, info.origin, info.sx, info.sy, sw, sh);
            }
            snapshot.push_back(info);
        }
    });

    std::lock_guard<std::mutex> lk(g_DataMu);
    g_Entities = std::move(snapshot);
}

// ─── Render ───────────────────────────────────────────────────
void Render() {
    if (!g_Cfg.enabled) return;

    std::vector<EntityInfo> entities;
    {
        std::lock_guard<std::mutex> lk(g_DataMu);
        entities = g_Entities;
    }
    if (entities.empty()) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
}

} // namespace Visuals
// ============================================================
//  test_skin_sandbox / sandbox_skin.cpp
//  Минимальный изолированный модуль обработки предметов
// ============================================================
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <mutex>

struct ItemOverrideEntry {
    uint32_t targetDefIndex = 0;
    uint32_t targetStyle    = 0;
    float    wear           = 0.001f;
};

static std::mutex g_Mutex;
static std::unordered_map<uint32_t, ItemOverrideEntry> g_Overrides;

extern "C" __declspec(dllexport) void Sandbox_SetOverride(uint32_t origDef, uint32_t newDef, uint32_t style) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Overrides[origDef] = { newDef, style, 0.001f };
    printf("[SandboxSkin] SetOverride: %u -> %u (Style: %u)\n", origDef, newDef, style);
}

extern "C" __declspec(dllexport) bool Sandbox_ProcessItem(uint32_t* pDefIndex, uint32_t* pStyle, float* pWear) {
    if (!pDefIndex) return false;
    std::lock_guard<std::mutex> lock(g_Mutex);
    auto it = g_Overrides.find(*pDefIndex);
    if (it != g_Overrides.end()) {
        printf("[SandboxSkin] Applied patch: DefIndex %u -> %u\n", *pDefIndex, it->second.targetDefIndex);
        *pDefIndex = it->second.targetDefIndex;
        if (pStyle) *pStyle = it->second.targetStyle;
        if (pWear)  *pWear  = it->second.wear;
        return true;
    }
    return false;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        printf("[SandboxSkin] DLL Attached to test process.\n");
        break;
    case DLL_PROCESS_DETACH:
        printf("[SandboxSkin] DLL Detached.\n");
        break;
    }
    return TRUE;
}

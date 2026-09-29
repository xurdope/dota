#include <windows.h>
#include "system/system_utils.h"
#include "core/skin_changer.h"
#include "hooks/present_hook.h"
#include "ui/ui_manager.h"

DWORD WINAPI MainThread(LPVOID lpParam) {
    HMODULE hModule = static_cast<HMODULE>(lpParam);

    SystemUtils::InitializeConsole();
    SystemUtils::Log("[+] Spectre.gg loaded.");

    // ── Ждём client.dll ─────────────────────────────────────────────────
    SystemUtils::Log("[Main] Ожидаем client.dll...");
    for (int i = 0; i < 50; ++i) {
        if (GetModuleHandleA("client.dll")) break;
        Sleep(200);
    }
    if (!GetModuleHandleA("client.dll")) {
        SystemUtils::Log("[Main] FATAL: client.dll не загружена за 10 секунд!");
        FreeLibraryAndExitThread(hModule, 1);
        return 1;
    }
    SystemUtils::Log("[Main] client.dll найдена.");

    // ── SkinChanger ─────────────────────────────────────────────────────
    // EntitySystem ищется лениво в первых тиках Tick()
    SystemUtils::Log("[Main] >>> SkinChanger::Initialize()");
    SkinChanger::GetInstance().Initialize();
    SkinChanger::GetInstance().SetEnabled(true);
    SystemUtils::Log("[Main] <<< SkinChanger OK");

    // ── UIManager ────────────────────────────────────────────────────────
    SystemUtils::Log("[Main] >>> UIManager::Initialize()");
    UIManager::Initialize();
    SystemUtils::Log("[Main] <<< UIManager OK");

    // ── PresentHook (D3D11 рендер + ImGui) ───────────────────────────────
    SystemUtils::Log("[Main] Ожидаем готовность DirectX 11...");
    Sleep(2000);
    SystemUtils::Log("[Main] >>> PresentHook::Install()");
    if (PresentHook::Install()) {
        SystemUtils::Log("[Main] <<< PresentHook OK");
    } else {
        SystemUtils::Log("[Main] <<< PresentHook FAILED — меню недоступно");
    }

    SystemUtils::Log("[+] Spectre.gg ready. INSERT = toggle | END = unload");

    // ── Главный цикл ─────────────────────────────────────────────────────
    while (UIManager::IsRunning()) {
        if (GetAsyncKeyState(VK_END) & 0x8000) {
            SystemUtils::Log("[+] END нажат, выгружаем...");
            break;
        }
        Sleep(100);
    }

    // ── Shutdown ─────────────────────────────────────────────────────────
    SystemUtils::Log("[+] Shutting down...");
    SkinChanger::GetInstance().SetEnabled(false);
    SkinChanger::GetInstance().SetEnabled(false);
    PresentHook::Uninstall();
    UIManager::Shutdown();
    SystemUtils::Log("[+] Done.");
    SystemUtils::CleanupConsole();

    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        if (HANDLE hThread = CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr))
            CloseHandle(hThread);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
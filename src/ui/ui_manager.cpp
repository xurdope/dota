// ============================================================
//  Spectre.gg — UI MANAGER BRIDGE
//  Menu::Init вызывается из PresentHook после создания ImGui контекста
// ============================================================
#include "ui_manager.h"
#include "menu.h"
#include "../core/skin_changer.h"
#include "../core/visuals.h"
#include "../system/system_utils.h"
#include <windows.h>
#include <imgui.h>

namespace UIManager {

static bool g_Running   = true;
static bool g_ImGuiInit = false;

void Initialize() {
    // Не трогаем ImGui здесь — контекст создаёт PresentHook
    SystemUtils::Log("[UI] Spectre.gg UI pre-init (ImGui pending PresentHook).");
}

// Вызывается из PresentHook::hkPresent после ImGui::CreateContext + Impl::Init
void InitImGui() {
    if (g_ImGuiInit) return;
    Menu::Fonts fonts;
    fonts.regular = nullptr;
    fonts.bold    = nullptr;
    fonts.icons   = nullptr;
    Menu::Init(fonts, 1.0f);
    g_ImGuiInit = true;
    SystemUtils::Log("[UI] Menu::Init done. INSERT to toggle.");
}

void Shutdown() {
    SystemUtils::Log("[UI] Shutdown.");
    g_Running = false;
}

void ToggleMenu() {
    Menu::SetOpen(!Menu::IsOpen());
}

bool IsMenuOpen()  { return g_ImGuiInit && Menu::IsOpen(); }
bool IsRunning()   { return g_Running; }

void RenderFrame() {
    if (!g_ImGuiInit) return;

    // Единый жесткий переключатель кнопки INSERT с защитой от дребезга (debounce)
    static bool prevInsertState = false;
    bool currentInsertState = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (currentInsertState && !prevInsertState) {
        Menu::SetOpen(!Menu::IsOpen()); // Надежное переключение флага ровно 1 раз за нажатие
    }
    prevInsertState = currentInsertState;

    // Тик скинченджера и визуалов каждый кадр
    SkinChanger::GetInstance().Tick();
    Visuals::Tick();

    // Если меню закрыто по INSERT — полностью пропускаем рендер и фона, и окон!
    if (!IsMenuOpen()) {
        return;
    }

    // Основной рендер меню
    Menu::Render();
}

void RenderSkinChangerTab() {}

} // namespace UIManager
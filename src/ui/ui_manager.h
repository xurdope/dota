#pragma once

namespace UIManager {
    void Initialize();   // вызывается из DllMain (до ImGui)
    void InitImGui();    // вызывается из PresentHook (после ImGui::CreateContext)
    void Shutdown();
    void ToggleMenu();
    bool IsMenuOpen();
    bool IsRunning();
    void RenderFrame();
    void RenderSkinChangerTab();
}
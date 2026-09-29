#pragma once
// Красивое меню на Dear ImGui (1.92+).
// Файл не зависит от платформы: только imgui.h / imgui_internal.h.

struct ImFont;

namespace Menu
{
    struct Fonts
    {
        ImFont* regular = nullptr; // основной текст
        ImFont* bold    = nullptr; // заголовки (если nullptr — берётся regular)
        ImFont* icons   = nullptr; // иконочный шрифт (Segoe MDL2 / Font Awesome), можно nullptr
    };

    // Вызвать один раз после создания контекста ImGui и загрузки шрифтов.
    // dpi_scale — масштаб монитора (1.0 = 100%).
    void Init(const Fonts& fonts, float dpi_scale = 1.0f);

    // Вызывать каждый кадр между ImGui::NewFrame() и ImGui::Render().
    void Render();

    bool IsOpen();
    void SetOpen(bool open);
}

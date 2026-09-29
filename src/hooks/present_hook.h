#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

// ============================================================
//  PresentHook — ImGui overlay через vtable hook на IDXGISwapChain::Present
//
//  Как работает:
//    1. Создаём dummy D3D11 device + swapchain для получения vtable
//    2. Патчим vtable[8] (Present) — все IDXGISwapChain делят одну vtable (COM)
//    3. В hooked Present: первый вызов = инит ImGui на реальном device игры
//    4. Каждый вызов = рендерим ImGui поверх игрового кадра
//
//  WndProc тоже хукается для корректного ввода (мышь/клавиатура в ImGui).
// ============================================================

namespace PresentHook {
    bool Install();    // Создаёт dummy device, патчит vtable
    void Uninstall();  // Убирает WndProc hook, шатдаун ImGui, освобождает ресурсы

    bool IsInstalled();
    bool IsImGuiReady(); // true = ImGui уже инициализирован на реальном device
}

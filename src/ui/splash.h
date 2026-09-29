#pragma once

// Экран загрузки в стиле NonMain.fun.
// Рисуется поверх всего на foreground draw list, ничего не «занимает» в layout.
namespace splash
{
    // progress — 0..1, ты сам его двигаешь под свою реальную загрузку.
    // Возвращает true, когда заставка полностью отыграла и погасла (можно показывать меню/логин).
    bool Render(float progress);

    // Демо-режим: сам гонит прогресс от 0 к 1 за duration секунд. Возвращает true по завершении.
    bool RenderAuto(float duration = 3.0f);

    void Reset();   // прокрутить заставку заново
}

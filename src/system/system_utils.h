#pragma once
#include <windows.h>

namespace SystemUtils {
    void InitializeConsole();
    void CleanupConsole();
    void Log(const char* format, ...);
    HWND GetTargetWindow();
}
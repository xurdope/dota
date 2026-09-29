#include "system_utils.h"
#include <cstdio>
#include <cstdarg>

namespace SystemUtils {
    static FILE* g_pCin  = nullptr;
    static FILE* g_pCout = nullptr;
    static FILE* g_pCerr = nullptr;
    static FILE* g_pLog  = nullptr;

    void InitializeConsole() {
        // ── 1. Файл-лог (переживёт краш) ─────────────────────
        fopen_s(&g_pLog, "C:\\spectre_log.txt", "w");
        if (g_pLog) {
            setvbuf(g_pLog, nullptr, _IONBF, 0);   // без буферизации
            fprintf(g_pLog, "[+] Spectre.gg | Log: C:\\spectre_log.txt\n");
        }

        // ── 2. Консоль ───────────────────────────────────────
        if (AllocConsole()) {
            freopen_s(&g_pCin,  "CONIN$",  "r", stdin);
            freopen_s(&g_pCout, "CONOUT$", "w", stdout);
            freopen_s(&g_pCerr, "CONOUT$", "w", stderr);
            SetConsoleTitleA("Spectre.gg | Debug Console");
            printf("[+] Spectre.gg loaded.\n");
        }
    }

    void CleanupConsole() {
        if (g_pLog) { fprintf(g_pLog, "[+] Cleanup\n"); fclose(g_pLog); g_pLog = nullptr; }
        if (g_pCout) {
            printf("[+] Cleaning up console...\n");
            fclose(g_pCout);
            fclose(g_pCin);
            fclose(g_pCerr);
            FreeConsole();
        }
    }

    void Log(const char* format, ...) {
        char buf[2048];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);

        // В консоль
        printf("%s\n", buf);
        fflush(stdout);

        // В файл — без буферизации, каждая строка сразу на диске
        if (g_pLog) {
            fprintf(g_pLog, "%s\n", buf);
            fflush(g_pLog);
        }
    }

    struct EnumWindowData {
        DWORD pid;
        HWND  hwnd;
    };

    static BOOL CALLBACK EnumWindowCallback(HWND hwnd, LPARAM lParam) {
        EnumWindowData* data = reinterpret_cast<EnumWindowData*>(lParam);
        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);

        if (windowPid == data->pid &&
            GetWindow(hwnd, GW_OWNER) == NULL &&
            IsWindowVisible(hwnd)) {
            RECT rect;
            GetClientRect(hwnd, &rect);
            if (rect.right > 100 && rect.bottom > 100) {
                data->hwnd = hwnd;
                return FALSE;
            }
        }
        return TRUE;
    }

    HWND GetTargetWindow() {
        EnumWindowData data = { GetCurrentProcessId(), NULL };
        EnumWindows(EnumWindowCallback, reinterpret_cast<LPARAM>(&data));
        return data.hwnd;
    }
}
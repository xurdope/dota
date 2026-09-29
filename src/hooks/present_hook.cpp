#include "present_hook.h"
#include "../ui/ui_manager.h"
#include "../core/visuals.h"
#include "../system/system_utils.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>


// ============================================================
//  PresentHook Implementation
// ============================================================

// ImGui_ImplWin32_WndProcHandler: объявлен в imgui_impl_win32.h внутри #if 0 блока
// намеренно (чтобы не тащить <windows.h>). Копируем объявление вручную как написано в ImGui доках.
IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace PresentHook {

// IDXGISwapChain vtable indices (стандарт DXGI, не меняется)
static constexpr int kSlotPresent       = 8;
static constexpr int kSlotResizeBuffers = 13;

// Типы оригинальных функций
using FnPresent       = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using FnResizeBuffers = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

// ─── State ────────────────────────────────────────────────────
static FnPresent       g_oPresent        = nullptr;
static FnResizeBuffers g_oResizeBuffers  = nullptr;

static ID3D11Device*           g_pDevice   = nullptr;
static ID3D11DeviceContext*    g_pContext  = nullptr;
static ID3D11RenderTargetView* g_pMainRTV  = nullptr;
static HWND                    g_hGameWnd  = nullptr;
static WNDPROC                 g_oWndProc  = nullptr;

static bool g_bInstalled   = false;
static bool g_bImGuiReady  = false;

// ─── Vtable patcher ───────────────────────────────────────────
static bool PatchVtable(void** vtable, int slot, void* hookFn, void** outOriginal) {
    DWORD oldProt = 0;
    if (!VirtualProtect(&vtable[slot], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt))
        return false;
    *outOriginal    = vtable[slot];
    vtable[slot]    = hookFn;
    VirtualProtect(&vtable[slot], sizeof(void*), oldProt, &oldProt);
    return true;
}

// ─── Render Target View ───────────────────────────────────────
static void CreateMainRTV(IDXGISwapChain* pSC) {
    ID3D11Texture2D* pBB = nullptr;
    if (SUCCEEDED(pSC->GetBuffer(0, IID_PPV_ARGS(&pBB)))) {
        g_pDevice->CreateRenderTargetView(pBB, nullptr, &g_pMainRTV);
        pBB->Release();
    }
}

static void ReleaseMainRTV() {
    if (g_pMainRTV) { g_pMainRTV->Release(); g_pMainRTV = nullptr; }
}

// ─── WndProc Hook ─────────────────────────────────────────────
static LRESULT WINAPI hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!g_oWndProc) return DefWindowProcW(hWnd, msg, wParam, lParam);

    if (g_bImGuiReady && ImGui::GetCurrentContext() != nullptr) {
        if (::ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
            if (UIManager::IsMenuOpen()) return true;
        }
    }

    // Блокируем мышь игре пока меню открыто
    if (UIManager::IsMenuOpen()) {
        switch (msg) {
            case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN: case WM_MBUTTONUP:
            case WM_MOUSEWHEEL:  case WM_MOUSEMOVE:
                return 0; // не передаём игре
        }
    }

    return CallWindowProcW(g_oWndProc, hWnd, msg, wParam, lParam);
}

// ─── ResizeBuffers Hook (пересоздаём RTV при изменении размера окна) ──
static HRESULT STDMETHODCALLTYPE hkResizeBuffers(
        IDXGISwapChain* pSC, UINT Count,
        UINT W, UINT H, DXGI_FORMAT Fmt, UINT Flags) {
    ReleaseMainRTV();
    HRESULT hr = g_oResizeBuffers(pSC, Count, W, H, Fmt, Flags);
    if (SUCCEEDED(hr) && g_pDevice) CreateMainRTV(pSC);
    return hr;
}

// ─── Present Hook — сердце всего ──────────────────────────────
static HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain* pSC, UINT SyncInterval, UINT Flags) {

    // ─── Первый вызов: инициализируем ImGui на реальном device игры ──
    if (!g_bImGuiReady) {
        DXGI_SWAP_CHAIN_DESC sd = {};
        if (FAILED(pSC->GetDesc(&sd)) || sd.BufferDesc.Width < 100 || sd.BufferDesc.Height < 100 || !IsWindow(sd.OutputWindow) || !IsWindowVisible(sd.OutputWindow)) {
            return g_oPresent(pSC, SyncInterval, Flags);
        }

        // Получаем device из реального SwapChain Dota 2
        if (FAILED(pSC->GetDevice(IID_PPV_ARGS(&g_pDevice)))) {
            return g_oPresent(pSC, SyncInterval, Flags);
        }
        g_pDevice->GetImmediateContext(&g_pContext);

        g_hGameWnd = sd.OutputWindow;

        // Создаём RTV на backbuffer игры
        CreateMainRTV(pSC);
        if (!g_pMainRTV) {
            SystemUtils::Log("[PresentHook] Failed to create RTV. Skipping init.");
            return g_oPresent(pSC, SyncInterval, Flags);
        }

        // Инициализируем ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io    = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // ─── Font loading ─────────────────────────────────────
        // Попробуем загрузить Segoe UI (чёткий системный шрифт).
        // Fallback → ImGui default 13px
        static const char* kFontPaths[] = {
            "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\calibri.ttf",
            "C:\\Windows\\Fonts\\arial.ttf",
            nullptr
        };
        bool fontLoaded = false;
        for (int fi = 0; kFontPaths[fi] && !fontLoaded; ++fi) {
            if (GetFileAttributesA(kFontPaths[fi]) == INVALID_FILE_ATTRIBUTES) continue;
            if (io.Fonts->AddFontFromFileTTF(kFontPaths[fi], 13.f)) {
                io.Fonts->AddFontFromFileTTF(kFontPaths[fi], 15.f);
                io.Fonts->AddFontFromFileTTF(kFontPaths[fi], 10.f);
                fontLoaded = true;
                SystemUtils::Log("[PresentHook] Font loaded: %s", kFontPaths[fi]);
            }
        }
        if (!fontLoaded) {
            io.Fonts->AddFontDefault();
            SystemUtils::Log("[PresentHook] Using default ImGui font.");
        }
        // NOTE: do NOT call io.Fonts->Build() manually —
        // ImGui DX11 backend with ImGuiBackendFlags_RendererHasTextures
        // builds the atlas automatically on first frame.

        ImGui_ImplWin32_Init(g_hGameWnd);
        ImGui_ImplDX11_Init(g_pDevice, g_pContext);

        // Теперь ImGui контекст готов — инициализируем наш UI (Menu::Init)
        UIManager::InitImGui();

        // Хукаем WndProc для корректного ввода (Unicode для Source 2)
        g_oWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(g_hGameWnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(hkWndProc)));

        g_bImGuiReady = true;
        SystemUtils::Log("[PresentHook] ImGui ready. Device=0x%p  HWND=0x%p",
            g_pDevice, g_hGameWnd);
    }

    if (!g_pMainRTV) return g_oPresent(pSC, SyncInterval, Flags);

    // ─── Каждый кадр: рендерим ImGui поверх игры ─────────────────
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    UIManager::RenderFrame(); // INSERT toggle + SkinChanger tick + menu draw

    ImGui::Render();
    g_pContext->OMSetRenderTargets(1, &g_pMainRTV, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return g_oPresent(pSC, SyncInterval, Flags);
}

// ─── Public API ───────────────────────────────────────────────

bool Install() {
    SystemUtils::Log("[PresentHook] Installing...");

    // Создаём dummy невидимое окно
    HWND hDummy = CreateWindowExA(
        0, "STATIC", "DX11Dummy",
        WS_POPUP, 0, 0, 8, 8,
        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (!hDummy) {
        SystemUtils::Log("[PresentHook] Failed to create dummy window. GLE=%lu", GetLastError());
        return false;
    }

    // Минимальный SwapChain descriptor для dummy device
    DXGI_SWAP_CHAIN_DESC sd   = {};
    sd.BufferCount            = 1;
    sd.BufferDesc.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.Width       = 8;
    sd.BufferDesc.Height      = 8;
    sd.BufferUsage            = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow           = hDummy;
    sd.SampleDesc.Count       = 1;
    sd.Windowed               = TRUE;
    sd.SwapEffect             = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device*   pDummyDev = nullptr;
    IDXGISwapChain* pDummySC  = nullptr;
    D3D_FEATURE_LEVEL fl;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &pDummySC, &pDummyDev, &fl, nullptr);

    if (FAILED(hr) || !pDummySC) {
        SystemUtils::Log("[PresentHook] D3D11 dummy device failed. HR=0x%08X", hr);
        DestroyWindow(hDummy);
        return false;
    }

    // Все IDXGISwapChain из одной DLL (dxgi.dll) — одна vtable
    // Патчим через dummy → затрагивает и Dota SwapChain
    void** vtable = *reinterpret_cast<void***>(pDummySC);

    bool ok = PatchVtable(vtable, kSlotPresent,
        reinterpret_cast<void*>(hkPresent),
        reinterpret_cast<void**>(&g_oPresent));

    if (!ok) {
        SystemUtils::Log("[PresentHook] VirtualProtect failed on vtable[%d].", kSlotPresent);
        pDummySC->Release(); pDummyDev->Release();
        DestroyWindow(hDummy);
        return false;
    }

    // ResizeBuffers hook — чтобы не крашилось при изменении размера окна
    void* dummy = nullptr;
    PatchVtable(vtable, kSlotResizeBuffers,
        reinterpret_cast<void*>(hkResizeBuffers),
        &dummy);
    g_oResizeBuffers = reinterpret_cast<FnResizeBuffers>(dummy);

    // Освобождаем dummy (hook уже работает — vtable пропатчена)
    pDummySC->Release();
    pDummyDev->Release();
    DestroyWindow(hDummy);

    g_bInstalled = true;
    SystemUtils::Log("[PresentHook] vtable hook installed. oPresent=0x%p", (void*)g_oPresent);
    return true;
}

void Uninstall() {
    if (!g_bInstalled) return;
    SystemUtils::Log("[PresentHook] Uninstalling...");

    // Восстанавливаем WndProc
    if (g_hGameWnd && g_oWndProc) {
        SetWindowLongPtrW(g_hGameWnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(g_oWndProc));
        g_oWndProc = nullptr;
    }

    // Шатдаун ImGui
    if (g_bImGuiReady) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_bImGuiReady = false;
    }

    // Освобождаем D3D ресурсы
    ReleaseMainRTV();
    if (g_pContext) { g_pContext->Release(); g_pContext = nullptr; }
    if (g_pDevice)  { g_pDevice->Release();  g_pDevice  = nullptr; }

    // Примечание: vtable не восстанавливаем намеренно —
    // после FreeLibraryAndExitThread код DLL выгружается,
    // восстановление vtable здесь безопаснее не делать
    // (можно добавить MinHook для чистого unhook).

    g_bInstalled = false;
    SystemUtils::Log("[PresentHook] Done.");
}

bool IsInstalled()  { return g_bInstalled;  }
bool IsImGuiReady() { return g_bImGuiReady; }

} // namespace PresentHook

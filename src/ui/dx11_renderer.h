#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

// ============================================================
//  DX11Renderer — УСТАРЕЛО / LEGACY
//
//  Рендеринг ImGui теперь происходит через PresentHook
//  (src/hooks/present_hook.h/cpp).
//
//  Этот файл оставлен для совместимости, но DX11Renderer
//  больше не используется в основном потоке.
// ============================================================

namespace Graphics {

    class DX11Renderer {
    public:
        static DX11Renderer& GetInstance() {
            static DX11Renderer instance;
            return instance;
        }

        bool InitializeDevice(HWND hWnd, UINT width, UINT height);
        bool InitializeImGui(HWND hWnd);

        void BeginFrame();
        void RenderUI();
        void EndFrame();
        void Shutdown();

        ID3D11Device*        GetDevice()        const { return m_pd3dDevice; }
        ID3D11DeviceContext* GetDeviceContext()  const { return m_pd3dImmediateContext; }
        IDXGISwapChain*      GetSwapChain()      const { return m_pSwapChain; }

    private:
        DX11Renderer()  = default;
        ~DX11Renderer() { Shutdown(); }
        DX11Renderer(const DX11Renderer&) = delete;
        DX11Renderer& operator=(const DX11Renderer&) = delete;

        void CreateRenderTarget();
        void CleanupRenderTarget();

        ID3D11Device*           m_pd3dDevice             = nullptr;
        ID3D11DeviceContext*    m_pd3dImmediateContext    = nullptr;
        IDXGISwapChain*         m_pSwapChain              = nullptr;
        ID3D11RenderTargetView* m_mainRenderTargetView    = nullptr;
        bool                    m_isImGuiInitialized      = false;
        HWND                    m_hWnd                    = nullptr;
    };

} // namespace Graphics

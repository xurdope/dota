#include "dx11_renderer.h"
#include "../system/system_utils.h"
#include "../core/state_manager.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include "ui_manager.h"

namespace Graphics {

    bool DX11Renderer::InitializeDevice(HWND hWnd, UINT width, UINT height) {
        if (!hWnd) {
            SystemUtils::Log("[DX11Renderer] Invalid window handle (HWND is null).");
            return false;
        }

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        m_hWnd = hWnd; // сохраняем для foreground-проверки

        UINT createDeviceFlags = 0;
#ifdef _DEBUG
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0,
        };

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &m_pSwapChain,
            &m_pd3dDevice,
            &featureLevel,
            &m_pd3dImmediateContext
        );

        if (FAILED(hr)) {
            SystemUtils::Log("[DX11Renderer] Failed to create D3D11 device and Swap Chain. HRESULT: 0x%08X", hr);
            return false;
        }

        CreateRenderTarget();
        SystemUtils::Log("[DX11Renderer] DirectX 11 Device and SwapChain initialized successfully.");
        return true;
    }

    void DX11Renderer::CreateRenderTarget() {
        if (!m_pSwapChain || !m_pd3dDevice) return;

        ID3D11Texture2D* pBackBuffer = nullptr;
        HRESULT hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        if (SUCCEEDED(hr) && pBackBuffer) {
            m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_mainRenderTargetView);
            
            D3D11_TEXTURE2D_DESC desc;
            pBackBuffer->GetDesc(&desc);
            
            D3D11_VIEWPORT vp = {};
            vp.Width = static_cast<FLOAT>(desc.Width);
            vp.Height = static_cast<FLOAT>(desc.Height);
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            vp.TopLeftX = 0;
            vp.TopLeftY = 0;
            m_pd3dImmediateContext->RSSetViewports(1, &vp);

            pBackBuffer->Release();
        }
    }

    void DX11Renderer::CleanupRenderTarget() {
        if (m_mainRenderTargetView) {
            m_mainRenderTargetView->Release();
            m_mainRenderTargetView = nullptr;
        }
    }

    bool DX11Renderer::InitializeImGui(HWND hWnd) {
        if (m_isImGuiInitialized) return true;

        SystemUtils::Log("[DX11Renderer] Initializing ImGui context and backends...");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(hWnd);
        ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dImmediateContext);

        m_isImGuiInitialized = true;
        SystemUtils::Log("[DX11Renderer] ImGui successfully initialized.");
        return true;
    }

    void DX11Renderer::BeginFrame() {
        if (!m_isImGuiInitialized) return;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    // LEGACY: рендеринг теперь через PresentHook
    void DX11Renderer::RenderUI() {
        UIManager::RenderFrame();
    }


    void DX11Renderer::EndFrame() {
        if (!m_isImGuiInitialized || !m_pd3dImmediateContext || !m_pSwapChain) return;

        ImGui::Render();

        const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_pd3dImmediateContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
        m_pd3dImmediateContext->ClearRenderTargetView(m_mainRenderTargetView, clearColor);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        m_pSwapChain->Present(1, 0);
    }

    void DX11Renderer::Shutdown() {
        if (m_isImGuiInitialized) {
            SystemUtils::Log("[DX11Renderer] Shutting down ImGui backends and context...");
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            m_isImGuiInitialized = false;
        }

        CleanupRenderTarget();

        if (m_pSwapChain) {
            m_pSwapChain->Release();
            m_pSwapChain = nullptr;
        }
        if (m_pd3dImmediateContext) {
            m_pd3dImmediateContext->Release();
            m_pd3dImmediateContext = nullptr;
        }
        if (m_pd3dDevice) {
            m_pd3dDevice->Release();
            m_pd3dDevice = nullptr;
        }

        SystemUtils::Log("[DX11Renderer] DirectX 11 graphics context cleaned up.");
    }

} // namespace Graphics
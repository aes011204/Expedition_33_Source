#include "pch.h"
#include "UI_EditorApp.h"

namespace UIEditor
{
    UI_EditorApp::~UI_EditorApp()
    {
        Shutdown();
    }

    HRESULT UI_EditorApp::Initialize(HWND hWnd)
    {
        _hWnd = hWnd;

        if (FAILED(Create_DeviceObjects(hWnd)))
            return E_FAIL;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(hWnd);
        ImGui_ImplDX11_Init(_device.Get(), _context.Get());

        _initialized = true;
        return S_OK;
    }

    void UI_EditorApp::Render_Frame()
    {
        if (!_initialized)
            return;

        Begin_ImGuiFrame();

        ImGui::Render();

        constexpr float clearColor[4] = { 0.08f, 0.08f, 0.09f, 1.0f };
        _context->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), nullptr);
        _context->ClearRenderTargetView(_renderTargetView.Get(), clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        _swapChain->Present(1, 0);
    }

    void UI_EditorApp::Resize(uint32 width, uint32 height)
    {
        if (!_initialized || width == 0 || height == 0)
            return;

        Cleanup_RenderTarget();
        if (SUCCEEDED(_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)))
            Create_RenderTarget();
    }

    void UI_EditorApp::Shutdown()
    {
        if (!_initialized)
            return;

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        Cleanup_RenderTarget();
        _swapChain.Reset();
        _context.Reset();
        _device.Reset();
        _initialized = false;
    }

    HRESULT UI_EditorApp::Create_DeviceObjects(HWND hWnd)
    {
        DXGI_SWAP_CHAIN_DESC swapDesc {};
        swapDesc.BufferCount = 2;
        swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.OutputWindow = hWnd;
        swapDesc.SampleDesc.Count = 1;
        swapDesc.Windowed = TRUE;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL featureLevel {};
        const D3D_FEATURE_LEVEL requestedLevels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0,
        };

        if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            requestedLevels,
            static_cast<UINT>(sizeof(requestedLevels) / sizeof(requestedLevels[0])),
            D3D11_SDK_VERSION,
            &swapDesc,
            _swapChain.GetAddressOf(),
            _device.GetAddressOf(),
            &featureLevel,
            _context.GetAddressOf())))
        {
            return E_FAIL;
        }

        return Create_RenderTarget();
    }

    void UI_EditorApp::Cleanup_RenderTarget()
    {
        _renderTargetView.Reset();
    }

    HRESULT UI_EditorApp::Create_RenderTarget()
    {
        ComPtr<ID3D11Texture2D> backBuffer = nullptr;
        if (FAILED(_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()))))
            return E_FAIL;

        return _device->CreateRenderTargetView(backBuffer.Get(), nullptr, _renderTargetView.GetAddressOf());
    }

    void UI_EditorApp::Begin_ImGuiFrame()
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }
}

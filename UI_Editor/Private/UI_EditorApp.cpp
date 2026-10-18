#include "pch.h"
#include "UI_EditorApp.h"

#include "GameInstance.h"
#include "UI_Editor_Instance.h"

namespace UIEditor
{
    UI_EditorApp::~UI_EditorApp()
    {
        Shutdown();
    }

    HRESULT UI_EditorApp::Initialize(HWND hWnd)
    {
        const auto startupBeginTime = chrono::steady_clock::now();
        auto lastStepTime = startupBeginTime;
        auto logStartupStep = [&lastStepTime](const char* stepName)
        {
            const auto nowTime = chrono::steady_clock::now();
            const double elapsedMilliseconds = chrono::duration<double, milli>(nowTime - lastStepTime).count();
            LOG_INFO("UI Editor startup: {} {:.2f} ms", stepName, elapsedMilliseconds);
            lastStepTime = nowTime;
        };

        _hWnd = hWnd;

        if (FAILED(Create_DeviceObjects(hWnd)))
            return E_FAIL;
        logStartupStep("device");

        const wstring resourceRoot = Resolve_ClientResourceRoot();
        logStartupStep("resource-root");
        if (!resourceRoot.empty())
        {
            _assetManagerReady = SUCCEEDED(GAME->Initialize_AssetManager(resourceRoot));
            if (!_assetManagerReady)
                GameInstance::DestroyInstance();
        }
        logStartupStep("asset-manager");

        UI_EDITOR_DESC editorDesc {};
        editorDesc.hWnd = _hWnd;
        editorDesc.winMode = WinMode::Win;
        editorDesc.assetManagerReady = _assetManagerReady;

        if (FAILED(UI_EDITOR->Initialize_Editor(editorDesc, _device, _context)))
        {
            UI_Editor_Instance::DestroyInstance();
            GameInstance::DestroyInstance();
            _assetManagerReady = false;
            Cleanup_RenderTarget();
            _swapChain.Reset();
            _context.Reset();
            _device.Reset();
            return E_FAIL;
        }
        logStartupStep("editor-manager");

        _initialized = true;
        const double totalMilliseconds = chrono::duration<double, milli>(chrono::steady_clock::now() - startupBeginTime).count();
        LOG_INFO("UI Editor startup: total {:.2f} ms", totalMilliseconds);
        return S_OK;
    }

    void UI_EditorApp::Render_Frame()
    {
        if (!_initialized)
            return;

        UI_EDITOR->Update_Editor(0.f);

        constexpr float clearColor[4] = { 0.08f, 0.08f, 0.09f, 1.0f };
        _context->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), nullptr);
        _context->ClearRenderTargetView(_renderTargetView.Get(), clearColor);

        UI_EDITOR->Render_Editor();

        const ImGuiIO& io = ImGui::GetIO();
        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX = 0.f;
        viewport.TopLeftY = 0.f;
        viewport.Width = max(1.f, io.DisplaySize.x);
        viewport.Height = max(1.f, io.DisplaySize.y);
        viewport.MinDepth = 0.f;
        viewport.MaxDepth = 1.f;
        _context->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), nullptr);
        _context->RSSetViewports(1, &viewport);

        UI_EDITOR->Render_FontPreviews();

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

    bool UI_EditorApp::Request_Close()
    {
        if (!_initialized)
            return true;

        return UI_EDITOR->Request_Close();
    }

    void UI_EditorApp::Shutdown()
    {
        if (!_initialized)
            return;

        UI_Editor_Instance::DestroyInstance();

        GameInstance::DestroyInstance();
        _assetManagerReady = false;

        Cleanup_RenderTarget();
        _swapChain.Reset();
        _context.Reset();
        _device.Reset();
        _hWnd = nullptr;
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

    wstring UI_EditorApp::Resolve_ClientResourceRoot() const
    {
        vector<fs::path> startPaths;

        wchar_t modulePath[MAX_PATH] = {};
        if (0 != GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
            startPaths.push_back(fs::path(modulePath).parent_path());

        startPaths.push_back(fs::current_path());

        for (fs::path startPath : startPaths)
        {
            startPath = fs::absolute(startPath).lexically_normal();

            for (fs::path currentPath = startPath; !currentPath.empty(); currentPath = currentPath.parent_path())
            {
                const fs::path candidate = currentPath / L"Client" / L"Bin" / L"Resources";
                if (fs::exists(candidate) && fs::is_directory(candidate))
                    return candidate.lexically_normal().wstring();

                if (currentPath == currentPath.parent_path())
                    break;
            }
        }

        return {};
    }
}

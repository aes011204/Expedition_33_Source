#pragma once
#include "UI_Editor_Defines.h"

namespace UIEditor
{
    class UI_EditorApp final
    {
    public:
        UI_EditorApp() = default;
        ~UI_EditorApp();

        HRESULT Initialize(HWND hWnd);
        void Render_Frame();
        void Resize(uint32 width, uint32 height);
        bool Request_Close();
        void Shutdown();

    private:
        HRESULT Create_DeviceObjects(HWND hWnd);
        void Cleanup_RenderTarget();
        HRESULT Create_RenderTarget();
        wstring Resolve_ClientResourceRoot() const;

    private:
        HWND _hWnd = nullptr;
        ComPtr<Device> _device = nullptr;
        ComPtr<Context> _context = nullptr;
        ComPtr<SwapChain> _swapChain = nullptr;
        ComPtr<ID3D11RenderTargetView> _renderTargetView = nullptr;

        bool _assetManagerReady = false;
        bool _initialized = false;
    };
}

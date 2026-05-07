#pragma once
#include "UI_Editor_Manager.h"

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
        void Shutdown();

    private:
        HRESULT Create_DeviceObjects(HWND hWnd);
        void Cleanup_RenderTarget();
        HRESULT Create_RenderTarget();

        void Begin_ImGuiFrame();

    private:
        HWND _hWnd = nullptr;
        ComPtr<ID3D11Device> _device = nullptr;
        ComPtr<ID3D11DeviceContext> _context = nullptr;
        ComPtr<IDXGISwapChain> _swapChain = nullptr;
        ComPtr<ID3D11RenderTargetView> _renderTargetView = nullptr;
        Unique<UI_Editor_Manager> _editorManager = nullptr;

        bool _assetManagerReady = false;
        bool _initialized = false;
    };
}

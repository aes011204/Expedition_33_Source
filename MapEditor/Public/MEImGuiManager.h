#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(Engine)
class RenderTarget;
NS_END

NS_BEGIN(MapEditor)

class MEImGuiManager
{
public:
    MEImGuiManager() = default;
    ~MEImGuiManager();

public:
    ID3D11ShaderResourceView* Get_SharedViewportSRV() const;

public:
    HRESULT Initialize(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Update(float timeDelta);
    void Render();

private:
    ComPtr<Device>  _device;
    ComPtr<Context> _context;
    Shared<RenderTarget> _sharedViewportRenderTarget;

private:
    Shared<RenderTarget> Prepare_SharedViewportRenderTarget(uint32 width, uint32 height);
    void ApplyStyle();

public:
    void Capture_BackBuffer_ToViewport();

public:
    static Unique<MEImGuiManager> Create(HWND hWnd, const ComPtr<Device>& device, const ComPtr<Context>& context);
};

NS_END

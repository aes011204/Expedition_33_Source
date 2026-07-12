#pragma once

#include "Base.h"
#include <vector>

NS_BEGIN(Engine)

class ShaderCom;
class VIBuffer_Rect;

class ENGINE_DLL RenderTarget final : public Base
{
public:
    RenderTarget(const ComPtr<Device>& device, const ComPtr<Context>& context);
	~RenderTarget() override;

public:
    HRESULT Initialize(const wstring& targetTag, uint32 width, uint32 height, DXGI_FORMAT pixelFormat, const Vec4& clearColor, bool useDepthStencil = true, uint32 arraySize = 1);
    HRESULT Resize(uint32 width, uint32 height, DXGI_FORMAT pixelFormat, const Vec4& clearColor, bool useDepthStencil = true, uint32 arraySize = 1);
    HRESULT Begin(bool clearRenderTarget = true, bool clearDepthStencil = true, uint32 arraySlice = 0);
    HRESULT Bind_ShaderResource(ShaderCom* shader, const char* constantName);

    void Clear_RenderTarget(uint32 arraySlice = 0);
    void Clear_DepthStencil(uint32 arraySlice = 0);

    HRESULT Copy_Resource(ID3D11Texture2D* destTexture);

public:
    RenderTargetView* Get_RTV() const;
    RenderTargetView* Get_RTV(uint32 arraySlice) const;
    DepthStencilView* Get_DSV() const;
    DepthStencilView* Get_DSV(uint32 arraySlice) const;
    ShaderResourceView* Get_SRV() const { return _srv.Get(); }

    uint32 Get_Width() const { return _width; }
    uint32 Get_Height() const { return _height; }
    uint32 Get_ArraySize() const { return _arraySize; }

    ComPtr<ID3D11Texture2D> Get_Texture() const { return _texture; }

private:
    void Release_TargetResources();

private:
    wstring                     _targetTag = L"";
                                
    ComPtr<Device>              _device = nullptr;
    ComPtr<Context>             _context = nullptr;
    ComPtr<Texture2D>           _texture = nullptr;
    vector<ComPtr<RenderTargetView>> _rtvSlices{};
    ComPtr<ShaderResourceView>  _srv = nullptr;
    ComPtr<Texture2D>           _depthStencilTexture = nullptr;
    vector<ComPtr<DepthStencilView>> _dsvSlices{};

    uint32      _width = 0;
    uint32      _height = 0;
    uint32      _arraySize = 1;
    DXGI_FORMAT _pixelFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    Vec4        _clearColor = Vec4(0.f, 0.f, 0.f, 1.f);

public:
    static Shared<RenderTarget> Create(
        const wstring& targetTag,
        ComPtr<Device> device,
        ComPtr<Context> context,
        uint32 width,
        uint32 height,
        DXGI_FORMAT pixelFormat,
        const Vec4& clearColor,
        bool useDepthStencil = false,
        uint32 arraySize = 1);

    void Free() override;

#ifdef _DEBUG
public:
    HRESULT Setup_Debug(float x, float y, float sizeX, float sizeY);
    HRESULT Render_Debug(const Shared<VIBuffer_Rect>& viBuffer, const Shared<ShaderCom>& shader);
private:
    Matrix _debugWorldMatrix = Matrix::Identity;

#endif
};

NS_END

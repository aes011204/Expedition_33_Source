#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ShaderCom;
class VIBuffer_Rect;

class ENGINE_DLL RenderTarget final : public Base
{
public:
    RenderTarget(const ComPtr<Device>& device, const ComPtr<Context>& context);
	~RenderTarget() override;

public:
    HRESULT Initialize(const wstring& targetTag, uint32 width, uint32 height, DXGI_FORMAT pixelFormat, const Vec4& clearColor, bool useDepthStencil = true);
    HRESULT Resize(uint32 width, uint32 height, DXGI_FORMAT pixelFormat, const Vec4& clearColor, bool useDepthStencil = true);
    HRESULT Begin(bool clearRenderTarget = true, bool clearDepthStencil = true);
    HRESULT Bind_ShaderResource(ShaderCom* shader, const char* constantName);

    void Clear_RenderTarget();
    void Clear_DepthStencil();
    // 이후 Clear_RenderTarget 호출에서 사용할 clear color 값을 갱신한다.
    void Set_ClearColor(const Vec4& clearColor);

    HRESULT Copy_Resource(ID3D11Texture2D* destTexture);

public:
    RenderTargetView* Get_RTV() const { return _rtv.Get(); }
    DepthStencilView* Get_DSV() const { return _dsv.Get(); }
    ShaderResourceView* Get_SRV() const { return _srv.Get(); }

    uint32 Get_Width() const { return _width; }
    uint32 Get_Height() const { return _height; }

    ComPtr<ID3D11Texture2D> Get_Texture() const { return _texture; }

private:
    void Release_TargetResources();

private:
    wstring                     _targetTag = L"";
                                
    ComPtr<Device>              _device = nullptr;
    ComPtr<Context>             _context = nullptr;
    ComPtr<Texture2D>           _texture = nullptr;
    ComPtr<RenderTargetView>    _rtv = nullptr;
    ComPtr<ShaderResourceView>  _srv = nullptr;
    ComPtr<Texture2D>           _depthStencilTexture = nullptr;
    ComPtr<DepthStencilView>    _dsv = nullptr;

    uint32      _width = 0;
    uint32      _height = 0;
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
        bool useDepthStencil = false);

    void Free() override;

#ifdef _DEBUG
public:
    HRESULT Setup_Debug(float x, float y, float sizeX, float sizeY);
    HRESULT Render_Debug(Shared<VIBuffer_Rect> viBuffer, Shared<ShaderCom> shader);
private:
    Matrix _debugWorldMatrix = Matrix::Identity;

#endif
};

NS_END

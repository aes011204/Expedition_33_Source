#pragma once
#include "Base.h"

NS_BEGIN(Engine)

class RenderTarget;
class ShaderCom;
class VIBuffer_Rect;

class ENGINE_DLL Target_Manager final : public Base
{
private :
    // TODO 네이밍 규칙 물어보기
    struct TargetDesc
    {
        Shared<RenderTarget>    target;
        DXGI_FORMAT             format;
        Color                   clearColor;
        bool                    hasDepth = false;
        bool                    resizeWithViewport = true;
    };

public :
    explicit Target_Manager(ComPtr<Device> device, ComPtr<Context> context);
    virtual ~Target_Manager() = default;

public :
    HRESULT Add_RenderTarget(const wstring& targetTag, uint32 sizeX, uint32 sizeY,
        DXGI_FORMAT pixelFormat, const Color& clearColor, bool resizeWithViewport = true);
    HRESULT Add_MRT(const wstring& mrtTag, const wstring& targetTag);

    HRESULT Bind_ShaderResource(Shared<ShaderCom> shader, const char* constantName, const wstring& targetTag);
    HRESULT Begin_MRT(const wstring& mrtTag, ComPtr<DepthStencilView> shaderDSV = nullptr);
    HRESULT End_MRT();

    HRESULT Resize_MRTs(uint32 width, uint32 height);

    HRESULT Copy_CurrentRTV_To(const wstring& targetTag);
    HRESULT Copy_RTV(const wstring& srcTargetTag, const wstring& dstTargetTag);

private :
    Shared<RenderTarget>        Find_RenderTarget(const wstring& targetTag);
    list<Shared<RenderTarget>>* Find_MRT(const wstring& mrtTag);

    ComPtr<Device>                  _device;
    ComPtr<Context>                 _context;
    ComPtr<RenderTargetView>        _backBuffer;
    ComPtr<DepthStencilView>        _originalDSV;

    map<wstring, TargetDesc>                    _renderTargets;
    map<wstring, list<Shared<RenderTarget>>>    _MRTs;

    D3D11_VIEWPORT  _originalViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    uint32          _originalViewportCount = 0;
    uint32          _currentWidth = 0;
    uint32          _currentHeight = 0;

public :
    static Unique<Target_Manager>   Create(ComPtr<Device> device, ComPtr<Context> context);
    virtual void Free() override;

#ifdef _DEBUG
public :
    HRESULT Initialize_Debug(float sizeX, float sizeY);
    HRESULT SetUp_Debug(const wstring& targetTag, float x, float y, float sizeX, float sizeY, bool render = true);
    HRESULT Render_Debug(Shared<ShaderCom> shader, Shared<VIBuffer_Rect> viBuffer);
    HRESULT Render_Debug(const wstring& mrtTag, Shared<ShaderCom> shader, Shared<VIBuffer_Rect> viBuffer);

private :
    map<wstring, bool>    _drawMapMRTs;
#endif

};

NS_END

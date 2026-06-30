#pragma once
#include "Base.h"
#include <vector>

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
        uint32                  arraySize = 1;
    };

public :
    explicit Target_Manager(ComPtr<Device> device, ComPtr<Context> context);
    virtual ~Target_Manager() = default;

public :
    HRESULT Ready_AllRenderTargets();

    HRESULT Add_RenderTarget(const wstring& targetTag, uint32 sizeX, uint32 sizeY,
        DXGI_FORMAT pixelFormat, const Color& clearColor, bool resizeWithViewport = true, bool useDSV = false, uint32 arraySize = 1);
    HRESULT Add_MRT(const wstring& mrtTag, const wstring& targetTag);

    HRESULT Bind_ShaderResource(Shared<ShaderCom> shader, const char* constantName, const wstring& targetTag);
    HRESULT Begin_MRT(const wstring& mrtTag, bool useDSV, bool clearRTV, uint32 arraySlice = 0);
    HRESULT End_MRT();

    HRESULT Resize_MRTs(uint32 width, uint32 height);
    HRESULT Resize_ShadowRenderTargets(uint32 width, uint32 height);

    HRESULT Copy_CurrentRTV_To(const wstring& targetTag);
    HRESULT Copy_RTV(const wstring& srcTargetTag, const wstring& dstTargetTag);
    HRESULT Copy_RTV_ArraySlice(const wstring& srcTargetTag, uint32 arraySlice, const wstring& dstTargetTag);

    HRESULT Copy_Resource(const wstring& targetTag, ID3D11Texture2D* destTexture);
    ID3D11ShaderResourceView* Get_RT_SRV(const wstring& targetTag);

private :
    Shared<RenderTarget>        Find_RenderTarget(const wstring& targetTag);
    list<Shared<RenderTarget>>* Find_MRT(const wstring& mrtTag);
    bool                        Try_GetViewTexture(ID3D11View* view, D3D11_TEXTURE2D_DESC& outDesc);

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

    // TODO ! Shadow 관련 클래스 확장되기 전까지는 여기서 Renderer랑 동기화해줘야 한다.
    uint32                          _shadowMapWidth = 2048;
    uint32                          _shadowMapHeight = 2048;

public :
    static Unique<Target_Manager>   Create(ComPtr<Device> device, ComPtr<Context> context);
    virtual void Free() override;

#ifdef _DEBUG
    // 에디터 RT 디버그 렌더 
public :
    HRESULT Initialize_Debug(float sizeX, float sizeY);
    HRESULT Resize_DrawArea(float sizeX, float sizeY);
    HRESULT Display_RT(const wstring& targetTag, bool render);
    HRESULT Display_AllRTs(bool render);
    HRESULT Render_Debug(Shared<ShaderCom> shader, Shared<VIBuffer_Rect> viBuffer);
    const map<wstring, bool>&  Get_RTs() const;
    ComPtr<ID3D11Texture2D> Get_RT_Texture(const wstring& targetTag);

private :
    struct DebugDrawArea
    {
        float width = 0.f;
        float height = 0.f;
    };

    struct DebugLayoutCell
    {
        float x = 0.f;
        float y = 0.f;
        float size = 0.f;
    };

    HRESULT Reconstruct_DrawArea();
    HRESULT Apply_DebugLayout(const wstring& targetTag, const DebugLayoutCell& cell);

private :
    DebugDrawArea                   _debugDrawArea{};
    map<wstring, bool>              _debugRTMaps;
    vector<wstring>                 _debugVisibleRTs;
    map<wstring, DebugLayoutCell>   _debugLayoutByRT;
    bool                            _debugLayoutDirty = true;
#endif

};

NS_END

#include "Target_Manager.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Helper_String.h"
#include "Renderer_Define.h"
#include "GameInstance.h"

Target_Manager::Target_Manager(ComPtr<Device> device, ComPtr<ID3D11DeviceContext> context)
    : _device(device), _context(context)
{
}

HRESULT Target_Manager::Ready_AllRenderTargets()
{
    const uint32 width = GAME->Get_ViewportWidth();
    const uint32 height = GAME->Get_ViewportHeight();

    const bool resizeWithViewport = true;
    const bool useDSV = true;

    // MRT_GameObjects
    CHECK_FAILED(Add_RenderTarget(RT::Diffuse,
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::Normal,
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::Depth,
        width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::Material,
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::Emissive,
        width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);

    // SSAO
    CHECK_FAILED(Add_RenderTarget(RT::SSAO,
        width, height, DXGI_FORMAT_R16_FLOAT, Color(1.f, 1.f, 1.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::SSAOBlur,
        width, height, DXGI_FORMAT_R16_FLOAT, Color(1.f, 1.f, 1.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);

    // MRT_LightAcc
    CHECK_FAILED(Add_RenderTarget(RT::Shade,
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::Ambient,
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::Specular,
        width, height, DXGI_FORMAT_R16G16B16A16_UNORM, Color(0, 0, 0, 0), resizeWithViewport, !useDSV), E_FAIL);

    // MRT_ShadowObjects
    CHECK_FAILED(Add_RenderTarget(RT::LightDepth,
        _shadowMapWidth, _shadowMapHeight,
        DXGI_FORMAT_R32G32B32A32_FLOAT, Color(1.f, 1.f, 1.f, 1.f), !resizeWithViewport, useDSV), E_FAIL);

    CHECK_FAILED(Add_RenderTarget(RT::StaticLightDepth,
        _staticShadowMapWidth, _staticShadowMapHeight,
        DXGI_FORMAT_R32G32B32A32_FLOAT, Color(1.f, 1.f, 1.f, 1.f), !resizeWithViewport, useDSV), E_FAIL);


    // MRT_PostProcess
    const uint32 bloomHalfWidth = max(1u, width / 2u);
    const uint32 bloomHalfHeight = max(1u, height / 2u);
    const uint32 bloomQuarterWidth = max(1u, width / 4u);
    const uint32 bloomQuarterHeight = max(1u, height / 4u);
    const uint32 bloomEighthWidth = max(1u, width / 8u);
    const uint32 bloomEighthHeight = max(1u, height / 8u);

    CHECK_FAILED(Add_RenderTarget(RT::BloomHalfA,
        bloomHalfWidth, bloomHalfHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::BloomHalfB,
        bloomHalfWidth, bloomHalfHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::BloomQuarterA,
        bloomQuarterWidth, bloomQuarterHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::BloomQuarterB,
        bloomQuarterWidth, bloomQuarterHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::BloomEighthA,
        bloomEighthWidth, bloomEighthHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::BloomEighthB,
        bloomEighthWidth, bloomEighthHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);

    CHECK_FAILED(Add_RenderTarget(RT::HDRColor,
        width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::DistortionSource,
        width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::DistortionField,
        width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, Color(0.f, 0.f, 0.f, 0.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::BackgroundMask,
        width, height, DXGI_FORMAT_R8_UNORM, Color(0.f, 0.f, 0.f, 0.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::ScreenFxColorA,
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::ScreenFxColorB,
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);
    CHECK_FAILED(Add_RenderTarget(RT::ScreenCapture,
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM, Color(0.f, 0.f, 0.f, 1.f), resizeWithViewport, !useDSV), E_FAIL);

    //CHECK_FAILED(Ready_ShadowDSV(), E_FAIL);

    // Create MRT
    CHECK_FAILED(Add_MRT(MRT::GameObjects, RT::Diffuse), E_FAIL);		// SV_TARGET0
    CHECK_FAILED(Add_MRT(MRT::GameObjects, RT::Normal), E_FAIL);		// SV_TARGET1
    CHECK_FAILED(Add_MRT(MRT::GameObjects, RT::Depth), E_FAIL);		// SV_TARGET2
    CHECK_FAILED(Add_MRT(MRT::GameObjects, RT::Material), E_FAIL);	// SV_TARGET3
    CHECK_FAILED(Add_MRT(MRT::GameObjects, RT::Emissive), E_FAIL);	// SV_TARGET4

    // SSAO
    CHECK_FAILED(Add_MRT(MRT::SSAO, RT::SSAO), E_FAIL);
    CHECK_FAILED(Add_MRT(MRT::SSAOBlur, RT::SSAOBlur), E_FAIL);

    // Light
    CHECK_FAILED(Add_MRT(MRT::LightAcc, RT::Shade), E_FAIL);			// SV_TARGET0
    CHECK_FAILED(Add_MRT(MRT::LightAcc, RT::Ambient), E_FAIL);		// SV_TARGET1
    CHECK_FAILED(Add_MRT(MRT::LightAcc, RT::Specular), E_FAIL);		// SV_TARGET2

    CHECK_FAILED(Add_MRT(MRT::ShadowObjects, RT::LightDepth), E_FAIL);
    CHECK_FAILED(Add_MRT(MRT::StaticShadowObjects, RT::StaticLightDepth), E_FAIL);

    // PostProcess
    CHECK_FAILED(Add_MRT(MRT::HDR, RT::HDRColor), E_FAIL);
    CHECK_FAILED(Add_MRT(MRT::HDR, RT::BackgroundMask), E_FAIL);
    CHECK_FAILED(Add_MRT(MRT::DistortionField, RT::DistortionField), E_FAIL);
    CHECK_FAILED(Add_MRT(MRT::ScreenFxA, RT::ScreenFxColorA), E_FAIL);
    CHECK_FAILED(Add_MRT(MRT::ScreenFxB, RT::ScreenFxColorB), E_FAIL);

    CHECK_FAILED(Add_MRT(MRT::BloomHalfA, RT::BloomHalfA), E_FAIL);
    CHECK_FAILED(Add_MRT(MRT::BloomHalfB, RT::BloomHalfB), E_FAIL);
    CHECK_FAILED(Add_MRT(MRT::BloomQuarterA, RT::BloomQuarterA), E_FAIL);
    CHECK_FAILED(Add_MRT(MRT::BloomQuarterB, RT::BloomQuarterB), E_FAIL);
    CHECK_FAILED(Add_MRT(MRT::BloomEighthA, RT::BloomEighthA), E_FAIL);
    CHECK_FAILED(Add_MRT(MRT::BloomEighthB, RT::BloomEighthB), E_FAIL);

#ifdef _DEBUG
    // 전체 뷰포트의 1/4만 사용하여 디버그 렌더
    Initialize_Debug(width * 0.5f, height * 0.5f);
#endif

    return S_OK;
}

HRESULT Target_Manager::Add_RenderTarget(const wstring& targetTag, uint32 sizeX, uint32 sizeY, DXGI_FORMAT pixelFormat, const Color& clearColor, bool resizeWithViewport, bool useDSV)
{
    if (Find_RenderTarget(targetTag))
        return E_FAIL;

    auto target = RenderTarget::Create(targetTag, _device, _context, sizeX, sizeY, pixelFormat, clearColor, useDSV);

    if (target == nullptr)
        return E_FAIL;

    target->Clear_RenderTarget();
    target->Clear_DepthStencil();

    TargetDesc desc;
    desc.target = target;
    desc.format = pixelFormat;
    desc.clearColor = clearColor;
    desc.hasDepth = useDSV;
    desc.resizeWithViewport = resizeWithViewport; 

    _renderTargets.emplace(targetTag, desc);

    if (resizeWithViewport)
    {
        _currentWidth = sizeX;
        _currentHeight = sizeY;
    }

#ifdef _DEBUG
    _debugRTMaps.emplace(targetTag, false);
    _debugLayoutDirty = true;
#endif

    return S_OK;
}

HRESULT Target_Manager::Add_MRT(const wstring& mrtTag, const wstring& targetTag)
{
    auto renderTarget = Find_RenderTarget(targetTag);
    CHECK_NULL(renderTarget, E_FAIL);

    auto mrt = Find_MRT(mrtTag);

    if (!mrt)
    {
        list<Shared<RenderTarget>> newMRT;
        newMRT.push_back(renderTarget);

        _MRTs.emplace(mrtTag, move(newMRT));

        return S_OK;
    }

    mrt->push_back(renderTarget);

    return S_OK;
}

HRESULT Target_Manager::Bind_ShaderResource(Shared<ShaderCom> shader, const char* constantName, const wstring& targetTag)
{
    auto renderTarget = Find_RenderTarget(targetTag);
    CHECK_NULL(renderTarget, E_FAIL);

    return shader->Bind_SRV(constantName, renderTarget->Get_SRV());
}

HRESULT Target_Manager::Begin_MRT(const wstring& mrtTag, bool useDSV, bool clearRTV)
{
    auto mrt = Find_MRT(mrtTag);
    CHECK_NULL(mrt, E_FAIL);

    // 기존 RS에 설정된 뷰포트와 개수를 저장해둔다.
    _originalViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    _context->RSGetViewports(&_originalViewportCount, _originalViewports);
    _context->OMGetRenderTargets(1, _backBuffer.ReleaseAndGetAddressOf(), _originalDSV.ReleaseAndGetAddressOf());

    // SRV 슬롯 전부 비우기
    ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    _context->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
    _context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

    RenderTargetView*   renderTargets[8] = { nullptr };
    uint32              renderTargetCount = 0;
    D3D11_VIEWPORT      mrtViewport = {};
    bool                hasMRTViewport = false;

    DepthStencilView* dsv = nullptr;


    for (auto& rt : *mrt)
    {
        for (auto& desc : _renderTargets)
        {
            if (desc.second.target == rt)
            {
                if (useDSV)
                {
                    rt->Clear_DepthStencil();
                    dsv = rt->Get_DSV();
                }
                if (clearRTV)
                {
                    rt->Clear_RenderTarget();
                }
                break;
            }
        }

        if (!hasMRTViewport)
        {
            mrtViewport.TopLeftX = 0.f;
            mrtViewport.TopLeftY = 0.f;
            mrtViewport.Width = static_cast<float>(rt->Get_Width());
            mrtViewport.Height = static_cast<float>(rt->Get_Height());
            mrtViewport.MinDepth = 0.f;
            mrtViewport.MaxDepth = 1.f;
            hasMRTViewport = true;
        }

        if (renderTargetCount >= 8)
            return E_FAIL;

        renderTargets[renderTargetCount++] = rt->Get_RTV();
    }

    dsv = dsv ? dsv : _originalDSV.Get();
    // ^ fix to down

    //if (!useDSV)
    //{
    //    dsv = nullptr;
    //}
    //else if (!dsv && hasMRTViewport && _originalDSV)
    //{
    //    // RTV 크기와 DSV 크기가 같을 때만 fallback 사용
    //    // 크기가 다르면(bloom half/quarter 등) depth 불필요하므로 nullptr
    //    D3D11_TEXTURE2D_DESC dsvDesc{};
    //    if (Try_GetViewTexture(_originalDSV.Get(), dsvDesc) &&
    //        static_cast<uint32>(mrtViewport.Width) == dsvDesc.Width &&
    //        static_cast<uint32>(mrtViewport.Height) == dsvDesc.Height)
    //    {
    //        dsv = _originalDSV.Get();
    //    }
    //}
    //

    //if (!useDSV)
    //{
    //    dsv = nullptr;
    //}
    //else
    //{
    //    if (!dsv)
    //    {
    //        // Depth 정확도 보장을 위해 원본(backbuffer) DSV fallback을 막는다. 창이 리사이즈되면 backbuffer DSV 크기가 MRT 크기와 달라질 수 있음.
    //        LOG_ERROR("[Target_Manager::Begin_MRT] No DSV in MRT. mrtTag='{}'", Engine::String::ToString(mrtTag));
    //        return E_FAIL;
    //    }
    //}

    _context->OMSetRenderTargets(renderTargetCount, renderTargets, dsv);

    if (hasMRTViewport)
        _context->RSSetViewports(1, &mrtViewport);

    return S_OK;
}

HRESULT Target_Manager::End_MRT()
{
    RenderTargetView* backBuffer = _backBuffer.Get();

    _context->OMSetRenderTargets(1, &backBuffer, _originalDSV.Get());

    if (_originalViewportCount > 0)
    {
        _context->RSSetViewports(_originalViewportCount, _originalViewports);
        _originalViewportCount = 0;
    }

    _backBuffer.Reset();
    _originalDSV.Reset();

    return S_OK;
}

HRESULT Target_Manager::Resize_MRTs(uint32 width, uint32 height)
{
    if (width == 0 || height == 0)
        return S_OK;

    for (auto& rt : _renderTargets)
    {
        TargetDesc& desc = rt.second;

        if (!desc.resizeWithViewport)
            continue;

        if (FAILED(desc.target->Resize(width, height, desc.format, desc.clearColor, desc.hasDepth)))
            return E_FAIL;
    }

    _currentWidth = width;
    _currentHeight = height;

    return S_OK;
}

HRESULT Target_Manager::Copy_CurrentRTV_To(const wstring& targetTag)
{
    // 현재 OM에 바인딩된 RTV
    ComPtr<RenderTargetView> currentRTV;
    _context->OMGetRenderTargets(1, currentRTV.GetAddressOf(), nullptr);
    CHECK_NULL(currentRTV, E_FAIL);

    // current = src
    ComPtr<ID3D11Resource> srcResource;
    currentRTV->GetResource(srcResource.GetAddressOf());
    CHECK_NULL(srcResource, E_FAIL);

    ComPtr<Texture2D> srcTexture;
    CHECK_FAILED(srcResource.As(&srcTexture), E_FAIL);

    // dst
    auto dstTarget = Find_RenderTarget(targetTag);
    CHECK_NULL(dstTarget, E_FAIL);

    ComPtr<Texture2D> dstTexture = dstTarget->Get_Texture();
    CHECK_NULL(dstTexture, E_FAIL);

    if (srcTexture.Get() == dstTexture.Get())
        return S_OK;

    D3D11_TEXTURE2D_DESC srcDesc{};
    D3D11_TEXTURE2D_DESC dstDesc{};
    srcTexture->GetDesc(&srcDesc);
    dstTexture->GetDesc(&dstDesc);

    if (srcDesc.Width != dstDesc.Width
        || srcDesc.Height != dstDesc.Height
        || srcDesc.Format != dstDesc.Format)
    {
        return E_FAIL;
    }

    // src 를 dst 에 그대로 덮어쓴다
    _context->CopyResource(dstTexture.Get(), srcTexture.Get());

    return S_OK;
}

HRESULT Target_Manager::Copy_RTV(const wstring& srcTargetTag, const wstring& dstTargetTag)
{
    // src
    auto srcTarget = Find_RenderTarget(srcTargetTag);
    CHECK_NULL(srcTarget, E_FAIL);

    ComPtr<Texture2D> srcTexture = srcTarget->Get_Texture();
    CHECK_NULL(srcTexture, E_FAIL);

    // dst
    auto dstTarget = Find_RenderTarget(dstTargetTag);
    CHECK_NULL(dstTarget, E_FAIL);

    ComPtr<Texture2D> dstTexture = dstTarget->Get_Texture();
    CHECK_NULL(dstTexture, E_FAIL);

    if (srcTexture.Get() == dstTexture.Get())
        return S_OK;

    D3D11_TEXTURE2D_DESC srcDesc{};
    D3D11_TEXTURE2D_DESC dstDesc{};
    srcTexture->GetDesc(&srcDesc);
    dstTexture->GetDesc(&dstDesc);

    if (srcDesc.Width != dstDesc.Width
        || srcDesc.Height != dstDesc.Height
        || srcDesc.Format != dstDesc.Format)
    {
        return E_FAIL;
    }

    // src 를 dst 에 그대로 덮어쓴다
    _context->CopyResource(dstTexture.Get(), srcTexture.Get());

    return S_OK;
}

HRESULT Target_Manager::Copy_Resource(const wstring& targetTag, ID3D11Texture2D* destTexture)
{
    auto renderTarget = Find_RenderTarget(targetTag);
    if (renderTarget == nullptr)
        return E_FAIL;

    return renderTarget->Copy_Resource(destTexture);
}

Shared<RenderTarget> Target_Manager::Find_RenderTarget(const wstring& targetTag)
{
    auto iter = _renderTargets.find(targetTag);

    if (iter == _renderTargets.end())
        return nullptr;

    return iter->second.target;
}

list<Shared<RenderTarget>>* Target_Manager::Find_MRT(const wstring& mrtTag)
{
    auto iter = _MRTs.find(mrtTag);
    if (iter == _MRTs.end())
        return nullptr;

    return &iter->second;
}

bool Target_Manager::Try_GetViewTexture(ID3D11View* view, D3D11_TEXTURE2D_DESC& outDesc)
{
    if (!view)
        return false;

    ComPtr<ID3D11Resource> resource;
    view->GetResource(resource.GetAddressOf());
    if (!resource)
        return false;

    ComPtr<ID3D11Texture2D> tex2d;
    if (FAILED(resource.As(&tex2d)) || !tex2d)
        return false;

    tex2d->GetDesc(&outDesc);
    return true;
}

Unique<Target_Manager> Target_Manager::Create(ComPtr<Device> device, ComPtr<Context> context)
{
    return make_unique<Target_Manager>(device, context);
}

void Target_Manager::Free()
{
    Base::Free();
}

#ifdef _DEBUG
HRESULT Target_Manager::Initialize_Debug(float totalSizeX, float totalSizeY)
{
    return Resize_DrawArea(totalSizeX, totalSizeY);
}

HRESULT Target_Manager::Resize_DrawArea(float sizeX, float sizeY)
{
    // Debug MRTs이 그려질 전체 영역 조절
    _debugDrawArea.width = max(sizeX, 0.f);
    _debugDrawArea.height = max(sizeY, 0.f);
    _debugLayoutDirty = true;

    return Reconstruct_DrawArea();
}

HRESULT Target_Manager::Display_RT(const wstring& targetTag, bool render)
{
    auto iter = _debugRTMaps.find(targetTag);
    if (iter == _debugRTMaps.end())
        return E_FAIL;

    if (iter->second == render)
        return S_OK;

    iter->second = render;
    _debugLayoutDirty = true;

    return Reconstruct_DrawArea();
}

HRESULT Target_Manager::Display_AllRTs(bool render)
{
    bool changed = false;
    for (auto& [_, isVisible] : _debugRTMaps)
    {
        if (isVisible != render)
        {
            isVisible = render;
            changed = true;
        }
    }

    if (!changed)
        return S_OK;

    _debugLayoutDirty = true;
    return Reconstruct_DrawArea();
}

HRESULT Target_Manager::Reconstruct_DrawArea()
{
    _debugVisibleRTs.clear();
    _debugLayoutByRT.clear();

    if (_debugDrawArea.width <= 0.f || _debugDrawArea.height <= 0.f)
    {
        _debugLayoutDirty = false;
        return S_OK;
    }

    for (const auto& [targetTag, isVisible] : _debugRTMaps)
    {
        if (isVisible)
            _debugVisibleRTs.push_back(targetTag);
    }

    if (_debugVisibleRTs.empty())
    {
        _debugLayoutDirty = false;
        return S_OK;
    }

    // 레이아웃 재구성
    const uint32 rtCount = static_cast<uint32>(_debugVisibleRTs.size());
    const uint32 cols = max(1u, static_cast<uint32>(ceilf(sqrtf(static_cast<float>(rtCount)))));
    const uint32 rows = max(1u, static_cast<uint32>(ceilf(static_cast<float>(rtCount) / static_cast<float>(cols))));
    const float cellSize = min(_debugDrawArea.width / static_cast<float>(cols), _debugDrawArea.height / static_cast<float>(rows));

    for (uint32 index = 0; index < rtCount; ++index)
    {
        const uint32 row = index / cols;
        const uint32 col = index % cols;

        DebugLayoutCell cell{};
        cell.x = static_cast<float>(col) * cellSize;
        cell.y = static_cast<float>(row) * cellSize;
        cell.size = cellSize;

        const wstring& targetTag = _debugVisibleRTs[index];
        _debugLayoutByRT.emplace(targetTag, cell);
        CHECK_FAILED(Apply_DebugLayout(targetTag, cell), E_FAIL);
    }

    _debugLayoutDirty = false;

    return S_OK;
}

HRESULT Target_Manager::Apply_DebugLayout(const wstring& targetTag, const DebugLayoutCell& cell)
{
    auto renderTarget = Find_RenderTarget(targetTag);
    CHECK_NULL(renderTarget, E_FAIL);

    CHECK_FAILED(renderTarget->Setup_Debug(cell.x, cell.y, cell.size, cell.size), E_FAIL);

    return S_OK;
}

HRESULT Target_Manager::Render_Debug(Shared<ShaderCom> shader, Shared<VIBuffer_Rect> viBuffer)
{
    if (_debugLayoutDirty)
        CHECK_FAILED(Reconstruct_DrawArea(), E_FAIL);

    for (const auto& targetTag : _debugVisibleRTs)
    {
        auto renderTarget = Find_RenderTarget(targetTag);
        CHECK_NULL(renderTarget, E_FAIL);

        CHECK_FAILED(renderTarget->Render_Debug(viBuffer, shader), E_FAIL);
    }

    return S_OK;
}

const map<wstring, bool>& Target_Manager::Get_RTs() const
{
    return _debugRTMaps;
}

ComPtr<ID3D11Texture2D> Target_Manager::Get_RT_Texture(const wstring& targetTag)
{
    auto rt = Find_RenderTarget(targetTag);
    if (!rt)
    {
        return nullptr;
    }
    return rt->Get_Texture();
}

#endif

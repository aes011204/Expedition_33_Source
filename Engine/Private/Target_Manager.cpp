#include "Target_Manager.h"
#include "RenderTarget.h"
#include "Shader.h"

Target_Manager::Target_Manager(ComPtr<Device> device, ComPtr<ID3D11DeviceContext> context)
    : _device(device), _context(context)
{
}

HRESULT Target_Manager::Add_RenderTarget(const wstring& targetTag, uint32 sizeX, uint32 sizeY, DXGI_FORMAT pixelFormat, const Color& clearColor, bool resizeWithViewport)
{
    if (Find_RenderTarget(targetTag))
        return E_FAIL;

    auto target = make_shared<RenderTarget>(_device, _context);

    if (FAILED(target->Initialize(sizeX, sizeY, pixelFormat, clearColor, false)))
        return E_FAIL;

    target->Clear_RenderTarget();
    target->Clear_DepthStencil();

    TargetDesc desc;
    desc.target = target;
    desc.format = pixelFormat;
    desc.clearColor = clearColor;
    desc.hasDepth = false;              // TODO false일 거면 왜 있는 건지? 어디에 사용됨? 
    desc.resizeWithViewport = resizeWithViewport; 

    _renderTargets.emplace(targetTag, desc);

    if (resizeWithViewport)
    {
        _currentWidth = sizeX;
        _currentHeight = sizeY;
    }

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

HRESULT Target_Manager::Bind_ShaderResource(Shared<Shader> shader, const char* constantName, const wstring& targetTag)
{
    auto renderTarget = Find_RenderTarget(targetTag);
    CHECK_NULL(renderTarget, E_FAIL);

    return shader->Bind_SRV(constantName, renderTarget->Get_SRV());
}

HRESULT Target_Manager::Begin_MRT(const wstring& mrtTag, ComPtr<DepthStencilView> shaderDSV)
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

    for (auto& rt : *mrt)
    {
        for (auto& desc : _renderTargets)
        {
            if (desc.second.target == rt)
            {
                rt->Clear_RenderTarget();
                // TODO 이 위치에서 DSV 초기화가 필요한가? 
                // rt->Clear_DepthStencil();
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

    DepthStencilView* dsv = shaderDSV.Get() ? shaderDSV.Get() : _originalDSV.Get();
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

#ifdef _DEBUG
HRESULT Target_Manager::Ready_Debug(const wstring& targetTag, float x, float y, float sizeX, float sizeY)
{
    auto renderTarget = Find_RenderTarget(targetTag);
    CHECK_NULL(renderTarget, E_FAIL);
    // TODO 필요 시
    //return renderTarget->Ready_Debug(x, y, sizeX, sizeY);
}

HRESULT Target_Manager::Render_Debug(const wstring& mrtTag, Shared<Shader> shader, Shared<VIBuffer_Rect> viBuffer)
{
    auto mrtList = Find_MRT(mrtTag);
    CHECK_NULL(mrtList, E_FAIL);

    for (auto renderTarget : *mrtList)
    {
        // TODO 필요 시

        //renderTarget->Render(viBuffer, shader);
    }

    return S_OK;
}

#endif

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

Unique<Target_Manager> Target_Manager::Create(ComPtr<Device> device, ComPtr<Context> context)
{
    return make_unique<Target_Manager>(device, context);
}

void Target_Manager::Free()
{
    Base::Free();
}

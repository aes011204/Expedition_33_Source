#include "pch.h"
#include "RenderTarget.h"
#include "GameInstance.h"
#include "VIBuffer_Rect.h"
#include "ShaderCom.h"

RenderTarget::RenderTarget(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}
RenderTarget::~RenderTarget()
{
}

HRESULT RenderTarget::Initialize(const wstring& targetTag, uint32 width, uint32 height, DXGI_FORMAT pixelFormat, const Vec4& clearColor, bool useDepthStencil, uint32 arraySize)
{
    if (nullptr == _device || nullptr == _context || 0 == width || 0 == height || arraySize == 0)
        return E_FAIL;

    Release_TargetResources();

    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = arraySize;
    textureDesc.Format = pixelFormat;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    if (FAILED(_device->CreateTexture2D(&textureDesc, nullptr, _texture.GetAddressOf())))
        return E_FAIL;

    _rtvSlices.clear();
    _rtvSlices.resize(arraySize);

    for (uint32 arraySlice = 0; arraySlice < arraySize; ++arraySlice)
    {
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = pixelFormat;
        rtvDesc.ViewDimension = (arraySize > 1) ? D3D11_RTV_DIMENSION_TEXTURE2DARRAY : D3D11_RTV_DIMENSION_TEXTURE2D;
        if (arraySize > 1)
        {
            rtvDesc.Texture2DArray.MipSlice = 0;
            rtvDesc.Texture2DArray.FirstArraySlice = arraySlice;
            rtvDesc.Texture2DArray.ArraySize = 1;
        }
        else
        {
            rtvDesc.Texture2D.MipSlice = 0;
        }

        if (FAILED(_device->CreateRenderTargetView(_texture.Get(), &rtvDesc, _rtvSlices[arraySlice].GetAddressOf())))
            return E_FAIL;
    }

    _targetTag = targetTag;

    for (uint32 arraySlice = 0; arraySlice < arraySize; ++arraySlice)
    {
        _rtvSlices[arraySlice]->SetPrivateData(
            WKPDID_D3DDebugObjectNameW,
            static_cast<UINT>((targetTag.size() + 1) * sizeof(wchar_t)),
            targetTag.c_str());
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = pixelFormat;
    srvDesc.ViewDimension = (arraySize > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DARRAY : D3D11_SRV_DIMENSION_TEXTURE2D;
    if (arraySize > 1)
    {
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.MipLevels = 1;
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize = arraySize;
    }
    else
    {
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
    }

    if (FAILED(_device->CreateShaderResourceView(_texture.Get(), &srvDesc, _srv.GetAddressOf())))
        return E_FAIL;

    if (useDepthStencil)
    {
        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = arraySize;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depthDesc.CPUAccessFlags = 0;
        depthDesc.MiscFlags = 0;

        if (FAILED(_device->CreateTexture2D(&depthDesc, nullptr, _depthStencilTexture.GetAddressOf())))
            return E_FAIL;

        _dsvSlices.clear();
        _dsvSlices.resize(arraySize);

        for (uint32 arraySlice = 0; arraySlice < arraySize; ++arraySlice)
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
            dsvDesc.Format = depthDesc.Format;
            dsvDesc.ViewDimension = (arraySize > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DARRAY : D3D11_DSV_DIMENSION_TEXTURE2D;
            if (arraySize > 1)
            {
                dsvDesc.Texture2DArray.MipSlice = 0;
                dsvDesc.Texture2DArray.FirstArraySlice = arraySlice;
                dsvDesc.Texture2DArray.ArraySize = 1;
            }
            else
            {
                dsvDesc.Texture2D.MipSlice = 0;
            }

            if (FAILED(_device->CreateDepthStencilView(_depthStencilTexture.Get(), &dsvDesc, _dsvSlices[arraySlice].GetAddressOf())))
                return E_FAIL;
        }
    }

    _width = width;
    _height = height;
    _arraySize = arraySize;
    _pixelFormat = pixelFormat;
    _clearColor = clearColor;

    return S_OK;
}

HRESULT RenderTarget::Resize(uint32 width, uint32 height, DXGI_FORMAT pixelFormat, const Vec4& clearColor, bool useDepthStencil, uint32 arraySize)
{
    if (_width == width && _height == height && _arraySize == arraySize && _pixelFormat == pixelFormat && !_rtvSlices.empty() && nullptr != _srv)
    {
        _clearColor = clearColor;
        return S_OK;
    }

    return Initialize(_targetTag, width, height, pixelFormat, clearColor, useDepthStencil, arraySize);
}

HRESULT RenderTarget::Begin(bool clearRenderTarget, bool clearDepthStencil, uint32 arraySlice)
{
    RenderTargetView* rtv = Get_RTV(arraySlice);
    DepthStencilView* dsv = Get_DSV(arraySlice);
    if (nullptr == _context || nullptr == rtv)
        return E_FAIL;

    ID3D11RenderTargetView* rtvs[] = { rtv };
    _context->OMSetRenderTargets(1, rtvs, dsv);

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.f;
    viewport.TopLeftY = 0.f;
    viewport.Width = static_cast<float>(_width);
    viewport.Height = static_cast<float>(_height);
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    _context->RSSetViewports(1, &viewport);

    if (clearRenderTarget)
        Clear_RenderTarget(arraySlice);

    if (clearDepthStencil)
        Clear_DepthStencil(arraySlice);

    return S_OK;
}

HRESULT RenderTarget::Bind_ShaderResource(ShaderCom* shader, const char* constantName)
{
    if (nullptr == shader || nullptr == _srv)
        return E_FAIL;

    return shader->Bind_SRV(constantName, _srv.Get());
}

void RenderTarget::Clear_RenderTarget(uint32 arraySlice)
{
    RenderTargetView* rtv = Get_RTV(arraySlice);
    if (nullptr != _context && nullptr != rtv)
        _context->ClearRenderTargetView(rtv, reinterpret_cast<const float*>(&_clearColor));
}

void RenderTarget::Clear_DepthStencil(uint32 arraySlice)
{
    DepthStencilView* dsv = Get_DSV(arraySlice);
    if (nullptr != _context && nullptr != dsv)
        _context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
}

HRESULT RenderTarget::Copy_Resource(ID3D11Texture2D* destTexture)
{
    _context->CopyResource(destTexture, _texture.Get());

    return S_OK;
}

void RenderTarget::Release_TargetResources()
{
    _dsvSlices.clear();
    _depthStencilTexture.Reset();
    _srv.Reset();
    _rtvSlices.clear();
    _texture.Reset();
    _width = 0;
    _height = 0;
    _arraySize = 1;
}

Shared<RenderTarget> RenderTarget::Create(
    const wstring& targetTag,
    ComPtr<Device> device,
    ComPtr<Context> context,
    uint32 width,
    uint32 height,
    DXGI_FORMAT pixelFormat,
    const Vec4& clearColor,
    bool useDepthStencil,
    uint32 arraySize)
{
    auto instance = make_shared<RenderTarget>(device, context);

    if (FAILED(instance->Initialize(targetTag, width, height, pixelFormat, clearColor, useDepthStencil, arraySize)))
    {
        MSG_BOX("Failed to Created : RenderTarget");
        return nullptr;
    }

    return instance;
}

void RenderTarget::Free()
{
    __super::Free();

    Release_TargetResources();
    _context.Reset();
    _device.Reset();
}

#ifdef _DEBUG

HRESULT RenderTarget::Setup_Debug(float x, float y, float sizeX, float sizeY)
{
    const float viewportWidth = static_cast<float>(GAME->Get_ViewportWidth());
    const float viewportHeight = static_cast<float>(GAME->Get_ViewportHeight());
    const float centerX = x + sizeX * 0.5f - viewportWidth * 0.5f;
    const float centerY = viewportHeight * 0.5f - (y + sizeY * 0.5f);

    _debugWorldMatrix = Matrix::Identity;
    _debugWorldMatrix._11 = sizeX;
    _debugWorldMatrix._22 = sizeY;
    _debugWorldMatrix._41 = centerX;
    _debugWorldMatrix._42 = centerY;

    return S_OK;
}

HRESULT RenderTarget::Render_Debug(Shared<VIBuffer_Rect> viBuffer, Shared<ShaderCom> shader)
{
    CHECK_NULL(viBuffer, E_FAIL);
    CHECK_NULL(shader, E_FAIL);

    shader->Bind_ObjectCB(_debugWorldMatrix);
    shader->Bind_SRV("g_Texture", _srv.Get());
    shader->Begin(0);

    viBuffer->Bind_Resources();
    viBuffer->Render();

    return S_OK;
}

#endif

RenderTargetView* RenderTarget::Get_RTV() const
{
    if (_rtvSlices.empty())
        return nullptr;

    return _rtvSlices[0].Get();
}

RenderTargetView* RenderTarget::Get_RTV(uint32 arraySlice) const
{
    if (_rtvSlices.empty())
        return nullptr;

    if (arraySlice >= _rtvSlices.size())
        return nullptr;

    return _rtvSlices[arraySlice].Get();
}

DepthStencilView* RenderTarget::Get_DSV() const
{
    if (_dsvSlices.empty())
        return nullptr;

    return _dsvSlices[0].Get();
}

DepthStencilView* RenderTarget::Get_DSV(uint32 arraySlice) const
{
    if (_dsvSlices.empty())
        return nullptr;

    if (arraySlice >= _dsvSlices.size())
        return nullptr;

    return _dsvSlices[arraySlice].Get();
}

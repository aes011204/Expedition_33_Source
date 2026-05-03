#include "pch.h"
#include "RenderTarget.h"
#include "VIBuffer_Rect.h"
#include "ShaderCom.h"

RenderTarget::RenderTarget(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT RenderTarget::Initialize(uint32 width, uint32 height, DXGI_FORMAT pixelFormat, const Vec4& clearColor, bool useDepthStencil)
{
    if (nullptr == _device || nullptr == _context || 0 == width || 0 == height)
        return E_FAIL;

    Release_TargetResources();

    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = pixelFormat;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    if (FAILED(_device->CreateTexture2D(&textureDesc, nullptr, _texture.GetAddressOf())))
        return E_FAIL;

    if (FAILED(_device->CreateRenderTargetView(_texture.Get(), nullptr, _rtv.GetAddressOf())))
        return E_FAIL;

    if (FAILED(_device->CreateShaderResourceView(_texture.Get(), nullptr, _srv.GetAddressOf())))
        return E_FAIL;

    if (useDepthStencil)
    {
        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depthDesc.CPUAccessFlags = 0;
        depthDesc.MiscFlags = 0;

        if (FAILED(_device->CreateTexture2D(&depthDesc, nullptr, _depthStencilTexture.GetAddressOf())))
            return E_FAIL;

        if (FAILED(_device->CreateDepthStencilView(_depthStencilTexture.Get(), nullptr, _dsv.GetAddressOf())))
            return E_FAIL;
    }

    _width = width;
    _height = height;
    _pixelFormat = pixelFormat;
    _clearColor = clearColor;

    return S_OK;
}

HRESULT RenderTarget::Resize(uint32 width, uint32 height, DXGI_FORMAT pixelFormat, const Vec4& clearColor, bool useDepthStencil)
{
    if (_width == width && _height == height && _pixelFormat == pixelFormat && nullptr != _rtv && nullptr != _srv)
    {
        _clearColor = clearColor;
        return S_OK;
    }

    return Initialize(width, height, pixelFormat, clearColor, useDepthStencil);
}

HRESULT RenderTarget::Begin(bool clearRenderTarget, bool clearDepthStencil)
{
    if (nullptr == _context || nullptr == _rtv)
        return E_FAIL;

    ID3D11RenderTargetView* rtvs[] = { _rtv.Get() };
    _context->OMSetRenderTargets(1, rtvs, _dsv.Get());

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.f;
    viewport.TopLeftY = 0.f;
    viewport.Width = static_cast<float>(_width);
    viewport.Height = static_cast<float>(_height);
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    _context->RSSetViewports(1, &viewport);

    if (clearRenderTarget)
        Clear_RenderTarget();

    if (clearDepthStencil)
        Clear_DepthStencil();

    return S_OK;
}

HRESULT RenderTarget::Bind_ShaderResource(ShaderCom* shader, const char* constantName)
{
    if (nullptr == shader || nullptr == _srv)
        return E_FAIL;

    return shader->Bind_SRV(constantName, _srv.Get());
}

void RenderTarget::Clear_RenderTarget()
{
    if (nullptr != _context && nullptr != _rtv)
        _context->ClearRenderTargetView(_rtv.Get(), reinterpret_cast<const float*>(&_clearColor));
}

void RenderTarget::Clear_DepthStencil()
{
    if (nullptr != _context && nullptr != _dsv)
        _context->ClearDepthStencilView(_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
}

void RenderTarget::Release_TargetResources()
{
    _dsv.Reset();
    _depthStencilTexture.Reset();
    _srv.Reset();
    _rtv.Reset();
    _texture.Reset();
    _width = 0;
    _height = 0;
}

Shared<RenderTarget> RenderTarget::Create(
    ComPtr<Device> device,
    ComPtr<Context> context,
    uint32 width,
    uint32 height,
    DXGI_FORMAT pixelFormat,
    const Vec4& clearColor,
    bool useDepthStencil)
{
    auto instance = make_shared<RenderTarget>(device, context);

    if (FAILED(instance->Initialize(width, height, pixelFormat, clearColor, useDepthStencil)))
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
    _debugWorldMatrix = Matrix::Identity;
    _debugWorldMatrix._11 = sizeX;
    _debugWorldMatrix._22 = sizeY;
    _debugWorldMatrix._41 = x;
    _debugWorldMatrix._42 = y;

    return S_OK;
}

HRESULT RenderTarget::Render_Debug(Shared<VIBuffer_Rect> viBuffer, Shared<ShaderCom> shader)
{
    CHECK_NULL(viBuffer, E_FAIL);
    CHECK_NULL(shader, E_FAIL);

    shader->Bind_Matrix("g_WorldMatrix", &_debugWorldMatrix);
    shader->Bind_SRV("g_Texture", _srv.Get());
    shader->Begin(0);

    viBuffer->Bind_Resources();
    viBuffer->Render();

    return S_OK;
}

#endif

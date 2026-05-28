#include "MELandscapeActor.h"
#include "GameInstance.h"

MELandscapeActor::MELandscapeActor(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

HRESULT MELandscapeActor::Initialize(const VIBuffer_Landscape::LAND_DESC& desc)
{
    if (FAILED(__super::Initialize(nullptr)))
    {
        return E_FAIL;
    }

    if (FAILED(Add_Component(0, L"Shader_VtxNorTex", _shader)))
    {
        return E_FAIL;
    }

    _vibuffer = Engine::VIBuffer_Landscape::Create(_device, _context, desc);
    if (_vibuffer == nullptr)
    {
        return E_FAIL;
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = texDesc.Height = texDesc.MipLevels = texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    uint32 whitePixel = 0xFFFFFFFF;
    D3D11_SUBRESOURCE_DATA initData = { &whitePixel, 4, 0 };
    ComPtr<ID3D11Texture2D> tex;
    _device->CreateTexture2D(&texDesc, &initData, tex.GetAddressOf());
    _device->CreateShaderResourceView(tex.Get(), nullptr, _whiteSRV.GetAddressOf());

    return S_OK;
}

void MELandscapeActor::Late_Update(float timeDelta)
{
    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT MELandscapeActor::Render()
{
    if (FAILED(Bind_ShaderResources()))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Begin(0)))
    {
        return E_FAIL;
    }

    if (FAILED(_vibuffer->Bind_Resources()))
    {
        return E_FAIL;
    }

    if (FAILED(_vibuffer->Render()))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT MELandscapeActor::Bind_ShaderResources()
{
    if (FAILED(_transformCom->Bind_ShaderResource(_shader.get(), "g_WorldMatrix")))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Bind_RawValue("g_Far", &_cameraFar, sizeof(float))))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Bind_SRV("g_DiffuseTexture", _whiteSRV.Get())))
    {
        return E_FAIL;
    }

    return S_OK;
}

Shared<MELandscapeActor> MELandscapeActor::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const VIBuffer_Landscape::LAND_DESC& desc)
{
    auto instance = make_shared<MELandscapeActor>(device, context);
    if (FAILED(instance->Initialize(desc)))
    {
        return nullptr;
    }

    return instance;
}

void MELandscapeActor::Free()
{
    GameObject::Free();
}

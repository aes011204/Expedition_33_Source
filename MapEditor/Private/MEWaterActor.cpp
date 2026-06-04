#include "MEWaterActor.h"
#include "GameInstance.h"
#include "Renderer_Define.h"

MEWaterActor::MEWaterActor(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

HRESULT MEWaterActor::Initialize(const VIBuffer_Plane::DESC& desc)
{
    if (FAILED(__super::Initialize(nullptr)))
    {
        return E_FAIL;
    }

    if (FAILED(Add_Component(0, L"Shader_Water", _shader)))
    {
        return E_FAIL;
    }

    _vibuffer = VIBuffer_Plane::Create(_device, _context, desc);
    if (!_vibuffer)
    {
        return E_FAIL;
    }

    return S_OK;
}

void MEWaterActor::Update(float timeDelta)
{
    _waterCB.elapsedTime += timeDelta;
}

void MEWaterActor::Late_Update(float timeDelta)
{
    GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT MEWaterActor::Render()
{
    if (!_shader || !_vibuffer)
    {
        return S_OK;
    }

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

    return _vibuffer->Render();
}

HRESULT MEWaterActor::Bind_ShaderResources()
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

    if (FAILED(_shader->Bind_RawValue("g_CamPosition", &_cameraPos, sizeof(Vec4))))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Bind_CBufferData(_waterCB)))
    {
        return E_FAIL;
    }

    if (_largeWaveNormalSRV)
    {
        _shader->Bind_SRV("g_LargeWaveNormal", _largeWaveNormalSRV.Get());
    }

    if (_smallWaveNormalSRV)
    {
        _shader->Bind_SRV("g_SmallWaveNormal", _smallWaveNormalSRV.Get());
    }

    if (_seafoamSRV)
    {
        _shader->Bind_SRV("g_SeafoamTexture", _seafoamSRV.Get());
    }

    if (FAILED(GAME->Bind_RT_ShaderResource(_shader, "g_DepthTexture", RT::Depth)))
    {
        return E_FAIL;
    }

    if (_cubemapSRV)
    {
        _shader->Bind_SRV("g_ReflectionCubemap", _cubemapSRV.Get());
    }

    return S_OK;
}

Shared<MEWaterActor> MEWaterActor::Create(ComPtr<Device> device, ComPtr<Context> context, const VIBuffer_Plane::DESC& desc)
{
    auto instance = make_shared<MEWaterActor>(device, context);
    if (FAILED(instance->Initialize(desc)))
    {
        return nullptr;
    }

    return instance;
}

void MEWaterActor::Free()
{
    GameObject::Free();
}

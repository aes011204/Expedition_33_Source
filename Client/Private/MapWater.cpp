#include "MapWater.h"
#include "GameInstance.h"
#include "Renderer_Define.h"

IMPLEMENT_REFLECTION(MapWater)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "MapWater";
    info.category = "World";
    return true;
}

MapWater::MapWater(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

MapWater::MapWater(const MapWater& prototype)
    : GameObject{ prototype }
{
}

HRESULT MapWater::Initialize_Prototype()
{
    return S_OK;
}

HRESULT MapWater::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(nullptr)))
    {
        return E_FAIL;
    }

    if (FAILED(Add_Component(0, L"Shader_Water", _shader)))
    {
        return E_FAIL;
    }

    auto* desc = static_cast<MAPWATER_DESC*>(arg);
    if (!desc)
    {
        return E_FAIL;
    }

    _transformCom->Set_Position(desc->position);
    _transformCom->Set_RotationQuaternion(desc->rotation);
    _transformCom->Set_Scale(desc->scale);

    _vibuffer = VIBuffer_Plane::Create(_device, _context, desc->planeDesc);
    if (!_vibuffer)
    {
        return E_FAIL;
    }

    _waterCB = desc->waterParams;

    if (!desc->largeWaveNormalPath.empty())
    {
        _largeWaveNormalSRV = Load_SRV(desc->largeWaveNormalPath);
    }

    if (!desc->smallWaveNormalPath.empty())
    {
        _smallWaveNormalSRV = Load_SRV(desc->smallWaveNormalPath);
    }

    if (!desc->seafoamPath.empty())
    {
        _seafoamSRV = Load_SRV(desc->seafoamPath);
    }

    if (!desc->cubemapPath.empty())
    {
        _cubemapSRV = Load_SRV(desc->cubemapPath);
    }

    return S_OK;
}

void MapWater::Update(float timeDelta)
{
    _waterCB.elapsedTime += timeDelta;
}

void MapWater::Late_Update(float timeDelta)
{
    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT MapWater::Render()
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

HRESULT MapWater::Bind_ShaderResources()
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

    if (FAILED(GAME->Bind_CameraFar(_shader, "g_Far")))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_CamPosition(_shader.get(), "g_CamPosition")))
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

ComPtr<ID3D11ShaderResourceView> MapWater::Load_SRV(const wstring& path)
{
    WCHAR ext[MAX_PATH] = {};
    _wsplitpath_s(path.c_str(), nullptr, 0, nullptr, 0, nullptr, 0, ext, MAX_PATH);

    ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = (lstrcmp(ext, L".dds") == 0)
        ? CreateDDSTextureFromFile(_device.Get(), path.c_str(), nullptr, srv.GetAddressOf())
        : CreateWICTextureFromFile(_device.Get(), path.c_str(), nullptr, srv.GetAddressOf());

    if (FAILED(hr))
    {
        return nullptr;
    }

    return srv;
}

Shared<MapWater> MapWater::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MapWater>(device, context);
    if (FAILED(instance->Initialize_Prototype()))
    {
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MapWater::Clone(void* arg)
{
    auto instance = make_shared<MapWater>(*this);
    if (FAILED(instance->Initialize(arg)))
    {
        return nullptr;
    }

    return instance;
}

void MapWater::Free()
{
    GameObject::Free();
}

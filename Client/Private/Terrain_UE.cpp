#include "Terrain_UE.h"
#include "GameInstance.h"
#include "ShaderCom.h"

IMPLEMENT_REFLECTION(Terrain_UE)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "Terrain_UE";
    info.category = "World";
    return true;
}

Terrain_UE::Terrain_UE(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

Terrain_UE::Terrain_UE(const Terrain_UE& prototype)
    : GameObject{ prototype }
{
}

HRESULT Terrain_UE::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Terrain_UE::Initialize(void* arg)
{
    if (!arg)
    {
        return E_FAIL;
    }

    if (FAILED(__super::Initialize(arg)))
    {
        return E_FAIL;
    }

    const TERRAIN_UE_DESC& desc = *static_cast<TERRAIN_UE_DESC*>(arg);

    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxNorTex"), _shader)))
    {
        return E_FAIL;
    }

    _vibuffer = VIBuffer_TerrainUE::Create(_device, _context, desc.terrainDesc);
    if (!_vibuffer)
    {
        return E_FAIL;
    }

    return S_OK;
}

void Terrain_UE::Priority_Update(float timeDelta)
{
}

void Terrain_UE::Update(float timeDelta)
{
}

void Terrain_UE::Late_Update(float timeDelta)
{
    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT Terrain_UE::Render()
{
    if (FAILED(Bind_ShaderResources()))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Begin(0)))
    {
        return E_FAIL;
    }

    if (FAILED(_vibuffer->Render()))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT Terrain_UE::Bind_ShaderResources()
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

    return S_OK;
}

Shared<Terrain_UE> Terrain_UE::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Terrain_UE>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Terrain_UE");
        return nullptr;
    }
    return instance;
}

Shared<GameObject> Terrain_UE::Clone(void* pArg)
{
    auto instance = make_shared<Terrain_UE>(*this);

    if (FAILED(instance->Initialize(pArg)))
    {
        MSG_BOX("Failed to Cloned : Terrain_UE");
        return nullptr;
    }
    return instance;
}

void Terrain_UE::Free()
{
    __super::Free();
}

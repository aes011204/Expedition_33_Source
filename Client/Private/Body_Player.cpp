#include "Body_Player.h"
#include "GameInstance.h"
#include "Material.h"
#include "Material_Types.h"
#include "Player.h"
#include "ModelCom.h"

Body_Player::Body_Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : PlayableParts{ device, context }
{
}

Body_Player::Body_Player(const Body_Player& prototype)
    : PlayableParts{ prototype }
{
}

HRESULT Body_Player::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Body_Player::Initialize(void* arg)
{
    if (FAILED(PlayableParts::Initialize(arg)))
    {
        MSG_BOX("[Body_Player] : Failed to Initialize");
        return E_FAIL;
    }

    _rootBoneIndex = _modelCom->Get_BoneIndex("root");
    _modelCom->Set_LocalRootNode(_rootBoneIndex);

    return S_OK;
}

void Body_Player::Priority_Update(float fTimeDelta)
{
}

void Body_Player::Update(float fTimeDelta)
{
    _modelCom->Play_Animation(fTimeDelta);

    Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
}

void Body_Player::Late_Update(float fTimeDelta)
{
    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
    GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT Body_Player::Render()
{
    return PlayableParts::Render();
}

HRESULT Body_Player::Render_Shadow()
{
    if (FAILED(_shaderCom->Bind_Matrix("g_WorldMatrix", &_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowMatrices(_shaderCom, "g_ViewMatrix", "g_ProjMatrix")))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowLightDesc(_shaderCom, "g_fLightFar")))
        return E_FAIL;

    const uint32 iNumMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());

    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        _modelCom->Bind_BoneMatrices(_shaderCom.get(), "g_BoneMatrices", i);

        if (FAILED(_shaderCom->Begin(1)))
            return E_FAIL;

        _modelCom->Render(i);
    }

    return S_OK;
}

Shared<Body_Player> Body_Player::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Body_Player>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Body_Player");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Body_Player::Clone(void* arg)
{
    auto clone = make_shared<Body_Player>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Body_Player");
        return nullptr;
    }

    return clone;
}

void Body_Player::Free()
{
    __super::Free();
}

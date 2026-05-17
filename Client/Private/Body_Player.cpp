#include "Body_Player.h"
#include "GameInstance.h"
#include "Material.h"
#include "Material_Types.h"
#include "Player_WorldBase.h"
#include "ModelCom.h"
#include "Debug_Profile.h"
#include "ShaderPassFinder.h"

REGISTER_GLOBAL_ENUM(Client::Body_Player::PlayerWorldAnimation, "PlayerAnimation")

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

    _useComputeSkinning = false;

    if (_useComputeSkinning)
    {
        if (FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _computeRenderShaderCom)))
            return E_FAIL;

        if (FAILED(Add_Component(ETOI(LevelType::Static), TEXT("ComputeShader_AnimMeshSkinning"), _computeSkinningShaderCom)))
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
    Debug_Profile::Scope profileScope("Player::Body::Update");

    if (Has_LeaderPoseComponent())
    {
        Update_LeaderPoseMatrices();
    }
    else
    {
        _modelCom->Play_Animation(fTimeDelta);
    }

    Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
}

void Body_Player::Late_Update(float fTimeDelta)
{
    __super::Late_Update(fTimeDelta);
}

HRESULT Body_Player::Render()
{
    Debug_Profile::Scope profileScope("Player::Body::Render");

     if (!_useComputeSkinning)
        return PlayableParts::Render();

    if (FAILED(_modelCom->Prepare_ComputeSkinning(_computeSkinningShaderCom.get())))
        return E_FAIL;

    if (FAILED(_computeRenderShaderCom->Bind_ObjectCB(_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(_computeRenderShaderCom)))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const uint32 materialIndex = _modelCom->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _modelCom->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        if (FAILED(_computeRenderShaderCom->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_computeRenderShaderCom->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        // compute 결과는 VTXMESH layout으로 그리므로,
        // pass index도 VtxMesh 규칙으로 다시 계산한다.
        const uint32 passIndex =
            ShaderPassFinder::Find_PassIndex(ShaderType::VtxMesh, material->Get_RenderState());

        if (FAILED(_computeRenderShaderCom->Begin(passIndex)))
            return E_FAIL;

        if (FAILED(_modelCom->Render_ComputeSkinned(i)))
            return E_FAIL;
    }

    return S_OK;

}

HRESULT Body_Player::Render_Shadow()
{
    if (!_useComputeSkinning)
        return PlayableParts::Render_Shadow();

    if (FAILED(_modelCom->Prepare_ComputeSkinning(_computeSkinningShaderCom.get())))
        return E_FAIL;

    if (FAILED(_computeRenderShaderCom->Bind_ObjectCB(_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowCB(_computeRenderShaderCom)))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        // Shader_VtxMesh.hlsl 의 ShadowPass index = 1
        if (FAILED(_computeRenderShaderCom->Begin(1)))
            return E_FAIL;

        if (FAILED(_modelCom->Render_ComputeSkinned(i)))
            return E_FAIL;
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

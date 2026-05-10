#include "pch.h"
#include "PlayableParts.h"
#include "ModelCom.h"
#include "ShaderCom.h"
#include "Engine_Enum.h"

NS_BEGIN(Client)

PlayableParts::PlayableParts(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : PartObject{ device, context } {}

PlayableParts::PlayableParts(const PlayableParts& prototype)
    : PartObject{prototype} {}

const Matrix* PlayableParts::Get_SocketBoneMatrixPtr(const string& boneName) const
{
    if (nullptr == _modelCom)
    {
        LOG_ERROR("[PlayableParts] : There is no model ");
        return nullptr;
    }

    return _modelCom->Get_BoneMatrixPtr(boneName.c_str());
}

const Matrix* PlayableParts::Get_SocketBoneMatrixPtr(uint32 boneIndex) const
{
    if (nullptr == _modelCom)
    {
        LOG_ERROR("[PlayableParts] : There is no model ");
        return nullptr;
    }

    return _modelCom->Get_BoneMatrixPtr(boneIndex);
}

HRESULT PlayableParts::Initialize_Prototype()
{
    return PartObject::Initialize_Prototype();
}

HRESULT PlayableParts::Initialize(void* arg)
{
    if (FAILED(PartObject::Initialize(arg)))
    {
        LOG_ERROR("[PlayableParts] : Failed to Initialize PlayableParts");
        return S_OK;
    }

    if (auto playableDesc = static_cast<PLAYABLE_PART_DESC*>(arg))
    {
        if (FAILED(Ready_Components(*playableDesc)))
        {
            LOG_ERROR("[PlayableParts] : Failed to Ready Components");
            return S_OK;
        }
    }

    return S_OK;
}

void PlayableParts::Priority_Update(float timeDelta)
{
    
}

void PlayableParts::Update(float timeDelta)
{
       
}

void PlayableParts::Late_Update(float timeDelta)
{
    
}

HRESULT PlayableParts::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    const uint32 iNumMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());

    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        const Shared<Material> material = _modelCom->Get_Material(i);
        if (material == nullptr)
            continue;

        if (FAILED(_shaderCom->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_shaderCom->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        _modelCom->Bind_BoneMatrices(_shaderCom.get(), "g_BoneMatrices", i);

        const uint32 passIndex = material->Get_PassIndex();
        if (FAILED(_shaderCom->Begin(passIndex)))
            return E_FAIL;

        _modelCom->Render(i);
    }

    return S_OK;
}

HRESULT PlayableParts::Render_Shadow()
{
    if (FAILED(_shaderCom->Bind_Matrix("g_WorldMatrix", &_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowMatrices(_shaderCom, "g_ViewMatrix", "g_ProjMatrix")))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowLightDesc(_shaderCom, "g_fLightFar")))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; i++) {
        _modelCom->Bind_BoneMatrices(_shaderCom.get(), "g_BoneMatrices", i);

        //if (FAILED(_shaderCom->Begin(static_cast<uint32>(VtxAnimMeshPass::Shadow))))
        //    return E_FAIL;

        _modelCom->Render(i);
    }

    return S_OK;
}

HRESULT PlayableParts::Ready_Components(const PLAYABLE_PART_DESC& desc)
{

    if (desc.modelType == ModelType::Anim)
    {
        if (FAILED(Add_Component(ETOI(LevelType::GamePlay), TEXT("Shader_VtxAnimMesh"), _shaderCom)))
            return E_FAIL;
    }
    else
    {
        if (FAILED(Add_Component(ETOI(LevelType::GamePlay), TEXT("Shader_VtxMesh"), _shaderCom)))
            return E_FAIL;
    }

    if (FAILED(Add_Component(ETOI(LevelType::GamePlay), desc.modelTag, _modelCom)))
        return E_FAIL;

    return S_OK;
}

HRESULT PlayableParts::Bind_ShaderResources()
{

    if (FAILED(_shaderCom->Bind_Matrix("g_WorldMatrix", &_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shaderCom.get(), "g_ViewMatrix")))
        return E_FAIL;

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shaderCom.get(), "g_ProjMatrix")))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraFar(_shaderCom, "g_fFar")))
        return E_FAIL;

    return S_OK;
}


void PlayableParts::Free()
{
    PartObject::Free();
}

NS_END

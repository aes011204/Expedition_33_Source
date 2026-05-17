#include "pch.h"
#include "PlayableParts.h"
#include "ModelCom.h"
#include "ShaderCom.h"
#include "Engine_Enum.h"
#include "GameInstance.h"

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

void PlayableParts::Set_Animation(uint32 animIndex, float blendDuration, bool isLoop) const
{
    if (nullptr == _modelCom) return;

    _modelCom->Set_Animation(animIndex, blendDuration, isLoop);
}

void PlayableParts::Set_Animation(uint32 animIndex, bool isLoop) const
{
    if (nullptr == _modelCom) return;

    _modelCom->Set_Animation(animIndex, isLoop);
}

void PlayableParts::Set_AnimationByState(uint32 stateIndex, float blendDuration, bool isLoop) const
{
    if (nullptr == _modelCom) return;

    _modelCom->Set_AnimationByState(stateIndex, blendDuration, isLoop);
}

void PlayableParts::Set_AnimationByState(uint32 stateIndex, bool isLoop) const
{
    if (nullptr == _modelCom) return;

    _modelCom->Set_AnimationByState(stateIndex, isLoop);
}


uint32 PlayableParts::Get_AnimationIndex() const
{
    if (nullptr == _modelCom) return UINT_MAX;

    return _modelCom->Get_AnimationIndex();
}

uint32 PlayableParts::Get_AnimationStateIndex() const
{
    if (nullptr == _modelCom) return UINT_MAX;

    return _modelCom->Get_AnimationStateIndex();
}

float PlayableParts::Get_AnimationProgress() const
{
    if (nullptr == _modelCom) return D3D11_FLOAT32_MAX;

    return _modelCom->Get_CurrentAnimationTimeSec() / _modelCom->Get_CurrentAnimationDurationSec();
}

bool PlayableParts::Is_AnimationFinished() const
{
    return _modelCom->Is_AnimationFinished();
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
        _offsetMatrix = Matrix::Identity;
        _socketMatrix = playableDesc->socketMatrixPointer;

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
    if (!Get_Owner() || !Get_Owner()->Is_Visible())
    {
        return;
    }

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
    GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT PlayableParts::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    const uint32 iNumMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());

    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        const uint32 materialIndex = _modelCom->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _modelCom->Get_Material(materialIndex);

        if (FAILED(_shaderCom->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_shaderCom->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        CHECK_FAILED(_modelCom->Bind_BoneMatrices(_shaderCom.get(), "g_BoneMatricesBuffer", i), E_FAIL);

        const uint32 passIndex = material->Get_PassIndex();
        if (FAILED(_shaderCom->Begin(passIndex)))
            return E_FAIL;

        _modelCom->Render(i);
    }

    return S_OK;
}

HRESULT PlayableParts::Render_Shadow()
{
    if (FAILED(_shaderCom->Bind_ObjectCB(_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowCB(_shaderCom)))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; i++)
    {
        CHECK_FAILED(_modelCom->Bind_BoneMatrices(_shaderCom.get(), "g_BoneMatricesBuffer", i), E_FAIL);

        //if (FAILED(_shaderCom->Begin(static_cast<uint32>(VtxAnimMeshPass::Shadow))))
        //    return E_FAIL;

        _modelCom->Render(i);
    }

    return S_OK;
}

void PlayableParts::Set_LeaderPoseComponent(const PlayableParts* leaderPart) const
{
    CHECK_NULL(_modelCom);

    if (leaderPart && leaderPart->_modelCom)
        _modelCom->Set_LeaderPoseComponent(leaderPart->_modelCom.get());
    else
        _modelCom->Clear_LeaderPoseComponent();
}

bool PlayableParts::Has_LeaderPoseComponent() const
{
    if (_modelCom == nullptr)
        return false;

    return _modelCom->Has_LeaderPoseComponent();
}

void PlayableParts::Update_LeaderPoseMatrices() const
{
    if (_modelCom)
        _modelCom->Update_LeaderPoseMatrices();
}

void PlayableParts::Update_CombinedWorldMatrix(const Matrix& childMatrix)
{
    Matrix socketMatrix = (nullptr != _socketMatrix) ? *_socketMatrix : Matrix::Identity;

    Matrix parentMatrix = Matrix::Identity;

    if (const Shared<GameObject> owner = Get_Owner())
    {
        if (const Shared<TransformCom> ownerTransform = owner->Get_Transform())
            parentMatrix = ownerTransform->Get_WorldMatrix(); // 최신 world 강제 확보
    }
    else if (nullptr != _parentMatrix)
    {
        parentMatrix = *_parentMatrix; // owner가 없는 특수 케이스만 fallback
    }

    const Matrix combinedWorld = _offsetMatrix * childMatrix * socketMatrix * parentMatrix;
    _combinedWorldMatrix = combinedWorld;
}

HRESULT PlayableParts::Ready_Components(const PLAYABLE_PART_DESC& desc)
{

    if (desc.modelType == ModelType::Anim)
    {
        if (FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxAnimMesh"), _shaderCom)))
            return E_FAIL;
    }
    else
    {
        if (FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _shaderCom)))
            return E_FAIL;
    }

    if (FAILED(Add_Component(ETOI(LevelType::Static), desc.modelTag, _modelCom)))
        return E_FAIL;

    return S_OK;
}

HRESULT PlayableParts::Bind_ShaderResources()
{
    if (FAILED(_shaderCom->Bind_ObjectCB(_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(_shaderCom)))
        return E_FAIL;

    return S_OK;
}


void PlayableParts::Free()
{
    PartObject::Free();
}

NS_END

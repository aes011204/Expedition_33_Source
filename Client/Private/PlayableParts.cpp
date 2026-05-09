#include "pch.h"
#include "PlayableParts.h"
#include "ModelCom.h"
#include "ShaderCom.h"

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

uint32 PlayableParts::Get_AnimationIndex() const
{
    if (nullptr == _modelCom) return UINT_MAX;

    return _modelCom->Get_AnimationIndex();
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
    
}

HRESULT PlayableParts::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    const uint32 iNumMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());

    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        if (FAILED(_modelCom->Bind_Material(_shaderCom.get(), "g_DiffuseTexture", i, MaterialTextureSlot::BaseColor)))
            continue;

        _modelCom->Bind_Material(_shaderCom.get(), "g_NormalTexture", i, MaterialTextureSlot::Normal);
        _modelCom->Bind_Material(_shaderCom.get(), "g_EmissiveTexture", i, MaterialTextureSlot::Emissive);

        const Material::TextureSlot* baseColorSlot =
            _modelCom->Find_MaterialSlot(i, { MaterialTextureSlot::BaseColor });
        const Vec2 uvScale = baseColorSlot ? baseColorSlot->uvScale : Vec2(1.f, 1.f);
        const Vec2 uvOffset = baseColorSlot ? baseColorSlot->uvOffset : Vec2(0.f, 0.f);
        const float uvRotation = baseColorSlot ? baseColorSlot->uvRotation : 0.f;
        const int useOpacityMask = 0;

        _shaderCom->Bind_RawValue("g_UVScale", &uvScale, sizeof(Vec2));
        _shaderCom->Bind_RawValue("g_UVOffset", &uvOffset, sizeof(Vec2));
        _shaderCom->Bind_RawValue("g_UVRotation", &uvRotation, sizeof(float));
        _shaderCom->Bind_RawValue("g_UseOpacityMask", &useOpacityMask, sizeof(int));

        _modelCom->Bind_BoneMatrices(_shaderCom.get(), "g_BoneMatrices", i);

        if (FAILED(_shaderCom->Begin(0)))
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

        if (FAILED(_shaderCom->Begin(1)))
            return E_FAIL;

        _modelCom->Render(i);
    }

    return S_OK;
}

void PlayableParts::Update_CombinedWorldMatrix(const Matrix& childMatrix)
{
    Matrix socketMatrix = (nullptr != _socketMatrix) ? *_socketMatrix : Matrix::Identity;

    //Vector3 right = socketMatrix.Right();
    //Vector3 up = socketMatrix.Up();
    //Vector3 forward = socketMatrix.Backward();
    //right.Normalize();
    //up.Normalize();
    //forward.Normalize();
    //socketMatrix.Right(right);
    //socketMatrix.Up(up);
    //socketMatrix.Backward(forward);

    const Matrix& parentMatrix = (nullptr != _parentMatrix) ? *_parentMatrix : Matrix::Identity;
    const Matrix childWorld = childMatrix;
    const Matrix combinedWorld = childWorld * socketMatrix * parentMatrix;
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

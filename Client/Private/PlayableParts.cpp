#include "pch.h"
#include "PlayableParts.h"
#include "ModelCom.h"
#include "ShaderCom.h"
#include "Engine_Enum.h"
#include "GameInstance.h"
#include "AnimComputeRuntime.h"
#include "ComputeShaderCom.h"
#include "ContainerObject.h"
#include "EffectCom.h"
#include "PreviewRimFeature.h"
#include "SwitchDissolveFeature.h"
#include "BattleTurnCamera.h"

NS_BEGIN(Client)

namespace
{
    bool SupportsPreviewRimPartTag(const wstring& partTag)
    {
        return partTag == L"Part_Body" ||
               partTag == L"Part_Head" ||
               partTag == L"Part_Hair";
    }
}

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

bool PlayableParts::Set_AnimationByState(uint32 stateIndex, float blendDuration, bool isLoop) const
{
    if (nullptr == _modelCom) return false;

    return _modelCom->Set_AnimationByState(stateIndex, blendDuration, isLoop);
}

bool PlayableParts::Set_AnimationByState(uint32 stateIndex, bool isLoop) const
{
    if (nullptr == _modelCom) return false;

    return _modelCom->Set_AnimationByState(stateIndex, isLoop);
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

    return _modelCom->Get_CurrentProgress();
}

bool PlayableParts::Is_AnimationBlending() const
{
    return _modelCom->Is_AnimationBlending();
}

bool PlayableParts::Is_AnimationFinished() const
{
    return _modelCom->Is_AnimationFinished();
}

Vec3 PlayableParts::Get_RootMotionPosition() const
{
    return _modelCom->Get_RootTransformVelocity().position;
}

void PlayableParts::Set_AnimationSpeed(float speed) const
{
    return _modelCom->Set_FrameSpeed(speed);
}

float PlayableParts::Get_AnimationSpeed() const
{
    return _modelCom->Get_FrameSpeed();
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
        return E_FAIL;
    }

    Set_ShadowCasterType(ShadowCasterType::Dynamic);

    if (auto playableDesc = static_cast<PLAYABLE_PART_DESC*>(arg))
    {
        _offsetMatrix = Matrix::Identity;
        _socketMatrix = playableDesc->socketMatrixPointer;

        if (FAILED(Ready_Components(*playableDesc)))
        {
            LOG_ERROR("[PlayableParts] : Failed to Ready Components");
            return E_FAIL;
        }
    }

    return S_OK;
}

void PlayableParts::Priority_Update(float timeDelta)
{
    
}

void PlayableParts::Update(float timeDelta)
{
    // 기존에 있던 _switchDissolve Update는 EffectCom으로 이동
}

void PlayableParts::Late_Update(float timeDelta)
{
    Shared<GameObject> owner = Get_Owner();
    if (!owner)
    {
        return;
    }

    if (Casts_DynamicShadow() && Is_Visible())
        GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());

    if (!owner->Is_Visible())
        return;

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

Matrix PlayableParts::Get_GizmoWorldMatrix() const
{
    if (_transformCom == nullptr)
        return _combinedWorldMatrix;

    Matrix parentMatrix = Matrix::Identity;

    if (const Shared<GameObject> owner = Get_Owner())
    {
        if (const Shared<TransformCom> ownerTransform = owner->Get_Transform())
            parentMatrix = ownerTransform->Get_WorldMatrix();
    }
    else if (nullptr != _parentMatrix)
    {
        parentMatrix = *_parentMatrix;
    }

    const Matrix socketMatrix = (nullptr != _socketMatrix) ? *_socketMatrix : Matrix::Identity;
    return _offsetMatrix * _transformCom->Get_WorldMatrix() * socketMatrix * parentMatrix;
}

bool PlayableParts::Apply_GizmoWorldMatrix(const Matrix& gizmoWorldMatrix)
{
    if (_transformCom == nullptr)
        return false;

    Matrix parentMatrix = Matrix::Identity;

    if (const Shared<GameObject> owner = Get_Owner())
    {
        if (const Shared<TransformCom> ownerTransform = owner->Get_Transform())
            parentMatrix = ownerTransform->Get_WorldMatrix();
    }
    else if (nullptr != _parentMatrix)
    {
        parentMatrix = *_parentMatrix;
    }

    Matrix socketMatrix = (nullptr != _socketMatrix) ? *_socketMatrix : Matrix::Identity;
    Matrix localWorldMatrix = _offsetMatrix.Invert() * gizmoWorldMatrix * (socketMatrix * parentMatrix).Invert();

    Vec3 scale = Vec3::One;
    Quat rotation = Quat::Identity;
    Vec3 translation = Vec3::Zero;

    if (!localWorldMatrix.Decompose(scale, rotation, translation))
        return false;

    _transformCom->Set_WorldPosition(translation);
    _transformCom->Set_RotationQuaternion(rotation);
    _transformCom->Set_Scale(scale);
    return true;
}

HRESULT PlayableParts::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    CHECK_FAILED_THROTTLED(_animComputeRuntime->Prepare_BoneMatrixBuffers(), 60, E_FAIL);

    const uint32 iNumMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());
    const uint32 supportFeatureMask = Build_DrawFeatureSupportMask(*this);

    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        const uint32 materialIndex = _modelCom->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _modelCom->Get_Material(materialIndex);

        if (FAILED(_shaderCom->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_shaderCom->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        CHECK_FAILED_THROTTLED(_animComputeRuntime->Bind_PreparedBoneMatrices(_shaderCom.get(), "g_BoneMatricesBuffer", i), 60, E_FAIL);

        CHECK_FAILED_THROTTLED(EffectCom::Bind_DrawFeatures(Get_Owner(), _shaderCom.get(), supportFeatureMask), 60, E_FAIL);

        const uint32 passIndex = material->Get_PassIndex();
        if (FAILED(_shaderCom->Begin(passIndex)))
            return E_FAIL;

        // 플레이어 파츠가 그려진 픽셀에 stencil=1을 남겨 Background UI가 이 영역을 피하게 한다.
        const bool depthWrite = passIndex != static_cast<uint32>(VtxAnimMeshPass::HairBlend);
        CHECK_FAILED_THROTTLED(GAME->Bind_PlayerStencilWriteState(depthWrite), 60, E_FAIL);

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

    // shadow pass 전에도 bone buffer 최신화
    CHECK_FAILED(_animComputeRuntime->Prepare_BoneMatrixBuffers(), E_FAIL);

    const uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; i++)
    {
        CHECK_FAILED(_animComputeRuntime->Bind_PreparedBoneMatrices(_shaderCom.get(), "g_BoneMatricesBuffer", i), E_FAIL);

        if (FAILED(_shaderCom->Begin(static_cast<uint32>(VtxAnimMeshPass::Shadow))))
            return E_FAIL;

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

void PlayableParts::Update_LeaderPoseMatrices(float timeDelta) const
{
    if (_modelCom == nullptr)
        return;

    _modelCom->Update_LeaderPoseMatrices(timeDelta);
    CHECK_FAILED(_animComputeRuntime->Prepare_BoneMatrixBuffers());
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

HRESULT PlayableParts::Bind_PlayerRenderCB(Shared<ShaderCom> shader)
{
    CHECK_NULL(shader, E_FAIL);

    PlayerRenderCB playerRenderCB{};

    if (const auto battleCamera =
        dynamic_pointer_cast<BattleTurnCamera>(GAME->Get_CameraByType(ObjectType::BattleTurnCamera)))
    {
        if (battleCamera->Should_ClipPlayerNearCamera())
        {
            playerRenderCB.nearClipParams.x = 1.f;
            playerRenderCB.nearClipParams.y = battleCamera->Get_PlayerNearClipDistance();
        }
    }

    return shader->Bind_CBufferData(playerRenderCB);
}

uint32 PlayableParts::Build_DrawFeatureSupportMask(const PlayableParts& playablePart)
{
    // 디졸브는 PlayableParts 전체에 적용 가능하니까 일단 기본으로 켜놓는다.
    uint32 supportMask = DrawFeatureFlagDissolve;

    const Shared<ContainerObject> ownerContainer =
        dynamic_pointer_cast<ContainerObject>(playablePart.Get_Owner());
    if (ownerContainer == nullptr)
        return supportMask;

    for (const auto& [partTag, partObject] : ownerContainer->Get_PartObjects())
    {
        if (partObject.get() != &playablePart)
            continue;

        if (SupportsPreviewRimPartTag(partTag))
            supportMask |= DrawFeatureFlagPreviewRim;
        break;
    }

    return supportMask;
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

    Shared<ComputeShaderCom> boneRemapShader{};
    Shared<ComputeShaderCom> skinningShader{};
 
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("ComputeShader_BoneRemap"), boneRemapShader), E_FAIL); 
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("ComputeShader_AnimMeshSkinning"), skinningShader), E_FAIL); 
 
    AnimComputeRuntime::CreateDesc runtimeDesc{};
    runtimeDesc.model = _modelCom.get();
    runtimeDesc.boneRemapShader = boneRemapShader;
    runtimeDesc.skinningShader = skinningShader;

    // 모든 playable part가 공통 runtime을 하나씩 가진다
    _animComputeRuntime = AnimComputeRuntime::Create(_device, _context, runtimeDesc); 
    CHECK_NULL(_animComputeRuntime, E_FAIL);

    return S_OK;
}

HRESULT PlayableParts::Bind_ShaderResources()
{
    if (FAILED(_shaderCom->Bind_ObjectCB(_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(_shaderCom)))
        return E_FAIL;

    if (FAILED(Bind_PlayerRenderCB(_shaderCom)))
        return E_FAIL;

    return S_OK;
}

void PlayableParts::Free()
{
    PartObject::Free();
}

NS_END

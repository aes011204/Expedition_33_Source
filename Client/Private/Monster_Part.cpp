#include "Monster_Part.h"

#include "GameInstance.h"
#include "Material_Types.h"
#include "ModelCom.h"
#include "ShaderCom.h"
#include "AnimComputeRuntime.h"
#include "ComputeShaderCom.h"

NS_BEGIN(Client)

Monster_Part::Monster_Part(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : PartObject(device, context)
{
}

Monster_Part::Monster_Part(const Monster_Part& prototype)
    : PartObject(prototype)
{
}

HRESULT Monster_Part::Initialize_Prototype()
{
    return PartObject::Initialize_Prototype();
}

HRESULT Monster_Part::Initialize(void* arg)
{
    if (FAILED(PartObject::Initialize(arg)))
        return E_FAIL;

    auto* desc = static_cast<MONSTER_PART_DESC*>(arg);
    if (desc == nullptr)
        return E_FAIL;

    Set_ShadowCasterType(ShadowCasterType::Dynamic);

    return Ready_Components(*desc);
}

void Monster_Part::Priority_Update(float timeDelta)
{
}

void Monster_Part::Update(float timeDelta)
{
    Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());
}

void Monster_Part::Late_Update(float timeDelta)
{
    // culling
    if (!Get_Owner() || !Get_Owner()->Is_Visible())
    {
        return;
    }

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());

    if (Casts_DynamicShadow())
        GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
}

HRESULT Monster_Part::Render()
{
    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    // runtime compute 경로가 있으면 runtime buffer 준비
    if (_animComputeRuntime != nullptr)
    {
        CHECK_FAILED(_animComputeRuntime->Prepare_BoneMatrixBuffers(), E_FAIL);
    }
    // runtime이 없는 anim/noncompute 경로는 ModelCom 기본 bone buffer 사용S
    else if (_model->Get_NumAnimations() != 0)
    {
        CHECK_FAILED(_model->Prepare_BoneMatrixBuffers(), E_FAIL);
    }

    const uint32 iNumMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _model->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_shader->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        // runtime compute 경로
        if (_animComputeRuntime != nullptr)
        {
            CHECK_FAILED_ONCE(
                _animComputeRuntime->Bind_PreparedBoneMatrices(
                    _shader.get(),
                    "g_BoneMatricesBuffer",
                    i),
                E_FAIL);
        }
        // runtime 없는 기본 ModelCom 경로
        else if (_model->Get_NumAnimations() != 0)
        {
            CHECK_FAILED_ONCE(
                _model->Bind_PreparedBoneMatrices(
                    _shader.get(),
                    "g_BoneMatricesBuffer",
                    i),
                E_FAIL);
        }

        const uint32 passIndex = Decide_PassIndex(material);
        if (FAILED(_shader->Begin(passIndex)))
            return E_FAIL;

        _model->Render(i);
    }

    return S_OK;
}

HRESULT Monster_Part::Render_Shadow()
{
    if (FAILED(_shader->Bind_ObjectCB(_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_ShadowCB(_shader)))
        return E_FAIL;

    // runtime compute 경로가 있으면 runtime buffer 준비
    if (_animComputeRuntime != nullptr)
    {
        CHECK_FAILED(_animComputeRuntime->Prepare_BoneMatrixBuffers(), E_FAIL);
    }
    // runtime이 없는 anim/noncompute 경로는 ModelCom 기본 bone buffer 사용
    else if (_model->Get_NumAnimations() != 0)
    {
        CHECK_FAILED(_model->Prepare_BoneMatrixBuffers(), E_FAIL);
    }

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        // runtime compute 경로
        if (_animComputeRuntime != nullptr)
        {
            CHECK_FAILED(
                _animComputeRuntime->Bind_PreparedBoneMatrices(
                    _shader.get(),
                    "g_BoneMatricesBuffer",
                    i),
                E_FAIL);
        }
        // runtime 없는 기본 ModelCom 경로
        else if (_model->Get_NumAnimations() != 0)
        {
            CHECK_FAILED(
                _model->Bind_PreparedBoneMatrices(
                    _shader.get(),
                    "g_BoneMatricesBuffer",
                    i),
                E_FAIL);
        }

        const uint32 shadowPass =
            _model->Is_Skeletal() ? static_cast<uint32>(VtxAnimMeshPass::Shadow) : 1u;

        if (FAILED(_shader->Begin(shadowPass)))
            return E_FAIL;

        _model->Render(i);
    }

    return S_OK;
}

void Monster_Part::Set_Animation(uint32 animIndex, float blendDuration, bool isLoop)
{
    if (_model == nullptr)
        return;

    _model->Set_Animation(animIndex, blendDuration, isLoop);
}

void Monster_Part::Set_Animation(const string& animName, float blendDuration, bool isLoop, float startFrame)
{
    if (_model == nullptr)
        return;

    _model->Set_Animation(animName, blendDuration, isLoop, startFrame);
}

void Monster_Part::Set_AnimationByState(uint32 stateIndex, float blendDuration, bool isLoop, float startFrame)
{
    if (_model == nullptr)
        return;

    _model->Set_AnimationByState(stateIndex, blendDuration, isLoop,startFrame);
}

bool Monster_Part::Play_Animation(float timeDelta)
{
    if (_model == nullptr)
        return false;

    return _model->Play_Animation(timeDelta);
}

const RootMotionFrame& Monster_Part::Get_RootTransformVelocity() const
{
    return _model->Get_RootTransformVelocity();
}

const Matrix* Monster_Part::Get_SocketBoneMatrixPtr(const string& boneName) const
{
    return _model ? _model->Get_BoneMatrixPtr(boneName.c_str()) : nullptr;
}

const Matrix* Monster_Part::Get_SocketBoneMatrixPtr(uint32 boneIndex) const
{
    return _model ? _model->Get_BoneMatrixPtr(boneIndex) : nullptr;
}

HRESULT Monster_Part::Ready_Components(const MONSTER_PART_DESC& desc)
{
    if (desc.modelType == ModelType::Anim)
    {
        if (FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxAnimMesh"), _shader))) // anim 몬스터도 shader를 먼저 붙여야 한다
            return E_FAIL;
    }
    else
    {
        if (FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _shader))) 
            return E_FAIL;
    }

    if (FAILED(Add_Component(ETOI(LevelType::Static), desc.modelTag, _model))) // 몬스터 파츠가 참조하는 모델 prototype은 Static에 둔다.
        return E_FAIL;

    if (desc.modelType == ModelType::Anim)
    {
        Shared<ComputeShaderCom> boneRemapShader{};
        Shared<ComputeShaderCom> skinningShader{};

        CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("ComputeShader_BoneRemap"), boneRemapShader), E_FAIL);
        CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("ComputeShader_AnimMeshSkinning"), skinningShader), E_FAIL);

        AnimComputeRuntime::CreateDesc runtimeDesc{};
        runtimeDesc.model = _model.get();
        runtimeDesc.boneRemapShader = boneRemapShader;
        runtimeDesc.skinningShader = skinningShader;

        _animComputeRuntime = AnimComputeRuntime::Create(_device, _context, runtimeDesc);
        CHECK_NULL(_animComputeRuntime, E_FAIL);
    }

    return S_OK;
}

HRESULT Monster_Part::Bind_ShaderResources()
{
    if (FAILED(_shader->Bind_ObjectCB(_combinedWorldMatrix)))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
        return E_FAIL;

    return S_OK;
}

uint32 Monster_Part::Decide_PassIndex(Shared<Material> mat)
{
    return mat->Get_PassIndex();
}

void Monster_Part::Free()
{
    PartObject::Free();
}

NS_END

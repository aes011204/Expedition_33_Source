#include "Monster_Part.h"

#include "GameInstance.h"
#include "Material_Types.h"
#include "ModelCom.h"
#include "ShaderCom.h"
#include "AnimComputeRuntime.h"
#include "ComputeShaderCom.h"
#include "EffectCom.h"
#include "PreviewRimFeature.h"
#include "SwitchDissolveFeature.h"

#include <filesystem>

NS_BEGIN(Client)

namespace
{
    namespace fs = std::filesystem;

    ModelType Parse_ModelTypeString(const string& modelTypeString, ModelType fallbackType)
    {
        if (modelTypeString == "ANIM" || modelTypeString == "Anim")
            return ModelType::Anim;

        if (modelTypeString == "NONANIM" || modelTypeString == "NonAnim")
            return ModelType::NonAnim;

        return fallbackType;
    }

    wstring Resolve_ModelTablePath(const wstring& rawPath)
    {
        if (rawPath.empty())
            return {};

        const fs::path inputPath(rawPath);
        if (inputPath.is_absolute())
            return inputPath.lexically_normal().wstring();

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
            return fs::absolute(inputPath).lexically_normal().wstring();

        const fs::path assetRootPath(assetRoot);
        const fs::path clientBinPath = assetRootPath.parent_path();

        wstring normalized = rawPath;
        for (wchar_t& ch : normalized)
        {
            if (ch == L'/')
                ch = L'\\';
            else
                ch = static_cast<wchar_t>(towlower(ch));
        }

        constexpr const wchar_t* kResourcesPrefix = L"..\\bin\\resources\\";
        if (normalized.starts_with(kResourcesPrefix))
        {
            const wstring suffix = rawPath.substr(wcslen(kResourcesPrefix));
            return (assetRootPath / fs::path(suffix)).lexically_normal().wstring();
        }

        constexpr const wchar_t* kResourcesRootPrefix = L"..\\resources\\";
        if (normalized.starts_with(kResourcesRootPrefix))
        {
            const wstring suffix = rawPath.substr(wcslen(kResourcesRootPrefix));
            return (assetRootPath / fs::path(suffix)).lexically_normal().wstring();
        }

        return (clientBinPath / inputPath).lexically_normal().wstring();
    }

    Shared<ModelCom> Try_CreateModelFromDT(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wstring& modelTag,
        ModelType fallbackType)
    {
        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
            return nullptr;

        const fs::path dtPath = fs::path(assetRoot) / L"Data" / L"json" / L"DT_Model.json";
        const json root = GAME->Load_JsonFile(dtPath);
        if (!root.is_object())
            return nullptr;

        const json* modelArray = nullptr;
        if (root.contains("DT_Model") && root["DT_Model"].is_array())
            modelArray = &root["DT_Model"];
        else if (root.contains("ModelCom") && root["ModelCom"].is_array())
            modelArray = &root["ModelCom"];

        if (modelArray == nullptr)
            return nullptr;

        const string modelTagString = String::ToString(modelTag);
        for (const json& modelNode : *modelArray)
        {
            if (!modelNode.is_object())
                continue;

            if (modelNode.value("Id", string{}) != modelTagString)
                continue;

            const string modelPathString = modelNode.value("Path", string{});
            if (modelPathString.empty())
                return nullptr;

            const ModelType modelType =
                Parse_ModelTypeString(modelNode.value("ModelType", string{}), fallbackType);

            const Matrix preLocalTransform =
                XMMatrixScaling(
                    modelNode.value("ScaleX", 1.f),
                    modelNode.value("ScaleY", 1.f),
                    modelNode.value("ScaleZ", 1.f)) *
                XMMatrixRotationX(XMConvertToRadians(modelNode.value("RotX", 0.f))) *
                XMMatrixRotationY(XMConvertToRadians(modelNode.value("RotY", 0.f))) *
                XMMatrixRotationZ(XMConvertToRadians(modelNode.value("RotZ", 0.f)));

            const wstring resolvedPath = Resolve_ModelTablePath(String::ToWString(modelPathString));
            return ModelCom::Create(
                device,
                context,
                modelType,
                String::ToString(resolvedPath).c_str(),
                preLocalTransform);
        }

        return nullptr;
    }
}

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
    Shared<GameObject> owner = Get_Owner();
    if (!owner)
    {
        return;
    }

    if (Casts_DynamicShadow() && Is_Visible())
        GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
   
    // 메인 카메라 가시성은 본체 렌더만 막고, shadow 등록은 위에서 분리한다.
    if (!owner->Is_Visible() || !Is_Visible())
    {
        return;
    }
   
    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT Monster_Part::Render()
{
    if (_model == nullptr || _shader == nullptr)
        return S_OK;

    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    CHECK_FAILED_THROTTLED(
        EffectCom::Bind_DrawFeatures(
            GetSharedPtr<Monster_Part>(),
            _shader.get(),
            DrawFeatureFlagDissolve | DrawFeatureFlagPreviewRim),
        60,
        E_FAIL
    );

    // runtime compute 경로가 있으면 runtime buffer 준비
    if (_animComputeRuntime != nullptr)
        CHECK_FAILED_THROTTLED(_animComputeRuntime->Prepare_BoneMatrixBuffers(), 60, E_FAIL);
    // runtime이 없는 anim/noncompute 경로는 ModelCom 기본 bone buffer 사용S
    else if (_model->Get_NumAnimations() != 0)
        CHECK_FAILED_THROTTLED(_model->Prepare_BoneMatrixBuffers(), 60, E_FAIL);

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
            CHECK_FAILED_THROTTLED(
                _animComputeRuntime->Bind_PreparedBoneMatrices(_shader.get(), "g_BoneMatricesBuffer", i),
                60,
                E_FAIL
            );
        }
        // runtime 없는 기본 ModelCom 경로
        else if (_model->Get_NumAnimations() != 0)
        {
            CHECK_FAILED_THROTTLED(
                _model->Bind_PreparedBoneMatrices(_shader.get(), "g_BoneMatricesBuffer", i),
                60,
                E_FAIL
            );
        }

        const uint32 passIndex = Decide_PassIndex(material);
        if (FAILED(_shader->Begin(passIndex)))
            return E_FAIL;

        // DepthStencil을 사용해서 ScreenFX 에서 제외할지 말지 파악하기 위한 depth 기록
        const bool depthWrite = passIndex != static_cast<uint32>(VtxAnimMeshPass::HairBlend);
        CHECK_FAILED_THROTTLED(GAME->Bind_ScreenFxExclusionStencilWriteState(depthWrite), 60, E_FAIL);

        _model->Render(i);
    }

    return S_OK;
}

HRESULT Monster_Part::Render_Shadow()
{
    if (_model == nullptr || _shader == nullptr)
        return S_OK;

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
                _animComputeRuntime->Bind_PreparedBoneMatrices(_shader.get(), "g_BoneMatricesBuffer", i), E_FAIL);
        }
        // runtime 없는 기본 ModelCom 경로
        else if (_model->Get_NumAnimations() != 0)
        {
            CHECK_FAILED(
                _model->Bind_PreparedBoneMatrices(_shader.get(), "g_BoneMatricesBuffer", i), E_FAIL);
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

bool Monster_Part::Set_AnimationByState(uint32 stateIndex, float blendDuration, bool isLoop, float startFrame)
{
    if (_model == nullptr)
        return false;

    const bool applied = _model->Set_AnimationByState(stateIndex, blendDuration, isLoop, startFrame);
    const char* animName = _model->Get_AnimationName(_model->Get_CurrentAnimationIndex());

    //LOG_DEBUG("Monster_Part::Set_AnimationByState state={} applied={} currentAnim={}",
    //    stateIndex,
    //    applied,
    //    animName != nullptr ? animName : "<null>");

    return applied;
}

bool Monster_Part::Play_Animation(float timeDelta)
{
    if (_model == nullptr)
        return false;

    return _model->Play_Animation(timeDelta);
}

const RootMotionFrame& Monster_Part::Get_RootTransformVelocity() const
{
    static RootMotionFrame empty{};
    return _model ? _model->Get_RootTransformVelocity() : empty;
}

const Matrix* Monster_Part::Get_SocketBoneMatrixPtr(const string& boneName) const
{
    return _model ? _model->Get_BoneMatrixPtr(boneName.c_str()) : nullptr;
}

const Matrix* Monster_Part::Get_SocketBoneMatrixPtr(uint32 boneIndex) const
{
    return _model ? _model->Get_BoneMatrixPtr(boneIndex) : nullptr;
}


bool Monster_Part::Is_AnimationFinished() const
{
    return _model->Is_AnimationFinished();
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
    {
        _model = Try_CreateModelFromDT(_device, _context, desc.modelTag, desc.modelType);
        if (_model == nullptr)
        {
            if (!desc.allowMissingModel)
                return E_FAIL;
        }
        else
        {
            _model->Set_Owner(this->GetSharedPtr<GameObject>());
            _components.emplace(desc.modelTag, _model);
        }

        //LOG_WARN("Monster_Part: missing model allowed. modelTag={}",
        //    String::ToString(desc.modelTag).c_str());
    }

    if (desc.modelType == ModelType::Anim && _model != nullptr)
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


    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _partEffectCom), E_FAIL);


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

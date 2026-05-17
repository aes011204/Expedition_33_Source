#include "ModelCom.h"
#include "GameInstance.h"
#include "Asset_Manager.h"
#include "MaterialBuilder.h"
#include "Mesh.h"
#include "Bone.h"
#include "ShaderCom.h"
#include "Material.h"
#include "Animation.h"
#include "SkeletalMesh.h"
#include "StaticMesh.h"
#include "ComputeStructuredBuffer.h"

#include "AnimNotify_Parser.h"
#include "AnimNotify.h"
#include "AnimNotifyState.h"
#include "AnimState_Parser.h"

ModelCom::ModelCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component{ device, context }
{
}

ModelCom::ModelCom(const ModelCom& prototype)
    : Component{ prototype }
    , _type{ prototype._type }
    , _preLocalTransformMatrix{ prototype._preLocalTransformMatrix }
    , _isSkeletal{ prototype._isSkeletal }
    , _hasLocalBounds{ prototype._hasLocalBounds }
    , _localBoundsMin{ prototype._localBoundsMin }
    , _localBoundsMax{ prototype._localBoundsMax }
    , _maxTextureType{ prototype._maxTextureType }
    , _numMeshes{ prototype._numMeshes }
    , _numMaterials{ prototype._numMaterials }
    , _numBones{ prototype._numBones }
    , _numAnimations{ prototype._numAnimations }
    , _meshes{ prototype._meshes }
    , _materials{ prototype._materials }
    , _sourceModelPath{ prototype._sourceModelPath }
    , _isAnimationPaused{ prototype._isAnimationPaused }
    , _collisionMeshes{ prototype._collisionMeshes }
    , _stateAnimIndices{ prototype._stateAnimIndices }
    , _animEnumType{ prototype._animEnumType }
    , _mappedAnimIndices{ prototype._mappedAnimIndices }
{
    for (auto& prototypeAnim : prototype._animations)
        _animations.push_back(prototypeAnim->Clone());

    for (auto& prototypeBone : prototype._bones)
        _bones.push_back(prototypeBone->Clone());

    _combinedBoneMatrixCache.resize(_numBones, Matrix::Identity);
    _meshBoneMatrixCaches.resize(_meshes.size());
    _meshBoneMatrixBuffers.resize(_meshes.size());
    _boneMatricesDirty = true;

    Mark_BoneMatricesDirty();

    _leaderPoseMatricesValid = false;
}

uint32 ModelCom::Get_MeshMaterialIndex(uint32 meshIndex) const
{
    if (meshIndex >= _meshes.size())
        return UINT32_MAX;

    return _meshes[meshIndex]->Get_MaterialIndex();
}

Shared<Material> ModelCom::Get_Material(uint32 materialIndex) const
{
    if (materialIndex >= _materials.size())
        return nullptr;

    return _materials[materialIndex];
}

const Material::TextureSlot* ModelCom::Find_MaterialSlot(uint32 meshIndex, std::initializer_list<MaterialTextureSlot> materialSlots,
    uint32 textureIndex) const
{
     if (meshIndex >= _meshes.size())
        return nullptr;

    const uint32 materialIndex = _meshes[meshIndex]->Get_MaterialIndex();
    if (materialIndex >= _materials.size() || _materials[materialIndex] == nullptr)
        return nullptr;

    const Shared<Material>& material = _materials[materialIndex];

    for (MaterialTextureSlot slot : materialSlots)
    {
        if (const Material::TextureSlot* textureSlot = material->Find_TextureSlot(slot, textureIndex))
            return textureSlot;
    }

    return nullptr;
}

int32 ModelCom::Get_BoneIndex(const string& boneName) const
{
    for (uint32 i = 0; i < _numBones; ++i)
    {
        if (_bones[i]->Check_BoneByName(boneName))
            return i;
    }

    return -1;
}

const string& ModelCom::Get_BoneName(uint32 boneIndex) const
{
    return _bones[boneIndex]->Get_BoneName();
}

void ModelCom::Set_Animation(uint32 animIndex, bool isLoop, float startFrame)
{
    if (animIndex >= _animations.size()) return;

    _currentAnimIndex = animIndex;
    _isAnimLoop = isLoop;
    _isBlending = false;
    _isFinished = false;
    _animations[_currentAnimIndex]->Set_StartFrame(startFrame);

    Mark_BoneMatricesDirty();
}

void ModelCom::Set_AnimationByState(uint32 stateIndex, bool isLoop, float startFrame)
{
    uint32 animIndex = stateIndex;

    if (!_stateAnimIndices.empty() && stateIndex < _stateAnimIndices.size())
    {
        if (_stateAnimIndices[stateIndex] != UINT32_MAX)
        {
            animIndex = _stateAnimIndices[stateIndex];
        }
    }

    if (animIndex >= _animations.size()) return;

    _currentAnimStateIndex = stateIndex;
    _currentAnimIndex = animIndex;
    _isAnimLoop = isLoop;
    _isBlending = false;
    _isFinished = false;
    _animations[_currentAnimIndex]->Set_StartFrame(startFrame);

    Mark_BoneMatricesDirty();
}

void ModelCom::Set_Animation(uint32 animIndex, float blendDuration, bool isLoop, float startFrame)
{
    if (animIndex >= _animations.size()) return;

    _nextAnimIndex = animIndex;
    _isAnimLoop = isLoop;
    _isBlending = true;
    _blendDuration = blendDuration;
    _blendElapsedTime = 0.f;
    _isFinished = false;
    _animations[_nextAnimIndex]->Set_StartFrame(startFrame);

    Mark_BoneMatricesDirty();
}

void ModelCom::Set_Animation(const string& animName, bool isLoop, float startFrame)
{
    if (!_mappedAnimIndices.contains(animName))
    {
        LOG_WARN("Failed to find animation by name: {}", animName);
        return;
    }

    Set_Animation(_mappedAnimIndices[animName], isLoop, startFrame);
}

void ModelCom::Set_Animation(const string& animName, float blendDuration, bool isLoop, float startFrame)
{
    if (!_mappedAnimIndices.contains(animName))
    {
        LOG_WARN("Failed to find animation by name: {}", animName);
        return;
    }

    Set_Animation(_mappedAnimIndices[animName], blendDuration, isLoop, startFrame);
}

void ModelCom::Set_AnimationByState(uint32 stateIndex, float blendDuration, bool isLoop, float startFrame)
{
    uint32 animIndex = stateIndex;

    if (!_stateAnimIndices.empty() && stateIndex < _stateAnimIndices.size())
    {
        if (_stateAnimIndices[stateIndex] != UINT32_MAX)
        {
            animIndex = _stateAnimIndices[stateIndex];
        }
    }

    if (animIndex >= _animations.size()) return;

    _nextAnimStateIndex = stateIndex;
    _nextAnimIndex = animIndex;
    _isAnimLoop = isLoop;
    _isBlending = true;
    _blendDuration = blendDuration;
    _blendElapsedTime = 0.f;
    _isFinished = false;
    _animations[_nextAnimIndex]->Set_StartFrame(startFrame);

    Mark_BoneMatricesDirty();
}

const char* ModelCom::Get_AnimationName(uint32 animationIndex) const
{
    if (animationIndex >= _animations.size())
        return nullptr;

    return _animations[animationIndex]->Get_AnimationName().c_str();
}

const Matrix* ModelCom::Get_BoneMatrixPtr(const char* boneName) const
{
    int32 boneIndex = Get_BoneIndex(boneName);
    if (boneIndex == -1)
        return nullptr;

    return Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
}

const Matrix* ModelCom::Get_BoneMatrixPtr(uint32 boneIndex) const
{
    if (!_leaderPoseCombinedMatrices.empty())
        return &_leaderPoseCombinedMatrices[boneIndex];

    return _bones[boneIndex]->Get_CombinedTransformationMatrixPtr();
}

int32 ModelCom::Get_BoneParentIndex(uint32 boneIndex) const
{
    if (boneIndex >= _bones.size() || _bones[boneIndex] == nullptr)
        return -1;

    return _bones[boneIndex]->Get_ParentIndex();
}

const Matrix& ModelCom::Get_BoneLocalMatrix(uint32 boneIndex) const
{
    static const Matrix identity = Matrix::Identity;

    if (boneIndex >= _bones.size() || _bones[boneIndex] == nullptr)
        return identity;

    return _bones[boneIndex]->Get_TransformationMatrix();
}

void ModelCom::Set_BoneLocalMatrix(uint32 boneIndex, const Matrix& localMatrix)
{
    if (boneIndex >= _bones.size() || _bones[boneIndex] == nullptr)
        return;

    _bones[boneIndex]->Update_TransformationMatrix(localMatrix);
    Mark_BoneMatricesDirty();
}

void ModelCom::Rebuild_BoneCombinedMatrices()
{
    if (!_isSkeletal || _numBones == 0)
        return;

    for (uint32 i = 0; i < _numBones; ++i)
    {
        if (_bones[i] != nullptr)
            _bones[i]->Update_CombinedTransformationMatrix(_bones, _preLocalTransformMatrix);
    }

    Mark_BoneMatricesDirty();
}

float ModelCom::Get_CurrentProgress() const
{
    if (_animations.empty() || _currentAnimIndex >= _animations.size())
        return 0.f;

    return _animations[_currentAnimIndex]->Get_Progress();
}

float ModelCom::Get_CurrentFrame() const
{
    if (_animations.empty() || _currentAnimIndex >= _animations.size())
        return 0.f;

    return _animations[_currentAnimIndex]->Get_Progress() * _animations[_currentAnimIndex]->Get_Duration();
}

const RootMotionFrame& ModelCom::Get_RootTransformVelocity(float scaleOffset)
{
    _rootFrameVelocity.position = scaleOffset * _rootOriginalFrameVelocity.position;
    _rootFrameVelocity.rotation = scaleOffset * _rootOriginalFrameVelocity.rotation;

    return _rootFrameVelocity;
}

HRESULT ModelCom::Initialize_Prototype(ModelType type, const char* modelFilePath, const Matrix& preLocalTransformMatrix)
{
    _type = type;
    _preLocalTransformMatrix = preLocalTransformMatrix;

    _sourceModelPath = Normalize_ModelAssetPath(modelFilePath);

    const string sourcePathUtf8 = String::ToString(_sourceModelPath);

    if (FAILED(Ready_Model(sourcePathUtf8.c_str())))
    {
        LOG_ERROR("Failed to Load Model Path {}", sourcePathUtf8);
        return E_FAIL;
    }

    Ensure_MaterialBindings();

    if (FAILED(Ready_Animation(sourcePathUtf8.c_str())))
    {
        LOG_ERROR("Failed to Load Model Animation Path {}", sourcePathUtf8);
        return E_FAIL;
    }

    return S_OK;
}

HRESULT ModelCom::Initialize(void* arg)
{
    return S_OK;
}

void ModelCom::Ensure_MaterialBindings()
{
    const size_t desired = _materials.size();
    if (_materialBindings.size() == desired)
        return;

    _materialBindings.resize(desired);

    const string defaultKey = String::ToLowerCopy(String::ToString(fs::path(_sourceModelPath).stem().wstring()));
    for (size_t i = 0; i < _materialBindings.size(); ++i)
    {
        if (_materialBindings[i].baseBundleKey.empty())
            _materialBindings[i].baseBundleKey = defaultKey;
    }
}

void ModelCom::Set_BaseBundleKey(uint32 materialIndex, const string& bundleKey)
{
    Ensure_MaterialBindings();
    if (materialIndex >= _materialBindings.size())
        return;

    _materialBindings[materialIndex].baseBundleKey = String::ToLowerCopy(bundleKey);
}

void ModelCom::Set_InstanceOverride(uint32 materialIndex, const MaterialInstanceOverride& overrideData)
{
    Ensure_MaterialBindings();
    if (materialIndex >= _materialBindings.size())
        return;

    _materialBindings[materialIndex].instance = overrideData;
}

void ModelCom::Clear_InstanceOverride(uint32 materialIndex)
{
    Ensure_MaterialBindings();
    if (materialIndex >= _materialBindings.size())
        return;

    _materialBindings[materialIndex].instance = MaterialInstanceOverride{};
}

static void Apply_TextureOverride(Engine::Material* material, Engine::MaterialTextureSlot slot, const Engine::TextureOverride& ov)
{
    if (material == nullptr || !ov.enabled)
        return;
    (void)material->Override_TextureSlot(slot, 0, ov.path);
}

void ModelCom::Apply_BaseMaterialFromBundle_AllSlots()
{
    Ensure_MaterialBindings();

    Asset_Manager* assetManager = GAME->Get_AssetManager();
    if (assetManager == nullptr)
        return;

    MaterialBuilder* builder = assetManager->Get_MaterialBuilder();
    if (builder == nullptr)
        return;

    for (uint32 i = 0; i < static_cast<uint32>(_materials.size()); ++i)
        Apply_EffectiveMaterial(i);
}

void ModelCom::Apply_EffectiveMaterial(uint32 materialIndex)
{
    Ensure_MaterialBindings();
    if (materialIndex >= _materials.size() || materialIndex >= _materialBindings.size())
        return;

    const Shared<Material> material = _materials[materialIndex];
    if (material == nullptr)
        return;

    Asset_Manager* assetManager = GAME->Get_AssetManager();
    if (assetManager == nullptr)
        return;

    MaterialBuilder* builder = assetManager->Get_MaterialBuilder();
    if (builder == nullptr)
        return;

    const MaterialBinding& binding = _materialBindings[materialIndex];
    if (!binding.baseBundleKey.empty())
    {
        const MaterialBuilder::BundleAsset* bundle = builder->GetOrLoadBundle(binding.baseBundleKey);
        if (bundle != nullptr)
        {
            const auto itName = bundle->materialIndexToAssetName.find(materialIndex);
            if (itName != bundle->materialIndexToAssetName.end())
            {
                const auto itInst = bundle->instancesByAssetName.find(itName->second);
                if (itInst != bundle->instancesByAssetName.end())
                {
                    const auto& base = itInst->second;

                    material->Set_RenderState(base.renderState);
                    material->Set_CB(base.cb);
                    material->Set_ForcedPassIndex(base.forcePassIndex);

                    if (base.baseColorOverride) (void)material->Override_TextureSlot(MaterialTextureSlot::BaseColor, 0, base.baseColorPath);
                    if (base.normalOverride) (void)material->Override_TextureSlot(MaterialTextureSlot::Normal, 0, base.normalPath);
                    if (base.ormOverride) (void)material->Override_TextureSlot(MaterialTextureSlot::ORM, 0, base.ormPath);
                    if (base.emissiveOverride) (void)material->Override_TextureSlot(MaterialTextureSlot::Emissive, 0, base.emissivePath);
                    if (base.hairDepthOverride) (void)material->Override_TextureSlot(MaterialTextureSlot::HairDepth, 0, base.hairDepthPath);
                    if (base.hairIDOverride) (void)material->Override_TextureSlot(MaterialTextureSlot::HairID, 0, base.hairIDPath);
                    if (base.irisHeightOverride) (void)material->Override_TextureSlot(MaterialTextureSlot::IrisHeight, 0, base.irisHeightPath);
                    if (base.irisMaskOverride) (void)material->Override_TextureSlot(MaterialTextureSlot::IrisMask, 0, base.irisMaskPath);

                    // opacity mask는 "없으면 지우기"가 현행 에디터 Apply 정책이라 동일하게 맞춘다(스테일 슬롯 방지).
                    if (base.opacityMaskOverride)
                        (void)material->Override_TextureSlot(MaterialTextureSlot::OpacityMask, 0, base.opacityMaskPath);
                    else
                        (void)material->Override_TextureSlot(MaterialTextureSlot::OpacityMask, 0, L"");
                }
            }
        }
    }

    const MaterialInstanceOverride& inst = binding.instance;
    if (inst.overrideRenderStateEnabled)
        material->Set_RenderState(inst.renderState);

    if (inst.cbMask.Any())
    {
        MaterialCB effective = material->Get_CB();

#define MODEL_COM_APPLY_CB_FIELD(_key, _member, _kind) \
        if (inst.cbMask._member) effective._member = inst.cb._member;
        MATERIAL_CB_OVERRIDE_FIELDS(MODEL_COM_APPLY_CB_FIELD)
#undef MODEL_COM_APPLY_CB_FIELD

        material->Set_CB(effective);
    }

    Apply_TextureOverride(material.get(), MaterialTextureSlot::BaseColor, inst.baseColor);
    Apply_TextureOverride(material.get(), MaterialTextureSlot::Normal, inst.normal);
    Apply_TextureOverride(material.get(), MaterialTextureSlot::ORM, inst.orm);
    Apply_TextureOverride(material.get(), MaterialTextureSlot::Emissive, inst.emissive);
    Apply_TextureOverride(material.get(), MaterialTextureSlot::OpacityMask, inst.opacityMask);
    Apply_TextureOverride(material.get(), MaterialTextureSlot::HairDepth, inst.hairDepth);
    Apply_TextureOverride(material.get(), MaterialTextureSlot::HairID, inst.hairID);
    Apply_TextureOverride(material.get(), MaterialTextureSlot::IrisHeight, inst.irisHeight);
    Apply_TextureOverride(material.get(), MaterialTextureSlot::IrisMask, inst.irisMask);
}

json ModelCom::To_Json() const
{
    json root = __super::To_Json();

    if (!_sourceModelPath.empty())
    {
        const string modelGuid = GAME->Ensure_AssetGUID(_sourceModelPath, "Model");
        if (!modelGuid.empty())
            root["model_guid"] = modelGuid;
    }

    root["model_type"] = To_ModelTypeString(_type);

    // NOTE:
    // 기존 `material_overrides`는 Runtime Material의 isOverride 플래그 기반이라,
    // 번들(base) 적용 시 "자산 값"까지 override로 저장되는 문제가 생길 수 있다.
    // 따라서 v2부터는 `material_bindings`(base+instance)만 저장한다.
    const string defaultKey = String::ToLowerCopy(String::ToString(fs::path(_sourceModelPath).stem().wstring()));

    json materialBindings = json::array();
    const uint32 numBindings = static_cast<uint32>(std::min(_materials.size(), _materialBindings.size()));
    for (uint32 materialIndex = 0; materialIndex < numBindings; ++materialIndex)
    {
        const MaterialBinding& binding = _materialBindings[materialIndex];

        const MaterialInstanceOverride& inst = binding.instance;
        const bool hasAnyTextureOverride =
            inst.baseColor.enabled
            || inst.normal.enabled
            || inst.orm.enabled
            || inst.emissive.enabled
            || inst.opacityMask.enabled
            || inst.hairDepth.enabled       // Hair 전용 추가
            || inst.hairID.enabled
            || inst.irisHeight.enabled
            || inst.irisMask.enabled;

        const bool hasInstanceOverride =
            inst.overrideRenderStateEnabled || inst.cbMask.Any() || hasAnyTextureOverride;

        const string baseKey = String::ToLowerCopy(binding.baseBundleKey);
        const bool hasBaseKey = !baseKey.empty() && baseKey != defaultKey;

        if (!hasBaseKey && !hasInstanceOverride)
            continue;

        json entry{};
        entry["material_index"] = materialIndex;
        if (hasBaseKey)
            entry["base_bundle_key"] = baseKey;

        if (hasInstanceOverride)
        {
            json instJson{};
            instJson["override_render_state_enabled"] = inst.overrideRenderStateEnabled;

            if (inst.overrideRenderStateEnabled)
            {
                json rs{};
                rs["twoSided"] = inst.renderState.twoSided;
                rs["blendMode"] = static_cast<uint32>(inst.renderState.blendMode);
                rs["opacityMaskClipValue"] = inst.renderState.opacityMaskClipValue;
                rs["alphaClip"] = inst.renderState.alphaClip;
                instJson["render_state"] = rs;
            }

            if (inst.cbMask.Any())
            {
                json jmask = json::object();

#define MODEL_COM_WRITE_CB_MASK(_key, _member, _kind) \
                if (inst.cbMask._member) jmask[_key] = true;
                MATERIAL_CB_OVERRIDE_FIELDS(MODEL_COM_WRITE_CB_MASK)
#undef MODEL_COM_WRITE_CB_MASK

                instJson["cb_overrides"] = jmask;

                json cb{};

                if (inst.cbMask.baseColorTint)
                    cb["baseColorTint"] = { inst.cb.baseColorTint.x, inst.cb.baseColorTint.y, inst.cb.baseColorTint.z, inst.cb.baseColorTint.w };
                if (inst.cbMask.emissiveColor)
                    cb["emissiveColor"] = { inst.cb.emissiveColor.x, inst.cb.emissiveColor.y, inst.cb.emissiveColor.z, inst.cb.emissiveColor.w };

                if (inst.cbMask.emissiveIntensity) cb["emissiveIntensity"] = inst.cb.emissiveIntensity;
                if (inst.cbMask.emissiveMaskMul) cb["emissiveMaskMul"] = inst.cb.emissiveMaskMul;
                if (inst.cbMask.normalStrength) cb["normalStrength"] = inst.cb.normalStrength;
                if (inst.cbMask.opacityMaskClipValue) cb["opacityMaskClipValue"] = inst.cb.opacityMaskClipValue;
                if (inst.cbMask.roughnessScale) cb["roughnessScale"] = inst.cb.roughnessScale;
                if (inst.cbMask.metalnessScale) cb["metalnessScale"] = inst.cb.metalnessScale;
                if (inst.cbMask.aoScale) cb["aoScale"] = inst.cb.aoScale;

                if (inst.cbMask.uvScale) cb["uvScale"] = { inst.cb.uvScale.x, inst.cb.uvScale.y };
                if (inst.cbMask.uvOffset) cb["uvOffset"] = { inst.cb.uvOffset.x, inst.cb.uvOffset.y };
                if (inst.cbMask.uvRotation) cb["uvRotation"] = inst.cb.uvRotation;
                if (inst.cbMask.materialMask) cb["materialMask"] = inst.cb.materialMask;

                instJson["cb"] = cb;
            }

            auto writeTextureOverride = [&](const char* name, const TextureOverride& ov) -> void
            {
                if (!ov.enabled)
                    return;

                json tex{};
                tex["enabled"] = true;

                if (!ov.path.empty())
                {
                    const string textureGuid = GAME->Ensure_AssetGUID(ov.path, "Texture");
                    if (!textureGuid.empty())
                        tex["texture_guid"] = textureGuid;
                    else
                        tex["texture_path"] = String::ToString(ov.path);
                }
                else
                {
                    // enabled + empty path = clear slot
                    tex["texture_path"] = string{};
                }

                if (!instJson.contains("textures"))
                    instJson["textures"] = json::object();
                instJson["textures"][name] = tex;
            };

            writeTextureOverride("base_color", inst.baseColor);
            writeTextureOverride("normal", inst.normal);
            writeTextureOverride("orm", inst.orm);
            writeTextureOverride("emissive", inst.emissive);
            writeTextureOverride("opacity_mask", inst.opacityMask);

            writeTextureOverride("hair_depth", inst.hairDepth);
            writeTextureOverride("hair_id", inst.hairID);
            writeTextureOverride("iris_height", inst.irisHeight);
            writeTextureOverride("iris_mask", inst.irisMask);

            entry["instance_override"] = instJson;
        }

        materialBindings.push_back(entry);
    }

    if (!materialBindings.empty())
        root["material_bindings"] = materialBindings;

    return root;
}

void ModelCom::From_Json(const json& data)
{
    __super::From_Json(data);

    if (!data.contains("model_guid") || !data["model_guid"].is_string())
    {
        Asset_Manager* assetManager = GAME->Get_AssetManager();
        MaterialBuilder* builder = assetManager ? assetManager->Get_MaterialBuilder() : nullptr;
        if (builder != nullptr)
        {
            builder->Try_ApplyMaterialBindingsFromJson(*this, data);
            builder->Try_ApplyModelMaterialOverridesFromJson(*this, data);
        }

        Apply_BaseMaterialFromBundle_AllSlots();
        return;
    }

    const string modelGuid = data["model_guid"].get<string>();
    if (modelGuid.empty())
    {
        Asset_Manager* assetManager = GAME->Get_AssetManager();
        MaterialBuilder* builder = assetManager ? assetManager->Get_MaterialBuilder() : nullptr;
        if (builder != nullptr)
        {
            builder->Try_ApplyMaterialBindingsFromJson(*this, data);
            builder->Try_ApplyModelMaterialOverridesFromJson(*this, data);
        }

        Apply_BaseMaterialFromBundle_AllSlots();
        return;
    }

    const string modelTypeString = data.value("model_type", To_ModelTypeString(_type));
    const ModelType modelType = Parse_ModelType(modelTypeString);

    const string currentGuid = _sourceModelPath.empty()
        ? string{}
        : GAME->Find_AssetGUID(_sourceModelPath);

    if (modelGuid == currentGuid && modelType == _type)
    {
        Asset_Manager* assetManager = GAME->Get_AssetManager();
        MaterialBuilder* builder = assetManager ? assetManager->Get_MaterialBuilder() : nullptr;
        if (builder != nullptr)
        {
            builder->Try_ApplyMaterialBindingsFromJson(*this, data);
            builder->Try_ApplyModelMaterialOverridesFromJson(*this, data);
        }

        Apply_BaseMaterialFromBundle_AllSlots();
        return;
    }

    if (FAILED(Reload_ModelFromGuid(modelGuid, modelType)))
        LOG_WARN("ModelCom::From_Json - failed to reload model. guid={}", modelGuid);

    Asset_Manager* assetManager = GAME->Get_AssetManager();
    MaterialBuilder* builder = assetManager ? assetManager->Get_MaterialBuilder() : nullptr;
    if (builder != nullptr)
    {
        builder->Try_ApplyMaterialBindingsFromJson(*this, data);
        builder->Try_ApplyModelMaterialOverridesFromJson(*this, data);
    }

    Apply_BaseMaterialFromBundle_AllSlots();

    Ensure_AnimStateAssetLoaded(); // 애니메이션은 Prefab기반 로드 싱크를 여기다 맞춤
}

HRESULT ModelCom::Reload_ModelFromGuid(const string& guid, ModelType modelType)
{
    const wstring resolvedPath = GAME->Resolve_AssetPath(guid);
    if (resolvedPath.empty())
        return E_FAIL;

    const fs::path modelPath = fs::path(resolvedPath).lexically_normal();
    if (!fs::exists(modelPath) || !fs::is_regular_file(modelPath))
        return E_FAIL;

    Reset_ModelData();

    _type = modelType;
    _sourceModelPath = modelPath.wstring();

    const string sourcePathUtf8 = String::ToString(_sourceModelPath);
    CHECK_FAILED(Ready_Model(sourcePathUtf8.c_str()), E_FAIL);
    CHECK_FAILED(Ready_Animation(sourcePathUtf8.c_str()), E_FAIL);

    return S_OK;
}

HRESULT ModelCom::Render(uint32 meshIndex)
{
    if (meshIndex >= _numMeshes)
    {
        LOG_ERROR("[MODEL] Mesh Render Index Out of Range!");
        return E_FAIL;
    }

    _meshes[meshIndex]->Bind_Resources();
    _meshes[meshIndex]->Render();

    return S_OK;
}

HRESULT ModelCom::RenderInstanced(uint32 meshIndex, Buffer* instanceVB, uint32 instanceStride, uint32 instanceCount)
{
    if (meshIndex >= _numMeshes || !instanceVB || instanceCount == 0)
    {
        return E_FAIL;
    }

    return _meshes[meshIndex]->RenderInstanced(instanceVB, instanceStride, instanceCount);
}

bool ModelCom::Play_Animation(float timeDelta)
{
    float adjustedTimeDelta = timeDelta * _frameSpeed;

    if (!_isSkeletal || _animations.empty()) return false;

    if (_isAnimationPaused)
    {
        Refresh_CurrentAnimationPose();
        return false;
    }

    const float previousTimeSec = Get_CurrentAnimationTimeSec();

    int32 rootIndex = _isRootMotion ? _rootNodeIndex : -1;

    if (_isBlending)
    {
        _blendElapsedTime += adjustedTimeDelta;
        float ratio = _blendElapsedTime / _blendDuration;

        if (ratio >= 1.f)
        {
            _isBlending = false;
            _currentAnimIndex = _nextAnimIndex;
            _currentAnimStateIndex = _nextAnimStateIndex;
            _isFinished = _animations[_currentAnimIndex]->Update_TransformationMatrices(adjustedTimeDelta, _bones, _isAnimLoop, rootIndex);
            _rootOriginalFrameVelocity = rootIndex >= 0 ? _animations[_currentAnimIndex]->Get_TransformVelocity(rootIndex) : RootMotionFrame{};
        }
        else
        {
            _animations[_currentAnimIndex]->Blend_TransformMatrix(adjustedTimeDelta, _animations[_nextAnimIndex], ratio, _bones, _isAnimLoop, rootIndex);

            if (rootIndex >= 0)
            {
                const auto& curDelta = _animations[_currentAnimIndex]->Get_TransformVelocity(rootIndex);
                const auto& nextDelta = _animations[_nextAnimIndex]->Get_TransformVelocity(rootIndex);
                _rootOriginalFrameVelocity.position = Vector3::Lerp(curDelta.position, nextDelta.position, ratio);
                _rootOriginalFrameVelocity.rotation = Quaternion::Slerp(curDelta.rotation, nextDelta.rotation, ratio);
            }
        }
    }
    else
    {
        _isFinished = _animations[_currentAnimIndex]->Update_TransformationMatrices(adjustedTimeDelta, _bones, _isAnimLoop, rootIndex);
        _rootOriginalFrameVelocity = (rootIndex >= 0) ? _animations[_currentAnimIndex]->Get_TransformVelocity(rootIndex) : RootMotionFrame{};
    }

    if (rootIndex >= 0)
    {
        _rootOriginalFrameVelocity.position = Vec3::TransformNormal(_rootOriginalFrameVelocity.position, _preLocalTransformMatrix);
        auto temp = _rootOriginalFrameVelocity.position.z;
        _rootOriginalFrameVelocity.position.z = -_rootOriginalFrameVelocity.position.y;
        _rootOriginalFrameVelocity.position.y = temp;
    }

    for (uint32 i = 0; i < _numBones; ++i)
    {
        _bones[i]->Update_CombinedTransformationMatrix(_bones, _preLocalTransformMatrix);
    }

    Mark_BoneMatricesDirty();

    const float currentTimeSec = Get_CurrentAnimationTimeSec();
    const bool wrapped = (_isAnimLoop && currentTimeSec < previousTimeSec);

    if (Ensure_AnimNotifyAssetLoaded())
    {
        if (const AnimNotifyData* animationData = Find_CurrentNotifyAnimation())
        {
            AnimNotifyContext context{};
            context.owner = Get_Owner().get();
            context.model = this;
            context.modelGuid = _animNotifyAsset.modelGuid;
            context.animationName = Get_AnimationName(_currentAnimIndex) ? Get_AnimationName(_currentAnimIndex) : "";
            context.previousTimeSec = previousTimeSec;
            context.currentTimeSec = currentTimeSec;
            context.deltaTime = adjustedTimeDelta;
            context.isLooping = _isAnimLoop;
            context.wrapped = wrapped;
            context.isPreview = !_isAnimNotifyExecutionEnabled;

            Update_AnimNotifies(*animationData, context, _isAnimNotifyExecutionEnabled);
            Update_AnimNotifyStates(*animationData, context, _isAnimNotifyExecutionEnabled);
        }
    }

    if (_isFinished && !_isAnimLoop)
        Stop_AllNotifyStates(_isAnimNotifyExecutionEnabled);

    return _isFinished;
}

HRESULT ModelCom::Bind_Material(ShaderCom* shader, const char* constantName, uint32 meshIndex, MaterialTextureSlot slot, uint32 textureIndex)
{
    return _materials[_meshes[meshIndex]->Get_MaterialIndex()]->Bind_Material(shader, constantName, slot, textureIndex);
}

HRESULT ModelCom::Bind_BoneMatrices(ShaderCom* shader, const char* constantName, uint32 meshIndex)
{
    if (shader == nullptr || constantName == nullptr)
        return E_FAIL;

    if (!_isSkeletal || _numBones == 0)
        return S_OK;

    if (meshIndex >= _meshes.size())
        return E_FAIL;

    if (_leaderPoseModel != nullptr)
    {
        if (!_leaderPoseMatricesValid)
            Update_LeaderPoseMatrices();

        if (_leaderPoseCombinedMatrices.empty())
            return E_FAIL;
    }

    CHECK_FAILED(Ensure_MeshBoneMatrixBuffers(), E_FAIL);

    if (meshIndex >= _meshBoneMatrixBuffers.size())
        return E_FAIL;

    const Shared<ComputeStructuredBuffer>& meshBoneMatrixBuffer = _meshBoneMatrixBuffers[meshIndex];

    if (meshBoneMatrixBuffer == nullptr)
        return S_OK;

    return shader->Bind_SRV(constantName, meshBoneMatrixBuffer->Get_SRV());
}

bool ModelCom::Try_Get_LocalBounds(Vec3& outMin, Vec3& outMax) const
{
    if (!_hasLocalBounds)
        return false;

    outMin = _localBoundsMin;
    outMax = _localBoundsMax;
    return true;
}

void ModelCom::Apply_MaterialByAssetName(uint32 materialIndex, const string& bundleKey, const string& assetName)
{
    if (materialIndex >= _materials.size())
    {
        return;
    }

    if (_appliedMaterialAssets.size() < _materials.size())
        _appliedMaterialAssets.resize(_materials.size());

    const string normalizedBundleKey = String::ToLowerCopy(bundleKey);

    {
        AppliedMaterialAsset& applied = _appliedMaterialAssets[materialIndex];

        if (applied.bundleKey == normalizedBundleKey &&
            applied.assetName == assetName)
        {
            // 같은 모델 슬롯에 같은 material instance를 다시 꽂는 경우는 전체 override 비용을 스킵
            return;
        }
    }

    const Shared<Material> material = _materials[materialIndex];
    if (!material)
    {
        return;
    }

    Asset_Manager* assetManager = GAME->Get_AssetManager();
    if (!assetManager)
    {
        return;
    }

    MaterialBuilder* builder = assetManager->Get_MaterialBuilder();
    if (!builder)
    {
        return;
    }

    const MaterialBuilder::BundleAsset* bundle = builder->GetOrLoadBundle(normalizedBundleKey);
    if (!bundle)
    {
        return;
    }

    auto it = bundle->instancesByAssetName.find(assetName);
    if (it == bundle->instancesByAssetName.end())
    {
        return;
    }

    const auto& base = it->second;
    material->Set_RenderState(base.renderState);
    material->Set_CB(base.cb);
    material->Set_ForcedPassIndex(base.forcePassIndex);

    if (base.baseColorOverride)
    {
        material->Override_TextureSlot(MaterialTextureSlot::BaseColor, 0, base.baseColorPath);
    }

    if (base.normalOverride)
    {
        material->Override_TextureSlot(MaterialTextureSlot::Normal, 0, base.normalPath);
    }

    if (base.ormOverride)
    {
        material->Override_TextureSlot(MaterialTextureSlot::ORM, 0, base.ormPath);
    }

    if (base.emissiveOverride)
    {
        material->Override_TextureSlot(MaterialTextureSlot::Emissive, 0, base.emissivePath);
    }

    if (base.opacityMaskOverride)
    {
        material->Override_TextureSlot(MaterialTextureSlot::OpacityMask, 0, base.opacityMaskPath);
    }

    else
    {
        material->Override_TextureSlot(MaterialTextureSlot::OpacityMask, 0, L"");
    }

    // 실제 적용이 끝난 뒤에만 기록해서 실패/미존재 material은 다음 호출에서 다시 시도 가능하게 둔다.
    AppliedMaterialAsset& applied = _appliedMaterialAssets[materialIndex];
    applied.bundleKey = normalizedBundleKey;
    applied.assetName = assetName;
}

wstring ModelCom::Normalize_ModelAssetPath(const char* modelFilePath)
{
    string resolvedPath = modelFilePath ? modelFilePath : "";

    if (const size_t extPos = resolvedPath.rfind(".fbx"); extPos != string::npos)
        resolvedPath = resolvedPath.substr(0, extPos) + ".model";

    return fs::absolute(fs::path(String::ToWString(resolvedPath))).lexically_normal().wstring();
}

uint32 ModelCom::Get_AnimationIndex() const
{
    return _isBlending ? _nextAnimIndex : _currentAnimIndex;
}

uint32 ModelCom::Get_AnimationStateIndex() const
{
    uint32 activeState = _isBlending ? _nextAnimStateIndex : _currentAnimStateIndex;
    if (activeState != UINT32_MAX)
        return activeState;

    return _isBlending ? _nextAnimIndex : _currentAnimIndex;
}

float ModelCom::Get_CurrentAnimationDurationSec() const
{
    if (_currentAnimIndex >= _animations.size())
        return 0.f;

    return _animations[_currentAnimIndex]->Get_DurationSec();
}

float ModelCom::Get_CurrentAnimationTimeSec() const
{
    if (_currentAnimIndex >= _animations.size())
        return 0.f;

    return _animations[_currentAnimIndex]->Get_CurrentTimeSec();
}

void ModelCom::Set_AnimationPaused(bool paused)
{
    _isAnimationPaused = paused;
    Mark_BoneMatricesDirty();
}

void ModelCom::Set_CurrentAnimationTimeSec(float timeSec)
{
    if (_currentAnimIndex >= _animations.size())
        return;

    _animations[_currentAnimIndex]->Set_CurrentTimeSec(timeSec);

    Mark_BoneMatricesDirty();
}

void ModelCom::Refresh_CurrentAnimationPose()
{
    if (!_isSkeletal || _currentAnimIndex >= _animations.size())
        return;

    const int32 rootIndex = _isRootMotion ? _rootNodeIndex : -1;

    _animations[_currentAnimIndex]->Evaluate_CurrentPose(_bones, rootIndex, _isAnimLoop);
    _rootOriginalFrameVelocity = (rootIndex >= 0) ? _animations[_currentAnimIndex]->Get_TransformVelocity(rootIndex) : RootMotionFrame{};

    for (uint32 i = 0; i < _numBones; ++i)
        _bones[i]->Update_CombinedTransformationMatrix(_bones, _preLocalTransformMatrix);

    Mark_BoneMatricesDirty();
}

bool ModelCom::Ensure_AnimNotifyAssetLoaded()
{
     if (_isAnimNotifyAssetLoaded)
        return true;

    _animNotifyAsset = {};
    _activeNotifyStates.clear();

    if (_sourceModelPath.empty())
    {
        _isAnimNotifyAssetLoaded = true;
        return false;
    }

    const string modelGuid = GAME->Ensure_AssetGUID(_sourceModelPath, "Model");
    _animNotifyAsset.modelGuid = modelGuid;

    const fs::path filePath = AnimNotify_Parser::Get_ModelNotifyFilePath(modelGuid);
    if (!AnimNotify_Parser::Load_FromFile(filePath.wstring(), _animNotifyAsset))
    {
        const fs::path legacyFilePath = AnimNotify_Parser::Get_LegacyModelNotifyFilePath(modelGuid);
        AnimNotify_Parser::Load_FromFile(legacyFilePath.wstring(), _animNotifyAsset);
    }

    if (_animNotifyAsset.modelGuid.empty())
        _animNotifyAsset.modelGuid = modelGuid;

    _isAnimNotifyAssetLoaded = true;
    return true;
}

void ModelCom::Invalidate_AnimNotifyAsset()
{
    _isAnimNotifyAssetLoaded = false;
    _animNotifyAsset = {};
    _activeNotifyStates.clear();
}

const AnimNotifyData* ModelCom::Find_CurrentNotifyAnimation() const
{
    if (_currentAnimIndex >= _animations.size())
        return nullptr;

    const char* animationName = Get_AnimationName(_currentAnimIndex);
    if (animationName == nullptr)
    {
        return nullptr;
    }

    return AnimNotify_Parser::Find_Animation(_animNotifyAsset, animationName);
}

void ModelCom::Stop_AllNotifyStates(bool executeEndCheck)
{
    if (_activeNotifyStates.empty())
        return;

    if (executeEndCheck)
    {
        AnimNotifyContext context{};
        context.owner = Get_Owner().get();
        context.model = this;
        context.modelGuid = _animNotifyAsset.modelGuid;
        context.animationName = "";
        context.previousTimeSec = 0.f;
        context.currentTimeSec = 0.f;
        context.deltaTime = 0.f;
        context.isLooping = false;
        context.wrapped = false;
        context.isPreview = !_isAnimNotifyExecutionEnabled;

        for (auto& active : _activeNotifyStates)
        {
            if (active.notifyState)
                active.notifyState->On_End(context);
        }
    }

    _activeNotifyStates.clear();
}

void ModelCom::Stop_NotifyStates_ExceptAnimation(const string& keepAnimationName, bool executeEndCheck)
{
    if (_activeNotifyStates.empty())
        return;

    for (int32 i = static_cast<int32>(_activeNotifyStates.size()) - 1; i >= 0; --i)
    {
        if (_activeNotifyStates[i].animationName == keepAnimationName)
            continue;

        Shared<AnimNotifyState> instance = _activeNotifyStates[i].notifyState;
        _activeNotifyStates.erase(_activeNotifyStates.begin() + i);

        if (executeEndCheck && instance)
        {
            AnimNotifyContext context{};
            context.owner = Get_Owner().get();
            context.model = this;
            context.modelGuid = _animNotifyAsset.modelGuid;
            context.animationName = keepAnimationName;
            context.previousTimeSec = 0.f;
            context.currentTimeSec = 0.f;
            context.deltaTime = 0.f;
            context.isLooping = false;
            context.wrapped = false;
            context.isPreview = !_isAnimNotifyExecutionEnabled;

            instance->On_End(context);
        }
    }
}

void ModelCom::Update_AnimNotifies(const AnimNotifyData& animationData, const AnimNotifyContext& context,
    bool executeNotifies)
{
    for (const auto& entry : animationData.notifies)
    {
        if (entry.notify == nullptr)
            continue;

        // 시작 타이밍
        if (!Is_NotifyTimeInRange(entry.timeSec, context.previousTimeSec, context.currentTimeSec, context.wrapped))
            continue;

        if (executeNotifies)
        {
            entry.notify->Execute(context);
        }
    }
}

void ModelCom::Update_AnimNotifyStates(const AnimNotifyData& animationData, const AnimNotifyContext& context,
    bool executeNotifies)
{
    Stop_NotifyStates_ExceptAnimation(context.animationName, executeNotifies);

    for (int32 stateIndex = 0; stateIndex < static_cast<int32>(animationData.notifyStates.size()); ++stateIndex)
    {
        const auto& entry = animationData.notifyStates[stateIndex];
        if (entry.notifyState == nullptr)
            continue;

        const bool isActiveNow = Is_NotifyStateActiveTime(context.currentTimeSec, entry.startSec, entry.durationSec);
        const int32 activeIndex = Find_ActiveNotifyStateIndex(_activeNotifyStates, context.animationName, stateIndex);

        if (isActiveNow)
        {
            if (activeIndex == -1)
            {
                ActiveAnimNotifyState active{};
                active.animationName = context.animationName;
                active.stateIndex = stateIndex;
                active.startSec = entry.startSec;
                active.endSec = entry.startSec + entry.durationSec;
                active.lastTimeSec = context.currentTimeSec;
                active.notifyState = entry.notifyState;
                _activeNotifyStates.push_back(active);

                if (executeNotifies)
                    entry.notifyState->On_Begin(context);
            }
            else
            {
                const float previousActiveTimeSec = _activeNotifyStates[activeIndex].lastTimeSec;
                _activeNotifyStates[activeIndex].lastTimeSec = context.currentTimeSec;

                if (executeNotifies &&
                    (context.wrapped ||
                     context.deltaTime > 0.f ||
                     previousActiveTimeSec != context.currentTimeSec))
                {
                    _activeNotifyStates[activeIndex].notifyState->On_Tick(context);
                }
            }
        }
        else if (activeIndex != -1)
        {
            Shared<AnimNotifyState> instance = _activeNotifyStates[activeIndex].notifyState;
            _activeNotifyStates.erase(_activeNotifyStates.begin() + activeIndex);

            if (executeNotifies && instance)
                instance->On_End(context);
        }
    }
}

bool ModelCom::Is_NotifyTimeInRange(float targetTime, float previousTime, float currentTime, bool wrapped)
{
    if (!wrapped)
        return previousTime <= targetTime && targetTime <= currentTime;

    return targetTime >= previousTime || targetTime <= currentTime;
}

bool ModelCom::Is_NotifyStateActiveTime(float currentTime, float startTime, float duration)
{
    return startTime <= currentTime && currentTime <= (startTime + duration);
}

int32 ModelCom::Find_ActiveNotifyStateIndex(const vector<ActiveAnimNotifyState>& activeStates,
    const string& animationName, int32 stateIndex)
{
    for (int32 i = 0; i < static_cast<int32>(activeStates.size()); ++i)
    {
        if (activeStates[i].animationName == animationName &&
            activeStates[i].stateIndex == stateIndex)
        {
            return i;
        }
    }

    return -1;
}

void ModelCom::Set_LeaderPoseComponent(const ModelCom* leaderModel)
{
    _leaderPoseModel = leaderModel;
    _leaderPoseBoneMap.clear();
    _leaderPoseCombinedMatrices.clear();

    if (nullptr == _leaderPoseModel || !_isSkeletal || !_leaderPoseModel->Is_Skeletal())
        return;

    _leaderPoseBoneMap.resize(_numBones, -1);
    _leaderPoseCombinedMatrices.resize(_numBones, Matrix::Identity);

    for (uint32 i = 0; i < _numBones; ++i)
    {
        _leaderPoseBoneMap[i] =
            _leaderPoseModel->Get_BoneIndex(_bones[i]->Get_BoneName());
    }

    _leaderPoseMatricesValid = false;
    Update_LeaderPoseMatrices();
}

void ModelCom::Clear_LeaderPoseComponent()
{
    _leaderPoseModel = nullptr;
    _leaderPoseBoneMap.clear();
    _leaderPoseCombinedMatrices.clear();

    _leaderPoseMatricesValid = false;
    Mark_BoneMatricesDirty();
}

void ModelCom::Update_LeaderPoseMatrices()
{
    if (nullptr == _leaderPoseModel || _leaderPoseBoneMap.empty())
        return;

    vector<uint8> visited(_numBones, 0);

    for (uint32 i = 0; i < _numBones; ++i)
    {
        Build_LeaderPoseMatrix(i, visited);
    }

    _leaderPoseMatricesValid = true;
    Mark_BoneMatricesDirty();
}

void ModelCom::Mark_BoneMatricesDirty()
{
    _boneMatricesDirty = true;
    _boneMatrixBuffersDirty = true;
}

HRESULT ModelCom::Ensure_MeshBoneMatrices()
{
    if (!_isSkeletal || _numBones == 0)
        return S_OK;

    if (!_boneMatricesDirty)
        return S_OK;

    if (_meshBoneMatrixCaches.size() != _meshes.size())
        _meshBoneMatrixCaches.resize(_meshes.size());

    const vector<Matrix>* sourceMatrices = nullptr;
    if (_leaderPoseModel != nullptr)
    {
        sourceMatrices = &_leaderPoseCombinedMatrices;
    }
    else
    {
        _combinedBoneMatrixCache.resize(_numBones);

        for (uint32 i = 0; i < _numBones; ++i)
        {
            const Matrix* combined = _bones[i]->Get_CombinedTransformationMatrixPtr();
            _combinedBoneMatrixCache[i] = combined ? *combined : Matrix::Identity;
        }

        sourceMatrices = &_combinedBoneMatrixCache;
    }

    if (sourceMatrices == nullptr || sourceMatrices->empty())
        return E_FAIL;

    for (uint32 meshIndex = 0; meshIndex < _meshes.size(); ++meshIndex)
    {
        const Shared<Mesh>& mesh = _meshes[meshIndex];
        if (mesh == nullptr)
            return E_FAIL;

        CHECK_FAILED(
            mesh->Build_MeshBoneMatrices(*sourceMatrices, _meshBoneMatrixCaches[meshIndex]),
            E_FAIL);
    }

    _boneMatricesDirty = false;
    return S_OK;
}

Matrix ModelCom::Convert_LeaderPoseMatrix(const Matrix& leaderMatrix) const
{
    return leaderMatrix;
}

Matrix ModelCom::Build_LeaderPoseMatrix(uint32 boneIndex, vector<uint8>& visited)
{
    if (visited[boneIndex])
        return _leaderPoseCombinedMatrices[boneIndex];

    visited[boneIndex] = 1;

    const int32 leaderBoneIndex = _leaderPoseBoneMap[boneIndex];

    if (leaderBoneIndex >= 0)
    {
        _leaderPoseCombinedMatrices[boneIndex] =
            Convert_LeaderPoseMatrix(
                *_leaderPoseModel->Get_BoneMatrixPtr(static_cast<uint32>(leaderBoneIndex))
            );

        return _leaderPoseCombinedMatrices[boneIndex];
    }

    const int32 parentIndex = _bones[boneIndex]->Get_ParentIndex();

    if (parentIndex >= 0)
    {
        _leaderPoseCombinedMatrices[boneIndex] =
            _bones[boneIndex]->Get_TransformationMatrix() *
            Build_LeaderPoseMatrix(static_cast<uint32>(parentIndex), visited);
    }
    else
    {
        _leaderPoseCombinedMatrices[boneIndex] =
            _bones[boneIndex]->Get_TransformationMatrix() *
            _preLocalTransformMatrix;
    }

    return _leaderPoseCombinedMatrices[boneIndex];
}

HRESULT ModelCom::Ensure_MeshBoneMatrixBuffers()
{
    // CPU 쪽 mesh별 bone matrices cache가 최신인지 먼저 보장
    CHECK_FAILED(Ensure_MeshBoneMatrices(), E_FAIL);

    // GPU buffer가 이미 최신이면 이번 draw에서는 다시 업로드하지 않는다.
    if (!_boneMatrixBuffersDirty)
        return S_OK;

    // mesh 개수와 GPU buffer cache 개수를 맞춘다.
    if (_meshBoneMatrixBuffers.size() != _meshBoneMatrixCaches.size())
        _meshBoneMatrixBuffers.resize(_meshBoneMatrixCaches.size());

    for (uint32 meshIndex = 0; meshIndex < _meshBoneMatrixCaches.size(); ++meshIndex)
    {
        const vector<Matrix>& meshBoneMatrices = _meshBoneMatrixCaches[meshIndex];
        if (meshBoneMatrices.empty())
            continue;

        const uint32 matrixCount = static_cast<uint32>(meshBoneMatrices.size());

        // buffer가 없거나 현재 mesh의 bone matrix 개수를 담기 부족하면 새로 만든다.
        if (_meshBoneMatrixBuffers[meshIndex] == nullptr ||
            _meshBoneMatrixBuffers[meshIndex]->Get_ElementCount() < matrixCount)
        {
            _meshBoneMatrixBuffers[meshIndex] = ComputeStructuredBuffer::Create(
                _device, _context, sizeof(Matrix), matrixCount, meshBoneMatrices.data());

            if (_meshBoneMatrixBuffers[meshIndex] == nullptr)
                return E_FAIL;
        }
        else
        {
            // 기존 buffer가 충분하면 데이터만 갱신한다.
            CHECK_FAILED(
                _meshBoneMatrixBuffers[meshIndex]->Update_Data(
                    meshBoneMatrices.data(),
                    matrixCount),
                E_FAIL);
        }
    }

    // 모든 mesh GPU bone matrix buffer가 최신 상태임
    _boneMatrixBuffersDirty = false;
    return S_OK;
}

HRESULT ModelCom::Ready_Model(const char* modelPath)
{
    std::string resolvedPath(modelPath);
    {
        size_t extPos = resolvedPath.rfind(".fbx");
        if (extPos != std::string::npos)
            resolvedPath = resolvedPath.substr(0, extPos) + ".model";
    }

    // 모델 guid
    const string modelGuid = GAME->Ensure_AssetGUID(String::ToWString(resolvedPath), "Model");
    const AssetMeta* modelAssetMeta = modelGuid.empty()
        ? nullptr
        : GAME->Find_AssetByGUID(modelGuid);
    UNREFERENCED_PARAMETER(modelAssetMeta);

    std::ifstream stream(resolvedPath, ::ios_base::binary);

    //std::ifstream stream(modelPath, std::ios::binary);
    if (!stream.is_open()) return E_FAIL;

    stream.read(reinterpret_cast<char*>(&_isSkeletal), sizeof(bool));
    stream.read(reinterpret_cast<char*>(&_maxTextureType), sizeof(uint32));
    stream.read(reinterpret_cast<char*>(&_numMaterials), sizeof(uint32));
    stream.read(reinterpret_cast<char*>(&_numMeshes), sizeof(uint32));
    stream.read(reinterpret_cast<char*>(&_numBones), sizeof(uint32));

    auto ReadString = [](std::ifstream& in) {
        uint32 len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(uint32));
        std::string str;
        if (len > 0) {
            str.resize(len);
            in.read(&str[0], len);
        }
        return str;
    };

    Vec3 minBounds(
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max());
    Vec3 maxBounds(
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max());
    bool hasBounds = false;

    // Load Material
    for (uint32 i = 0; i < _numMaterials; ++i)
    {
        ModelMaterialData materialData{};
        materialData.name = ReadString(stream);
        stream.read(reinterpret_cast<char*>(&materialData.diffuseColor), sizeof(Vec4) * 4);
        stream.read(reinterpret_cast<char*>(&materialData.shininess), sizeof(float) * 4);
        stream.read(reinterpret_cast<char*>(&materialData.numSlots), sizeof(uint32));

        vector<ModelTextureSlot> slots(materialData.numSlots);
        for (uint32 j = 0; j < materialData.numSlots; ++j)
        {
            slots[j].path = ReadString(stream);
            stream.read(reinterpret_cast<char*>(&slots[j].typeIndex), sizeof(uint32) * 2);
            stream.read(reinterpret_cast<char*>(&slots[j].uvScale), sizeof(Vec2) * 2);
            stream.read(reinterpret_cast<char*>(&slots[j].uvRotation), sizeof(float) * 2);
            stream.read(reinterpret_cast<char*>(&slots[j].blendOp), sizeof(uint32) * 3);
        }

        _materials.push_back(Material::Create(_device, _context, modelPath, materialData, slots, _maxTextureType));
    }

    // Load Mesh
    for (uint32 i = 0; i < _numMeshes; ++i)
    {
        ModelMeshData meshData;
        meshData.name = ReadString(stream);
        stream.read(reinterpret_cast<char*>(&meshData.materialIndex), sizeof(uint32) * 4);

        size_t vtxSize = _isSkeletal ? sizeof(VTXANIMMESH) : sizeof(VTXMESH);
        vector<uint8> vtxBuffer(vtxSize * meshData.numVertices);
        stream.read(reinterpret_cast<char*>(vtxBuffer.data()), vtxBuffer.size());
        meshData.vertices = vtxBuffer.data();

        if (meshData.numVertices > 0)
        {
            if (_isSkeletal)
            {
                VTXANIMMESH* vertices = reinterpret_cast<VTXANIMMESH*>(vtxBuffer.data());
                for (uint32 vertexIndex = 0; vertexIndex < meshData.numVertices; ++vertexIndex)
                {
                    const Vec3& position = vertices[vertexIndex].position;
                    minBounds.x = (std::min)(minBounds.x, position.x);
                    minBounds.y = (std::min)(minBounds.y, position.y);
                    minBounds.z = (std::min)(minBounds.z, position.z);
                    maxBounds.x = (std::max)(maxBounds.x, position.x);
                    maxBounds.y = (std::max)(maxBounds.y, position.y);
                    maxBounds.z = (std::max)(maxBounds.z, position.z);
                }
            }
            else
            {
                VTXMESH* vertices = reinterpret_cast<VTXMESH*>(vtxBuffer.data());
                for (uint32 vertexIndex = 0; vertexIndex < meshData.numVertices; ++vertexIndex)
                {
                    const Vec3& position = vertices[vertexIndex].position;
                    minBounds.x = (std::min)(minBounds.x, position.x);
                    minBounds.y = (std::min)(minBounds.y, position.y);
                    minBounds.z = (std::min)(minBounds.z, position.z);
                    maxBounds.x = (std::max)(maxBounds.x, position.x);
                    maxBounds.y = (std::max)(maxBounds.y, position.y);
                    maxBounds.z = (std::max)(maxBounds.z, position.z);
                }
            }

            hasBounds = true;
        }

        vector<uint32> idxBuffer(meshData.numIndices);
        stream.read(reinterpret_cast<char*>(idxBuffer.data()), sizeof(uint32) * meshData.numIndices);
        meshData.indices = idxBuffer.data();

        // StaticMesh용 PhysX collision CPU 데이터 보관
        if (!_isSkeletal && meshData.numVertices > 0 && meshData.numIndices >= 3)
        {
            VTXMESH* vertices = reinterpret_cast<VTXMESH*>(vtxBuffer.data());

            ModelCollisionMesh collisionMesh{};
            collisionMesh.name = meshData.name;
            collisionMesh.positions.reserve(meshData.numVertices);
            collisionMesh.indices.reserve(meshData.numIndices);

            for (uint32 vertexIndex = 0; vertexIndex < meshData.numVertices; ++vertexIndex)
                collisionMesh.positions.push_back(vertices[vertexIndex].position);

            for (uint32 index : idxBuffer)
                collisionMesh.indices.push_back(index);

            _collisionMeshes.push_back(std::move(collisionMesh));
        }

        vector<uint32> boneRefs;
        vector<Matrix> offsets;

        if (_isSkeletal)
        {
            if (meshData.numBoneRefs > 0)
            {
                boneRefs.resize(meshData.numBoneRefs);
                offsets.resize(meshData.numBoneRefs);
                stream.read(reinterpret_cast<char*>(boneRefs.data()), sizeof(uint32) * meshData.numBoneRefs);
                stream.read(reinterpret_cast<char*>(offsets.data()), sizeof(Matrix) * meshData.numBoneRefs);

                meshData.boneIndices = boneRefs.data();
                meshData.offsetMatrices = offsets.data();
            }
            else
            {
                meshData.boneIndices = nullptr;
                meshData.offsetMatrices = nullptr;
            }

            _meshes.push_back(SkeletalMesh::Create(_device, _context, meshData, _preLocalTransformMatrix));
            _type = ModelType::Anim;
        }
        else
        {
            meshData.boneIndices = nullptr;
            meshData.offsetMatrices = nullptr;
            _meshes.push_back(StaticMesh::Create(_device, _context, meshData, _preLocalTransformMatrix));
            _type = ModelType::NonAnim;
        }
    }

    // Load Bone
    if (_numBones > 0)
    {
        for (uint32 i = 0; i < _numBones; ++i)
        {
            ModelBoneData data{};
            data.name = ReadString(stream);
            stream.read(reinterpret_cast<char*>(&data.parentBoneIndex), sizeof(int32));
            stream.read(reinterpret_cast<char*>(&data.localTransform), sizeof(Matrix));
            stream.read(reinterpret_cast<char*>(&data.offsetMatrix), sizeof(Matrix));
            _bones.push_back(Bone::Create(data));
        }

        /* 바인드 포즈 초기화: 애니메이션 없이도 Combined 행렬이 유효하도록 1회 계산 */
        for (uint32 i = 0; i < _numBones; ++i)
        {
            _bones[i]->Update_CombinedTransformationMatrix(_bones, _preLocalTransformMatrix);
        }
    }

    _combinedBoneMatrixCache.resize(_numBones, Matrix::Identity);
    _meshBoneMatrixCaches.resize(_meshes.size());
    _meshBoneMatrixBuffers.resize(_meshes.size());
    Mark_BoneMatricesDirty();
    _leaderPoseMatricesValid = false;

    _hasLocalBounds = hasBounds;
    if (hasBounds)
    {
        _localBoundsMin = minBounds;
        _localBoundsMax = maxBounds;
    }

    stream.close();
    return S_OK;
}


HRESULT ModelCom::Ready_Animation(const char* modelPath)
{
    fs::path path(modelPath);
    string modelDirectory = path.parent_path().string();
    string modelName = path.stem().string();

    uint32 trueAnimIndex{ 0 };
    for (const auto& entry : fs::recursive_directory_iterator(modelDirectory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".anim")
        {
            std::ifstream stream(entry.path(), std::ios::binary);
            if (!stream.is_open()) continue;

            auto ReadString = [](std::ifstream& in) {
                uint32 len = 0;
                in.read(reinterpret_cast<char*>(&len), sizeof(uint32));
                std::string str;
                if (len > 0) {
                    str.resize(len);
                    in.read(&str[0], len);
                }
                return str;
            };

            ModelAnimationData animData{};
            animData.name = ReadString(stream);
            stream.read(reinterpret_cast<char*>(&animData.duration), sizeof(double) * 2);
            stream.read(reinterpret_cast<char*>(&animData.numChannels), sizeof(uint32));
            stream.read(reinterpret_cast<char*>(&animData.rootTotalTranslation), sizeof(Vec3));
            stream.read(reinterpret_cast<char*>(&animData.rootTotalRotation), sizeof(Vec4));

            vector<ModelChannelData> channels(animData.numChannels);
            vector<vector<KEYFRAME>> keyframeSets(animData.numChannels);

            for (uint32 i = 0; i < animData.numChannels; ++i)
            {
                channels[i].boneName = ReadString(stream);
                stream.read(reinterpret_cast<char*>(&channels[i].boneIndex), sizeof(int32));
                stream.read(reinterpret_cast<char*>(&channels[i].numKeyframes), sizeof(uint32));

                channels[i].boneIndex = Get_BoneIndex(channels[i].boneName);

                keyframeSets[i].resize(channels[i].numKeyframes);
                stream.read(reinterpret_cast<char*>(keyframeSets[i].data()), sizeof(KEYFRAME) * channels[i].numKeyframes);
                channels[i].keyframes = keyframeSets[i].data();
            }

            string animName = entry.path().stem().string();
            _mappedAnimIndices[animName] = trueAnimIndex;

            _animations.push_back(Animation::Create(animData, channels));
            stream.close();
            ++trueAnimIndex;
        }
    }

    _numAnimations = trueAnimIndex;

    Ensure_AnimStateAssetLoaded();

    return S_OK;
}

bool ModelCom::Ensure_AnimStateAssetLoaded()
{
    return AnimState_Parser::Load_StateAnimations(_sourceModelPath, _animations, _stateAnimIndices, &_animEnumType);
}

void ModelCom::Reset_ModelData()
{
    Stop_AllNotifyStates(false);

    _isSkeletal = false;
    _hasLocalBounds = false;
    _localBoundsMin = {};
    _localBoundsMax = {};
    _maxTextureType = 0;
    _numMeshes = 0;
    _numMaterials = 0;
    _numBones = 0;
    _numAnimations = 0;

    _meshes.clear();
    _materials.clear();
    _bones.clear();
    _animations.clear();
    _stateAnimIndices.clear();
    _mappedAnimIndices.clear();
    _collisionMeshes.clear();

    _isFinished = false;
    _isAnimLoop = false;
    _isBlending = false;
    _currentAnimIndex = 0;
    _nextAnimIndex = 0;
    _blendDuration = 0.f;
    _blendElapsedTime = 0.f;

    _isRootMotion = false;
    _rootNodeIndex = -1;
    _rootOriginalFrameVelocity = {};
    _rootFrameVelocity = {};

    _combinedBoneMatrixCache.clear();
    _meshBoneMatrixCaches.clear();
    _meshBoneMatrixBuffers.clear();
    _boneMatricesDirty = true;
    _boneMatrixBuffersDirty = true;
    _leaderPoseMatricesValid = false;

    Invalidate_AnimNotifyAsset();
    Clear_LeaderPoseComponent();
}

ModelType ModelCom::Parse_ModelType(const string& modelType)
{
    if (modelType == "Anim" || modelType == "ANIM" || modelType == "SkeletalMesh")
        return ModelType::Anim;

    if (modelType == "NonAnim" || modelType == "NONANIM" || modelType == "StaticMesh")
        return ModelType::NonAnim;

    return ModelType::NonAnim;
}

string ModelCom::To_ModelTypeString(ModelType modelType)
{
    return modelType == ModelType::Anim ? "Anim" : "NonAnim";
}

Shared<ModelCom> ModelCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, ModelType type, const char* modelFilePath, const Matrix& preLocalTransformMatrix)
{
    auto instance = make_shared<ModelCom>(device, context);

    if (FAILED(instance->Initialize_Prototype(type, modelFilePath, preLocalTransformMatrix)))
    {
        LOG_WARN("Failed to create ModelCom: path='{}'", nullptr != modelFilePath ? modelFilePath : "");
        return nullptr;
    }
    return instance;
}

Shared<Component> ModelCom::Clone(void* arg)
{
    auto instance = make_shared<ModelCom>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : ModelCom");
        return nullptr;
    }
    return instance;
}

void ModelCom::Free()
{
    __super::Free();

    _meshBoneMatrixBuffers.clear();
    _meshBoneMatrixCaches.clear();
    _combinedBoneMatrixCache.clear();

    _mappedAnimIndices.clear();
    _stateAnimIndices.clear();
    _animations.clear();
    _materials.clear();
    _meshes.clear();
    _bones.clear();
}



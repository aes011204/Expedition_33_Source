#include "pch.h"
#include "ModelCom.h"
#include "GameInstance.h"
#include "Asset_Manager.h"
#include "MaterialBuilder.h"
#include "Mesh.h"
#include "Bone.h"
#include "ShaderCom.h"
#include "Material.h"
#include "Animation.h"
#include "AnimationModifier.h"
#include "SkeletalMesh.h"
#include "StaticMesh.h"
#include "ComputeStructuredBuffer.h"
#include "DebugMemoryTracker.h"
#include "Debug_Profile.h"

#include "AnimNotify_Parser.h"
#include "AnimNotify.h"
#include "AnimNotifyState.h"
#include "AnimState_Parser.h"
#include "ComputeShaderCom.h"
#include "GameObject.h"
#include "ModelDeform.h"

ModelCom::ModelCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component{ device, context }
{
}

ModelCom::ModelCom(const ModelCom& prototype)
    : Component{ prototype }
    , _type{ prototype._type }
    , _preLocalTransformMatrix{ prototype._preLocalTransformMatrix }
    , _preLocalRotationMatrix{ prototype._preLocalRotationMatrix }
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

    _boneMatricesDirty = true;

    Mark_BoneMatricesDirty();

    _leaderPoseMatricesValid = false;

    if (prototype._modelDeform != nullptr)
    {
        if (const MorphData* morphData = prototype._modelDeform->Get_MorphData())
        {
            _modelDeform = ModelDeform::Create();
            if (_modelDeform != nullptr)
                _modelDeform->Set_MorphData(MorphData(*morphData));
        }
    }

    const size_t animationBytes = Get_DebugEstimatedAnimationBytes();
    if (animationBytes > 0)
    {
        const string modelCategory = DebugMemoryTracker::Guess_ModelCategory(_sourceModelPath);
        const string category = modelCategory + ".animation.clone";
        DebugMemoryTracker::Record_CPUBytes(
            category.c_str(),
            "ModelCom.Clone",
            _sourceModelPath,
            animationBytes);
    }
}

void ModelCom::Set_SuppressedAnimNotifyTypeNames(const vector<string>& typeNames)
{
    _suppressedAnimNotifyTypeNames.clear();

    for (const string& typeName : typeNames)
    {
        if (!typeName.empty())
            _suppressedAnimNotifyTypeNames.insert(typeName);
    }
}

vector<string> ModelCom::Get_SuppressedAnimNotifyTypeNames() const
{
    return { _suppressedAnimNotifyTypeNames.begin(), _suppressedAnimNotifyTypeNames.end() };
}

size_t ModelCom::Get_DebugEstimatedAnimationBytes() const
{
    size_t bytes = sizeof(Shared<Animation>) * _animations.capacity()
        + sizeof(uint32) * _stateAnimIndices.capacity();

    for (const Shared<Animation>& animation : _animations)
    {
        if (animation)
            bytes += animation->Get_DebugEstimatedBytes();
    }

    return bytes;
}

static json Write_PreLocalTransformJson(const Matrix& matrix)
{
    return json::array({
        matrix._11, matrix._12, matrix._13, matrix._14,
        matrix._21, matrix._22, matrix._23, matrix._24,
        matrix._31, matrix._32, matrix._33, matrix._34,
        matrix._41, matrix._42, matrix._43, matrix._44
        });
}

static bool Read_PreLocalTransformJson(const json& data, Matrix& outMatrix)
{
    if (!data.contains("pre_local_transform") || !data["pre_local_transform"].is_array())
        return false;

    const json& matrixJson = data["pre_local_transform"];
    if (matrixJson.size() != 16)
        return false;

    outMatrix = Matrix(
        matrixJson[0].get<float>(), matrixJson[1].get<float>(), matrixJson[2].get<float>(), matrixJson[3].get<float>(),
        matrixJson[4].get<float>(), matrixJson[5].get<float>(), matrixJson[6].get<float>(), matrixJson[7].get<float>(),
        matrixJson[8].get<float>(), matrixJson[9].get<float>(), matrixJson[10].get<float>(), matrixJson[11].get<float>(),
        matrixJson[12].get<float>(), matrixJson[13].get<float>(), matrixJson[14].get<float>(), matrixJson[15].get<float>());
    return true;
}

uint32 ModelCom::Get_NumIndices(uint32 meshIndex) const
{
    if (meshIndex >= _meshes.size())
        return 0;

    return _meshes[meshIndex]->Get_NumIndices();
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
            return static_cast<int32>(i);
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

    _hasExplicitAnimation = true;
    _skipInitialBlendSourceNotify = false;
    _currentAnimIndex = animIndex;
    _currentAnimStateIndex = UINT32_MAX;
    _nextAnimStateIndex = UINT32_MAX;
    _isAnimLoop = isLoop;
    _isBlending = false;
    Clear_SameAnimationBlend();
    _isFinished = false;
    Apply_StartFrameWithPlaySection(_currentAnimIndex, _isAnimLoop, startFrame);

    Mark_BoneMatricesDirty();
}

bool ModelCom::Set_AnimationByState(uint32 stateIndex, bool isLoop, float startFrame)
{
    if (stateIndex >= _stateAnimIndices.size() || _stateAnimIndices[stateIndex] == UINT32_MAX)
    {
         //LOG_WARN("[{}] Set_AnimationByState Failed: Invalid StateIndex [{}]",
          //   String::ToString(Get_Owner()->Get_Owner()->Get_Name()), stateIndex);

        return false;
    }

    const uint32 animIndex = _stateAnimIndices[stateIndex];
    _hasExplicitAnimation = true;
    _skipInitialBlendSourceNotify = false;
    _currentAnimStateIndex = stateIndex;
    _nextAnimStateIndex = UINT32_MAX;
    _currentAnimIndex = animIndex;
    _isAnimLoop = isLoop;
    _isBlending = false;
    Clear_SameAnimationBlend();
    _isFinished = false;
    Apply_StartFrameWithPlaySection(_currentAnimIndex, _isAnimLoop, startFrame);

    Mark_BoneMatricesDirty();
    return true;
}

void ModelCom::Set_Animation(uint32 animIndex, float blendDuration, bool isLoop, float startFrame)
{
    if (animIndex >= _animations.size()) return;

    _skipInitialBlendSourceNotify =
        !_hasExplicitAnimation && _currentAnimIndex != animIndex;
    Begin_SameAnimationBlendIfNeeded(animIndex, blendDuration);
    _hasExplicitAnimation = true;
    _nextAnimStateIndex = UINT32_MAX;
    _nextAnimIndex = animIndex;
    _isAnimLoop = isLoop;
    _isBlending = true;
    _blendDuration = blendDuration;
    _blendElapsedTime = 0.f;
    _isFinished = false;
    Apply_StartFrameWithPlaySection(_nextAnimIndex, _isAnimLoop, startFrame);

    Mark_BoneMatricesDirty();
}

void ModelCom::Set_Animation(const string& animName, bool isLoop, float startFrame)
{
    if (!_mappedAnimIndices.contains(animName))
    {
        if (_missingAnimationWarningKeys.emplace(animName).second)
            LOG_WARN("Failed to find animation by name: {}", animName);
        return;
    }

    Set_Animation(_mappedAnimIndices[animName], isLoop, startFrame);
}

void ModelCom::Set_Animation(const string& animName, float blendDuration, bool isLoop, float startFrame)
{
    if (!_mappedAnimIndices.contains(animName))
    {
        if (_missingAnimationWarningKeys.emplace(animName).second)
            LOG_WARN("Failed to find animation by name: {}", animName);
        return;
    }

    Set_Animation(_mappedAnimIndices[animName], blendDuration, isLoop, startFrame);
}

bool ModelCom::Set_AnimationByState(uint32 stateIndex, float blendDuration, bool isLoop, float startFrame)
{
    if (stateIndex >= _stateAnimIndices.size() || _stateAnimIndices[stateIndex] == UINT32_MAX)
    {
        // LOG_WARN("[{}] Set_AnimationByState Failed: Invalid StateIndex [{}]",
        //     String::ToString(Get_Owner()->Get_Owner()->Get_Name()), stateIndex);

        return false;
    }

    const uint32 animIndex = _stateAnimIndices[stateIndex];
    _skipInitialBlendSourceNotify =
        !_hasExplicitAnimation && _currentAnimIndex != animIndex;
    Begin_SameAnimationBlendIfNeeded(animIndex, blendDuration);
    _hasExplicitAnimation = true;
    _nextAnimStateIndex = stateIndex;
    _nextAnimIndex = animIndex;
    _isAnimLoop = isLoop;
    _isBlending = true;
    _blendDuration = blendDuration;
    _blendElapsedTime = 0.f;
    _isFinished = false;
    Apply_StartFrameWithPlaySection(_nextAnimIndex, _isAnimLoop, startFrame);

    Mark_BoneMatricesDirty();
    return true;
}

bool ModelCom::Set_AnimationByStateTimeSec(uint32 stateIndex, float blendDuration, bool isLoop, float startTimeSec)
{
    if (stateIndex >= _stateAnimIndices.size() || _stateAnimIndices[stateIndex] == UINT32_MAX)
        return false;

    const uint32 animIndex = _stateAnimIndices[stateIndex];

    if (blendDuration <= 0.f)
    {
        _hasExplicitAnimation = true;
        _skipInitialBlendSourceNotify = false;
        _currentAnimStateIndex = stateIndex;
        _nextAnimStateIndex = UINT32_MAX;
        _currentAnimIndex = animIndex;
        _isAnimLoop = isLoop;
        _isBlending = false;
        Clear_SameAnimationBlend();
        _isFinished = false;
        Apply_StartTimeSecWithPlaySection(_currentAnimIndex, _isAnimLoop, startTimeSec);

        Mark_BoneMatricesDirty();
        return true;
    }

    _skipInitialBlendSourceNotify =
        !_hasExplicitAnimation && _currentAnimIndex != animIndex;
    Begin_SameAnimationBlendIfNeeded(animIndex, blendDuration);
    _hasExplicitAnimation = true;
    _nextAnimStateIndex = stateIndex;
    _nextAnimIndex = animIndex;
    _isAnimLoop = isLoop;
    _isBlending = true;
    _blendDuration = blendDuration;
    _blendElapsedTime = 0.f;
    _isFinished = false;
    Apply_StartTimeSecWithPlaySection(_nextAnimIndex, _isAnimLoop, startTimeSec);

    Mark_BoneMatricesDirty();
    return true;
}

uint32 ModelCom::Get_AnimIndexByState(uint32 stateIndex) const
{
    return _stateAnimIndices[stateIndex];
}

bool ModelCom::Has_MorphData() const
{
    return _modelDeform != nullptr && _modelDeform->Has_MorphData();
}

const MorphData* ModelCom::Get_MorphData() const
{
    return _modelDeform != nullptr ? _modelDeform->Get_MorphData() : nullptr;
}

const vector<float>* ModelCom::Get_MorphWeights() const
{
    return _modelDeform != nullptr ? _modelDeform->Get_MorphWeights() : nullptr;
}

bool ModelCom::Consume_MorphDirty()
{
    return _modelDeform != nullptr && _modelDeform->Consume_MorphDirty();
}

HRESULT ModelCom::Apply_MorphWeightsToCpuVertices()
{
    if (_modelDeform == nullptr || !_modelDeform->Has_MorphData())
        return S_OK;

    const MorphData* morphData = _modelDeform->Get_MorphData();
    const vector<float>* weights = _modelDeform->Get_MorphWeights();
    if (morphData == nullptr || weights == nullptr)
        return S_OK;

    for (const MorphMeshData& morphMesh : morphData->meshes)
    {
        uint32 meshIndex = morphMesh.meshIndex;
        if (meshIndex >= _meshes.size())
        {
            const int32 foundMeshIndex = Find_MeshIndexByName(morphMesh.meshName);
            if (foundMeshIndex < 0)
                continue;

            meshIndex = static_cast<uint32>(foundMeshIndex);
        }

        Shared<SkeletalMesh> skeletalMesh = dynamic_pointer_cast<SkeletalMesh>(_meshes[meshIndex]);
        if (skeletalMesh == nullptr)
            continue;

        const vector<VTXANIMMESH>& sourceVertices = skeletalMesh->Get_SourceVertices();
        if (sourceVertices.empty() || sourceVertices.size() != morphMesh.vertexCount)
        {
            const int32 foundMeshIndex = Find_MeshIndexByName(morphMesh.meshName);
            if (foundMeshIndex < 0 || static_cast<uint32>(foundMeshIndex) == meshIndex)
                continue;

            meshIndex = static_cast<uint32>(foundMeshIndex);
            skeletalMesh = dynamic_pointer_cast<SkeletalMesh>(_meshes[meshIndex]);
            if (skeletalMesh == nullptr)
                continue;

            if (skeletalMesh->Get_SourceVertices().size() != morphMesh.vertexCount)
                continue;
        }

        vector<VTXANIMMESH> morphedVertices = skeletalMesh->Get_SourceVertices();
        const uint32 targetCount = static_cast<uint32>(std::min(weights->size(), morphMesh.vertexRanges.size()));
        for (uint32 targetIndex = 0; targetIndex < targetCount; ++targetIndex)
        {
            const float targetWeight = (*weights)[targetIndex];
            if (std::abs(targetWeight) <= 0.000001f)
                continue;

            const MorphVertexRange& range = morphMesh.vertexRanges[targetIndex];
            const uint32 end = range.start + range.count;
            if (range.start > morphMesh.vertexDeltas.size() || end > morphMesh.vertexDeltas.size())
                continue;

            for (uint32 deltaIndex = range.start; deltaIndex < end; ++deltaIndex)
            {
                const MorphVertexDelta& delta = morphMesh.vertexDeltas[deltaIndex];
                if (delta.vertexIndex >= morphedVertices.size())
                    continue;

                const float scaledWeight = targetWeight * delta.weightScale;
                VTXANIMMESH& vertex = morphedVertices[delta.vertexIndex];
                vertex.position += delta.deltaPosition * scaledWeight;
                vertex.normal += delta.deltaNormal * scaledWeight;
                if (vertex.normal.LengthSquared() > 0.000001f)
                    vertex.normal.Normalize();
            }
        }

        CHECK_FAILED(skeletalMesh->Update_VertexBuffer(morphedVertices), E_FAIL);
    }

    return S_OK;
}

const vector<VTXANIMMESH>* ModelCom::Get_Vertices(uint32 meshIndex) const
{
    if (meshIndex >= _meshes.size())
        return nullptr;

    const Shared<SkeletalMesh> skeletalMesh = dynamic_pointer_cast<SkeletalMesh>(_meshes[meshIndex]);
    if (skeletalMesh == nullptr)
        return nullptr;

    return &skeletalMesh->Get_SourceVertices();
}

const string* ModelCom::Get_MeshName(uint32 meshIndex) const
{
    if (meshIndex >= _meshes.size() || _meshes[meshIndex] == nullptr)
        return nullptr;

    return &_meshes[meshIndex]->Get_Name();
}

int32 ModelCom::Find_MeshIndexByName(const string& meshName) const
{
    for (uint32 i = 0; i < _meshes.size(); ++i)
    {
        if (_meshes[i] != nullptr && _meshes[i]->Get_Name() == meshName)
            return static_cast<int32>(i);
    }

    return -1;
}

HRESULT ModelCom::Try_LoadMorphData(const string& modelPath)
{
    if (_modelDeform == nullptr)
        _modelDeform = ModelDeform::Create();

    CHECK_NULL(_modelDeform, E_FAIL);
    return _modelDeform->Try_LoadMorphData(modelPath);
}

uint32 ModelCom::Get_MorphTargetCount() const
{
    return _modelDeform != nullptr ? _modelDeform->Get_MorphTargetCount() : 0;
}

const string* ModelCom::Get_MorphTargetName(uint32 index) const
{
    return _modelDeform != nullptr ? _modelDeform->Get_MorphTargetName(index) : nullptr;
}

int32 ModelCom::Find_MorphTargetIndex(const string& name) const
{
    return _modelDeform != nullptr ? _modelDeform->Find_MorphTargetIndex(name) : -1;
}

void ModelCom::Set_MorphWeight(uint32 targetIndex, float weight)
{
    if (_modelDeform != nullptr)
        _modelDeform->Set_MorphWeight(targetIndex, weight);
}

void ModelCom::Set_MorphWeight_Unclamped(uint32 targetIndex, float weight)
{
    if (_modelDeform != nullptr)
        _modelDeform->Set_MorphWeight_Unclamped(targetIndex, weight);
}

void ModelCom::Reset_MorphWeights()
{
    if (_modelDeform != nullptr)
        _modelDeform->Reset_MorphWeights();
}

const char* ModelCom::Get_AnimationName(uint32 animationIndex) const
{
    if (animationIndex >= _animations.size())
        return nullptr;

    return _animations[animationIndex]->Get_AnimationName().c_str();
}

uint32 ModelCom::Get_AnimationIndexByName(const string& animationName) const
{
    for (auto& [name, index] : _mappedAnimIndices)
    {
        if (name == animationName)
            return index;
    }

    return UINT_MAX;
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
    if (boneIndex >= _numBones)
        return nullptr;

    if (_leaderPoseModel != nullptr && !_leaderPoseCombinedMatrices.empty())
    {
        ModelCom* mutableThis = const_cast<ModelCom*>(this);
        mutableThis->Add_LeaderPoseRequiredBone(boneIndex);

        vector<uint8> visited(_numBones, 0);
        mutableThis->Build_LeaderPoseMatrix(boneIndex, visited);

        return &_leaderPoseCombinedMatrices[boneIndex];
    }
    
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

void ModelCom::Set_AnimationSpeedByState(uint32 stateIndex, float speed)
{
    uint32 animIndex = Get_AnimIndexByState(stateIndex);
    if (animIndex != UINT32_MAX && animIndex < _animations.size())
    {
        _animations[animIndex]->Set_AnimationSpeed(speed);
    }
}

float ModelCom::Get_AnimationSpeedByState(uint32 stateIndex) const
{
    uint32 animIndex = Get_AnimIndexByState(stateIndex);
    if (animIndex != UINT32_MAX && animIndex < _animations.size())
    {
        return _animations[animIndex]->Get_AnimationSpeed();
    }
    return FLT_MAX;
}

void ModelCom::Set_AnimationSpeedByName(const string& animName, float speed)
{
    uint32 animIndex = Get_AnimationIndexByName(animName);

    if (animIndex != UINT_MAX && animIndex < _animations.size())
    {
        _animations[animIndex]->Set_AnimationSpeed(speed);
    }
}

float ModelCom::Get_AnimationSpeedByName(const string& animName) const
{
    uint32 animIndex = Get_AnimationIndexByName(animName);

    if (animIndex != UINT_MAX && animIndex < _animations.size())
    {
        return _animations[animIndex]->Get_AnimationSpeed();
    }

    return FLT_MAX;
}

void ModelCom::Set_CurrentAnimationSpeed(float speed)
{
    uint32 animIndex = _isBlending ? _nextAnimIndex : _currentAnimIndex;
    if (animIndex < _animations.size() && _animations[animIndex] != nullptr)
    {
        _animations[animIndex]->Set_AnimationSpeed(speed);
    }
}

float ModelCom::Get_CurrentAnimationSpeed() const
{
    uint32 animIndex = _isBlending ? _nextAnimIndex : _currentAnimIndex;
    if (animIndex < _animations.size() && _animations[animIndex] != nullptr)
    {
        return _animations[animIndex]->Get_AnimationSpeed();
    }
    return FLT_MAX;
}

float ModelCom::Get_CurrentProgress() const
{
	uint32 index = _isBlending ? _nextAnimIndex : _currentAnimIndex;
	if (_animations.empty() || index >= _animations.size())
		return 0.f;

	return _animations[index]->Get_Progress();
}

float ModelCom::Get_CurrentFrame() const
{
	uint32 index = _isBlending ? _nextAnimIndex : _currentAnimIndex;
	if (_animations.empty() || index >= _animations.size())
		return 0.f;

	return _animations[index]->Get_Progress() * _animations[index]->Get_Duration();
}

const RootMotionFrame& ModelCom::Get_RootTransformVelocity(float scaleOffset)
{
    _rootFrameVelocity.position = scaleOffset * _rootOriginalFrameVelocity.position;
    _rootFrameVelocity.rotation = scaleOffset * _rootOriginalFrameVelocity.rotation;

    return _rootFrameVelocity;
}

float ModelCom::Get_RootMotionTranslationRatio() const
{
    if (_animations.empty() || _rootNodeIndex == -1)
        return 0.f;

    // 블렌딩 중이면 다음 애니메이션(실질적으로 재생 중인 타겟)의 진행도를 가져옴
    uint32 activeAnimIndex = _isBlending ? _nextAnimIndex : _currentAnimIndex;

    if (activeAnimIndex >= _animations.size())
        return 0.f;

    const auto& curAnim = _animations[activeAnimIndex];
    float totalDist = curAnim->Get_RootTranslation(_rootNodeIndex).Length();

    if (totalDist <= 0.001f)
        return 0.f;

    Vec3 currentTranslation = curAnim->Get_CurrentRootTranslation(_rootNodeIndex);
    float currentDist = currentTranslation.Length();

    return clamp(currentDist / totalDist, 0.f, 1.f);
}

HRESULT ModelCom::Initialize_Prototype(ModelType type, const char* modelFilePath, const Matrix& preLocalTransformMatrix)
{
    _type = type;
    _preLocalTransformMatrix = preLocalTransformMatrix;

    Vec3 pos{}, scale{};
    Quat quat{};
    _preLocalTransformMatrix.Decompose(scale, quat, pos);
    _preLocalRotationMatrix = Matrix::CreateFromQuaternion(quat) * Matrix::CreateTranslation(pos);

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

bool ModelCom::Try_BuildDefaultPrototypeTag(const wstring& sourceModelPath, wstring& outPrototypeTag)
{
    outPrototypeTag.clear();

    if (sourceModelPath.empty())
        return false;

    const wstring stem = fs::path(sourceModelPath).stem().wstring();
    if (stem.empty())
        return false;

    const wstring candidateTag = L"Model_" + stem;
    if (!GAME->Has_Prototype(0, candidateTag))
        return false;

    outPrototypeTag = candidateTag;
    return true;
}

string ModelCom::Build_DefaultBaseBundleKey(const wstring& sourceModelPath)
{
    if (sourceModelPath.empty())
        return {};

    return String::ToLowerCopy(String::ToString(fs::path(sourceModelPath).stem().wstring()));
}

string ModelCom::Build_DefaultBaseBundleKeyFromPrototypeTag(const wstring& prototypeTag)
{
    const wstring prefix = L"Model_";
    if (!prototypeTag.starts_with(prefix))
        return {};

    const wstring stem = prototypeTag.substr(prefix.size());
    if (stem.empty())
        return {};

    return String::ToLowerCopy(String::ToString(stem));
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

bool ModelCom::Normalize_OwnerComponentTagAndBindings()
{
    Shared<GameObject> owner = Get_Owner();
    if (owner == nullptr)
        return false;

    wstring currentTag{};
    bool foundCurrentTag = false;
    owner->Visit_Components([&](const wstring& tag, const Shared<Component>& component)
    {
        if (foundCurrentTag || component.get() != this)
            return;

        currentTag = tag; // 현재 owner 안에서 이 ModelCom가 어떤 태그로 걸려 있는지 찾는다.
        foundCurrentTag = true;
    });

    if (!foundCurrentTag)
        return false;

    const string oldDefaultBundleKey = Build_DefaultBaseBundleKeyFromPrototypeTag(currentTag);
    const string newDefaultBundleKey = Build_DefaultBaseBundleKey(_sourceModelPath);

    bool changed = false;

    wstring normalizedTag{};
    const bool hasNormalizedTag =
        Try_BuildDefaultPrototypeTag(_sourceModelPath, normalizedTag); // 태그 계산은 "되면 적용", 실패해도 아래 bundle key 정상화는 계속 진행한다.

    if (hasNormalizedTag && currentTag != normalizedTag)
        changed |= owner->Rename_ComponentTag(currentTag, normalizedTag);

    Ensure_MaterialBindings();

    for (MaterialBinding& binding : _materialBindings)
    {
        if (binding.baseBundleKey.empty() || binding.baseBundleKey == oldDefaultBundleKey)
        {
            binding.baseBundleKey = newDefaultBundleKey; // 예전 기본값만 새 모델 기본 key로 바꾼다.
            changed = true;
        }
    }

    if (changed)
        Apply_BaseMaterialFromBundle_AllSlots(); // tag rename이든 bundle key 교체든 하나라도 바뀌면 실제 material에 다시 반영한다.

    return changed;
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
                    if (base.orm2Override) (void)material->Override_TextureSlot(MaterialTextureSlot::ORM2, 0, base.orm2Path);
                    if (base.orm3Override) (void)material->Override_TextureSlot(MaterialTextureSlot::ORM3, 0, base.orm3Path);
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
    if (inst.overrideTemplateEnabled)
        material->Set_TemplateId(inst.templateId);

    if (inst.forcedPassIndex >= 0)
        material->Set_ForcedPassIndex(inst.forcedPassIndex);

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
    Apply_TextureOverride(material.get(), MaterialTextureSlot::ORM2, inst.orm2);
    Apply_TextureOverride(material.get(), MaterialTextureSlot::ORM3, inst.orm3);
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
        root["model_path"] = String::ToString(_sourceModelPath);

        const string modelGuid = GAME->Ensure_AssetGUID(_sourceModelPath, "Model");
        if (!modelGuid.empty())
            root["model_guid"] = modelGuid;
    }

    root["model_type"] = To_ModelTypeString(_type);
    root["pre_local_transform"] = Write_PreLocalTransformJson(_preLocalTransformMatrix);

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
            inst.overrideTemplateEnabled || (inst.forcedPassIndex >= 0) || inst.overrideRenderStateEnabled || inst.cbMask.Any() || hasAnyTextureOverride;

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
            instJson["override_template_enabled"] = inst.overrideTemplateEnabled;
            if (inst.overrideTemplateEnabled)
                instJson["template"] = static_cast<uint32>(inst.templateId);

            instJson["override_render_state_enabled"] = inst.overrideRenderStateEnabled;
            if (inst.forcedPassIndex >= 0)
                instJson["force_pass_index"] = inst.forcedPassIndex;

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
                if (inst.cbMask.ormAoSource) cb["ormAoSource"] = inst.cb.ormAoSource;
                if (inst.cbMask.ormRoughnessSource) cb["ormRoughnessSource"] = inst.cb.ormRoughnessSource;
                if (inst.cbMask.ormMetalnessSource) cb["ormMetalnessSource"] = inst.cb.ormMetalnessSource;
                if (inst.cbMask.opacityMaskSource) cb["opacityMaskSource"] = inst.cb.opacityMaskSource;

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

    Matrix preLocalTransform{};
    if (Read_PreLocalTransformJson(data, preLocalTransform))
    {
        Vec3 pos{}, scale{};
        Quat quat{};
        _preLocalTransformMatrix = preLocalTransform;
        _preLocalTransformMatrix.Decompose(scale, quat, pos);
        _preLocalRotationMatrix = Matrix::CreateFromQuaternion(quat) * Matrix::CreateTranslation(pos);
    }

    const string modelTypeString = data.value("model_type", To_ModelTypeString(_type));
    const ModelType modelType = Parse_ModelType(modelTypeString);

    if (!data.contains("model_guid") || !data["model_guid"].is_string())
    {
        const string modelPathString = data.value("model_path", string{});
        if (!modelPathString.empty())
        {
            const fs::path modelPath = fs::path(String::ToWString(modelPathString)).lexically_normal();
            const fs::path currentPath = fs::path(_sourceModelPath).lexically_normal();
            if (_sourceModelPath.empty() || modelPath != currentPath || modelType != _type)
            {
                if (FAILED(Reload_ModelFromPath(modelPath, modelType)))
                    LOG_WARN("ModelCom::From_Json - failed to reload model. path={}", modelPathString);
            }
        }

        Asset_Manager* assetManager = GAME->Get_AssetManager();
        MaterialBuilder* builder = assetManager ? assetManager->Get_MaterialBuilder() : nullptr;
        if (builder != nullptr)
        {
            builder->Try_ApplyMaterialBindingsFromJson(*this, data);
            builder->Try_ApplyModelMaterialOverridesFromJson(*this, data);
        }

        Apply_BaseMaterialFromBundle_AllSlots();
        Normalize_OwnerComponentTagAndBindings();
        return;
    }

    const string modelGuid = data["model_guid"].get<string>();
    if (modelGuid.empty())
    {
        const string modelPathString = data.value("model_path", string{});
        if (!modelPathString.empty())
        {
            const fs::path modelPath = fs::path(String::ToWString(modelPathString)).lexically_normal();
            const fs::path currentPath = fs::path(_sourceModelPath).lexically_normal();
            if (_sourceModelPath.empty() || modelPath != currentPath || modelType != _type)
            {
                if (FAILED(Reload_ModelFromPath(modelPath, modelType)))
                    LOG_WARN("ModelCom::From_Json - failed to reload model. path={}", modelPathString);
            }
        }

        Asset_Manager* assetManager = GAME->Get_AssetManager();
        MaterialBuilder* builder = assetManager ? assetManager->Get_MaterialBuilder() : nullptr;
        if (builder != nullptr)
        {
            builder->Try_ApplyMaterialBindingsFromJson(*this, data);
            builder->Try_ApplyModelMaterialOverridesFromJson(*this, data);
        }

        Apply_BaseMaterialFromBundle_AllSlots();
        Normalize_OwnerComponentTagAndBindings();
        return;
    }

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
        Normalize_OwnerComponentTagAndBindings();
        Ensure_AnimStateAssetLoaded(modelGuid);
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
    Normalize_OwnerComponentTagAndBindings();

    Ensure_AnimStateAssetLoaded(modelGuid); // 애니메이션은 Prefab기반 로드 싱크를 여기다 맞춤
}

HRESULT ModelCom::Reload_ModelFromGuid(const string& guid, ModelType modelType)
{
    const wstring resolvedPath = GAME->Resolve_AssetPath(guid);
    if (resolvedPath.empty())
        return E_FAIL;

    return Reload_ModelFromPath(fs::path(resolvedPath), modelType);
}

HRESULT ModelCom::Reload_ModelFromPath(const fs::path& path, ModelType modelType)
{
    const fs::path modelPath = fs::path(path).lexically_normal();
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

HRESULT ModelCom::RenderInstancedIndirect(uint32 meshIndex, Buffer* instanceVB, uint32 instanceStride, Buffer* indirectArgsBuffer)
{
    if (meshIndex >= _numMeshes || !instanceVB || !indirectArgsBuffer)
    {
        return E_FAIL;
    }

    return _meshes[meshIndex]->RenderInstancedIndirect(instanceVB, instanceStride, indirectArgsBuffer);
}

bool ModelCom::Play_Animation(float timeDelta)
{
    AnimPipelineContext context{};
    Begin_AnimationPipeline(timeDelta, context);

    Step_SampleAnimationPose(context);
    Evaluate_AnimationModifiers(timeDelta);
    Step_RebuildCombinedBones();
    Step_FinalizeAnimationFrame(context);

    return _isFinished;
}

HRESULT ModelCom::Bind_Material(ShaderCom* shader, const char* constantName, uint32 meshIndex, MaterialTextureSlot slot, uint32 textureIndex)
{
    if (meshIndex >= _meshes.size() || _meshes[meshIndex] == nullptr)
        return E_FAIL;

    const uint32 materialIndex = _meshes[meshIndex]->Get_MaterialIndex();
    if (materialIndex >= _materials.size() || _materials[materialIndex] == nullptr)
        return E_FAIL;

    return _materials[materialIndex]->Bind_Material(shader, constantName, slot, textureIndex);
}

HRESULT ModelCom::Bind_BoneMatrices(ShaderCom* shader, const char* constantName, uint32 meshIndex)
{
    ENGINE_PROFILE_SCOPE_DETAIL("AnimMesh::BindBoneMatrices");

    CHECK_FAILED(Prepare_BoneMatrixBuffers(), E_FAIL);
    return Bind_PreparedBoneMatrices(shader, constantName, meshIndex);
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

    if (base.orm2Override)
    {
        material->Override_TextureSlot(MaterialTextureSlot::ORM2, 0, base.orm2Path);
    }

    if (base.orm3Override)
    {
        material->Override_TextureSlot(MaterialTextureSlot::ORM3, 0, base.orm3Path);
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

HRESULT ModelCom::Prepare_BoneMatrixBuffers()
{
    ENGINE_PROFILE_SCOPE_DETAIL("AnimMesh::PrepareBoneMatrixBuffers");

    if (!_isSkeletal || _numBones == 0)
        return S_OK;

    if (_leaderPoseModel != nullptr)
    {
        if (!_leaderPoseMatricesValid)
            Update_LeaderPoseMatrices(0.f);

        if (_leaderPoseCombinedMatrices.empty())
            return E_FAIL;
    }

    return Ensure_MeshBoneMatrixBuffers();
}

HRESULT ModelCom::Bind_PreparedBoneMatrices(ShaderCom* shader, const char* constantName, uint32 meshIndex)
{
    ENGINE_PROFILE_SCOPE_DETAIL("AnimMesh::BindPreparedBoneMatrices");

    if (shader == nullptr || constantName == nullptr)
        return E_FAIL;

    if (!_isSkeletal || _numBones == 0)
        return S_OK;

    if (meshIndex >= _meshes.size())
        return E_FAIL;

    if (meshIndex >= _meshBoneMatrixBuffers.size())
        return E_FAIL;

    const Shared<ComputeStructuredBuffer>& meshBoneMatrixBuffer = _meshBoneMatrixBuffers[meshIndex];
    if (meshBoneMatrixBuffer == nullptr)
        return S_OK;

    return shader->Bind_SRV(constantName, meshBoneMatrixBuffer->Get_SRV()); // 모델 뷰어 같은 툴 경로 bind 유지
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

const AnimNotifyData* ModelCom::Find_AnimNotifyDataByState(uint32 stateIndex)
{
    if (!Ensure_AnimNotifyAssetLoaded())
        return nullptr;

    if (stateIndex >= _stateAnimIndices.size() || _stateAnimIndices[stateIndex] == UINT32_MAX)
        return nullptr;

    const uint32 animIndex = _stateAnimIndices[stateIndex];
    const char* animationName = Get_AnimationName(animIndex);
    if (animationName == nullptr)
        return nullptr;

    return AnimNotify_Parser::Find_Animation(_animNotifyAsset, animationName);
}

const AnimNotifyData* ModelCom::Find_CurrentAnimNotifyData()
{
    if (!Ensure_AnimNotifyAssetLoaded())
        return nullptr;

    return Find_CurrentNotifyAnimation();
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

    AnimNotify_Parser::Load_ModelAsset(modelGuid, _animNotifyAsset);

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

bool ModelCom::Try_GetPlaySection(uint32 animIndex, float& outStartSec, float& outEndSec)
{
    outStartSec = 0.f;
    outEndSec = -1.f;

    if (animIndex >= _animations.size())
        return false;

    if (!Ensure_AnimNotifyAssetLoaded())
        return false;

    const char* animationName = Get_AnimationName(animIndex);
    if (animationName == nullptr)
        return false;

    const AnimNotifyData* animationData = AnimNotify_Parser::Find_Animation(_animNotifyAsset, animationName);
    if (animationData == nullptr)
        return false;

    outStartSec = max(0.f, animationData->playSectionStartSec);
    outEndSec = animationData->playSectionEndSec;
    return true;
}

void ModelCom::Apply_StartFrameWithPlaySection(uint32 animIndex, bool isLoop, float startFrame)
{
    if (animIndex >= _animations.size())
        return;

    if (isLoop || startFrame > 0.f)
    {
        _animations[animIndex]->Set_StartFrame(startFrame);
        return;
    }

    float sectionStartSec = 0.f;
    float sectionEndSec = -1.f;
    if (Try_GetPlaySection(animIndex, sectionStartSec, sectionEndSec) && sectionStartSec > 0.f)
    {
        _animations[animIndex]->Set_StartTimeSec(sectionStartSec);
        return;
    }

    _animations[animIndex]->Set_StartFrame(startFrame);
}

void ModelCom::Apply_StartTimeSecWithPlaySection(uint32 animIndex, bool isLoop, float startTimeSec)
{
    if (animIndex >= _animations.size())
        return;

    if (isLoop || startTimeSec > 0.f)
    {
        _animations[animIndex]->Set_StartTimeSec(startTimeSec);
        return;
    }

    float sectionStartSec = 0.f;
    float sectionEndSec = -1.f;
    if (Try_GetPlaySection(animIndex, sectionStartSec, sectionEndSec) && sectionStartSec > 0.f)
    {
        _animations[animIndex]->Set_StartTimeSec(sectionStartSec);
        return;
    }

    _animations[animIndex]->Set_StartTimeSec(startTimeSec);
}

bool ModelCom::Apply_CurrentPlaySectionEnd(const AnimPipelineContext& context)
{
    if (_isBlending || _isAnimLoop || _currentAnimIndex >= _animations.size())
        return false;

    float sectionStartSec = 0.f;
    float sectionEndSec = -1.f;
    if (!Try_GetPlaySection(_currentAnimIndex, sectionStartSec, sectionEndSec))
        return false;

    if (sectionEndSec < 0.f)
        return false;

    const float animationLengthSec = _animations[_currentAnimIndex]->Get_DurationSec();
    const float clampedEndSec = clamp(sectionEndSec, sectionStartSec, animationLengthSec);
    if (_animations[_currentAnimIndex]->Get_CurrentTimeSec() + 0.0001f < clampedEndSec)
        return false;

    _animations[_currentAnimIndex]->Set_CurrentTimeSec(clampedEndSec);
    _animations[_currentAnimIndex]->Evaluate_CurrentPose(_bones, context.rootIndex, _isAnimLoop);
    _rootOriginalFrameVelocity =
        (context.rootIndex >= 0)
        ? _animations[_currentAnimIndex]->Get_TransformVelocity(context.rootIndex)
        : RootMotionFrame{};
    _isFinished = true;
    return true;
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
        context.notifyStateProgress = 1.f; // 강제 종료여도 End에서는 완료 상태로 사용하려면 1.f
        context.notifyStateDurationSec = 0.f;
        context.deltaTime = 0.f;
        context.isLooping = false;
        context.wrapped = false;
        context.isPreview = !_isAnimNotifyExecutionEnabled;
        context.isCinematic = _isCinematicNotifyContext;

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
            context.notifyStateProgress = 1.f;
            context.notifyStateDurationSec = 0.f;
            context.isLooping = false;
            context.wrapped = false;
            context.isPreview = !_isAnimNotifyExecutionEnabled;
            context.isCinematic = _isCinematicNotifyContext;

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

        if (Is_AnimNotifyTypeSuppressed(entry.notify->Get_TypeName()))
            continue;

        // 시작 타이밍
        if (!Is_NotifyTimeInRange(entry.timeSec, context.previousTimeSec, context.currentTimeSec, context.wrapped))
            continue;

        if (executeNotifies)
        {
            if (_isAnimationPaused)
                return; // notify 실행 중 튜토리얼 pause가 걸리면 같은 프레임의 남은 notify를 막는다.

            entry.notify->Execute(context);
        }
    }
}

void ModelCom::Update_AnimNotifyStates(const AnimNotifyData& animationData, const AnimNotifyContext& context,
    bool executeNotifies)
{
    if (_isAnimationPaused)
        return; // notify 실행 중 튜토리얼 pause가 걸리면 같은 프레임의 state notify도 막는다.

    for (int32 stateIndex = 0; stateIndex < static_cast<int32>(animationData.notifyStates.size()); ++stateIndex)
    {
        const auto& entry = animationData.notifyStates[stateIndex];
        if (entry.notifyState == nullptr)
            continue;

        if (Is_AnimNotifyTypeSuppressed(entry.notifyState->Get_TypeName()))
            continue;

        const bool isActiveNow = Is_NotifyStateActiveTime(context.currentTimeSec, entry.startSec, entry.durationSec);
        const int32 activeIndex = Find_ActiveNotifyStateIndex(_activeNotifyStates, context.animationName, stateIndex);

        AnimNotifyContext stateContext = context;
        stateContext.notifyStateDurationSec = entry.durationSec;

        if (entry.durationSec > 0.f)
        {
            stateContext.notifyStateProgress = // 현재 시간이 notifyState에서 몇퍼 진행됐는지
                ::clamp((context.currentTimeSec - entry.startSec) / entry.durationSec, 0.f, 1.f); 
        }
        else
        {
            stateContext.notifyStateProgress = isActiveNow ? 1.f : 0.f;
        }

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
                {
                    if (_isAnimationPaused)
                        return; // On_Begin 연쇄 실행을 같은 프레임에서 끊는다.

                    entry.notifyState->On_Begin(stateContext);
                }
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
                    if (_isAnimationPaused)
                        return; // On_Tick 연쇄 실행을 같은 프레임에서 끊는다.

                    _activeNotifyStates[activeIndex].notifyState->On_Tick(stateContext);
                }
            }
        }
        else if (activeIndex != -1)
        {
            Shared<AnimNotifyState> instance = _activeNotifyStates[activeIndex].notifyState;
            _activeNotifyStates.erase(_activeNotifyStates.begin() + activeIndex);

            stateContext.notifyStateProgress = 1.f; 

            if (executeNotifies && instance)
            {
                if (_isAnimationPaused)
                    return; // On_End 연쇄 실행을 같은 프레임에서 끊는다.

                instance->On_End(stateContext);
            }
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

bool ModelCom::Is_AnimNotifyTypeSuppressed(const string& typeName) const
{
    return !typeName.empty() &&
        _suppressedAnimNotifyTypeNames.contains(typeName);
}

void ModelCom::Set_LeaderPoseComponent(const ModelCom* leaderModel)
{
    _leaderPoseModel = leaderModel;
    _leaderPoseBoneMap.clear();
    _leaderPoseCombinedMatrices.clear();
    _leaderPoseRequiredBones.clear();

    _leaderPoseLastSyncedGeneration = UINT64_MAX;

    if (nullptr == _leaderPoseModel || !_isSkeletal || !_leaderPoseModel->Is_Skeletal())
        return;

    _leaderPoseBoneMap.resize(_numBones, -1);
    _leaderPoseCombinedMatrices.resize(_numBones, Matrix::Identity);

    for (uint32 i = 0; i < _numBones; ++i)
    {
        _leaderPoseBoneMap[i] =
            _leaderPoseModel->Get_BoneIndex(_bones[i]->Get_BoneName());
    }

    Build_LeaderPoseRequiredBones();

    _leaderPoseMatricesValid = false;
    Update_LeaderPoseMatrices(0.f);
}

void ModelCom::Clear_LeaderPoseComponent()
{
    _leaderPoseModel = nullptr;
    _leaderPoseBoneMap.clear();
    _leaderPoseCombinedMatrices.clear();
    _leaderPoseRequiredBones.clear();

    _leaderPoseLastSyncedGeneration = UINT64_MAX;

    _leaderPoseMatricesValid = false;
    Mark_BoneMatricesDirty();
}

void ModelCom::Update_LeaderPoseMatrices(float timeDelta)
{
    ENGINE_PROFILE_SCOPE_DETAIL("AnimMesh::UpdateLeaderPose");
    if (nullptr == _leaderPoseModel || _leaderPoseBoneMap.empty())
        return;

    // leader가 이번 프레임에 bone을 갱신하지 않았으면 재계산 스킵
    const uint64 leaderGen = _leaderPoseModel->_boneUpdateGeneration;
    const bool leaderPoseChanged = leaderGen != _leaderPoseLastSyncedGeneration;

    if (leaderPoseChanged)
    {
        _leaderPoseLastSyncedGeneration = leaderGen;

        _leaderPoseVisited.assign(_numBones, 0);

        if (_leaderPoseRequiredBones.empty())
            Build_LeaderPoseRequiredBones();

        for (uint32 boneIndex : _leaderPoseRequiredBones)
        {
            Build_LeaderPoseMatrix(boneIndex, _leaderPoseVisited);
        }

        _leaderPoseMatricesValid = true;
        Mark_BoneMatricesDirty();
    }

    Evaluate_AnimationModifiersLeaderPose(timeDelta, true);
    Step_RebuildCombinedBones();
}

void ModelCom::Build_LeaderPoseRequiredBones()
{
    _leaderPoseRequiredBones.clear();

    if (!_isSkeletal || _numBones == 0)
        return;

    vector<uint8> requiredFlags(_numBones, 0);

    for (const Shared<Mesh>& mesh : _meshes)
    {
        if (mesh == nullptr)
            continue;

        const vector<uint32>& boneIndices = mesh->Get_BoneIndices();
        for (uint32 boneIndex : boneIndices)
            Mark_LeaderPoseRequiredBone(boneIndex, requiredFlags);
    }
}

void ModelCom::Add_LeaderPoseRequiredBone(uint32 boneIndex)
{
    if (boneIndex >= _numBones)
        return;

    vector<uint8> requiredFlags(_numBones, 0);
    for (uint32 requiredBoneIndex : _leaderPoseRequiredBones)
    {
        if (requiredBoneIndex < _numBones)
            requiredFlags[requiredBoneIndex] = 1;
    }

    Mark_LeaderPoseRequiredBone(boneIndex, requiredFlags);
}

void ModelCom::Mark_LeaderPoseRequiredBone(uint32 boneIndex, vector<uint8>& requiredFlags)
{
    if (boneIndex >= _numBones)
        return;

    if (requiredFlags.size() != _numBones)
        requiredFlags.assign(_numBones, 0);

    if (requiredFlags[boneIndex])
        return;

    requiredFlags[boneIndex] = 1;
    _leaderPoseRequiredBones.push_back(boneIndex);

    const int32 parentIndex = _bones[boneIndex]->Get_ParentIndex();
    if (parentIndex >= 0)
        Mark_LeaderPoseRequiredBone(static_cast<uint32>(parentIndex), requiredFlags);
}

void ModelCom::Mark_BoneMatricesDirty()
{
    ++_boneUpdateGeneration;
    _boneMatricesDirty = true;
    _boneMatrixBuffersDirty = true;
}

HRESULT ModelCom::Ensure_MeshBoneMatrices()
{
    ENGINE_PROFILE_SCOPE_DETAIL("AnimMesh::EnsureMeshBoneMatrices");

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

HRESULT ModelCom::Ensure_MeshBoneMatrixBuffers()
{
    ENGINE_PROFILE_SCOPE_DETAIL("AnimMesh::EnsureMeshBoneMatrixBuffers");

    CHECK_FAILED(Ensure_MeshBoneMatrices(), E_FAIL); // CPU mesh bone matrices cache 최신화

    if (!_boneMatrixBuffersDirty)
        return S_OK; // 이미 업로드된 bone buffer가 최신이면 재업로드 생략

    if (_meshBoneMatrixBuffers.size() != _meshBoneMatrixCaches.size())
        _meshBoneMatrixBuffers.resize(_meshBoneMatrixCaches.size());

    for (uint32 meshIndex = 0; meshIndex < _meshBoneMatrixCaches.size(); ++meshIndex)
    {
        ENGINE_PROFILE_SCOPE_DETAIL("AnimMesh::UploadBoneBuffer");

        const vector<Matrix>& meshBoneMatrices = _meshBoneMatrixCaches[meshIndex];
        if (meshBoneMatrices.empty())
            continue;

        const uint32 matrixCount = static_cast<uint32>(meshBoneMatrices.size());

        if (_meshBoneMatrixBuffers[meshIndex] == nullptr ||
            _meshBoneMatrixBuffers[meshIndex]->Get_ElementCount() < matrixCount)
        {
            _meshBoneMatrixBuffers[meshIndex] = ComputeStructuredBuffer::Create(
                _device, _context, sizeof(Matrix), matrixCount, meshBoneMatrices.data()); // non-compute path용 bone buffer 생성

            if (_meshBoneMatrixBuffers[meshIndex] == nullptr)
                return E_FAIL;
        }
        else
        {
            CHECK_FAILED(
                _meshBoneMatrixBuffers[meshIndex]->Update_Data(
                    meshBoneMatrices.data(),
                    matrixCount), // 기존 buffer가 충분하면 bone data만 갱신
                E_FAIL);
        }
    }

    _boneMatrixBuffersDirty = false; // 모든 mesh bone buffer 업로드 완료
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

void ModelCom::Begin_AnimationPipeline(float timeDelta, AnimPipelineContext& outContext)
{
    outContext.timeDelta = timeDelta;
    outContext.adjustedTimeDelta = timeDelta * _frameSpeed;
    outContext.previousTimeSec = Get_CurrentAnimationTimeSec();
    outContext.previousAnimIndex = _currentAnimIndex;
    outContext.previousAnimStateIndex = _currentAnimStateIndex;
    outContext.rootIndex = _isRootMotion ? _rootNodeIndex : -1;
}

bool ModelCom::Should_UseSameAnimationBlend(uint32 animIndex, float blendDuration) const
{
    if (blendDuration <= 0.f || !_hasExplicitAnimation)
        return false;

    if (animIndex >= _animations.size())
        return false;

    // Reuse the source-pose snapshot path for explicit blends so notify/modifier poses
    // do not snap back to raw animation samples.
    return true;
}

void ModelCom::Begin_SameAnimationBlendIfNeeded(uint32 animIndex, float blendDuration)
{
    if (!Should_UseSameAnimationBlend(animIndex, blendDuration))
    {
        Clear_SameAnimationBlend();
        return;
    }

    Capture_SameAnimationBlendSourcePose();
    _isSameAnimationBlending = !_sameAnimationBlendSourceLocalMatrices.empty();
}

void ModelCom::Clear_SameAnimationBlend()
{
    _isSameAnimationBlending = false;
    _sameAnimationBlendSourceLocalMatrices.clear();
}

void ModelCom::Capture_SameAnimationBlendSourcePose()
{
    _sameAnimationBlendSourceLocalMatrices.clear();
    _sameAnimationBlendSourceLocalMatrices.reserve(_bones.size());

    for (const Shared<Bone>& bone : _bones)
    {
        if (bone != nullptr)
            _sameAnimationBlendSourceLocalMatrices.push_back(bone->Get_TransformationMatrix());
        else
            _sameAnimationBlendSourceLocalMatrices.push_back(Matrix::Identity);
    }
}

void ModelCom::Apply_SameAnimationBlendPose(float blendRatio, int32 rootNodeIndex)
{
    if (_sameAnimationBlendSourceLocalMatrices.size() != _bones.size())
        return;

    for (uint32 boneIndex = 0; boneIndex < _bones.size(); ++boneIndex)
    {
        if (_bones[boneIndex] == nullptr)
            continue;

        RootMotionFrame source{};
        RootMotionFrame target{};

        Matrix sourceMatrix = _sameAnimationBlendSourceLocalMatrices[boneIndex];
        sourceMatrix.Decompose(
            source.scale,
            source.rotation,
            source.position);

        Matrix targetMatrix = _bones[boneIndex]->Get_TransformationMatrix();
        targetMatrix.Decompose(
            target.scale,
            target.rotation,
            target.position);

        const Vec3 blendedScale = Vec3::Lerp(source.scale, target.scale, blendRatio);
        const Quat blendedRotation = Quat::Slerp(source.rotation, target.rotation, blendRatio);
        const Vec3 blendedPosition = Vec3::Lerp(source.position, target.position, blendRatio);

        Matrix blendedMatrix{};
        if (static_cast<int32>(boneIndex) == rootNodeIndex)
            blendedMatrix = XMMatrixAffineTransformation(blendedScale, Vec3::Zero, blendedRotation, Vec3::Zero);
        else
            blendedMatrix = XMMatrixAffineTransformation(blendedScale, Vec3::Zero, blendedRotation, blendedPosition);

        _bones[boneIndex]->Update_TransformationMatrix(blendedMatrix);
    }
}

bool ModelCom::Step_SampleAnimationPose(const AnimPipelineContext& context)
{
    if (!_isSkeletal || _animations.empty())
        return false;

    if (_isAnimationPaused)
    {
        Refresh_CurrentAnimationPose();
        return false;
    }

    if (_isBlending)
    {
        const float ratio = (_blendDuration > 0.f)
            ? clamp(_blendElapsedTime / _blendDuration, 0.f, 1.f)
            : 1.f;

        if (_isSameAnimationBlending && ratio < 1.f)
        {
            _isFinished = _animations[_nextAnimIndex]->Update_TransformationMatrices(
                context.adjustedTimeDelta,
                _bones,
                _isAnimLoop,
                context.rootIndex);

            if (context.rootIndex >= 0)
            {
                const RootMotionFrame sourceDelta{};
                const auto& targetDelta = _animations[_nextAnimIndex]->Get_TransformVelocity(context.rootIndex);

                _rootOriginalFrameVelocity.position = Vec3::Lerp(sourceDelta.position, targetDelta.position, ratio);
                _rootOriginalFrameVelocity.rotation = Quat::Slerp(sourceDelta.rotation, targetDelta.rotation, ratio);
            }
            else
            {
                _rootOriginalFrameVelocity = {};
            }

            Apply_SameAnimationBlendPose(ratio, context.rootIndex);
            
            _blendElapsedTime += context.adjustedTimeDelta;
            if (_blendElapsedTime >= _blendDuration)
            {
                _isBlending = false;
                Clear_SameAnimationBlend();
                _skipInitialBlendSourceNotify = false;
                _currentAnimIndex = _nextAnimIndex;
                _currentAnimStateIndex = _nextAnimStateIndex;
                _isFinished = (!_isAnimLoop && _animations[_currentAnimIndex]->Get_Progress() >= 1.f);
            }
        }
        else if (ratio >= 1.f)
        {
            _isBlending = false;
            Clear_SameAnimationBlend();
            _skipInitialBlendSourceNotify = false;
            _currentAnimIndex = _nextAnimIndex;
            _currentAnimStateIndex = _nextAnimStateIndex;

            _isFinished = _animations[_currentAnimIndex]->Update_TransformationMatrices(
                context.adjustedTimeDelta,
                _bones,
                _isAnimLoop,
                context.rootIndex);

            _rootOriginalFrameVelocity =
                (context.rootIndex >= 0)
                ? _animations[_currentAnimIndex]->Get_TransformVelocity(context.rootIndex)
                : RootMotionFrame{};
        }
        else
        {
            _animations[_currentAnimIndex]->Blend_TransformMatrix(
                context.adjustedTimeDelta,
                _animations[_nextAnimIndex],
                ratio,
                _bones,
                _isAnimLoop,
                context.rootIndex);

            if (context.rootIndex >= 0)
            {
                const auto& curDelta = _animations[_currentAnimIndex]->Get_TransformVelocity(context.rootIndex);
                const auto& nextDelta = _animations[_nextAnimIndex]->Get_TransformVelocity(context.rootIndex);

                _rootOriginalFrameVelocity.position = Vector3::Lerp(curDelta.position, nextDelta.position, ratio);
                _rootOriginalFrameVelocity.rotation = Quaternion::Slerp(curDelta.rotation, nextDelta.rotation, ratio);
            }

            _blendElapsedTime += context.adjustedTimeDelta;
            if (_blendElapsedTime >= _blendDuration)
            {
                _isBlending = false;
                Clear_SameAnimationBlend();
                _skipInitialBlendSourceNotify = false;
                _currentAnimIndex = _nextAnimIndex;
                _currentAnimStateIndex = _nextAnimStateIndex;
                _isFinished = (!_isAnimLoop && _animations[_currentAnimIndex]->Get_Progress() >= 1.f);
            }
        }
    }
    else
    {
        _isFinished = _animations[_currentAnimIndex]->Update_TransformationMatrices(
            context.adjustedTimeDelta,
            _bones,
            _isAnimLoop,
            context.rootIndex);

        _rootOriginalFrameVelocity =
            (context.rootIndex >= 0)
            ? _animations[_currentAnimIndex]->Get_TransformVelocity(context.rootIndex)
            : RootMotionFrame{};
    }

    Apply_CurrentPlaySectionEnd(context);

    return _isFinished;
}

void ModelCom::Step_FinalizeAnimationFrame(const AnimPipelineContext& context)
{
    if (context.rootIndex >= 0)
    {
        _rootOriginalFrameVelocity.position =
            Vec3::TransformNormal(_rootOriginalFrameVelocity.position, _preLocalRotationMatrix);

        const float tempZ = _rootOriginalFrameVelocity.position.z;

        _rootOriginalFrameVelocity.position.z = -_rootOriginalFrameVelocity.position.y;
        _rootOriginalFrameVelocity.position.y = tempZ;




    }

    Mark_BoneMatricesDirty();

    if (_isAnimationPaused)
        return; // 포즈는 고정하되 pause 프레임에서 AN_PlaySound/ANS_Trail/ANS_QTE가 반복 실행되지 않게 막는다.

    const float currentTimeSec = Get_CurrentAnimationTimeSec();
    const bool animationChangedThisFrame = context.previousAnimIndex != _currentAnimIndex;
    const float notifyPreviousTimeSec = animationChangedThisFrame ? 0.f : context.previousTimeSec;
    const bool wrapped = (_isAnimLoop && currentTimeSec < notifyPreviousTimeSec);

    if (Ensure_AnimNotifyAssetLoaded())
    {
        //Debug_Profile::Scope profileScope("AnimMesh::PlayAnimation::Notify");

        const char* animationName = Get_AnimationName(_currentAnimIndex);
        AnimNotifyContext notifyContext{};
        notifyContext.owner = Get_Owner().get();
        notifyContext.model = this;
        notifyContext.modelGuid = _animNotifyAsset.modelGuid;
        notifyContext.animationName = animationName ? animationName : "";
        notifyContext.previousTimeSec = notifyPreviousTimeSec; // 전환 프레임에는 새 애니 시작부터 현재 시간까지 검사한다.
        notifyContext.currentTimeSec = currentTimeSec;
        notifyContext.deltaTime = context.adjustedTimeDelta;
        notifyContext.isLooping = _isAnimLoop;
        notifyContext.wrapped = wrapped;
        notifyContext.isPreview = !_isAnimNotifyExecutionEnabled;
        notifyContext.isCinematic = _isCinematicNotifyContext;

        Stop_NotifyStates_ExceptAnimation(notifyContext.animationName, _isAnimNotifyExecutionEnabled);

        if (!_skipInitialBlendSourceNotify)
        {
            if (const AnimNotifyData* animationData = Find_CurrentNotifyAnimation())
            {
                const bool executeEventNotifies = _isAnimNotifyExecutionEnabled && !_isBlending;
                Update_AnimNotifies(*animationData, notifyContext, executeEventNotifies);
                Update_AnimNotifyStates(*animationData, notifyContext, _isAnimNotifyExecutionEnabled);
            }
        }
    }

    {
    ENGINE_PROFILE_SCOPE_DETAIL("AnimMesh::PlayAnimation::StopNotifyStates");

        if (_isFinished && !_isAnimLoop)
            Stop_AllNotifyStates(_isAnimNotifyExecutionEnabled);
    }
}

void ModelCom::Step_RebuildCombinedBones()
{
    ENGINE_PROFILE_SCOPE_DETAIL("AnimMesh::PlayAnimation::CombinedBones");

    for (uint32 i = 0; i < _numBones; ++i)
        _bones[i]->Update_CombinedTransformationMatrix(_bones, _preLocalTransformMatrix);
}

HRESULT ModelCom::Ensure_AnimationComputeSource()
{
    if (_computeAnimationSource.meshBoneRefBuffers.size() == _meshes.size())
        return S_OK;

    _computeAnimationSource = {};

    _computeAnimationSource.meshBoneRefs.resize(_meshes.size());
    _computeAnimationSource.meshBoneRefBuffers.resize(_meshes.size());

    for (uint32 meshIndex = 0; meshIndex < _meshes.size(); ++meshIndex)
    {
        const Shared<SkeletalMesh> skeletalMesh =
            dynamic_pointer_cast<SkeletalMesh>(_meshes[meshIndex]);

        if (skeletalMesh == nullptr)
            continue;

        auto& refs = _computeAnimationSource.meshBoneRefs[meshIndex];
        CHECK_FAILED(skeletalMesh->Build_ComputeMeshBoneRefs(refs), E_FAIL);

        if (!refs.empty())
        {
            _computeAnimationSource.meshBoneRefBuffers[meshIndex] =
                ComputeStructuredBuffer::Create(
                    _device,
                    _context,
                    sizeof(AnimComputeMeshBoneRef),
                    static_cast<uint32>(refs.size()),
                    refs.data());

            if (_computeAnimationSource.meshBoneRefBuffers[meshIndex] == nullptr)
                return E_FAIL;
        }
    }

    return S_OK;
}

void ModelCom::Add_AnimModifier(const Shared<AnimationModifier>& modifier)
{
    if (modifier)
    {
        modifier->Bind_Model(this);
        _modifiers.push_back(modifier);
    }
}

AnimationModifier* ModelCom::Find_AnimModifierByTag(const wstring& modifierTag) const
{
    for (auto mod : _modifiers)
    {
        if (mod->Get_ModifierTag() == modifierTag)
            return mod.get();
    }
    
    return nullptr;
}

void ModelCom::Evaluate_AnimationModifiers(float timeDelta)
{
    for (auto& modifier : _modifiers)
    {
        if (modifier != nullptr)
            modifier->Evaluate_Modifier(this, timeDelta);
    }
}

void ModelCom::Evaluate_AnimationModifiersLeaderPose(float timeDelta, bool leaderPoseContext)
{
    for (auto& modifier : _modifiers)
    {
        if (modifier == nullptr)
            continue;

        if (leaderPoseContext && !modifier->Is_EvaluateOnLeaderPose())
            continue;

        modifier->Evaluate_Modifier(this, timeDelta);
    }
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

        /* 바인드 포즈 초기화: 애니메이션 없이도 CombinedHDR 행렬이 유효하도록 1회 계산 */
        for (uint32 i = 0; i < _numBones; ++i)
        {
            _bones[i]->Update_CombinedTransformationMatrix(_bones, _preLocalTransformMatrix);
        }
    }

    _combinedBoneMatrixCache.resize(_numBones, Matrix::Identity);
    _meshBoneMatrixCaches.resize(_meshes.size());

    Mark_BoneMatricesDirty();
    _leaderPoseMatricesValid = false;

    _hasLocalBounds = hasBounds;
    if (hasBounds)
    {
        _localBoundsMin = minBounds;
        _localBoundsMax = maxBounds;
    }

    stream.close();

    CHECK_FAILED(Try_LoadMorphData(resolvedPath), E_FAIL);

    size_t meshBytes = 0;
    for (const Shared<Mesh>& mesh : _meshes)
    {
        if (mesh)
            meshBytes += mesh->Get_DebugEstimatedBufferBytes();
    }

    if (meshBytes > 0)
    {
        const string modelCategory = DebugMemoryTracker::Guess_ModelCategory(_sourceModelPath);
        const string category = modelCategory + ".mesh";
        DebugMemoryTracker::Record_CPUBytes(
            category.c_str(),
            "ModelCom.Ready_Model",
            _sourceModelPath,
            meshBytes);
    }

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

            {   // 파일명 기반으로 작동하게
                string stemName = entry.path().stem().string();
                animData.name = stemName;
            }

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
            Shared<Animation> animation = Animation::Create(animData, channels);
            if (!animation)
            {
                LOG_WARN("[ModelCom] Skipped invalid animation: {}", animName);
                stream.close();
                continue;
            }

            _mappedAnimIndices[animName] = trueAnimIndex;
            _animations.push_back(animation);
            stream.close();
            ++trueAnimIndex;
        }
    }

    _numAnimations = trueAnimIndex;

    const size_t animationBytes = Get_DebugEstimatedAnimationBytes();
    if (animationBytes > 0)
    {
        const string modelCategory = DebugMemoryTracker::Guess_ModelCategory(_sourceModelPath);
        const string category = modelCategory + ".animation";
        DebugMemoryTracker::Record_CPUBytes(
            category.c_str(),
            "ModelCom.Ready_Animation",
            _sourceModelPath,
            animationBytes);
    }

    return S_OK;
}

bool ModelCom::Ensure_AnimStateAssetLoaded(const string &sourceObjectGUID)
{
	if (!sourceObjectGUID.empty())
	{
		wstring resolvedPath = GAME->Resolve_AssetPath(sourceObjectGUID);
		// sourceObjectGUID가 animmap asset일 때만 direct load를 허용한다.
		// model guid가 들어오면 prefab context 기반 매핑 탐색까지 내려가야 한다.
		if (!resolvedPath.empty())
		{
            const fs::path resolvedAssetPath = fs::path(resolvedPath);
            const string resolvedFileName = resolvedAssetPath.filename().string();
            if (resolvedFileName.find(".animmap.json") != string::npos)
            {
			    return AnimState_Parser::Load_StateAnimations_FromFile(
				    resolvedPath, _animations, _stateAnimIndices, &_animEnumType);
            }
		}
	}

	string targetPrefabGuid = "";
	string targetPrefabName = "";
	if (Get_Owner())
	{
		auto prefabContextOwner = Get_Owner();
		if (auto containerOwner = prefabContextOwner->Get_Owner())
		{
			if (!containerOwner->Get_SourcePrefabName().empty())
			{
				prefabContextOwner = containerOwner;
			}
		}

		targetPrefabName = prefabContextOwner->Get_SourcePrefabName();
		if (targetPrefabName.empty())
		{
			string selfClass = prefabContextOwner->Get_ReflectionInfo().className;
			string ownerClass = "";
			if (auto ownerOfContext = prefabContextOwner->Get_Owner())
			{
				ownerClass = ownerOfContext->Get_ReflectionInfo().className;
			}
			targetPrefabName = AnimState_Parser::Find_PrefabName_ByStaticClass(ownerClass, selfClass);
			if (!targetPrefabName.empty())
			{
				prefabContextOwner->Set_SourcePrefabName(targetPrefabName);
			}
		}

		targetPrefabGuid = AnimState_Parser::Get_PrefabGuid(targetPrefabName);
	}

	return AnimState_Parser::Load_StateAnimations(_sourceModelPath, _animations, _stateAnimIndices, &_animEnumType, targetPrefabGuid, targetPrefabName);
}

bool ModelCom::Refresh_AnimStateMappingFromOwnerContext()
{
    return Ensure_AnimStateAssetLoaded("");
}

bool ModelCom::Load_AnimStateMappingFromFile(const wstring& animMapPath)
{
    return AnimState_Parser::Load_StateAnimations_FromFile(
        animMapPath,
        _animations,
        _stateAnimIndices,
        &_animEnumType);
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
    _missingAnimationWarningKeys.clear();
    _computeAnimationSource = {};

    _isFinished = false;
    _isAnimLoop = false;
    _isBlending = false;
    Clear_SameAnimationBlend();
    _hasExplicitAnimation = false;
    _skipInitialBlendSourceNotify = false;
    _currentAnimIndex = 0;
    _nextAnimIndex = 0;
    _currentAnimStateIndex = UINT32_MAX;
    _nextAnimStateIndex = UINT32_MAX;
    _blendDuration = 0.f;
    _blendElapsedTime = 0.f;

    _isRootMotion = false;
    _rootNodeIndex = -1;
    _rootOriginalFrameVelocity = {};
    _rootFrameVelocity = {};

    _combinedBoneMatrixCache.clear();
    _meshBoneMatrixCaches.clear();

    _boneMatricesDirty = true;
    _leaderPoseMatricesValid = false;
    _modelDeform.reset();

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

    _meshBoneMatrixCaches.clear();
    _combinedBoneMatrixCache.clear();

    _mappedAnimIndices.clear();
    _missingAnimationWarningKeys.clear();
    _stateAnimIndices.clear();
    _animations.clear();
    Clear_SameAnimationBlend();
    _materials.clear();
    _meshes.clear();
    _bones.clear();
}

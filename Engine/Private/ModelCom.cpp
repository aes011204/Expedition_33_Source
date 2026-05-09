#include "ModelCom.h"
#include "GameInstance.h"
#include "Mesh.h"
#include "Bone.h"
#include "ShaderCom.h"
#include "Material.h"
#include "Animation.h"
#include "SkeletalMesh.h"
#include "StaticMesh.h"
#include <limits>

#include "AnimNotify_Parser.h"
#include "AnimNotify.h"
#include "AnimNotifyState.h"

ModelCom::ModelCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component{ device, context } {
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
{
    for (auto& prototypeAnim : prototype._animations)
        _animations.push_back(prototypeAnim->Clone());

    for (auto& prototypeBone : prototype._bones)
        _bones.push_back(prototypeBone->Clone());

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
    for (int32 i = 0; i < _numBones; ++i)
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

// TODO : 기존 애니메이션이면 progress 초기화 하고 정리, 그게 아니라면 바로 다음 애니메이션 초기로 세팅
void ModelCom::Set_Animation(uint32 animIndex, bool isLoop)
{
    if (animIndex >= _animations.size()) return;

    _currentAnimIndex = animIndex;
    _isAnimLoop = isLoop;
    _isBlending = false;
    _animations[_currentAnimIndex]->Reset();
}

// TODO : BlendSetting으로 보간 애니메이션 세팅 이미
void ModelCom::Set_Animation(uint32 animIndex, float blendDuration, bool isLoop)
{
    if (animIndex >= _animations.size()) return;

    _nextAnimIndex = animIndex;
    _isAnimLoop = isLoop;
    _isBlending = true;
    _blendDuration = blendDuration;
    _blendElapsedTime = 0.f;
    _animations[_nextAnimIndex]->Reset();
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

    return _bones[boneIndex]->Get_CombinedTransformationMatrixPtr();
}

const Matrix* ModelCom::Get_BoneMatrixPtr(uint32 boneIndex) const
{
    return _bones[boneIndex]->Get_CombinedTransformationMatrixPtr();
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

bool ModelCom::Play_Animation(float timeDelta)
{
    if (!_isSkeletal || _animations.empty()) return false;

    if (_isAnimationPaused)
    {
        Refresh_CurrentAnimationPose();
        return false;
    }

    const float previousTimeSec = Get_CurrentAnimationTimeSec();

    bool isFinish = false;
    int32 rootIndex = _isRootMotion ? _rootNodeIndex : -1;

    if (_isBlending)
    {
        _blendElapsedTime += timeDelta;
        float ratio = _blendElapsedTime / _blendDuration;

        if (ratio >= 1.f)
        {
            _isBlending = false;
            _currentAnimIndex = _nextAnimIndex;
            isFinish = _animations[_currentAnimIndex]->Update_TransformationMatrices(timeDelta, _bones, _isAnimLoop, rootIndex);
            _rootOriginalFrameVelocity = rootIndex >= 0 ? _animations[_currentAnimIndex]->Get_TransformVelocity(rootIndex) : RootMotionFrame{};
        }
        else
        {
            _animations[_currentAnimIndex]->Blend_TransformMatrix(timeDelta, _animations[_nextAnimIndex], ratio, _bones, _isAnimLoop, rootIndex);

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
        isFinish = _animations[_currentAnimIndex]->Update_TransformationMatrices(timeDelta, _bones, _isAnimLoop, rootIndex);
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
            context.deltaTime = timeDelta;
            context.isLooping = _isAnimLoop;
            context.wrapped = wrapped;
            context.isPreview = !_isAnimNotifyExecutionEnabled;

            Update_AnimNotifies(*animationData, context, _isAnimNotifyExecutionEnabled);
            Update_AnimNotifyStates(*animationData, context, _isAnimNotifyExecutionEnabled);
        }
    }

    if (isFinish && !_isAnimLoop)
        Stop_AllNotifyStates(_isAnimNotifyExecutionEnabled);

    return isFinish;
}

HRESULT ModelCom::Bind_Material(ShaderCom* shader, const char* constantName, uint32 meshIndex, MaterialTextureSlot slot, uint32 textureIndex)
{
    return _materials[_meshes[meshIndex]->Get_MaterialIndex()]->Bind_Material(shader, constantName, slot, textureIndex);
}

HRESULT ModelCom::Bind_BoneMatrices(ShaderCom* shader, const char* constantName, uint32 meshIndex)
{
    if (_numBones == 0) return S_OK;

    return _meshes[meshIndex]->Bind_BoneMatrices(shader, constantName, _bones);
}

bool ModelCom::Try_Get_LocalBounds(Vec3& outMin, Vec3& outMax) const
{
    if (!_hasLocalBounds)
        return false;

    outMin = _localBoundsMin;
    outMax = _localBoundsMax;
    return true;
}

wstring ModelCom::Normalize_ModelAssetPath(const char* modelFilePath)
{
    string resolvedPath = modelFilePath ? modelFilePath : "";

    if (const size_t extPos = resolvedPath.rfind(".fbx"); extPos != string::npos)
        resolvedPath = resolvedPath.substr(0, extPos) + ".model";

    return fs::absolute(fs::path(String::ToWString(resolvedPath))).lexically_normal().wstring();
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

void ModelCom::Set_CurrentAnimationTimeSec(float timeSec)
{
    if (_currentAnimIndex >= _animations.size())
        return;

    _animations[_currentAnimIndex]->Set_CurrentTimeSec(timeSec);
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
                _activeNotifyStates[activeIndex].lastTimeSec = context.currentTimeSec;

                if (executeNotifies)
                    _activeNotifyStates[activeIndex].notifyState->On_Tick(context);
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

HRESULT ModelCom::Ready_Model(const char* modelPath)
{
    std::string resolvedPath(modelPath);
    {
        size_t extPos = resolvedPath.rfind(".fbx");
        if (extPos != std::string::npos)
            resolvedPath = resolvedPath.substr(0, extPos) + ".model";
    }
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
        }
        else
        {
            meshData.boneIndices = nullptr;
            meshData.offsetMatrices = nullptr;
            _meshes.push_back(StaticMesh::Create(_device, _context, meshData, _preLocalTransformMatrix));
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

            _animations.push_back(Animation::Create(animData, channels));
            stream.close();
            ++trueAnimIndex;
        }
    }

    _numAnimations = trueAnimIndex;

    return S_OK;
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

    _animations.clear();
    _materials.clear();
    _meshes.clear();
    _bones.clear();
}



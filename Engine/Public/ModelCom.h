#pragma once

#include "AnimNotify_Types.h"
#include "AnimCompute_Types.h"
#include "Component.h"
#include "Material.h"

NS_BEGIN(Engine)

class Mesh;
class Material;
class Bone;
class Animation;
class ComputeStructuredBuffer;
class ComputeShaderCom;
class AnimComputeRuntime;

class ENGINE_DLL ModelCom final : public Component
{
    friend class AnimComputeRuntime;

private:
    struct ComputeAnimationSourceCache
    {
        vector<vector<AnimComputeMeshBoneRef>> meshBoneRefs{};

        vector<Shared<ComputeStructuredBuffer>> meshBoneRefBuffers{};
    };

public:
    ModelCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ModelCom(const ModelCom& prototype);
    ~ModelCom() override = default;

public:
    ModelType Get_ModelType() const { return _type; }
    uint32 Get_MaxTextureType() const { return _maxTextureType; }
    size_t Get_NumMeshes() const { return _meshes.size(); }
    size_t Get_NumMaterials() const { return _materials.size(); }
    size_t Get_NumBones() const { return _bones.size(); }
    size_t Get_NumAnimations() const { return _animations.size(); }
    const Matrix& Get_PreLocalTransformMatrix() const { return _preLocalTransformMatrix; }
    const vector<Shared<Mesh>>& Get_Meshes() const { return _meshes; };

    uint32 Get_MeshMaterialIndex(uint32 meshIndex) const;
    Shared<Material> Get_Material(uint32 materialIndex) const;

    const Material::TextureSlot* Find_MaterialSlot(uint32 meshIndex, std::initializer_list<MaterialTextureSlot> materialSlots, uint32 textureIndex = 0) const;
    const wstring& Get_SourceModelPath() const { return _sourceModelPath; }

public:
    const vector<MaterialBinding>& Get_MaterialBindings() const { return _materialBindings; }
    vector<MaterialBinding>& Get_MaterialBindings() { return _materialBindings; }

    void Set_BaseBundleKey(uint32 materialIndex, const string& bundleKey);
    void Set_InstanceOverride(uint32 materialIndex, const MaterialInstanceOverride& overrideData);
    void Clear_InstanceOverride(uint32 materialIndex);

    void Apply_BaseMaterialFromBundle_AllSlots();
    void Apply_EffectiveMaterial(uint32 materialIndex);

public:
    // MaterialBuilder가 prefab/json에서 binding을 채울 때 사용한다.
    // (material slot 수에 맞춰 _materialBindings를 준비하고 기본 baseBundleKey를 설정)
    void Ensure_MaterialBindings();

public:
    bool  Is_Skeletal() const { return _isSkeletal; }
	int32 Get_BoneIndex(const string& boneName) const;
    const string& Get_BoneName(uint32 boneIndex) const;
    const Matrix* Get_BoneMatrixPtr(const char* boneName) const;
    const Matrix* Get_BoneMatrixPtr(uint32 boneIndex) const;

    int32 Get_BoneParentIndex(uint32 boneIndex) const;
    const Matrix& Get_BoneLocalMatrix(uint32 boneIndex) const;
    void Set_BoneLocalMatrix(uint32 boneIndex, const Matrix& localMatrix);

    // 모든 bone의 CombinedHDR Matrix를 parent 순서대로 다시 계산
    void Rebuild_BoneCombinedMatrices();

public:
    float Get_FrameSpeed() const { return _frameSpeed; }
    void Set_FrameSpeed(float speed) { _frameSpeed = speed; }

    float Get_CurrentProgress() const;
    float Get_CurrentFrame() const;

    bool Is_AnimationFinished() const { return _isFinished; }
    bool Is_AnimationBlending() const { return _isBlending; }
    void Set_Animation(uint32 animIndex, bool isLoop = false, float startFrame = 0.f);
    void Set_Animation(uint32 animIndex, float blendDuration, bool isLoop = false, float startFrame = 0.f);
    void Set_Animation(const string& animName, bool isLoop = false, float startFrame = 0.f);
    void Set_Animation(const string& animName, float blendDuration, bool isLoop = false, float startFrame = 0.f);

    bool Set_AnimationByState(uint32 stateIndex, bool isLoop = false, float startFrame = 0.f);
    bool Set_AnimationByState(uint32 stateIndex, float blendDuration, bool isLoop = false, float startFrame = 0.f);
    const char* Get_AnimationName(uint32 animationIndex) const;

public:
    void Set_RootMotionEnabled(bool enable) { _isRootMotion = enable; }
    void Set_LocalRootNode(int32 nodeIndex) { _rootNodeIndex = nodeIndex; _isRootMotion = true; }
    const RootMotionFrame& Get_RootTransformVelocity() const { return _rootOriginalFrameVelocity; }
    const RootMotionFrame& Get_RootTransformVelocity(float scaleOffset);
    float Get_RootMotionTranslationRatio() const;

public:
    HRESULT Initialize_Prototype(ModelType type, const char* modelFilePath, const Matrix& preLocalTransformMatrix);
    HRESULT Initialize(void* arg) override;
    json To_Json() const override;
    void From_Json(const json& data) override;
    HRESULT Reload_ModelFromGuid(const string& guid, ModelType modelType);
    HRESULT Render(uint32 meshIndex);
    HRESULT RenderInstanced(uint32 meshIndex, Buffer* instanceVB, uint32 instanceStride, uint32 instanceCount);

public:
    bool Play_Animation(float timeDelta);

public:
    HRESULT Bind_Material(class ShaderCom* shader, const char* constantName, uint32 meshIndex, MaterialTextureSlot slot, uint32 textureIndex = 0);
    HRESULT Bind_BoneMatrices(class ShaderCom* shader, const char* constantName, uint32 meshIndex);
    bool Try_Get_LocalBounds(Vec3& outMin, Vec3& outMax) const;

    HRESULT Prepare_ComputeSkinning(ComputeShaderCom* computeShader); // body 전용 실험에서 호출
    HRESULT Render_ComputeSkinned(uint32 meshIndex);                  // compute output VB로 draw

    // for map StaticMesh
    void Apply_MaterialByAssetName(uint32 materialIndex, const string& bundleKey, const string& assetName);

    // bone matrix 준비 한번만 하도록
    HRESULT Prepare_BoneMatrixBuffers();
    HRESULT Bind_PreparedBoneMatrices(ShaderCom* shader, const char* constantName, uint32 meshIndex);

public: /* Guid */
    static wstring Normalize_ModelAssetPath(const char* modelFilePath);

public: /* AnimNotify */
    uint32  Get_CurrentAnimationIndex() const { return _currentAnimIndex; }
    uint32  Get_AnimationIndex() const;
    uint32  Get_AnimationStateIndex() const;

    float   Get_CurrentAnimationDurationSec() const;
    float   Get_CurrentAnimationTimeSec() const;

    bool    Is_AnimationPaused() const { return _isAnimationPaused; }
    void    Set_AnimationPaused(bool paused);

    void    Set_CurrentAnimationTimeSec(float timeSec);
    void    Refresh_CurrentAnimationPose();

    // 현재 모델의 notify asset lazy-load
    bool    Ensure_AnimNotifyAssetLoaded();
    // 소스 모델이 바뀌면 notify 캐시 무효화
    void    Invalidate_AnimNotifyAsset();

    void    Set_AnimNotifyExecutionEnabled(bool enabled) { _isAnimNotifyExecutionEnabled = enabled; }
    bool    Is_AnimNotifyExecutionEnabled() const { return _isAnimNotifyExecutionEnabled; }

private:
    // 현재 애니메이션 이름으로 노티파이 데이터 찾기
    const AnimNotifyData* Find_CurrentNotifyAnimation() const;
    void    Stop_AllNotifyStates(bool executeEndCheck);
    // 다른 애니메이션에 속한 active state 정리
    void    Stop_NotifyStates_ExceptAnimation(const string& keepAnimationName, bool executeEndCheck);

    // 현재 재생 구간에서 event notify를 갱신한다.
    void    Update_AnimNotifies(
        const AnimNotifyData& animationData,
        const AnimNotifyContext& context,
        bool executeNotifies);

    // 현재 재생 구간에서 state notify를 갱신한다.
    void Update_AnimNotifyStates(
        const AnimNotifyData& animationData,
        const AnimNotifyContext& context,
        bool executeNotifies);

    // 루프 wrap을 포함해서 특정 시점이 재생 구간 안에 있는지 판정한다.
    static bool Is_NotifyTimeInRange(float targetTime, float previousTime, float currentTime, bool wrapped);

    // 현재 시간이 state 활성 범위 안에 있는지 판정한다.
    static bool Is_NotifyStateActiveTime(float currentTime, float startTime, float duration);

    // active state 목록에서 동일 state를 찾는다.
    static int32 Find_ActiveNotifyStateIndex(
        const vector<ActiveAnimNotifyState>& activeStates,
        const string& animationName,
        int32 stateIndex);

public:
    bool Ensure_AnimStateAssetLoaded(const string& sourceObjectGUID);
    bool Refresh_AnimStateMappingFromOwnerContext();


public: /* 머리 붙이기 */
    void Set_LeaderPoseComponent(const ModelCom* leaderModel);
    void Clear_LeaderPoseComponent();
    bool Has_LeaderPoseComponent() const { return _leaderPoseModel != nullptr; }

    void Update_LeaderPoseMatrices();

public:
    void Set_AnimEnumType(const wstring& enumType) { _animEnumType = enumType; }
    const wstring& Get_AnimEnumType() const { return _animEnumType; }

private:
    // bone pose 변경으로 Matrix를 다시 만들어야하는지?
    void    Mark_BoneMatricesDirty();
    // dirty 가 마크됐을 때 재생성 진행. leaderPose 여기서 갱신 X
    HRESULT Ensure_MeshBoneMatrices();

    HRESULT Ensure_ComputeSkinningBuffers();
    HRESULT Ensure_ComputeSkinningConstantBuffer();
    HRESULT Update_ComputeSkinningConstantBuffer(uint32 vertexCount);

private:
    Matrix  Convert_LeaderPoseMatrix(const Matrix& leaderMatrix) const;
    Matrix  Build_LeaderPoseMatrix(uint32 boneIndex, vector<uint8>& visited);
    void    Build_LeaderPoseRequiredBones();
    void    Add_LeaderPoseRequiredBone(uint32 boneIndex);
    void    Mark_LeaderPoseRequiredBone(uint32 boneIndex, vector<uint8>& requiredFlags);

    HRESULT Ensure_MeshBoneMatrixBuffers();

public: /* Compute Shader */
    void    Begin_AnimationPipeline(float timeDelta, AnimPipelineContext& outContext);
    bool    Step_SampleAnimationPose(const AnimPipelineContext& context);
    void    Step_RebuildCombinedBones();
    void    Step_FinalizeAnimationFrame(const AnimPipelineContext& context);
    HRESULT Step_PrepareBoneBuffers();
    HRESULT Step_ComputeSkinning(ComputeShaderCom* computeShader);
    void    Set_ComputeCombinedBoneMatrices(const vector<Matrix>& combinedBones);
    void    Clear_ComputeCombinedBoneMatrices();

private:
    HRESULT Ensure_AnimationComputeSource();

private:
    ModelType					        _type = { ModelType::END };
    bool                                _isSkeletal{ false };
    Matrix					            _preLocalTransformMatrix = {};
    bool                                _hasLocalBounds{ false };
    Vec3                                _localBoundsMin{};
    Vec3                                _localBoundsMax{};

private:
    uint32                              _maxTextureType{};
    uint32						        _numMeshes{};
    uint32						        _numMaterials{};
    uint32                              _numBones{};
    uint32                              _numAnimations{};

    vector<Shared<Mesh>>	            _meshes;
    vector<Shared<Material>>	        _materials;
    vector<Shared<Bone>>		        _bones;
    vector<Shared<Animation>>	        _animations;
    vector<uint32>                      _stateAnimIndices;
    unordered_map<string, uint32>       _mappedAnimIndices;

    struct AppliedMaterialAsset
    {
        string bundleKey{};
        string assetName{};
    };

    vector<AppliedMaterialAsset>        _appliedMaterialAssets;

private: /* For Animation Control */
    bool                                _isFinished{ false };
    bool						        _isAnimLoop{ false };
    bool                                _isBlending{ false };
    uint32						        _currentAnimIndex{ 0 };
    uint32                              _nextAnimIndex{ 0 };
    uint32                              _currentAnimStateIndex{ UINT32_MAX };
    uint32                              _nextAnimStateIndex{ UINT32_MAX };
    float                               _frameSpeed{ 1.f };
    float                               _blendDuration{ 0.f };
    float                               _blendElapsedTime{ 0.f };

private: /* For Root Motion */
    bool                                _isRootMotion{ false };
    int32                               _rootNodeIndex{ -1 }; // -1 일때는 최상위 노드 기준으로
    RootMotionFrame                     _rootOriginalFrameVelocity{};
    RootMotionFrame                     _rootFrameVelocity{};

private: /* guid */
    wstring                             _sourceModelPath = {};

private:
    vector<MaterialBinding>             _materialBindings{};

private: /* notify */
    bool                                _isAnimationPaused = false;

    bool                                _isAnimNotifyAssetLoaded = false;
    bool                                _isAnimNotifyExecutionEnabled = true;

    AnimNotifyAsset                     _animNotifyAsset{};
    vector<ActiveAnimNotifyState>       _activeNotifyStates{};

private: /* 머리 붙이기 */
    const ModelCom* _leaderPoseModel = nullptr;

    vector<int32>       _leaderPoseBoneMap;           // follower bone index => leader bone index
    vector<Matrix>      _leaderPoseCombinedMatrices;  // follower 기준으로 최종 행렬

    vector<uint32>      _leaderPoseRequiredBones;

    mutable vector<uint8> _leaderPoseVisited;
    uint64 _boneUpdateGeneration = 0;   // body가 bone을 갱신할 때마다 1씩 증가
    uint64 _leaderPoseLastSyncedGeneration = UINT64_MAX; // follower가 마지막으로 sync한 generation

    // 프레임마다 계산되지 않게 캐싱
    vector<Matrix>          _combinedBoneMatrixCache;
    vector<vector<Matrix>>  _meshBoneMatrixCaches;
    bool                    _boneMatricesDirty = true;
    bool                    _leaderPoseMatricesValid = false;

private: /* Compute Shader */
    ComputeAnimationSourceCache _computeAnimationSource{};

    vector<Shared<ComputeStructuredBuffer>> _meshBoneMatrixBuffers;
    bool    _boneMatrixBuffersDirty = true;

    vector<Shared<ComputeStructuredBuffer>> _meshSourceVertexBuffers;   // VTXANIMMESH input
    vector<Shared<ComputeStructuredBuffer>> _meshComputedVertexBuffers; // compute output structured buffer
    vector<ComPtr<Buffer>> _meshComputedDrawVertexBuffers;              // 실제 draw에 사용할 vertex buffer
    ComPtr<Buffer> _computeSkinningConstantBuffer{};
    bool _computeSkinningDirty = true;
    vector<Matrix> _computeCombinedBoneMatrices{};


private: // Model Custom Animation Mapping
    wstring             _animEnumType{};


private:
    HRESULT Ready_Model(const char* modelPath);
    HRESULT Ready_Animation(const char* modelPath);
    void Reset_ModelData();
    static ModelType Parse_ModelType(const string& modelType);
    static string To_ModelTypeString(ModelType modelType);

public:
    static Shared<ModelCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, ModelType type, const char* modelFilePath, const Matrix& preLocalTransformMatrix = XMMatrixIdentity());
    virtual Shared<Component> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END


/*
  // 1. 필요한 정보들
 float targetDistance = 10.f; // 내가 이번 애니메이션 동안 가야 할 총 목표 거리
 Vec3 targetDirection = Vec3(0, 0, 1); // 가야 할 방향 (정규화된 벡터)

 // 2. 비율 계산
 float currentRatio = _modelCom->Get_RootMotionTranslationRatio();
 float deltaRatio = currentRatio - _previousRatio; // 저번 프레임의 비율을 멤버 변수로 저장해둬야 함
 _previousRatio = currentRatio;

 // 3. 이번 프레임의 실제 이동량 계산
 float moveAmountThisFrame = targetDistance * deltaRatio;

 // 4. 캐릭터 이동 적용 (Transform이나 PhysX 컨트롤러에 적용)
 Vec3 deltaPosition = targetDirection * moveAmountThisFrame;
 _transformCom->Add_Position(deltaPosition);
 */

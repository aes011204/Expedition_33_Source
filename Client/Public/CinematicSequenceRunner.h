#pragma once

#include <unordered_map>

#include "Base.h"
#include "Cinematic_Types.h"

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(Client)

class CinematicCamera;
class Cinematic_Player;

class CinematicSequenceRunner : public Base
{
public:
    enum class CameraTrackKind
    {
        None,
        World,
        ActorRelative,
    };

public:
    bool Play_Sequence(
        const wstring& filePath,
        int trackIndex,
        bool applyCameraTrack = true,
        bool applyEventTrack = true,
        bool applyActorTransformTrack = true);
    bool Play_Sequence(
        const CINEMATIC_SEQUENCE& sequence,
        int trackIndex,
        bool applyCameraTrack = true,
        bool applyEventTrack = true,
        bool applyActorTransformTrack = true);

    void Pause_Sequence();
    void Resume_Sequence(
        bool applyCameraTrack = true,
        bool applyEventTrack = true,
        bool applyActorTransformTrack = true);
    void Set_CurrentFrame(int frame);
    void Refresh_CurrentFrameContext();
    void Preview_CurrentFrame(
        int frame,
        bool applyCameraTrack,
        bool applyEventTrack,
        bool applyActorTransformTrack,
        bool forceRebuildContext = false);
    void Preview_CurrentFrameExact(
        float frameExact,
        bool applyCameraTrack,
        bool applyEventTrack,
        bool applyActorTransformTrack,
        bool forceRebuildContext = false);
    bool Sync_PreviewSequence(const CINEMATIC_SEQUENCE& sequence, int trackIndex = -1);
    void Set_ActorTransformTrackPreviewSuspended(bool suspended);
    void Stop_Sequence();
    void Stop_SequenceKeepContext();
    void Update(float timeDelta, bool tickBoundAnimationsManually = false);
    bool Is_CinematicPlaying() const;
    bool Is_CinematicPaused() const;
    int  Get_CurrentFrame() const;
    float Get_CurrentFrameExact() const;
    Shared<GameObject> Get_BoundActor(const string& targetId) const;
    bool Is_ActorTransformTrackPreviewSuspended() const { return _suspendActorTransformTrackPreview; }

    FDelegateHandle Add_CompletedCallback(const FOnCinematicCompleted::FunctionType& callback);;
    void Clear_CompletedCallbacks(); // 시퀀스 교체/수동 중단 전에 오래된 완료 콜백을 제거
    void Queue_PrewarmSequence(const wstring& filePath, uint32 targetLevelIndex);
    void Flush_PrewarmQueue(uint32 createBudget = UINT32_MAX);
    void Clear_PrewarmQueue();

private:
    bool Resolve_CameraTrackSelection(
        int trackIndex,
        CINEMATIC_TRACK*& outWorldTrack,
        CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK*& outActorRelativeTrack,
        CameraTrackKind& outKind) const;
    bool Evaluate_ActorRelativeCameraTrack(
        const CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK& track,
        float currentFrameExact,
        CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME& outKeyframe) const;
    bool Build_ActorRelativeCameraPose(
        const CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK& track,
        float currentFrameExact,
        CAMERAPOSE& outPose) const;
    void Apply_ActiveCameraTrackPose(float currentFrameExact);
    bool Resolve_ActiveCameraTrackForFrame(
        float currentFrameExact,
        CINEMATIC_TRACK*& outWorldTrack,
        CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK*& outActorRelativeTrack,
        CameraTrackKind& outKind,
        int& outTrackIndex) const;
    bool Uses_CameraShots() const;
    static Vec3 Build_LookAtRotation(const Vec3& fromWorldPos, const Vec3& toWorldPos, const Vec3& fallbackRotation);

    struct BindingRuntimeState
    {
        Shared<GameObject> actor = nullptr;
        bool spawnedBySequence = false;
        CinematicCleanupPolicy cleanupPolicy = CinematicCleanupPolicy::DespawnOnStop;
        bool hasActivePlayAnim = false;
        CinematicPlayAnimEventData activePlayAnim{};
        int activePlayAnimEventFrame = 0;
        float pendingPlayAnimInitialAdvanceSec = -1.f;
        bool hasCinematicNotifyContextOverride = false;
        bool previousCinematicNotifyContext = false;
        bool hasAnimNotifyExecutionOverride = false;
        bool previousAnimNotifyExecutionEnabled = true;
    };

    void Execute_EventsUpToFrame(int currentFrame, float currentFrameExact);
    void Execute_Event(const CINEMATIC_EVENT& event, float currentFrameExact);

    bool Execute_SpawnEvent(const string& targetId, const CinematicSpawnEventData& payload);
    bool Execute_PlayAnimEvent(
        int eventFrame,
        float currentFrameExact,
        const string& targetId,
        const CinematicPlayAnimEventData& payload);
    bool Execute_DespawnEvent(const string& targetId, const CinematicDespawnEventData& payload);
    bool Execute_PartVisibilityEvent(const string& targetId, const CinematicPartVisibilityEventData& payload);
    bool Execute_WorldTimeScaleEvent(const CinematicWorldTimeScaleEventData& payload);
    Shared<GameObject> Find_ReusableActorByPrefabName(const string& prefabName) const;
    bool Is_ActorAlreadyBound(const Shared<GameObject>& actor) const;
    void Apply_CinematicDrivenState(const Shared<GameObject>& actor, bool cinematicDriven) const;
    void Bind_ActorToTarget(
        const string& targetId,
        const Shared<GameObject>& actor,
        bool spawnedBySequence,
        CinematicCleanupPolicy cleanupPolicy);

    void Apply_ActorTransformTracks(float currentFrameExact);
    void Apply_AuthoredActorTracks(float currentFrameExact);
    void Apply_PartRotationTracks(float currentFrameExact);
    bool Evaluate_ActorTransformTrack(
        const CINEMATIC_ACTOR_TRANSFORM_TRACK& track,
        float currentFrameExact,
        Vec3& outPosition,
        Vec3& outRotation) const;
    bool Evaluate_PartRotationTrack(
        const CINEMATIC_PART_ROTATION_TRACK& track,
        float currentFrameExact,
        Vec3& outRotation) const;

    void Apply_BoundActorAnimationPreview(float currentFrameExact);
    void Set_BoundActorAnimationPaused(bool paused);
    void Update_BoundActorAnimations(float timeDelta);
    void Cleanup_Context(bool forceRemoveAllSpawned = false);
    void Set_GameplayUIHiddenForCinematic(bool hidden) const;

public:
    struct PrewarmSequenceResult
    {
        wstring filePath{};
        bool succeeded = false;
        CINEMATIC_SEQUENCE sequence{};
        vector<string> spawnPrefabNames{};
    };

    struct PrewarmModelDependency
    {
        string modelGuid{};
        wstring modelPath{};
        wstring prototypeTag{};
        ModelType modelType = ModelType::NonAnim;
        Matrix preLocalTransform = Matrix::Identity;
    };

    struct PrewarmPrefabResult
    {
        fs::path prefabPath{};
        bool succeeded = false;
        json root{};
        vector<string> referencedPrefabNames{};
        vector<PrewarmModelDependency> modelDependencies{};
    };

private:
    void Drain_CompletedPrewarmSequences();
    void Drain_CompletedPrewarmPrefabs();
    void Queue_PrewarmPrefabName(const string& prefabName);
    void Queue_PrewarmPrefabPath(const fs::path& prefabPath);
    void Queue_PrewarmModelDependency(const PrewarmModelDependency& dependency);
    size_t Process_PrewarmModelDependencies(uint32 createBudget);
    Shared<GameObject> Find_ReusableActorByPrefabName(uint32 levelIndex, const string& prefabName) const;

private:
    CINEMATIC_SEQUENCE _sequence{};
    Shared<Cinematic_Player> _player = nullptr;
    Shared<CinematicCamera> _activeCinematicCamera = nullptr;
    std::unordered_map<string, BindingRuntimeState> _bindingStates{};
    uint32 _sequenceLevelIndex = ETOI(LevelType::GamePlay);
    int _trackIndex = 0;
    int _nextEventIndex = 0;
    CameraTrackKind _cameraTrackKind = CameraTrackKind::None;
    bool _suspendActorTransformTrackPreview = false;
    bool _applyCameraTrack = true;
    bool _applyEventTrack = true;
    bool _applyActorTransformTrack = true;
    bool _suppressAnimNotifyExecutionForBoundActors = false;
    bool _hasWorldTimeScaleOverride = false;
    float _restoreWorldTimeScale = 1.f;
    wstring _activeSequencePath{};

    std::unordered_map<wstring, CINEMATIC_SEQUENCE> _prewarmedSequences{};
    std::unordered_set<wstring> _queuedPrewarmSequencePaths{};
    deque<PrewarmSequenceResult> _completedPrewarmSequences{};
    std::unordered_set<wstring> _queuedPrewarmPrefabPaths{};
    std::unordered_set<wstring> _prewarmedPrefabPaths{};
    deque<PrewarmPrefabResult> _completedPrewarmPrefabs{};
    vector<PrewarmModelDependency> _pendingPrewarmModelDependencies{};
    std::unordered_set<wstring> _queuedPrewarmModelPrototypeTags{};
    mutable mutex _prewarmMutex{};
    atomic<uint32> _activePrewarmSequenceJobs = 0;
    atomic<uint32> _activePrewarmPrefabJobs = 0;
    size_t _prewarmedPrefabCount = 0;
    size_t _prewarmedModelCount = 0;

    FOnCinematicCompleted _onNaturalCompleted;

public:
    static Unique<CinematicSequenceRunner> Create();
    virtual void Free() override;
};

NS_END

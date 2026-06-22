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
    bool Sync_PreviewSequence(const CINEMATIC_SEQUENCE& sequence, int trackIndex = -1);
    void Set_ActorTransformTrackPreviewSuspended(bool suspended);
    void Stop_Sequence();
    void Stop_SequenceKeepContext();
    void Update(float timeDelta, bool tickBoundAnimationsManually = false);
    bool Is_CinematicPlaying() const;
    bool Is_CinematicPaused() const;
    int  Get_CurrentFrame() const;
    Shared<GameObject> Get_BoundActor(const string& targetId) const;
    bool Is_ActorTransformTrackPreviewSuspended() const { return _suspendActorTransformTrackPreview; }

private:
    struct BindingRuntimeState
    {
        Shared<GameObject> actor = nullptr;
        bool spawnedBySequence = false;
        CinematicCleanupPolicy cleanupPolicy = CinematicCleanupPolicy::DespawnOnStop;
        bool hasActivePlayAnim = false;
        CinematicPlayAnimEventData activePlayAnim{};
        int activePlayAnimEventFrame = 0;
    };

    void Execute_EventsUpToFrame(int currentFrame);
    void Execute_Event(const CINEMATIC_EVENT& event);

    bool Execute_SpawnEvent(const string& targetId, const CinematicSpawnEventData& payload);
    bool Execute_PlayAnimEvent(int eventFrame, const string& targetId, const CinematicPlayAnimEventData& payload);
    bool Execute_DespawnEvent(const string& targetId, const CinematicDespawnEventData& payload);

    void Apply_ActorTransformTracks(int currentFrame);
    bool Evaluate_ActorTransformTrack(
        const CINEMATIC_ACTOR_TRANSFORM_TRACK& track,
        int currentFrame,
        Vec3& outPosition,
        Vec3& outRotation) const;

    void Apply_BoundActorAnimationPreview(int currentFrame);
    void Set_BoundActorAnimationPaused(bool paused);
    void Update_BoundActorAnimations(float timeDelta);
    void Cleanup_Context(bool forceRemoveAllSpawned = false);

private:
    CINEMATIC_SEQUENCE _sequence{};
    Shared<Cinematic_Player> _player = nullptr;
    std::unordered_map<string, BindingRuntimeState> _bindingStates{};
    uint32 _sequenceLevelIndex = ETOI(LevelType::GamePlay);
    int _trackIndex = 0;
    int _nextEventIndex = 0;
    bool _suspendActorTransformTrackPreview = false;
    bool _applyCameraTrack = true;
    bool _applyEventTrack = true;
    bool _applyActorTransformTrack = true;

public:
    static Unique<CinematicSequenceRunner> Create();
    virtual void Free() override;
};

NS_END

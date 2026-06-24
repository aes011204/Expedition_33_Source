#include "CinematicSequenceRunner.h"

#include "Cinematic_Player.h"
#include "Cinematic_SequenceIO.h"
#include "CinematicCamera.h"
#include "ClientInstance.h"
#include "ContainerObject.h"
#include "GameInstance.h"
#include "Helper_String.h"
#include "ModelCom.h"
#include "Monster_WorldBase.h"
#include "PartObject.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"
#include <algorithm>
#include <cmath>

namespace
{
Shared<ModelCom> Find_TargetModel(const Shared<GameObject>& actor)
{
    if (actor == nullptr)
        return nullptr;

    if (const auto container = dynamic_pointer_cast<ContainerObject>(actor))
    {
        if (PartObject* bodyPart = container->Get_PartObject(L"Part_Body"))
        {
            if (Shared<ModelCom> bodyModel = bodyPart->Get_Component<ModelCom>())
                return bodyModel;
        }
    }

    return actor->Get_Component<ModelCom>();
}

PartObject* Find_TargetAnimPart(const Shared<GameObject>& actor)
{
    if (actor == nullptr)
        return nullptr;

    if (const auto container = dynamic_pointer_cast<ContainerObject>(actor))
        return container->Get_PartObject(L"Part_Body");

    return nullptr;
}
}

bool CinematicSequenceRunner::Play_Sequence(
    const wstring& filePath,
    int trackIndex,
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack)
{
    if (_player == nullptr)
        _player = Cinematic_Player::Create();

    if (_player == nullptr)
        return false;

    if (!_bindingStates.empty())
        Cleanup_Context();

    if (_player->Is_CinematicPlaying())
    {
        _player->Stop();
        Clear_CompletedCallbacks();
        Cleanup_Context();
    }

    const string stringPath = String::ToString(filePath);
    if (!Cinematic_SequenceIO::LoadSequenceFromFile(stringPath, _sequence))
        return false;

    if (trackIndex < 0 || trackIndex >= static_cast<int>(_sequence.cinematicTracks.size()))
        return false;

    Shared<CinematicCamera> cinematicCamera = nullptr;
    if (applyCameraTrack)
    {
        cinematicCamera = CLIENT->Request_Cinematic();
        if (cinematicCamera == nullptr)
            return false;
    }

    _bindingStates.clear();
    _trackIndex = trackIndex;
    _nextEventIndex = 0;
    _sequenceLevelIndex = GAME->Current_LevelIndex();
    _suspendActorTransformTrackPreview = false;
    _applyCameraTrack = applyCameraTrack;
    _applyEventTrack = applyEventTrack;
    _applyActorTransformTrack = applyActorTransformTrack;

    _player->Ready_CinematicPlayer(&_sequence, &_sequence.cinematicTracks[trackIndex], cinematicCamera, true, applyCameraTrack);
    if (_applyEventTrack)
        Execute_EventsUpToFrame(_sequence.startFrame);
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_ActorTransformTracks(_sequence.startFrame);
    if (applyCameraTrack)
        _player->Play();

    return true;
}

bool CinematicSequenceRunner::Play_Sequence(
    const CINEMATIC_SEQUENCE& sequence,
    int trackIndex,
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack)
{
    if(_player == nullptr)
        _player = Cinematic_Player::Create();

    if(_player == nullptr)
        return false;

    if (!_bindingStates.empty())
        Cleanup_Context();

    if (_player->Is_CinematicPlaying())
    {
        _player->Stop();
        Cleanup_Context();
    }

    if (trackIndex < 0 || trackIndex >= static_cast<int>(sequence.cinematicTracks.size()))
        return false;


    Shared<CinematicCamera> cinematicCamera = nullptr;
    if (applyCameraTrack)
    {
        cinematicCamera = CLIENT->Request_Cinematic();
        if (cinematicCamera == nullptr)
            return false;
    }

    _sequence = sequence;
    _bindingStates.clear();
    _trackIndex = trackIndex;
    _nextEventIndex = 0;
    _sequenceLevelIndex = GAME->Current_LevelIndex();
    _suspendActorTransformTrackPreview = false;
    _applyCameraTrack = applyCameraTrack;
    _applyEventTrack = applyEventTrack;
    _applyActorTransformTrack = applyActorTransformTrack;

   _player->Ready_CinematicPlayer(&_sequence, &_sequence.cinematicTracks[trackIndex], cinematicCamera, true, applyCameraTrack);
    if (_applyEventTrack)
        Execute_EventsUpToFrame(_sequence.startFrame);
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_ActorTransformTracks(_sequence.startFrame);
    if (applyCameraTrack)
        _player->Play();

    return true;
}

void CinematicSequenceRunner::Stop_Sequence()
{
    if (_player == nullptr)
        return;

    const bool wasPlaying = _player->Is_CinematicPlaying();
    if (!wasPlaying)
        return;

    _player->Stop();
    Clear_CompletedCallbacks();
    Cleanup_Context();
    CLIENT->Restore_PreviousCamera();
}

void CinematicSequenceRunner::Stop_SequenceKeepContext()
{
    if (_player == nullptr)
        return;

    const bool wasPlaying = _player->Is_CinematicPlaying();
    if (!wasPlaying)
        return;

    _player->Stop();
}

void CinematicSequenceRunner::Pause_Sequence()
{
    if (_player == nullptr || !_player->Is_CinematicPlaying())
        return;

    _player->Pause();
}

void CinematicSequenceRunner::Resume_Sequence(
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack)
{
    if (_player == nullptr || !_player->Is_CinematicPlaying())
        return;

    _suspendActorTransformTrackPreview = false;
    _applyCameraTrack = applyCameraTrack;
    _applyEventTrack = applyEventTrack;
    _applyActorTransformTrack = applyActorTransformTrack;
    _player->Set_ApplyCameraTrack(applyCameraTrack);
    Set_BoundActorAnimationPaused(false);
    _player->Resume();
}

void CinematicSequenceRunner::Set_CurrentFrame(int frame)
{
    if (_player == nullptr || !_player->Is_CinematicPlaying())
        return;

    const int targetFrame = std::clamp(frame, _sequence.startFrame, _sequence.endFrame);
    const int previousFrame = _player->Get_CurrentFrame();

    if (targetFrame < previousFrame)
    {
        Cleanup_Context(true);
        _nextEventIndex = 0;
    }

    _player->Set_CurrentFrame(targetFrame, _applyCameraTrack);
    if (_applyEventTrack)
        Execute_EventsUpToFrame(targetFrame);
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_ActorTransformTracks(targetFrame);
    if (_applyEventTrack)
        Apply_BoundActorAnimationPreview(targetFrame);
}

void CinematicSequenceRunner::Refresh_CurrentFrameContext()
{
    if (_player == nullptr || !_player->Is_CinematicPlaying())
        return;

    const int currentFrame = _player->Get_CurrentFrame();
    const bool suspendActorTransformTrackPreview = _suspendActorTransformTrackPreview;

    Cleanup_Context(true);
    _nextEventIndex = 0;
    _suspendActorTransformTrackPreview = suspendActorTransformTrackPreview;

    _player->Set_CurrentFrame(currentFrame, _applyCameraTrack);
    if (_applyEventTrack)
        Execute_EventsUpToFrame(currentFrame);
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_ActorTransformTracks(currentFrame);
    if (_applyEventTrack)
        Apply_BoundActorAnimationPreview(currentFrame);
}

void CinematicSequenceRunner::Preview_CurrentFrame(
    int frame,
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack,
    bool forceRebuildContext)
{
    if (_player == nullptr || !_player->Is_CinematicPlaying())
        return;

    const int targetFrame = std::clamp(frame, _sequence.startFrame, _sequence.endFrame);
    const int previousFrame = _player->Get_CurrentFrame();
    const bool suspendActorTransformTrackPreview = _suspendActorTransformTrackPreview;
    const bool requiresContextRebuild =
        forceRebuildContext ||
        targetFrame < previousFrame ||
        _applyEventTrack != applyEventTrack ||
        _applyActorTransformTrack != applyActorTransformTrack;

    _applyCameraTrack = applyCameraTrack;
    _applyEventTrack = applyEventTrack;
    _applyActorTransformTrack = applyActorTransformTrack;

    if (requiresContextRebuild)
    {
        Cleanup_Context(true);
        _nextEventIndex = 0;
        _suspendActorTransformTrackPreview = suspendActorTransformTrackPreview;
    }

    _player->Set_CurrentFrame(targetFrame, applyCameraTrack);

    if (applyEventTrack)
        Execute_EventsUpToFrame(targetFrame);

    if (applyEventTrack && applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_ActorTransformTracks(targetFrame);

    if (applyEventTrack)
        Apply_BoundActorAnimationPreview(targetFrame);
}

bool CinematicSequenceRunner::Sync_PreviewSequence(
    const CINEMATIC_SEQUENCE& sequence,
    int trackIndex)
{
    if (_player == nullptr || !_player->Is_CinematicPlaying())
        return false;

    if (trackIndex >= 0)
        _trackIndex = trackIndex;

    if (_trackIndex < 0 || _trackIndex >= static_cast<int>(sequence.cinematicTracks.size()))
        return false;

    _sequence = sequence;
    _player->Rebind_Sequence(&_sequence, &_sequence.cinematicTracks[_trackIndex]);
    return true;
}

void CinematicSequenceRunner::Set_ActorTransformTrackPreviewSuspended(bool suspended)
{
    _suspendActorTransformTrackPreview = suspended;

    if (!_suspendActorTransformTrackPreview && _player != nullptr && _player->Is_CinematicPlaying())
        Apply_ActorTransformTracks(_player->Get_CurrentFrame());
}

void CinematicSequenceRunner::Update(float timeDelta, bool tickBoundAnimationsManually)
{
    if (_player == nullptr)
        return;

    const bool wasPlaying = _player->Is_CinematicPlaying();
    if (!wasPlaying)
        return;

    _player->Update(timeDelta);
    if (_applyEventTrack)
        Execute_EventsUpToFrame(_player->Get_CurrentFrame());
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_ActorTransformTracks(_player->Get_CurrentFrame());

    if (tickBoundAnimationsManually && !_player->Is_CinematicPaused())
        Update_BoundActorAnimations(timeDelta);

    if (wasPlaying && !_player->Is_CinematicPlaying())
    {
        CLIENT->Restore_PreviousCamera(); // 완료 콜백이 전투 카메라를 요청할 수 있으므로, 시네마틱 카메라 복원은 먼저 끝낸다.
        _onNaturalCompleted.Broadcast();
        Clear_CompletedCallbacks(); // 완료 콜백

        Cleanup_Context();
    }
}

bool CinematicSequenceRunner::Is_CinematicPlaying() const
{
    return _player != nullptr && _player->Is_CinematicPlaying();
}

bool CinematicSequenceRunner::Is_CinematicPaused() const
{
    return _player != nullptr && _player->Is_CinematicPlaying() && _player->Is_CinematicPaused();
}

int CinematicSequenceRunner::Get_CurrentFrame() const
{
    if (_player == nullptr)
        return _sequence.startFrame;

    return _player->Get_CurrentFrame();
}

Shared<GameObject> CinematicSequenceRunner::Get_BoundActor(const string& targetId) const
{
    const auto iter = _bindingStates.find(targetId);
    if (iter == _bindingStates.end())
        return nullptr;

    return iter->second.actor;
}

FDelegateHandle CinematicSequenceRunner::Add_CompletedCallback(const FOnCinematicCompleted::FunctionType& callback)
{
    return _onNaturalCompleted.Add(callback);
}

void CinematicSequenceRunner::Clear_CompletedCallbacks()
{
    _onNaturalCompleted.Clear();
}

void CinematicSequenceRunner::Execute_EventsUpToFrame(int currentFrame)
{
    while (_nextEventIndex < static_cast<int>(_sequence.eventTrack.size()))
    {
        const CINEMATIC_EVENT& event = _sequence.eventTrack[_nextEventIndex];
        if (event.frame > currentFrame)
            break;

        Execute_Event(event);
        ++_nextEventIndex;
    }
}

void CinematicSequenceRunner::Execute_Event(const CINEMATIC_EVENT& event)
{
    if (const auto* spawn = get_if<CinematicSpawnEventData>(&event.payload))
    {
        Execute_SpawnEvent(event.targetId, *spawn);
    }
    else if (const auto* playAnim = get_if<CinematicPlayAnimEventData>(&event.payload))
    {
        Execute_PlayAnimEvent(event.frame, event.targetId, *playAnim);
    }
    else if (get_if<CinematicDespawnEventData>(&event.payload) != nullptr)
    {
        Execute_DespawnEvent(event.targetId, CinematicDespawnEventData{});
    }
}

bool CinematicSequenceRunner::Execute_SpawnEvent(const string& targetId, const CinematicSpawnEventData& payload)
{
    if (targetId.empty())
    {
        LOG_WARN("[CinematicSequenceRunner] SpawnEvent skipped: empty targetId. frame={}", Get_CurrentFrame());
        return false;
    }

    if (payload.prefabName.empty())
    {
        LOG_WARN("[CinematicSequenceRunner] SpawnEvent skipped: target='{}' has empty prefabName. frame={}",
            targetId,
            Get_CurrentFrame());
        return false;
    }

    if (const auto iter = _bindingStates.find(targetId); iter != _bindingStates.end() && iter->second.actor != nullptr)
    {
        LOG_INFO("[CinematicSequenceRunner] SpawnEvent ignored: target='{}' already spawned. frame={}",
            targetId,
            Get_CurrentFrame());
        return true;
    }

    Shared<GameObject> actor = Spawn_Prefab::Prefab(payload.prefabName)
        .AtLevel(_sequenceLevelIndex)
        .InLayer(payload.layerTag)
        .Position(payload.position)
        .Rotation(payload.rotation)
        .Spawn();
    if (actor == nullptr)
    {
        LOG_WARN("[CinematicSequenceRunner] SpawnEvent failed: target='{}' prefab='{}' did not spawn. frame={}",
            targetId,
            payload.prefabName,
            Get_CurrentFrame());
        return false;
    }

    BindingRuntimeState state{};
    state.actor = actor;
    state.spawnedBySequence = true;
    state.cleanupPolicy = payload.cleanupPolicy;

    if (Shared<ModelCom> model = Find_TargetModel(actor))
    {
        state.hasCinematicNotifyContextOverride = true;
        state.previousCinematicNotifyContext = model->Is_CinematicNotifyContext();
        model->Set_CinematicNotifyContext(true);
    }

    _bindingStates[targetId] = state;

    if (const auto worldMonster = dynamic_pointer_cast<Monster_WorldBase>(actor))
        worldMonster->Set_CinematicDriven(true);

    LOG_INFO(
        "[CinematicSequenceRunner] SpawnEvent success: target='{}' prefab='{}' frame={} pos=({:.2f}, {:.2f}, {:.2f})",
        targetId,
        payload.prefabName,
        Get_CurrentFrame(),
        payload.position.x,
        payload.position.y,
        payload.position.z);
    return true;
}

bool CinematicSequenceRunner::Execute_PlayAnimEvent(
    int eventFrame,
    const string& targetId,
    const CinematicPlayAnimEventData& payload)
{
    if (targetId.empty())
    {
        LOG_WARN("[CinematicSequenceRunner] PlayAnimEvent skipped: empty targetId. frame={}", Get_CurrentFrame());
        return false;
    }

    if (payload.animName.empty())
    {
        LOG_WARN("[CinematicSequenceRunner] PlayAnimEvent skipped: target='{}' has empty animName. frame={}",
            targetId,
            Get_CurrentFrame());
        return false;
    }

    Shared<GameObject> actor = Get_BoundActor(targetId);
    if (actor == nullptr)
    {
        LOG_WARN("[CinematicSequenceRunner] PlayAnimEvent failed: target='{}' is not bound. frame={}",
            targetId,
            Get_CurrentFrame());
        return false;
    }

    Shared<ModelCom> model = Find_TargetModel(actor);
    if (model == nullptr)
    {
        LOG_WARN("[CinematicSequenceRunner] PlayAnimEvent failed: target='{}' has no ModelCom. frame={}",
            targetId,
            Get_CurrentFrame());
        return false;
    }

    if (auto iter = _bindingStates.find(targetId); iter != _bindingStates.end() &&
        !iter->second.hasCinematicNotifyContextOverride)
    {
        iter->second.hasCinematicNotifyContextOverride = true;
        iter->second.previousCinematicNotifyContext = model->Is_CinematicNotifyContext();
        model->Set_CinematicNotifyContext(true);
    }

    if (payload.blendDuration > 0.f)
        model->Set_Animation(payload.animName, payload.blendDuration, payload.loop, payload.startFrame);
    else
        model->Set_Animation(payload.animName, payload.loop, payload.startFrame);

    model->Set_CurrentAnimationSpeed(std::max(payload.speed, 0.01f));
    model->Play_Animation(0.f);

    if (auto iter = _bindingStates.find(targetId); iter != _bindingStates.end())
    {
        iter->second.hasActivePlayAnim = true;
        iter->second.activePlayAnim = payload;
        iter->second.activePlayAnimEventFrame = eventFrame;
    }
      
    LOG_INFO("[CinematicSequenceRunner] PlayAnimEvent success: target='{}' anim='{}' blend={:.2f} loop={} startFrame={:.2f} frame={}",
        targetId,
        payload.animName,
        payload.blendDuration,
        payload.loop,
        payload.startFrame,
        Get_CurrentFrame());
    return true;
}

bool CinematicSequenceRunner::Execute_DespawnEvent(const string& targetId, const CinematicDespawnEventData& payload)
{
    UNREFERENCED_PARAMETER(payload);

    if (targetId.empty())
    {
        LOG_WARN("[CinematicSequenceRunner] DespawnEvent skipped: empty targetId. frame={}", Get_CurrentFrame());
        return false;
    }

    const auto iter = _bindingStates.find(targetId);
    if (iter == _bindingStates.end() || iter->second.actor == nullptr)
    {
        LOG_WARN("[CinematicSequenceRunner] DespawnEvent failed: target='{}' is not bound. frame={}",
            targetId,
            Get_CurrentFrame());
        return false;
    }

    if (iter->second.hasCinematicNotifyContextOverride)
    {
        if (Shared<ModelCom> model = Find_TargetModel(iter->second.actor))
            model->Set_CinematicNotifyContext(iter->second.previousCinematicNotifyContext);
    }

    if (iter->second.spawnedBySequence)
        GAME->Remove_GameObject(_sequenceLevelIndex, iter->second.actor);

    _bindingStates.erase(iter);

    LOG_INFO("[CinematicSequenceRunner] DespawnEvent success: target='{}' frame={}",
        targetId,
        Get_CurrentFrame());
    return true;
}

void CinematicSequenceRunner::Apply_ActorTransformTracks(int currentFrame)
{
    for (const auto& track : _sequence.actorTransformTracks)
    {
        if (!track.enabled || track.targetId.empty())
            continue;

        Shared<GameObject> actor = Get_BoundActor(track.targetId);
        if (actor == nullptr)
            continue;

        Shared<TransformCom> transform = actor->Get_Transform();
        if (transform == nullptr)
            continue;

        Vec3 position = Vec3::Zero;
        Vec3 rotation = Vec3::Zero;
        if (!Evaluate_ActorTransformTrack(track, currentFrame, position, rotation))
            continue;

        transform->Set_WorldPosition(position);
        transform->Set_WorldRotationEuler(rotation);
    }
}

bool CinematicSequenceRunner::Evaluate_ActorTransformTrack(
    const CINEMATIC_ACTOR_TRANSFORM_TRACK& track,
    int currentFrame,
    Vec3& outPosition,
    Vec3& outRotation) const
{
    if (!track.enabled || track.keyframes.empty())
        return false;

    const auto& keyframes = track.keyframes;
    if (currentFrame <= keyframes.front().frame)
    {
        outPosition = keyframes.front().position;
        outRotation = keyframes.front().rotation;
        return true;
    }

    if (currentFrame >= keyframes.back().frame)
    {
        outPosition = keyframes.back().position;
        outRotation = keyframes.back().rotation;
        return true;
    }

    for (size_t i = 0; i + 1 < keyframes.size(); ++i)
    {
        const auto& left = keyframes[i];
        const auto& right = keyframes[i + 1];
        if (currentFrame < left.frame || currentFrame > right.frame)
            continue;

        if (left.frame == right.frame)
        {
            outPosition = right.position;
            outRotation = right.rotation;
            return true;
        }

        const float t = static_cast<float>(currentFrame - left.frame) /
            static_cast<float>(right.frame - left.frame);
        outPosition = Vec3::Lerp(left.position, right.position, t);
        outRotation = Vec3::Lerp(left.rotation, right.rotation, t);
        return true;
    }

    return false;
}

void CinematicSequenceRunner::Apply_BoundActorAnimationPreview(int currentFrame)
{
    const float sequenceFps = static_cast<float>(std::max(_sequence.fps, 1));

    for (const auto& [targetId, state] : _bindingStates)
    {
        UNREFERENCED_PARAMETER(targetId);

        if (!state.hasActivePlayAnim || state.actor == nullptr)
            continue;

        Shared<ModelCom> model = Find_TargetModel(state.actor);
        if (model == nullptr)
            continue;

        const float elapsedSec =
            static_cast<float>(std::max(currentFrame - state.activePlayAnimEventFrame, 0)) / sequenceFps;

        if (state.activePlayAnim.blendDuration > 0.f)
            model->Set_Animation(
                state.activePlayAnim.animName,
                state.activePlayAnim.blendDuration,
                state.activePlayAnim.loop,
                state.activePlayAnim.startFrame);
        else
            model->Set_Animation(
                state.activePlayAnim.animName,
                state.activePlayAnim.loop,
                state.activePlayAnim.startFrame);

        model->Set_CurrentAnimationSpeed(std::max(state.activePlayAnim.speed, 0.01f));
        model->Set_AnimationPaused(false);
        model->Play_Animation(elapsedSec);
        model->Set_AnimationPaused(true);

        if (PartObject* animPart = Find_TargetAnimPart(state.actor))
            animPart->Update(0.f);
    }
}

void CinematicSequenceRunner::Set_BoundActorAnimationPaused(bool paused)
{
    for (const auto& [targetId, state] : _bindingStates)
    {
        UNREFERENCED_PARAMETER(targetId);

        if (!state.hasActivePlayAnim || state.actor == nullptr)
            continue;

        if (Shared<ModelCom> model = Find_TargetModel(state.actor))
            model->Set_AnimationPaused(paused);
    }
}

void CinematicSequenceRunner::Update_BoundActorAnimations(float timeDelta)
{
    if (timeDelta <= 0.f)
        return;

    for (const auto& [targetId, state] : _bindingStates)
    {
        UNREFERENCED_PARAMETER(targetId);

        if (state.actor == nullptr)
            continue;

        Shared<ModelCom> model = Find_TargetModel(state.actor);
        if (model == nullptr)
            continue;

        model->Play_Animation(timeDelta);

        if (PartObject* animPart = Find_TargetAnimPart(state.actor))
            animPart->Update(0.f);
    }
}

void CinematicSequenceRunner::Cleanup_Context(bool forceRemoveAllSpawned)
{
    for (const auto& [targetId, state] : _bindingStates)
    {
        UNREFERENCED_PARAMETER(targetId);

        if (state.actor == nullptr)
            continue;

        if (state.hasCinematicNotifyContextOverride)
        {
            if (Shared<ModelCom> model = Find_TargetModel(state.actor))
                model->Set_CinematicNotifyContext(state.previousCinematicNotifyContext);
        }

        if (!state.spawnedBySequence)
            continue;

        if (!forceRemoveAllSpawned && state.cleanupPolicy == CinematicCleanupPolicy::Keep)
        {
            if (const auto worldMonster = dynamic_pointer_cast<Monster_WorldBase>(state.actor))
                worldMonster->Set_CinematicDriven(false);
            continue;
        }

        GAME->Remove_GameObject(_sequenceLevelIndex, state.actor);
    }

    _bindingStates.clear();
    _nextEventIndex = 0;
    _suspendActorTransformTrackPreview = false;

    if (forceRemoveAllSpawned)
        Clear_CompletedCallbacks();
}

Unique<CinematicSequenceRunner> CinematicSequenceRunner::Create()
{
    return make_unique<CinematicSequenceRunner>();
}

void CinematicSequenceRunner::Free()
{
    __super::Free();
}

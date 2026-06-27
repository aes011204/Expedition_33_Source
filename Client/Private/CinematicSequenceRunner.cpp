#include "CinematicSequenceRunner.h"

#include "Cinematic_Player.h"
#include "Cinematic_SequenceIO.h"
#include "Cinematic_Utils.h"
#include "CinematicCamera.h"
#include "ClientInstance.h"
#include "ContainerObject.h"
#include "GameInstance.h"
#include "Helper_String.h"
#include "ModelCom.h"
#include "Monster_WorldBase.h"
#include "PartObject.h"
#include "SL_Dualliste.h"
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

Vec3 CinematicSequenceRunner::Build_LookAtRotation(
    const Vec3& fromWorldPos,
    const Vec3& toWorldPos,
    const Vec3& fallbackRotation)
{
    const Vec3 direction = toWorldPos - fromWorldPos;
    const float horizontalLength = sqrtf(direction.x * direction.x + direction.z * direction.z);
    if (direction.LengthSquared() <= 0.0001f || horizontalLength <= 0.0001f)
        return fallbackRotation;

    Vec3 rotation = fallbackRotation;
    rotation.y = XMConvertToDegrees(atan2f(direction.x, direction.z));
    rotation.x = -XMConvertToDegrees(atan2f(direction.y, horizontalLength));
    rotation.z = 0.f;
    return rotation;
}

bool CinematicSequenceRunner::Resolve_CameraTrackSelection(
    int trackIndex,
    CINEMATIC_TRACK*& outWorldTrack,
    CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK*& outActorRelativeTrack,
    CameraTrackKind& outKind)
{
    outWorldTrack = nullptr;
    outActorRelativeTrack = nullptr;
    outKind = CameraTrackKind::None;

    const int worldTrackCount = static_cast<int>(_sequence.cinematicTracks.size());
    if (trackIndex >= 0 && trackIndex < worldTrackCount)
    {
        outWorldTrack = &_sequence.cinematicTracks[trackIndex];
        outKind = CameraTrackKind::World;
        return true;
    }

    const int actorRelativeTrackIndex = trackIndex - worldTrackCount;
    if (actorRelativeTrackIndex >= 0 &&
        actorRelativeTrackIndex < static_cast<int>(_sequence.actorRelativeCameraTracks.size()))
    {
        outActorRelativeTrack = &_sequence.actorRelativeCameraTracks[actorRelativeTrackIndex];
        outKind = CameraTrackKind::ActorRelative;
        return true;
    }

    return false;
}

bool CinematicSequenceRunner::Evaluate_ActorRelativeCameraTrack(
    const CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK& track,
    int currentFrame,
    CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME& outKeyframe) const
{
    if (!track.enabled || track.keyframes.empty())
        return false;

    const auto& keyframes = track.keyframes;
    if (currentFrame <= keyframes.front().frame)
    {
        outKeyframe = keyframes.front();
        return true;
    }

    if (currentFrame >= keyframes.back().frame)
    {
        outKeyframe = keyframes.back();
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
            outKeyframe = right;
            return true;
        }

        const float t = static_cast<float>(currentFrame - left.frame) /
            static_cast<float>(right.frame - left.frame);
        outKeyframe.frame = currentFrame;
        outKeyframe.distance = std::lerp(left.distance, right.distance, t);
        outKeyframe.height = std::lerp(left.height, right.height, t);
        outKeyframe.sideOffset = std::lerp(left.sideOffset, right.sideOffset, t);
        outKeyframe.yawOffset = std::lerp(left.yawOffset, right.yawOffset, t);
        outKeyframe.pitch = std::lerp(left.pitch, right.pitch, t);
        outKeyframe.lookHeight = std::lerp(left.lookHeight, right.lookHeight, t);
        outKeyframe.fovY = std::lerp(left.fovY, right.fovY, t);
        outKeyframe.nearPlane = std::lerp(left.nearPlane, right.nearPlane, t);
        outKeyframe.farPlane = std::lerp(left.farPlane, right.farPlane, t);
        outKeyframe.interpolation = right.interpolation;
        return true;
    }

    return false;
}

bool CinematicSequenceRunner::Build_ActorRelativeCameraPose(
    const CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK& track,
    int currentFrame,
    CAMERAPOSE& outPose) const
{
    if (track.targetId.empty())
        return false;

    Shared<GameObject> actor = Get_BoundActor(track.targetId);
    if (actor == nullptr)
        return false;

    Shared<TransformCom> transform = actor->Get_Transform();
    if (transform == nullptr)
        return false;

    CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME sample{};
    if (!Evaluate_ActorRelativeCameraTrack(track, currentFrame, sample))
        return false;

    const Vec3 actorPos = transform->Get_WorldPosition();
    const float actorYaw = transform->Get_WorldRotationEuler().y;
    const float finalYaw = actorYaw + sample.yawOffset;
    const float yawRad = XMConvertToRadians(finalYaw);
    const float pitchRad = XMConvertToRadians(sample.pitch);

    Vec3 backDir{};
    backDir.x = -sinf(yawRad);
    backDir.y = 0.f;
    backDir.z = -cosf(yawRad);
    backDir.Normalize();

    Vec3 rightDir{};
    rightDir.x = cosf(yawRad);
    rightDir.y = 0.f;
    rightDir.z = -sinf(yawRad);
    rightDir.Normalize();

    const Vec3 baseOffset = backDir * sample.distance;
    const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, pitchRad);
    const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);

    const Vec3 cameraPos = actorPos + orbitOffset + Vec3::Up * sample.height + rightDir * sample.sideOffset;
    const Vec3 lookTarget = actorPos + Vec3::Up * sample.lookHeight;

    outPose.position = cameraPos;
    outPose.rotation = Build_LookAtRotation(cameraPos, lookTarget, outPose.rotation);
    outPose.fovY = sample.fovY;
    outPose.nearPlane = sample.nearPlane;
    outPose.farPlane = sample.farPlane;
    return true;
}

void CinematicSequenceRunner::Apply_ActiveCameraTrackPose(int currentFrame)
{
    if (!_applyCameraTrack || _activeCinematicCamera == nullptr)
        return;

    if (_cameraTrackKind != CameraTrackKind::ActorRelative)
        return;

    const int actorRelativeTrackIndex = _trackIndex - static_cast<int>(_sequence.cinematicTracks.size());
    if (actorRelativeTrackIndex < 0 ||
        actorRelativeTrackIndex >= static_cast<int>(_sequence.actorRelativeCameraTracks.size()))
    {
        return;
    }

    CAMERAPOSE camPose{};
    if (!Build_ActorRelativeCameraPose(_sequence.actorRelativeCameraTracks[actorRelativeTrackIndex], currentFrame, camPose))
        return;

    const Client::CameraModifierSample modifierSample = Evaluate_CinematicModifierSample(_sequence, currentFrame);
    const float sequenceTime = Evaluate_CinematicSequenceTime(_sequence, currentFrame);

    _activeCinematicCamera->Apply_CinematicPose(camPose);
    if (_activeCinematicCamera->Get_Transform() == nullptr)
        return;

    const Vec3 shakeOffset = Sample_CameraModifierShakeOffset(
        modifierSample,
        sequenceTime,
        _activeCinematicCamera->Get_Transform()->Get_WorldRight(),
        _activeCinematicCamera->Get_Transform()->Get_WorldUp());

    camPose.position += shakeOffset;
    _activeCinematicCamera->Apply_CinematicPose(camPose);
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

    CINEMATIC_TRACK* cinematicTrack = nullptr;
    CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* actorRelativeTrack = nullptr;
    CameraTrackKind trackKind = CameraTrackKind::None;
    if (!Resolve_CameraTrackSelection(trackIndex, cinematicTrack, actorRelativeTrack, trackKind))
        return false;

    Shared<CinematicCamera> cinematicCamera = nullptr;
    if (applyCameraTrack)
    {
        cinematicCamera = CLIENT->Request_Cinematic();
        if (cinematicCamera == nullptr)
            return false;
    }

    _activeCinematicCamera = cinematicCamera;
    _bindingStates.clear();
    _trackIndex = trackIndex;
    _nextEventIndex = 0;
    _cameraTrackKind = trackKind;
    _sequenceLevelIndex = GAME->Current_LevelIndex();
    _suspendActorTransformTrackPreview = false;
    _applyCameraTrack = applyCameraTrack;
    _applyEventTrack = applyEventTrack;
    _applyActorTransformTrack = applyActorTransformTrack;

    _player->Ready_CinematicPlayer(&_sequence, cinematicTrack, cinematicCamera, true, applyCameraTrack);
    Apply_ActiveCameraTrackPose(_sequence.startFrame);
    if (_applyEventTrack)
        Execute_EventsUpToFrame(_sequence.startFrame, static_cast<float>(_sequence.startFrame));
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_ActorTransformTracks(_sequence.startFrame);
    if (applyCameraTrack)
        _player->Play();

    GAME->Set_LetterboxEnabled(true);

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

    _sequence = sequence;

    CINEMATIC_TRACK* cinematicTrack = nullptr;
    CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* actorRelativeTrack = nullptr;
    CameraTrackKind trackKind = CameraTrackKind::None;
    if (!Resolve_CameraTrackSelection(trackIndex, cinematicTrack, actorRelativeTrack, trackKind))
        return false;

    Shared<CinematicCamera> cinematicCamera = nullptr;
    if (applyCameraTrack)
    {
        cinematicCamera = CLIENT->Request_Cinematic();
        if (cinematicCamera == nullptr)
            return false;
    }

    _activeCinematicCamera = cinematicCamera;
    _bindingStates.clear();
    _trackIndex = trackIndex;
    _nextEventIndex = 0;
    _cameraTrackKind = trackKind;
    _sequenceLevelIndex = GAME->Current_LevelIndex();
    _suspendActorTransformTrackPreview = false;
    _applyCameraTrack = applyCameraTrack;
    _applyEventTrack = applyEventTrack;
    _applyActorTransformTrack = applyActorTransformTrack;

   _player->Ready_CinematicPlayer(&_sequence, cinematicTrack, cinematicCamera, true, applyCameraTrack);
    Apply_ActiveCameraTrackPose(_sequence.startFrame);
    if (_applyEventTrack)
        Execute_EventsUpToFrame(_sequence.startFrame, static_cast<float>(_sequence.startFrame));
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_ActorTransformTracks(_sequence.startFrame);
    if (applyCameraTrack)
        _player->Play();

    GAME->Set_LetterboxEnabled(true);

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

    GAME->Set_LetterboxEnabled(false);
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
    Apply_ActiveCameraTrackPose(targetFrame);
    if (_applyEventTrack)
        Execute_EventsUpToFrame(targetFrame, static_cast<float>(targetFrame));
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
    Apply_ActiveCameraTrackPose(currentFrame);
    if (_applyEventTrack)
        Execute_EventsUpToFrame(currentFrame, static_cast<float>(currentFrame));
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
    Apply_ActiveCameraTrackPose(targetFrame);

    if (applyEventTrack)
        Execute_EventsUpToFrame(targetFrame, static_cast<float>(targetFrame));

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

    _sequence = sequence;

    CINEMATIC_TRACK* cinematicTrack = nullptr;
    CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* actorRelativeTrack = nullptr;
    CameraTrackKind trackKind = CameraTrackKind::None;
    if (!Resolve_CameraTrackSelection(_trackIndex, cinematicTrack, actorRelativeTrack, trackKind))
        return false;

    _cameraTrackKind = trackKind;
    _player->Rebind_Sequence(&_sequence, cinematicTrack);
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
    Apply_ActiveCameraTrackPose(_player->Get_CurrentFrame());

    const int currentFrame = _player->Get_CurrentFrame();
    const float currentFrameExact = _player->Get_CurrentFrameExact();
    const bool isPlayingAfterUpdate = _player->Is_CinematicPlaying();

    if (_applyEventTrack)
        Execute_EventsUpToFrame(currentFrame, currentFrameExact);
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_ActorTransformTracks(currentFrame);

    if (tickBoundAnimationsManually && isPlayingAfterUpdate && !_player->Is_CinematicPaused())
        Update_BoundActorAnimations(timeDelta);

    if (wasPlaying && !_player->Is_CinematicPlaying())
    {
        CLIENT->Restore_PreviousCamera(); // 완료 콜백이 전투 카메라를 요청할 수 있으므로, 시네마틱 카메라 복원은 먼저 끝낸다.
        _onNaturalCompleted.Broadcast();
        Clear_CompletedCallbacks(); // 완료 콜백

        Cleanup_Context();
        CLIENT->Restore_PreviousCamera();
        GAME->Set_LetterboxEnabled(false);
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

void CinematicSequenceRunner::Execute_EventsUpToFrame(int currentFrame, float currentFrameExact)
{
    while (_nextEventIndex < static_cast<int>(_sequence.eventTrack.size()))
    {
        const CINEMATIC_EVENT& event = _sequence.eventTrack[_nextEventIndex];
        if (event.frame > currentFrame)
            break;

        Execute_Event(event, currentFrameExact);
        ++_nextEventIndex;
    }
}

void CinematicSequenceRunner::Execute_Event(const CINEMATIC_EVENT& event, float currentFrameExact)
{
    if (const auto* spawn = get_if<CinematicSpawnEventData>(&event.payload))
    {
        Execute_SpawnEvent(event.targetId, *spawn);
    }
    else if (const auto* playAnim = get_if<CinematicPlayAnimEventData>(&event.payload))
    {
        Execute_PlayAnimEvent(event.frame, currentFrameExact, event.targetId, *playAnim);
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

    if (Shared<GameObject> existingActor = Find_ReusableActorByPrefabName(payload.prefabName))
    {
        Bind_ActorToTarget(targetId, existingActor, false, payload.cleanupPolicy);

        LOG_INFO(
            "[CinematicSequenceRunner] SpawnEvent reused existing actor: target='{}' prefab='{}' frame={}",
            targetId,
            payload.prefabName,
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

    Bind_ActorToTarget(targetId, actor, true, payload.cleanupPolicy);

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
    float currentFrameExact,
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
        const float sequenceFps = static_cast<float>(std::max(_sequence.fps, 1));
        const float frameOvershoot = std::max(currentFrameExact - static_cast<float>(eventFrame), 0.f);

        iter->second.hasActivePlayAnim = true;
        iter->second.activePlayAnim = payload;
        iter->second.activePlayAnimEventFrame = eventFrame;
        iter->second.pendingPlayAnimInitialAdvanceSec = frameOvershoot > 0.f ? frameOvershoot / sequenceFps : -1.f;
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

    Apply_CinematicDrivenState(iter->second.actor, false);

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

    for (auto& [targetId, state] : _bindingStates)
    {
        UNREFERENCED_PARAMETER(targetId);

        if (state.actor == nullptr)
            continue;

        Shared<ModelCom> model = Find_TargetModel(state.actor);
        if (model == nullptr)
            continue;

        float animationDelta = timeDelta;
        if (state.pendingPlayAnimInitialAdvanceSec >= 0.f)
        {
            animationDelta = state.pendingPlayAnimInitialAdvanceSec;
            state.pendingPlayAnimInitialAdvanceSec = -1.f;
        }

        model->Play_Animation(animationDelta);

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

        Apply_CinematicDrivenState(state.actor, false);

        if (!state.spawnedBySequence)
            continue;

        if (!forceRemoveAllSpawned && state.cleanupPolicy == CinematicCleanupPolicy::Keep)
        {
            continue;
        }

        GAME->Remove_GameObject(_sequenceLevelIndex, state.actor);
    }

    _bindingStates.clear();
    _activeCinematicCamera = nullptr;
    _nextEventIndex = 0;
    _cameraTrackKind = CameraTrackKind::None;
    _suspendActorTransformTrackPreview = false;

    if (forceRemoveAllSpawned)
        Clear_CompletedCallbacks();
}

Shared<GameObject> CinematicSequenceRunner::Find_ReusableActorByPrefabName(const string& prefabName) const
{
    if (prefabName.empty())
        return nullptr;

    Shared<GameObject> matchedActor = nullptr;
    GAME->Visit_LevelObjects(
        _sequenceLevelIndex,
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            UNREFERENCED_PARAMETER(layerTag);

            if (matchedActor != nullptr || gameObject == nullptr || gameObject->Is_MarkDestroy())
                return;

            if (gameObject->Get_SourcePrefabName() != prefabName)
                return;

            if (Is_ActorAlreadyBound(gameObject))
                return;

            matchedActor = gameObject;
        });

    return matchedActor;
}

bool CinematicSequenceRunner::Is_ActorAlreadyBound(const Shared<GameObject>& actor) const
{
    if (actor == nullptr)
        return false;

    for (const auto& [targetId, state] : _bindingStates)
    {
        UNREFERENCED_PARAMETER(targetId);

        if (state.actor == actor)
            return true;
    }

    return false;
}

void CinematicSequenceRunner::Apply_CinematicDrivenState(const Shared<GameObject>& actor, bool cinematicDriven) const
{
    if (const auto worldMonster = dynamic_pointer_cast<Monster_WorldBase>(actor))
    {
        worldMonster->Set_CinematicDriven(cinematicDriven);
        return;
    }

    if (const auto dualliste = dynamic_pointer_cast<SL_Dualliste>(actor))
        dualliste->Set_CinematicDriven(cinematicDriven);
}

void CinematicSequenceRunner::Bind_ActorToTarget(
    const string& targetId,
    const Shared<GameObject>& actor,
    bool spawnedBySequence,
    CinematicCleanupPolicy cleanupPolicy)
{
    BindingRuntimeState state{};
    state.actor = actor;
    state.spawnedBySequence = spawnedBySequence;
    state.cleanupPolicy = cleanupPolicy;

    if (Shared<ModelCom> model = Find_TargetModel(actor))
    {
        state.hasCinematicNotifyContextOverride = true;
        state.previousCinematicNotifyContext = model->Is_CinematicNotifyContext();
        model->Set_CinematicNotifyContext(true);
    }

    Apply_CinematicDrivenState(actor, true);
    _bindingStates[targetId] = state;
}

Unique<CinematicSequenceRunner> CinematicSequenceRunner::Create()
{
    return make_unique<CinematicSequenceRunner>();
}

void CinematicSequenceRunner::Free()
{
    __super::Free();
}

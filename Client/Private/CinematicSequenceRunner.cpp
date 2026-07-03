#include "CinematicSequenceRunner.h"
#include "Cinematic_Player.h"
#include "Cinematic_SequenceIO.h"
#include "Cinematic_Utils.h"
#include "CinematicCamera.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "ContainerObject.h"
#include "GameInstance.h"
#include "GameplayUI.h"
#include "Helper_String.h"
#include "ModelCom.h"
#include "Monster_WorldBase.h"
#include "Player_WorldBase.h"
#include "PartObject.h"
#include "SL_Dualliste.h"
#include "SL_Maelle.h"
#include "SL_TargetCom.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"
#include <algorithm>
#include <cmath>
#include <thread>

namespace
{
float WrapDegrees180(float degrees)
{
    float wrapped = fmodf(degrees + 180.f, 360.f);
    if (wrapped < 0.f)
        wrapped += 360.f;

    return wrapped - 180.f;
}

float LerpDegreesShortest(float fromDegrees, float toDegrees, float t)
{
    const float delta = WrapDegrees180(toDegrees - fromDegrees);
    return WrapDegrees180(fromDegrees + delta * t);
}

Vec3 LerpRotationDegreesShortest(const Vec3& fromDegrees, const Vec3& toDegrees, float t)
{
    return Vec3(
        LerpDegreesShortest(fromDegrees.x, toDegrees.x, t),
        LerpDegreesShortest(fromDegrees.y, toDegrees.y, t),
        LerpDegreesShortest(fromDegrees.z, toDegrees.z, t));
}

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

void Visit_Colliders(GameObject* gameObject, const function<void(const Shared<ColliderCom>&)>& visitor)
{
    if (gameObject == nullptr)
        return;

    gameObject->Visit_Components(
        [&](const wstring& componentTag, const Shared<Component>& component)
        {
            UNREFERENCED_PARAMETER(componentTag);

            if (Shared<ColliderCom> collider = dynamic_pointer_cast<ColliderCom>(component))
                visitor(collider);
        });
}

void Visit_ActorColliders(const Shared<GameObject>& actor, const function<void(const Shared<ColliderCom>&)>& visitor)
{
    if (actor == nullptr)
        return;

    Visit_Colliders(actor.get(), visitor);

    if (const auto container = dynamic_pointer_cast<ContainerObject>(actor))
    {
        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);
            Visit_Colliders(partObject.get(), visitor);
        }
    }
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
    CameraTrackKind& outKind) const
{
    outWorldTrack = nullptr;
    outActorRelativeTrack = nullptr;
    outKind = CameraTrackKind::None;

    const int worldTrackCount = static_cast<int>(_sequence.cinematicTracks.size());
    if (trackIndex >= 0 && trackIndex < worldTrackCount)
    {
        outWorldTrack = const_cast<CINEMATIC_TRACK*>(&_sequence.cinematicTracks[trackIndex]);
        outKind = CameraTrackKind::World;
        return true;
    }

    const int actorRelativeTrackIndex = trackIndex - worldTrackCount;
    if (actorRelativeTrackIndex >= 0 &&
        actorRelativeTrackIndex < static_cast<int>(_sequence.actorRelativeCameraTracks.size()))
    {
        outActorRelativeTrack = const_cast<CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK*>(
            &_sequence.actorRelativeCameraTracks[actorRelativeTrackIndex]);
        outKind = CameraTrackKind::ActorRelative;
        return true;
    }

    return false;
}

bool CinematicSequenceRunner::Evaluate_ActorRelativeCameraTrack(
    const CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK& track,
    float currentFrameExact,
    CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME& outKeyframe) const
{
    if (!track.enabled || track.keyframes.empty())
        return false;

    const auto& keyframes = track.keyframes;
    if (currentFrameExact <= static_cast<float>(keyframes.front().frame))
    {
        outKeyframe = keyframes.front();
        return true;
    }

    if (currentFrameExact >= static_cast<float>(keyframes.back().frame))
    {
        outKeyframe = keyframes.back();
        return true;
    }

    for (size_t i = 0; i + 1 < keyframes.size(); ++i)
    {
        const auto& left = keyframes[i];
        const auto& right = keyframes[i + 1];
        if (currentFrameExact < static_cast<float>(left.frame) ||
            currentFrameExact > static_cast<float>(right.frame))
            continue;

        if (left.frame == right.frame)
        {
            outKeyframe = right;
            return true;
        }

        const float t = (currentFrameExact - static_cast<float>(left.frame)) /
            static_cast<float>(right.frame - left.frame);
        outKeyframe.frame = static_cast<int>(std::floor(currentFrameExact));
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
    float currentFrameExact,
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
    if (!Evaluate_ActorRelativeCameraTrack(track, currentFrameExact, sample))
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

bool CinematicSequenceRunner::Resolve_ActiveCameraTrackForFrame(
    float currentFrameExact,
    CINEMATIC_TRACK*& outWorldTrack,
    CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK*& outActorRelativeTrack,
    CameraTrackKind& outKind,
    int& outTrackIndex) const
{
    outWorldTrack = nullptr;
    outActorRelativeTrack = nullptr;
    outKind = CameraTrackKind::None;
    outTrackIndex = _trackIndex;

    if (Uses_CameraShots())
    {
        const int currentFrame = std::clamp(
            static_cast<int>(std::floor(currentFrameExact)),
            _sequence.startFrame,
            _sequence.endFrame);

        for (const auto& shot : _sequence.cameraShots)
        {
            if (!shot.enabled)
                continue;

            if (currentFrame < shot.startFrame || currentFrame > shot.endFrame)
                continue;

            CINEMATIC_TRACK* shotWorldTrack = nullptr;
            CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* shotActorRelativeTrack = nullptr;
            CameraTrackKind shotTrackKind = CameraTrackKind::None;
            if (Resolve_CameraTrackSelection(
                shot.trackIndex,
                shotWorldTrack,
                shotActorRelativeTrack,
                shotTrackKind))
            {
                outWorldTrack = shotWorldTrack;
                outActorRelativeTrack = shotActorRelativeTrack;
                outKind = shotTrackKind;
                outTrackIndex = shot.trackIndex;
                return true;
            }
        }
    }

    return Resolve_CameraTrackSelection(outTrackIndex, outWorldTrack, outActorRelativeTrack, outKind);
}

bool CinematicSequenceRunner::Uses_CameraShots() const
{
    return !_sequence.cameraShots.empty();
}

void CinematicSequenceRunner::Apply_ActiveCameraTrackPose(float currentFrameExact)
{
    if (!_applyCameraTrack || _activeCinematicCamera == nullptr)
        return;

    CAMERAPOSE camPose{};
    CINEMATIC_TRACK* worldTrack = nullptr;
    CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* actorRelativeTrack = nullptr;
    CameraTrackKind activeTrackKind = CameraTrackKind::None;
    int activeTrackIndex = _trackIndex;
    if (!Resolve_ActiveCameraTrackForFrame(
        currentFrameExact,
        worldTrack,
        actorRelativeTrack,
        activeTrackKind,
        activeTrackIndex))
    {
        return;
    }

    if (activeTrackKind == CameraTrackKind::World)
    {
        CINEMATIC_KEYFRAME* leftKey = nullptr;
        CINEMATIC_KEYFRAME* rightKey = nullptr;
        camPose = Evaluate_CinematicPose(worldTrack, currentFrameExact, leftKey, rightKey);
    }
    else if (activeTrackKind == CameraTrackKind::ActorRelative)
    {
        if (!Build_ActorRelativeCameraPose(*actorRelativeTrack, currentFrameExact, camPose))
            return;
    }
    else
    {
        return;
    }

    const Client::CameraModifierSample modifierSample = Evaluate_CinematicModifierSample(_sequence, currentFrameExact);
    const float sequenceTime = Evaluate_CinematicSequenceTime(_sequence, currentFrameExact);

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
        Set_GameplayUIHiddenForCinematic(false);
        GAME->Set_LetterboxEnabled(false);
        Clear_CompletedCallbacks();
        Cleanup_Context();
    }

    if (const auto cachedSequence = _prewarmedSequences.find(filePath); cachedSequence != _prewarmedSequences.end())
    {
        _sequence = cachedSequence->second;
    }
    else
    {
        const string stringPath = String::ToString(filePath);
        if (!Cinematic_SequenceIO::LoadSequenceFromFile(stringPath, _sequence))
        {
            _activeSequencePath.clear();
            return false;
        }
    }

    CINEMATIC_TRACK* cinematicTrack = nullptr;
    CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* actorRelativeTrack = nullptr;
    CameraTrackKind trackKind = CameraTrackKind::None;
    if (!Resolve_CameraTrackSelection(trackIndex, cinematicTrack, actorRelativeTrack, trackKind))
    {
        _activeSequencePath.clear();
        return false;
    }

    Shared<CinematicCamera> cinematicCamera = nullptr;
    if (applyCameraTrack)
    {
        cinematicCamera = CLIENT->Request_Cinematic();
        if (cinematicCamera == nullptr)
        {
            _activeSequencePath.clear();
            return false;
        }
    }

    _activeCinematicCamera = cinematicCamera;
    _bindingStates.clear();
    _activeSequencePath = filePath;
    _trackIndex = trackIndex;
    _nextEventIndex = 0;
    _cameraTrackKind = trackKind;
    _sequenceLevelIndex = GAME->Current_LevelIndex();
    _suspendActorTransformTrackPreview = false;
    _applyCameraTrack = applyCameraTrack;
    _applyEventTrack = applyEventTrack;
    _applyActorTransformTrack = applyActorTransformTrack;

    Set_GameplayUIHiddenForCinematic(true);
    const bool playerAppliesCameraTrack = applyCameraTrack && !Uses_CameraShots() && trackKind == CameraTrackKind::World;
    _player->Ready_CinematicPlayer(&_sequence, cinematicTrack, cinematicCamera, true, playerAppliesCameraTrack);
    Apply_ActiveCameraTrackPose(static_cast<float>(_sequence.startFrame));
    if (_applyEventTrack)
        Execute_EventsUpToFrame(_sequence.startFrame, static_cast<float>(_sequence.startFrame));
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_AuthoredActorTracks(static_cast<float>(_sequence.startFrame));
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
        Set_GameplayUIHiddenForCinematic(false);
        GAME->Set_LetterboxEnabled(false);
        Cleanup_Context();
    }

    _sequence = sequence;
    _activeSequencePath.clear();

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

    Set_GameplayUIHiddenForCinematic(true);
    const bool playerAppliesCameraTrack = applyCameraTrack && !Uses_CameraShots() && trackKind == CameraTrackKind::World;
    _player->Ready_CinematicPlayer(&_sequence, cinematicTrack, cinematicCamera, true, playerAppliesCameraTrack);
    Apply_ActiveCameraTrackPose(static_cast<float>(_sequence.startFrame));
    if (_applyEventTrack)
        Execute_EventsUpToFrame(_sequence.startFrame, static_cast<float>(_sequence.startFrame));
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_AuthoredActorTracks(static_cast<float>(_sequence.startFrame));
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

    Set_GameplayUIHiddenForCinematic(false);
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
    if (_hasWorldTimeScaleOverride)
    {
        GAME->Set_WorldTimeScale(_restoreWorldTimeScale);
        _hasWorldTimeScaleOverride = false;
        _restoreWorldTimeScale = 1.f;
    }
    Set_GameplayUIHiddenForCinematic(false);
    GAME->Set_LetterboxEnabled(false);
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
    const bool playerAppliesCameraTrack = applyCameraTrack && !Uses_CameraShots() && _cameraTrackKind == CameraTrackKind::World;
    _player->Set_ApplyCameraTrack(playerAppliesCameraTrack);
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
    Apply_ActiveCameraTrackPose(static_cast<float>(targetFrame));
    if (_applyEventTrack)
        Execute_EventsUpToFrame(targetFrame, static_cast<float>(targetFrame));
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_AuthoredActorTracks(static_cast<float>(targetFrame));
    if (_applyEventTrack)
        Apply_BoundActorAnimationPreview(static_cast<float>(targetFrame));
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
    Apply_ActiveCameraTrackPose(static_cast<float>(currentFrame));
    if (_applyEventTrack)
        Execute_EventsUpToFrame(currentFrame, static_cast<float>(currentFrame));
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_AuthoredActorTracks(static_cast<float>(currentFrame));
    if (_applyEventTrack)
        Apply_BoundActorAnimationPreview(static_cast<float>(currentFrame));
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
    Apply_ActiveCameraTrackPose(static_cast<float>(targetFrame));

    if (applyEventTrack)
        Execute_EventsUpToFrame(targetFrame, static_cast<float>(targetFrame));

    if (applyEventTrack && applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_AuthoredActorTracks(static_cast<float>(targetFrame));

    if (applyEventTrack)
        Apply_BoundActorAnimationPreview(static_cast<float>(targetFrame));
}

void CinematicSequenceRunner::Preview_CurrentFrameExact(
    float frameExact,
    bool applyCameraTrack,
    bool applyEventTrack,
    bool applyActorTransformTrack,
    bool forceRebuildContext)
{
    if (_player == nullptr || !_player->Is_CinematicPlaying())
        return;

    const float targetFrameExact = std::clamp(
        frameExact,
        static_cast<float>(_sequence.startFrame),
        static_cast<float>(_sequence.endFrame));
    const int targetFrame = std::clamp(
        static_cast<int>(std::floor(targetFrameExact)),
        _sequence.startFrame,
        _sequence.endFrame);
    const float previousFrameExact = _player->Get_CurrentFrameExact();
    const bool suspendActorTransformTrackPreview = _suspendActorTransformTrackPreview;
    const bool requiresContextRebuild =
        forceRebuildContext ||
        targetFrameExact + 0.0001f < previousFrameExact ||
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

    _player->Set_CurrentFrameExact(targetFrameExact, applyCameraTrack);
    Apply_ActiveCameraTrackPose(targetFrameExact);

    if (applyEventTrack)
        Execute_EventsUpToFrame(targetFrame, targetFrameExact);

    if (applyEventTrack && applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_AuthoredActorTracks(targetFrameExact);

    if (applyEventTrack)
        Apply_BoundActorAnimationPreview(targetFrameExact);
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
    const bool playerAppliesCameraTrack = _applyCameraTrack && !Uses_CameraShots() && trackKind == CameraTrackKind::World;
    _player->Set_ApplyCameraTrack(playerAppliesCameraTrack);
    return true;
}

void CinematicSequenceRunner::Set_ActorTransformTrackPreviewSuspended(bool suspended)
{
    _suspendActorTransformTrackPreview = suspended;

    if (!_suspendActorTransformTrackPreview && _player != nullptr && _player->Is_CinematicPlaying())
        Apply_AuthoredActorTracks(_player->Get_CurrentFrameExact());
}

void CinematicSequenceRunner::Update(float timeDelta, bool tickBoundAnimationsManually)
{
    if (_player == nullptr)
        return;

    const bool wasPlaying = _player->Is_CinematicPlaying();
    if (!wasPlaying)
        return;

    _player->Update(timeDelta);
    Apply_ActiveCameraTrackPose(_player->Get_CurrentFrameExact());

    const int currentFrame = _player->Get_CurrentFrame();
    const float currentFrameExact = _player->Get_CurrentFrameExact();
    const bool isPlayingAfterUpdate = _player->Is_CinematicPlaying();

    if (_applyEventTrack)
        Execute_EventsUpToFrame(currentFrame, currentFrameExact);
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_AuthoredActorTracks(currentFrameExact);

    if (tickBoundAnimationsManually && isPlayingAfterUpdate && !_player->Is_CinematicPaused())
        Update_BoundActorAnimations(timeDelta);

    if (wasPlaying && !_player->Is_CinematicPlaying())
    {
        CLIENT->Restore_PreviousCamera();   // 완료 콜백이 전투 카메라를 요청할 수 있으므로, 시네마틱 카메라 복원은 먼저 끝낸다.
        Set_GameplayUIHiddenForCinematic(false);
        _onNaturalCompleted.Broadcast();
        Clear_CompletedCallbacks();         // 완료 콜백

        Cleanup_Context();

        // 인수 : 이거 필요한지? 이 친구 때문에 시네마틱 끝나고나서 비숍이랑 전투 시작할 때, BattleCamera로 바인딩이 되지 않고있음
        //CLIENT->Restore_PreviousCamera(); 
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

float CinematicSequenceRunner::Get_CurrentFrameExact() const
{
    if (_player == nullptr)
        return static_cast<float>(_sequence.startFrame);

    return _player->Get_CurrentFrameExact();
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

void CinematicSequenceRunner::Queue_PrewarmSequence(const wstring& filePath, uint32 targetLevelIndex)
{
    if (filePath.empty())
        return;

    if (_prewarmedSequences.find(filePath) != _prewarmedSequences.end())
        return;

    if (!_queuedPrewarmSequencePaths.insert(filePath).second)
        return;

    _activePrewarmSequenceJobs.fetch_add(1);

    const bool enqueued = GAME->Enqueue_ThreadJob(
        [this, filePath, targetLevelIndex]()
        {
            PrewarmSequenceResult result{};
            result.filePath = filePath;
            result.targetLevelIndex = targetLevelIndex;

            const string stringPath = String::ToString(filePath);
            result.succeeded = Cinematic_SequenceIO::LoadSequenceFromFile(stringPath, result.sequence);

            {
                lock_guard<mutex> lock(_prewarmMutex);
                _completedPrewarmSequences.push_back(std::move(result));
            }

            _activePrewarmSequenceJobs.fetch_sub(1);
        });

    if (enqueued)
        return;

    _activePrewarmSequenceJobs.fetch_sub(1);

    PrewarmSequenceResult result{};
    result.filePath = filePath;
    result.targetLevelIndex = targetLevelIndex;
    result.succeeded = Cinematic_SequenceIO::LoadSequenceFromFile(String::ToString(filePath), result.sequence);

    {
        lock_guard<mutex> lock(_prewarmMutex);
        _completedPrewarmSequences.push_back(std::move(result));
    }
}

void CinematicSequenceRunner::Flush_PrewarmQueue(uint32 createBudget)
{
    const auto startedAt = chrono::steady_clock::now();
    const size_t startActorCount = _prewarmedActors.size();
    const uint32 budget = max(1u, createBudget);

    while (true)
    {
        Drain_CompletedPrewarmSequences();
        const size_t processedCount = Process_PrewarmSpawnRequests(budget);

        if (_activePrewarmSequenceJobs.load() == 0)
        {
            Drain_CompletedPrewarmSequences();
            if (_pendingPrewarmSpawnRequests.empty())
                break;
        }

        if (processedCount == 0)
            this_thread::yield();
    }

#ifdef _DEBUG
    const auto elapsed = chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - startedAt);
    const size_t createdActorCount = _prewarmedActors.size() - min(_prewarmedActors.size(), startActorCount);
    if (createdActorCount > 0 || elapsed.count() > 0)
    {
        LOG_DEBUG(
            "[CinematicSequenceRunner::Prewarm] sequences={}, actorsCreated={}, elapsed={}ms",
            _prewarmedSequences.size(),
            createdActorCount,
            elapsed.count());
    }
#endif
}

void CinematicSequenceRunner::Clear_PrewarmQueue()
{
    _pendingPrewarmSpawnRequests.clear();
    _queuedPrewarmSequencePaths.clear();
    _prewarmedSequences.clear();

    {
        lock_guard<mutex> lock(_prewarmMutex);
        _completedPrewarmSequences.clear();
    }

    for (const PrewarmedActorState& state : _prewarmedActors)
    {
        if (state.actor != nullptr)
            GAME->Remove_GameObject(state.levelIndex, state.actor);
    }

    _prewarmedActors.clear();
}

void CinematicSequenceRunner::Drain_CompletedPrewarmSequences()
{
    deque<PrewarmSequenceResult> completedSequences{};

    {
        lock_guard<mutex> lock(_prewarmMutex);
        completedSequences.swap(_completedPrewarmSequences);
    }

    while (!completedSequences.empty())
    {
        PrewarmSequenceResult result = std::move(completedSequences.front());
        completedSequences.pop_front();

        if (!result.succeeded)
        {
            LOG_WARN(
                "[CinematicSequenceRunner::Prewarm] sequence load failed. path={}",
                String::ToString(result.filePath));
            _queuedPrewarmSequencePaths.erase(result.filePath);
            continue;
        }

        const auto [iter, inserted] = _prewarmedSequences.emplace(result.filePath, std::move(result.sequence));
        if (inserted)
            Queue_StartFrameSpawnPrewarmRequests(result.filePath, result.targetLevelIndex, iter->second);

        _queuedPrewarmSequencePaths.erase(result.filePath);
    }
}

size_t CinematicSequenceRunner::Process_PrewarmSpawnRequests(uint32 createBudget)
{
    if (createBudget == 0)
        return 0;

    uint32 remainingBudget = createBudget;
    size_t processedCount = 0;

    while (remainingBudget > 0 && !_pendingPrewarmSpawnRequests.empty())
    {
        PrewarmSpawnRequest request = std::move(_pendingPrewarmSpawnRequests.front());
        _pendingPrewarmSpawnRequests.erase(_pendingPrewarmSpawnRequests.begin());
        ++processedCount;

        if (request.targetId.empty() || request.payload.prefabName.empty())
            continue;

        if (Is_PrewarmSpawnAlreadyQueuedOrReady(
            request.sequencePath,
            request.targetId,
            request.payload.prefabName))
        {
            continue;
        }

        if (request.payload.cleanupPolicy == CinematicCleanupPolicy::Keep &&
            Find_ReusableActorByPrefabName(request.targetLevelIndex, request.payload.prefabName) != nullptr)
        {
            continue;
        }

        Shared<GameObject> actor = Spawn_Prefab::Prefab(request.payload.prefabName)
            .AtLevel(request.targetLevelIndex)
            .InLayer(request.payload.layerTag)
            .Position(request.payload.position)
            .Rotation(request.payload.rotation)
            .Spawn();

        --remainingBudget;

        if (actor == nullptr)
        {
            LOG_WARN(
                "[CinematicSequenceRunner::Prewarm] actor prewarm failed. target='{}' prefab='{}'",
                request.targetId,
                request.payload.prefabName);
            continue;
        }

        PrewarmedActorState state{};
        state.sequencePath = request.sequencePath;
        state.targetId = request.targetId;
        state.prefabName = request.payload.prefabName;
        state.levelIndex = request.targetLevelIndex;
        state.actor = actor;

        Suppress_PrewarmedActor(state);
        actor->Try_BeginPlay();

        _prewarmedActors.push_back(std::move(state));
    }

    return processedCount;
}

void CinematicSequenceRunner::Queue_StartFrameSpawnPrewarmRequests(
    const wstring& sequencePath,
    uint32 targetLevelIndex,
    const CINEMATIC_SEQUENCE& sequence)
{
    for (const CINEMATIC_EVENT& event : sequence.eventTrack)
    {
        if (event.frame > sequence.startFrame)
            break;

        const auto* spawn = get_if<CinematicSpawnEventData>(&event.payload);
        if (spawn == nullptr)
            continue;

        if (event.targetId.empty() || spawn->prefabName.empty())
            continue;

        if (Is_PrewarmSpawnAlreadyQueuedOrReady(sequencePath, event.targetId, spawn->prefabName))
            continue;

        PrewarmSpawnRequest request{};
        request.sequencePath = sequencePath;
        request.targetLevelIndex = targetLevelIndex;
        request.targetId = event.targetId;
        request.payload = *spawn;
        _pendingPrewarmSpawnRequests.push_back(std::move(request));
    }
}

bool CinematicSequenceRunner::Is_PrewarmSpawnAlreadyQueuedOrReady(
    const wstring& sequencePath,
    const string& targetId,
    const string& prefabName) const
{
    for (const PrewarmedActorState& state : _prewarmedActors)
    {
        if (state.sequencePath == sequencePath &&
            state.targetId == targetId &&
            state.prefabName == prefabName &&
            state.actor != nullptr)
        {
            return true;
        }
    }

    for (const PrewarmSpawnRequest& request : _pendingPrewarmSpawnRequests)
    {
        if (request.sequencePath == sequencePath &&
            request.targetId == targetId &&
            request.payload.prefabName == prefabName)
        {
            return true;
        }
    }

    return false;
}

bool CinematicSequenceRunner::Try_ConsumePrewarmedActor(const string& targetId, const CinematicSpawnEventData& payload)
{
    if (_activeSequencePath.empty())
        return false;

    auto iter = find_if(
        _prewarmedActors.begin(),
        _prewarmedActors.end(),
        [&](const PrewarmedActorState& state)
        {
            return state.sequencePath == _activeSequencePath &&
                state.levelIndex == _sequenceLevelIndex &&
                state.targetId == targetId &&
                state.prefabName == payload.prefabName &&
                state.actor != nullptr &&
                !state.actor->Is_MarkDestroy();
        });

    if (iter == _prewarmedActors.end())
        return false;

    PrewarmedActorState state = std::move(*iter);
    _prewarmedActors.erase(iter);

    if (state.actor->Get_Transform())
    {
        state.actor->Get_Transform()->Set_WorldPosition(payload.position);
        state.actor->Get_Transform()->Set_WorldRotationEuler(payload.rotation);
        state.actor->Get_Transform()->Set_Scale(payload.scale);
    }

    Apply_CinematicDrivenState(state.actor, false);
    Restore_PrewarmedActor(state);
    Bind_ActorToTarget(targetId, state.actor, true, payload.cleanupPolicy);

    LOG_INFO(
        "[CinematicSequenceRunner] SpawnEvent used prewarmed actor: target='{}' prefab='{}' frame={}",
        targetId,
        payload.prefabName,
        Get_CurrentFrame());
    return true;
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
    else if (const auto* partVisibility = get_if<CinematicPartVisibilityEventData>(&event.payload))
    {
        Execute_PartVisibilityEvent(event.targetId, *partVisibility);
    }
    else if (const auto* worldTimeScale = get_if<CinematicWorldTimeScaleEventData>(&event.payload))
    {
        Execute_WorldTimeScaleEvent(*worldTimeScale);
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

    if (Try_ConsumePrewarmedActor(targetId, payload))
        return true;

    if (payload.cleanupPolicy == CinematicCleanupPolicy::Keep)
    {
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
    model->Set_AnimationPaused(payload.holdFrame);

    if (auto iter = _bindingStates.find(targetId); iter != _bindingStates.end())
    {
        const float sequenceFps = static_cast<float>(std::max(_sequence.fps, 1));
        const float frameOvershoot = std::max(currentFrameExact - static_cast<float>(eventFrame), 0.f);

        iter->second.hasActivePlayAnim = true;
        iter->second.activePlayAnim = payload;
        iter->second.activePlayAnimEventFrame = eventFrame;
        iter->second.pendingPlayAnimInitialAdvanceSec = payload.holdFrame
            ? -1.f
            : (frameOvershoot > 0.f ? frameOvershoot / sequenceFps : -1.f);
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

bool CinematicSequenceRunner::Execute_PartVisibilityEvent(
    const string& targetId,
    const CinematicPartVisibilityEventData& payload)
{
    if (targetId.empty())
    {
        LOG_WARN("[CinematicSequenceRunner] PartVisibilityEvent skipped: empty targetId. frame={}", Get_CurrentFrame());
        return false;
    }

    if (payload.partTag.empty())
    {
        LOG_WARN("[CinematicSequenceRunner] PartVisibilityEvent skipped: target='{}' has empty partTag. frame={}",
            targetId,
            Get_CurrentFrame());
        return false;
    }

    Shared<GameObject> actor = Get_BoundActor(targetId);
    if (actor == nullptr)
    {
        LOG_WARN("[CinematicSequenceRunner] PartVisibilityEvent failed: target='{}' is not bound. frame={}",
            targetId,
            Get_CurrentFrame());
        return false;
    }

    const auto container = dynamic_pointer_cast<ContainerObject>(actor);
    if (container == nullptr)
    {
        LOG_WARN("[CinematicSequenceRunner] PartVisibilityEvent failed: target='{}' is not ContainerObject. frame={}",
            targetId,
            Get_CurrentFrame());
        return false;
    }

    PartObject* part = container->Get_PartObject(String::ToWString(payload.partTag));
    if (part == nullptr)
    {
        LOG_WARN("[CinematicSequenceRunner] PartVisibilityEvent failed: target='{}' part='{}' not found. frame={}",
            targetId,
            payload.partTag,
            Get_CurrentFrame());
        return false;
    }

    const bool previousVisible = part->Is_Visible();
    part->Set_Visible(payload.visible);
    LOG_INFO(
        "[CinematicSequenceRunner] PartVisibilityEvent success: target='{}' part='{}' visible={} previousVisible={} frame={}",
        targetId,
        payload.partTag,
        payload.visible,
        previousVisible,
        Get_CurrentFrame());
    return true;
}

bool CinematicSequenceRunner::Execute_WorldTimeScaleEvent(const CinematicWorldTimeScaleEventData& payload)
{
    if (!_hasWorldTimeScaleOverride)
    {
        _restoreWorldTimeScale = GAME->Get_WorldTimeScale();
        _hasWorldTimeScaleOverride = true;
    }

    const float clampedWorldTimeScale = std::clamp(payload.worldTimeScale, 0.f, 8.f);
    GAME->Set_WorldTimeScale(clampedWorldTimeScale);

    LOG_INFO(
        "[CinematicSequenceRunner] WorldTimeScaleEvent success: scale={:.3f} frame={}",
        clampedWorldTimeScale,
        Get_CurrentFrame());
    return true;
}

void CinematicSequenceRunner::Apply_ActorTransformTracks(float currentFrameExact)
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
        if (!Evaluate_ActorTransformTrack(track, currentFrameExact, position, rotation))
            continue;

        transform->Set_WorldPosition(position);
        transform->Set_WorldRotationEuler(rotation);
        transform->Update_WorldMatrix();

        if (const auto container = dynamic_pointer_cast<ContainerObject>(actor))
            container->Refresh_PartObjectTransforms();
    }
}

void CinematicSequenceRunner::Apply_AuthoredActorTracks(float currentFrameExact)
{
    Apply_ActorTransformTracks(currentFrameExact);
    Apply_PartRotationTracks(currentFrameExact);
}

void CinematicSequenceRunner::Apply_PartRotationTracks(float currentFrameExact)
{
    for (const auto& track : _sequence.partRotationTracks)
    {
        if (!track.enabled || track.targetId.empty() || track.partTag.empty())
            continue;

        Shared<GameObject> actor = Get_BoundActor(track.targetId);
        if (actor == nullptr)
            continue;

        const auto container = dynamic_pointer_cast<ContainerObject>(actor);
        if (container == nullptr)
            continue;

        PartObject* part = container->Get_PartObject(String::ToWString(track.partTag));
        if (part == nullptr)
            continue;

        Shared<TransformCom> transform = part->Get_Transform();
        if (transform == nullptr)
            continue;

        Vec3 rotation = Vec3::Zero;
        if (!Evaluate_PartRotationTrack(track, currentFrameExact, rotation))
            continue;

        transform->Set_RotationEuler(rotation);
        transform->Update_WorldMatrix();
        part->Update(0.f);
    }
}

bool CinematicSequenceRunner::Evaluate_ActorTransformTrack(
    const CINEMATIC_ACTOR_TRANSFORM_TRACK& track,
    float currentFrameExact,
    Vec3& outPosition,
    Vec3& outRotation) const
{
    if (!track.enabled || track.keyframes.empty())
        return false;

    const auto& keyframes = track.keyframes;
    if (currentFrameExact <= static_cast<float>(keyframes.front().frame))
    {
        outPosition = keyframes.front().position;
        outRotation = keyframes.front().rotation;
        return true;
    }

    if (currentFrameExact >= static_cast<float>(keyframes.back().frame))
    {
        outPosition = keyframes.back().position;
        outRotation = keyframes.back().rotation;
        return true;
    }

    for (size_t i = 0; i + 1 < keyframes.size(); ++i)
    {
        const auto& left = keyframes[i];
        const auto& right = keyframes[i + 1];
        if (currentFrameExact < static_cast<float>(left.frame) ||
            currentFrameExact > static_cast<float>(right.frame))
            continue;

        if (left.frame == right.frame)
        {
            outPosition = right.position;
            outRotation = right.rotation;
            return true;
        }

        const float t = (currentFrameExact - static_cast<float>(left.frame)) /
            static_cast<float>(right.frame - left.frame);
        outPosition = Vec3::Lerp(left.position, right.position, t);
        outRotation = LerpRotationDegreesShortest(left.rotation, right.rotation, t);
        return true;
    }

    return false;
}

bool CinematicSequenceRunner::Evaluate_PartRotationTrack(
    const CINEMATIC_PART_ROTATION_TRACK& track,
    float currentFrameExact,
    Vec3& outRotation) const
{
    if (!track.enabled || track.keyframes.empty())
        return false;

    const auto& keyframes = track.keyframes;
    if (currentFrameExact <= static_cast<float>(keyframes.front().frame))
    {
        outRotation = keyframes.front().rotation;
        return true;
    }

    if (currentFrameExact >= static_cast<float>(keyframes.back().frame))
    {
        outRotation = keyframes.back().rotation;
        return true;
    }

    for (size_t i = 0; i + 1 < keyframes.size(); ++i)
    {
        const auto& left = keyframes[i];
        const auto& right = keyframes[i + 1];
        if (currentFrameExact < static_cast<float>(left.frame) ||
            currentFrameExact > static_cast<float>(right.frame))
        {
            continue;
        }

        if (left.frame == right.frame)
        {
            outRotation = right.rotation;
            return true;
        }

        const float t = (currentFrameExact - static_cast<float>(left.frame)) /
            static_cast<float>(right.frame - left.frame);
        outRotation = LerpRotationDegreesShortest(left.rotation, right.rotation, t);
        return true;
    }

    return false;
}

void CinematicSequenceRunner::Apply_BoundActorAnimationPreview(float currentFrameExact)
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

        const float elapsedFrame = std::max(currentFrameExact - static_cast<float>(state.activePlayAnimEventFrame), 0.f);
        const float elapsedSec = elapsedFrame / sequenceFps;

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

        if (!state.hasActivePlayAnim || state.actor == nullptr)
            continue;

        Shared<ModelCom> model = Find_TargetModel(state.actor);
        if (model == nullptr)
            continue;

        if (state.activePlayAnim.holdFrame)
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
    if (_hasWorldTimeScaleOverride)
    {
        GAME->Set_WorldTimeScale(_restoreWorldTimeScale);
        _hasWorldTimeScaleOverride = false;
        _restoreWorldTimeScale = 1.f;
    }

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
    _activeSequencePath.clear();

    if (forceRemoveAllSpawned)
        Clear_CompletedCallbacks();
}

void CinematicSequenceRunner::Set_GameplayUIHiddenForCinematic(bool hidden) const
{
    if (const Shared<GameplayUI> gameplayUI = CLIENT->Get_GameplayUI())
        gameplayUI->Set_CinematicUIHidden(hidden);
}

Shared<GameObject> CinematicSequenceRunner::Find_ReusableActorByPrefabName(const string& prefabName) const
{
    return Find_ReusableActorByPrefabName(_sequenceLevelIndex, prefabName);
}

Shared<GameObject> CinematicSequenceRunner::Find_ReusableActorByPrefabName(uint32 levelIndex, const string& prefabName) const
{
    if (prefabName.empty())
        return nullptr;

    Shared<GameObject> matchedActor = nullptr;
    GAME->Visit_LevelObjects(
        levelIndex,
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

void CinematicSequenceRunner::Suppress_PrewarmedActor(PrewarmedActorState& state) const
{
    if (state.actor == nullptr)
        return;

    state.wasVisible = state.actor->Is_Visible();
    state.actor->Set_Visible(false);

    Visit_ActorColliders(
        state.actor,
        [&](const Shared<ColliderCom>& collider)
        {
            if (collider == nullptr)
                return;

            PrewarmedColliderState colliderState{};
            colliderState.collider = collider;
            colliderState.wasActive = collider->Get_IsActive();
            state.colliderStates.push_back(colliderState);

            collider->Clear_Overlap();
            collider->Set_IsActive(false);
        });

    Apply_CinematicDrivenState(state.actor, true);
}

void CinematicSequenceRunner::Restore_PrewarmedActor(PrewarmedActorState& state) const
{
    if (state.actor == nullptr)
        return;

    state.actor->Set_Visible(state.wasVisible);

    for (const PrewarmedColliderState& colliderState : state.colliderStates)
    {
        if (colliderState.collider != nullptr)
            colliderState.collider->Set_IsActive(colliderState.wasActive);
    }
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
    
    if (const auto worldPlayer = dynamic_pointer_cast<Player_WorldBase>(actor))
    {
        worldPlayer->Set_CinematicDriven(cinematicDriven);
        return;
    }

    if (const auto dualliste = dynamic_pointer_cast<SL_Dualliste>(actor))
    {
        dualliste->Set_CinematicDriven(cinematicDriven);
        return;
    }

    if (const auto maelle = dynamic_pointer_cast<SL_Maelle>(actor))
    {
        if (cinematicDriven)
        {
            if (const Shared<SL_TargetCom> targetCom = maelle->Get_Component<SL_TargetCom>())
                targetCom->Clear_Candidates();
        }

        maelle->Set_CinematicDriven(cinematicDriven);
    }
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
    Clear_PrewarmQueue();
    __super::Free();
}

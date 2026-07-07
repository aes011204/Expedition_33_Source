#include "CinematicSequenceRunner.h"
#include "Cinematic_Player.h"
#include "Cinematic_SequenceIO.h"
#include "Cinematic_Utils.h"
#include "CinematicCamera.h"
#include "ClientInstance.h"
#include "ContainerObject.h"
#include "EffectCom.h"
#include "EffectInstancePool.h"
#include "GameInstance.h"
#include "GameplayUI.h"
#include "Hair_Player.h"
#include "Helper_String.h"
#include "ModelCom.h"
#include "Monster_WorldBase.h"
#include "Player_WorldBase.h"
#include "PartObject.h"
#include "ResourceLoader.h"
#include "SL_Dualliste.h"
#include "SL_Maelle.h"
#include "SL_TargetCom.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <system_error>
#include <thread>

namespace
{
constexpr float kPerfDropLogThresholdSec = 1.f / 30.f;
constexpr float kPerfDropLogCooldownSec = 0.25f;

using PerfClock = std::chrono::steady_clock;

float ElapsedMs(PerfClock::time_point start)
{
    return std::chrono::duration<float, std::milli>(PerfClock::now() - start).count();
}

bool NearlyEqualVec3(const Vec3& lhs, const Vec3& rhs, float epsilon = 0.0005f)
{
    return (lhs - rhs).LengthSquared() <= epsilon * epsilon;
}

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

wstring Make_PathKey(const fs::path& path)
{
    return path.lexically_normal().wstring();
}

bool Load_JsonFileForPrewarm(const fs::path& path, json& outRoot, fs::file_time_type& outLastWriteTime)
{
    outRoot = json{};
    outLastWriteTime = {};

    if (path.empty() || !fs::exists(path))
        return false;

    std::error_code beforeError{};
    const fs::file_time_type beforeWriteTime = fs::last_write_time(path, beforeError);
    if (beforeError)
        return false;

    ifstream file(path);
    if (!file.is_open())
        return false;

    file >> outRoot;
    file.close();

    std::error_code afterError{};
    const fs::file_time_type afterWriteTime = fs::last_write_time(path, afterError);
    if (afterError || beforeWriteTime != afterWriteTime)
        return false;

    outLastWriteTime = afterWriteTime;
    return outRoot.is_object();
}

ModelType Parse_ModelTypeForPrewarm(const string& modelType)
{
    return modelType == "Anim" || modelType == "ANIM"
        ? ModelType::Anim
        : ModelType::NonAnim;
}

Matrix Read_PreLocalTransformForPrewarm(const json& data)
{
    if (!data.contains("pre_local_transform") ||
        !data["pre_local_transform"].is_array() ||
        data["pre_local_transform"].size() < 16)
    {
        return Matrix::Identity;
    }

    const json& values = data["pre_local_transform"];
    return Matrix{
        values[0].get<float>(), values[1].get<float>(), values[2].get<float>(), values[3].get<float>(),
        values[4].get<float>(), values[5].get<float>(), values[6].get<float>(), values[7].get<float>(),
        values[8].get<float>(), values[9].get<float>(), values[10].get<float>(), values[11].get<float>(),
        values[12].get<float>(), values[13].get<float>(), values[14].get<float>(), values[15].get<float>()
    };
}

bool Is_WeaponPrefabName(const string& prefabName)
{
    return prefabName.starts_with("WP_");
}

void Add_UniqueString(vector<string>& values, const string& value)
{
    if (value.empty())
        return;

    if (find(values.begin(), values.end(), value) != values.end())
        return;

    values.push_back(value);
}

void Add_UniqueModelDependency(
    vector<CinematicSequenceRunner::PrewarmModelDependency>& dependencies,
    CinematicSequenceRunner::PrewarmModelDependency dependency)
{
    if (dependency.prototypeTag.empty() || (dependency.modelGuid.empty() && dependency.modelPath.empty()))
        return;

    const auto iter = find_if(
        dependencies.begin(),
        dependencies.end(),
        [&](const CinematicSequenceRunner::PrewarmModelDependency& existing)
        {
            return existing.prototypeTag == dependency.prototypeTag;
        });

    if (iter != dependencies.end())
        return;

    dependencies.push_back(std::move(dependency));
}

void Collect_PrefabDependencyJson(
    const json& objectJson,
    vector<string>& referencedPrefabNames,
    vector<CinematicSequenceRunner::PrewarmModelDependency>& modelDependencies)
{
    if (!objectJson.is_object())
        return;

    const string sourcePrefab = objectJson.value("source_prefab", string{});
    if (Is_WeaponPrefabName(sourcePrefab))
        Add_UniqueString(referencedPrefabNames, sourcePrefab);

    if (objectJson.contains("components") && objectJson["components"].is_array())
    {
        for (const json& componentJson : objectJson["components"])
        {
            if (!componentJson.is_object() || !componentJson.contains("model_type"))
                continue;

            CinematicSequenceRunner::PrewarmModelDependency dependency{};
            dependency.modelGuid = componentJson.value("model_guid", string{});
            dependency.modelPath = String::ToWString(componentJson.value("model_path", string{}));
            dependency.prototypeTag = String::ToWString(componentJson.value("prototype_tag", string{}));
            dependency.modelType = Parse_ModelTypeForPrewarm(componentJson.value("model_type", string{}));
            dependency.preLocalTransform = Read_PreLocalTransformForPrewarm(componentJson);
            Add_UniqueModelDependency(modelDependencies, std::move(dependency));
        }
    }

    if (objectJson.contains("parts") && objectJson["parts"].is_array())
    {
        for (const json& partJson : objectJson["parts"])
        {
            if (!partJson.is_object() || !partJson.contains("object"))
                continue;

            Collect_PrefabDependencyJson(partJson["object"], referencedPrefabNames, modelDependencies);
        }
    }
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

string Resolve_ObjectLabel(const Shared<GameObject>& object)
{
    if (object == nullptr)
        return "<null>";

    const string& prefabName = object->Get_SourcePrefabName();
    if (!prefabName.empty())
        return prefabName;

    const wstring& objectName = object->Get_Name();
    return objectName.empty() ? "<unnamed>" : String::ToString(objectName);
}

bool Try_GetWorldPosition(const Shared<GameObject>& object, Vec3& outPosition)
{
    if (object == nullptr || object->Get_Transform() == nullptr)
        return false;

    outPosition = object->Get_Transform()->Get_WorldPosition();
    return true;
}

EffectCom::AttachedEffectDebugSummary Collect_AttachedEffectDebugSummary(const Shared<GameObject>& actor)
{
    EffectCom::AttachedEffectDebugSummary summary{};
    if (actor == nullptr)
        return summary;

    const auto addSummary = [&summary](const Shared<EffectCom>& effectCom)
        {
            if (effectCom == nullptr)
                return;

            const EffectCom::AttachedEffectDebugSummary current = effectCom->Get_AttachedEffectDebugSummary();
            summary.totalCount += current.totalCount;
            summary.activeCount += current.activeCount;
            summary.trailCount += current.trailCount;
            summary.trailDrainingCount += current.trailDrainingCount;
            summary.sourceHistoryCount += current.sourceHistoryCount;
            summary.sourceHistoryDrainingCount += current.sourceHistoryDrainingCount;
        };

    addSummary(actor->Get_Component<EffectCom>());

    const Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(actor);
    if (container == nullptr)
        return summary;

    for (const auto& [partTag, partObject] : container->Get_PartObjects())
    {
        UNREFERENCED_PARAMETER(partTag);

        if (partObject != nullptr)
            addSummary(partObject->Get_Component<EffectCom>());
    }

    return summary;
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
    _suppressAnimNotifyExecutionForBoundActors = false;
    _usePreviewPlaybackOptimizations = false;

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
    _suppressAnimNotifyExecutionForBoundActors = false;
    _usePreviewPlaybackOptimizations = true;

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

void CinematicSequenceRunner::Stop_Sequence(bool forceRemoveAllSpawned)
{
    if (_player == nullptr)
        return;

    const bool wasPlaying = _player->Is_CinematicPlaying();
    if (!wasPlaying)
        return;

    _player->Stop();
    Clear_CompletedCallbacks();
    Cleanup_Context(forceRemoveAllSpawned);
    _suppressAnimNotifyExecutionForBoundActors = false;
    _usePreviewPlaybackOptimizations = false;
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
        Execute_EventsUpToFrame(currentFrame, static_cast<float>(currentFrame), true);
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_AuthoredActorTracks(static_cast<float>(currentFrame));
    if (_applyEventTrack)
        Apply_BoundActorAnimationPreview(static_cast<float>(currentFrame), true);
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
    const bool suppressTransientEffects = requiresContextRebuild;

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
        Execute_EventsUpToFrame(targetFrame, static_cast<float>(targetFrame), suppressTransientEffects);

    if (applyEventTrack && applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_AuthoredActorTracks(static_cast<float>(targetFrame));

    if (applyEventTrack)
        Apply_BoundActorAnimationPreview(static_cast<float>(targetFrame), requiresContextRebuild);
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
    const bool suppressTransientEffects = requiresContextRebuild;

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
        Execute_EventsUpToFrame(targetFrame, targetFrameExact, suppressTransientEffects);

    if (applyEventTrack && applyActorTransformTrack && !_suspendActorTransformTrackPreview)
        Apply_AuthoredActorTracks(targetFrameExact);

    if (applyEventTrack)
        Apply_BoundActorAnimationPreview(targetFrameExact, requiresContextRebuild);
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

    const PerfClock::time_point runnerStart = PerfClock::now();
    FramePerfBreakdown perf{};

    PerfClock::time_point stageStart = PerfClock::now();
    _player->Update(timeDelta);
    perf.playerMs = ElapsedMs(stageStart);

    stageStart = PerfClock::now();
    Apply_ActiveCameraTrackPose(_player->Get_CurrentFrameExact());
    perf.cameraMs = ElapsedMs(stageStart);

    const int currentFrame = _player->Get_CurrentFrame();
    const float currentFrameExact = _player->Get_CurrentFrameExact();
    const bool isPlayingAfterUpdate = _player->Is_CinematicPlaying();
    const bool willTickBoundAnimations =
        tickBoundAnimationsManually && isPlayingAfterUpdate && !_player->Is_CinematicPaused();

    if (_applyEventTrack)
    {
        stageStart = PerfClock::now();
        Execute_EventsUpToFrame(currentFrame, currentFrameExact);
        perf.eventsMs = ElapsedMs(stageStart);
    }
    if (_applyEventTrack && _applyActorTransformTrack && !_suspendActorTransformTrackPreview)
    {
        stageStart = PerfClock::now();
        Apply_AuthoredActorTracks(currentFrameExact, willTickBoundAnimations, &perf);
        perf.actorTracksMs = ElapsedMs(stageStart);
    }

    if (willTickBoundAnimations)
    {
        stageStart = PerfClock::now();
        Update_BoundActorAnimations(timeDelta, &perf);
        perf.animMs = ElapsedMs(stageStart);
        if (_usePreviewPlaybackOptimizations)
        {
            stageStart = PerfClock::now();
            Update_BoundActorAttachedEffects(timeDelta, &perf);
            perf.attachedEffectMs = ElapsedMs(stageStart);
        }
    }

    perf.runnerMs = ElapsedMs(runnerStart);
    Log_PerfDropIfNeeded(timeDelta, currentFrameExact, perf);

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
    UNREFERENCED_PARAMETER(targetLevelIndex);

    if (filePath.empty())
        return;

    if (_prewarmedSequences.find(filePath) != _prewarmedSequences.end())
        return;

    if (!_queuedPrewarmSequencePaths.insert(filePath).second)
        return;

    _activePrewarmSequenceJobs.fetch_add(1);

    const bool enqueued = GAME->Enqueue_ThreadJob(
        [this, filePath]()
        {
            PrewarmSequenceResult result{};
            result.filePath = filePath;

            const string stringPath = String::ToString(filePath);
            result.succeeded = Cinematic_SequenceIO::LoadSequenceFromFile(stringPath, result.sequence);
            if (result.succeeded)
            {
                for (const CINEMATIC_EVENT& event : result.sequence.eventTrack)
                {
                    if (const auto* spawn = get_if<CinematicSpawnEventData>(&event.payload))
                        Add_UniqueString(result.spawnPrefabNames, spawn->prefabName);
                }
            }

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
    result.succeeded = Cinematic_SequenceIO::LoadSequenceFromFile(String::ToString(filePath), result.sequence);
    if (result.succeeded)
    {
        for (const CINEMATIC_EVENT& event : result.sequence.eventTrack)
        {
            if (const auto* spawn = get_if<CinematicSpawnEventData>(&event.payload))
                Add_UniqueString(result.spawnPrefabNames, spawn->prefabName);
        }
    }

    {
        lock_guard<mutex> lock(_prewarmMutex);
        _completedPrewarmSequences.push_back(std::move(result));
    }
}

void CinematicSequenceRunner::Flush_PrewarmQueue(uint32 createBudget)
{
    const auto startedAt = chrono::steady_clock::now();
    const uint32 budget = max(1u, createBudget);

    while (true)
    {
        Drain_CompletedPrewarmSequences();
        Drain_CompletedPrewarmPrefabs();
        const size_t processedModelCount = Process_PrewarmModelDependencies(budget);

        if (_activePrewarmSequenceJobs.load() == 0 && _activePrewarmPrefabJobs.load() == 0)
        {
            Drain_CompletedPrewarmSequences();
            Drain_CompletedPrewarmPrefabs();
            if (_pendingPrewarmModelDependencies.empty())
                break;
        }

        if (processedModelCount == 0)
            this_thread::yield();
    }

#ifdef _DEBUG
    const auto elapsed = chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - startedAt);
    if (!_prewarmedSequences.empty() || elapsed.count() > 0)
    {
        LOG_DEBUG(
            "[CinematicSequenceRunner::Prewarm] sequences={}, prefabs={}, models={}, actorsCreated=0, elapsed={}ms",
            _prewarmedSequences.size(),
            _prewarmedPrefabCount,
            _prewarmedModelCount,
            elapsed.count());
    }
#endif
}

void CinematicSequenceRunner::Clear_PrewarmQueue()
{
    _queuedPrewarmSequencePaths.clear();
    _queuedPrewarmPrefabPaths.clear();
    _prewarmedPrefabPaths.clear();
    _pendingPrewarmModelDependencies.clear();
    _queuedPrewarmModelPrototypeTags.clear();
    _prewarmedSequences.clear();
    _prewarmedPrefabCount = 0;
    _prewarmedModelCount = 0;
    Spawn_Prefab::Clear_PrefabJsonCache();

    {
        lock_guard<mutex> lock(_prewarmMutex);
        _completedPrewarmSequences.clear();
        _completedPrewarmPrefabs.clear();
    }
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

        _prewarmedSequences.emplace(result.filePath, std::move(result.sequence));
        for (const string& prefabName : result.spawnPrefabNames)
            Queue_PrewarmPrefabName(prefabName);

        _queuedPrewarmSequencePaths.erase(result.filePath);
    }
}

void CinematicSequenceRunner::Drain_CompletedPrewarmPrefabs()
{
    deque<PrewarmPrefabResult> completedPrefabs{};

    {
        lock_guard<mutex> lock(_prewarmMutex);
        completedPrefabs.swap(_completedPrewarmPrefabs);
    }

    while (!completedPrefabs.empty())
    {
        PrewarmPrefabResult result = std::move(completedPrefabs.front());
        completedPrefabs.pop_front();

        const wstring prefabPathKey = Make_PathKey(result.prefabPath);

        if (!result.succeeded)
        {
            LOG_WARN(
                "[CinematicSequenceRunner::Prewarm] prefab load failed. path={}",
                String::ToString(result.prefabPath.wstring()));
            _queuedPrewarmPrefabPaths.erase(prefabPathKey);
            continue;
        }

        if (!Spawn_Prefab::Cache_PrefabJsonIfUnmodified(
            result.prefabPath,
            result.root,
            result.lastWriteTime))
        {
            _queuedPrewarmPrefabPaths.erase(prefabPathKey);
            continue;
        }

        _prewarmedPrefabPaths.insert(prefabPathKey);
        ++_prewarmedPrefabCount;

        for (const string& prefabName : result.referencedPrefabNames)
            Queue_PrewarmPrefabName(prefabName);

        for (const PrewarmModelDependency& dependency : result.modelDependencies)
            Queue_PrewarmModelDependency(dependency);

        _queuedPrewarmPrefabPaths.erase(prefabPathKey);
    }
}

void CinematicSequenceRunner::Queue_PrewarmPrefabName(const string& prefabName)
{
    if (prefabName.empty())
        return;

    Queue_PrewarmPrefabPath(Spawn_Prefab::Resolve_PrefabPath(prefabName));
}

void CinematicSequenceRunner::Queue_PrewarmPrefabPath(const fs::path& prefabPath)
{
    if (prefabPath.empty())
        return;

    const wstring prefabPathKey = Make_PathKey(prefabPath);
    if (_prewarmedPrefabPaths.find(prefabPathKey) != _prewarmedPrefabPaths.end())
        return;

    if (!_queuedPrewarmPrefabPaths.insert(prefabPathKey).second)
        return;

    _activePrewarmPrefabJobs.fetch_add(1);

    const bool enqueued = GAME->Enqueue_ThreadJob(
        [this, prefabPath]()
        {
            PrewarmPrefabResult result{};
            result.prefabPath = prefabPath;
            result.succeeded = Load_JsonFileForPrewarm(prefabPath, result.root, result.lastWriteTime);
            if (result.succeeded)
            {
                Collect_PrefabDependencyJson(
                    result.root,
                    result.referencedPrefabNames,
                    result.modelDependencies);
            }

            {
                lock_guard<mutex> lock(_prewarmMutex);
                _completedPrewarmPrefabs.push_back(std::move(result));
            }

            _activePrewarmPrefabJobs.fetch_sub(1);
        });

    if (enqueued)
        return;

    _activePrewarmPrefabJobs.fetch_sub(1);

    PrewarmPrefabResult result{};
    result.prefabPath = prefabPath;
    result.succeeded = Load_JsonFileForPrewarm(prefabPath, result.root, result.lastWriteTime);
    if (result.succeeded)
    {
        Collect_PrefabDependencyJson(
            result.root,
            result.referencedPrefabNames,
            result.modelDependencies);
    }

    {
        lock_guard<mutex> lock(_prewarmMutex);
        _completedPrewarmPrefabs.push_back(std::move(result));
    }
}

void CinematicSequenceRunner::Queue_PrewarmModelDependency(const PrewarmModelDependency& dependency)
{
    if (dependency.prototypeTag.empty())
        return;

    if (GAME->Has_Prototype(ETOI(LevelType::Static), dependency.prototypeTag))
        return;

    if (!_queuedPrewarmModelPrototypeTags.insert(dependency.prototypeTag).second)
        return;

    _pendingPrewarmModelDependencies.push_back(dependency);
}

size_t CinematicSequenceRunner::Process_PrewarmModelDependencies(uint32 createBudget)
{
    if (createBudget == 0)
        return 0;

    Shared<ResourceLoader> resourceLoader = CLIENT->Get_ResourceLoader();
    if (resourceLoader == nullptr)
    {
        _queuedPrewarmModelPrototypeTags.clear();
        const size_t skippedCount = _pendingPrewarmModelDependencies.size();
        _pendingPrewarmModelDependencies.clear();
        return skippedCount;
    }

    uint32 remainingBudget = createBudget;
    size_t processedCount = 0;

    while (remainingBudget > 0 && !_pendingPrewarmModelDependencies.empty())
    {
        PrewarmModelDependency dependency = std::move(_pendingPrewarmModelDependencies.front());
        _pendingPrewarmModelDependencies.erase(_pendingPrewarmModelDependencies.begin());
        _queuedPrewarmModelPrototypeTags.erase(dependency.prototypeTag);
        ++processedCount;

        if (dependency.prototypeTag.empty())
            continue;

        if (GAME->Has_Prototype(ETOI(LevelType::Static), dependency.prototypeTag))
            continue;

        wstring resolvedModelPath = dependency.modelPath;
        if (!dependency.modelGuid.empty())
        {
            const wstring guidPath = GAME->Resolve_AssetPath(dependency.modelGuid);
            if (!guidPath.empty())
                resolvedModelPath = guidPath;
        }

        if (resolvedModelPath.empty())
            continue;

        ModelPrototypePrewarmDesc desc{};
        desc.levelIndex = ETOI(LevelType::Static);
        desc.prototypeTag = dependency.prototypeTag;
        desc.modelPath = resolvedModelPath;
        desc.modelType = dependency.modelType;
        desc.preLocalTransform = dependency.preLocalTransform;

        if (SUCCEEDED(resourceLoader->Prewarm_ModelPrototype(desc)))
            ++_prewarmedModelCount;

        --remainingBudget;
    }

    return processedCount;
}

void CinematicSequenceRunner::Execute_EventsUpToFrame(
    int currentFrame,
    float currentFrameExact,
    bool suppressTransientEffects)
{
    while (_nextEventIndex < static_cast<int>(_sequence.eventTrack.size()))
    {
        const CINEMATIC_EVENT& event = _sequence.eventTrack[_nextEventIndex];
        if (event.frame > currentFrame)
            break;

        Execute_Event(event, currentFrameExact, suppressTransientEffects);
        ++_nextEventIndex;
    }
}

void CinematicSequenceRunner::Execute_Event(
    const CINEMATIC_EVENT& event,
    float currentFrameExact,
    bool suppressTransientEffects)
{
    if (const auto* spawn = get_if<CinematicSpawnEventData>(&event.payload))
    {
        Execute_SpawnEvent(event.targetId, *spawn);
    }
    else if (const auto* playAnim = get_if<CinematicPlayAnimEventData>(&event.payload))
    {
        Execute_PlayAnimEvent(event.frame, currentFrameExact, event.targetId, *playAnim);
    }
    else if (const auto* playEffect = get_if<CinematicPlayEffectEventData>(&event.payload))
    {
        if (!suppressTransientEffects)
            Execute_PlayEffectEvent(event.targetId, *playEffect);
    }
    else if (const auto* playSound = get_if<CinematicPlaySoundEventData>(&event.payload))
    {
        if (!suppressTransientEffects)
            Execute_PlaySoundEvent(event.targetId, *playSound);
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

    if (payload.cleanupPolicy == CinematicCleanupPolicy::Keep)
    {
        if (Shared<GameObject> existingActor = Find_ReusableActorByPrefabName(payload.prefabName))
        {
            const bool wasSpawnedBySequence =
                _sequenceOwnedActors.find(existingActor.get()) != _sequenceOwnedActors.end();
            Bind_ActorToTarget(targetId, existingActor, wasSpawnedBySequence, payload.cleanupPolicy);

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
    _sequenceOwnedActors.insert(actor.get());

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
        iter->second.hasPreviewPlayAnimElapsedSec = false;
        iter->second.previewPlayAnimEventFrame = eventFrame;
        iter->second.previewPlayAnimElapsedSec = 0.f;
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

bool CinematicSequenceRunner::Execute_PlayEffectEvent(
    const string& targetId,
    const CinematicPlayEffectEventData& payload)
{
    if (targetId.empty())
    {
        LOG_WARN("[CinematicSequenceRunner] PlayEffectEvent skipped: empty targetId. frame={}", Get_CurrentFrame());
        return false;
    }

    if (payload.effectName.empty())
    {
        LOG_WARN("[CinematicSequenceRunner] PlayEffectEvent skipped: target='{}' has empty effectName. frame={}",
            targetId,
            Get_CurrentFrame());
        return false;
    }

    Shared<GameObject> actor = Get_BoundActor(targetId);
    if (actor == nullptr)
    {
        LOG_WARN("[CinematicSequenceRunner] PlayEffectEvent failed: target='{}' is not bound. frame={}",
            targetId,
            Get_CurrentFrame());
        return false;
    }

    const Shared<EffectCom> effectCom = EffectCom::Find_FromNotifyOwner(actor.get(), payload.target);
    if (effectCom == nullptr)
    {
        LOG_WARN(
            "[CinematicSequenceRunner] PlayEffectEvent failed: target='{}' effect='{}' has no EffectCom. frame={}",
            targetId,
            String::ToString(payload.effectName),
            Get_CurrentFrame());
        return false;
    }

    EffectCom::EffectPlayDesc desc{};
    desc.levelIndex = _sequenceLevelIndex;
    desc.effectName = payload.effectName;
    desc.target = payload.target;
    desc.partTag = payload.partTag;
    desc.socketName = payload.socketName;
    desc.layerTag = payload.layerTag;
    desc.localOffset = payload.localOffset;
    desc.localRotation = payload.localRotation;
    desc.scale = payload.scale;
    desc.playbackSpeed = std::max(payload.playbackSpeed, 0.01f);

    EffectInstancePool::AttachedHandle previewHandle{};
    const HRESULT result = _usePreviewPlaybackOptimizations
        ? effectCom->Play_Effect(desc, &previewHandle)
        : effectCom->Play_Effect(desc);

    if (FAILED(result))
    {
        LOG_WARN(
            "[CinematicSequenceRunner] PlayEffectEvent failed: target='{}' effect='{}' hr={} frame={}",
            targetId,
            String::ToString(payload.effectName),
            result,
            Get_CurrentFrame());
        return false;
    }

    LOG_INFO(
        "[CinematicSequenceRunner] PlayEffectEvent success: target='{}' effect='{}' frame={}",
        targetId,
        String::ToString(payload.effectName),
        Get_CurrentFrame());
    return true;
}

bool CinematicSequenceRunner::Execute_PlaySoundEvent(
    const string& targetId,
    const CinematicPlaySoundEventData& payload)
{
    if (payload.soundCue.empty())
    {
        LOG_WARN("[CinematicSequenceRunner] PlaySoundEvent skipped: empty soundCue. frame={}", Get_CurrentFrame());
        return false;
    }

    Vec3 soundPositionValue = Vec3::Zero;
    const Vec3* soundPosition = nullptr;
    if (payload.useTargetPosition && !targetId.empty())
    {
        Shared<GameObject> actor = Get_BoundActor(targetId);
        if (actor == nullptr)
        {
            LOG_WARN(
                "[CinematicSequenceRunner] PlaySoundEvent failed: target='{}' sound='{}' is not bound. frame={}",
                targetId,
                String::ToString(payload.soundCue),
                Get_CurrentFrame());
            return false;
        }

        if (!Try_GetWorldPosition(actor, soundPositionValue))
        {
            LOG_WARN(
                "[CinematicSequenceRunner] PlaySoundEvent failed: target='{}' sound='{}' has no transform. frame={}",
                targetId,
                String::ToString(payload.soundCue),
                Get_CurrentFrame());
            return false;
        }

        soundPosition = &soundPositionValue;
    }

    const float volume = std::clamp(payload.volume, 0.f, 10.f);
    const float speed = std::max(payload.speed, 0.01f);
    const float startRatio = std::clamp(payload.startRatio, 0.f, 0.99f);
    if (!GAME->Play_SoundCue(payload.soundCue, soundPosition, volume, speed, startRatio))
    {
        LOG_WARN(
            "[CinematicSequenceRunner] PlaySoundEvent failed: target='{}' sound='{}' frame={}",
            targetId,
            String::ToString(payload.soundCue),
            Get_CurrentFrame());
        return false;
    }

    LOG_INFO(
        "[CinematicSequenceRunner] PlaySoundEvent success: target='{}' sound='{}' frame={}",
        targetId,
        String::ToString(payload.soundCue),
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

    Stop_ActorAttachedEffects(iter->second.actor);

    if (iter->second.hasCinematicNotifyContextOverride)
    {
        if (Shared<ModelCom> model = Find_TargetModel(iter->second.actor))
            model->Set_CinematicNotifyContext(iter->second.previousCinematicNotifyContext);
    }

    if (iter->second.hasAnimNotifyExecutionOverride)
    {
        if (Shared<ModelCom> model = Find_TargetModel(iter->second.actor))
            model->Set_AnimNotifyExecutionEnabled(iter->second.previousAnimNotifyExecutionEnabled);
    }

    Restore_PreviewPlaybackOptimization(iter->second);
    Apply_CinematicDrivenState(iter->second.actor, false);

    if (iter->second.spawnedBySequence)
    {
        _sequenceOwnedActors.erase(iter->second.actor.get());
        GAME->Remove_GameObject(_sequenceLevelIndex, iter->second.actor);
    }

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

void CinematicSequenceRunner::Apply_ActorTransformTracks(
    float currentFrameExact,
    bool deferActiveAnimPartRefresh,
    FramePerfBreakdown* perf)
{
    for (const auto& track : _sequence.actorTransformTracks)
    {
        if (!track.enabled || track.targetId.empty())
            continue;

        if (perf != nullptr)
            ++perf->actorTrackCount;

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

        if (NearlyEqualVec3(transform->Get_WorldPosition(), position) &&
            NearlyEqualVec3(transform->Get_WorldRotationEuler(), rotation))
            continue;

        transform->Set_WorldPosition(position);
        transform->Set_WorldRotationEuler(rotation);
        transform->Update_WorldMatrix();
        if (perf != nullptr)
            ++perf->actorTrackAppliedCount;

        bool deferPartRefresh = false;
        if (deferActiveAnimPartRefresh)
        {
            const auto stateIt = _bindingStates.find(track.targetId);
            deferPartRefresh =
                stateIt != _bindingStates.end() &&
                stateIt->second.actor == actor &&
                stateIt->second.hasActivePlayAnim &&
                !stateIt->second.activePlayAnim.holdFrame &&
                Find_TargetModel(actor) != nullptr;
        }

        if (deferPartRefresh)
        {
            if (perf != nullptr)
                ++perf->actorTrackDeferredRefreshCount;
            continue;
        }

        if (const auto container = dynamic_pointer_cast<ContainerObject>(actor))
        {
            const PerfClock::time_point refreshStart = PerfClock::now();
            container->Refresh_PartObjectWorldMatrices();
            if (perf != nullptr)
            {
                perf->actorTrackRefreshMs += ElapsedMs(refreshStart);
                ++perf->actorTrackRefreshCount;
            }
        }
    }
}

void CinematicSequenceRunner::Apply_AuthoredActorTracks(
    float currentFrameExact,
    bool deferActiveAnimPartRefresh,
    FramePerfBreakdown* perf)
{
    Apply_ActorTransformTracks(currentFrameExact, deferActiveAnimPartRefresh, perf);
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
        part->Refresh_CombinedWorldMatrix();
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

void CinematicSequenceRunner::Apply_BoundActorAnimationPreview(float currentFrameExact, bool suppressNotifyExecution)
{
    const float sequenceFps = static_cast<float>(std::max(_sequence.fps, 1));

    for (auto& [targetId, state] : _bindingStates)
    {
        UNREFERENCED_PARAMETER(targetId);

        if (!state.hasActivePlayAnim || state.actor == nullptr)
            continue;

        Shared<ModelCom> model = Find_TargetModel(state.actor);
        if (model == nullptr)
            continue;

        const float elapsedFrame = std::max(currentFrameExact - static_cast<float>(state.activePlayAnimEventFrame), 0.f);
        const float elapsedSec = state.activePlayAnim.holdFrame
            ? 0.f
            : (elapsedFrame / sequenceFps);

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

        const bool previousNotifyExecutionEnabled = model->Is_AnimNotifyExecutionEnabled();
        const bool canExecutePreviewNotifyRange =
            !suppressNotifyExecution &&
            previousNotifyExecutionEnabled &&
            state.hasPreviewPlayAnimElapsedSec &&
            state.previewPlayAnimEventFrame == state.activePlayAnimEventFrame &&
            state.previewPlayAnimElapsedSec + 0.0001f < elapsedSec;

        model->Set_AnimationPaused(false);
        if (canExecutePreviewNotifyRange)
        {
            const float previousElapsedSec = std::max(state.previewPlayAnimElapsedSec, 0.f);
            const float notifyDeltaSec = elapsedSec - previousElapsedSec;

            if (previousElapsedSec > 0.f)
            {
                model->Set_AnimNotifyExecutionEnabled(false);
                model->Play_Animation(previousElapsedSec);
            }

            model->Set_AnimNotifyExecutionEnabled(true);
            model->Play_Animation(notifyDeltaSec);
        }
        else
        {
            model->Set_AnimNotifyExecutionEnabled(false);
            model->Play_Animation(elapsedSec);
        }
        model->Set_AnimationPaused(true);
        model->Set_AnimNotifyExecutionEnabled(previousNotifyExecutionEnabled);

        if (PartObject* animPart = Find_TargetAnimPart(state.actor))
            animPart->Update(0.f);

        state.hasPreviewPlayAnimElapsedSec = true;
        state.previewPlayAnimEventFrame = state.activePlayAnimEventFrame;
        state.previewPlayAnimElapsedSec = elapsedSec;
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

void CinematicSequenceRunner::Update_BoundActorAnimations(float timeDelta, FramePerfBreakdown* perf)
{
    if (timeDelta <= 0.f)
        return;

    for (auto& [targetId, state] : _bindingStates)
    {
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

        const PerfClock::time_point actorStart = PerfClock::now();
        PerfClock::time_point stageStart = PerfClock::now();
        const bool animationFinished = model->Play_Animation(animationDelta);
        const float playMs = ElapsedMs(stageStart);

        float partMs = 0.f;
        if (const auto container = dynamic_pointer_cast<ContainerObject>(state.actor))
        {
            stageStart = PerfClock::now();
            container->Refresh_PartObjectTransforms();
            partMs = ElapsedMs(stageStart);
        }
        else if (PartObject* animPart = Find_TargetAnimPart(state.actor))
        {
            stageStart = PerfClock::now();
            animPart->Update(0.f);
            partMs = ElapsedMs(stageStart);
        }

        if (perf != nullptr)
        {
            const float actorMs = ElapsedMs(actorStart);
            ++perf->animActorCount;
            perf->animPlayMs += playMs;
            perf->animPartMs += partMs;
            if (actorMs > perf->maxAnimActorMs)
            {
                perf->maxAnimActorMs = actorMs;
                perf->maxAnimActorId = targetId;
            }
        }

        if (animationFinished && !state.activePlayAnim.loop)
            state.hasActivePlayAnim = false;
    }
}

void CinematicSequenceRunner::Update_BoundActorAttachedEffects(float timeDelta, FramePerfBreakdown* perf) const
{
    if (timeDelta <= 0.f)
        return;

    std::unordered_set<EffectCom*> updatedEffectComs{};

    const auto updateEffectCom = [&updatedEffectComs, timeDelta, perf](const Shared<EffectCom>& effectCom)
        {
            if (effectCom == nullptr)
                return;

            if (!updatedEffectComs.insert(effectCom.get()).second)
                return;

            effectCom->Update_AttachedEffects(timeDelta);
            effectCom->Tick_AttachedEffectInstances(timeDelta);
            if (perf != nullptr)
                ++perf->attachedEffectComCount;
        };

    for (const auto& [targetId, state] : _bindingStates)
    {
        UNREFERENCED_PARAMETER(targetId);

        if (state.actor == nullptr)
            continue;

        updateEffectCom(state.actor->Get_Component<EffectCom>());

        const Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(state.actor);
        if (container == nullptr)
            continue;

        for (const auto& [partTag, partObject] : container->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);

            if (partObject == nullptr)
                continue;

            updateEffectCom(partObject->Get_Component<EffectCom>());
        }
    }
}

void CinematicSequenceRunner::Log_PerfDropIfNeeded(
    float timeDelta,
    float currentFrameExact,
    const FramePerfBreakdown& perf)
{
    _perfDropLogCooldownSec = max(0.f, _perfDropLogCooldownSec - max(0.f, timeDelta));
    if (timeDelta < kPerfDropLogThresholdSec || _perfDropLogCooldownSec > 0.f)
        return;

    _perfDropLogCooldownSec = kPerfDropLogCooldownSec;

    EffectInstancePool::DebugSummary poolSummary{};
    string activeEffectBreakdown = "<no-pool>";
    if (CLIENT != nullptr)
    {
        if (EffectInstancePool* effectPool = CLIENT->Get_EffectInstancePool())
        {
            poolSummary = effectPool->Get_DebugSummary();
            activeEffectBreakdown = effectPool->Build_DebugActiveEffectBreakdown();
        }
    }

    Shared<GameObject> simon = Get_BoundActor("Simon");
    Vec3 simonPosition{};
    const bool hasSimonPosition = Try_GetWorldPosition(simon, simonPosition);

    EffectCom::AttachedEffectDebugSummary totalEffectSummary{};
    std::ostringstream actors;
    bool firstActor = true;

    for (const auto& [targetId, state] : _bindingStates)
    {
        if (state.actor == nullptr)
            continue;

        const EffectCom::AttachedEffectDebugSummary actorEffectSummary =
            Collect_AttachedEffectDebugSummary(state.actor);
        totalEffectSummary.totalCount += actorEffectSummary.totalCount;
        totalEffectSummary.activeCount += actorEffectSummary.activeCount;
        totalEffectSummary.trailCount += actorEffectSummary.trailCount;
        totalEffectSummary.trailDrainingCount += actorEffectSummary.trailDrainingCount;
        totalEffectSummary.sourceHistoryCount += actorEffectSummary.sourceHistoryCount;
        totalEffectSummary.sourceHistoryDrainingCount += actorEffectSummary.sourceHistoryDrainingCount;

        Vec3 actorPosition{};
        const bool hasActorPosition = Try_GetWorldPosition(state.actor, actorPosition);
        const float distanceToSimon =
            hasActorPosition && hasSimonPosition && state.actor != simon
            ? (actorPosition - simonPosition).Length()
            : -1.f;

        const string activeAnimName = state.hasActivePlayAnim ? state.activePlayAnim.animName : "<none>";
        float animTimeSec = -1.f;
        float animDurationSec = -1.f;
        if (Shared<ModelCom> model = Find_TargetModel(state.actor))
        {
            animTimeSec = model->Get_CurrentAnimationTimeSec();
            animDurationSec = model->Get_CurrentAnimationDurationSec();
        }

        if (!firstActor)
            actors << "; ";
        firstActor = false;

        actors
            << targetId
            << "{obj=" << Resolve_ObjectLabel(state.actor)
            << ",anim=" << activeAnimName
            << ",animSec=" << format("{:.3f}/{:.3f}", animTimeSec, animDurationSec)
            << ",pos=";

        if (hasActorPosition)
            actors << format("{:.2f},{:.2f},{:.2f}", actorPosition.x, actorPosition.y, actorPosition.z);
        else
            actors << "n/a";

        if (distanceToSimon >= 0.f)
            actors << ",distSimon=" << format("{:.2f}", distanceToSimon);

        actors
            << ",attached=" << actorEffectSummary.totalCount
            << ",active=" << actorEffectSummary.activeCount
            << ",trail=" << actorEffectSummary.trailCount
            << ",trailDrain=" << actorEffectSummary.trailDrainingCount
            << "}";
    }

    LOG_WARN(
        "[CinematicPerf][Drop] seq='{}' frame={:.2f}/{} dtMs={:.2f} fps={:.1f} nextEvent={} pool(active={}, idle={}, entries={}, activeEffects=[{}]) attached(total={}, active={}, trail={}, trailDrain={}, srcHist={}, srcHistDrain={}) actors=[{}]",
        _sequence.name,
        currentFrameExact,
        _sequence.endFrame,
        timeDelta * 1000.f,
        timeDelta > 0.f ? 1.f / timeDelta : 0.f,
        _nextEventIndex,
        poolSummary.activeCount,
        poolSummary.idleCount,
        poolSummary.entryCount,
        activeEffectBreakdown,
        totalEffectSummary.totalCount,
        totalEffectSummary.activeCount,
        totalEffectSummary.trailCount,
        totalEffectSummary.trailDrainingCount,
        totalEffectSummary.sourceHistoryCount,
        totalEffectSummary.sourceHistoryDrainingCount,
        actors.str()
    );

    LOG_WARN(
        "[CinematicPerf][Stage] seq='{}' frame={:.2f} dtMs={:.2f} runnerMs={:.2f} playerMs={:.2f} cameraMs={:.2f} eventsMs={:.2f} actorTracksMs={:.2f} actorRefreshMs={:.2f} actorApplied={} actorRefresh={} actorDeferred={} animMs={:.2f} animPlayMs={:.2f} animPartMs={:.2f} animActors={} maxAnim={}:{:.2f} attachedMs={:.2f} attachedComs={}",
        _sequence.name,
        currentFrameExact,
        timeDelta * 1000.f,
        perf.runnerMs,
        perf.playerMs,
        perf.cameraMs,
        perf.eventsMs,
        perf.actorTracksMs,
        perf.actorTrackRefreshMs,
        perf.actorTrackAppliedCount,
        perf.actorTrackRefreshCount,
        perf.actorTrackDeferredRefreshCount,
        perf.animMs,
        perf.animPlayMs,
        perf.animPartMs,
        perf.animActorCount,
        perf.maxAnimActorId.empty() ? "<none>" : perf.maxAnimActorId,
        perf.maxAnimActorMs,
        perf.attachedEffectMs,
        perf.attachedEffectComCount
    );
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

        if (state.hasAnimNotifyExecutionOverride)
        {
            if (Shared<ModelCom> model = Find_TargetModel(state.actor))
                model->Set_AnimNotifyExecutionEnabled(state.previousAnimNotifyExecutionEnabled);
        }

        Stop_ActorAttachedEffects(state.actor);
        Restore_PreviewPlaybackOptimization(state);
        Apply_CinematicDrivenState(state.actor, false);

        if (!state.spawnedBySequence)
            continue;

        if (!forceRemoveAllSpawned && state.cleanupPolicy == CinematicCleanupPolicy::Keep)
        {
            continue;
        }

        _sequenceOwnedActors.erase(state.actor.get());
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

void CinematicSequenceRunner::Stop_ActorAttachedEffects(const Shared<GameObject>& actor) const
{
    if (actor == nullptr)
        return;

    if (Shared<EffectCom> effectCom = actor->Get_Component<EffectCom>())
        effectCom->Stop_AllAttachedEffects();

    const Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(actor);
    if (container == nullptr)
        return;

    for (const auto& [partTag, partObject] : container->Get_PartObjects())
    {
        UNREFERENCED_PARAMETER(partTag);

        if (partObject == nullptr)
            continue;

        if (Shared<EffectCom> partEffectCom = partObject->Get_Component<EffectCom>())
            partEffectCom->Stop_AllAttachedEffects();
    }
}

void CinematicSequenceRunner::Track_PreviewShadowCasterRestore(
    BindingRuntimeState& state,
    const Shared<GameObject>& object) const
{
    if (object == nullptr)
        return;

    const auto iter = find_if(
        state.previewShadowCasterRestores.begin(),
        state.previewShadowCasterRestores.end(),
        [&](const BindingRuntimeState::ShadowCasterRestoreState& restore)
        {
            return restore.object == object;
        });

    if (iter != state.previewShadowCasterRestores.end())
        return;

    state.previewShadowCasterRestores.push_back({ object, object->Get_ShadowCasterType() });
    object->Set_ShadowCasterType(ShadowCasterType::None);
}

void CinematicSequenceRunner::Apply_PreviewPlaybackOptimization(BindingRuntimeState& state) const
{
    if (!_usePreviewPlaybackOptimizations || state.actor == nullptr)
        return;

    Track_PreviewShadowCasterRestore(state, state.actor);

    const auto container = dynamic_pointer_cast<ContainerObject>(state.actor);
    if (container == nullptr)
        return;

    for (const auto& [partTag, partObject] : container->Get_PartObjects())
    {
        UNREFERENCED_PARAMETER(partTag);

        Track_PreviewShadowCasterRestore(state, partObject);

        const Shared<Hair_Player> hair = dynamic_pointer_cast<Hair_Player>(partObject);
        if (hair == nullptr)
            continue;

        const auto iter = find_if(
            state.previewHairRestores.begin(),
            state.previewHairRestores.end(),
            [&](const BindingRuntimeState::HairPreviewRestoreState& restore)
            {
                return restore.hair == hair;
            });

        if (iter != state.previewHairRestores.end())
            continue;

        state.previewHairRestores.push_back({ hair, hair->Is_CinematicPreviewSimulationSuppressed() });
        hair->Set_CinematicPreviewSimulationSuppressed(true);
    }
}

void CinematicSequenceRunner::Restore_PreviewPlaybackOptimization(const BindingRuntimeState& state) const
{
    for (const BindingRuntimeState::HairPreviewRestoreState& restore : state.previewHairRestores)
    {
        if (restore.hair)
            restore.hair->Set_CinematicPreviewSimulationSuppressed(restore.previousSimulationSuppressed);
    }

    for (const BindingRuntimeState::ShadowCasterRestoreState& restore : state.previewShadowCasterRestores)
    {
        if (restore.object)
            restore.object->Set_ShadowCasterType(restore.previousType);
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

        if (_suppressAnimNotifyExecutionForBoundActors)
        {
            state.hasAnimNotifyExecutionOverride = true;
            state.previousAnimNotifyExecutionEnabled = model->Is_AnimNotifyExecutionEnabled();
            model->Set_AnimNotifyExecutionEnabled(false);
        }
    }

    Apply_CinematicDrivenState(actor, true);
    Apply_PreviewPlaybackOptimization(state);
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

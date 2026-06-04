#include "BattleTurnCamera.h"
#include <CameraPresetIO.h>

#include "BattleCameraClipIO.h"
#include "BattleCameraClipUtils.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "Character.h"
#include "PartObject.h"
#include "ModelCom.h"
#include "Event_Bus.h"
#include "Battle_Event.h"
#include "Character_BattleBase.h"

namespace
{
BattleCameraClipTimeMode Get_DefaultBattleCameraClipTimeMode(BattleCameraMode cameraMode)
{
    return cameraMode == BattleCameraMode::BATTLEINTRO ||
        cameraMode == BattleCameraMode::SELECTSKILL
        ? BattleCameraClipTimeMode::Independent
        : BattleCameraClipTimeMode::AnimationSync;
}

Shared<ModelCom> Resolve_BattleCameraSubjectModel(const Shared<Character>& character)
{
    if (character == nullptr)
        return nullptr;

    PartObject* body = character->Get_PartObject(L"Part_Body");
    return body ? body->Get_Component<ModelCom>() : nullptr;
}

BattleTurnCamera::BattleShotRequest Build_BattleShotRequestFromEvent(const EvtBattlePresentation& event)
{
    BattleTurnCamera::BattleShotRequest request{};
    request.mode = event.cameraMode;
    request.subject = event.subject;
    request.opponent = event.opponent;
    request.skillId = event.skillId;
    request.presetId = event.presetId;
    request.clipId = event.clipId;
    request.useClip = event.useClip;
    request.restartClip = event.restartClip;
    request.keepCurrentClip = event.keepCurrentClip;

    if (event.cameraMode == BattleCameraMode::SELECTITEM ||
        event.cameraMode == BattleCameraMode::SELECTSKILL)
    {
        request.freezeSubjectDuringTransition = true;
    }

    return request;
}
}

IMPLEMENT_REFLECTION(BattleTurnCamera)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "BattleTurnCamera";
    info.category = "Camera";

    return true;
}

BattleTurnCamera::BattleTurnCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera(device, context)
{
}

BattleTurnCamera::BattleTurnCamera(const BattleTurnCamera& prototype)
    : Camera(prototype)
    , _isFreeAimActive(prototype._isFreeAimActive)
{
}

HRESULT BattleTurnCamera::Initialize_Prototype()
{
    return S_OK;
}

HRESULT BattleTurnCamera::Initialize(void* arg)
{
    CHECK_FAILED(Camera::Initialize(arg), E_FAIL);

    const auto desc = static_cast<BATTLE_TURN_CAMERA_DESC*>(arg);
    if (desc)
    {
        _mouseSensor = desc->mouseSensor;
    }

    Set_Name(L"Battle Turn Camera");

    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::WIDE, _wideModePreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::BATTLEINTRO, _battleIntroPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::PLAYERFOCUS, _playerFocusPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::ENEMYFOCUS, _enemyFocusPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::ENEMYATTACK, _enemyAttackPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::PLAYERATTACK, _playerAttackPreset);

    _enemyAttackClip.duration = 5.f;

   _battleIntroClip.timeMode = Get_DefaultBattleCameraClipTimeMode(BattleCameraMode::BATTLEINTRO);
   _playerAttackClip.timeMode = Get_DefaultBattleCameraClipTimeMode(BattleCameraMode::PLAYERATTACK);
   _enemyAttackClip.timeMode = Get_DefaultBattleCameraClipTimeMode(BattleCameraMode::ENEMYATTACK);

   BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode::BATTLEINTRO, _battleIntroClip);
   BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode::PLAYERATTACK, _playerAttackClip);
   BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode::ENEMYATTACK, _enemyAttackClip);

   _playerAttackDefaultProfile.preset = _playerAttackPreset;
   _playerAttackDefaultProfile.clip = _playerAttackClip;

   for (const string& skillId : CLIENT->Get_AllSkillIds())
   {
       BattleCameraPreset skillPreset = _playerAttackDefaultProfile.preset;
       BattleCameraClip skillClip{};
       BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode::PLAYERATTACK, skillClip, skillId);
       CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::PLAYERATTACK, skillPreset, skillId);

       BattleCameraProfile profile{};
       profile.preset = skillPreset;
       profile.clip = skillClip.duration > 0.f ? skillClip : _playerAttackDefaultProfile.clip;
       _playerAttackProfilesBySkill[skillId] = profile;
   }

   _battlePresentationHandle = GAME->Get_EventBus().Subscribe<EvtBattlePresentation>
    ([this](const EvtBattlePresentation& event)
        {
            Apply_ShotRequest(Build_BattleShotRequestFromEvent(event));
        }
    );

   _battleCameraShakeHandle = GAME->Get_EventBus().Subscribe<EvtBattleCameraShake>([this](const EvtBattleCameraShake& event)
        {
            Start_RuntimeShake(event);
        }
   );

    _currentRequest.mode = BattleCameraMode::WIDE;
    _currentPreset = _wideModePreset;
    _targetPreset = _wideModePreset;

    return S_OK;
}

void BattleTurnCamera::BeginPlay()
{
    Camera::BeginPlay();

    GAME->Add_Camera(GetSharedPtr<Camera>());
}

void BattleTurnCamera::Update(float timeDelta)
{

    if (!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    if (_isLiveEditEnabled)
        Update_LiveEditInput(timeDelta);

    if(_isClipPreview)
        return;

    if (_poseTransition.active)
    {
        Update_PoseTransition(timeDelta);
        return;
    }

    switch (_currentRequest.mode)
    {
    case BattleCameraMode::WIDE:
        {
            Focus_Battle(_heroes, _enemies, timeDelta);
        }
        break;

    case BattleCameraMode::BATTLEINTRO:
    case BattleCameraMode::PLAYERFOCUS:
    case BattleCameraMode::ENEMYFOCUS:
    case BattleCameraMode::PLAYERATTACK:
    case BattleCameraMode::SELECTITEM:
    case BattleCameraMode::SELECTSKILL:
    case BattleCameraMode::PARRY:
        Update_FromCurrentPreset(timeDelta);
        break;
    case BattleCameraMode::ENEMYATTACK:
        Update_EnemyAttackMode(timeDelta);
        break;

    default:
        Update_FromCurrentPreset(timeDelta);
    }

    Update_TransformMatrices();
}

void BattleTurnCamera::Focus_Battle(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies, float timeDelta)
{
    UNREFERENCED_PARAMETER(heroes);
    UNREFERENCED_PARAMETER(enemies);

    if (!_hasWideAnchor)
        _hasWideAnchor = Try_CaptureWideAnchor(_wideAnchorHeroCenter, _wideAnchorYawDegrees);

    EvaluatedPose pose{};
    if (!Evaluate_WidePose(_currentPreset.camera.fovy, pose))
        return;
    Apply_RuntimeShake(pose, timeDelta);
    Apply_EvaluatedPose(pose);
    _smoothedTargetPos = pose.cameraPos;
}

void BattleTurnCamera::Update_FreeAimRotation(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    CHECK_NULL(_transformCom);

    const Vec2 mouseDelta = GAME->Get_MouseDelta();

    _yaw += mouseDelta.x * _mouseSensor;
    _pitch += mouseDelta.y * _mouseSensor;
    _pitch = std::clamp(_pitch, -25.f, 20.f); // 상하 각도 제한
    _yaw = std::clamp(_yaw, -90.f, 90.f);     // 상하 각도 제한

    _transformCom->Set_RotationEuler(Vec3(_pitch, _yaw, 0.f));




    Update_TransformMatrices();
}

Vec3 BattleTurnCamera::Get_AimOrigin() const
{
    CHECK_NULL(_transformCom, Vec3::Zero);

    return _transformCom->Get_WorldPosition();
}

Vec3 BattleTurnCamera::Get_AimDirection() const
{
    CHECK_NULL(_transformCom, Vec3::Zero);

    Vec3 lookDir = _transformCom->Get_WorldForward();
    if (lookDir.LengthSquared() <= 0.0001f)
        return Vec3::Look;

    lookDir.Normalize();
    return lookDir;
}

void BattleTurnCamera::Apply_FreeAimCamera()
{
}

void BattleTurnCamera::Set_Participants(
    const vector<BattleChar>& heroes,
    const vector<BattleChar>& enemies,
    const vector<BattleMonsterSpawnDesc>& enemySpawnDescs)
{
    _heroes = heroes;
    _enemies = enemies;
    _enemySpawnDescs = enemySpawnDescs;
    _hasWideAnchor = Try_CaptureWideAnchor(_wideAnchorHeroCenter, _wideAnchorYawDegrees);
}

void BattleTurnCamera::Reset_RuntimeState()
{
    _currentRequest = {};
    _targetTransform.reset();
    _activeClip = nullptr;
    Reset_ClipPlaybackState();
    _poseTransition = {};
    _smoothedTargetPos = Vec3::Zero;
    _prevTargetPos = Vec3::Zero;
    _enemyPos = Vec3::Zero;
    _playerPos = Vec3::Zero;
    _enemySpawnDescs.clear();
    _hasWideAnchor = false;
    _wideAnchorHeroCenter = Vec3::Zero;
    _wideAnchorYawDegrees = 0.f;
    _isClipPreview = false;
    _isFirstUpdate = true;
    _isWideMode = false;
}

bool BattleTurnCamera::Try_CaptureWideAnchor(Vec3& outHeroCenter, float& outYawDegrees) const
{
    outHeroCenter = Vec3::Zero;
    outYawDegrees = 0.f;

    uint32 heroCount = 0;
    for (const BattleChar& hero : _heroes)
    {
        if (hero.actor == nullptr || hero.actor->Get_Transform() == nullptr)
            continue;

        outHeroCenter += hero.actor->Get_Transform()->Get_WorldPosition();
        ++heroCount;
    }

    if (heroCount == 0)
        return false;

    outHeroCenter /= static_cast<float>(heroCount);

    Shared<GameObject> yawBaseActor = nullptr;
    if (_heroes.size() > 1 && _heroes[1].actor && _heroes[1].actor->Get_Transform())
        yawBaseActor = _heroes[1].actor;
    else if (!_heroes.empty() && _heroes[0].actor && _heroes[0].actor->Get_Transform())
        yawBaseActor = _heroes[0].actor;

    if (yawBaseActor == nullptr)
        return false;

    outYawDegrees = yawBaseActor->Get_Transform()->Get_RotationEuler().y;
    return true;
}

BattleTurnCamera::ResolvedShotAssets BattleTurnCamera::Resolve_ShotAssets(
    const BattleShotRequest& request,
    const BattleCameraPreset* resolvedPresetById,
    const BattleCameraClip* resolvedClipById) const
{
    ResolvedShotAssets resolved{};

    if (resolvedPresetById != nullptr)
    {
        resolved.preset = *resolvedPresetById;
    }
    else if (request.mode == BattleCameraMode::PLAYERATTACK)
    {
        if (!request.skillId.empty())
        {
            auto iter = _playerAttackProfilesBySkill.find(request.skillId);
            resolved.preset = (iter != _playerAttackProfilesBySkill.end())
                ? iter->second.preset
                : _playerAttackDefaultProfile.preset;
        }
        else
        {
            resolved.preset = _playerAttackDefaultProfile.preset;
        }
    }
    else if (const BattleCameraPreset* presetStorage = Find_PresetStorage(request.mode))
    {
        resolved.preset = *presetStorage;
    }

    if (resolvedClipById != nullptr)
    {
        resolved.clip = const_cast<BattleCameraClip*>(resolvedClipById);
    }
    else if (request.mode == BattleCameraMode::PLAYERATTACK)
    {
        if (!request.skillId.empty())
        {
            auto iter = _playerAttackProfilesBySkill.find(request.skillId);
            resolved.clip = (iter != _playerAttackProfilesBySkill.end())
                ? const_cast<BattleCameraClip*>(&iter->second.clip)
                : const_cast<BattleCameraClip*>(&_playerAttackDefaultProfile.clip);
        }
        else
        {
            resolved.clip = const_cast<BattleCameraClip*>(&_playerAttackDefaultProfile.clip);
        }
    }
    else if (request.mode == BattleCameraMode::BATTLEINTRO ||
        request.mode == BattleCameraMode::ENEMYATTACK)
    {
        resolved.clip = const_cast<BattleCameraClip*>(Find_ClipStorage(request.mode));
    }

    return resolved;
}

BattleCameraPreset* BattleTurnCamera::Find_PresetStorage(BattleCameraMode cameraMode)
{
    return const_cast<BattleCameraPreset*>(
        static_cast<const BattleTurnCamera*>(this)->Find_PresetStorage(cameraMode));
}

const BattleCameraPreset* BattleTurnCamera::Find_PresetStorage(BattleCameraMode cameraMode) const
{
    switch (cameraMode)
    {
    case BattleCameraMode::WIDE:
        return &_wideModePreset;

    case BattleCameraMode::BATTLEINTRO:
        return &_battleIntroPreset;

    case BattleCameraMode::PLAYERFOCUS:
        return &_playerFocusPreset;

    case BattleCameraMode::ENEMYFOCUS:
        return &_enemyFocusPreset;

    case BattleCameraMode::ENEMYATTACK:
        return &_enemyAttackPreset;

    case BattleCameraMode::PLAYERATTACK:
        return &_playerAttackPreset;

    default:
        return nullptr;
    }
}

BattleCameraClip* BattleTurnCamera::Find_ClipStorage(BattleCameraMode cameraMode)
{
    return const_cast<BattleCameraClip*>(
        static_cast<const BattleTurnCamera*>(this)->Find_ClipStorage(cameraMode));
}

const BattleCameraClip* BattleTurnCamera::Find_ClipStorage(BattleCameraMode cameraMode) const
{
    switch (cameraMode)
    {
    case BattleCameraMode::WIDE:
        return &_wideModeClip;

    case BattleCameraMode::BATTLEINTRO:
        return &_battleIntroClip;

    case BattleCameraMode::PLAYERFOCUS:
        return &_playerFocusClip;

    case BattleCameraMode::ENEMYFOCUS:
        return &_enemyFocusClip;

    case BattleCameraMode::ENEMYATTACK:
        return &_enemyAttackClip;

    case BattleCameraMode::PLAYERATTACK:
        return &_playerAttackClip;

    default:
        return nullptr;
    }
}

void BattleTurnCamera::Start_RuntimeShake(const EvtBattleCameraShake& event)
{
    _runtimeShake.active = true;
    _runtimeShake.elapsed = 0.f;
    _runtimeShake.duration = event.duration;
    _runtimeShake.amplitude = event.amplitude;
    _runtimeShake.frequencyX = event.frequencyX;
    _runtimeShake.frequencyY = event.frequencyY;
    _runtimeShake.decayRateX = event.decayRateX;
    _runtimeShake.decayRateY = event.decayRateY;
    _runtimeShake.verticalRatio = event.verticalRatio;
    _runtimeShake.strengthScale = event.strengthScale;
}

void BattleTurnCamera::Apply_RuntimeShake(EvaluatedPose& pose, float timeDelta)
{

   if(!_runtimeShake.active)
      return;

    _runtimeShake.elapsed += timeDelta;

    Vec3 forward = pose.lookTarget - pose.cameraPos;

    forward.Normalize();

    Vec3 right = Vec3::Up.Cross(forward);

    right.Normalize();

    Vec3 up = forward.Cross(right);
    up.Normalize();

   const Vec3 shakeOffset = Sample_BattleCameraShakeOffset(_runtimeShake.amplitude,
       _runtimeShake.verticalRatio,
       _runtimeShake.strengthScale,
       _runtimeShake.frequencyX,
       _runtimeShake.frequencyY,
       _runtimeShake.decayRateX,
       _runtimeShake.decayRateY,
       _runtimeShake.elapsed,
       right,
       up);

    pose.cameraPos += shakeOffset;
    pose.lookTarget += shakeOffset;

    if(_runtimeShake.elapsed >= _runtimeShake.duration)
        _runtimeShake.active = false;
}

const BattleCameraPreset* BattleTurnCamera::Resolve_BattleCameraPresetById(BattleCameraMode mode, const string& presetId) const
{
    //if (presetId.empty())
    //    return nullptr;
    const string cacheKey = string(magic_enum::enum_name(mode)) + ":" + presetId;
    if (auto iter = _presetsById.find(cacheKey); iter != _presetsById.end())
        return &iter->second;

    BattleCameraPreset preset{};
    if (!CameraPresetIO::LoadBattlePresetFromFile(mode, preset, presetId))
        return nullptr;

    _presetsById[cacheKey] = preset;
    return &_presetsById[cacheKey];
}

const BattleCameraClip* BattleTurnCamera::Resolve_BattleCameraClipById(BattleCameraMode mode, const string& clipId) const
{
    //if (clipId.empty())
    //    return nullptr;

    const string cacheKey = string(magic_enum::enum_name(mode)) + ":" + clipId;
    if (auto iter = _clipsById.find(cacheKey); iter != _clipsById.end())
        return &iter->second;

    BattleCameraClip clip{};
    if (!BattleCameraClipIO::LoadBattleCameraClipFromFile(mode, clip, clipId))
    {
        return nullptr;
    }

    _clipsById[cacheKey] = clip;
    return &_clipsById[cacheKey];
}


void BattleTurnCamera::Apply_LookAt(const Vec3& cameraPos, const Vec3& lookAt)
{
    _transformCom->Set_WorldPosition(cameraPos);
    _transformCom->LookAt(lookAt);

    Update_TransformMatrices();
}

void BattleTurnCamera::Apply_EvaluatedPose(const EvaluatedPose& pose)
{
    Set_FovY(pose.fovY);
    Apply_LookAt(pose.cameraPos, pose.lookTarget);
    _transformCom->Add_LocalRotationQuaternion(Quat::CreateFromAxisAngle(Vec3::Forward, pose.rollOffset));
    Update_TransformMatrices();
}

BattleTurnCamera::EvaluatedPose BattleTurnCamera::Capture_CurrentPose()
{
    EvaluatedPose pose{};
    pose.fovY = Get_FovY();

    if (_transformCom == nullptr)
        return pose;

    const Vec3 cameraPos = _transformCom->Get_WorldPosition();
    Vec3 forward = _transformCom->Get_WorldForward();
    if (forward.LengthSquared() <= 0.0001f)
        forward = Vec3::Look;
    else
        forward.Normalize();

    pose.cameraPos = cameraPos;
    pose.lookTarget = cameraPos + forward * 10.f;
    pose.rollOffset = 0.f;
    return pose;
}

bool BattleTurnCamera::Evaluate_WidePose(float fovY, EvaluatedPose& outPose) const
{
    auto selectDominantOffset = [](float current, float candidate)
    {
        return fabsf(candidate) > fabsf(current) ? candidate : current;
    };

    float wideBackOffset = 0.f;
    float wideHeightOffset = 0.f;
    float wideLookHeightOffset = 0.f;

    for (const BattleMonsterSpawnDesc& spawnDesc : _enemySpawnDescs)
    {
        wideBackOffset = selectDominantOffset(wideBackOffset, spawnDesc.wideBackOffset);
        wideHeightOffset = selectDominantOffset(wideHeightOffset, spawnDesc.wideHeightOffset);
        wideLookHeightOffset = selectDominantOffset(wideLookHeightOffset, spawnDesc.wideLookHeightOffset);
    }

    Vec3 heroCenter = _wideAnchorHeroCenter;
    float yawDegrees = _wideAnchorYawDegrees;

    if (!_hasWideAnchor && !Try_CaptureWideAnchor(heroCenter, yawDegrees))
        return false;

    return Evaluate_BattleTurnWidePose(
        heroCenter,
        yawDegrees,
        fovY,
        wideBackOffset,
        wideHeightOffset,
        wideLookHeightOffset,
        outPose);
}

bool BattleTurnCamera::Evaluate_ShotRequestPose(
    const BattleShotRequest& request,
    const BattleCameraPreset& preset,
    const Vec3* smoothedTargetPos,
    const BattleCameraClipSample& clipSample,
    float clipTime,
    EvaluatedPose& outPose) const
{
    switch (request.mode)
    {
    case BattleCameraMode::WIDE:
        return Evaluate_WidePose(preset.camera.fovy, outPose);
    case BattleCameraMode::ENEMYATTACK:
    {
        if (request.subject == nullptr || request.opponent == nullptr)
            return false;

        Vec3 subjectPos = _enemyPos;
        Vec3 opponentPos = _playerPos;
        if (subjectPos.LengthSquared() <= 0.0001f)
            subjectPos = request.subject->Get_Transform()->Get_WorldPosition();
        if (opponentPos.LengthSquared() <= 0.0001f)
            opponentPos = request.opponent->Get_Transform()->Get_WorldPosition();
        return Evaluate_BattleTurnEnemyAttackPose(
            subjectPos,
            opponentPos,
            preset,
            clipSample,
            clipTime,
            outPose);
    }
    default:
        if (request.subject == nullptr)
            return false;

        const Vec3 subjectPos = request.subject->Get_Transform()->Get_WorldPosition();
        return Evaluate_BattleTurnPresetPose(
            subjectPos,
            preset,
            clipSample,
            _yaw,
            _pitch,
            smoothedTargetPos,
            clipTime,
            outPose);
    }
}

bool BattleTurnCamera::Should_StartPoseTransition(const BattleShotRequest& request) const
{
    if (_transformCom == nullptr)
        return false;

    if (request.subject != nullptr)
        return true;

    return _currentRequest.mode == BattleCameraMode::WIDE && _isWideMode;
}

bool BattleTurnCamera::Start_PoseTransition(
    const BattleShotRequest& request,
    const BattleCameraPreset& preset,
    BattleCameraClip* activeClip,
    const Vec3& targetSmoothedTargetPos,
    const Vec3& targetPrevTargetPos)
{
    EvaluatedPose fromPose = Capture_CurrentPose();

    _poseTransition.hasFrozenPos = false;
    if (request.freezeSubjectDuringTransition == true &&
        request.subject != nullptr &&
        request.subject->Get_Transform() != nullptr)
    {
        _poseTransition.frozenSubjectPos = request.subject->Get_Transform()->Get_WorldPosition();
        _poseTransition.hasFrozenPos = true;
    }

    EvaluatedPose toPose{};
    const bool shouldUseFrozenSelectItemPose =
        request.freezeSubjectDuringTransition &&
        request.mode == BattleCameraMode::SELECTITEM &&
        _poseTransition.hasFrozenPos;

    if (shouldUseFrozenSelectItemPose)
    {
        if (!Evaluate_CurrentPresetPose(
            _poseTransition.frozenSubjectPos,
            preset,
            BattleCameraClipSample{},
            _yaw,
            _pitch,
            &targetSmoothedTargetPos,
            toPose))
        {
            return false;
        }
    }
    else if (!Evaluate_ShotRequestPose(request, preset, &targetSmoothedTargetPos, BattleCameraClipSample{}, 0.f, toPose))
    {
        return false;
    }

    _activeClip = activeClip;
    Reset_ClipPlaybackState();

    _poseTransition.active = true;
        _poseTransition.elapsed = 0.f;
    _poseTransition.duration = preset.transitionDuration;
    _poseTransition.easing = preset.transitionEasing;
    _poseTransition.fromPose = fromPose;
    _poseTransition.toPose = toPose;
    _poseTransition.targetRequest = request;
    _poseTransition.targetPreset = preset;
    _poseTransition.targetActiveClip = activeClip;
    _poseTransition.targetSmoothedTargetPos = targetSmoothedTargetPos;
    _poseTransition.targetPrevTargetPos = targetPrevTargetPos;
    _isClipPreview = false;

    return true;
}

void BattleTurnCamera::Update_PoseTransition(float timeDelta)
{
    const float clampedDelta = std::max(0.f, timeDelta);
    const BattleCameraClipSample clipSample = Advance_ActiveClip(clampedDelta, _poseTransition.targetRequest);
    _poseTransition.elapsed += clampedDelta;
    const float duration = std::max(0.0001f, _poseTransition.duration);
    const float t = std::clamp(_poseTransition.elapsed / duration, 0.f, 1.f);

    float  easedT = Evaluate_CameraTransitionT(_poseTransition.easing, t);
    EvaluatedPose blended{};

    blended.cameraPos = Vec3::Lerp(_poseTransition.fromPose.cameraPos, _poseTransition.toPose.cameraPos, easedT);
    blended.lookTarget = Vec3::Lerp(_poseTransition.fromPose.lookTarget, _poseTransition.toPose.lookTarget, easedT);
    blended.fovY = lerp(_poseTransition.fromPose.fovY, _poseTransition.toPose.fovY, easedT);
    blended.rollOffset = lerp(_poseTransition.fromPose.rollOffset, _poseTransition.toPose.rollOffset, easedT);
    EvaluatedPose clippedTargetPose{};
    bool hasClippedTargetPose = false;
    if (_poseTransition.targetRequest.freezeSubjectDuringTransition &&
        _poseTransition.targetRequest.mode == BattleCameraMode::SELECTITEM &&
        _poseTransition.hasFrozenPos)
    {
        hasClippedTargetPose = Evaluate_CurrentPresetPose(
            _poseTransition.frozenSubjectPos,
            _poseTransition.targetPreset,
            clipSample,
            _yaw,
            _pitch,
            &_poseTransition.targetSmoothedTargetPos,
            clippedTargetPose);
    }
    else
    {
        hasClippedTargetPose = Evaluate_ShotRequestPose(
            _poseTransition.targetRequest,
            _poseTransition.targetPreset,
            &_poseTransition.targetSmoothedTargetPos,
            clipSample,
            _clipTime,
            clippedTargetPose);
    }

    if (hasClippedTargetPose)
    {
        float targetCorrectionWeight  = 0.05f;

        blended.cameraPos += (clippedTargetPose.cameraPos - _poseTransition.toPose.cameraPos) * targetCorrectionWeight;
        blended.lookTarget += (clippedTargetPose.lookTarget - _poseTransition.toPose.lookTarget) * targetCorrectionWeight;
        blended.fovY += (clippedTargetPose.fovY - _poseTransition.toPose.fovY) * targetCorrectionWeight;
        blended.rollOffset += (clippedTargetPose.rollOffset - _poseTransition.toPose.rollOffset) * targetCorrectionWeight;
    }

    Apply_RuntimeShake(blended, timeDelta);
    Apply_EvaluatedPose(blended);

    if (t < 1.f)
        return;

    _poseTransition.active = false;
    _currentRequest = _poseTransition.targetRequest;
    _currentPreset = _poseTransition.targetPreset;
    _targetPreset = _poseTransition.targetPreset;
    _targetTransform = _currentRequest.subject ? _currentRequest.subject->Get_Transform() : nullptr;
    _smoothedTargetPos = _poseTransition.targetSmoothedTargetPos;
    _prevTargetPos = _poseTransition.targetPrevTargetPos;
    _isFirstUpdate = true;
}

bool BattleTurnCamera::Evaluate_CurrentPresetPose(
    const Vec3& targetPos,
    const BattleCameraPreset& preset,
    const BattleCameraClipSample& clipSample,
    float yawBase,
    float pitchBase,
    const Vec3* smoothedTargetPos,
    EvaluatedPose& outPose) const
{
    return Evaluate_BattleTurnPresetPose(
        targetPos,
        preset,
        clipSample,
        yawBase,
        pitchBase,
        smoothedTargetPos,
        _clipTime,
        outPose);
}

bool BattleTurnCamera::Evaluate_EnemyAttackPose(
    const Vec3& subjectPos,
    const Vec3& opponentPos,
    const BattleCameraPreset& preset,
    const BattleCameraClipSample& clipSample,
    EvaluatedPose& outPose) const
{
    return Evaluate_BattleTurnEnemyAttackPose(
        subjectPos,
        opponentPos,
        preset,
        clipSample,
        _clipTime,
        outPose);
}

void BattleTurnCamera::Apply_ShotRequest(const BattleShotRequest& request)
{
    const bool isSameRequest =
        _currentRequest.mode == request.mode &&
        _currentRequest.subject == request.subject &&
        _currentRequest.opponent == request.opponent &&
        _currentRequest.skillId == request.skillId &&
        _currentRequest.presetId == request.presetId &&
        _currentRequest.clipId == request.clipId &&
        _currentRequest.useClip == request.useClip;

    const bool isSamePreset = _currentRequest.presetId == request.presetId && _currentRequest.mode == request.mode;
    const bool isSameClip =
        _currentRequest.clipId == request.clipId &&
        _currentRequest.useClip == request.useClip &&
        _currentRequest.mode == request.mode;

    const bool shouldKeepCurrentClip = request.useClip == false && request.keepCurrentClip;
    const bool shouldDisableClip = request.useClip == false && !request.keepCurrentClip;
    const bool shouldRestartClip = !shouldKeepCurrentClip && (request.restartClip || !isSameClip);

    if (isSameRequest &&
        request.restartClip == false &&
        (!request.useClip || _activeClip != nullptr))
    {
        return;
    }

    bool wasWide = _currentRequest.mode == BattleCameraMode::WIDE;
    bool willWide = request.mode == BattleCameraMode::WIDE;
    const bool wasAim = _currentRequest.mode == BattleCameraMode::AIM;
    const bool willAim = request.mode == BattleCameraMode::AIM;

    if(wasWide && !willWide)
        _isWideMode = false;

    if (wasAim && !willAim)
    {
        // Free aim rotates the runtime yaw/pitch bases directly, so reset them
        // before resolving the next authored preset pose.
        _yaw = 0.f;
        _pitch = 0.f;
    }
    const BattleCameraPreset* resolvedPresetById;
    const BattleCameraClip* resolvedClipById;

    resolvedPresetById = Resolve_BattleCameraPresetById(request.mode, request.presetId);
    resolvedClipById = Resolve_BattleCameraClipById(request.mode, request.clipId);

    if (!request.presetId.empty() && resolvedPresetById == nullptr)
        return;

    ResolvedShotAssets resolvedAssets = Resolve_ShotAssets(request, resolvedPresetById, resolvedClipById);
    BattleCameraClip* nextActiveClip = resolvedAssets.clip;

    BattleCameraPreset nextPreset = resolvedAssets.preset;

    if (shouldDisableClip)
        nextActiveClip = nullptr;
    else if (shouldKeepCurrentClip)
        nextActiveClip = _activeClip;

    Vec3 targetSmoothedTargetPos = _smoothedTargetPos;
    Vec3 targetPrevTargetPos = _prevTargetPos;
    Shared<TransformCom> nextTargetTransform = request.subject ? request.subject->Get_Transform() : nullptr;
    Vec3 nextEnemyAttackSubjectPos = _enemyPos;
    Vec3 nextEnemyAttackOpponentPos = _playerPos;

    const bool shouldReAnchored =
        _currentRequest.subject != request.subject ||
        _currentRequest.opponent != request.opponent ||
        _currentRequest.mode != request.mode;

    if (request.subject && (nextPreset.snapOnEnter || shouldReAnchored)) //|| _currentRequest.subject == nullptr))
    {
        const Vec3 targetPos = request.subject->Get_Transform()->Get_WorldPosition();
        targetSmoothedTargetPos = targetPos;
        targetPrevTargetPos = targetPos;
    }

    if (request.mode == BattleCameraMode::ENEMYATTACK)
    {
        const bool shouldCaptureEnemyAttackAnchor =
            nextPreset.snapOnEnter ||
            shouldReAnchored ||
            nextEnemyAttackSubjectPos.LengthSquared() <= 0.0001f ||
            nextEnemyAttackOpponentPos.LengthSquared() <= 0.0001f;

        if (shouldCaptureEnemyAttackAnchor)
        {
            if (request.subject != nullptr && request.subject->Get_Transform() != nullptr)
                nextEnemyAttackSubjectPos = request.subject->Get_Transform()->Get_WorldPosition();

            if (request.opponent != nullptr && request.opponent->Get_Transform() != nullptr)
                nextEnemyAttackOpponentPos = request.opponent->Get_Transform()->Get_WorldPosition();
        }
    }
    else
    {
        nextEnemyAttackSubjectPos = Vec3::Zero;
        nextEnemyAttackOpponentPos = Vec3::Zero;
    }

    _enemyPos = nextEnemyAttackSubjectPos;
    _playerPos = nextEnemyAttackOpponentPos;

    if (nextPreset.snapOnEnter || request.mode == BattleCameraMode::WIDE)
    {
        _poseTransition.active = false;
        _activeClip = nextActiveClip;
        Reset_ClipPlaybackState();
        _isClipPreview = false;
        _isFirstUpdate = true;
        _currentRequest = request;
        _targetTransform = nextTargetTransform;
        _targetPreset = nextPreset;
        _currentPreset = _targetPreset;
        _smoothedTargetPos = targetSmoothedTargetPos;
        _prevTargetPos = targetPrevTargetPos;

        EvaluatedPose toPose {};

        if(Evaluate_ShotRequestPose(_currentRequest, _currentPreset, &_smoothedTargetPos, BattleCameraClipSample{}, _clipTime, toPose ))
        Apply_EvaluatedPose(toPose);

        return;
    }

    if ((Should_StartPoseTransition(request)&&!isSamePreset)|| shouldReAnchored)
    {
        if (Start_PoseTransition(request, nextPreset, nextActiveClip, targetSmoothedTargetPos, targetPrevTargetPos))
            return;
    }

    _poseTransition.active = false;

    if(shouldRestartClip)
    {
        _activeClip = nextActiveClip;
        Reset_ClipPlaybackState();
    }
    else if (shouldKeepCurrentClip)
    {
    }
    else if(shouldDisableClip)
    {
        _activeClip = nullptr;
        Reset_ClipPlaybackState();
    }

    _isClipPreview = false;
    _isFirstUpdate = true;
    _currentRequest = request;
    _targetTransform = nextTargetTransform;
    _targetPreset = nextPreset;
    _currentPreset = _targetPreset;
    _smoothedTargetPos = targetSmoothedTargetPos;
    _prevTargetPos = targetPrevTargetPos;
}

void BattleTurnCamera::Begin_LiveEdit(BattleCameraMode mode, const Shared<Character>& subject, const Shared<Character>& opponent)
{
    _isLiveEditEnabled = true;
    Update_LiveEditContext(mode, subject, opponent);
}

void BattleTurnCamera::Update_LiveEditContext(BattleCameraMode mode, const Shared<Character>& target, const Shared<Character>& opponent)
{
    BattleShotRequest request{};
    request.mode = mode;
    request.subject = target;
    request.opponent = opponent;
    Apply_ShotRequest(request);

    if (BattleCameraPreset* preset = Get_MutablePreset(mode))
    {
        _currentPreset = *preset;
        _targetPreset = *preset;
    }
}

void BattleTurnCamera::End_LiveEdit()
{
    _isLiveEditEnabled = false;
}

void BattleTurnCamera::Update_EnemyAttackMode(float timeDelta)
{
    if (_currentRequest.subject == nullptr || _currentRequest.opponent == nullptr)
    {
        Update_FromCurrentPreset(timeDelta);
        return;
    }

    const BattleCameraClipSample clip = Advance_ActiveClip(timeDelta, _currentRequest);
    Vec3 enemyPos = _enemyPos;
    Vec3 playerPos = _playerPos;
    if (enemyPos.LengthSquared() <= 0.0001f)
        enemyPos = _currentRequest.subject->Get_Transform()->Get_WorldPosition();
    if (playerPos.LengthSquared() <= 0.0001f)
        playerPos = _currentRequest.opponent->Get_Transform()->Get_WorldPosition();

    EvaluatedPose pose{};
    if (!Evaluate_EnemyAttackPose(enemyPos, playerPos, _currentPreset, clip, pose))
        return;
    Apply_RuntimeShake(pose,timeDelta);
    Apply_EvaluatedPose(pose);
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));

    _smoothedTargetPos = enemyPos;
    _prevTargetPos = enemyPos;
}

void BattleTurnCamera::Update_FromCurrentPreset(float timeDelta)
{
    if(_currentRequest.subject == nullptr)
        return;

    // Clip이 있을 시 적용
    const BattleCameraClipSample clip  = Advance_ActiveClip(timeDelta, _currentRequest);

    //if (_currentRequest.mode != BattleCameraMode::PLAYERATTACK)// && _currentRequest.mode != BattleCameraMode::SELECTSKILL)
    if(_currentPreset.snapOnEnter == false)
        _smoothedTargetPos = _currentRequest.subject->Get_Transform()->Get_WorldPosition();
    
    EvaluatedPose pose{};
    Evaluate_CurrentPresetPose(_smoothedTargetPos, _currentPreset, clip, _yaw, _pitch, &_smoothedTargetPos, pose);
    Apply_RuntimeShake(pose,timeDelta);
    Apply_EvaluatedPose(pose);
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));

    _prevTargetPos = _smoothedTargetPos;
}

void BattleTurnCamera::Update_LiveEditInput(float timeDelta)
{
    if (!GAME->KeyPress(KEY_TYPE::RBUTTON))
        return;

    BattleCameraPreset* preset = Get_MutablePreset(_currentRequest.mode);
    if (preset == nullptr)
        return;

    const float wheelDelta = GAME->Get_MouseWheel();
    if (wheelDelta != 0.f)
    {
        const float wheelStep = 0.4f;
        preset->distance += (wheelDelta > 0.f) ? -wheelStep : wheelStep;
    }

    const float moveStep = 3.f * timeDelta;

    if (GAME->KeyPress(KEY_TYPE::W))
        preset->distance -= moveStep;

    if (GAME->KeyPress(KEY_TYPE::S))
        preset->distance += moveStep;

    if (GAME->KeyPress(KEY_TYPE::A))
        preset->aimOffset -= moveStep;

    if (GAME->KeyPress(KEY_TYPE::D))
        preset->aimOffset += moveStep;

    if (GAME->KeyPress(KEY_TYPE::E))
        preset->pivotHeight += moveStep;

    if (GAME->KeyPress(KEY_TYPE::Q))
        preset->pivotHeight -= moveStep;

    preset->distance = std::max(0.1f, preset->distance);

    const Vec2 mouseDelta = GAME->Get_MouseDelta();
    preset->yawOffset += mouseDelta.x * preset->camera.mouseSensor;
    preset->pitchOffset += mouseDelta.y * preset->camera.mouseSensor * preset->camera.mouseSensitivity;
    preset->pitchOffset = std::clamp(preset->pitchOffset, preset->pitchMin, preset->pitchMax);

    _currentPreset = *preset;
    _targetPreset = *preset;
}

const BattleCameraPreset& BattleTurnCamera::Get_Preset(BattleCameraMode mode) const
{
    if (const BattleCameraPreset* preset = Find_PresetStorage(mode))
        return *preset;

    return _wideModePreset;
}

void BattleTurnCamera::Set_Preset(BattleCameraMode mode, const BattleCameraPreset& preset)
{
    BattleCameraPreset* targetPreset = Find_PresetStorage(mode);
    if (targetPreset == nullptr)
        return;

    *targetPreset = preset;
    if (mode == BattleCameraMode::PLAYERATTACK)
        _playerAttackDefaultProfile.preset = preset;
}

void BattleTurnCamera::Set_Clip(BattleCameraMode mode, const BattleCameraClip& clip, const string& clipId)
{
    if (BattleCameraClip* targetClip = Get_MutableClip(mode))
    {
        *targetClip = clip;

        if (mode == BattleCameraMode::PLAYERATTACK)
            _playerAttackDefaultProfile.clip = clip;
    }

    if (!clipId.empty())
    {
        const string cacheKey = string(magic_enum::enum_name(mode)) + ":" + clipId;
        _clipsById[cacheKey] = clip;
    }
}

void BattleTurnCamera::Apply_PreviewPreset(const BattleCameraPreset& preset)
{
    _currentPreset = preset;
    _targetPreset = preset;
}

void BattleTurnCamera::Preview_ClipAtTime(const BattleShotRequest& request, float time)
{
    if (_currentRequest.mode != request.mode ||
        _currentRequest.subject != request.subject ||
        _currentRequest.opponent != request.opponent ||
        _currentRequest.skillId != request.skillId ||
        _currentRequest.presetId != request.presetId ||
        _currentRequest.clipId != request.clipId)
        Apply_ShotRequest(request);
    else
    {
        _currentRequest = request;
        _targetTransform = _currentRequest.subject ? _currentRequest.subject->Get_Transform() : nullptr;
    }

    switch (_currentRequest.mode)
    {
    case BattleCameraMode::ENEMYATTACK:
        Preview_EnemyAttackAtTime(time);
        break;

    default:
        Preview_CurrentPresetAtTime(time);
        break;
    }
}

BattleCameraPreset* BattleTurnCamera::Get_MutablePreset(BattleCameraMode cameraMode)
{
    return Find_PresetStorage(cameraMode);
}

BattleCameraClip* BattleTurnCamera::Get_MutableClip(BattleCameraMode cameraMode)
{
    return Find_ClipStorage(cameraMode);
}

void BattleTurnCamera::Preview_CurrentPresetAtTime(float time)
{
    if (_currentRequest.subject == nullptr)
        return;

    BattleCameraClip* clip = Get_MutableClip(_currentRequest.mode);
    const BattleCameraClipSample clipSample = clip ? Sample_BattleCameraClip(*clip, clamp(time, 0.f, clip->duration)) : BattleCameraClipSample {};

    const Vec3 targetPos = _currentRequest.subject->Get_Transform()->Get_WorldPosition();

    EvaluatedPose pose{};
    Evaluate_CurrentPresetPose(targetPos, _currentPreset, clipSample, _yaw, _pitch, nullptr, pose);
    Apply_EvaluatedPose(pose);
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));
}

void BattleTurnCamera::Preview_EnemyAttackAtTime(float time)
{
    if (_currentRequest.subject == nullptr || _currentRequest.opponent == nullptr)
        return;

    BattleCameraClip* clip = Get_MutableClip(_currentRequest.mode);
    const BattleCameraClipSample clipSample = clip ? Sample_BattleCameraClip(*clip, clamp(time, 0.f, clip->duration)) : BattleCameraClipSample {};

    const Vec3 enemyPos = _currentRequest.subject->Get_Transform()->Get_WorldPosition();
    const Vec3 playerPos = _currentRequest.opponent->Get_Transform()->Get_WorldPosition();

    EvaluatedPose pose{};
    if (!Evaluate_EnemyAttackPose(enemyPos, playerPos, _currentPreset, clipSample, pose))
        return;

    Apply_EvaluatedPose(pose);
}

float BattleTurnCamera::Evaluate_CameraTransitionT(BattleCameraTransitionEasing easing, float t)
{
    t = clamp(t, 0.f, 1.f);

    switch (easing)
    {
    case BattleCameraTransitionEasing::Linear:
        return t;

    case BattleCameraTransitionEasing::EaseIn:
        return t * t;

    case BattleCameraTransitionEasing::EaseOut:
        return 1.f - (1.f - t) * (1.f - t);

    case BattleCameraTransitionEasing::EaseInOut:
        return t * t * (3.f - 2.f * t);

    default:
        return t;
    }
}

BattleCameraClipSample BattleTurnCamera::Advance_ActiveClip(float timeDelta, const BattleShotRequest& request)
{
    BattleCameraClipSample sample{};

    if(_activeClip == nullptr)
        return sample;

    if (_activeClip->timeMode == BattleCameraClipTimeMode::AnimationSync)
    {
        Shared<ModelCom> model = Resolve_BattleCameraSubjectModel(request.subject);
        if (model == nullptr)
            return sample;

        const char* currentAnimationNamePtr = model->Get_AnimationName(model->Get_CurrentAnimationIndex());
        const string currentAnimationName = currentAnimationNamePtr ? currentAnimationNamePtr : "";
        const float currentAnimationTimeSec = std::max(0.f, model->Get_CurrentAnimationTimeSec());
        const float currentAnimationDurationSec = std::max(0.f, model->Get_CurrentAnimationDurationSec());
        const bool hasCapturedSyncAnimation =
            !_clipSyncAnimationName.empty() ||
            _clipSyncAnimationDuration > 0.f;

        if (!hasCapturedSyncAnimation)
        {
            _clipSyncAnimationName = currentAnimationName;
            _clipSyncAnimationDuration = currentAnimationDurationSec;
            _clipSyncBaseTime = currentAnimationTimeSec;
        }
        else if (!_clipSyncAnimationName.empty() && currentAnimationName != _clipSyncAnimationName)
        {
            _activeClip = nullptr;
            Reset_ClipPlaybackState();
            return sample;
        }

        if (currentAnimationTimeSec + 0.0001f < _clipSyncPrevAnimationTime)
        {
            _activeClip = nullptr;
            Reset_ClipPlaybackState();
            return sample;
        }

        const float syncDurationSec = _clipSyncAnimationDuration > 0.f
            ? _clipSyncAnimationDuration
            : currentAnimationDurationSec;
        const float sampleTime = syncDurationSec > 0.f
            ? clamp(currentAnimationTimeSec / syncDurationSec, 0.f, 1.f)
            : 0.f;

        _clipTime = sampleTime;
        _clipSyncPrevAnimationTime = currentAnimationTimeSec;

        sample = Sample_BattleCameraClip(*_activeClip, sampleTime);

        if (sampleTime >= 1.f)
        {
            _activeClip = nullptr;
            Reset_ClipPlaybackState();
        }

        return sample;
    }

    _clipTime += timeDelta;
    float sampleTime = clamp(_clipTime, 0.f, _activeClip->duration);
    _clipTime = sampleTime;

    sample = Sample_BattleCameraClip(*_activeClip, sampleTime);

    if(sampleTime >= _activeClip->duration)
    {
        _clipTime = _activeClip->duration;
        _activeClip = nullptr;
    }

    return sample;
}

void BattleTurnCamera::Reset_ClipPlaybackState()
{
    _clipTime = 0.f;
    _clipSyncBaseTime = 0.f;
    _clipSyncPrevAnimationTime = 0.f;
    _clipSyncAnimationDuration = 0.f;
    _clipSyncAnimationName.clear();
    _clipDebugSampleLogCount = 0;
    _clipDebugLoggedShake = false;
}

Shared<BattleTurnCamera> BattleTurnCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleTurnCamera>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> BattleTurnCamera::Clone(void* arg)
{
    auto instance = make_shared<BattleTurnCamera>(*this);

    if (FAILED(instance->Initialize(arg)))
        return nullptr;

    return instance;
}

void BattleTurnCamera::Free()
{
    if (_battlePresentationHandle.IsValid())
    {
        GAME->Get_EventBus().Unsubscribe(_battlePresentationHandle);
        _battlePresentationHandle = {};
    }

    if (_battleCameraShakeHandle.IsValid())
    {
        GAME->Get_EventBus().Unsubscribe(_battleCameraShakeHandle);
        _battleCameraShakeHandle = {};

    }

    Camera::Free();
}

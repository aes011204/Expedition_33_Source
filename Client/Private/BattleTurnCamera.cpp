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
    return cameraMode == BattleCameraMode::BATTLEINTRO
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
            BattleShotRequest request;

            switch(event.cameraMode)
            {
                case BattleCameraMode::ENEMYFOCUS:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    request.presetId = event.presetId;
                    request.clipId = event.clipId;
                    break;

                case BattleCameraMode::PLAYERFOCUS:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    request.presetId = event.presetId;
                    request.clipId = event.clipId;
                    break;

                case BattleCameraMode::WIDE:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    request.presetId = event.presetId;
                    request.clipId = event.clipId;
                    break;
                        
                case BattleCameraMode::BATTLEINTRO:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    request.presetId = event.presetId;
                    request.clipId = event.clipId;
                    break;

                case BattleCameraMode::ENEMYATTACK:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    request.opponent = event.opponent;
                    request.presetId = event.presetId;
                    request.clipId = event.clipId;
                    break;

                case BattleCameraMode::PLAYERATTACK:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    request.skillId = event.skillId;
                    request.presetId = event.presetId;
                    request.clipId = event.clipId;
                    break;

                case BattleCameraMode::SELECTITEM:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    request.skillId = event.skillId;
                    request.presetId = event.presetId;
                    request.clipId = event.clipId;
                    break;

                case BattleCameraMode::SELECTSKILL:
                    request.mode = event.cameraMode;
                    request.subject = event.subject;
                    request.skillId = event.skillId;
                    request.presetId = event.presetId;
                    request.clipId = event.clipId;
                    break;
            }

            request.useClip = event.useClip;
            request.restartClip = event.restartClip;

            Apply_ShotRequest(request);
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
    LOG_DEBUG("Battle Camera Mode = {} ", magic_enum::enum_name(_currentRequest.mode));

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
         if(!_isWideMode)
         {
            Focus_Battle(_heroes, _enemies);
             _isWideMode = true;
         }
        }
        break;
    case BattleCameraMode::BATTLEINTRO:
        Update_FromCurrentPreset(timeDelta);
        break;
    case BattleCameraMode::PLAYERFOCUS:
        Update_PlayerFocusMode(timeDelta);
        break;
    case BattleCameraMode::ENEMYFOCUS:
        Update_EnemyFocusMode(timeDelta);
        break;
    case BattleCameraMode::ENEMYATTACK:
        Update_EnemyAttackMode(timeDelta);
        break;
    case BattleCameraMode::PLAYERATTACK:
        Update_PlayerAttackMode(timeDelta);
        break;

    default:
        Update_FromCurrentPreset(timeDelta);
    }

    Update_TransformMatrices();
}

void BattleTurnCamera::Focus_Battle(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies)
{
    // 위치는 일단 임시
    Vec3 heroCenter = Vec3::Zero;
    uint32 heroCount = 0;

    for (size_t i = 0; i < heroes.size(); ++i)
    {
        if (heroes[i].actor && heroes[i].actor->Get_Transform())
        {
            // 플레이어 3명 위치 누적
            heroCenter += heroes[i].actor->Get_Transform()->Get_WorldPosition();
            ++heroCount;
        }
    }

    if (heroCount == 0)
        return;

    heroCenter /= static_cast<float>(heroCount);

    // 시작 위치 기준점으로 잡을 플레이어
    Shared<GameObject> yawBaseActor = nullptr;

    if (heroes.size() > 1 && heroes[1].actor && heroes[1].actor->Get_Transform())
        yawBaseActor = heroes[1].actor; 
    else if (!heroes.empty() && heroes[0].actor && heroes[0].actor->Get_Transform())
        yawBaseActor = heroes[0].actor; // hero[1]이 없으면 hero[0] fallback

    CHECK_NULL(yawBaseActor);

    const Vec3 baseRot = yawBaseActor->Get_Transform()->Get_RotationEuler();
    const float cameraYawDeg = baseRot.y;
    const float cameraYawRad = XMConvertToRadians(cameraYawDeg);

    Vec3 forwardDir{};
    forwardDir.x = sinf(cameraYawRad);
    forwardDir.y = 0.f;
    forwardDir.z = cosf(cameraYawRad);
    forwardDir.Normalize();

    // 카메라 위치는 플레이어 뒤쪽
    Vec3 backDir = -forwardDir;
    Vec3 rightDir{};
    rightDir.x = cosf(cameraYawRad); // 카메라를 정면 중앙보다 약간 옆으로 빼기 위한 right 방향
    rightDir.y = 0.f;
    rightDir.z = -sinf(cameraYawRad);
    rightDir.Normalize();

    const Vec3 lookAt = heroCenter + Vec3(0.f, 2.2f, 0.f);

    const Vec3 cameraPos =
        heroCenter +
        backDir * 8.5f +          
        rightDir * 2.0f +         
        Vec3(2.f, 3.f, -2.f);     // 테스트용 임시 위치 보정

    Apply_LookAt(cameraPos, lookAt);

    _smoothedTargetPos = cameraPos;
}

void BattleTurnCamera::Update_FreeAimRotation(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    CHECK_NULL(_transformCom);

    const Vec2 mouseDelta = GAME->Get_MouseDelta();

    _yaw += mouseDelta.x * _mouseSensor;
    _pitch += mouseDelta.y * _mouseSensor;
    _pitch = std::clamp(_pitch, -25.f, 20.f); // 상하 각도 제한

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

void BattleTurnCamera::Set_Participants(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies)
{
    _heroes = heroes;
    _enemies = enemies;
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
    _isClipPreview = false;
    _isFirstUpdate = true;
    _isWideMode = false;
}

const BattleCameraPreset* BattleTurnCamera::Resolve_BattleCameraPresetById(BattleCameraMode mode, const string& presetId) const
{
    if (presetId.empty())
        return nullptr;

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
    if (clipId.empty())
        return nullptr;

    const string cacheKey = string(magic_enum::enum_name(mode)) + ":" + clipId;
    if (auto iter = _clipsById.find(cacheKey); iter != _clipsById.end())
    {
        LOG_DEBUG(
            "[BattleCameraClip] cache mode={} clipId={} duration={:.3f} timeMode={} shakeKeys={}",
            string(magic_enum::enum_name(mode)),
            clipId,
            iter->second.duration,
            string(magic_enum::enum_name(iter->second.timeMode)),
            iter->second.shakeAmplitude.keyframes.size());
        return &iter->second;
    }

    BattleCameraClip clip{};
    if (!BattleCameraClipIO::LoadBattleCameraClipFromFile(mode, clip, clipId))
    {
        LOG_WARN(
            "[BattleCameraClip] load failed mode={} clipId={}",
            string(magic_enum::enum_name(mode)),
            clipId);
        return nullptr;
    }

    const BattleCameraTrack& shakeTrack = clip.shakeAmplitude;
    const float firstShakeTime = shakeTrack.keyframes.empty() ? -1.f : shakeTrack.keyframes.front().time;
    const float firstShakeValue = shakeTrack.keyframes.empty() ? 0.f : shakeTrack.keyframes.front().value;
    const float lastShakeTime = shakeTrack.keyframes.empty() ? -1.f : shakeTrack.keyframes.back().time;
    const float lastShakeValue = shakeTrack.keyframes.empty() ? 0.f : shakeTrack.keyframes.back().value;
    LOG_DEBUG(
        "[BattleCameraClip] loaded mode={} clipId={} duration={:.3f} timeMode={} shakeKeys={} first={:.3f}:{:.3f} last={:.3f}:{:.3f}",
        string(magic_enum::enum_name(mode)),
        clipId,
        clip.duration,
        string(magic_enum::enum_name(clip.timeMode)),
        shakeTrack.keyframes.size(),
        firstShakeTime,
        firstShakeValue,
        lastShakeTime,
        lastShakeValue);

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
    {
        Vec3 heroCenter = Vec3::Zero;
        uint32 heroCount = 0;

        for (const BattleChar& hero : _heroes)
        {
            if (hero.actor == nullptr || hero.actor->Get_Transform() == nullptr)
                continue;

            heroCenter += hero.actor->Get_Transform()->Get_WorldPosition();
            ++heroCount;
        }

        if (heroCount == 0)
            return false;

        heroCenter /= static_cast<float>(heroCount);

        Shared<GameObject> yawBaseActor = nullptr;
        if (_heroes.size() > 1 && _heroes[1].actor && _heroes[1].actor->Get_Transform())
            yawBaseActor = _heroes[1].actor;
        else if (!_heroes.empty() && _heroes[0].actor && _heroes[0].actor->Get_Transform())
            yawBaseActor = _heroes[0].actor;

        if (yawBaseActor == nullptr)
            return false;

        const Vec3 baseRot = yawBaseActor->Get_Transform()->Get_RotationEuler();
        const float cameraYawRad = XMConvertToRadians(baseRot.y);

        Vec3 forwardDir{};
        forwardDir.x = sinf(cameraYawRad);
        forwardDir.y = 0.f;
        forwardDir.z = cosf(cameraYawRad);
        forwardDir.Normalize();

        const Vec3 backDir = -forwardDir;
        Vec3 rightDir{};
        rightDir.x = cosf(cameraYawRad);
        rightDir.y = 0.f;
        rightDir.z = -sinf(cameraYawRad);
        rightDir.Normalize();

        outPose.cameraPos =
            heroCenter +
            backDir * 8.5f +
            rightDir * 2.0f +
            Vec3(2.f, 3.f, -2.f);
        outPose.lookTarget = heroCenter + Vec3(0.f, 2.2f, 0.f);
        outPose.fovY = preset.camera.fovy;
        outPose.rollOffset = 0.f;
        return true;
    }
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
    EvaluatedPose toPose{};
    if (!Evaluate_ShotRequestPose(request, preset, &targetSmoothedTargetPos, BattleCameraClipSample{}, 0.f, toPose))
        return false;

    _activeClip = activeClip;
    Reset_ClipPlaybackState();

    _poseTransition.active = true;
    _poseTransition.elapsed = 0.f;
    _poseTransition.duration = 0.35f;
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
    if (Evaluate_ShotRequestPose(
        _poseTransition.targetRequest,
        _poseTransition.targetPreset,
        &_poseTransition.targetSmoothedTargetPos,
        clipSample,
        _clipTime,
        clippedTargetPose))
    {
        blended.cameraPos += clippedTargetPose.cameraPos - _poseTransition.toPose.cameraPos;
        blended.lookTarget += clippedTargetPose.lookTarget - _poseTransition.toPose.lookTarget;
        blended.fovY += clippedTargetPose.fovY - _poseTransition.toPose.fovY;
        blended.rollOffset += clippedTargetPose.rollOffset - _poseTransition.toPose.rollOffset;
    }

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

    const bool isSameClip = _currentRequest.clipId == request.clipId && _currentRequest.useClip == request.useClip;
    const bool shouldDisableClip = request.useClip == false;
    const bool shouldRestartClip = request.restartClip || !isSameClip;

    if (isSameRequest &&
        request.restartClip == false &&
        (!request.useClip || _activeClip != nullptr))
    {
        LOG_DEBUG(
            "[BattleCameraRequest] ignored same request mode={} presetId={} clipId={} useClip={} restartClip={} activeClip={}",
            string(magic_enum::enum_name(request.mode)),
            request.presetId,
            request.clipId,
            request.useClip,
            request.restartClip,
            _activeClip != nullptr);
        return;
    }

    bool wasWide = _currentRequest.mode == BattleCameraMode::WIDE;
    bool willWide = request.mode == BattleCameraMode::WIDE;

    if(wasWide && !willWide)
        _isWideMode = false;

    const BattleCameraPreset* resolvedPresetById = Resolve_BattleCameraPresetById(request.mode, request.presetId);
    const BattleCameraClip* resolvedClipById = Resolve_BattleCameraClipById(request.mode, request.clipId);

    if (!request.presetId.empty() && resolvedPresetById == nullptr)
        return;

    if (!request.clipId.empty() && resolvedClipById == nullptr)
    {
        LOG_DEBUG(
        "[BattleCameraClip] missing clip, continue with preset only mode={} clipId={}",
        string(magic_enum::enum_name(request.mode)),
        request.clipId);
    }

    BattleCameraClip* nextActiveClip = nullptr;
    BattleCameraPreset nextPreset{};

    switch(request.mode)
    {
    case BattleCameraMode::WIDE:
        if (resolvedPresetById != nullptr)
        {
            nextPreset = *resolvedPresetById;
        }
        else
        {
            nextPreset = _wideModePreset;
        }

        if (resolvedClipById != nullptr)
            nextActiveClip = const_cast<BattleCameraClip*>(resolvedClipById);
        break;

    case BattleCameraMode::BATTLEINTRO:
        if (resolvedPresetById != nullptr)
        {
            nextPreset = *resolvedPresetById;
        }
        else
        {
            nextPreset = _battleIntroPreset;
        }

        if (resolvedClipById != nullptr)
            nextActiveClip = const_cast<BattleCameraClip*>(resolvedClipById);
        else
            nextActiveClip = &_battleIntroClip;
        break;

    case BattleCameraMode::PLAYERFOCUS: 
        if (resolvedPresetById != nullptr)
        {
            nextPreset = *resolvedPresetById;
        }
        else
        {
            nextPreset = _playerFocusPreset;
        }
            
        if (resolvedClipById != nullptr)
            nextActiveClip = const_cast<BattleCameraClip*>(resolvedClipById);
        break;

    case BattleCameraMode::ENEMYFOCUS:
        if (resolvedPresetById != nullptr)
        {
            nextPreset = *resolvedPresetById;
        }
        else
        {
            nextPreset = _enemyFocusPreset;
        }

        if (resolvedClipById != nullptr)
            nextActiveClip = const_cast<BattleCameraClip*>(resolvedClipById);
        break;

    case BattleCameraMode::ENEMYATTACK:
        if (resolvedPresetById != nullptr)
        {
            nextPreset = *resolvedPresetById;
        }
        else
        {
            nextPreset = _enemyAttackPreset;
        }

        if (resolvedClipById != nullptr)
            nextActiveClip = const_cast<BattleCameraClip*>(resolvedClipById);
        else
            nextActiveClip = &_enemyAttackClip;
        break;

    case BattleCameraMode::PLAYERATTACK:
        if (resolvedPresetById != nullptr)
        {
            nextPreset = *resolvedPresetById;
        }
        else if (!request.skillId.empty())
        {
            auto iter = _playerAttackProfilesBySkill.find(request.skillId);
            nextPreset = (iter != _playerAttackProfilesBySkill.end())
                ? iter->second.preset
                : _playerAttackDefaultProfile.preset;
        }
        else
        {
            nextPreset = _playerAttackDefaultProfile.preset;
        }

        if (resolvedClipById != nullptr)
            nextActiveClip = const_cast<BattleCameraClip*>(resolvedClipById);
        else if (!request.skillId.empty())
        {
            auto iter = _playerAttackProfilesBySkill.find(request.skillId);
            nextActiveClip = (iter != _playerAttackProfilesBySkill.end())
                ? const_cast<BattleCameraClip*>(&iter->second.clip)
                : &_playerAttackDefaultProfile.clip;
        }
        else
            nextActiveClip = &_playerAttackDefaultProfile.clip;
        break;

    case BattleCameraMode::SELECTITEM:
        if(resolvedPresetById != nullptr)
        {
            nextPreset = *resolvedPresetById;   
        }
    }

    if(request.useClip == false)
        nextActiveClip = nullptr;

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

    if(nextPreset.snapOnEnter || request.mode == BattleCameraMode::WIDE)
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

    if (Should_StartPoseTransition(request))
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

void BattleTurnCamera::Update_WideMode(float timeDelta)
{
    Update_FromCurrentPreset(timeDelta);
}

void BattleTurnCamera::Update_PlayerFocusMode(float timeDelta)
{
    Update_FromCurrentPreset(timeDelta);
}

void BattleTurnCamera::Update_EnemyFocusMode(float timeDelta)
{
    Update_FromCurrentPreset(timeDelta);
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

    Apply_EvaluatedPose(pose);
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));

    _smoothedTargetPos = enemyPos;
    _prevTargetPos = enemyPos;
}

void BattleTurnCamera::Update_PlayerAttackMode(float timeDelta)
{
    Update_FromCurrentPreset(timeDelta);
}

void BattleTurnCamera::Update_FromCurrentPreset(float timeDelta)
{
    if(_currentRequest.subject == nullptr)
        return;

    // Clip이 있을 시 적용
    const BattleCameraClipSample clip  = Advance_ActiveClip(timeDelta, _currentRequest);
    if (_currentRequest.mode != BattleCameraMode::PLAYERATTACK)
        _smoothedTargetPos = _currentRequest.subject->Get_Transform()->Get_WorldPosition();
    
    EvaluatedPose pose{};
    Evaluate_CurrentPresetPose(_smoothedTargetPos, _currentPreset, clip, _yaw, _pitch, &_smoothedTargetPos, pose);
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
    switch (mode)
    {
    case BattleCameraMode::WIDE:
        return _wideModePreset;

    case BattleCameraMode::BATTLEINTRO:
        return _battleIntroPreset;

    case BattleCameraMode::PLAYERFOCUS:
        return _playerFocusPreset;

    case BattleCameraMode::ENEMYFOCUS:
        return _enemyFocusPreset;

    case BattleCameraMode::ENEMYATTACK:
        return _enemyAttackPreset;

    case BattleCameraMode::PLAYERATTACK:
        return _playerAttackPreset;

    default:
        return _wideModePreset;
    }
}

void BattleTurnCamera::Set_Preset(BattleCameraMode mode, const BattleCameraPreset& preset)
{
    switch (mode)
    {
    case BattleCameraMode::WIDE:
        _wideModePreset = preset;
        break;

    case BattleCameraMode::BATTLEINTRO:
        _battleIntroPreset = preset;
        break;

    case BattleCameraMode::PLAYERFOCUS:
        _playerFocusPreset = preset;
        break;

    case BattleCameraMode::ENEMYFOCUS:
        _enemyFocusPreset = preset;
        break;

    case BattleCameraMode::ENEMYATTACK:
        _enemyAttackPreset = preset;
        break;

    case BattleCameraMode::PLAYERATTACK:
        _playerAttackPreset = preset;
        _playerAttackDefaultProfile.preset = preset;
        break;

    default:
        break;
    }
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

BattleCameraClip* BattleTurnCamera::Get_MutableClip(BattleCameraMode cameraMode)
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

bool BattleTurnCamera::Evaluate_PreviewCurrentPresetPose(
    const Vec3& targetPos,
    const BattleCameraPreset& preset,
    const BattleCameraClipSample& clipSample,
    float yawBase,
    float pitchBase,
    EvaluatedPose& outPose) const
{
    return Evaluate_CurrentPresetPose(targetPos, preset, clipSample, yawBase, pitchBase, nullptr, outPose);
}

bool BattleTurnCamera::Evaluate_PreviewEnemyAttackPose(
    const Vec3& subjectPos,
    const Vec3& opponentPos,
    const BattleCameraPreset& preset,
    const BattleCameraClipSample& clipSample,
    EvaluatedPose& outPose) const
{
    return Evaluate_EnemyAttackPose(subjectPos, opponentPos, preset, clipSample, outPose);
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
            LOG_DEBUG(
                "[BattleCameraClipSync] stop animation changed clipId={} captured={} current={}",
                request.clipId,
                _clipSyncAnimationName,
                currentAnimationName);
            _activeClip = nullptr;
            Reset_ClipPlaybackState();
            return sample;
        }

        if (currentAnimationTimeSec + 0.0001f < _clipSyncPrevAnimationTime)
        {
            LOG_DEBUG(
                "[BattleCameraClipSync] stop animation time rewound clipId={} prev={:.3f} current={:.3f}",
                request.clipId,
                _clipSyncPrevAnimationTime,
                currentAnimationTimeSec);
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

        if (_clipDebugSampleLogCount < 20 || (!_clipDebugLoggedShake && sample.shakeAmplitude > 0.f))
        {
            const BattleCameraTrack& shakeTrack = _activeClip->shakeAmplitude;
            const float firstShakeTime = shakeTrack.keyframes.empty() ? -1.f : shakeTrack.keyframes.front().time;
            const float firstShakeValue = shakeTrack.keyframes.empty() ? 0.f : shakeTrack.keyframes.front().value;
            const float lastShakeTime = shakeTrack.keyframes.empty() ? -1.f : shakeTrack.keyframes.back().time;
            const float lastShakeValue = shakeTrack.keyframes.empty() ? 0.f : shakeTrack.keyframes.back().value;
            LOG_DEBUG(
                "[BattleCameraClipSync] clipId={} anim={} animTime={:.3f}/{:.3f} base={:.3f} sample={:.3f} shake={:.3f} keys={} first={:.3f}:{:.3f} last={:.3f}:{:.3f}",
                request.clipId,
                currentAnimationName,
                currentAnimationTimeSec,
                syncDurationSec,
                _clipSyncBaseTime,
                sampleTime,
                sample.shakeAmplitude,
                shakeTrack.keyframes.size(),
                firstShakeTime,
                firstShakeValue,
                lastShakeTime,
                lastShakeValue);
            ++_clipDebugSampleLogCount;
            if (sample.shakeAmplitude > 0.f)
                _clipDebugLoggedShake = true;
        }

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

    Camera::Free();
}

#include "BattleTurnCamera.h"
#include <CameraPresetIO.h>

#include "BattleCameraClipIO.h"
#include "BattleCameraClipUtils.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "PostProcessPipeline.h"
#include "Character.h"
#include "PartObject.h"
#include "ModelCom.h"
#include "MonsterStatCom.h"
#include "Event_Bus.h"
#include "Battle_Event.h"
#include "Character_BattleBase.h"
#include "Helper_String.h"
#include <chrono>
#include <cmath>

namespace
{
    // DOF_MODIFY 
    constexpr char kBattleDOFOwnerTag[] = "BattleTurn";
    constexpr float kMaxFormationYawBiasDegrees = 35.f;
    constexpr float kAbbestExtraPlayerAttackYawBiasDegrees = 22.f;
    constexpr float kJumpCounterLookOffsetY = 2.f;
    constexpr float kJumpCounterOffsetFadeInSpeed = 10.f;
    constexpr float kJumpCounterOffsetFadeOutSpeed = 12.f;
    constexpr char kAbbestBattlePrefabName[] = "MB_Abbest";
    constexpr char kPotatoBagDashOutReversedPresetId[] = "PotatoBag_DashOut_Reversed";
    constexpr char kPotatoBagAttShieldPresetId[] = "PotatoBag_AttShield";

    constexpr wchar_t kBattleClipShakeDebugSlot[] =
        L"[BattleClipShake] anim={:.3f} sample={:.3f} shakeT={:.3f} amp={:.3f}";

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
    request.skipIfParryActive = event.skipIfParryActive;

    if (event.cameraMode == BattleCameraMode::SELECTITEM ||
        event.cameraMode == BattleCameraMode::SELECTSKILL)
    {
        request.freezeSubjectDuringTransition = true;
    }

    return request;
}

bool Is_PlayerNearClipBattleCameraMode(BattleCameraMode mode)
{
    switch (mode)
    {
    case BattleCameraMode::BATTLEINTRO:
    case BattleCameraMode::PLAYERFOCUS:
    case BattleCameraMode::ENEMYFOCUS:
    case BattleCameraMode::ENEMYGROUP:
    case BattleCameraMode::SELECTITEM:
    case BattleCameraMode::SELECTSKILL:
    case BattleCameraMode::PARRY:
        return true;

    default:
        return false;
    }
}

// DOF_MODIFY : DOF 적용 코드할 때 경우에 따라서 프리셋 다르게 주려고 남김
bool Should_ApplyBattleDOF(BattleCameraMode mode)
{
    switch (mode)
    {
    case BattleCameraMode::PLAYERFOCUS:
    case BattleCameraMode::ENEMYFOCUS:
    case BattleCameraMode::SELECTITEM:
    case BattleCameraMode::SELECTSKILL:
    case BattleCameraMode::PLAYERATTACK:
    case BattleCameraMode::ENEMYATTACK:
    case BattleCameraMode::PARRY:
    case BattleCameraMode::SELECTPLAYER:
        return true;

    default:
        return false;
    }
}

DOFPreset Build_BattleDOFPreset()
{
    DOFPreset preset{};
    preset.enableDOF = true;
    preset.focusRange = 1.5f;
    return preset;
}

DOFTransitionDesc Build_BattleDOFTransition()
{
    DOFTransitionDesc transition{};
    transition.easing = DOFTransitionEasing::EaseInOut;
    transition.duration = 0.35f;
    return transition;
}

float NormalizeAngleDegrees(float angle)
{
    while (angle > 180.f)
        angle -= 360.f;

    while (angle < -180.f)
        angle += 360.f;

    return angle;
}

float Resolve_BattlePlaneSideSignDegrees(
    const Shared<Character>& actor,
    const Vec3& referenceCenter,
    float battlePlaneYawDegrees)
{
    if (actor == nullptr)
        return 0.f;

    const Shared<TransformCom> actorTransform = actor->Get_Transform();
    if (actorTransform == nullptr)
        return 0.f;

    const float yawRad = XMConvertToRadians(battlePlaneYawDegrees);
    const Vec3 battleRight = Vec3(cosf(yawRad), 0.f, -sinf(yawRad));

    const Vec3 delta = actorTransform->Get_WorldPosition() - referenceCenter;
    const float sideDot = delta.x * battleRight.x + delta.y * battleRight.y + delta.z * battleRight.z;
    if (fabsf(sideDot) <= 0.001f)
        return 0.f;

    return sideDot > 0.f ? 1.f : -1.f;
}

bool Is_PotatoBagSamePosePresetSwap(
    const BattleTurnCamera::BattleShotRequest& currentRequest,
    const BattleTurnCamera::BattleShotRequest& nextRequest)
{
    return currentRequest.mode == BattleCameraMode::ENEMYATTACK &&
        nextRequest.mode == BattleCameraMode::ENEMYATTACK &&
        currentRequest.presetId == kPotatoBagDashOutReversedPresetId &&
        nextRequest.presetId == kPotatoBagAttShieldPresetId;
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
    , _playerNearClipDistance(prototype._playerNearClipDistance)
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
    _enemyGroupPreset = _enemyFocusPreset;
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::ENEMYGROUP, _enemyGroupPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::ENEMYATTACK, _enemyAttackPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::PLAYERATTACK, _playerAttackPreset);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::WIDE, _wideModeDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::BATTLEINTRO, _battleIntroDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::PLAYERFOCUS, _playerFocusDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::ENEMYFOCUS, _enemyFocusDrift);
    _enemyGroupDrift = _enemyFocusDrift;
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::ENEMYGROUP, _enemyGroupDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::ENEMYATTACK, _enemyAttackDrift);
    if (!CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::ENEMYATTACK, _enemyAttackBossDrift, true))
        _enemyAttackBossDrift = _enemyAttackDrift;
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::PLAYERATTACK, _playerAttackDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::SELECTITEM, _selectItemDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::SELECTSKILL, _selectSkillDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::PARRY, _parryDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::AIM, _aimDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::SELECTPLAYER, _selectPlayerDrift);

    _enemyAttackClip.duration = 5.f;

   _battleIntroClip.timeMode = Get_DefaultBattleCameraClipTimeMode(BattleCameraMode::BATTLEINTRO);
   _playerAttackClip.timeMode = Get_DefaultBattleCameraClipTimeMode(BattleCameraMode::PLAYERATTACK);
   _enemyAttackClip.timeMode = Get_DefaultBattleCameraClipTimeMode(BattleCameraMode::ENEMYATTACK);

   BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode::BATTLEINTRO, _battleIntroClip);
   BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode::PLAYERATTACK, _playerAttackClip);
   BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode::ENEMYATTACK, _enemyAttackClip);

   // DOF_MODIFY 
    // DOF 프리셋은 일단 카메라 프리셋과 동일하게 세팅한 뒤, 필요한 모드에만 별도 조정값 적용하는 방식으로 간다.
    // 각 값 조정은 주석 확인... 
   _playerFocusDOFProfile.preset = Build_BattleDOFPreset();
   _playerFocusDOFProfile.preset.focusDistance = 0.01f;
   _playerFocusDOFProfile.preset.focusRange = 25.61f;
   _playerFocusDOFProfile.preset.farMaxRadius = 1.25f;
   _playerFocusDOFProfile.preset.farIntensity = 0.45f;

   _enemyFocusDOFProfile.preset = Build_BattleDOFPreset();
   _enemyFocusDOFProfile.preset.focusDistance = 2.f;
   _enemyFocusDOFProfile.preset.focusRange = 15.f;
   _enemyFocusDOFProfile.preset.farIntensity = 0.65f;

   _selectItemDOFProfile.preset = Build_BattleDOFPreset();
   _selectItemDOFProfile.preset.focusDistance = 0.f;
   _selectItemDOFProfile.preset.focusRange = 2.16f;
   _selectItemDOFProfile.preset.farIntensity = 0.50f;

   _selectSkillDOFProfile.preset = Build_BattleDOFPreset();
   _selectSkillDOFProfile.preset.focusDistance = 0.f;
   _selectSkillDOFProfile.preset.focusRange = 5.f;
   _selectSkillDOFProfile.preset.farIntensity = 0.70f;

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
    Refresh_RuntimeDriftFromProfile(_wideModeDrift, true);

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

    if (_isRuntimePausedByTutorial)
        return; // pauseBattle 튜토리얼이 열린 동안 transition/clip/drift 카메라 진행을 고정한다.

    Update_JumpCounterPoseOffset(timeDelta);

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
    case BattleCameraMode::ENEMYGROUP:
        Update_EnemyGroupMode(timeDelta);
        break;

    case BattleCameraMode::BATTLEINTRO:
    case BattleCameraMode::PLAYERFOCUS:
    case BattleCameraMode::ENEMYFOCUS:
    case BattleCameraMode::PLAYERATTACK:
    case BattleCameraMode::SELECTITEM:
    case BattleCameraMode::SELECTSKILL:
    case BattleCameraMode::PARRY:
    case BattleCameraMode::AIM:
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
    Apply_RuntimeDrift(pose, timeDelta);
    Apply_RuntimeShake(pose, timeDelta);
    Apply_EvaluatedPoseWithDOF(pose, BattleCameraMode::WIDE);
    _smoothedTargetPos = pose.cameraPos;
}

void BattleTurnCamera::Reset_FreeAimRotation(float baseYaw)
{
    _freeAimBaseYaw = baseYaw;
    _yaw = 0.f;
    _pitch = 0.f;

    if (_transformCom)
    {
        _transformCom->Set_RotationEuler(Vec3(_pitch, _freeAimBaseYaw, 0.f));
        Update_TransformMatrices();
    }
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

    if (_currentRequest.mode == BattleCameraMode::AIM)
    {
        Update_FromCurrentPreset(0.f);
        return;
    }

    _transformCom->Set_RotationEuler(Vec3(_pitch, _freeAimBaseYaw + _yaw, 0.f));

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

bool BattleTurnCamera::Should_ClipPlayerNearCamera()
{
    if (!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return false;

    const BattleCameraMode mode =
        _poseTransition.active
            ? _poseTransition.targetRequest.mode   // 전환 중에는 다음 연출 구도 기준으로 플레이어를 먼저 잘라낸다.
            : _currentRequest.mode;

    return Is_PlayerNearClipBattleCameraMode(mode);
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
    _hasEnemyGroupAnchor = Try_CaptureEnemyGroupAnchor(_enemyGroupAnchorCenter, _enemyGroupAnchorYawDegrees);
}

void BattleTurnCamera::Reset_RuntimeState()
{
    _currentRequest = {};
    _targetTransform.reset();
    _activeClip = nullptr;
    Reset_ClipPlaybackState();
    _poseTransition = {};
    _runtimeDrift.elapsed = 0.f;
    _runtimeDrift.active = true;
    _runtimeShake = {};
    _smoothedTargetPos = Vec3::Zero;
    _prevTargetPos = Vec3::Zero;
    _enemyPos = Vec3::Zero;
    _playerPos = Vec3::Zero;
    _playerAttackSubjectPos = Vec3::Zero;
    _playerAttackOpponentPos = Vec3::Zero;
    _enemySpawnDescs.clear();
    _hasWideAnchor = false;
    _hasEnemyGroupAnchor = false;
    _wideAnchorHeroCenter = Vec3::Zero;
    _battlePlaneYawDegrees = 0.f;
    _wideAnchorYawDegrees = 0.f;
    _enemyGroupAnchorCenter = Vec3::Zero;
    _enemyGroupAnchorYawDegrees = 0.f;
    _yaw = 0.f;
    _pitch = 0.f;
    _isClipPreview = false;
    _isRuntimePausedByTutorial = false;
    _isFirstUpdate = true;
    _isWideMode = false;
    _battleDOFPresetApplied = false;
    _battleDOFActiveMode = BattleCameraMode::WIDE;
    _battleDOFLastAutoFocusDistance = 0.f;
    _battleDOFLastAppliedFocusDistance = 0.f;
    _runtimeDrift = {};
    _isJumpCounterPoseOffsetActive = false;
    _jumpCounterPoseOffsetWeight = 0.f;
}

void BattleTurnCamera::Set_JumpCounterPoseOffsetActive(bool active)
{
    _isJumpCounterPoseOffsetActive = active;
}

void BattleTurnCamera::Refresh_RuntimeDriftFromProfile(const BattleCameraDriftProfile& profile, bool resetElapsed)
{
    if (resetElapsed)
        _runtimeDrift.elapsed = 0.f;

    _runtimeDrift.active = profile.enabled;
    _runtimeDrift.lateralAmplitude = profile.lateralAmplitude;
    _runtimeDrift.verticalAmplitude = profile.verticalAmplitude;
    _runtimeDrift.depthAmplitude = profile.depthAmplitude;
    _runtimeDrift.rollAmplitude = profile.rollAmplitude;
    _runtimeDrift.lateralFrequency = profile.lateralFrequency;
    _runtimeDrift.verticalFrequency = profile.verticalFrequency;
    _runtimeDrift.depthFrequency = profile.depthFrequency;
    _runtimeDrift.rollFrequency = profile.rollFrequency;
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
    outYawDegrees = Resolve_BattlePlaneYaw();
    return true;
}

bool BattleTurnCamera::Try_CaptureEnemyGroupAnchor(Vec3& outEnemyCenter, float& outYawDegrees) const
{
    outEnemyCenter = Vec3::Zero;
    outYawDegrees = Resolve_BattlePlaneYaw();

    uint32 enemyCount = 0;
    for (const BattleChar& enemy : _enemies)
    {
        if (enemy.actor == nullptr || enemy.actor->Get_Transform() == nullptr)
            continue;

        outEnemyCenter += enemy.actor->Get_Transform()->Get_WorldPosition();
        ++enemyCount;
    }

    if (enemyCount == 0)
        return false;

    outEnemyCenter /= static_cast<float>(enemyCount);
    return true;
}

BattleCameraDriftProfile* BattleTurnCamera::Find_DriftStorage(BattleCameraMode cameraMode, bool enemyAttackBossVariant)
{
    switch (cameraMode)
    {
    case BattleCameraMode::WIDE: return &_wideModeDrift;
    case BattleCameraMode::BATTLEINTRO: return &_battleIntroDrift;
    case BattleCameraMode::PLAYERFOCUS: return &_playerFocusDrift;
    case BattleCameraMode::ENEMYFOCUS: return &_enemyFocusDrift;
    case BattleCameraMode::ENEMYGROUP: return &_enemyGroupDrift;
    case BattleCameraMode::ENEMYATTACK: return enemyAttackBossVariant ? &_enemyAttackBossDrift : &_enemyAttackDrift;
    case BattleCameraMode::PLAYERATTACK: return &_playerAttackDrift;
    case BattleCameraMode::SELECTITEM: return &_selectItemDrift;
    case BattleCameraMode::SELECTSKILL: return &_selectSkillDrift;
    case BattleCameraMode::PARRY: return &_parryDrift;
    case BattleCameraMode::AIM: return &_aimDrift;
    case BattleCameraMode::SELECTPLAYER: return &_selectPlayerDrift;
    default: return nullptr;
    }
}

const BattleCameraDriftProfile* BattleTurnCamera::Find_DriftStorage(BattleCameraMode cameraMode, bool enemyAttackBossVariant) const
{
    return const_cast<BattleTurnCamera*>(this)->Find_DriftStorage(cameraMode, enemyAttackBossVariant);
}

bool BattleTurnCamera::Is_BossEnemyAttackRequest(const BattleShotRequest& request) const
{
    if (request.mode != BattleCameraMode::ENEMYATTACK)
        return false;

    const Shared<CombatStatCom> subjectStatCom =
        request.subject ? request.subject->Get_Component<CombatStatCom>() : nullptr;

    if (const Shared<MonsterStatCom> directMonsterStat = dynamic_pointer_cast<MonsterStatCom>(subjectStatCom))
        return directMonsterStat->Is_Boss();

    for (const BattleChar& enemy : _enemies)
    {
        const bool isSameActor = enemy.actor.get() == request.subject.get();
        const bool isSameStatCom = subjectStatCom != nullptr && enemy.statCom.get() == subjectStatCom.get();
        if ((!isSameActor && !isSameStatCom) || enemy.statCom == nullptr)
            continue;

        const Shared<MonsterStatCom> monsterStat = dynamic_pointer_cast<MonsterStatCom>(enemy.statCom);
        if (monsterStat != nullptr)
            return monsterStat->Is_Boss();
    }

    return false;
}

const BattleCameraDriftProfile* BattleTurnCamera::Resolve_RuntimeDriftProfile(const BattleShotRequest& request) const
{
    return Find_DriftStorage(request.mode, Is_BossEnemyAttackRequest(request));
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

    case BattleCameraMode::ENEMYGROUP:
        return &_enemyGroupPreset;

    case BattleCameraMode::ENEMYATTACK:
        return &_enemyAttackPreset;

    case BattleCameraMode::PLAYERATTACK:
        return &_playerAttackPreset;

    default:
        return nullptr;
    }
}

BattleTurnCamera::BattleDOFProfile* BattleTurnCamera::Find_DOFProfileStorage(BattleCameraMode cameraMode)
{
    return const_cast<BattleDOFProfile*>(
        static_cast<const BattleTurnCamera*>(this)->Find_DOFProfileStorage(cameraMode));
}

const BattleTurnCamera::BattleDOFProfile* BattleTurnCamera::Find_DOFProfileStorage(BattleCameraMode cameraMode) const
{
    switch (cameraMode)
    {
    case BattleCameraMode::PLAYERFOCUS:
        return &_playerFocusDOFProfile;
    case BattleCameraMode::ENEMYFOCUS:
        return &_enemyFocusDOFProfile;
    case BattleCameraMode::SELECTITEM:
        return &_selectItemDOFProfile;
    case BattleCameraMode::SELECTSKILL:
        return &_selectSkillDOFProfile;
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

    case BattleCameraMode::ENEMYGROUP:
        return &_enemyGroupClip;

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
   UNREFERENCED_PARAMETER(timeDelta);

   if(!_runtimeShake.active)
      return;

    _runtimeShake.elapsed += std::max(0.f, GAME->Get_UnscaledTimeDelta());

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

void BattleTurnCamera::Update_JumpCounterPoseOffset(float timeDelta)
{
    const float targetWeight = _isJumpCounterPoseOffsetActive ? 1.f : 0.f;
    const float speed = _isJumpCounterPoseOffsetActive
        ? kJumpCounterOffsetFadeInSpeed
        : kJumpCounterOffsetFadeOutSpeed;
    const float alpha = 1.f - expf(-speed * std::max(0.f, timeDelta));

    _jumpCounterPoseOffsetWeight = lerp(_jumpCounterPoseOffsetWeight, targetWeight, alpha);

    if (fabsf(_jumpCounterPoseOffsetWeight - targetWeight) <= 0.001f)
        _jumpCounterPoseOffsetWeight = targetWeight;
}

void BattleTurnCamera::Apply_RuntimeDrift(EvaluatedPose& pose, float timeDelta)
{
    if (!_runtimeDrift.active)
        return;

    _runtimeDrift.elapsed += std::max(0.f, timeDelta);

    BattleCameraDriftProfile profile{};
    profile.enabled = _runtimeDrift.active;
    profile.lateralAmplitude = _runtimeDrift.lateralAmplitude;
    profile.verticalAmplitude = _runtimeDrift.verticalAmplitude;
    profile.depthAmplitude = _runtimeDrift.depthAmplitude;
    profile.rollAmplitude = _runtimeDrift.rollAmplitude;
    profile.lateralFrequency = _runtimeDrift.lateralFrequency;
    profile.verticalFrequency = _runtimeDrift.verticalFrequency;
    profile.depthFrequency = _runtimeDrift.depthFrequency;
    profile.rollFrequency = _runtimeDrift.rollFrequency;
    Apply_RuntimeDriftProfile(pose, profile, _runtimeDrift.elapsed, 1.f);
}

void BattleTurnCamera::Apply_RuntimeDriftProfile(
    EvaluatedPose& pose,
    const BattleCameraDriftProfile& profile,
    float elapsed,
    float weight) const
{
    if (!profile.enabled || weight <= 0.f)
        return;

    Vec3 forward = pose.lookTarget - pose.cameraPos;
    if (forward.LengthSquared() <= 0.0001f)
        return;

    forward.Normalize();

    Vec3 right = Vec3::Up.Cross(forward);
    if (right.LengthSquared() <= 0.0001f)
        return;

    right.Normalize();

    Vec3 up = forward.Cross(right);
    up.Normalize();

    const float t = std::max(0.f, elapsed);
    const float driftWeight = std::clamp(weight, 0.f, 1.f);

    const float x = sinf(t * profile.lateralFrequency) * profile.lateralAmplitude * driftWeight;
    const float y = sinf(t * profile.verticalFrequency + 1.37f) * profile.verticalAmplitude * driftWeight;
    const float z = sinf(t * profile.depthFrequency + 2.11f) * profile.depthAmplitude * driftWeight;
    const float roll = sinf(t * profile.rollFrequency + 0.73f) * profile.rollAmplitude * driftWeight;

    const Vec3 driftOffset = right * x + up * y + forward * z;

    pose.cameraPos += driftOffset;
    pose.lookTarget += driftOffset * 0.6f;
    pose.rollOffset += roll;
}

void BattleTurnCamera::Apply_ClipShake(
    EvaluatedPose& pose,
    const BattleCameraClipSample& clipSample,
    float clipShakeTime) const
{
    // clip shake도 drift 이후의 최종 pose에 additive로 얹어야
    // notify shake와 비슷한 화면 체감을 내고 다른 보정에 덜 묻힌다.

    if (clipSample.shakeAmplitude <= 0.f)
        return;

    Vec3 forward = pose.lookTarget - pose.cameraPos;
    if (forward.LengthSquared() <= 0.0001f)
        return;

    forward.Normalize();

    Vec3 right = Vec3::Up.Cross(forward);
    if (right.LengthSquared() <= 0.0001f)
        return;

    right.Normalize();

    Vec3 up = forward.Cross(right);
    up.Normalize();

    const Vec3 shakeOffset = Sample_BattleCameraShakeOffset(
        clipSample.shakeAmplitude,
        clipSample.shakeVerticalRatio,
        clipSample.shakeStrengthScale,
        clipSample.shakeFrequencyX,
        clipSample.shakeFrequencyY,
        clipSample.shakeDecayRateX,
        clipSample.shakeDecayRate,
        clipShakeTime,
        right,
        up);

    pose.cameraPos += shakeOffset;
    pose.lookTarget += shakeOffset;
}

void BattleTurnCamera::Debug_PrintClipShakeSample(
    float animationTimeSec,
    float sampleTime,
    float shakeSampleTime,
    const BattleCameraClipSample& clipSample) const
{
#ifdef _DEBUG
    GAME->Print(
        kBattleClipShakeDebugSlot,
        animationTimeSec,
        sampleTime,
        shakeSampleTime,
        clipSample.shakeAmplitude,
        duration(0.f));
#endif
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

void BattleTurnCamera::Apply_JumpCounterPoseOffset(EvaluatedPose& pose) const
{
    if (_jumpCounterPoseOffsetWeight <= 0.001f)
        return;

    pose.lookTarget += Vec3::Up * (kJumpCounterLookOffsetY * _jumpCounterPoseOffsetWeight);
}

// DOF 적용 코드(카메라 전환에 따른 초점 조절 코드)
void BattleTurnCamera::Apply_EvaluatedPoseWithDOF(const EvaluatedPose& pose, BattleCameraMode cameraMode)
{
    EvaluatedPose adjustedPose = pose;
    Apply_JumpCounterPoseOffset(adjustedPose);
    Apply_EvaluatedPose(adjustedPose);
    Sync_BattleDOF(adjustedPose, cameraMode);
}

void BattleTurnCamera::Sync_BattleDOF(const EvaluatedPose& pose, BattleCameraMode cameraMode)
{
    const DOFTransitionDesc transition = Build_BattleDOFTransition();

    if (!Should_ApplyBattleDOF(cameraMode))
    {
        _battleDOFPresetApplied = false;
        _battleDOFActiveMode = BattleCameraMode::WIDE;
        _battleDOFLastAutoFocusDistance = 0.f;
        _battleDOFLastAppliedFocusDistance = 0.f;
        GAME->Clear_DOFOverrideToBaseline(kBattleDOFOwnerTag, transition);
        return;
    }

    const BattleDOFProfile* dofProfile = Find_DOFProfileStorage(cameraMode);
    const bool presetModeChanged = !_battleDOFPresetApplied || _battleDOFActiveMode != cameraMode;
    const float autoFocusDistance = max(0.f, (pose.lookTarget - pose.cameraPos).Length());
    DOFPreset preset = dofProfile ? dofProfile->preset : Build_BattleDOFPreset();
    float focusDistance = max(0.f, autoFocusDistance + preset.focusDistance);
    float focusRange = preset.focusRange;

    if (Shared<PostProcessPipeline> pipeline = GAME->Get_PostProcessPipeline())
    {
        const PostProcessCB& params = pipeline->Get_PostProcessParams();
        if (!presetModeChanged)
            focusRange = max(0.01f, params.dofFocusRange);

        if (!presetModeChanged)
        {
            const float userFocusDistanceDelta = params.dofFocusDistance - _battleDOFLastAppliedFocusDistance;
            focusDistance = max(0.f, focusDistance + userFocusDistanceDelta);
        }
    }

    preset.focusDistance = focusDistance;
    preset.focusRange = focusRange;

    if (presetModeChanged)
    {
        GAME->Set_DOFOverridePreset(kBattleDOFOwnerTag, preset, transition);
        _battleDOFPresetApplied = true;
        _battleDOFActiveMode = cameraMode;
    }

    GAME->Set_DOFOverrideFocus(kBattleDOFOwnerTag, preset.focusDistance, preset.focusRange);
    _battleDOFLastAutoFocusDistance = autoFocusDistance;
    _battleDOFLastAppliedFocusDistance = preset.focusDistance;
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

bool BattleTurnCamera::Evaluate_EnemyGroupPose(
    const BattleCameraPreset& preset,
    const BattleCameraClipSample& clipSample,
    float clipTime,
    EvaluatedPose& outPose) const
{
    UNREFERENCED_PARAMETER(clipTime);

    auto selectDominantOffset = [](float current, float candidate)
    {
        return fabsf(candidate) > fabsf(current) ? candidate : current;
    };

    float enemyGroupBackOffset = 0.f;
    float enemyGroupHeightOffset = 0.f;
    float enemyGroupLookHeightOffset = 0.f;

    for (const BattleMonsterSpawnDesc& spawnDesc : _enemySpawnDescs)
    {
        enemyGroupBackOffset = selectDominantOffset(enemyGroupBackOffset, spawnDesc.enemyGroupBackOffset);
        enemyGroupHeightOffset = selectDominantOffset(enemyGroupHeightOffset, spawnDesc.enemyGroupHeightOffset);
        enemyGroupLookHeightOffset = selectDominantOffset(enemyGroupLookHeightOffset, spawnDesc.enemyGroupLookHeightOffset);
    }

    Vec3 enemyCenter = _enemyGroupAnchorCenter;
    float yawDegrees = _enemyGroupAnchorYawDegrees;

    if (!_hasEnemyGroupAnchor && !Try_CaptureEnemyGroupAnchor(enemyCenter, yawDegrees))
        return false;

    const float finalFovY = clamp(
        preset.camera.fovy + clipSample.fovOffset,
        XMConvertToRadians(10.f),
        XMConvertToRadians(120.f));

    return Evaluate_BattleTurnWidePose(
        enemyCenter,
        yawDegrees,
        finalFovY,
        enemyGroupBackOffset,
        enemyGroupHeightOffset,
        enemyGroupLookHeightOffset,
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
    case BattleCameraMode::ENEMYGROUP:
        {
            return Evaluate_EnemyGroupPose(preset, clipSample, clipTime, outPose);
        }

    case BattleCameraMode::PLAYERATTACK:
        {
            if (request.subject == nullptr || request.opponent == nullptr)
                return false;

            Vec3 subjectPos = _playerAttackSubjectPos;
            Vec3 opponentPos = _playerAttackOpponentPos;

            const float yawBase = Resolve_YawBaseForPresetMode(request);

            return Evaluate_BattleTurnPlayerAttackBattlePlanePose(
                subjectPos,
                opponentPos,
                preset,
                clipSample,
                yawBase,
                clipTime,
                outPose);
        }

    case BattleCameraMode::WIDE:
        {
            return Evaluate_WidePose(preset.camera.fovy, outPose);
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
        {
            if (request.subject == nullptr)
                return false;

            const Vec3 subjectPos = request.subject->Get_Transform()->Get_WorldPosition();
            const float yawBase = Resolve_YawBaseForPresetMode(request);

            return Evaluate_BattleTurnPresetPose(
                subjectPos,
                preset,
                clipSample,
                yawBase,
                _pitch,
                smoothedTargetPos,
                clipTime,
                outPose);
        }
    }
}

float BattleTurnCamera::Resolve_BattlePlaneYaw() const
{
    return _battlePlaneYawDegrees;
}

int BattleTurnCamera::Resolve_EnemyIndexForActor(const Shared<Character>& actor) const
{
    if (actor == nullptr)
        return -1;

    const Shared<CombatStatCom> actorStatCom = actor->Get_Component<CombatStatCom>();
    for (int i = 0; i < static_cast<int>(_enemies.size()); ++i)
    {
        const BattleChar& enemy = _enemies[i];
        const bool isSameActor = enemy.actor.get() == actor.get();
        const bool isSameStatCom = actorStatCom != nullptr && enemy.statCom.get() == actorStatCom.get();
        if (isSameActor || isSameStatCom)
            return i;
    }

    return -1;
}

float BattleTurnCamera::Resolve_EnemyFormationYawBiasDegrees(const Shared<Character>& enemyActor) const
{
    const int enemyIndex = Resolve_EnemyIndexForActor(enemyActor);
    if (enemyIndex < 0)
        return 0.f;

    const Shared<TransformCom> enemyTransform =
        _enemies[enemyIndex].actor != nullptr ? _enemies[enemyIndex].actor->Get_Transform() :
        (enemyActor != nullptr ? enemyActor->Get_Transform() : nullptr);
    if (enemyTransform == nullptr)
        return 0.f;

    const float expectedFacingYaw = NormalizeAngleDegrees(Resolve_BattlePlaneYaw() + 180.f);
    const float enemyYaw = NormalizeAngleDegrees(enemyTransform->Get_RotationEuler().y);
    const float rawBias = NormalizeAngleDegrees(enemyYaw - expectedFacingYaw);
    return std::clamp(rawBias, -kMaxFormationYawBiasDegrees, kMaxFormationYawBiasDegrees);
}

float BattleTurnCamera::Resolve_RequestFormationYawBiasDegrees(const BattleShotRequest& request) const
{
    switch (request.mode)
    {
    case BattleCameraMode::ENEMYFOCUS:
        return Resolve_EnemyFormationYawBiasDegrees(request.subject);

    case BattleCameraMode::PLAYERATTACK:
        {
            float bias = Resolve_EnemyFormationYawBiasDegrees(request.opponent);

            if (request.opponent != nullptr &&
                request.opponent->Get_SourcePrefabName() == kAbbestBattlePrefabName)
            {
                const float sideSign = Resolve_BattlePlaneSideSignDegrees(
                    request.opponent,
                    _enemyGroupAnchorCenter,
                    Resolve_BattlePlaneYaw());
                bias += sideSign * kAbbestExtraPlayerAttackYawBiasDegrees;
            }

            return bias;
        }

    default:
        return 0.f;
    }
}

float BattleTurnCamera::Resolve_YawBaseForPresetMode(const BattleShotRequest& request) const
{ 
    switch (request.mode)
    {
    case BattleCameraMode::AIM:
        return _freeAimBaseYaw + _yaw;

    case BattleCameraMode::PLAYERFOCUS:
    case BattleCameraMode::SELECTITEM:
    case BattleCameraMode::SELECTSKILL:
    case BattleCameraMode::PARRY:
    case BattleCameraMode::BATTLEINTRO:
        return Resolve_BattlePlaneYaw();

    case BattleCameraMode::ENEMYFOCUS:
    case BattleCameraMode::PLAYERATTACK:
        return Resolve_BattlePlaneYaw() + Resolve_RequestFormationYawBiasDegrees(request);

    case BattleCameraMode::ENEMYGROUP:
        return _enemyGroupAnchorYawDegrees;

    default:
        return _yaw;
    }
}

bool BattleTurnCamera::Should_StartPoseTransition(const BattleShotRequest& request) const
{
    if (_transformCom == nullptr)
        return false;

    if (request.subject != nullptr)
        return true;

    return request.mode == BattleCameraMode::ENEMYGROUP ||
        (_currentRequest.mode == BattleCameraMode::WIDE && _isWideMode);
}

bool BattleTurnCamera::Start_PoseTransition(
    const BattleShotRequest& request,
    const BattleCameraPreset& preset,
    BattleCameraClip* activeClip,
    bool shouldRestartClipPlayback,
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
    const bool shouldUseFrozenTransitionPose =
        request.freezeSubjectDuringTransition &&
        _poseTransition.hasFrozenPos;

    if (shouldUseFrozenTransitionPose)
    {
        const float yawBase = Resolve_YawBaseForPresetMode(request);
        if (!Evaluate_CurrentPresetPose(
            _poseTransition.frozenSubjectPos,
            preset,
            BattleCameraClipSample{},
            yawBase,
            _pitch,
            &targetSmoothedTargetPos,
            toPose))
        {
            return false;
        }
    }
    else if (request.mode == BattleCameraMode::ENEMYATTACK)
    {
        Vec3 subjectPos = _poseTransition.targetEnemyAttackSubjectPos;
        Vec3 opponentPos = _poseTransition.targetEnemyAttackOpponentPos;
        if (subjectPos.LengthSquared() <= 0.0001f && request.subject != nullptr && request.subject->Get_Transform() != nullptr)
            subjectPos = request.subject->Get_Transform()->Get_WorldPosition();
        if (opponentPos.LengthSquared() <= 0.0001f && request.opponent != nullptr && request.opponent->Get_Transform() != nullptr)
            opponentPos = request.opponent->Get_Transform()->Get_WorldPosition();

        if (!Evaluate_EnemyAttackPose(subjectPos, opponentPos, preset, BattleCameraClipSample{}, toPose))
            return false;
    }
    else if (request.mode == BattleCameraMode::PLAYERATTACK)
    {
        Vec3 subjectPos = _poseTransition.targetPlayerAttackSubjectPos;
        Vec3 opponentPos = _poseTransition.targetPlayerAttackOpponentPos;
        if (subjectPos.LengthSquared() <= 0.0001f && request.subject != nullptr && request.subject->Get_Transform() != nullptr)
            subjectPos = request.subject->Get_Transform()->Get_WorldPosition();
        if (opponentPos.LengthSquared() <= 0.0001f && request.opponent != nullptr && request.opponent->Get_Transform() != nullptr)
            opponentPos = request.opponent->Get_Transform()->Get_WorldPosition();

        const float yawBase = Resolve_YawBaseForPresetMode(request);
        if (!Evaluate_BattleTurnPlayerAttackBattlePlanePose(
            subjectPos,
            opponentPos,
            preset,
            BattleCameraClipSample{},
            yawBase,
            0.f,
            toPose))
        {
            return false;
        }
    }
    else if (!Evaluate_ShotRequestPose(request, preset, &targetSmoothedTargetPos, BattleCameraClipSample{}, 0.f, toPose))
    {
        return false;
    }

    if (shouldRestartClipPlayback || _activeClip != activeClip)
    {
        _activeClip = activeClip;
        Reset_ClipPlaybackState();
    }

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
    EvaluatedPose clippedTargetPose{};
    bool hasClippedTargetPose = false;
    if (_poseTransition.targetRequest.freezeSubjectDuringTransition &&
        _poseTransition.hasFrozenPos)
    {
        const float yawBase = Resolve_YawBaseForPresetMode(_poseTransition.targetRequest);
        hasClippedTargetPose = Evaluate_CurrentPresetPose(
            _poseTransition.frozenSubjectPos,
            _poseTransition.targetPreset,
            clipSample,
            yawBase,
            _pitch,
            &_poseTransition.targetSmoothedTargetPos,
            clippedTargetPose);
    }
    else
    {
        if (_poseTransition.targetRequest.mode == BattleCameraMode::ENEMYATTACK)
        {
            hasClippedTargetPose = Evaluate_EnemyAttackPose(
                _poseTransition.targetEnemyAttackSubjectPos,
                _poseTransition.targetEnemyAttackOpponentPos,
                _poseTransition.targetPreset,
                clipSample,
                clippedTargetPose);
        }
        else if (_poseTransition.targetRequest.mode == BattleCameraMode::PLAYERATTACK)
        {
            const float yawBase = Resolve_YawBaseForPresetMode(_poseTransition.targetRequest);
            hasClippedTargetPose = Evaluate_BattleTurnPlayerAttackBattlePlanePose(
                _poseTransition.targetPlayerAttackSubjectPos,
                _poseTransition.targetPlayerAttackOpponentPos,
                _poseTransition.targetPreset,
                clipSample,
                yawBase,
                _clipShakeTime,
                clippedTargetPose);
        }
        else
        {
            hasClippedTargetPose = Evaluate_ShotRequestPose(
                _poseTransition.targetRequest,
                _poseTransition.targetPreset,
                &_poseTransition.targetSmoothedTargetPos,
                clipSample,
                _clipShakeTime,
                clippedTargetPose);
        }
    }

    const EvaluatedPose& transitionTargetPose = hasClippedTargetPose
        ? clippedTargetPose
        : _poseTransition.toPose;

    EvaluatedPose blended{};
    blended.cameraPos = Vec3::Lerp(_poseTransition.fromPose.cameraPos, transitionTargetPose.cameraPos, easedT);
    blended.lookTarget = Vec3::Lerp(_poseTransition.fromPose.lookTarget, transitionTargetPose.lookTarget, easedT);
    blended.fovY = lerp(_poseTransition.fromPose.fovY, transitionTargetPose.fovY, easedT);
    blended.rollOffset = lerp(_poseTransition.fromPose.rollOffset, transitionTargetPose.rollOffset, easedT);

    const bool isTransitionFinished = t >= 1.f;
    const bool isModeChanging = _currentRequest.mode != _poseTransition.targetRequest.mode;
    if (!isTransitionFinished)
    {
        if (const BattleCameraDriftProfile* targetDrift = Resolve_RuntimeDriftProfile(_poseTransition.targetRequest))
        {
            if (!isModeChanging)
                _runtimeDrift.elapsed += clampedDelta;

            const float driftElapsed = isModeChanging ? _poseTransition.elapsed : _runtimeDrift.elapsed;
            Apply_RuntimeDriftProfile(blended, *targetDrift, driftElapsed, t);
        }

        Apply_ClipShake(blended, clipSample, _clipShakeTime);
        Apply_RuntimeShake(blended, timeDelta);
        Apply_EvaluatedPoseWithDOF(blended, _poseTransition.targetRequest.mode);
        GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));
        return;
    }

    const BattleCameraMode previousMode = _currentRequest.mode;
    _poseTransition.active = false;
    _currentRequest = _poseTransition.targetRequest;
    _currentPreset = _poseTransition.targetPreset;
    _targetPreset = _poseTransition.targetPreset;
    if (const BattleCameraDriftProfile* drift = Resolve_RuntimeDriftProfile(_currentRequest))
        Refresh_RuntimeDriftFromProfile(*drift, previousMode != _currentRequest.mode);
    if (previousMode != _currentRequest.mode)
        _runtimeDrift.elapsed = std::max(0.f, _poseTransition.elapsed - clampedDelta);
    _targetTransform = _currentRequest.subject ? _currentRequest.subject->Get_Transform() : nullptr;
    _smoothedTargetPos = _poseTransition.targetSmoothedTargetPos;
    _prevTargetPos = _poseTransition.targetPrevTargetPos;
    _enemyPos = _poseTransition.targetEnemyAttackSubjectPos;
    _playerPos = _poseTransition.targetEnemyAttackOpponentPos;
    _playerAttackSubjectPos = _poseTransition.targetPlayerAttackSubjectPos;
    _playerAttackOpponentPos = _poseTransition.targetPlayerAttackOpponentPos;
    _isFirstUpdate = true;

    Apply_RuntimeDrift(blended, timeDelta);
    Apply_ClipShake(blended, clipSample, _clipShakeTime);
    Apply_RuntimeShake(blended, timeDelta);
    Apply_EvaluatedPoseWithDOF(blended, _currentRequest.mode);
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));

    const string previousModeName = string(magic_enum::enum_name(previousMode));
    const string currentModeName = string(magic_enum::enum_name(_currentRequest.mode));
    LOG_INFO(
        "[BattleCam][ShotTransitionEnd] prev={} next={} preset={} clip={} elapsed={:.3f}/{:.3f} snap={} driftElapsed={:.3f}",
        previousModeName.c_str(),
        currentModeName.c_str(),
        _currentRequest.presetId.c_str(),
        _currentRequest.clipId.c_str(),
        _poseTransition.elapsed,
        _poseTransition.duration,
        _currentPreset.snapOnEnter,
        _runtimeDrift.elapsed);
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
        _clipShakeTime,
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
        _clipShakeTime,
        outPose);
}

void BattleTurnCamera::Apply_ShotRequest(const BattleShotRequest& request)
{
    static uint64 requestSequence = 0;
    static const auto traceStartTime = chrono::steady_clock::now();

    const uint64 requestId = ++requestSequence;
    const int64 elapsedMs = chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - traceStartTime).count();

    const string incomingModeName = string(magic_enum::enum_name(request.mode));
    const string previousModeName = string(magic_enum::enum_name(_currentRequest.mode));

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
        LOG_INFO(
            "[BattleCam][ShotSkip] id={} elapsedMs={} reason=sameRequest mode={} preset={} clip={} useClip={} activeClip={}",
            requestId,
            elapsedMs,
            incomingModeName.c_str(),
            request.presetId.c_str(),
            request.clipId.c_str(),
            request.useClip,
            _activeClip != nullptr);
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
        _yaw = 0.f;
        _pitch = 0.f;
    }
    const BattleCameraPreset* resolvedPresetById;
    const BattleCameraClip* resolvedClipById;

    resolvedPresetById = Resolve_BattleCameraPresetById(request.mode, request.presetId);
    resolvedClipById = Resolve_BattleCameraClipById(request.mode, request.clipId);

    if (!request.presetId.empty() && resolvedPresetById == nullptr)
    {
        if(request.mode != BattleCameraMode::WIDE &&  request.mode != BattleCameraMode::BATTLEINTRO)
        {
            LOG_INFO(
                "[BattleCam][ShotSkip] id={} elapsedMs={} reason=missingPreset mode={} preset={}",
                requestId,
                elapsedMs,
                incomingModeName.c_str(),
                request.presetId.c_str());
            return;
        }
    }

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
    Vec3 nextPlayerAttackSubjectPos = _playerAttackSubjectPos;
    Vec3 nextPlayerAttackOpponentPos = _playerAttackOpponentPos;

    const bool shouldReAnchored =
        _currentRequest.subject != request.subject ||
        _currentRequest.opponent != request.opponent ||
        _currentRequest.mode != request.mode;
    const bool shouldSkipPotatoBagSamePoseTransition =
        !shouldReAnchored &&
        Is_PotatoBagSamePosePresetSwap(_currentRequest, request);

    LOG_INFO(
        "[BattleCam][ShotRequest] id={} elapsedMs={} prev={} next={} preset={} clip={} useClip={} samePreset={} sameClip={} reanchor={} snap={} transitionActive={} restartClip={} keepClip={} disableClip={}",
        requestId,
        elapsedMs,
        previousModeName.c_str(),
        incomingModeName.c_str(),
        request.presetId.c_str(),
        request.clipId.c_str(),
        request.useClip,
        isSamePreset,
        isSameClip,
        shouldReAnchored,
        nextPreset.snapOnEnter,
        _poseTransition.active,
        shouldRestartClip,
        shouldKeepCurrentClip,
        shouldDisableClip);

    if (request.subject && (nextPreset.snapOnEnter || shouldReAnchored)) //|| _currentRequest.subject == nullptr))
    {
        const Vec3 targetPos = request.subject->Get_Transform()->Get_WorldPosition();
        targetSmoothedTargetPos = targetPos;
        targetPrevTargetPos = targetPos;
    }

    if (request.mode == BattleCameraMode::ENEMYGROUP &&
        (nextPreset.snapOnEnter || shouldReAnchored || targetSmoothedTargetPos.LengthSquared() <= 0.0001f))
    {
        targetSmoothedTargetPos = _enemyGroupAnchorCenter;
        targetPrevTargetPos = _enemyGroupAnchorCenter;
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

    if (request.mode == BattleCameraMode::PLAYERATTACK)
    {
        const bool shouldCapturePlayerAttackAnchor =
            nextPreset.snapOnEnter ||
            shouldReAnchored ||
            nextPlayerAttackSubjectPos.LengthSquared() <= 0.0001f ||
            nextPlayerAttackOpponentPos.LengthSquared() <= 0.0001f;

        if (shouldCapturePlayerAttackAnchor)
        {
            if (request.subject != nullptr && request.subject->Get_Transform() != nullptr)
                nextPlayerAttackSubjectPos = request.subject->Get_Transform()->Get_WorldPosition();

            if (request.opponent != nullptr && request.opponent->Get_Transform() != nullptr)
                nextPlayerAttackOpponentPos = request.opponent->Get_Transform()->Get_WorldPosition();
        }
    }
    else
    {
        nextPlayerAttackSubjectPos = Vec3::Zero;
        nextPlayerAttackOpponentPos = Vec3::Zero;
    }

    if (nextPreset.snapOnEnter ||
        request.mode == BattleCameraMode::WIDE ||
        request.mode == BattleCameraMode::ENEMYGROUP ||
        request.mode == BattleCameraMode::BATTLEINTRO)
    {
        const BattleCameraMode previousMode = _currentRequest.mode;
        _poseTransition.active = false;
        _activeClip = nextActiveClip;
        Reset_ClipPlaybackState();
        _isClipPreview = false;
        _isFirstUpdate = true;
        _currentRequest = request;
        _targetTransform = nextTargetTransform;
        _targetPreset = nextPreset;
        _currentPreset = _targetPreset;
        if (const BattleCameraDriftProfile* drift = Resolve_RuntimeDriftProfile(_currentRequest))
            Refresh_RuntimeDriftFromProfile(*drift, previousMode != _currentRequest.mode);
        _smoothedTargetPos = targetSmoothedTargetPos;
        _prevTargetPos = targetPrevTargetPos;
        _enemyPos = nextEnemyAttackSubjectPos;
        _playerPos = nextEnemyAttackOpponentPos;
        _playerAttackSubjectPos = nextPlayerAttackSubjectPos;
        _playerAttackOpponentPos = nextPlayerAttackOpponentPos;

        EvaluatedPose toPose {};

        if(Evaluate_ShotRequestPose(_currentRequest, _currentPreset, &_smoothedTargetPos, BattleCameraClipSample{}, _clipShakeTime, toPose ))
        Apply_EvaluatedPoseWithDOF(toPose, _currentRequest.mode);

        LOG_INFO(
            "[BattleCam][ShotApply] id={} elapsedMs={} reason=immediate prev={} next={} preset={} clip={} snap={} restartClip={} keepClip={} disableClip={}",
            requestId,
            elapsedMs,
            previousModeName.c_str(),
            incomingModeName.c_str(),
            request.presetId.c_str(),
            request.clipId.c_str(),
            nextPreset.snapOnEnter,
            shouldRestartClip,
            shouldKeepCurrentClip,
            shouldDisableClip);

        return;
    }

    if (shouldSkipPotatoBagSamePoseTransition)
    {
        LOG_INFO(
            "[BattleCam][ShotSkip] id={} elapsedMs={} reason=samePosePresetSwap prevPreset={} nextPreset={} restartClip={}",
            requestId,
            elapsedMs,
            _currentRequest.presetId.c_str(),
            request.presetId.c_str(),
            shouldRestartClip);
    }

    const bool shouldStartPoseTransition = Should_StartPoseTransition(request);
    const bool shouldUsePoseTransition =
        (shouldStartPoseTransition && !isSamePreset && !shouldSkipPotatoBagSamePoseTransition) ||
        shouldReAnchored;

    if (shouldUsePoseTransition)
    {
        _poseTransition.targetEnemyAttackSubjectPos = nextEnemyAttackSubjectPos;
        _poseTransition.targetEnemyAttackOpponentPos = nextEnemyAttackOpponentPos;
        _poseTransition.targetPlayerAttackSubjectPos = nextPlayerAttackSubjectPos;
        _poseTransition.targetPlayerAttackOpponentPos = nextPlayerAttackOpponentPos;
        if (Start_PoseTransition(
            request,
            nextPreset,
            nextActiveClip,
            shouldRestartClip,
            targetSmoothedTargetPos,
            targetPrevTargetPos))
        {
            const string easingName = string(magic_enum::enum_name(nextPreset.transitionEasing));
            LOG_INFO(
                "[BattleCam][ShotTransition] id={} elapsedMs={} prev={} next={} preset={} clip={} duration={:.3f} easing={} reanchor={} samePreset={} restartClip={}",
                requestId,
                elapsedMs,
                previousModeName.c_str(),
                incomingModeName.c_str(),
                request.presetId.c_str(),
                request.clipId.c_str(),
                nextPreset.transitionDuration,
                easingName.c_str(),
                shouldReAnchored,
                isSamePreset,
                shouldRestartClip);
            return;
        }

        LOG_INFO(
            "[BattleCam][ShotTransitionFailed] id={} elapsedMs={} prev={} next={} preset={} clip={} reanchor={} samePreset={}",
            requestId,
            elapsedMs,
            previousModeName.c_str(),
            incomingModeName.c_str(),
            request.presetId.c_str(),
            request.clipId.c_str(),
            shouldReAnchored,
            isSamePreset);
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

    const BattleCameraMode previousMode = _currentRequest.mode;
    _isClipPreview = false;
    _isFirstUpdate = true;
    _currentRequest = request;
    _targetTransform = nextTargetTransform;
    _targetPreset = nextPreset;
    _currentPreset = _targetPreset;
    if (const BattleCameraDriftProfile* drift = Resolve_RuntimeDriftProfile(_currentRequest))
        Refresh_RuntimeDriftFromProfile(*drift, previousMode != _currentRequest.mode);
    _smoothedTargetPos = targetSmoothedTargetPos;
    _prevTargetPos = targetPrevTargetPos;
    _enemyPos = nextEnemyAttackSubjectPos;
    _playerPos = nextEnemyAttackOpponentPos;
    _playerAttackSubjectPos = nextPlayerAttackSubjectPos;
    _playerAttackOpponentPos = nextPlayerAttackOpponentPos;

    LOG_INFO(
        "[BattleCam][ShotApply] id={} elapsedMs={} reason=currentPreset prev={} next={} preset={} clip={} restartClip={} keepClip={} disableClip={}",
        requestId,
        elapsedMs,
        previousModeName.c_str(),
        incomingModeName.c_str(),
        request.presetId.c_str(),
        request.clipId.c_str(),
        shouldRestartClip,
        shouldKeepCurrentClip,
        shouldDisableClip);
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
        if (const BattleCameraDriftProfile* drift = Resolve_RuntimeDriftProfile(_currentRequest))
            Refresh_RuntimeDriftFromProfile(*drift, false);
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
    Apply_RuntimeDrift(pose, timeDelta);
    Apply_ClipShake(pose, clip, _clipShakeTime);
    Apply_RuntimeShake(pose,timeDelta);

    Apply_EvaluatedPoseWithDOF(pose, _currentRequest.mode);
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));

    _smoothedTargetPos = enemyPos;
    _prevTargetPos = enemyPos;
}

void BattleTurnCamera::Update_EnemyGroupMode(float timeDelta)
{
    const BattleCameraClipSample clip = Advance_ActiveClip(timeDelta, _currentRequest);

    EvaluatedPose pose{};
    if (!Evaluate_EnemyGroupPose(_currentPreset, clip, _clipShakeTime, pose))
        return;

    Apply_RuntimeDrift(pose, timeDelta);
    Apply_ClipShake(pose, clip, _clipShakeTime);
    Apply_RuntimeShake(pose, timeDelta);
    Apply_EvaluatedPoseWithDOF(pose, _currentRequest.mode);
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));

    _smoothedTargetPos = _enemyGroupAnchorCenter;
    _prevTargetPos = _enemyGroupAnchorCenter;
}

void BattleTurnCamera::Update_FromCurrentPreset(float timeDelta)
{
    if(_currentRequest.subject == nullptr)
        return;

    // Clip이 있을 시 적용
    const BattleCameraClipSample clip  = Advance_ActiveClip(timeDelta, _currentRequest);

    const bool shouldKeepFrozenAnchorWhileActive =
        _currentRequest.freezeSubjectDuringTransition;

    if (_currentPreset.snapOnEnter == false && !shouldKeepFrozenAnchorWhileActive)
        _smoothedTargetPos = _currentRequest.subject->Get_Transform()->Get_WorldPosition();
    
    EvaluatedPose pose{};
    const float yawBase = Resolve_YawBaseForPresetMode(_currentRequest);
    Evaluate_CurrentPresetPose(_smoothedTargetPos, _currentPreset, clip, yawBase, _pitch, &_smoothedTargetPos, pose);
    Apply_RuntimeDrift(pose, timeDelta);
    Apply_ClipShake(pose, clip, _clipShakeTime);
    Apply_RuntimeShake(pose,timeDelta);
    Apply_EvaluatedPoseWithDOF(pose, _currentRequest.mode);
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));

    _isFirstUpdate = false;
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

const BattleCameraDriftProfile& BattleTurnCamera::Get_DriftProfile(BattleCameraMode mode, bool enemyAttackBossVariant) const
{
    if (const BattleCameraDriftProfile* profile = Find_DriftStorage(mode, enemyAttackBossVariant))
        return *profile;

    return _wideModeDrift;
}

void BattleTurnCamera::Set_Preset(BattleCameraMode mode, const BattleCameraPreset& preset)
{
    BattleCameraPreset* targetPreset = Find_PresetStorage(mode);
    if (targetPreset == nullptr)
        return;

    *targetPreset = preset;
    if (mode == _currentRequest.mode)
    {
        _currentPreset = preset;
        _targetPreset = preset;
        if (const BattleCameraDriftProfile* drift = Find_DriftStorage(_currentRequest.mode))
            Refresh_RuntimeDriftFromProfile(*drift, false);
    }
    if (mode == BattleCameraMode::PLAYERATTACK)
        _playerAttackDefaultProfile.preset = preset;
}

void BattleTurnCamera::Set_DriftProfile(BattleCameraMode mode, const BattleCameraDriftProfile& profile, bool enemyAttackBossVariant)
{
    BattleCameraDriftProfile* targetProfile = Find_DriftStorage(mode, enemyAttackBossVariant);
    if (targetProfile == nullptr)
        return;

    *targetProfile = profile;
    if (mode == _currentRequest.mode && Is_BossEnemyAttackRequest(_currentRequest) == enemyAttackBossVariant)
        Refresh_RuntimeDriftFromProfile(*targetProfile, false);
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

void BattleTurnCamera::Reload_BattleCameraAuthoringData()
{
    _presetsById.clear();
    _clipsById.clear();

    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::WIDE, _wideModePreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::BATTLEINTRO, _battleIntroPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::PLAYERFOCUS, _playerFocusPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::ENEMYFOCUS, _enemyFocusPreset);
    _enemyGroupPreset = _enemyFocusPreset;
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::ENEMYGROUP, _enemyGroupPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::ENEMYATTACK, _enemyAttackPreset);
    CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode::PLAYERATTACK, _playerAttackPreset);

    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::WIDE, _wideModeDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::BATTLEINTRO, _battleIntroDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::PLAYERFOCUS, _playerFocusDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::ENEMYFOCUS, _enemyFocusDrift);
    _enemyGroupDrift = _enemyFocusDrift;
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::ENEMYGROUP, _enemyGroupDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::ENEMYATTACK, _enemyAttackDrift);
    if (!CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::ENEMYATTACK, _enemyAttackBossDrift, true))
        _enemyAttackBossDrift = _enemyAttackDrift;
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::PLAYERATTACK, _playerAttackDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::SELECTITEM, _selectItemDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::SELECTSKILL, _selectSkillDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::PARRY, _parryDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::AIM, _aimDrift);
    CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode::SELECTPLAYER, _selectPlayerDrift);

    _battleIntroClip.timeMode = Get_DefaultBattleCameraClipTimeMode(BattleCameraMode::BATTLEINTRO);
    _playerAttackClip.timeMode = Get_DefaultBattleCameraClipTimeMode(BattleCameraMode::PLAYERATTACK);
    _enemyAttackClip.timeMode = Get_DefaultBattleCameraClipTimeMode(BattleCameraMode::ENEMYATTACK);
    BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode::BATTLEINTRO, _battleIntroClip);
    BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode::PLAYERATTACK, _playerAttackClip);
    BattleCameraClipIO::LoadBattleCameraClipFromFile(BattleCameraMode::ENEMYATTACK, _enemyAttackClip);
}

void BattleTurnCamera::Apply_PreviewPreset(const BattleCameraPreset& preset)
{
    _currentPreset = preset;
    _targetPreset = preset;
    if (const BattleCameraDriftProfile* drift = Find_DriftStorage(_currentRequest.mode))
        Refresh_RuntimeDriftFromProfile(*drift, false);
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

    _clipShakeTime = time;

    BattleCameraClip* clip = Get_MutableClip(_currentRequest.mode);
    const BattleCameraClipSample clipSample = clip ? Sample_BattleCameraClip(*clip, clamp(time, 0.f, clip->duration)) : BattleCameraClipSample {};

    const Vec3 targetPos = _currentRequest.subject->Get_Transform()->Get_WorldPosition();

    EvaluatedPose pose{};
    const float yawBase = Resolve_YawBaseForPresetMode(_currentRequest);
    Evaluate_CurrentPresetPose(targetPos, _currentPreset, clipSample, yawBase, _pitch, nullptr, pose);
    if (const BattleCameraDriftProfile* drift = Resolve_RuntimeDriftProfile(_currentRequest))
        Refresh_RuntimeDriftFromProfile(*drift, false);
    Apply_RuntimeDrift(pose, 0.f);
    Apply_ClipShake(pose, clipSample, _clipShakeTime);
    Apply_EvaluatedPose(pose);
    Apply_EvaluatedPoseWithDOF(pose, _currentRequest.mode);
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));
}

void BattleTurnCamera::Preview_EnemyAttackAtTime(float time)
{
    if (_currentRequest.subject == nullptr || _currentRequest.opponent == nullptr)
        return;

    _clipShakeTime = time;

    BattleCameraClip* clip = Get_MutableClip(_currentRequest.mode);
    const BattleCameraClipSample clipSample = clip ? Sample_BattleCameraClip(*clip, clamp(time, 0.f, clip->duration)) : BattleCameraClipSample {};

    const Vec3 enemyPos = _currentRequest.subject->Get_Transform()->Get_WorldPosition();
    const Vec3 playerPos = _currentRequest.opponent->Get_Transform()->Get_WorldPosition();

    EvaluatedPose pose{};
    if (!Evaluate_EnemyAttackPose(enemyPos, playerPos, _currentPreset, clipSample, pose))
        return;

    if (const BattleCameraDriftProfile* drift = Resolve_RuntimeDriftProfile(_currentRequest))
        Refresh_RuntimeDriftFromProfile(*drift, false);
    Apply_RuntimeDrift(pose, 0.f);
    Apply_ClipShake(pose, clipSample, _clipShakeTime);
    Apply_EvaluatedPose(pose);
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));
    Apply_EvaluatedPoseWithDOF(pose, _currentRequest.mode);
}

float BattleTurnCamera::Evaluate_CameraTransitionT(BattleCameraTransitionEasing easing, float t)
{
    t = clamp(t, 0.f, 1.f);
    constexpr float kBackOvershoot = 1.70158f;

    switch (easing)
    {
    case BattleCameraTransitionEasing::Linear:
        return t;

    case BattleCameraTransitionEasing::EaseIn:
        return t * t;

    case BattleCameraTransitionEasing::EaseOut:
        return 1.f - (1.f - t) * (1.f - t);

    case BattleCameraTransitionEasing::EaseInStrong:
        return t * t * t;

    case BattleCameraTransitionEasing::EaseOutStrong:
        return 1.f - powf(1.f - t, 3.f);

    case BattleCameraTransitionEasing::EaseInOutStrong:
        return t < 0.5f
            ? 4.f * t * t * t
            : 1.f - powf(-2.f * t + 2.f, 3.f) * 0.5f;

    case BattleCameraTransitionEasing::EaseInCubic:
        return t * t * t;

    case BattleCameraTransitionEasing::EaseOutCubic:
        return 1.f - powf(1.f - t, 3.f);

    case BattleCameraTransitionEasing::EaseInExpo:
        return t <= 0.f ? 0.f : powf(2.f, 10.f * t - 10.f);

    case BattleCameraTransitionEasing::EaseOutExpo:
        return t >= 1.f ? 1.f : 1.f - powf(2.f, -10.f * t);

    case BattleCameraTransitionEasing::EaseOutBack:
    {
        const float x = t - 1.f;
        return 1.f + (kBackOvershoot + 1.f) * x * x * x + kBackOvershoot * x * x;
    }

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
    {
        GAME->Clear_Print(kBattleClipShakeDebugSlot);
        return sample;
    }

    _clipShakeTime += std::max(0.f, GAME->Get_UnscaledTimeDelta());

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

        // shake 외의 트랙은 애니메이션 진행률에 계속 맞추고 shake 트랙만 unscaled 시간 기준으로 다시 샘플링함
        // 런타임에서 월드 시간이나 애니메이션 배속이 바뀌어도 툴에서 편집한 shake의 리듬과 감쇠 느낌을 최대한 그대로 유지하기 위함
        Apply_BattleCameraClipShakeSample(*_activeClip, sampleTime, sample);
        Debug_PrintClipShakeSample(currentAnimationTimeSec, sampleTime, _clipShakeTime, sample);

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
    Debug_PrintClipShakeSample(_clipTime, sampleTime, sampleTime, sample);

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
    _clipShakeTime = 0.f;
    _clipSyncBaseTime = 0.f;
    _clipSyncPrevAnimationTime = 0.f;
    _clipSyncAnimationDuration = 0.f;
    _clipSyncAnimationName.clear();
    _clipDebugSampleLogCount = 0;
    _clipDebugLoggedShake = false;
    GAME->Clear_Print(kBattleClipShakeDebugSlot);
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

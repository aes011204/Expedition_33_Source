#include "SoulLikeController.h"
#include "BattleController.h"
#include "BattleTurnCamera.h"
#include "Client_Constants.h"
#include "Event_Bus.h"
#include "ClientInstance.h"
#include "ClientScreenFxManager.h"
#include "GameplayUI.h"
#include "GamePlayUI_SL.h"
#include "GradientSkill.h"
#include "GameInstance.h"
#include "PlayableParts.h"
#include "PlayerStatCom.h"
#include "Player_WorldBase.h"
#include "ScreenFx_Manager.h"
#include "SL_Base.h"
#include "SL_Maelle.h"
#include "SL_DamageSystem.h"
#include "SL_StateMachineCom.h"
#include "SL_TargetCom.h"
#include "Spawn_Prefab.h"
#include "SoulLikeCamera.h"
#include "SkillTreeCom.h"
#include "TransformCom.h"
#include "MonsterStatCom.h"
#include "SL_Skill.h"
#include "SL_EstherManager.h"

namespace
{
    constexpr float kDuallisteBGMFadeOutDuration = 1.f;
}

IMPLEMENT_REFLECTION(SoulLikeController)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.category = "SoulLike";
    info.displayName = "SoulLike Controller";

    PROPERTY_STRING("플레이어 프리팹", _playerPrefab);
    PROPERTY_VEC3("플레이어 스폰 위치", _playerSpawnPosition, 0.1f);
    PROPERTY_VEC3("플레이어 스폰 회전", _playerSpawnRotation, 1.f);
    PROPERTY_STRING("몬스터 프리팹", _monsterPrefab);
    PROPERTY_FLOAT("몬스터 스폰 거리", _monsterSpawnDistance, 0.f, 20.f, 0.1f);

    PROPERTY_FLOAT("플레이어 최대 체력", _playerMaxHp, 1.f, 9999.f, 1.f);
    PROPERTY_FLOAT("몬스터 최대 체력", _monsterMaxHp, 1.f, 9999.f, 1.f);

    PROPERTY_FLOAT("진입 연출 지연 시간", _enterFxDelay, 0.f, 5.f, 0.05f);
    PROPERTY_BOOL("디버그 3번키 시작 사용", _enableDebugKeyStart);

    // 전투 슬로우
    PROPERTY_FLOAT("승리 슬로우 지속 시간", _victorySlowDuration, 0.f, 5.f, 0.05f);
    PROPERTY_FLOAT("승리 슬로우 시간 배율", _victorySlowTimeScale, 0.f, 1.f, 0.01f);

    // 패링 밀려나는거
    PROPERTY_FLOAT("플레이어 피격 밀림 거리", _playerHitPushDistance, 0.f, 5.f, 0.05f);
    PROPERTY_FLOAT("플레이어 피격 밀림 시간", _playerHitPushDuration, 0.f, 1.f, 0.01f);

    PROPERTY_FLOAT("피격 히트스탑 시간 배율", _playerHitStopTimeScale, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("피격 히트스탑 지속 시간", _playerHitStopDuration, 0.f, 1.f, 0.01f);

    PROPERTY_FLOAT("공격 히트스탑 시간 배율", _playerAttackHitStopTimeScale, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("공격 히트스탑 지속 시간", _playerAttackHitStopDuration, 0.f, 1.f, 0.01f);

    return true;
}

SoulLikeController::SoulLikeController(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

SoulLikeController::SoulLikeController(const SoulLikeController& prototype)
    : GameObject(prototype)
    , _playerPrefab(prototype._playerPrefab)
    , _playerSpawnPosition(prototype._playerSpawnPosition)
    , _playerSpawnRotation(prototype._playerSpawnRotation)
    , _monsterPrefab(prototype._monsterPrefab)
    , _monsterSpawnDistance(prototype._monsterSpawnDistance)
    , _enterFxDelay(prototype._enterFxDelay)
    , _playerMaxHp(prototype._playerMaxHp)
    , _enableDebugKeyStart(prototype._enableDebugKeyStart)
    , _monsterMaxHp(prototype._monsterMaxHp)
    , _playerHitPushDistance(prototype._playerHitPushDistance)
    , _playerHitPushDuration(prototype._playerHitPushDuration)
    , _playerHitStopTimeScale(prototype._playerHitStopTimeScale)
    , _playerHitStopDuration(prototype._playerHitStopDuration)
    , _playerAttackHitStopTimeScale(prototype._playerAttackHitStopTimeScale)
    , _playerAttackHitStopDuration(prototype._playerAttackHitStopDuration)
    , _jumpCounterDamageMultiplier(prototype._jumpCounterDamageMultiplier)
{
}

HRESULT SoulLikeController::Initialize_Prototype()
{
    return GameObject::Initialize_Prototype();
}

HRESULT SoulLikeController::Initialize(void* arg)
{
    CHECK_FAILED(GameObject::Initialize(arg), E_FAIL);
    return S_OK;
}

void SoulLikeController::BeginPlay()
{
    GameObject::BeginPlay();

    if (!_editorStopHandle.IsValid())
    {
        _editorStopHandle = GAME->Get_DelegateHub().OnEditorStopRuntime.Add(
            this,
            &SoulLikeController::Handle_EditorStopRuntime);
    }

    if (_estherManager == nullptr)
        _estherManager = SL_EstherManager::Create(GetSharedPtr<SoulLikeController>());
}

void SoulLikeController::Update(float timeDelta)
{
    GameObject::Update(timeDelta);

    if (_state == BattleState::EnterFx)
    {
        Update_EnterFx(timeDelta);
        return;
    }

    if (_state == BattleState::VictorySlow)
    {
        Update_VictorySlow();
        return;
    }

    if (_state == BattleState::Playing)
    {
        if (_estherManager)
            _estherManager->Update(timeDelta);

        Update_Player(timeDelta);
        Check_BattleEnd();
    }
}

bool SoulLikeController::Begin_Battle()
{
    if (Is_BattleActive())
        return false;

    if (Shared<BattleController> battleController = CLIENT->Get_BattleController())
        battleController->Cancel_BattleEnterPending();

    GAME->Stop_WorldTimeSlow();
    GAME->Set_WorldTimeScale(1.f);

    Remove_SpawnedBattleActors();
    _jumpCounterAttacker.reset();
    _jumpCounterThreatOpen = false;
    _jumpCounterJumpSucceeded = false;
    _jumpCounterCounterWindowOpen = false;
    _jumpCounterAttackStarted = false;
    _jumpCounterDamageApplied = false;
    _playerSpawned = false;
    _enterFxTimer = max(0.f, _enterFxDelay);
    _victorySlowElapsed = 0.f;
    _lastVictoryRewardExp = 0;
    _healingPotionCount = kMaxHealingPotionCount;
    _state = BattleState::EnterFx;

    if (Is_DuallisteBattle())
    {
        Capture_PreSoulLikeBGM();
        Play_DuallisteBGM(DuallisteBGMPhase::Phase1);
    }

    if (_estherManager)
        _estherManager->Reset();

    if (auto screenFxManager = CLIENT->Get_ScreenFxManager())
    {
        screenFxManager->Play_BattleInCounterFx();
        screenFxManager->Transition_To_BattleField();
    }

    return true;
}

void SoulLikeController::End_Battle()
{
    GAME->Stop_WorldTimeSlow();
    GAME->Set_WorldTimeScale(1.f);
    Remove_SpawnedBattleActors();
    Restore_PreSoulLikeBGM();

    _state = BattleState::Finished;
    _enterFxTimer = 0.f;
    _victorySlowElapsed = 0.f;
    _jumpCounterAttacker.reset();
    _jumpCounterThreatOpen = false;
    _jumpCounterJumpSucceeded = false;
    _jumpCounterCounterWindowOpen = false;
    _jumpCounterAttackStarted = false;
    _jumpCounterDamageApplied = false;

    if (_estherManager)
        _estherManager->Reset();

    Release_ControllerRegistration(true);
}

void SoulLikeController::Confirm_Victory()
{
    if (_state != BattleState::Victory)
        return;

    Sync_PlayerResultToWorld();
    End_Battle();
}

void SoulLikeController::Handle_BattleShotNotify(
    const AnimNotifyContext& context, BattleCameraMode cameraMode, const string& presetId,
    const string& clipId, bool useClip, bool restartClip, bool keepCurrentClip)
{
    if (_state != BattleState::Playing)
        return;

    if (context.owner == nullptr || context.model == nullptr)
        return;

    Shared<SL_Maelle> player = _player.lock();
    if (player == nullptr)
        return;

    auto character = dynamic_pointer_cast<Character>(
        context.owner->GetSharedPtr<PlayableParts>()->Get_Owner());

    if (character == nullptr)
        return;

    Shared<SL_Base> subject = dynamic_pointer_cast<SL_Base>(character);
    if (subject == nullptr)
        return;

    if (subject.get() != player.get())
        return;

    Shared<SL_StateMachineCom> stateMachine = player->Get_Component<SL_StateMachineCom>();
    if (stateMachine == nullptr || stateMachine->Get_CurrentSoulLikeState() != SL_PlayerState::Skill)
        return;

    EvtBattlePresentation event{};
    event.cameraMode = cameraMode;
    event.subject = subject;
    event.opponent = Resolve_PlayerCameraOpponent(player);
    event.presetId = presetId;
    event.clipId = clipId;
    event.useClip = useClip;
    event.restartClip = restartClip;
    event.keepCurrentClip = keepCurrentClip;

    if (Shared<SL_Skill> skillState = dynamic_pointer_cast<SL_Skill>(stateMachine->Get_State(SL_PlayerState::Skill)))
        event.skillId = skillState->Get_CurrentSkillKey();

    Request_BattleShotCamera(event);
}
void SoulLikeController::End_BattleShotCamera()
{
    if (!Is_BattleActive())
        return;

    Request_SoulLikeCamera();
}

void SoulLikeController::Apply_SkillCameraPreset(const string& skillKey)
{
    if (_state != BattleState::Playing)
        return;

    if (skillKey.empty())
        return;

    if (!Ensure_SoulLikeCamera())
        return;

    if (Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock())
        soulLikeCamera->Apply_SkillPreset(skillKey);
}

void SoulLikeController::Restore_SkillCameraPreset()
{
    if (Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock())
        soulLikeCamera->Restore_BasePreset();
}

void SoulLikeController::Begin_MonsterAttackCameraPreset(const Shared<SL_Base>& attacker, const string& presetKey)
{
    if (_state != BattleState::Playing)
        return;

    const Shared<SL_Maelle> player = _player.lock();
    if (player == nullptr || player->Get_Transform() == nullptr)
        return;

    if (attacker == nullptr || attacker->Get_Transform() == nullptr)
        return;

    if (presetKey.empty())
        return;

    if (!Ensure_SoulLikeCamera())
        return;

    Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock();
    if (soulLikeCamera == nullptr)
        return;

    soulLikeCamera->SetCombatFocusTransforms(player->Get_Transform(), attacker->Get_Transform());
    soulLikeCamera->Apply_PresetOverride(presetKey);
    GAME->Change_ActiveCamera(static_pointer_cast<Camera>(soulLikeCamera));
    soulLikeCamera->Update(0.f);
    _monsterAttackCameraOwner = attacker;
}

void SoulLikeController::End_MonsterAttackCameraPreset(const Shared<SL_Base>& attacker)
{
    const Shared<SL_Base> currentOwner = _monsterAttackCameraOwner.lock();
    if (currentOwner == nullptr)
        return;

    if (attacker != nullptr && attacker.get() != currentOwner.get())
        return;

    _monsterAttackCameraOwner.reset();

    const Shared<SL_Maelle> player = _player.lock();
    Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock();
    if (player == nullptr || player->Get_Transform() == nullptr || soulLikeCamera == nullptr)
        return;

    soulLikeCamera->SetTargetTransform(player->Get_Transform());
    soulLikeCamera->Restore_BasePreset();
    soulLikeCamera->Update(0.f);
}

void SoulLikeController::Play_PlayerHitFeedback(const SL_DamageInfo& damageInfo)
{
    UNREFERENCED_PARAMETER(damageInfo);

    Play_PlayerParryFeedback();
}

void SoulLikeController::Play_PlayerParryFeedback()
{
    if (_state != BattleState::Playing)
        return;

    if (_playerHitStopDuration > 0.f)
        GAME->Slow_WorldTime(_playerHitStopTimeScale, _playerHitStopDuration);
}

void SoulLikeController::Notify_PlayerParrySuccess()
{
    if (_state != BattleState::Playing)
        return;

    if (_estherManager)
        _estherManager->Add_GaugeFromParry();

    Play_PlayerParryFeedback();
}

void SoulLikeController::Play_PlayerAttackHitFeedback(const SL_DamageInfo& damageInfo)
{
    UNREFERENCED_PARAMETER(damageInfo);

    if (_state != BattleState::Playing)
        return;

    GAME->Play_SoundCue(L"SFX_Hits_Sword");

    if (_playerAttackHitStopDuration <= 0.f)
        return;

    GAME->Slow_WorldTime(_playerAttackHitStopTimeScale, _playerAttackHitStopDuration);
}

void SoulLikeController::Play_DuallisteBGM(DuallisteBGMPhase phase)
{
    if (!Is_BattleActive())
        return;

    if (!Is_DuallisteBattle())
        return;

    const wchar_t* bgm = Resolve_DuallisteBGM(phase);
    if (bgm == nullptr || bgm[0] == L'\0')
        return;

    if (GAME->Get_CurrentBGM() == bgm)
        return;

    GAME->Play_BGM(bgm, 1.f, true, kDuallisteBGMFadeOutDuration);
}

void SoulLikeController::Set_JumpCounterThreat(const Shared<SL_Base>& attacker, bool active)
{
    if (_state != BattleState::Playing)
        return;

    if (active)
    {
        if (attacker == nullptr)
            return;

        _jumpCounterAttacker = attacker;
        _jumpCounterThreatOpen = true;
        _jumpCounterJumpSucceeded = false;
        _jumpCounterCounterWindowOpen = false;
        _jumpCounterAttackStarted = false;
        _jumpCounterDamageApplied = false;
        return;
    }

    Shared<SL_Base> currentAttacker = _jumpCounterAttacker.lock();
    if (attacker != nullptr && currentAttacker != nullptr && attacker.get() != currentAttacker.get())
        return;

    _jumpCounterThreatOpen = false;

    if (!_jumpCounterJumpSucceeded && !_jumpCounterAttackStarted)
    {
        _jumpCounterAttacker.reset();
        _jumpCounterCounterWindowOpen = false;
        _jumpCounterDamageApplied = false;
    }
}

void SoulLikeController::Set_JumpCounterWindowOpen(bool open)
{
    if (_state != BattleState::Playing)
        return;

    if (open)
    {
        if (!_jumpCounterJumpSucceeded || _jumpCounterAttackStarted)
            return;

        _jumpCounterCounterWindowOpen = true;
        return;
    }

    _jumpCounterCounterWindowOpen = false;

    if (!_jumpCounterAttackStarted)
    {
        _jumpCounterAttacker.reset();
        _jumpCounterThreatOpen = false;
        _jumpCounterJumpSucceeded = false;
        _jumpCounterDamageApplied = false;
    }
}

bool SoulLikeController::Try_ConsumeJumpCounterDodge(const Shared<SL_Base>& attacker, const Shared<SL_Maelle>& target)
{
    if (_state != BattleState::Playing)
        return false;

    if (!_jumpCounterJumpSucceeded)
        return false;

    if (target == nullptr || target.get() != _player.lock().get())
        return false;

    const Shared<SL_Base> currentAttacker = _jumpCounterAttacker.lock();
    if (attacker == nullptr || currentAttacker == nullptr || attacker.get() != currentAttacker.get())
        return false;

    return true;
}

void SoulLikeController::Apply_JumpCounterDamage()
{
    if (_state != BattleState::Playing)
        return;

    if (!_jumpCounterAttackStarted || _jumpCounterDamageApplied)
        return;

    const Shared<SL_Maelle> player = _player.lock();
    const Shared<SL_Base> target = _jumpCounterAttacker.lock();
    if (player == nullptr || target == nullptr || target->Is_Dead())
        return;

    SL_DamageInfo damageInfo{};
    if (!SL_DamageSystem::Apply_Damage(
        player,
        player->Get_Stat(),
        target,
        target->Get_Stat(),
        _jumpCounterDamageMultiplier,
        AttackType::Counter,
        damageInfo))
    {
        return;
    }

    _jumpCounterDamageApplied = true;
    target->Apply_Damage(damageInfo);

    // 이팩트
    {
        PlayEffectDesc effectDesc{};
        effectDesc.effectName = L"Double_Distotion.effect.json";
        effectDesc.worldPosition = damageInfo.target->Get_Transform()->Get_WorldPosition() + Vec3(0.f, 1.f, 0.f);
        effectDesc.scale = Vec3(1.f, 1.f, 1.f);
        CLIENT->Play_Effect(effectDesc);
    }

    Play_PlayerAttackHitFeedback(damageInfo);

    _jumpCounterAttacker.reset();
    _jumpCounterThreatOpen = false;
    _jumpCounterJumpSucceeded = false;
    _jumpCounterCounterWindowOpen = false;
    _jumpCounterAttackStarted = false;
}

float SoulLikeController::Get_EstherGaugeRatio() const
{
    return _estherManager ? _estherManager->Get_GaugeRatio() : 0.f;
}

bool SoulLikeController::Notify_EstherAttack(GameObject* notifyOwner)
{
    if (!_estherManager)
        return false;

    return _estherManager->Notify_Attack(notifyOwner);
}

bool SoulLikeController::Is_VictoryCameraBlocked() const
{
     return _state == BattleState::VictorySlow ||
        _state == BattleState::Victory;
}

void SoulLikeController::Request_SoulLikeCamera()
{
    Shared<SL_Maelle> player = _player.lock();
    if (player == nullptr || player->Get_Transform() == nullptr)
        return;

    if (!Ensure_SoulLikeCamera())
        return;

    Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock();
    if (soulLikeCamera == nullptr)
        return;

    soulLikeCamera->SetTargetTransform(player->Get_Transform());
    GAME->Change_ActiveCamera(static_pointer_cast<Camera>(soulLikeCamera));
    soulLikeCamera->Update(0.f);
}

void SoulLikeController::Request_BattleShotCamera(const EvtBattlePresentation& event)
{
    if (_state != BattleState::Playing || event.subject == nullptr)
        return;

    if (!Ensure_BattleTurnCamera())
        return;

    CLIENT->Request_BattleTurn();
    GAME->Get_EventBus().Publish(event);
}

bool SoulLikeController::Ensure_BattleTurnCamera()
{
    if (GAME->Get_CameraByType(ObjectType::BattleTurnCamera) != nullptr)
        return true;

    BattleTurnCamera::BATTLE_TURN_CAMERA_DESC desc{};
    desc.eye = XMFLOAT4(0.f, 8.f, -15.f, 1.f);
    desc.at = XMFLOAT4(0.f, 1.2f, 0.f, 1.f);
    desc.fovy = XMConvertToRadians(45.f);
    desc.nearPlane = 0.1f;
    desc.farPlane = 500.f;
    desc.mouseSensor = 0.15f;
    desc.mouseSensitivity = 0.f;

    Shared<GameObject> cameraObject = nullptr;
    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_BattleTurnCamera"),
            GAME->Current_LevelIndex(),
            TEXT("Layer_Camera"),
            cameraObject,
            &desc),
        false);

    Shared<BattleTurnCamera> battleTurnCamera = dynamic_pointer_cast<BattleTurnCamera>(cameraObject);
    CHECK_NULL(battleTurnCamera, false);

    battleTurnCamera->Try_BeginPlay();
    battleTurnCamera->Reset_RuntimeState();
    _soulLikeBattleTurnCameraLevelIndex = GAME->Current_LevelIndex();
    _createdSoulLikeBattleTurnCamera = true;

    return true;
}

void SoulLikeController::Restore_GameplayCamera()
{
    CLIENT->Request_GamePlay();
}

bool SoulLikeController::Ensure_SoulLikeCamera()
{
    if (Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock())
        return true;

    if (Shared<SoulLikeCamera> soulLikeCamera =
        dynamic_pointer_cast<SoulLikeCamera>(GAME->Get_CameraByType(ObjectType::SoulLikeCamera)))
    {
        _soulLikeCamera = soulLikeCamera;
        _soulLikeCameraLevelIndex = GAME->Current_LevelIndex();
        return true;
    }

    SoulLikeCamera::SOULLIKE_CAMERA_DESC desc{};
    desc.speedPerSec = 10.f;
    desc.degreePerSec = 180.f;
    desc.eye = XMFLOAT4(0.f, 20.f, -7.f, 1.f);
    desc.at = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
    desc.fovy = XMConvertToRadians(60.f);
    desc.nearPlane = 0.1f;
    desc.farPlane = 500.f;
    desc.mouseSensor = 0.06f;
    desc.mouseSensitivity = 0.5f;

    Shared<GameObject> cameraObject = nullptr;
    CHECK_FAILED(
        GAME->Add_GameObject(
            ETOI(LevelType::Static),
            TEXT("GameObject_SoulLikeCamera"),
            GAME->Current_LevelIndex(),
            TEXT("Layer_Camera"),
            cameraObject,
            &desc),
        false);

    Shared<SoulLikeCamera> soulLikeCamera = dynamic_pointer_cast<SoulLikeCamera>(cameraObject);
    CHECK_NULL(soulLikeCamera, false);

    soulLikeCamera->Try_BeginPlay();

    _soulLikeCamera = soulLikeCamera;
    _soulLikeCameraLevelIndex = GAME->Current_LevelIndex();

    return true;
}

void SoulLikeController::Remove_SoulLikeRuntimeCameras()
{
    _monsterAttackCameraOwner.reset();
    Restore_GameplayCamera();

    if (Shared<SoulLikeCamera> soulLikeCamera = _soulLikeCamera.lock())
        GAME->Remove_GameObject(_soulLikeCameraLevelIndex, soulLikeCamera);

    _soulLikeCamera.reset();
    _soulLikeCameraLevelIndex = 0;

    if (_createdSoulLikeBattleTurnCamera)
    {
        if (Shared<Camera> battleTurnCamera = GAME->Get_CameraByType(ObjectType::BattleTurnCamera))
            GAME->Remove_GameObject(_soulLikeBattleTurnCameraLevelIndex, battleTurnCamera);
    }

    _createdSoulLikeBattleTurnCamera = false;
    _soulLikeBattleTurnCameraLevelIndex = 0;
}

Shared<Character> SoulLikeController::Resolve_PlayerCameraOpponent(const Shared<SL_Maelle>& player) const
{
    if (player == nullptr)
        return nullptr;

    Shared<SL_TargetCom> targetCom = player->Get_Component<SL_TargetCom>();
    if (targetCom != nullptr)
    {
        Shared<Character> lockedTarget = targetCom->Get_LockedTarget();
        if (lockedTarget != nullptr)
            return lockedTarget;
    }

    return _monster.lock();
}

bool SoulLikeController::Spawn_Player()
{
    if (_playerPrefab.empty())
        return false;

    CLIENT->Ensure_DefaultEquippedWeapon(HeroId::Maelle);

    const Shared<Player_WorldBase> worldMaelle = CLIENT->Find_Hero(HeroId::Maelle);
    const Shared<PlayerStatCom> worldStat = worldMaelle
        ? worldMaelle->Get_Component<PlayerStatCom>()
        : nullptr;

    const Shared<SkillTreeCom> worldSkillTreeCom = worldMaelle
        ? worldMaelle->Get_Component<SkillTreeCom>()
        : nullptr;

    const uint32 levelIndex = GAME->Current_LevelIndex();

    Shared<GameObject> playerObject = Spawn_Prefab::Prefab(_playerPrefab)
        .AtLevel(levelIndex)
        .InLayer(TEXT("Layer_Player"))
        .Position(_playerSpawnPosition)
        .Rotation(_playerSpawnRotation)
        .Spawn();

    Shared<SL_Maelle> player = dynamic_pointer_cast<SL_Maelle>(playerObject);
    if (player == nullptr)
        return false;

    CHECK_NULL(worldStat, false);

    // 스탯 세팅
    if (FAILED(player->Apply_WorldStatSnapshot(worldStat->Create_BattleSyncSnapshot())))
        return false;

    vector<string> soulLikeSkillKeys = worldStat->Get_SkillKeys();
    if (soulLikeSkillKeys.empty() && worldSkillTreeCom != nullptr)
        soulLikeSkillKeys = worldSkillTreeCom->Get_UnlockedSkillIds();

    soulLikeSkillKeys.erase(
        remove_if(soulLikeSkillKeys.begin(), soulLikeSkillKeys.end(),
            [](const string& skillKey)
            {
                return GradientSkill::Is_GradientSkill(HeroId::Maelle, skillKey);
            }),
        soulLikeSkillKeys.end());

    soulLikeSkillKeys.resize(4);

    player->Apply_WorldSkillKeys(soulLikeSkillKeys);

    const Shared<PlayerStatCom> playerStat = dynamic_pointer_cast<PlayerStatCom>(player->Get_Stat());
    const float playerMaxHp = playerStat ? playerStat->Get_PlayerRuntimeStat().CurMaxHp : _playerMaxHp;
    player->Reset_CombatHp(playerMaxHp);

    _player = player;

    if (!Spawn_Monster(player))
    {
        Remove_SpawnedBattleActors();
        return false;
    }

    _playerSpawned = true;

    Request_SoulLikeCamera();

    return true;
}

bool SoulLikeController::Spawn_Monster(const Shared<SL_Maelle>& player)
{
    if (_monsterPrefab.empty())
        return true;

    if (player == nullptr || player->Get_Transform() == nullptr)
        return false;

    Vec3 forward = player->Get_Transform()->Get_WorldForward();
    forward.y = 0.f;

    if (forward.LengthSquared() <= 0.0001f)
        forward = Vec3::Look;
    else
        forward.Normalize();

    const Vec3 playerPosition = player->Get_Transform()->Get_WorldPosition();
    Vec3 opponentPosition = playerPosition + forward * max(0.f, _monsterSpawnDistance);
    opponentPosition.y = playerPosition.y;

    Vec3 opponentRotation = player->Get_Transform()->Get_WorldRotationEuler();
    opponentRotation.y += 180.f;

    Shared<GameObject> monsterObject = Spawn_Prefab::Prefab(_monsterPrefab)
        .AtLevel(GAME->Current_LevelIndex())
        .InLayer(TEXT("Layer_Monster"))
        .Position(opponentPosition)
        .Rotation(opponentRotation)
        .Spawn();

    Shared<SL_Base> opponent = dynamic_pointer_cast<SL_Base>(monsterObject);
    if (opponent == nullptr)
        return false;

    Shared<MonsterStatCom> monsterStat = dynamic_pointer_cast<MonsterStatCom>(opponent->Get_Stat());
    if (monsterStat != nullptr)
    {
        auto& desc = monsterStat->Get_MonsterDesc();

        monsterStat->Set_RuntimeCombatProfile(
            _monsterMaxHp,
            desc.maxAp,
            desc.physicalAttack,
            desc.physicalDefense,
            desc.speed);
    }

    _monster = opponent;

    return true;
}

void SoulLikeController::Remove_SpawnedBattleActors()
{
    const uint32 levelIndex = GAME->Current_LevelIndex();

    if (Shared<SL_Base> opponent = _monster.lock())
        GAME->Remove_GameObject(levelIndex, opponent);

    if (Shared<SL_Maelle> player = _player.lock())
        GAME->Remove_GameObject(levelIndex, player);

    _monster.reset();
    _player.reset();

    Remove_SoulLikeRuntimeCameras();
}

void SoulLikeController::Update_EnterFx(float timeDelta)
{
    _enterFxTimer -= timeDelta;

    if (_enterFxTimer > 0.f)
        return;

    if (!_playerSpawned && !Spawn_Player())
    {
        Restore_PreSoulLikeBGM();
        _state = BattleState::Finished;
        return;
    }

    _state = BattleState::Playing;
}

void SoulLikeController::Update_Player(float timeDelta)
{
    Shared<SL_Maelle> player = _player.lock();
    if (player == nullptr)
        return;

    Shared<SL_StateMachineCom> stateMachine = player->Get_Component<SL_StateMachineCom>();
    CHECK_NULL(stateMachine);

    Shared<SL_TargetCom> targetCom = player->Get_Component<SL_TargetCom>();
    if (targetCom && GAME->KeyDown(KEY_TYPE::T))
        targetCom->Toggle_LockOn();

    SL_MoveRequest moveRequest{};

    if (GAME->KeyPress(KEY_TYPE::W)) moveRequest.moveZ += 1.f;
    if (GAME->KeyPress(KEY_TYPE::S)) moveRequest.moveZ -= 1.f;
    if (GAME->KeyPress(KEY_TYPE::A)) moveRequest.moveX -= 1.f;
    if (GAME->KeyPress(KEY_TYPE::D)) moveRequest.moveX += 1.f;

    moveRequest.worldDir = Vec3(moveRequest.moveX, 0.f, moveRequest.moveZ);

    if (moveRequest.worldDir.LengthSquared() > 0.f)
        moveRequest.worldDir.Normalize();

    moveRequest.sprint = GAME->KeyPress(KEY_TYPE::SHIFT);
    moveRequest.lockOn = targetCom != nullptr && targetCom->Is_LockOn();

    if (Update_Jump(stateMachine))
        return;

    Update_Dodge(stateMachine, moveRequest);

    // TODO : 포션 먹을 때 간단한 애니메이션 + 이팩트 추가 (이팩트는 그대로)
    if (Update_HealingPotion(player, stateMachine))
        return;

    if (Update_Esther())
        return;

    if (Update_Parry(stateMachine))
        return;

    if (Update_Skill(player, stateMachine))
        return;

    stateMachine->Request_Move(moveRequest, timeDelta);

    Update_BaseAttack(stateMachine);
}

void SoulLikeController::Check_BattleEnd()
{
    Shared<SL_Maelle> player = _player.lock();
    Shared<SL_Base> opponent = _monster.lock();

    if (player == nullptr || player->Is_Dead())
    {
        End_Battle();
        return;
    }

    if (!_monsterPrefab.empty() && (opponent == nullptr || opponent->Is_Dead()))
        Begin_VictorySlow();
}

void SoulLikeController::Update_VictorySlow()
{
    _victorySlowElapsed += GAME->Get_UnscaledTimeDelta();

    if (_victorySlowElapsed < max(0.f, _victorySlowDuration))
        return;

    GAME->Set_WorldTimeScale(0.0001f);
    _victorySlowElapsed = 0.f;
    Begin_Victory();
}

void SoulLikeController::Begin_VictorySlow()
{
    if (_state == BattleState::VictorySlow || _state == BattleState::Victory)
        return;

    _state = BattleState::VictorySlow;
    _victorySlowElapsed = 0.f;

    if (_victorySlowDuration <= 0.f)
    {
        Begin_Victory();
        return;
    }

    GAME->Set_WorldTimeScale(_victorySlowTimeScale);
}

void SoulLikeController::Begin_Victory()
{
    if (_state == BattleState::Victory)
        return;

    GAME->Set_WorldTimeScale(0.0001f);
    Apply_VictoryRewardExp();

    _state = BattleState::Victory;
    _enterFxTimer = 0.f;
    _victorySlowElapsed = 0.f;

    if (const Shared<GameplayUI> gameplayUI = CLIENT->Get_GameplayUI())
    {
        if (GamePlayUI_SL* soulLikeUI = gameplayUI->Get_SoulLikeUI())
            soulLikeUI->Show_Victory();
    }
}

void SoulLikeController::Apply_VictoryRewardExp()
{
    if (_lastVictoryRewardExp > 0)
        return;

    const Shared<SL_Base> monster = _monster.lock();
    const Shared<SL_Maelle> player = _player.lock();
    const Shared<PlayerStatCom> playerStat = player
        ? dynamic_pointer_cast<PlayerStatCom>(player->Get_Stat())
        : nullptr;

    if (monster == nullptr || playerStat == nullptr)
        return;

    const Shared<CombatStatCom> monsterStat = monster->Get_Stat();
    if (monsterStat == nullptr)
        return;

    _lastVictoryRewardExp = monsterStat->Get_StatInfo().exp;

    if (_lastVictoryRewardExp > 0)
        playerStat->Add_Exp(_lastVictoryRewardExp);
}

void SoulLikeController::Sync_PlayerResultToWorld()
{
    const Shared<SL_Maelle> player = _player.lock();
    const Shared<PlayerStatCom> playerStat = player
        ? dynamic_pointer_cast<PlayerStatCom>(player->Get_Stat())
        : nullptr;

    const Shared<Player_WorldBase> worldMaelle = CLIENT->Find_Hero(HeroId::Maelle);
    const Shared<PlayerStatCom> worldStat = worldMaelle
        ? worldMaelle->Get_Component<PlayerStatCom>()
        : nullptr;

    if (playerStat == nullptr || worldStat == nullptr)
        return;

    const PlayerStatCom::PRuntimeCombatStat& runtimeStat = playerStat->Get_PlayerRuntimeStat();
    worldStat->Set_RuntimeProgress(
        runtimeStat.CurLevel,
        runtimeStat.CurExp,
        runtimeStat.CurStatPoint,
        runtimeStat.CurSkillPoint,
        runtimeStat.CurRespecCount);
}

void SoulLikeController::Handle_EditorStopRuntime()
{
    Remove_SpawnedBattleActors();
    Restore_PreSoulLikeBGM();

    _state = BattleState::None;
    _enterFxTimer = 0.f;
    _victorySlowElapsed = 0.f;
    _lastVictoryRewardExp = 0;
    _playerSpawned = false;
    _jumpCounterAttacker.reset();
    _jumpCounterThreatOpen = false;
    _jumpCounterJumpSucceeded = false;
    _jumpCounterCounterWindowOpen = false;
    _jumpCounterAttackStarted = false;
    _jumpCounterDamageApplied = false;

    if (_estherManager)
        _estherManager->Reset();

    GAME->Stop_WorldTimeSlow();
    GAME->Set_WorldTimeScale(1.f);

    if (auto screenFxManager = GAME->Get_ScreenFxManager())
        screenFxManager->Clear_ScreenEffect();

    Release_ControllerRegistration(true);
}

void SoulLikeController::Release_ControllerRegistration(bool removeFromWorld)
{
    if (CLIENT->Get_SoulLikeController().get() == this)
        CLIENT->Set_SoulLikeController(nullptr);

    if (removeFromWorld)
        GAME->Remove_GameObject(GAME->Current_LevelIndex(), GetSharedPtr<GameObject>());
}

void SoulLikeController::Capture_PreSoulLikeBGM()
{
    if (_hasPreSoulLikeBGM)
        return;

    _preSoulLikeBGM = GAME->Get_CurrentBGM();
    _hasPreSoulLikeBGM = true;
}

void SoulLikeController::Restore_PreSoulLikeBGM()
{
    if (!_hasPreSoulLikeBGM)
        return;

    if (_preSoulLikeBGM.empty())
        GAME->Stop_SoundChannel(SoundChannel::BGM, kDuallisteBGMFadeOutDuration);
    else
        GAME->Play_BGM(_preSoulLikeBGM, 1.f, true, kDuallisteBGMFadeOutDuration);

    _preSoulLikeBGM.clear();
    _hasPreSoulLikeBGM = false;
}

bool SoulLikeController::Is_DuallisteBattle() const
{
    return _monsterPrefab == Constants::Prefab::SL_Dualliste;
}

const wchar_t* SoulLikeController::Resolve_DuallisteBGM(DuallisteBGMPhase phase) const
{
    switch (phase)
    {
    case DuallisteBGMPhase::Phase1:
        return Constants::SoundBGM::DuallisteBattlePhase1Main;

    case DuallisteBGMPhase::PhaseTransition:
        return Constants::SoundBGM::DuallisteBattlePhaseTransition;

    case DuallisteBGMPhase::Phase2:
        return Constants::SoundBGM::DuallisteBattlePhase2Main;

    default:
        return nullptr;
    }
}

void SoulLikeController::Update_BaseAttack(const Shared<SL_StateMachineCom>& stateMachine)
{
    if (stateMachine == nullptr)
        return;

    if (!GAME->KeyDown(KEY_TYPE::LBUTTON))
        return;

    if (_jumpCounterCounterWindowOpen &&
        _jumpCounterJumpSucceeded &&
        !_jumpCounterAttackStarted)
    {
        if (stateMachine->Request_JumpCounter())
        {
            _jumpCounterAttackStarted = true;
            _jumpCounterCounterWindowOpen = false;
            return;
        }
    }

    stateMachine->Request_BasicAttack();
}

bool SoulLikeController::Update_Jump(const Shared<SL_StateMachineCom>& stateMachine)
{
    if (stateMachine == nullptr)
        return false;

    if (GAME->KeyDown(KEY_TYPE::SPACE) &&
        _jumpCounterThreatOpen &&
        !_jumpCounterJumpSucceeded &&
        !_jumpCounterAttackStarted)
    {
        if (!stateMachine->Request_Jump())
            return false;

        _jumpCounterJumpSucceeded = true;
        _jumpCounterCounterWindowOpen = false;
        return true;
    }

    return false;
}

bool SoulLikeController::Update_Parry(const Shared<SL_StateMachineCom>& stateMachine)
{
    if (stateMachine == nullptr)
        return false;

    if (!GAME->KeyDown(KEY_TYPE::E))
        return false;

    return stateMachine->Request_Parry();
}

bool SoulLikeController::Update_Skill(const Shared<SL_Maelle>& player, const Shared<SL_StateMachineCom>& stateMachine)
{
    if (player == nullptr || stateMachine == nullptr)
        return false;

    Skill_Data skillData{};
    uint32 skillSlotIndex = UINT32_MAX;

    if (GAME->KeyDown(KEY_TYPE::Z))      skillSlotIndex = 0;
    else if (GAME->KeyDown(KEY_TYPE::X)) skillSlotIndex = 1;
    else if (GAME->KeyDown(KEY_TYPE::C)) skillSlotIndex = 2;
    else if (GAME->KeyDown(KEY_TYPE::V)) skillSlotIndex = 3;

    if (skillSlotIndex == UINT32_MAX)
        return false;

    if (!player->Can_UseSkillSlot(skillSlotIndex, skillData))
        return false;

    if (!stateMachine->Request_Skill(skillData, skillSlotIndex))
        return false;

    player->Start_SkillCooldown(skillSlotIndex, skillData);

    return true;
}

void SoulLikeController::Update_Dodge(const Shared<SL_StateMachineCom>& stateMachine, const SL_MoveRequest& moveRequest)
{
    if (stateMachine == nullptr)
        return;

    if (GAME->KeyDown(KEY_TYPE::SPACE))
    {
        stateMachine->Request_Dodge(moveRequest);
        return;
    }
}

bool SoulLikeController::Update_HealingPotion(const Shared<SL_Maelle>& player,
    const Shared<SL_StateMachineCom>& stateMachine)
{
    if (player == nullptr || stateMachine == nullptr)
        return false;

    if (!GAME->KeyDown(KEY_TYPE::KEY_1))
        return false;

    if (_healingPotionCount == 0)
        return false;

    const SL_PlayerState currentState = stateMachine->Get_CurrentSoulLikeState();
    if (currentState != SL_PlayerState::Idle &&
        currentState != SL_PlayerState::Run &&
        currentState != SL_PlayerState::Sprint)
    {
        return false; // 포션은 이동 가능한 기본 상태에서만
    }

    const Shared<PlayerStatCom> playerStat =
        dynamic_pointer_cast<PlayerStatCom>(player->Get_Stat());

    if (playerStat == nullptr)
        return false;

    CombatStatCom::RuntimeCombatStat& runtimeStat = playerStat->Get_RuntimeStat();
    const float maxHp = max(playerStat->Get_StatInfo().maxHp, 1.f);
    const float curHp = clamp(runtimeStat.CurHp, 0.f, maxHp);

    if (curHp >= maxHp)
        return false;

    runtimeStat.CurHp = clamp(curHp + (maxHp * kHealingPotionHealRatio), 0.f, maxHp);
    --_healingPotionCount;

    return true;
}

bool SoulLikeController::Update_Esther()
{
    if (_estherManager == nullptr)
        return false;

    if (GAME->Is_UIInputEnabled() && GAME->Get_GameplayInputMode() == GameplayInputMode::UIOnly)
        return false;

    if (GAME->KeyDown(KEY_TYPE::KEY_3))
        return _estherManager->Request_Esther(SL_EstherType::GustaveOvercharge);

    if (GAME->KeyDown(KEY_TYPE::KEY_4))
        return _estherManager->Request_Esther(SL_EstherType::LuneImmolation);

    return false;
}

Shared<SoulLikeController> SoulLikeController::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SoulLikeController>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : SoulLikeController");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> SoulLikeController::Clone(void* arg)
{
    auto clone = make_shared<SoulLikeController>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : SoulLikeController");
        return nullptr;
    }

    return clone;
}

void SoulLikeController::Free()
{
    if (_editorStopHandle.IsValid())
    {
        GAME->Get_DelegateHub().OnEditorStopRuntime.Remove(_editorStopHandle);
        _editorStopHandle.Reset();
    }

    if (_estherManager)
    {
        _estherManager->Free();
        _estherManager.reset();
    }

    Release_ControllerRegistration(false);

    GameObject::Free();
}

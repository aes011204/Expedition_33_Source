#include "SoulLikeController.h"

#include "ClientInstance.h"
#include "ClientScreenFxManager.h"
#include "GameplayUI.h"
#include "GamePlayUI_SL.h"
#include "GameInstance.h"
#include "PlayableParts.h"
#include "PlayerStatCom.h"
#include "Player_WorldBase.h"
#include "ScreenFx_Manager.h"
#include "SL_Base.h"
#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"
#include "SL_TargetCom.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"
#include "SkillTreeCom.h"
#include "TransformCom.h"
#include "MonsterStatCom.h"

IMPLEMENT_REFLECTION(SoulLikeController)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.category = "SoulLike";
    info.displayName = "SoulLike Controller";

    PROPERTY_STRING("Player Prefab", _playerPrefab);
    PROPERTY_VEC3("Player Spawn Position", _playerSpawnPosition, 0.1f);
    PROPERTY_VEC3("Player Spawn Rotation", _playerSpawnRotation, 1.f);
    PROPERTY_STRING("Monster Prefab", _monsterPrefab);
    PROPERTY_FLOAT("Monster Spawn Distance", _monsterSpawnDistance, 0.f, 20.f, 0.1f);

    PROPERTY_FLOAT("Player Max HP", _playerMaxHp, 1.f, 9999.f, 1.f);
    PROPERTY_FLOAT("Monster Max HP", _monsterMaxHp, 1.f, 9999.f, 1.f);


    PROPERTY_FLOAT("Enter Fx Delay", _enterFxDelay, 0.f, 5.f, 0.05f);
    PROPERTY_FLOAT("Victory Slow Duration", _victorySlowDuration, 0.f, 5.f, 0.05f);
    PROPERTY_FLOAT("Victory Slow Time Scale", _victorySlowTimeScale, 0.f, 1.f, 0.01f);
    PROPERTY_BOOL("Enable Debug Key 3", _enableDebugKeyStart);

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
}

void SoulLikeController::Update(float timeDelta)
{
    GameObject::Update(timeDelta);

    if (_enableDebugKeyStart && GAME->KeyDown(KEY_TYPE::KEY_3))
        Begin_Battle();

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
        Update_Player(timeDelta);
        Check_BattleEnd();
    }
}

bool SoulLikeController::Begin_Battle()
{
    if (Is_BattleActive())
        return false;

    GAME->Stop_WorldTimeSlow();
    GAME->Set_WorldTimeScale(1.f);

    Remove_SpawnedBattleActors();
    _playerSpawned = false;
    _enterFxTimer = max(0.f, _enterFxDelay);
    _victorySlowElapsed = 0.f;
    _lastVictoryRewardExp = 0;
    _state = BattleState::EnterFx;

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
    Restore_TargetCamera();

    _state = BattleState::Finished;
    _enterFxTimer = 0.f;
    _victorySlowElapsed = 0.f;
}

void SoulLikeController::Confirm_Victory()
{
    if (_state != BattleState::Victory)
        return;

    Sync_PlayerResultToWorld();
    End_Battle();
}

void SoulLikeController::Handle_BattleShotNotify(const AnimNotifyContext& context, BattleCameraMode cameraMode,
    const string& presetId, const string& clipId, bool useClip, bool restartClip, bool keepCurrentClip)
{
    if (!Is_BattleActive())
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

    EvtBattlePresentation event{};
    event.cameraMode = cameraMode;
    event.subject = subject;
    event.opponent = (subject.get() == player.get()) ? Resolve_PlayerCameraOpponent(player) : player;
    event.presetId = presetId;
    event.clipId = clipId;
    event.useClip = useClip;
    event.restartClip = restartClip;
    event.keepCurrentClip = keepCurrentClip;

    Request_BattleShotCamera(event);
}

void SoulLikeController::End_BattleShotCamera()
{
    if (!Is_BattleActive())
        return;

    Request_TargetCamera();
}

void SoulLikeController::Request_TargetCamera()
{
    Shared<SL_Maelle> player = _player.lock();
    if (player == nullptr || player->Get_Transform() == nullptr)
        return;

    CLIENT->Request_GamePlay();

    Shared<TargetCamera> targetCamera =
        dynamic_pointer_cast<TargetCamera>(GAME->Get_CameraByType(ObjectType::TargetCamera));

    if (targetCamera == nullptr)
        return;

    targetCamera->SetTargetTransform(player->Get_Transform());
    targetCamera->Change_Mode(TargetCameraMode::SOULLIKE);
    targetCamera->Update(0.f);
}

void SoulLikeController::Request_BattleShotCamera(const EvtBattlePresentation& event)
{
    UNREFERENCED_PARAMETER(event);

    Request_TargetCamera();
}

void SoulLikeController::Restore_TargetCamera()
{
    CLIENT->Request_GamePlay();

    Shared<TargetCamera> targetCamera =
        dynamic_pointer_cast<TargetCamera>(GAME->Get_CameraByType(ObjectType::TargetCamera));

    if (targetCamera)
        targetCamera->Refresh_TargetTransform();
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

    // 스킬 세팅 -> 일단은 그냥 4개 꺼내오기. 나중에 그냥 직접 하나하나 세팅해서 고정시키는게 나을듯
    vector<string> soulLikeSkillKeys = worldStat->Get_SkillKeys();

    if (soulLikeSkillKeys.empty() && worldSkillTreeCom != nullptr)
        soulLikeSkillKeys = worldSkillTreeCom->Get_UnlockedSkillIds();

    if (soulLikeSkillKeys.size() > 4)
        soulLikeSkillKeys.resize(4);
    else
        soulLikeSkillKeys.resize(4);

    for (string& skillKey : soulLikeSkillKeys)
    {
        if (skillKey.empty())
            continue;

        if (CLIENT->Get_Skill(skillKey) == nullptr)
            skillKey.clear();
    }

    player->Apply_WorldSkillKeys(soulLikeSkillKeys);
    player->Reset_CombatHp(_playerMaxHp);

    _player = player;

    if (!Spawn_Monster(player))
    {
        Remove_SpawnedBattleActors();
        return false;
    }

    _playerSpawned = true;

    Request_TargetCamera();

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
}

void SoulLikeController::Update_EnterFx(float timeDelta)
{
    _enterFxTimer -= timeDelta;

    if (_enterFxTimer > 0.f)
        return;

    if (!_playerSpawned && !Spawn_Player())
    {
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

    // 구르기.. 점프 흠
    Update_Dodge(stateMachine, moveRequest);

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

    GAME->Stop_WorldTimeSlow();
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

    GAME->Slow_WorldTime(_victorySlowTimeScale, _victorySlowDuration);
}

void SoulLikeController::Begin_Victory()
{
    if (_state == BattleState::Victory)
        return;

    GAME->Stop_WorldTimeSlow();
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

    _state = BattleState::None;
    _enterFxTimer = 0.f;
    _victorySlowElapsed = 0.f;
    _lastVictoryRewardExp = 0;
    _playerSpawned = false;

    GAME->Stop_WorldTimeSlow();
    GAME->Set_WorldTimeScale(1.f);

    if (auto screenFxManager = GAME->Get_ScreenFxManager())
        screenFxManager->Clear_ScreenEffect();

}

void SoulLikeController::Update_BaseAttack(const Shared<SL_StateMachineCom>& stateMachine)
{
    if (stateMachine == nullptr)
        return;

    if (GAME->KeyDown(KEY_TYPE::LBUTTON))
        stateMachine->Request_BasicAttack();
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

    if (skillSlotIndex == UINT32_MAX || !player->Try_GetEquippedSkill(skillSlotIndex, skillData))
        return false;

    stateMachine->Request_Skill(skillData, skillSlotIndex);
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

    GameObject::Free();
}

#include "SoulLikeController.h"

#include "ClientInstance.h"
#include "ClientScreenFxManager.h"
#include "GameInstance.h"
#include "PlayableParts.h"
#include "PlayerStatCom.h"
#include "Player_WorldBase.h"
#include "ScreenFx_Manager.h"
#include "SL_Maelle.h"
#include "SL_StateMachineCom.h"
#include "SL_TargetCom.h"
#include "Spawn_Prefab.h"
#include "TargetCamera.h"
#include "SkillTreeCom.h"

IMPLEMENT_REFLECTION(SoulLikeController)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.category = "SoulLike";
    info.displayName = "SoulLike Controller";

    PROPERTY_STRING("Player Prefab", _playerPrefab);
    PROPERTY_VEC3("Player Spawn Position", _playerSpawnPosition, 0.1f);
    PROPERTY_VEC3("Player Spawn Rotation", _playerSpawnRotation, 1.f);
    PROPERTY_FLOAT("Enter Fx Delay", _enterFxDelay, 0.f, 5.f, 0.05f);
    PROPERTY_FLOAT("Player Max HP", _playerMaxHp, 1.f, 9999.f, 1.f);
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
    , _enterFxDelay(prototype._enterFxDelay)
    , _playerMaxHp(prototype._playerMaxHp)
    , _enableDebugKeyStart(prototype._enableDebugKeyStart)
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
            [this]
            {
                Handle_EditorStopRuntime();
            });
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

    _player.reset();
    _playerSpawned = false;
    _enterFxTimer = max(0.f, _enterFxDelay);
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
    Restore_TargetCamera();

    _state = BattleState::Finished;
    _enterFxTimer = 0.f;
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

    if (character.get() != player.get())
        return;

    EvtBattlePresentation event{};
    event.cameraMode = cameraMode;
    event.subject = player;
    event.opponent = nullptr;
    event.presetId = presetId;
    event.clipId = clipId;
    event.useClip = useClip;
    event.restartClip = restartClip;
    event.keepCurrentClip = keepCurrentClip;

    Request_BattleShotCamera(event);
}

HRESULT SoulLikeController::Ready_BattleTurnCamera()
{
    return S_OK;
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
}

void SoulLikeController::Request_BattleShotCamera(const EvtBattlePresentation& event)
{
}

void SoulLikeController::Restore_TargetCamera()
{
    CLIENT->Request_GamePlay();

    Shared<TargetCamera> targetCamera =
        dynamic_pointer_cast<TargetCamera>(GAME->Get_CameraByType(ObjectType::TargetCamera));

    if (targetCamera)
        targetCamera->Refresh_TargetTransform();
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
    _playerSpawned = true;

    Request_TargetCamera();

    return true;
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
    if (GAME->KeyDown(KEY_TYPE::SPACE))
    {
        stateMachine->Request_Dodge(moveRequest);
        return;
    }

    // 스킬
    {
        Skill_Data skillData{};
        uint32 skillSlotIndex = UINT32_MAX;

        if (GAME->KeyDown(KEY_TYPE::Z))      skillSlotIndex = 0;
        else if (GAME->KeyDown(KEY_TYPE::X)) skillSlotIndex = 1;
        else if (GAME->KeyDown(KEY_TYPE::C)) skillSlotIndex = 2;
        else if (GAME->KeyDown(KEY_TYPE::V)) skillSlotIndex = 3;

        if (skillSlotIndex != UINT32_MAX && player->Try_GetEquippedSkill(skillSlotIndex, skillData))
        {
            stateMachine->Request_Skill(skillData, skillSlotIndex);
            return;
        }
    }

    stateMachine->Request_Move(moveRequest, timeDelta);

    if (GAME->KeyDown(KEY_TYPE::LBUTTON))
        stateMachine->Request_BasicAttack();
}

void SoulLikeController::Check_BattleEnd()
{
    Shared<SL_Maelle> player = _player.lock();
    if (player == nullptr || player->Is_Dead())
        End_Battle();
}

void SoulLikeController::Handle_EditorStopRuntime()
{
    const uint32 levelIndex = GAME->Current_LevelIndex();

    if (Shared<SL_Maelle> player = _player.lock())
    {
        GAME->Remove_GameObject(levelIndex, player);
    }

    _player.reset();
    _battleTurnCamera.reset();
    _state = BattleState::None;
    _enterFxTimer = 0.f;
    _playerSpawned = false;

    if (auto screenFxManager = GAME->Get_ScreenFxManager())
        screenFxManager->Clear_ScreenEffect();

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

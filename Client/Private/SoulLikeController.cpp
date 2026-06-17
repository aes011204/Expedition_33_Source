#include "SoulLikeController.h"

#include "ClientInstance.h"
#include "ClientScreenFxManager.h"
#include "GameInstance.h"
#include "PlayableParts.h"
#include "SL_Maelle.h"
#include "Spawn_Prefab.h"

NS_BEGIN(Client)

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
}

void SoulLikeController::Request_TargetCamera()
{
}

void SoulLikeController::Request_BattleShotCamera(const EvtBattlePresentation& event)
{
}

void SoulLikeController::Restore_TargetCamera()
{
}

bool SoulLikeController::Spawn_Player()
{
    if (_playerPrefab.empty())
        return false;

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

    player->Reset_CombatHp(_playerMaxHp);
    _player = player;
    _playerSpawned = true;

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

    player->Clear_CombatLockIfAnimFinished();

    Vec3 moveDir = Vec3::Zero;

    if (GAME->KeyPress(KEY_TYPE::W)) moveDir.z += 1.f;
    if (GAME->KeyPress(KEY_TYPE::S)) moveDir.z -= 1.f;
    if (GAME->KeyPress(KEY_TYPE::A)) moveDir.x -= 1.f;
    if (GAME->KeyPress(KEY_TYPE::D)) moveDir.x += 1.f;

    if (moveDir.LengthSquared() > 0.f)
    {
        moveDir.Normalize();
        player->Apply_MoveInput(moveDir, GAME->KeyPress(KEY_TYPE::SHIFT), timeDelta);
    }
    else
    {
        player->Stop_Move();
    }

    if (GAME->KeyDown(KEY_TYPE::SPACE))
    {
        player->Set_InvincibleTime(0.35f);
        player->Play_SoulLikeAnim(SL_Maelle::SoulLikeAnim::Dodge, 0.05f, false);
    }

    if (GAME->KeyDown(KEY_TYPE::LBUTTON))
    {
        player->Set_CombatLocked(true);
        player->Play_SoulLikeAnim(SL_Maelle::SoulLikeAnim::Attack, 0.05f, false);
    }
}

void SoulLikeController::Check_BattleEnd()
{
    Shared<SL_Maelle> player = _player.lock();
    if (player == nullptr || player->Is_Dead())
        End_Battle();
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
    GameObject::Free();
}

NS_END

#pragma once

#include "Battle_Event.h"
#include "Delegate.h"
#include "GameObject.h"
#include "SL_Types.h"

NS_BEGIN(Client)

class SL_Base;
class SL_Maelle;

class SL_StateMachineCom;

class SoulLikeController final : public GameObject
{
    GENERATED_GAMEOBJECT(SoulLikeController, ObjectType::SoulLikeController)

public:
    enum class BattleState
    {
        None,
        EnterFx,
        Playing,
        VictorySlow,
        Victory,
        Finished,
    };

public:
    SoulLikeController(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SoulLikeController(const SoulLikeController& prototype);
    ~SoulLikeController() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Update(float timeDelta) override;

public:
    bool Begin_Battle();
    void End_Battle();
    void Confirm_Victory();

    bool Is_BattleActive() const
    {
        return _state == BattleState::EnterFx ||
            _state == BattleState::Playing ||
            _state == BattleState::VictorySlow ||
            _state == BattleState::Victory;
    }

    void Handle_BattleShotNotify(
        const AnimNotifyContext& context,
        BattleCameraMode cameraMode,
        const string& presetId,
        const string& clipId,
        bool useClip,
        bool restartClip,
        bool keepCurrentClip);

    Shared<SL_Maelle> Get_Player() const { return _player.lock(); }
    Shared<SL_Base> Get_Monster() const { return _monster.lock(); }
    uint32 Get_LastVictoryRewardExp() const { return _lastVictoryRewardExp; }

    void End_BattleShotCamera();

private:
    void    Request_TargetCamera();
    void    Request_BattleShotCamera(const EvtBattlePresentation& event);
    void    Restore_TargetCamera();
    Shared<Character> Resolve_PlayerCameraOpponent(const Shared<SL_Maelle>& player) const;

private:
    bool Spawn_Player();
    bool Spawn_Monster(const Shared<SL_Maelle>& player);
    void Remove_SpawnedBattleActors();
    void Update_EnterFx(float timeDelta);
    void Update_VictorySlow();
    void Check_BattleEnd();
    void Begin_VictorySlow();
    void Begin_Victory();
    void Apply_VictoryRewardExp();
    void Sync_PlayerResultToWorld();
    void Handle_EditorStopRuntime();

    void Update_Player(float timeDelta);
    void Update_BaseAttack(const Shared<SL_StateMachineCom>& stateMachine);
    bool Update_Skill(const Shared<SL_Maelle>& player, const Shared<SL_StateMachineCom>& stateMachine);
    void Update_Dodge(const Shared<SL_StateMachineCom>& stateMachine, const SL_MoveRequest& moveRequest);

private:
    string _playerPrefab = "SL_Maelle";
    Vec3   _playerSpawnPosition = Vec3(0.f, 0.f, 0.f);
    Vec3   _playerSpawnRotation = Vec3(0.f, 180.f, 0.f);

    string _monsterPrefab = "";
    float  _monsterSpawnDistance = 3.5f;

    float  _playerMaxHp = 100.f;
    float  _monsterMaxHp = 500.f;

    float  _enterFxDelay = 0.6f;
    float  _victorySlowDuration = 0.6f;
    float  _victorySlowTimeScale = 0.12f;
    bool   _enableDebugKeyStart = true;

    BattleState _state = BattleState::None;
    Weak<SL_Maelle> _player{};
    Weak<SL_Base> _monster{};

    float _enterFxTimer = 0.f;
    float _victorySlowElapsed = 0.f;
    uint32 _lastVictoryRewardExp = 0;
    bool  _playerSpawned = false;
    FDelegateHandle _editorStopHandle{};

public:
    static Shared<SoulLikeController> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

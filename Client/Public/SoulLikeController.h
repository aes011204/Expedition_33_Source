#pragma once

#include "Battle_Event.h"
#include "Delegate.h"
#include "GameObject.h"
#include "SL_Types.h"

NS_BEGIN(Client)

class SL_Base;
class SL_Maelle;
class SoulLikeCamera;
class SL_StateMachineCom;
class SL_EstherManager;

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

    enum class DuallisteBGMPhase
    {
        Phase1,
        PhaseTransition,
        Phase2,
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
    void Set_BattleSpawnPoseOverride(
        const Vec3& playerPosition,
        const Vec3& playerRotation,
        const Vec3& monsterPosition,
        const Vec3& monsterRotation);
    void Clear_BattleSpawnPoseOverride();
    void Set_NextMonsterActionDelay(float delaySec);
    bool Is_DebugKeyStartEnabled() const { return _enableDebugKeyStart; }

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
    void Apply_SkillCameraPreset(const string& skillKey);
    void Restore_SkillCameraPreset();
    void Begin_MonsterAttackCameraPreset(const Shared<SL_Base>& attacker, const string& presetKey);
    void End_MonsterAttackCameraPreset(const Shared<SL_Base>& attacker);

    void Play_PlayerHitFeedback(const SL_DamageInfo& damageInfo);
    void Play_PlayerParryFeedback();
    void Notify_PlayerParrySuccess();
    void Play_PlayerAttackHitFeedback(const SL_DamageInfo& damageInfo);
    void Start_DuallistePhase1BGM();
    void Play_DuallisteBGM(DuallisteBGMPhase phase);

    void Set_JumpCounterThreat(const Shared<SL_Base>& attacker, bool active);
    void Set_JumpCounterWindowOpen(bool open);
    bool Try_ConsumeJumpCounterDodge(const Shared<SL_Base>& attacker, const Shared<SL_Maelle>& target);
    void Apply_JumpCounterDamage();

    // 패링 시 뒤로 밀리기
    float Get_PlayerHitPushDistance() const { return _playerHitPushDistance; }
    float Get_PlayerHitPushDuration() const { return _playerHitPushDuration; }

    uint32 Get_HealingPotionCount() const { return _healingPotionCount; }
    bool   Try_ApplyHealingPotion(const Shared<SL_Maelle>& player);

    float Get_EstherGaugeRatio() const;

    bool    Notify_EstherAttack(GameObject* notifyOwner);

    bool    Is_VictoryCameraBlocked() const;


private:
    void    Request_SoulLikeCamera(bool alignBehindPlayer = false);
    void    Request_BattleShotCamera(const EvtBattlePresentation& event);
    void    Restore_GameplayCamera();
    bool    Ensure_SoulLikeCamera();
    bool    Ensure_BattleTurnCamera();
    void    Remove_SoulLikeRuntimeCameras();
    Shared<Character> Resolve_PlayerCameraOpponent(const Shared<SL_Maelle>& player) const;

private:
    bool Spawn_Player();
    bool Spawn_Monster(const Shared<SL_Maelle>& player);
    void Remove_SpawnedBattleActors();
    void Update_EnterFx(float timeDelta);
    void Update_MonsterActionDelay(float timeDelta);
    void Update_VictorySlow();
    void Check_BattleEnd();
    void Begin_VictorySlow();
    void Begin_Victory();
    void Apply_VictoryRewardExp();
    void Sync_PlayerResultToWorld();
    void Handle_EditorStopRuntime();
    void Release_ControllerRegistration(bool removeFromWorld);

    void Capture_PreSoulLikeBGM();
    void Restore_PreSoulLikeBGM();
    bool Is_DuallisteBattle() const;
    const wchar_t* Resolve_DuallisteBGM(DuallisteBGMPhase phase) const;

    void Update_Player(float timeDelta);
    void Update_BaseAttack(const Shared<SL_StateMachineCom>& stateMachine);
    bool Update_Jump(const Shared<SL_StateMachineCom>& stateMachine);
    bool Update_Parry(const Shared<SL_StateMachineCom>& stateMachine);
    bool Update_Skill(const Shared<SL_Maelle>& player, const Shared<SL_StateMachineCom>& stateMachine);
    void Update_Dodge(const Shared<SL_StateMachineCom>& stateMachine, const SL_MoveRequest& moveRequest);
    bool Update_HealingPotion(const Shared<SL_Maelle>& player, const Shared<SL_StateMachineCom>& stateMachine);
    bool Update_Esther();

private:
    Shared<SL_EstherManager> _estherManager{};

    string _playerPrefab = "SL_Maelle";
    Vec3   _playerSpawnPosition = Vec3(0.f, 0.f, 0.f);
    Vec3   _playerSpawnRotation = Vec3(0.f, 180.f, 0.f);

    string _monsterPrefab = "";
    float  _monsterSpawnDistance = 3.5f;
    bool   _hasBattleSpawnPoseOverride = false;
    Vec3   _overridePlayerSpawnPosition = Vec3::Zero;
    Vec3   _overridePlayerSpawnRotation = Vec3::Zero;
    Vec3   _overrideMonsterSpawnPosition = Vec3::Zero;
    Vec3   _overrideMonsterSpawnRotation = Vec3::Zero;

    float  _playerMaxHp = 100.f;
    float  _monsterMaxHp = 500.f;

    float  _enterFxDelay = 0.6f;
    float  _victorySlowDuration = 0.6f;
    float  _victorySlowTimeScale = 0.12f;
    bool   _enableDebugKeyStart = true;

    BattleState _state = BattleState::None;
    Weak<SL_Maelle> _player{};
    Weak<SL_Base> _monster{};
    Weak<SoulLikeCamera> _soulLikeCamera{};
    Weak<SL_Base> _monsterAttackCameraOwner{};

    float _enterFxTimer = 0.f;
    float _victorySlowElapsed = 0.f;
    uint32 _lastVictoryRewardExp = 0;
    wstring _preSoulLikeBGM{};
    bool _hasPreSoulLikeBGM = false;
    bool  _playerSpawned = false;
    float _nextMonsterActionDelay = 0.f;
    float _monsterActionDelayTimer = 0.f;
    FDelegateHandle _editorStopHandle{};
    uint32 _soulLikeCameraLevelIndex = 0;
    uint32 _soulLikeBattleTurnCameraLevelIndex = 0;
    bool _createdSoulLikeBattleTurnCamera = false;

private:
    /* 패링 */
    float _playerHitPushDistance = 0.65f;
    float _playerHitPushDuration = 0.18f;

    // 공격 받았을 때
    float _playerHitStopTimeScale = 0.05f;
    float _playerHitStopDuration = 0.08f;

    // 공격할 때
    float _playerAttackHitStopTimeScale = 0.08f;
    float _playerAttackHitStopDuration = 0.04f;

    // 점프 카운터
    Weak<SL_Base> _jumpCounterAttacker{};
    bool _jumpCounterThreatOpen = false;
    bool _jumpCounterJumpSucceeded = false;
    bool _jumpCounterCounterWindowOpen = false;
    bool _jumpCounterAttackStarted = false;
    bool _jumpCounterDamageApplied = false;
    float _jumpCounterDamageMultiplier = 1.f;

    // 포션
    static constexpr uint32 kMaxHealingPotionCount = 5;
    static constexpr float  kHealingPotionHealRatio = 0.3f; // 30퍼센트
    uint32 _healingPotionCount = kMaxHealingPotionCount;

public:
    static Shared<SoulLikeController> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END

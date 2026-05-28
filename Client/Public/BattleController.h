#pragma once

#include "Base.h"
#include "Delegate.h"
#include "BattleAction.h"
#include "Battle_Enum.h"
#include "BattleReactionController.h"

NS_BEGIN(Engine)
class Character;
class Camera;
class BattleAction;
NS_END

NS_BEGIN(Client)

class SkillScript;

class BattleActionExecutor;

class CombatStatCom;
class BuffSystem;
class BattleEffectResolver;
class BattleStateMachine;
class BattleTurnCamera;
class BattleSpawnRegistry;

class BattleController : public Base
{
public:
    using ReactionType = BattleReactionType;
    using ReactionResult = BattleReactionController::BattleReactionInputResult;

public: /* Delegate 선언 */
    // 전투 시작
    DECLARE_DELEGATE(FOnBattleStarted, uint32, bool);
    // 전투 종료
    DECLARE_DELEGATE(FOnBattleEnded, EBattleEndResult);
    // 턴 시작
    DECLARE_DELEGATE(FOnTurnStarted, const BattleChar&, BattleTurnState);
    // 턴 종료
    DECLARE_DELEGATE(FOnTurnEnded, const BattleChar&);
    // 타겟 전환
    DECLARE_DELEGATE(FOnTargetPreviewChanged, const BattleChar*);
    // 공격 시작
    DECLARE_DELEGATE(FOnActionStarted, const BattleAction&);
    // Reaction (Dodge, Parry)
    DECLARE_DELEGATE(FOnReactionSucceeded, ReactionType,const vector<BattleChar>&);

public:
    BattleController();
    ~BattleController() override;
public:
    HRESULT Initialize();
    void Update(float timeDelta);

    void Run_EnemyTurn();

    // 진입 시 데이터만 기록 -> 실제 전환은 슬로우모션 끝난 후 Complete_BattleFiled 함수
    void OnEnterBattleField(
        uint32 FieldID,
        bool IsFirstHero,
        const vector<HeroId>& heroIds,
        const vector<string>& battleMonsterPrefabNames,
        const Shared<GameObject>& sourceObject = nullptr);

    EBattleEndResult OnExitBattleField(EBattleEndResult result);

    void Set_BattleFSM();
    void PlayerStartAnim();
    void HitAction(BattleChar target, AttackType attackType);
    void ExecuteAction(const BattleAction& turnAction); //실제로 선택한 행동을 실행하는 함수
    void ExecuteActionRequest(const ActionRequest& request);

    void EndTurn();
    void StartTurn();

    EAPChangeReason FinishBattle();

    void Create_BattleChar(const vector<HeroId>& heroIds, vector<string>& battleMonsterPrefabNames);
    void Build_InitialTurnQueue(bool IsFirstHero);
    // 부활하면 턴 큐 한번 다시 리빌드
    void Rebuild_TurnQueue(const BattleChar& revivedActor);
    void SetCharInitPos();

    void OnActionEndNotify();

    void Spawn_BattleChar(bool IsHero, string prefabName, Vec3 pos);
public:
    Shared<BattleStateMachine> Get_BattleFSM() { return _battleFSM; };
    BattleAction& Get_TurnAction() { return _turnAction; }
    const BattleAction& Get_TurnAction() const { return _turnAction; }
    ActionRequest& Get_TurnActionRequest() { return _turnActionRequest; }
    const ActionRequest& Get_TurnActionRequest() const { return _turnActionRequest; }
    // Action& Get_TurnAction() { return _turnAction; }
    void    Request_WidePresentationMode();

    vector<BattleChar> Get_AliveEnemies();
    vector<BattleChar> Get_AliveHeroes();

    bool Is_BattleStarted() { return _isBattleActive; }
    bool Is_BattleEntering() const { return _isBattleEntering; } // 배틀 진입 시작

    Shared<SkillScript> Get_CurrentScript() const { return _curScript; }
    void Clear_CurrentScript() { _curScript.reset(); }

    const vector<BattleChar>& Get_BattleHeroes() const { return _heroes; }

public:
    void End_FirstAnim();
    void Handle_EditorStopRuntime();
   // void Handle_TargetPreviewChanged(const BattleChar* target);
    HRESULT Ready_Delegates();

    //리액션 인풋
    void    OpenReactionWindow(ReactionType type);
    void    CloseReactionWindow(ReactionType type); //일단 이건 쓰지 마
    void    ClearReactionState();
    void    UpdateReactionInput();

    bool    Try_ConsumeReactionGuard(const BattleChar& target);

public: /* 타겟팅 중인 적 세팅 */
    void                Set_TargetPreview(const BattleChar& target);
    void                Clear_TargetPreview();
    const BattleChar*   Get_TargetPreview() const;

public:
    bool                Is_PlayerTurnActive() const;
    BattleTurnState     Get_CurrentTurnState() const;

    Shared<BattleTurnCamera> Get_BattleTurnCamera() const { return _battleTurnCamera; }

    void                StartCounter(ModelCom* sourceModel, const BattleSlowDesc& slowDesc, float counterPlayRate);
    void                ApplyCounterDamage(ModelCom* sourceModel);

public:
    // Free Aim은 턴 소모 X
    void Begin_FreeAimShot(const BattleAction& shotAction);
    void Finish_FreeAimShot();
    bool Is_FreeAimShotPlaying() const { return _isFreeAimShotPlaying; }

    // 초상화 세팅
    vector<BattleChar> Get_TurnOrderPreview(uint32 maxCount = 8) const;

     // 전투 종료
    void Confirm_Victory();

    // 데미지 후 전투 진행 규칙을 한 곳에서 처리한다.
    void Handle_DamageApplied(const BattleChar& damagedTarget, float damage, AttackType attackType);

     // 현재 턴 액터가 죽으면 턴 종료를 예약한다.
    void Try_RequestEndTurnOnCurrentActorDeath(const BattleChar& damagedTarget, float damage);

    // 마지막 데미지 들어가면서 슬로우 시작
    void Try_BeginVictorySlowOnDamage(const BattleChar& damagedTarget, float damage);
    bool Can_TriggerVictory() const;

    uint32 Get_LastVictoryRewardExp() const { return _lastVictoryRewardExp; }

    void Set_FreeAimHitTarget(const BattleChar& target);
    void Clear_FreeAimHitTarget();
    bool Has_FreeAimHitTarget() const { return _hasFreeAimHitTarget; }
    const BattleChar* Get_FreeAimHitTarget() const { return _hasFreeAimHitTarget ? &_freeAimHitTarget : nullptr; }

private:
    HRESULT Ready_BattleTurnCamera();
    void    Activate_BattleTurnCamera();
    void    Restore_WorldCamera();
    void    Clear_BattleRuntimeState();

    void    Complete_BattleField();          // 2초 슬로우모션이 끝난 뒤 실제 PB/MB 생성과 턴 시작을 처리한다.
    void    Clear_BattleEnterPending();      // 전투 진입 대기 데이터만 정리

private:
    PlayerCharacterType To_PlayerCharacterType(HeroId heroId);

public: /* Delegate */
   // FDelegateHandle         _targetPreviewChangedHandle;
   // FOnBattleStarted        OnBattleStarted;
   // FOnBattleEnded          OnBattleEnded;
   // FOnTurnStarted          OnTurnStarted;
   // FOnTurnEnded            OnTurnEnded;
   // FOnTargetPreviewChanged OnTargetPreviewChanged;
   // FOnActionStarted        OnActionStarted;
    FOnBattleStarted                OnBattleStarted;        // 배틀 시작
    FOnBattleEnded                  OnBattleEnded;          // 배틀 끝
    FOnTurnStarted                  OnTurnStarted;          // 턴 시작
    FOnTurnEnded                    OnTurnEnded;            // 턴 끝
    FOnTargetPreviewChanged         OnTargetPreviewChanged; // Targeting 중일 때
    FOnActionStarted                OnActionStarted;        // 공격, 스킬 시작
    FOnReactionSucceeded            OnReactionSucceeded;    // 회피, 패링 성공

    FDelegateHandle                 _damageAppliedHandle{};
    FDelegateHandle                 _editorStopHandle = {};

private:
    BattleChar                      _currentTurnActor = {};
    BattleAction                   _turnAction = {};
    ActionRequest                  _turnActionRequest = {};

    Shared<SkillScript>             _curScript = {};

    bool _isBattleActive = { false };
    bool _isBattleEntering = { false };
    bool _hasRequestedBattleEnterCamera = false;

    int currentTurnIndex = {};

    bool _flag = { false };
private:
    BattleChar  _targetPreview = {};
    bool        _hasTargetPreview = false;

    BattleChar  _freeAimHitTarget{};
    bool        _hasFreeAimHitTarget = false;

private:
    void Execute_BaseAttack();
    void Execute_FreeAimShoot();
    void Execute_Skill();
    void Execute_Item();

private: /* 전투 AP */
    void Apply_BaseAttackAP(const BattleChar& actor); // Base Attack시 얻는 AP
    bool Apply_FreeAimShootAP(const BattleChar& actor); // Free Aim시 사용되는 AP

    void Apply_VictoryRewardExp(uint32 rewardExp);

private:
    bool _battleEnded = {};
    bool _pendingNextTurn = {};

    // 현재 턴을 진행중인 적이 죽으면.
    bool _requestEndTurnOnCurrentActorDeath = false; 

public:
    struct Compare
    {
        bool operator()(TurnEntry& a, TurnEntry& b)
        {
            if (a.nextTime > b.nextTime)
                // Ultimate_Data    _selected_ultimate
                return true;
            return false;
        }
    };

private:
    priority_queue<TurnEntry, vector<TurnEntry>, Compare> _turnQueue;
    float _battleTime = {};
    vector<BattleChar> _heroes;
    vector<BattleChar> _enemies;

    Shared<BattleStateMachine> _battleFSM;
    Shared<BuffSystem> _buffSystem;
    Shared<BattleActionExecutor> _battleActionExe;

    // 전투 시작 시 스폰되는 객체들 저장소
    Shared<BattleSpawnRegistry> _battleSpawnRegistry;
    BattleReactionController    _reactionController{};

    bool _firstAnimflag = { false };
    int  _firstAnimCount = {0};

    bool _isActionExecuting = false;


private:
    Shared<BattleTurnCamera> _battleTurnCamera;
  //  Shared<Camera> _worldCamera;    // 전투 시작 전 카메라 저장

private:  // 슬로우 처리
   // float _battleEnterElapsed = 0.f;
   // float _battleEnterDuration = 3.f;
   // float _battleEnterWorldTimeScale = 0.25f;


private:
    // 슬로우 처리
    BattleSlowDesc _battleEnterSlow{ 0.f, 2.f, 0.25f };
    BattleSlowDesc _victorySlow{ 0.f, 2.f, 0.12f };

    // 배틀 카메라는 이미 켰고, UI 전환 종료만 기다리는 상태
    bool _isWaitingBattleTransition = false;

    bool _isPendingVictorySlow = false;
    bool _isWaitingVictoryConfirm = false;

private:
    uint32  _pendingBattleFieldID = 0;
    bool    _pendingBattleIsFirstHero = false;
    vector<HeroId> _pendingBattleHeroIds = {};
    vector<string> _pendingBattleMonsterPrefabNames = {};

    bool _isFreeAimShotPlaying = false;
    BattleAction _freeAimShotAction {};

    uint32 _lastVictoryRewardExp = 0;

public:
    static Shared<BattleController> Create();
    void Free() override;

public:
};

NS_END

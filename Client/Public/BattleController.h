#pragma once

#include "Base.h"
#include "BattleMonsterSpawnDesc.h"
#include "Delegate.h"
#include "BattleAction.h"
#include "Battle_Enum.h"
#include "BattleReactionController.h"
#include "BattleHitSequenceRunner.h"
#include "BattleQTEController.h"

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
class SkillTreeCom;

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
    // AP ++
    DECLARE_DELEGATE(FOnBattleApChanged, const BattleChar&, uint32, EAPChangeReason);
    // QTE UI 시작/결과
    DECLARE_DELEGATE(FOnQTEStarted, const BattleQTERequest&);
    DECLARE_DELEGATE(FOnQTEResolved, BattleQTEResult);


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
        bool isPlayerAdvantage,
        const vector<HeroId>& heroIds,
        const vector<BattleMonsterSpawnDesc>& battleMonsterSpawnDescs,
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

    void Create_BattleChar(const vector<HeroId>& heroIds, const vector<BattleMonsterSpawnDesc>& battleMonsterSpawnDescs);
    void Build_InitialTurnQueue(bool isPlayerAdvantage);
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
    vector<BattleChar> Get_AllEnemies() { return _enemies; };
    vector<BattleChar> Get_AliveHeroes();

    bool Is_BattleStarted() { return _isBattleActive; }
    bool Is_BattleEntering() const { return _isBattleEntering; } // 배틀 진입 시작

    Shared<SkillScript> Get_CurrentScript() const { return _curScript; }
    void Clear_CurrentScript() { _curScript.reset(); }

    const vector<BattleChar>& Get_BattleHeroes() const { return _heroes; }
    const vector<BattleChar>& Get_BattleEnemies() const { return _enemies; }

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

    void                StartCounter(ModelCom* sourceModel);
    void                ApplyCounterDamage(ModelCom* sourceModel);

    // 카운터 시작 시 타임 관련
    void                BeginCounterTimeSection(ModelCom* sourceModel, float worldTimeScale, float counterFrameSpeed);
    void                EndCounterTimeSection(ModelCom* sourceModel);

public:
    // 부활
    void Revive_BattleMon(const BattleChar& battleChar, float hp);
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

    void Set_FreeAimHitTarget(const BattleFreeAimHitTarget& target);
    void Clear_FreeAimHitTarget();
    bool Has_FreeAimHitTarget() const { return _hasFreeAimHitTarget; }
    const BattleFreeAimHitTarget* Get_FreeAimHitTarget() const
    {
        return _hasFreeAimHitTarget ? &_freeAimHitTarget : nullptr;
    }

     // 스킬 스크립트가 다단히트를 예약할 때 사용
    BattleHitSequenceRunner& Get_HitSequenceRunner() { return _hitSequenceRunner; }

    // QTE
    void Begin_QTE(const BattleQTERequest& request);
    void Resolve_QTE(BattleQTEResult result);
    void Fail_QTEIfActive();
    void Clear_QTEState();
    
    bool Is_QTEActive() const;
    bool Has_LastQTEResult() const;
    BattleQTEResult Get_LastQTEResult() const;

    // 죽은놈, 부활한놈 충돌체 처리
    void Handle_BattleTargetDeathCollision(const BattleChar& deadTarget);
    void Restore_BattleTargetCollision(const BattleChar& revivedTarget);

    // 버프
    void Apply_Buff(const BattleChar& target, Buffs buffId, uint32 stack = 1, int32 durationTurn = 0);
    bool Has_Buff(const BattleChar& target, Buffs buffId) const;
    Shared<BuffSystem> Get_BuffSystem() const { return _buffSystem; }

private:
    HRESULT Ready_BattleTurnCamera();
    void    Activate_BattleTurnCamera();
    void    Restore_WorldCamera();
    void    Clear_BattleRuntimeState();
    float   Resolve_PrefabGroundOffsetY(const Shared<GameObject>& actor) const;

    void    Complete_BattleField();          // 2초 슬로우모션이 끝난 뒤 실제 PB/MB 생성과 턴 시작을 처리한다.
    void    Clear_BattleEnterPending();      // 전투 진입 대기 데이터만 정리

    // 몬스터 약점공격 보너스 데미지
    bool    Try_ApplyFreeAimWeakPointBonus(const BattleFreeAimHitTarget& hitTarget, const BattleChar& attacker);

private:
    // World의 SkillTree 상태를 읽어서 battle에서 사용할 기본 스킬 3개 세팅
    // TODO : 나중에는 실제 장착되는 스킬 세팅되게 해줄거같은데. 그러면 그 때 수정
    void Configure_BattlePlayerSkills(
        HeroId heroId,
        const Shared<CombatStatCom>& battleStat,
        const Shared<SkillTreeCom>& worldSkillTreeCom);

    // Q/W/E가 소비할 기본 스킬 일단은 고정.
    vector<string> Get_DefaultBattleSkillKeys(HeroId heroId);

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
    FOnBattleApChanged              OnBattleApChanged;

    FOnQTEStarted                   OnQTEStarted;
    FOnQTEResolved                  OnQTEResolved;

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

    BattleFreeAimHitTarget _freeAimHitTarget{};
    bool _hasFreeAimHitTarget = false;

private:
    void Execute_BaseAttack();
    void Execute_Skill();
    void Execute_Item();

private: /* 전투 AP */
    void Apply_BaseAttackAP(const BattleChar& actor); // Base Attack시 얻는 AP
    bool Apply_FreeAimShootAP(const BattleChar& actor); // Free Aim시 사용되는 AP

    void Apply_VictoryRewardExp(uint32 rewardExp);

    void Play_EnemyTargetedStart(const BattleChar& target);
    void Play_EnemyTargetedEnd();


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

    Shared<BattleStateMachine>   _battleFSM;
    Shared<BuffSystem>           _buffSystem;
    Shared<BattleActionExecutor> _battleActionExe;

    Shared<BattleSpawnRegistry> _battleSpawnRegistry;   // 전투 시작 시 스폰되는 객체들 저장소
    Shared<BattleQTEController> _qteController{};
    BattleReactionController    _reactionController{};
    BattleHitSequenceRunner     _hitSequenceRunner{};   // 루네 천둥번개 같은 다단히트 관리

    bool _firstAnimflag = { false };
    int  _firstAnimCount = {0};

    bool _isActionExecuting = false;


private:
    Shared<BattleTurnCamera> _battleTurnCamera;
    //  Shared<Camera> _worldCamera;    // 전투 시작 전 카메라 저장

private:
    // 슬로우 처리
    BattleSlowDesc _battleEnterSlow{ 0.f, 2.f, 0.25f };
    BattleSlowDesc _victorySlow{ 0.f, 2.f, 0.12f };

    // 배틀 카메라는 이미 켰고, UI 전환 종료만 기다리는 상태
    bool _isWaitingBattleTransition = false;

    bool _isPendingVictorySlow = false;
    bool _isWaitingVictoryConfirm = false;
    bool _hasRequestedTransitionCapture = false;

private:
    uint32  _pendingBattleFieldID = 0;
    bool    _pendingBattleIsPlayerAdvantage = false;
    vector<HeroId> _pendingBattleHeroIds = {};
    vector<BattleMonsterSpawnDesc> _pendingBattleMonsterSpawnDescs = {};
    int32   _pendingBattleStartSpawnerId = 0;

    bool _isFreeAimShotPlaying = false;
    BattleAction _freeAimShotAction {};

    uint32 _lastVictoryRewardExp = 0;

    BattleChar  _enemyTurnTargetedActor = {};
    bool        _hasEnemyTurnTargetedActor = false;

public:
    static Shared<BattleController> Create();
    void Free() override;

public:
};

NS_END

#pragma once

#include "Base.h"
#include "BattleMonsterSpawnDesc.h"
#include "Delegate.h"
#include "BattleAction.h"
#include "Battle_Enum.h"
#include "BattleReactionController.h"
#include "BattleHitSequenceRunner.h"
#include "BattleQTEController.h"
#include "CameraPresetTypes.h"
#include "Event_Struct.h"

NS_BEGIN(Engine)
class Character;
class Camera;
NS_END

NS_BEGIN(Client)

class SkillScript;
class BattleAction;

class BattleActionExecutor;

class CombatStatCom;
class BuffSystem;
class BattleEffectResolver;
class BattleStateMachine;
class BattleTurnCamera;
class BattleSpawnRegistry;
class SkillTreeCom;
class BattleTutorialController;
class BattleResultRecoder;
class PictosBattleSystem;

struct EvtBattlePresentation;

class BattleController : public Base
{
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
    // AP ++
    DECLARE_DELEGATE(FOnBattleApChanged, const BattleChar&, uint32, EAPChangeReason);
    // 턴 시작 상태 이상 Announce
    DECLARE_DELEGATE(FOnTurnStartStatusAnnounceRequested, const BattleChar&, Buffs);


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
        const Shared<GameObject>& sourceObject = nullptr,
        bool completeImmediately = false,
        int32 battleStartSpawnerId = 0,
        const Vec3& battleStartOffset = Vec3::Zero);
    void Cancel_BattleEnterPending();

    EBattleEndResult OnExitBattleField(EBattleEndResult result);

    void Set_BattleFSM();
    void PlayerStartAnim();
    void HitAction(BattleChar target, AttackType attackType, bool isCritical);
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

    void Spawn_BattleChar(bool IsHero, string prefabName, Vec3 pos, float yawDeg = 0.f);
public:
    Shared<BattleStateMachine> Get_BattleFSM() { return _battleFSM; };
    BattleAction& Get_TurnAction() { return _turnAction; }
    const BattleAction& Get_TurnAction() const { return _turnAction; }
    ActionRequest& Get_TurnActionRequest() { return _turnActionRequest; }
    const ActionRequest& Get_TurnActionRequest() const { return _turnActionRequest; }

    Shared<PictosBattleSystem> Get_PictosBattleSystem() const { return _pictosBattleSystem; }

    // Action& Get_TurnAction() { return _turnAction; }
    void    Request_WidePresentationMode();

    // 전투 도망
    void    Request_EscapeBattle();

    vector<BattleChar> Get_AliveEnemies();
    vector<BattleChar> Get_AllEnemies() { return _enemies; };
    vector<BattleChar> Get_AliveHeroes();
    bool Will_NextScheduledTurnBePlayer(const BattleChar& currentActor) const;

    BattleReactionController& Get_ReactionController() { return _reactionController; }
    const BattleReactionController& Get_ReactionController() const { return _reactionController; }
    Shared<BattleQTEController> Get_QTEController() const { return _qteController; }

    bool Is_BattleStarted() { return _isBattleActive; }
    bool Is_BattleEntering() const { return _isBattleEntering; } // 배틀 진입 시작
    bool Is_BattleIntroPresentationActive() const { return _isBattleEntering || _isWaitingBattleTransition; }

    bool Is_BattleFieldActiveOrEntering(uint32 fieldId) const;

    Shared<SkillScript> Get_CurrentScript() const { return _curScript; }
    void Clear_CurrentScript() { _curScript.reset(); }

    const vector<BattleChar>& Get_BattleHeroes() const { return _heroes; }
    const vector<BattleChar>& Get_BattleEnemies() const { return _enemies; }

public:
    void End_FirstAnim();
    void Handle_EditorStopRuntime();
    // void Handle_TargetPreviewChanged(const BattleChar* target);
    HRESULT Ready_Delegates();

private:
    void Handle_BattlePresentationEvent(const EvtBattlePresentation& event);
    void Sync_SkillPresentationHeroVisibility(
            BattleCameraMode cameraMode,
            const Shared<Engine::Character>& subject);
    void Restore_SkillPresentationHeroVisibility();
    void Begin_SkillPresentationVanish(const Shared<GameObject>& actor);
    void Begin_SkillPresentationReveal(const Shared<GameObject>& actor);

public: /* 타겟팅 중인 적 세팅 */
    void                Set_TargetPreview(const BattleChar& target);
    void                Clear_TargetPreview();
    const BattleChar*   Get_TargetPreview() const;

    bool                Ensure_EnemyTargetPreview();
    bool                Move_EnemyTargetPreview(int32 direction);
    void                Publish_TargetPreviewEnemyFocus() const;

public:
    bool                Is_PlayerTurnActive() const;
    BattleTurnState     Get_CurrentTurnState() const;
    void                Set_AutoParryEnabled(bool enabled) { _autoParryEnabled = enabled; }
    bool                Is_AutoParryEnabled() const { return _autoParryEnabled; }

    Shared<BattleTurnCamera> Get_BattleTurnCamera() const { return _battleTurnCamera; }
    Shared<BattleSpawnRegistry> Get_BattleSpawnRegistry() const { return _battleSpawnRegistry; }

    bool Should_UpdateRuntime() const;

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
    void Handle_DamageApplied(const DamageAppliedInfo& info);

    // 현재 턴 액터가 죽으면 턴 종료를 예약한다.
    void Try_RequestEndTurnOnCurrentActorDeath(const BattleChar& damagedTarget, float damage);

    // 마지막 데미지 들어가면서 슬로우 시작
    void Try_BeginVictorySlowOnDamage(const BattleChar& damagedTarget, float damage);
    bool Can_TriggerVictory() const;

    uint32 Get_LastVictoryRewardExp() const { return _lastVictoryRewardExp; }
    const vector<string>& Get_LastVictoryRewardPictosIds() const { return _lastVictoryRewardPictosIds; }

    void Set_FreeAimHitTarget(const BattleFreeAimHitTarget& target);
    void Clear_FreeAimHitTarget();
    bool Has_FreeAimHitTarget() const { return _hasFreeAimHitTarget; }
    const BattleFreeAimHitTarget* Get_FreeAimHitTarget() const
    {
        return _hasFreeAimHitTarget ? &_freeAimHitTarget : nullptr;
    }

    // 스킬 스크립트가 다단히트를 예약할 때 사용
    BattleHitSequenceRunner& Get_HitSequenceRunner() { return _hitSequenceRunner; }
    void Queue_DelayedEchoAttackStep(
            float delaySec,
            const BattleChar& attacker,
            const vector<BattleChar>& targets,
            const BattleChar& reactionTarget,
            uint32 sourceStepIndex,
            uint32 consumeKey,
            AttackType attackType,
            float powerMultiplier);

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

    bool Try_ApplySkillBuff(const Skill_Data& skill, const BattleChar& target);
    void Complete_TurnStartStatusAnnounce();

    // HitStop (패링 직후, Free Aim 으로 약점공격 성공했을 떄 잠깐)
    void Play_BattleHitStop(float worldTimeScale, float duration);

    // 전투 튜토리얼
    Shared<BattleTutorialController> Get_BattleTutorialController() const { return _battleTutorialController; }

    // 전투 기록
    Shared<BattleResultRecoder> Get_BattleResultTracker() const { return _battleResultRecoder; }
    void Handle_ReactionSucceededForResult(BattleReactionType reactionType, const vector<BattleChar>& targets);

private:
    HRESULT Ready_BattleTurnCamera();
    void    Activate_BattleTurnCamera();

    // DOF_MODIFY 
    void    Begin_BattleDOFOverride();
    void    End_BattleDOFOverride();
    Engine::DOFTransitionDesc Build_BattleDOFRestoreTransition();
    void    Restore_WorldCamera();
    void    Clear_BattleRuntimeState();
    void    Push_TurnEntry(const BattleChar& actor, float nextTime);
    float   Resolve_PrefabGroundOffsetY(const Shared<GameObject>& actor) const;

    void    Complete_BattleField();          // 2초 슬로우모션이 끝난 뒤 실제 PB/MB 생성과 턴 시작을 처리한다.
    void    Clear_BattleEnterPending();      // 전투 진입 대기 데이터만 정리

    // 몬스터 약점공격 보너스 데미지
    bool    Try_ApplyFreeAimWeakPointBonus(const BattleFreeAimHitTarget& hitTarget, const BattleChar& attacker);

    bool    Update_BattleTutorialGate(float timeDelta);
    int32   Find_EnemyTargetPreviewIndex(const vector<BattleChar>& aliveEnemies) const;

public:
    void    Capture_PreBattleBGM();
    void    Restore_PreBattleBGM();

private:
    // World의 SkillTree 상태를 읽어서 battle에서 사용할 스킬 세팅
    void Configure_BattlePlayerSkills(
        HeroId heroId,
        const Shared<CombatStatCom>& battleStat,
        const Shared<SkillTreeCom>& worldSkillTreeCom,
        const vector<string>& equippedSkillKeys);

    PlayerCharacterType To_PlayerCharacterType(HeroId heroId);

    bool Try_BeginTurnStartStatusAnnounce();
    void Continue_StartTurnAfterStatusEffects();

    struct DelayedEchoAttackStep
    {
        float remainingSec = 0.f;
        BattleChar attacker = {};
        vector<BattleChar> targets = {};
        BattleChar reactionTarget = {};
        uint32 sourceStepIndex = UINT32_MAX;
        uint32 consumeKey = UINT32_MAX;
        AttackType attackType = AttackType::BaseAttack;
        float powerMultiplier = 1.f;
    };

    void Update_DelayedEchoAttackSteps(float timeDelta);
    void Apply_DelayedEchoAttackStep(const DelayedEchoAttackStep& step);
    void Clear_DelayedEchoAttackSteps();

private:
    using BattleReactionInputResult = BattleReactionController::BattleReactionInputResult;
    void Apply_ReactionApReward(const BattleReactionInputResult& result);

public: /* Delegate */
    FOnBattleStarted                OnBattleStarted;        // 배틀 시작
    FOnBattleEnded                  OnBattleEnded;          // 배틀 끝
    FOnTurnStarted                  OnTurnStarted;          // 턴 시작
    FOnTurnEnded                    OnTurnEnded;            // 턴 끝
    FOnTargetPreviewChanged         OnTargetPreviewChanged; // Targeting 중일 때
    FOnActionStarted                OnActionStarted;        // 공격, 스킬 시작
    FOnBattleApChanged              OnBattleApChanged;
    FOnTurnStartStatusAnnounceRequested OnTurnStartStatusAnnounceRequested;


    FDelegateHandle                 _damageAppliedHandle{};
    FDelegateHandle                 _editorStopHandle = {};
    FDelegateHandle                 _reactionSucceededResultHandle{};
    EvtHandle                       _battlePresentationHandle{}; // Event_Bus 구독 핸들이라 FDelegateHandle이 아니라 EvtHandle을 쓴다.

private:
    BattleChar                      _currentTurnActor = {};
    BattleAction                   _turnAction = {};
    ActionRequest                  _turnActionRequest = {};

    Shared<SkillScript>             _curScript = {};

    bool _isBattleActive = { false };
    bool _isBattleEntering = { false };
    bool _hasRequestedBattleEnterCamera = false;
    bool _autoParryEnabled = false;

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

    void Play_EnemyTargetedStart(const vector<BattleChar>& targets);
    void Play_EnemyTargetedEnd();

    void Request_BattleSpawnRotationReturn(const BattleChar& target, float duration = 0.2f);
    void Update_BattleSpawnRotationReturns(float timeDelta);

private:
    bool _battleEnded = {};
    bool _pendingNextTurn = {};
    bool _hasPendingTutorialEnemyTurn = false;

    // 현재 턴을 진행중인 적이 죽으면.
    bool _requestEndTurnOnCurrentActorDeath = false;

public:
    struct Compare
    {
        bool operator()(const TurnEntry& a, const TurnEntry& b) const
        {
            if (a.nextTime != b.nextTime)
                return a.nextTime > b.nextTime; // nextTime이 작을수록 먼저 행동한다.

            return a.orderSeq > b.orderSeq; // 같은 시간대면 먼저 예약된 턴이 먼저다.
        }
    };

private:
    priority_queue<TurnEntry, vector<TurnEntry>, Compare> _turnQueue;
    float _battleTime = {};
    uint64 _nextTurnEntrySeq = 0;
    vector<BattleChar> _heroes;
    vector<BattleChar> _enemies;

    Shared<BattleStateMachine>   _battleFSM;
    Shared<BuffSystem>           _buffSystem;
    Shared<BattleActionExecutor> _battleActionExe;

    Shared<BattleSpawnRegistry>  _battleSpawnRegistry;   // 전투 시작 시 스폰되는 객체들 저장소
    Shared<BattleQTEController>  _qteController{};
    BattleReactionController     _reactionController{};
    BattleHitSequenceRunner      _hitSequenceRunner{};   // 루네 천둥번개 같은 다단히트 관리
    vector<DelayedEchoAttackStep> _delayedEchoAttackSteps{};
    Shared<BattleResultRecoder>  _battleResultRecoder{};

    Shared<PictosBattleSystem>   _pictosBattleSystem{};  // 픽토스

    bool _firstAnimflag = { false };
    int  _firstAnimCount = { 0 };
    uint32 _firstAnimTargetCount = { 0 };

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
    Vec3    _pendingBattleStartOffset = Vec3::Zero;
    float   _pendingBattlePlaneYaw = 0.f;

    bool _isFreeAimShotPlaying = false;
    BattleAction _freeAimShotAction{};

    uint32 _lastVictoryRewardExp = 0;
    vector<string> _lastVictoryRewardPictosIds{};
    wstring _preBattleBGM{};
    bool _hasCapturedPreBattleBGM = false;

    vector<BattleChar> _enemyTurnTargetedActors = {};
    vector<BattleSpawnRotationReturn> _battleSpawnRotationReturns = {};

    // 전투 튜토리얼
    Shared<BattleTutorialController> _battleTutorialController{};

    Buffs      _pendingTurnStartStatusBuff = Buffs::None;
    BattleChar _pendingTurnStartStatusActor = {};

    vector<Buffs> _processedTurnStartStatuses = {};
    bool          _isWaitingTurnStartStatusHit = false;

    uint32      _activeBattleFieldID = 0;

private:
    struct SkillPresentationHiddenHero
    {
        Shared<GameObject> actor{};
        bool wasVisible = false; // 연출 전 visibility를 저장해 원래 숨겨진 대상은 복구 때 새로 켜지 않게 한다.
    };

    vector<SkillPresentationHiddenHero> _skillPresentationHiddenHeroes = {};
    Shared<Engine::Character> _skillPresentationSubject = nullptr;

public:
    static Shared<BattleController> Create();
    void Free() override;

public:
};

NS_END

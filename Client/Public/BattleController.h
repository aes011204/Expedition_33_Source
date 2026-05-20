#pragma once

#include "Base.h"
#include "Delegate.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class Character;
class Camera;
NS_END

NS_BEGIN(Client)

class SkillScript;

class BattleActionExecutor;

class CombatStatCom;
class BuffSystem;
class BattleEffectResolver;
class BattleStateMachine;
class BattleTurnCamera;

class BattleController : public Base
{
public:
    enum ReactionType
    {
        None,
        Parry,
        Dodge,
        QTE
    };

    struct Reaction
    {
        ReactionType type = ReactionType::None;
        bool isOpen = false;
        bool isConsumed = false;
        ReactionType result = ReactionType::None;

    };

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
        vector<string>& battleMonsterPrefabNames);

    EBattleEndResult OnExitBattleField(EBattleEndResult result);

    void Set_BattleFSM();
    void ExecuteAction(Action turnAction); //실제로 선택한 행동을 실행하는 함수

    void EndTurn();
    void StartTurn();

    EAPChangeReason FinishBattle();

    void Create_BattleChar(const vector<HeroId>& heroIds, vector<string>& battleMonsterPrefabNames);
    void Build_InitialTurnQueue(bool IsFirstHero);
    void SetCharInitPos();

    void OnActionEndNotify();
public:
    Shared<BattleStateMachine> Get_BattleFSM() { return _battleFSM; };
    Action& Get_TurnAction() { return _turnAction; }
    //uint32 Get_EnemiesNum() { return _enemies.size(); }
    //uint32 Get_HeroesNum() { return _heroes.size(); }

    vector<BattleChar> Get_AliveEnemies();
    vector<BattleChar> Get_AliveHeroes();

    bool Is_BattleStarted() { return _isBattleActive; }
    bool Is_BattleEntering() const { return _isBattleEntering; } // 배틀 진입 시작


    //void Set_CurrentScript(const Shared<SkillScript>& script){_curScript = script;}
    Shared<SkillScript> Get_CurrentScript() const{return _curScript;}
    void Clear_CurrentScript(){_curScript.reset();}

    const Reaction& Get_Reaction() const{return _reaction;}
public:
    void Handle_EditorStopRuntime();
    HRESULT Ready_Delegates();

    //리액션 인풋
    void OpenReactionWindow(ReactionType type);
    void CloseReactionWindow(ReactionType type); //일단 이건 쓰지 마
    void ClearReactionState();
    void UpdateReactionInput();

public:
    bool            Is_PlayerTurnActive() const;
    BattleTurnState Get_CurrentTurnState() const;

private:
    HRESULT Ready_BattleTurnCamera();
    void    Activate_BattleTurnCamera();
    void    Restore_WorldCamera();

    void    Complete_BattleField();          // 2초 슬로우모션이 끝난 뒤 실제 PB/MB 생성과 턴 시작을 처리한다.
    void    Clear_BattleEnterPending();      // 전투 진입 대기 데이터만 정리

private:
    BattleChar _currentTurnActor = {};
    Action _turnAction = {};
    Reaction _reaction = {};

    Shared<SkillScript> _curScript = {};

    bool _isBattleActive = { false };
    bool _isBattleEntering = { false };

    int currentTurnIndex = {};

    bool _flag = { false };

private:
    void Execute_BaseAttack(const BattleChar actor, const Action& turnAction);
    void Execute_Skill(const BattleChar actor, const Action& turnAction);
    void Execute_Item(const BattleChar actor, const Action& turnAction);

    bool _battleEnded = {};
    bool _pendingNextTurn = {};
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
    /* Shared< BattleEffectResolver> _battleEffectResolver;*/
    Shared<BattleActionExecutor> _battleActionExe;

    FDelegateHandle _editorStopHandle = {};

private:
    Shared<BattleTurnCamera> _battleTurnCamera;
  //  Shared<Camera> _worldCamera;    // 전투 시작 전 카메라 저장

private:  // 슬로우 처리
    float _battleEnterElapsed = 0.f;
    float _battleEnterDuration = 2.f;
    float _battleEnterWorldTimeScale = 0.25f;

    uint32 _pendingBattleFieldID = 0;
    bool _pendingBattleIsFirstHero = false;
    vector<HeroId> _pendingBattleHeroIds = {};
    vector<string> _pendingBattleMonsterPrefabNames = {};

    // 배틀 카메라는 이미 켰고, UI 전환 종료만 기다리는 상태
    bool _isWaitingBattleTransition = false;

public:
    static Shared<BattleController> Create();
    void Free() override;

public:
};

NS_END

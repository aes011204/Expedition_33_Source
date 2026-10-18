#pragma once

#include "Base.h"
#include "BattleReactionController.h"
#include "Battle_Enum.h"
#include "Delegate.h"
#include "EffectCom.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(Client)

class BattleAction;
class BattleController;
class BattleSpawnRegistry;

class BattleTutorialController final : public Base
{
private:
    struct TutorialPausedModel
    {
        Shared<ModelCom> model{};
        bool wasPaused = false; // 튜토리얼 시작 전 pause 상태를 그대로 되돌리기 위해 저장
    };

    enum class TutorialSequenceStage
    {
        BattleSystem,
        TurnOrder,
        Attack,
        WaitingBaseAttack,
        WaitingFirstEnemyTurn,
        EnemyTurn,
        WaitingDodgeWindow,
        WaitingSkillTurn,
        Skill,
        WaitingLumiereAssaultCommand,
        WaitingQTE,
        WaitingSecondEnemyTurn,
        WaitingCounterWindow,
        WaitingFreeAimTurn,
        FreeAim,
        WaitingFreeAimInput,
        Done
    };

public:
    DECLARE_DELEGATE(FOnTutorialRequested, const BattleTutorialRequest&);

    DECLARE_DELEGATE(
        FOnTutorialClosed // UI가 현재 열려 있는 튜토리얼 패널을 닫는다.
    );

public:
    BattleTutorialController() = default;
    ~BattleTutorialController() override = default;

public:
    HRESULT Initialize();

    void Bind_BattleController(const Shared<BattleController>& battleController);
    void Unbind_BattleController();

    void Reset_ForBattle(uint32 encounterId, const Shared<BattleSpawnRegistry>& spawnRegistry);
    void Update(float timeDelta);
    void Close_Tutorial();
    void Reset_RuntimeState();

    bool Is_TutorialOpen() const { return _isTutorialOpen; }
    bool Should_BlockBattleUpdate() const { return _blockBattleUpdate; }
    bool Should_PauseBattlePresentation() const { return _pauseBattlePresentation; }

    BattleTutorialStep Get_ActiveStep() const { return _activeStep; }
    BattleTutorialStep Get_CommandFocusStep() const;
    BattleReactionType Get_RequiredReactionInput() const;
    // 닫기 키. 회피는 Q, 카운터는 E(실제 Parry 입력), 나머지는 F
    bool Can_CloseByConfirmInput() const;

    using BattleReactionInputResult = BattleReactionController::BattleReactionInputResult;

    bool Try_HandleReactionInput(
        BattleReactionController& reactionController,
        const BattleAction& turnAction,
        BattleReactionInputResult& outResult);

    bool Will_RequestTutorial(BattleTutorialStep step) const;

    void Advance_SequenceAfterClosed(BattleTutorialStep closedStep);
    bool Is_TutorialSequenceDone() const { return _sequenceStage == TutorialSequenceStage::Done; }

private:
    void Request_Tutorial(BattleTutorialStep step);
    void Open_Tutorial(const BattleTutorialRequest& request);
    void Clear_PendingTutorialRequest();

    bool Can_RequestTutorial(BattleTutorialStep step) const;
    BattleTutorialRequest Build_Request(BattleTutorialStep step) const;

    void Pause_TutorialModels();
    void Restore_TutorialModels();

    void Pause_TutorialEffects();
    void Restore_TutorialEffects();
    void Pause_TutorialSounds(const BattleTutorialRequest& request);
    void Restore_TutorialSounds();

public:
    bool Should_DeferEnemyTurnStart() const;
    bool Should_BlockPlayerBattleInput(
            BattleTurnState currentState,
            const ActionRequest& turnRequest) const;

private:
    void Handle_TurnStarted(const BattleChar& actor, BattleTurnState state);
    void Handle_ActionStarted(const BattleAction& action);
    void Handle_BattleTurnStateChanged(BattleTurnState prevState, BattleTurnState nextState);
    void Handle_ReactionWindowOpened(BattleReactionType reactionType);
    void Handle_QTEStarted(const BattleQTERequest& request);

public:
    FOnTutorialRequested OnTutorialRequested;
    FOnTutorialClosed OnTutorialClosed;

private:
    Weak<BattleController> _battleController;
    Weak<BattleSpawnRegistry> _spawnRegistry;

    uint32 _encounterId = 0;
    // 첫 번째 공격은 Dodge, 두 번쨰는 Counter UI
    uint32 _tutorialEnemyActionCount = 0;
    bool _isTutorialOpen = false;
    bool _blockBattleUpdate = false;
    bool _pauseBattlePresentation = false;
    bool _isTutorialSoundPaused = false; // QTE 중 이미 시작된 긴 Effect SFX를 튜토리얼 pause와 같이 멈춘다.

    bool _hasPendingRequest = false;
    float _pendingElapsedSec = 0.f;
    BattleTutorialRequest _pendingRequest{};

    BattleTutorialStep _activeStep = BattleTutorialStep::END;
    set<BattleTutorialStep> _shownSteps{};       // 같은 전투 안에서 같은 step 반복 표시를 막는다.
    vector<TutorialPausedModel> _pausedModels{}; // 튜토리얼이 멈춘 모델만 원복 대상으로 기록한다.
    vector<EffectCom::AttachedEffectPauseState> _pausedEffects{};

    TutorialSequenceStage _sequenceStage = TutorialSequenceStage::BattleSystem;


private:
    FDelegateHandle _turnStartedHandle{};
    FDelegateHandle _actionStartedHandle{};
    FDelegateHandle _turnStateChangedHandle{};
    FDelegateHandle _reactionWindowOpenedHandle{};
    FDelegateHandle _qteStartedHandle{};

public:
    static Shared<BattleTutorialController> Create();
    void Free() override;
};

NS_END

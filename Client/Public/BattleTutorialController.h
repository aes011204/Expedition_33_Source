#pragma once

#include "Base.h"
#include "Battle_Enum.h"
#include "Delegate.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(Client)

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

public:
    DECLARE_DELEGATE(FOnTutorialRequested,const BattleTutorialRequest&);

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

private:
    void Request_Tutorial(BattleTutorialStep step);
    void Open_Tutorial(const BattleTutorialRequest& request);

    bool Can_RequestTutorial(BattleTutorialStep step) const;
    BattleTutorialRequest Build_Request(BattleTutorialStep step) const;

    void Pause_TutorialModels();
    void Restore_TutorialModels();

private:
    void Handle_TurnStarted(const BattleChar& actor, BattleTurnState state);
    void Handle_BattleTurnStateChanged(BattleTurnState prevState, BattleTurnState nextState);
    void Handle_ReactionWindowOpened(BattleReactionType reactionType);
    void Handle_CounterStarted(const vector<BattleChar>& counterActors);
    void Handle_QTEStarted(const BattleQTERequest& request);

public:
    FOnTutorialRequested OnTutorialRequested;
    FOnTutorialClosed OnTutorialClosed;

private:
    Weak<BattleController> _battleController;
    Weak<BattleSpawnRegistry> _spawnRegistry;

    uint32 _encounterId = 0;
    bool _isTutorialOpen = false;
    bool _blockBattleUpdate = false;

    BattleTutorialStep _activeStep = BattleTutorialStep::None;
    set<BattleTutorialStep> _shownSteps{};       // 같은 전투 안에서 같은 step 반복 표시를 막는다.
    vector<TutorialPausedModel> _pausedModels{}; // 튜토리얼이 멈춘 모델만 원복 대상으로 기록한다.

    FDelegateHandle _turnStartedHandle{};
    FDelegateHandle _turnStateChangedHandle{};
    FDelegateHandle _reactionWindowOpenedHandle{};
    FDelegateHandle _counterStartedHandle{};
    FDelegateHandle _qteStartedHandle{};

public:
    static Shared<BattleTutorialController> Create();
    void Free() override;
};

NS_END

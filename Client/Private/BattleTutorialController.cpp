#include "BattleTutorialController.h"
#include "BattleController.h"
#include "BattleQTEController.h"
#include "BattleReactionController.h"
#include "BattleSpawnRegistry.h"
#include "BattleStateMachine.h"
#include "Character_BattleBase.h"
#include "ModelCom.h"
#include "PartObject.h"

namespace BattleEncounterIds
{
    constexpr uint32 LancelierTutorial = 99;
}

HRESULT BattleTutorialController::Initialize()
{
    return S_OK;
}

void BattleTutorialController::Bind_BattleController(const Shared<BattleController>& battleController)
{
    if (battleController == nullptr)
        return;

    Unbind_BattleController();

    _battleController = battleController;

    _turnStartedHandle = battleController->OnTurnStarted.Add(this, &BattleTutorialController::Handle_TurnStarted);

    if (Shared<BattleStateMachine> battleFSM = battleController->Get_BattleFSM())
    {
        _turnStateChangedHandle =
            battleFSM->OnBattleTurnStateChanged.Add(
                this, &BattleTutorialController::Handle_BattleTurnStateChanged); // 공격/자유조준 진입은 FSM 상태 변화로 판단
    }

    BattleReactionController& reactionController = battleController->Get_ReactionController();

    _reactionWindowOpenedHandle =
        reactionController.OnReactionWindowOpened.Add(this, &BattleTutorialController::Handle_ReactionWindowOpened); // Dodge/Parry 안내는 리액션 입력창 오픈 시점에 판단한다.

    _counterStartedHandle =
        reactionController.OnCounterStarted.Add(
            this, &BattleTutorialController::Handle_CounterStarted); // Counter 안내는 Counter 애니메이션 실제 시작 시점에 판단한다.

    if (Shared<BattleQTEController> qteController = battleController->Get_QTEController())
    {
        _qteStartedHandle =
            qteController->OnQTEStarted.Add(
                this, &BattleTutorialController::Handle_QTEStarted); // QTE 안내는 QTEController 시작 이벤트를 구독한다.
    }
}

void BattleTutorialController::Unbind_BattleController()
{
    Shared<BattleController> battleController = _battleController.lock();
    if (battleController == nullptr)
    {
        _battleController.reset();
        return;
    }

    if (_turnStartedHandle.IsValid())
        battleController->OnTurnStarted.Remove(_turnStartedHandle);

    if (Shared<BattleStateMachine> battleFSM = battleController->Get_BattleFSM())
    {
        if (_turnStateChangedHandle.IsValid())
            battleFSM->OnBattleTurnStateChanged.Remove(_turnStateChangedHandle);
    }

    BattleReactionController& reactionController = battleController->Get_ReactionController();

    if (_reactionWindowOpenedHandle.IsValid())
        reactionController.OnReactionWindowOpened.Remove(_reactionWindowOpenedHandle);

    if (_counterStartedHandle.IsValid())
        reactionController.OnCounterStarted.Remove(_counterStartedHandle);

    if (Shared<BattleQTEController> qteController = battleController->Get_QTEController())
    {
        if (_qteStartedHandle.IsValid())
            qteController->OnQTEStarted.Remove(_qteStartedHandle);
    }

    _turnStartedHandle.Reset();
    _turnStateChangedHandle.Reset();
    _reactionWindowOpenedHandle.Reset();
    _counterStartedHandle.Reset();
    _qteStartedHandle.Reset();

    _battleController.reset();
}

void BattleTutorialController::Reset_ForBattle(uint32 encounterId, const Shared<BattleSpawnRegistry>& spawnRegistry)
{
    Restore_TutorialModels();

    _spawnRegistry = spawnRegistry;
    _encounterId = encounterId;
    _isTutorialOpen = false;
    _blockBattleUpdate = false;
    _activeStep = BattleTutorialStep::None;
    _shownSteps.clear();
}

void BattleTutorialController::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void BattleTutorialController::Request_Tutorial(BattleTutorialStep step)
{
    if (!Can_RequestTutorial(step))
        return;

    BattleTutorialRequest request = Build_Request(step);

    _shownSteps.insert(step);

    // 튜토리얼 UI 열기
    Open_Tutorial(request);
}

void BattleTutorialController::Open_Tutorial(const BattleTutorialRequest& request)
{
    Restore_TutorialModels();

    _isTutorialOpen = true;
    _blockBattleUpdate = request.blockInput;
    _activeStep = request.step;

    if (request.pauseBattle)
        Pause_TutorialModels(); // 모델 pause 대상 수집과 원복 책임은 튜토리얼 컨트롤러가 가진다.

    OnTutorialRequested.Broadcast(request);
}

void BattleTutorialController::Close_Tutorial()
{
    if (!_isTutorialOpen)
        return;

    Restore_TutorialModels();

    _isTutorialOpen = false;
    _blockBattleUpdate = false;
    _activeStep = BattleTutorialStep::None;

    OnTutorialClosed.Broadcast();
}

void BattleTutorialController::Reset_RuntimeState()
{
    Restore_TutorialModels();

    _spawnRegistry.reset();
    _encounterId = 0;
    _isTutorialOpen = false;
    _blockBattleUpdate = false;
    _activeStep = BattleTutorialStep::None;
    _shownSteps.clear();

    OnTutorialClosed.Broadcast();
}

bool BattleTutorialController::Can_RequestTutorial(BattleTutorialStep step) const
{
    if (_encounterId != BattleEncounterIds::LancelierTutorial)
        return false;

    if (_isTutorialOpen)
        return false;

    if (step == BattleTutorialStep::None || step == BattleTutorialStep::END)
        return false;

    return _shownSteps.find(step) == _shownSteps.end();
}

BattleTutorialRequest BattleTutorialController::Build_Request(BattleTutorialStep step) const
{
    BattleTutorialRequest request{};
    request.step = step;

    switch (step)
    {
    case BattleTutorialStep::EnemyTurn:
        request.pauseBattle = false; // 적 턴 안내는 전투 흐름을 멈추지 않고 표시한다.
        request.blockInput = false;
        request.delaySec = 0.f;
        break;

    case BattleTutorialStep::Dodge:
    case BattleTutorialStep::Parry:
    case BattleTutorialStep::Counter:
    case BattleTutorialStep::QTE:
        request.pauseBattle = true; // 리액션/QTE는 애니메이션 타이밍 설명이라 현재 포즈를 고정한다.
        request.blockInput = true;
        request.delaySec = 0.f;
        break;

    case BattleTutorialStep::PlayerTurn:
    case BattleTutorialStep::Attack:
    case BattleTutorialStep::FreeAim:
        request.pauseBattle = true; // 플레이어 조작 설명은 입력을 막고 UI 확인 후 재개한다.
        request.blockInput = true;
        request.delaySec = 0.f;
        break;

    default:
        break;
    }

    return request;
}

void BattleTutorialController::Pause_TutorialModels()
{
    _pausedModels.clear();

    Shared<BattleSpawnRegistry> spawnRegistry = _spawnRegistry.lock();
    if (spawnRegistry == nullptr)
        return;

    spawnRegistry->Visit_SpawnedObjects([this](const BattleSpawnRegistry::BattleEntry& entry)
    {
        Shared<Character_BattleBase> character = dynamic_pointer_cast<Character_BattleBase>(entry.object);
        if (character == nullptr)
            return;

        PartObject* body = character->Get_PartObject(L"Part_Body");
        if (body == nullptr)
            return;

        Shared<ModelCom> model = body->Get_Component<ModelCom>();
        if (model == nullptr)
            return;

        for (const TutorialPausedModel& paused : _pausedModels)
        {
            if (paused.model == model)
                return;
        }

        TutorialPausedModel paused{};
        paused.model = model;
        paused.wasPaused = model->Is_AnimationPaused();

        model->Set_AnimationPaused(true); // 튜토리얼 UI가 열린 동안 현재 애니메이션 프레임을 고정한다.
        _pausedModels.push_back(paused);
    });
}

void BattleTutorialController::Restore_TutorialModels()
{
    for (TutorialPausedModel& paused : _pausedModels)
    {
        if (paused.model == nullptr)
            continue;

        paused.model->Set_AnimationPaused(paused.wasPaused); // 튜토리얼 시작 전 pause 상태로 되돌린다.
    }

    _pausedModels.clear();
}

void BattleTutorialController::Handle_TurnStarted(const BattleChar& actor, BattleTurnState state)
{
    if (state == BattleTurnState::ChooseAction)
        Request_Tutorial(BattleTutorialStep::PlayerTurn);
    else
        Request_Tutorial(BattleTutorialStep::EnemyTurn);
}

void BattleTutorialController::Handle_BattleTurnStateChanged(BattleTurnState prevState, BattleTurnState nextState)
{
    if (nextState == BattleTurnState::AttackTargeting)
        Request_Tutorial(BattleTutorialStep::Attack);

    if (nextState == BattleTurnState::FreeAimTargeting)
        Request_Tutorial(BattleTutorialStep::FreeAim);
}

void BattleTutorialController::Handle_ReactionWindowOpened(BattleReactionType reactionType)
{
    if (reactionType == BattleReactionType::Dodge)
        Request_Tutorial(BattleTutorialStep::Dodge);

    if (reactionType == BattleReactionType::Parry)
        Request_Tutorial(BattleTutorialStep::Parry);
}

void BattleTutorialController::Handle_CounterStarted(const vector<BattleChar>& counterActors)
{
    if (counterActors.empty())
        return;

    Request_Tutorial(BattleTutorialStep::Counter);
}

void BattleTutorialController::Handle_QTEStarted(const BattleQTERequest& request)
{
    Request_Tutorial(BattleTutorialStep::QTE);
}

Shared<BattleTutorialController> BattleTutorialController::Create()
{
    auto instance = make_shared<BattleTutorialController>();

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("Failed to Created : BattleTutorialController");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void BattleTutorialController::Free()
{
    Unbind_BattleController();
    Reset_RuntimeState();

    OnTutorialRequested.Clear();
    OnTutorialClosed.Clear();

    Base::Free();
}

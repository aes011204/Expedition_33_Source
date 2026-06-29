#include "BattleTutorialController.h"
#include "BattleController.h"
#include "BattleQTEController.h"
#include "BattleReactionController.h"
#include "BattleSpawnRegistry.h"
#include "BattleStateMachine.h"
#include "Character_BattleBase.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "BattleEncounterProfile.h"
#include "CombatStatCom.h"
#include "EffectCom.h"
#include "EffectInstance.h"
#include "Client_Constants.h"

namespace
{
    constexpr const char* kTutorialQTESkillKey = "LumiereAssault";
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
    _actionStartedHandle = battleController->OnActionStarted.Add(this, &BattleTutorialController::Handle_ActionStarted);

    if (Shared<BattleStateMachine> battleFSM = battleController->Get_BattleFSM())
    {
        _turnStateChangedHandle =
            battleFSM->OnBattleTurnStateChanged.Add(
                this, &BattleTutorialController::Handle_BattleTurnStateChanged); // 공격/자유조준 진입은 FSM 상태 변화로 판단
    }

    BattleReactionController& reactionController = battleController->Get_ReactionController();

    _reactionWindowOpenedHandle =
        reactionController.OnReactionWindowOpened.Add(this, &BattleTutorialController::Handle_ReactionWindowOpened); // 회피/카운터 안내는 리액션 입력창 오픈 시점에 판단한다.

    if (Shared<BattleQTEController> qteController = battleController->Get_QTEController())
    {
        _qteStartedHandle =
            qteController->OnQTEStarted.Add(
                this, &BattleTutorialController::Handle_QTEStarted);
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

    if (_actionStartedHandle.IsValid())
        battleController->OnActionStarted.Remove(_actionStartedHandle);

    if (Shared<BattleStateMachine> battleFSM = battleController->Get_BattleFSM())
    {
        if (_turnStateChangedHandle.IsValid())
            battleFSM->OnBattleTurnStateChanged.Remove(_turnStateChangedHandle);
    }

    BattleReactionController& reactionController = battleController->Get_ReactionController();

    if (_reactionWindowOpenedHandle.IsValid())
        reactionController.OnReactionWindowOpened.Remove(_reactionWindowOpenedHandle);

    if (Shared<BattleQTEController> qteController = battleController->Get_QTEController())
    {
        if (_qteStartedHandle.IsValid())
            qteController->OnQTEStarted.Remove(_qteStartedHandle);
    }

    _turnStartedHandle.Reset();
    _actionStartedHandle.Reset();
    _turnStateChangedHandle.Reset();
    _reactionWindowOpenedHandle.Reset();
    _qteStartedHandle.Reset();

    _battleController.reset();
}

void BattleTutorialController::Reset_ForBattle(uint32 encounterId, const Shared<BattleSpawnRegistry>& spawnRegistry)
{
    Restore_TutorialModels();
    Restore_TutorialEffects();
    Restore_TutorialSounds();

    _sequenceStage = TutorialSequenceStage::BattleSystem;
    _spawnRegistry = spawnRegistry;
    _encounterId = encounterId;
    _tutorialEnemyActionCount = 0;
    _isTutorialOpen = false;
    _blockBattleUpdate = false;
    _isTutorialSoundPaused = false;
    _hasPendingRequest = false;
    _pendingElapsedSec = 0.f;
    _pendingRequest = {};
    _activeStep = BattleTutorialStep::END;
    _shownSteps.clear();
}

void BattleTutorialController::Update(float timeDelta)
{
    if (!_hasPendingRequest)
        return;

    _pendingElapsedSec += max(0.f, timeDelta);

    if (_pendingElapsedSec < _pendingRequest.delaySec)
        return;

    BattleTutorialRequest request = _pendingRequest;
    Clear_PendingTutorialRequest();

    Open_Tutorial(request); // QTE는 delaySec동안 UI 를보여주고, 멈추기
}

BattleReactionType BattleTutorialController::Get_RequiredReactionInput() const
{
    if (!_isTutorialOpen)
        return BattleReactionType::None;

    switch (_activeStep)
    {
    case BattleTutorialStep::Dodge:
        return BattleReactionType::Dodge; // Q 입력으로 닫고 회피 성공 처리

    case BattleTutorialStep::Counter:
        return BattleReactionType::Parry; // Counter 안내는 E 패링 입력을 성공 처리한 뒤 실제 Counter 흐름으로 이어진다.
    default:
        return BattleReactionType::None;
    }
}

BattleTutorialStep BattleTutorialController::Get_CommandFocusStep() const
{
    if (_activeStep == BattleTutorialStep::Attack || _sequenceStage == TutorialSequenceStage::WaitingBaseAttack)
        return BattleTutorialStep::Attack;

    if (_activeStep == BattleTutorialStep::Skill || _sequenceStage == TutorialSequenceStage::WaitingLumiereAssaultCommand)
        return BattleTutorialStep::Skill;

    return BattleTutorialStep::END;
}

bool BattleTutorialController::Can_CloseByConfirmInput() const
{
    if (!_isTutorialOpen)
        return false;

    return Get_RequiredReactionInput() == BattleReactionType::None;
}

bool BattleTutorialController::Try_HandleReactionInput(BattleReactionController& reactionController,
    const BattleAction& turnAction, BattleReactionInputResult& outResult)
{
    outResult = {};

    const BattleReactionType requiredReaction = Get_RequiredReactionInput();
    if (requiredReaction == BattleReactionType::None)
        return false;

    const bool pressedRequiredKey =
        (requiredReaction == BattleReactionType::Dodge && GAME->KeyDown(KEY_TYPE::Q)) ||
        (requiredReaction == BattleReactionType::Parry && GAME->KeyDown(KEY_TYPE::E));

    if (!pressedRequiredKey)
        return true;

    if (!reactionController.IsWindowOpen(requiredReaction))
        return true;

    outResult = reactionController.SubmitInput(requiredReaction, turnAction);

    if (outResult.success)
        Close_Tutorial();

    return true;
}

bool BattleTutorialController::Will_RequestTutorial(BattleTutorialStep step) const
{
    return Can_RequestTutorial(step);
}

void BattleTutorialController::Advance_SequenceAfterClosed(BattleTutorialStep closedStep)
{
    switch (closedStep)
    {
    case BattleTutorialStep::BattleSystem:
        _sequenceStage = TutorialSequenceStage::TurnOrder;
        Request_Tutorial(BattleTutorialStep::TurnOrder); // 전투 시스템 설명 직후 턴 오더를 바로 보여준다.
        break;

    case BattleTutorialStep::TurnOrder:
        _sequenceStage = TutorialSequenceStage::Attack;
        Request_Tutorial(BattleTutorialStep::Attack); // 턴 오더 확인 뒤 기본 공격 조작을 바로 안내한다.
        break;

    case BattleTutorialStep::Attack:
        _sequenceStage = TutorialSequenceStage::WaitingBaseAttack;
        break;

    case BattleTutorialStep::EnemyTurn:
        _sequenceStage = TutorialSequenceStage::WaitingDodgeWindow;
        break;

    case BattleTutorialStep::Dodge:
        _sequenceStage = TutorialSequenceStage::WaitingSkillTurn;
        break;

    case BattleTutorialStep::Skill:
        _sequenceStage = TutorialSequenceStage::WaitingLumiereAssaultCommand;
        break;

    case BattleTutorialStep::QTE:
        _sequenceStage = TutorialSequenceStage::WaitingSecondEnemyTurn;
        break;

    case BattleTutorialStep::Counter:
        _sequenceStage = TutorialSequenceStage::WaitingFreeAimTurn;
        break;

    case BattleTutorialStep::FreeAim:
        _sequenceStage = TutorialSequenceStage::WaitingFreeAimInput;
        break;

    default:
        break;
    }
}

void BattleTutorialController::Request_Tutorial(BattleTutorialStep step)
{
    if (!Can_RequestTutorial(step))
        return;

    BattleTutorialRequest request = Build_Request(step);

    _shownSteps.insert(step);

    if (request.delaySec > 0.f)
    {
        _pendingRequest = request;
        _pendingElapsedSec = 0.f;
        _hasPendingRequest = true;
        return;
    }

    // 튜토리얼 UI 열기
    Open_Tutorial(request);
}

void BattleTutorialController::Open_Tutorial(const BattleTutorialRequest& request)
{
    Restore_TutorialModels();
    Restore_TutorialEffects();
    Restore_TutorialSounds();

    _isTutorialOpen = true;
    _blockBattleUpdate = request.blockInput;
    _activeStep = request.step;

    if (request.pauseBattle)
    {
        Pause_TutorialModels(); // 모델 pause 대상 수집과 원복 책임은 튜토리얼 컨트롤러가 가진다.
        Pause_TutorialEffects();
        Pause_TutorialSounds(request);
    }

    OnTutorialRequested.Broadcast(request);
}

void BattleTutorialController::Clear_PendingTutorialRequest()
{
    _hasPendingRequest = false;
    _pendingElapsedSec = 0.f;
    _pendingRequest = {};
}

void BattleTutorialController::Close_Tutorial()
{
    if (!_isTutorialOpen)
        return;

    const BattleTutorialStep closedStep = _activeStep;

    Restore_TutorialModels();
    Restore_TutorialEffects();
    Restore_TutorialSounds();

    _isTutorialOpen = false;
    _blockBattleUpdate = false;
    _activeStep = BattleTutorialStep::END;

    OnTutorialClosed.Broadcast();

    Advance_SequenceAfterClosed(closedStep);
}

void BattleTutorialController::Reset_RuntimeState()
{
    Restore_TutorialModels();
    Restore_TutorialEffects();
    Restore_TutorialSounds();

    _spawnRegistry.reset();
    _encounterId = 0;
    _tutorialEnemyActionCount = 0;
    _isTutorialOpen = false;
    _blockBattleUpdate = false;
    _isTutorialSoundPaused = false;
    _hasPendingRequest = false;
    _pendingElapsedSec = 0.f;
    _pendingRequest = {};
    _activeStep = BattleTutorialStep::END;
    _sequenceStage = TutorialSequenceStage::BattleSystem;
    _shownSteps.clear();

    OnTutorialClosed.Broadcast();
}

bool BattleTutorialController::Can_RequestTutorial(BattleTutorialStep step) const
{
    const bool isPotatoBagGradientTutorial =
        step == BattleTutorialStep::Gradient &&
        _encounterId == Constants::Value::potatoBagEncounterId;

    if (!isPotatoBagGradientTutorial && !Is_TutorialBattleEncounter(_encounterId))
        return false;

    if (_isTutorialOpen)
        return false;

    if (step == BattleTutorialStep::END)
        return false;

    return _shownSteps.find(step) == _shownSteps.end();
}

BattleTutorialRequest BattleTutorialController::Build_Request(BattleTutorialStep step) const
{
    BattleTutorialRequest request{};
    request.step = step;

    switch (step)
    {
    case BattleTutorialStep::QTE:
        request.pauseBattle = true;
        request.blockInput = true;
        request.delaySec = 0.27f;	// QTE는 약간 진행되고 멈추기
        break;

    case BattleTutorialStep::Dodge:
    case BattleTutorialStep::Counter:
        request.pauseBattle = true;
        request.blockInput = true;
        request.delaySec = 0.f;
        break;

    case BattleTutorialStep::BattleSystem:
        request.pauseBattle = true;
        request.blockInput = true;
        request.delaySec = 1.3f;
        break;
    case BattleTutorialStep::TurnOrder:
        request.pauseBattle = true;
        request.blockInput = true;
        request.delaySec = 0.f;
        break;
    case BattleTutorialStep::Attack:
    case BattleTutorialStep::EnemyTurn:
    case BattleTutorialStep::Skill:
    case BattleTutorialStep::FreeAim:
        request.pauseBattle = false;
        request.blockInput = true;
        request.delaySec = 0.f;
        break;

    case BattleTutorialStep::Gradient:
        request.pauseBattle = true;
        request.blockInput = true;
        request.delaySec = 0.f; // 감자자루 첫 플레이어 선택 전에 바로 안내를 띄운다.
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

void BattleTutorialController::Pause_TutorialEffects()
{
    _pausedEffects.clear();

    Shared<BattleSpawnRegistry> spawnRegistry = _spawnRegistry.lock();
    if (spawnRegistry == nullptr)
        return;

    spawnRegistry->Visit_SpawnedObjects([this](const BattleSpawnRegistry::BattleEntry& entry)
        {
            Shared<Character_BattleBase> character = dynamic_pointer_cast<Character_BattleBase>(entry.object);
            if (character == nullptr)
                return;

            if (Shared<EffectCom> effectCom = character->Get_Component<EffectCom>())
                effectCom->Capture_AttachedEffectPauseStates(_pausedEffects);

            for (const auto& [partTag, partObject] : character->Get_PartObjects())
            {
                UNREFERENCED_PARAMETER(partTag);

                if (partObject == nullptr)
                    continue;

                if (Shared<EffectCom> partEffectCom = partObject->Get_Component<EffectCom>())
                    partEffectCom->Capture_AttachedEffectPauseStates(_pausedEffects);
            }
        });
}

void BattleTutorialController::Restore_TutorialEffects()
{
    for (EffectCom::AttachedEffectPauseState& paused : _pausedEffects)
    {
        if (paused.effect == nullptr)
            continue;

        paused.effect->Set_PlaybackPaused(paused.wasPaused); // 튜토리얼 시작 전 effect pause 상태로 되돌린다.
    }

    _pausedEffects.clear();
}

void BattleTutorialController::Pause_TutorialSounds(const BattleTutorialRequest& request)
{
    if (request.step != BattleTutorialStep::QTE)
        return;

    if (_isTutorialSoundPaused)
        return;

    GAME->Set_SoundChannelPaused(SoundChannel::Effect, true); // 루미에르 어썰트처럼 이미 시작된 긴 Effect SFX를 QTE 설명 중 같이 멈춘다.
    _isTutorialSoundPaused = true;
}

void BattleTutorialController::Restore_TutorialSounds()
{
    if (!_isTutorialSoundPaused)
        return;

    GAME->Set_SoundChannelPaused(SoundChannel::Effect, false); // 튜토리얼 pause가 끝나면 QTE 이전에 진행 중이던 Effect SFX를 이어서 재생한다.
    _isTutorialSoundPaused = false;
}

bool BattleTutorialController::Should_DeferEnemyTurnStart() const
{
    return _isTutorialOpen &&
        (_activeStep == BattleTutorialStep::EnemyTurn ||
            _activeStep == BattleTutorialStep::Gradient);
}

bool BattleTutorialController::Should_BlockPlayerBattleInput(BattleTurnState currentState,
    const ActionRequest& turnRequest) const
{
    if (!Is_TutorialBattleEncounter(_encounterId) || Is_TutorialSequenceDone())
        return false;

    if (_isTutorialOpen)
        return false;

    if (currentState == BattleTurnState::ChooseAction)
    {
        const bool pressedBaseAttack = GAME->KeyDown(KEY_TYPE::F);
        const bool pressedSkill = GAME->KeyDown(KEY_TYPE::E);
        const bool pressedItem = GAME->KeyDown(KEY_TYPE::W);
        const bool pressedEscape = GAME->KeyDown(KEY_TYPE::C);
        const bool pressedSkipTurn = GAME->KeyDown(KEY_TYPE::TAB);
        const bool pressedFreeAim = GAME->KeyDown(KEY_TYPE::RBUTTON);

        if (!pressedBaseAttack && !pressedSkill && !pressedItem && !pressedEscape && !pressedSkipTurn && !pressedFreeAim)
            return false;

        switch (_sequenceStage)
        {
        case TutorialSequenceStage::WaitingBaseAttack:
            return !pressedBaseAttack; // 기본 공격만 기존 ChooseAction으로

        case TutorialSequenceStage::WaitingLumiereAssaultCommand:
            return !pressedSkill; // 스킬 메뉴 진입만 기존 ChooseAction으로

        case TutorialSequenceStage::WaitingFreeAimInput:
            return !pressedFreeAim; // 자유 조준 진입만 기존 ChooseAction으로

        default:
            return true;
        }
    }

    if (currentState == BattleTurnState::AttackTargeting)
    {
        if (_sequenceStage != TutorialSequenceStage::WaitingBaseAttack)
            return false;

        if (GAME->KeyDown(KEY_TYPE::F))
            return false;

        return GAME->KeyDown(KEY_TYPE::ESCAPE) ||
            GAME->KeyDown(KEY_TYPE::A) ||
            GAME->KeyDown(KEY_TYPE::D);
    }

    if (currentState == BattleTurnState::ChooseSkill)
    {
        if (_sequenceStage != TutorialSequenceStage::WaitingLumiereAssaultCommand)
            return false;

        const BattleChar& actor = turnRequest.actor;
        if (actor.statCom == nullptr)
            return false;

        if (GAME->KeyDown(KEY_TYPE::Q))
        {
            string skillKey{};
            if (actor.statCom->Try_GetSkillKeyBySlot(0, skillKey) && skillKey == kTutorialQTESkillKey)
                return false; // LumiereAssault 인지 확인

            return true;
        }

        return GAME->KeyDown(KEY_TYPE::W) ||
            GAME->KeyDown(KEY_TYPE::E) ||
            GAME->KeyDown(KEY_TYPE::ESCAPE);
    }

    if (currentState == BattleTurnState::SkillTargeting)
    {
        if (_sequenceStage != TutorialSequenceStage::WaitingLumiereAssaultCommand)
            return false;

        if (GAME->KeyDown(KEY_TYPE::F))
            return false; 

        return GAME->KeyDown(KEY_TYPE::ESCAPE) ||
            GAME->KeyDown(KEY_TYPE::A) ||
            GAME->KeyDown(KEY_TYPE::D);
    }

    return false;

}

void BattleTutorialController::Handle_TurnStarted(const BattleChar& actor, BattleTurnState state)
{
    if (actor.statCom == nullptr)
        return;

    if (_encounterId == Constants::Value::potatoBagEncounterId)
    {
        Request_Tutorial(BattleTutorialStep::Gradient);
        return; // 감자자루는 선턴이 몬스터여도 Gradient 안내를 전투 시작 카드로 먼저 띄운다.
    }

    if (!actor.statCom->Is_Player())
    {
        // 첫번째 공격 대기 -> 적 턴
        if (_sequenceStage == TutorialSequenceStage::WaitingFirstEnemyTurn)
        {
            _sequenceStage = TutorialSequenceStage::EnemyTurn;
            Request_Tutorial(BattleTutorialStep::EnemyTurn);
        }
        // 두 번 째 공격 대기 -> 바로 진입
        else if (_sequenceStage == TutorialSequenceStage::WaitingSecondEnemyTurn)
        {
            _sequenceStage = TutorialSequenceStage::WaitingCounterWindow;
        }

        return;
    }

    if (state != BattleTurnState::ChooseAction)
        return;

    // 전투 시스템
    if (_sequenceStage == TutorialSequenceStage::BattleSystem)
    {
        Request_Tutorial(BattleTutorialStep::BattleSystem);
    }
    // 스킬 턴
    else if (_sequenceStage == TutorialSequenceStage::WaitingSkillTurn)
    {
        _sequenceStage = TutorialSequenceStage::Skill;
        Request_Tutorial(BattleTutorialStep::Skill);
    }
    // 자유 조준
    else if (_sequenceStage == TutorialSequenceStage::WaitingFreeAimTurn)
    {
        _sequenceStage = TutorialSequenceStage::FreeAim;
        Request_Tutorial(BattleTutorialStep::FreeAim);
    }
}

void BattleTutorialController::Handle_ActionStarted(const BattleAction& action)
{
    const BattleChar& actor = action.Get_CurChar();

    if (actor.statCom == nullptr)
        return;

    if (!actor.statCom->Is_Player())
    {
        ++_tutorialEnemyActionCount;
        return;
    }

    if (_sequenceStage == TutorialSequenceStage::WaitingBaseAttack &&
        action.GetType() == EBattleContext::BaseAttack)
    {
        _sequenceStage = TutorialSequenceStage::WaitingFirstEnemyTurn;
        return;
    }

    if (_sequenceStage == TutorialSequenceStage::WaitingLumiereAssaultCommand &&
        action.GetType() == EBattleContext::Skill &&
        action.GetSkillData().skillKey == kTutorialQTESkillKey)
    {
        _sequenceStage = TutorialSequenceStage::WaitingQTE;
        return;
    }
}

void BattleTutorialController::Handle_BattleTurnStateChanged(BattleTurnState prevState, BattleTurnState nextState)
{
    if (_sequenceStage == TutorialSequenceStage::WaitingFreeAimInput &&
        nextState == BattleTurnState::FreeAimTargeting)
    {
        _sequenceStage = TutorialSequenceStage::Done; // 자유 조준 진입까지 확인하면 튜토리얼 시퀀스는 끝.
    }
}

void BattleTutorialController::Handle_ReactionWindowOpened(BattleReactionType reactionType)
{
    if (reactionType == BattleReactionType::Dodge &&
        _sequenceStage == TutorialSequenceStage::WaitingDodgeWindow)
    {
        Request_Tutorial(BattleTutorialStep::Dodge);
        return;
    }

    if (reactionType == BattleReactionType::Parry &&
        _sequenceStage == TutorialSequenceStage::WaitingCounterWindow)
    {
        Request_Tutorial(BattleTutorialStep::Counter);
        return;
    }
}

void BattleTutorialController::Handle_QTEStarted(const BattleQTERequest& request)
{
    UNREFERENCED_PARAMETER(request);

    if (_sequenceStage == TutorialSequenceStage::WaitingQTE)
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

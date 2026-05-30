#include "GameplayUI_Battle.h"

#include "Monster_BattleBase.h"
#include "ClientInstance.h"
#include "ClientScreenFxManager.h"
#include "CombatStatCom.h"
#include "DamageSystem.h"
#include "PlayerStatCom.h"

#include "UI_AttackTooltip.h"
#include "UI_BattleCommandHUD.h"
#include "UI_BattleDefense.h"
#include "UI_BattleMechanism.h"
#include "UI_BattleSystemHUD.h"
#include "UI_BattleSystem_TurnOrder.h"
#include "UI_BattleTransition.h"
#include "UI_CancelAim.h"
#include "UI_ChoiceTarget.h"
#include "UI_MonsterHpBar.h"
#include "UI_PlayerHpBar.h"
#include "UI_CrossHair.h"
#include "UI_DamageFont.h"
#include "UI_EscapeSkipTurnBack.h"
#include "UI_ReactionResult.h"
#include "UI_BattleVictory.h"
#include "UI_BattleItemPanel.h"
#include "UI_BattleSkillPanel.h"
#include "UI_HealFont.h"
#include "UI_BattleAnnounce.h"
#include "UI_SuddenAttack.h"

#include "UI_Utils.h"

using namespace UI_Utils;

GameplayUI_Battle::~GameplayUI_Battle() = default;

namespace
{
    void Append_RuntimePanelEntry(
        vector<RuntimeUIEntry>& outEntries,
        const char* name,
        UI_RuntimePanelBase* panel)
    {
        if (panel == nullptr)
            return;

        Shared<UI_Base> root = panel->Get_EditorRootUI();
        if (root == nullptr)
            return;

        outEntries.push_back(
            {
                "Battle",
                name,
                root,
                panel,
                panel->Is_EditorPositionDrivenByCode()
            });
    }
}

HRESULT GameplayUI_Battle::Initialize(uint32 levelIndex)
{
    auto Create_UI = [levelIndex](auto& ui, auto& createFn) -> HRESULT
        {
            ui = createFn(levelIndex);
            CHECK_NULL(ui, E_FAIL);
            return S_OK;
        };

    // 람다 의미없는거같기도 하고
    CHECK_FAILED(Create_UI(_battleTransition, UI_BattleTransition::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleCommandHUD, UI_BattleCommandHUD::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleSystemHUD, UI_BattleSystemHUD::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_monsterHpBar, UI_MonsterHpBar::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_playerHpBar, UI_PlayerHpBar::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_crossHair, UI_CrossHair::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleDefense, UI_BattleDefense::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_cancelAim, UI_CancelAim::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_choiceTarget, UI_ChoiceTarget::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_escapeSkipTurn, UI_EscapeSkipTurnBack::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_attackTooltip, UI_AttackTooltip::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_reactionResult, UI_ReactionResult::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleVictory, UI_BattleVictory::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_turnOrder, UI_BattleSystem_TurnOrder::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleMechanism, UI_BattleMechanism::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleItemPanel, UI_BattleItemPanel::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleSkillPanel, UI_BattleSkillPanel::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_battleAnnounce, UI_BattleAnnounce::Create), E_FAIL);
    CHECK_FAILED(Create_UI(_suddenAttack, UI_SuddenAttack::Create), E_FAIL);

    // 데미지 폰트 최대 8개
    for (auto& damageFont : _damageFonts)
    {
        damageFont = UI_DamageFont::Create(levelIndex);
        CHECK_NULL(damageFont, E_FAIL);
    }
    // 힐 폰트도 일단 최대 8개
    for (auto& healFont : _healFonts)
    {
        healFont = UI_HealFont::Create(levelIndex);
        CHECK_NULL(healFont, E_FAIL);
    }

    Bind_BattleDelegates();

    return S_OK;
}

void GameplayUI_Battle::Update(float timeDelta)
{
    Update_BattleCommandHUD(timeDelta); // 플레이어 전투 입력 허드
    Update_BattleSystemHUD(timeDelta);  // 파티 portrait HP/AP HUD
    Update_MonsterHpBar(timeDelta);     // 몬스터 체력
    Update_PlayerHpBar(timeDelta);
    Update_CrossHair(timeDelta);        // Free Aim 크로스헤어
    Update_BattleDefense(timeDelta);    // 몬스터 턴 방어 (좌측 하단)
    Update_CancelAim(timeDelta);        // Free Aim 취소
    Update_ChoiceTarget(timeDelta);     // 타겟 선택 UI (F 대상 선택, ESC 뒤로)
    Update_EscapeSkipTurn(timeDelta);   // ChooseAction 중 C 도망 / TAB 턴 넘기기
    Update_AttackTooltip(timeDelta);    // AttackTargeting 중 기본 공격 설명 툴팁
    Update_BattleMechanism(timeDelta);  // 플레이어 공격 액션 중 캐릭터별 메커니즘 UI
    Update_TurnOrder(timeDelta);        // 좌측상단 턴 UI
    Update_DamageFonts(timeDelta);      // 데미지 폰트
    Update_HealFonts(timeDelta);        // 힐 폰트
    Update_ReactionResult(timeDelta);   // 회피/패링 성공 텍스트 수명 갱신
    Update_BattleVictory(timeDelta);    // 전투 종료 승리 UI
    Update_BattleItemPanel(timeDelta);  // 아이템
    Update_BattleSkillPanel(timeDelta); // 스킬
    Update_BattleAnnounce(timeDelta);   // 스킬
    Update_SuddenAttack(timeDelta);     // 선제 공격

    if (_battleTransition)
        _battleTransition->Update(timeDelta);
}

void GameplayUI_Battle::Update_Input()
{

}

vector<RuntimeUIEntry> GameplayUI_Battle::Get_RuntimeUIEntries() const
{
    vector<RuntimeUIEntry> entries{};
    entries.reserve(18 + static_cast<uint32>(_damageFonts.size()) + static_cast<uint32>(_healFonts.size()));

    Append_RuntimePanelEntry(entries, "BattleTransition", _battleTransition.get());
    Append_RuntimePanelEntry(entries, "BattleCommandHUD", _battleCommandHUD.get());
    Append_RuntimePanelEntry(entries, "BattleSystemHUD", _battleSystemHUD.get());
    Append_RuntimePanelEntry(entries, "MonsterHpBar", _monsterHpBar.get());
    Append_RuntimePanelEntry(entries, "PlayerHpBar", _playerHpBar.get());
    Append_RuntimePanelEntry(entries, "CrossHair", _crossHair.get());
    Append_RuntimePanelEntry(entries, "BattleDefense", _battleDefense.get());
    Append_RuntimePanelEntry(entries, "CancelAim", _cancelAim.get());
    Append_RuntimePanelEntry(entries, "ChoiceTarget", _choiceTarget.get());
    Append_RuntimePanelEntry(entries, "EscapeSkipTurn", _escapeSkipTurn.get());
    Append_RuntimePanelEntry(entries, "AttackTooltip", _attackTooltip.get());
    Append_RuntimePanelEntry(entries, "ReactionResult", _reactionResult.get());
    Append_RuntimePanelEntry(entries, "BattleVictory", _battleVictory.get());
    Append_RuntimePanelEntry(entries, "TurnOrder", _turnOrder.get());
    Append_RuntimePanelEntry(entries, "BattleMechanism", _battleMechanism.get());
    Append_RuntimePanelEntry(entries, "BattleItemPanel", _battleItemPanel.get());
    Append_RuntimePanelEntry(entries, "BattleSkillPanel", _battleSkillPanel.get());
    Append_RuntimePanelEntry(entries, "BattleAnnounce", _battleAnnounce.get());
    Append_RuntimePanelEntry(entries, "SuddenAttack", _suddenAttack.get());

    for (uint32 i = 0; i < _damageFonts.size(); ++i)
        Append_RuntimePanelEntry(entries, ("DamageFont_" + to_string(i)).c_str(), _damageFonts[i].get());

    for (uint32 i = 0; i < _healFonts.size(); ++i)
        Append_RuntimePanelEntry(entries, ("HealFont_" + to_string(i)).c_str(), _healFonts[i].get());

    return entries;
}

void GameplayUI_Battle::Play_BattleEnterTransition()
{
    CLIENT->Get_ScreenFxManager()->Transition_To_BattleField();
}

void GameplayUI_Battle::Stop_BattleEnterTransition()
{
    if (_battleTransition)
        _battleTransition->Close();
}

bool GameplayUI_Battle::Is_BattleEnterTransitionPlaying() const
{
    return _battleTransition && _battleTransition->Is_Playing();
}

void GameplayUI_Battle::Handle_EditorStopRuntime()
{
    Reset_BattleUiRuntimeState(true);
}

bool GameplayUI_Battle::Needs_RuntimeUpdate() const
{
    if (CLIENT->Is_BattleEntering())
        return true;

    if (CLIENT->Is_BattleActive())
        return true;

    if (_isVictoryUiActive)
        return true;

    if (Is_BattleEnterTransitionPlaying())
        return true;

    return false;
}

void GameplayUI_Battle::Bind_BattleDelegates()
{
    _battleController = CLIENT->Get_BattleController();
    CHECK_NULL(_battleController);

    _battleStartedHandle =
        _battleController->OnBattleStarted.Add(this, &GameplayUI_Battle::Handle_BattleStarted);

    _battleEndedHandle =
        _battleController->OnBattleEnded.Add(this, &GameplayUI_Battle::Handle_BattleEnded);

    _turnStartedHandle =
        _battleController->OnTurnStarted.Add(this, &GameplayUI_Battle::Handle_TurnStarted);

    _turnEndedHandle =
        _battleController->OnTurnEnded.Add(this, &GameplayUI_Battle::Handle_TurnEnded);

    _targetPreviewChangedHandle =
        _battleController->OnTargetPreviewChanged.Add(this, &GameplayUI_Battle::Handle_TargetPreviewChanged);

    _actionStartedHandle =
        _battleController->OnActionStarted.Add(this, &GameplayUI_Battle::Handle_ActionStarted);

    _damageAppliedHandle =
        DamageSystem::OnDamageApplied.Add(this, &GameplayUI_Battle::Handle_DamageApplied);

    _healAppliedHandle =
        DamageSystem::OnHealApplied.Add(this, &GameplayUI_Battle::Handle_HealApplied);

    _reactionSucceededHandle =
        _battleController->OnReactionSucceeded.Add(this, &GameplayUI_Battle::Handle_ReactionSucceeded);
}

void GameplayUI_Battle::Unbind_BattleDelegates()
{
    CHECK_NULL(_battleController);

    if (_battleStartedHandle.IsValid())
        _battleController->OnBattleStarted.Remove(_battleStartedHandle);

    if (_battleEndedHandle.IsValid())
        _battleController->OnBattleEnded.Remove(_battleEndedHandle);

    if (_turnStartedHandle.IsValid())
        _battleController->OnTurnStarted.Remove(_turnStartedHandle);

    if (_turnEndedHandle.IsValid())
        _battleController->OnTurnEnded.Remove(_turnEndedHandle);

    if (_targetPreviewChangedHandle.IsValid())
        _battleController->OnTargetPreviewChanged.Remove(_targetPreviewChangedHandle);

    if (_actionStartedHandle.IsValid())
        _battleController->OnActionStarted.Remove(_actionStartedHandle);

    if (_damageAppliedHandle.IsValid())
        DamageSystem::OnDamageApplied.Remove(_damageAppliedHandle);

    if (_healAppliedHandle.IsValid())
        DamageSystem::OnHealApplied.Remove(_healAppliedHandle);

    if (_reactionSucceededHandle.IsValid())
        _battleController->OnReactionSucceeded.Remove(_reactionSucceededHandle);

    _battleStartedHandle.Reset();
    _battleEndedHandle.Reset();
    _turnStartedHandle.Reset();
    _turnEndedHandle.Reset();
    _targetPreviewChangedHandle.Reset();
    _actionStartedHandle.Reset();
    _damageAppliedHandle.Reset();
    _healAppliedHandle.Reset();
    _reactionSucceededHandle.Reset();

    _battleController.reset();
}

void GameplayUI_Battle::Handle_BattleStarted(uint32 fieldId, bool isPlayerAdvantage)
{
    _isBattleUiActive = true;
    _isPlayerTurnUiActive = false;
    _hasTargetPreviewSnapshot = false;
    _targetPreviewSnapshot = {};
    _hasCurrentPlayerTurnActor = false;
    _currentPlayerTurnActor = {};
    _isMechanismActionActive = false;
    _activePlayerId = HeroId::END;
    _battleStartVictoryStats.clear();
    _battleStartVictoryStats = Build_BattleVictoryPlayerStats();

    // 플레이어 선제공격인지
    _isPlayerAdvantageStart = isPlayerAdvantage;
    _hasPlayedSuddenAttack = false;

    UI_RuntimePanelBase* panelsToClose[] =
    {
        // 턴 시작 시 닫아야할 UI들이 있다면 여기에 추가
        _battleCommandHUD.get(),
        _battleSystemHUD.get(),
        _monsterHpBar.get(),
        _playerHpBar.get(),
        _crossHair.get(),
        _battleDefense.get(),
        _cancelAim.get(),
        _choiceTarget.get(),
        _reactionResult.get(),
        _battleMechanism.get(),
        _battleAnnounce.get(),
        _suddenAttack.get(),
    };

    for (UI_RuntimePanelBase* panel : panelsToClose)
    {
        if (panel)
            panel->Close();
    }

    // 이전 전투에서 남은 배틀 슬롯 정리
    for (auto& damageFont : _damageFonts)
    {
        if (damageFont)
            damageFont->Close();
    }

    Play_BattleEnterTransition(); // 전투 진입 연출은 battle UI 쪽에서 시작한다.
}

void GameplayUI_Battle::Handle_BattleEnded(EBattleEndResult result)
{
    Reset_BattleUiRuntimeState(result != EBattleEndResult::Victory);

    if (result == EBattleEndResult::Victory)
    {
        _isVictoryUiActive = true; // 실제 전투 승리 때만 결과창 갱신/표시를 허용

        if (_battleVictory)
        {
            const uint32 rewardExp = _battleController ? _battleController->Get_LastVictoryRewardExp() : 0;

            _battleVictory->Set_RewardExp(rewardExp);
            _battleVictory->Set_PlayerStatus(Build_BattleVictoryPlayerStats());
            _battleVictory->Open();
        }

        return;
    }
}

void GameplayUI_Battle::Handle_TurnStarted(const BattleChar& actor, BattleTurnState state)
{
    _isMechanismActionActive = false;
    _activePlayerId = HeroId::END;

    // 턴 시작 시, 메커니즘 닫아주고
    if (_battleMechanism)
        _battleMechanism->Close();

    // 플레이어 턴인지
    _isPlayerTurnUiActive =
        (actor.statCom != nullptr && actor.statCom->Is_Player());

    if (_isPlayerTurnUiActive)
    {
        _currentPlayerTurnActor = actor;
        _hasCurrentPlayerTurnActor = true;
        _activePlayerId = actor.heroId;

        if (_battleMechanism)
            _battleMechanism->Show_Hero(_activePlayerId);

    }
    else
    {
        _currentPlayerTurnActor = {};
        _hasCurrentPlayerTurnActor = false;
    }

    if (_battleCommandHUD)
    {
        if (_isPlayerTurnUiActive)
            _battleCommandHUD->Open();
        else
            _battleCommandHUD->Close();
    }

    if (_suddenAttack &&
        _isPlayerAdvantageStart &&
        !_hasPlayedSuddenAttack &&
        actor.statCom != nullptr &&
        actor.statCom->Is_Player())
    {
        _suddenAttack->Play_SuddenAttack();
        _hasPlayedSuddenAttack = true; // 첫 플레이어 턴에서 1회만 재생
    }

}

void GameplayUI_Battle::Handle_TurnEnded(const BattleChar& actor)
{
    _isPlayerTurnUiActive = false;
    _hasCurrentPlayerTurnActor = false;
    _currentPlayerTurnActor = {};
    _isMechanismActionActive = false;
    _activePlayerId = HeroId::END;

    UI_RuntimePanelBase* panelsToClose[] =
    {
        _battleCommandHUD.get(),
        _monsterHpBar.get(),
        _crossHair.get(),
        _battleDefense.get(),
        _cancelAim.get(),
        _choiceTarget.get(),
        _reactionResult.get(),
        _battleMechanism.get(),
        _battleAnnounce.get(),
        _suddenAttack.get(),
    };

    for (auto* panel : panelsToClose)
    {
        if (panel)
            panel->Close();
    }
}

void GameplayUI_Battle::Handle_TargetPreviewChanged(const BattleChar* target)
{
    if (target == nullptr || target->actor == nullptr || target->statCom == nullptr)
    {
        _targetPreviewSnapshot = {};
        _hasTargetPreviewSnapshot = false;

        return;
    }

    _targetPreviewSnapshot = *target;
    _hasTargetPreviewSnapshot = true;
}

void GameplayUI_Battle::Handle_ActionStarted(const BattleAction& action)
{
    if (_choiceTarget)
        _choiceTarget->Close();

    // 공격 시작하면, 몬스터 Hp bar 안보이게 하네
    if (_monsterHpBar)
        _monsterHpBar->Clear_TrackedTarget();

    const BattleChar& actor = action.Get_CurChar();

    const bool isPlayerAction =
        actor.statCom != nullptr &&
        actor.statCom->Is_Player();

    const bool isAttackAction =
        action.GetType() == EBattleContext::BaseAttack ||
        action.GetType() == EBattleContext::Skill;

    _isMechanismActionActive = isPlayerAction && isAttackAction;
    _activePlayerId = _isMechanismActionActive ? actor.heroId : HeroId::END;

    if (!_isMechanismActionActive && _battleMechanism)
        _battleMechanism->Close();

    if (_battleAnnounce && isAttackAction)
        _battleAnnounce->Show_TurnStart(action.Get_CurChar(), BattleTurnState::AttackPlaying);
}

void GameplayUI_Battle::Handle_DamageApplied(const BattleChar& target, float damage, AttackType attackType)
{
    if (!_isBattleUiActive)
        return;

    if (damage <= 0.f)
        return;

    if (target.statCom != nullptr)
    {
        if (target.statCom->Is_Player())
        {
            if (_playerHpBar)
                _playerHpBar->Show_DamagedTarget(target, damage, 1.5f);
        }
        else
        {
            if (_monsterHpBar)
                _monsterHpBar->Show_DamagedTarget(target, damage, 1.5f);
        }
    }


    Vec3 worldPos{};
    if (!Try_GetDamageFontWorldPos(target, worldPos))
        return;

    Vec2 screenPos{};
    if (!UI_Utils::Try_ProjectWorldToScreen(worldPos, screenPos))
        return; // 피격 순간에 카메라 밖이면 숫자를 띄우지 않는다.

    UI_DamageFont* selectedSlot = nullptr;
    UI_DamageFont* oldestSlot = nullptr;
    float longestElapsed = -1.f;
    uint32 stackIndex = 0;

    for (auto& damageFont : _damageFonts)
    {
        if (!damageFont)
            continue;

        if (damageFont->Is_Playing())
        {
            ++stackIndex; // 이미 떠 있는 숫자 수만큼 살짝 더 벌린다.

            if (damageFont->Get_Elapsed() > longestElapsed)
            {
                longestElapsed = damageFont->Get_Elapsed();
                oldestSlot = damageFont.get();
            }

            continue;
        }

        if (selectedSlot == nullptr)
            selectedSlot = damageFont.get();
    }

    if (selectedSlot == nullptr)
        selectedSlot = oldestSlot;

    if (selectedSlot == nullptr)
        return;

    stackIndex = min(stackIndex, kDamageFontPoolSize - 1);
    selectedSlot->Play_DamageFont(screenPos, damage, stackIndex);
}

void GameplayUI_Battle::Handle_HealApplied(const BattleChar& target, float healAmount)
{
    if (!_isBattleUiActive)
        return;

    if (healAmount <= 0.f)
        return;

    Vec3 worldPos{};
    if (!Try_GetHealFontWorldPos(target, worldPos))
        return;

    Vec2 screenPos{};
    if (!UI_Utils::Try_ProjectWorldToScreen(worldPos, screenPos))
        return;

    UI_HealFont* selectedSlot = nullptr;
    UI_HealFont* oldestSlot = nullptr;
    float longestElapsed = -1.f;
    uint32 stackIndex = 0;

    for (auto& healFont : _healFonts)
    {
        if (!healFont)
            continue;

        if (healFont->Is_Playing())
        {
            ++stackIndex;

            if (healFont->Get_Elapsed() > longestElapsed)
            {
                longestElapsed = healFont->Get_Elapsed();
                oldestSlot = healFont.get();
            }

            continue;
        }

        if (selectedSlot == nullptr)
            selectedSlot = healFont.get();
    }

    if (selectedSlot == nullptr)
        selectedSlot = oldestSlot;

    if (selectedSlot == nullptr)
        return;

    stackIndex = min(stackIndex, kHealFontPoolSize - 1);
    selectedSlot->Play_HealFont(screenPos, healAmount, stackIndex);
}

void GameplayUI_Battle::Handle_ReactionSucceeded(BattleController::ReactionType reactionType, const vector<BattleChar>& target)
{
    if (!_isBattleUiActive || _reactionResult == nullptr)
        return;

    Vec3 worldPos{};
    for(auto& targetChar: target)
    {
    if (!Try_GetReactionResultWorldPos(targetChar, worldPos))
        return;
        
    }


    Vec2 screenPos{};
    if (!Try_ProjectWorldToScreen(worldPos, screenPos))
        return;

    _reactionResult->Show_Reaction(reactionType, screenPos);
}

void GameplayUI_Battle::Update_BattleCommandHUD(float timeDelta)
{
    if (_battleCommandHUD == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive)
    {
        _battleCommandHUD->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _battleCommandHUD->Close();
        return;
    }

    const BattleTurnState currentState = battleController->Get_CurrentTurnState();

    if (currentState != BattleTurnState::ChooseAction)
    {
        _battleCommandHUD->Close(); // 공격 중에는 커맨드 HUD 숨기기
        return;
    }

    if (!_hasCurrentPlayerTurnActor || _currentPlayerTurnActor.statCom == nullptr)
    {
        _battleCommandHUD->Close();
        return;
    }

    Vec3 worldPos{};
    if (!Try_GetBattleCommandHudWorldPos(_currentPlayerTurnActor, worldPos))
    {
        _battleCommandHUD->Close();
        return;
    }

    _battleCommandHUD->Open();
    _battleCommandHUD->Set_TargetWorldPosition(worldPos);

    // 현재 턴 영웅을 HUD에 넘겨 Hero별 오프셋 세팅이 되게
    _battleCommandHUD->Set_CurrentHeroId(_currentPlayerTurnActor.heroId);

    _battleCommandHUD->Set_CurrentPlayerAP(
        _currentPlayerTurnActor.statCom->Get_CurAP(),
        _currentPlayerTurnActor.statCom->Get_MaxAP());

    _battleCommandHUD->Refresh_ForState(currentState);

    _battleCommandHUD->Update(timeDelta);
}

void GameplayUI_Battle::Update_BattleSystemHUD(float timeDelta)
{
    if (_battleSystemHUD == nullptr)
        return;

    if (!_isBattleUiActive)
    {
        _battleSystemHUD->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _battleSystemHUD->Close();
        return;
    }

    _battleSystemHUD->Open(); // 전투 중에는 파티 portrait HUD를 계속 유지한다.

    _battleSystemHUD->Set_PartyBattleStat(PlayerCharacterType::Gustave, 0.f, 1.f, 0u, 1u);
    _battleSystemHUD->Set_PartyBattleStat(PlayerCharacterType::Lune, 0.f, 1.f, 0u, 1u);
    _battleSystemHUD->Set_PartyBattleStat(PlayerCharacterType::Maelle, 0.f, 1.f, 0u, 1u);
    _battleSystemHUD->Clear_GustaveMechanismCharge();
    _battleSystemHUD->Clear_MaelleMechanismStance();

    const vector<BattleChar>& heroes = battleController->Get_BattleHeroes();
    for (const BattleChar& hero : heroes)
    {
        if (hero.statCom == nullptr)
            continue;

        _battleSystemHUD->Set_PartyBattleStat(
            hero.statCom->Get_PlayerType(),
            hero.statCom->Get_CurHp(),
            hero.statCom->Get_StatInfo().maxHp,
            hero.statCom->Get_CurAP(),
            hero.statCom->Get_MaxAP());

        if (hero.heroId == HeroId::Gustave)
        {
            _battleSystemHUD->Set_GustaveMechanismCharge(hero.mechanismCom);
        }
        else if (hero.heroId == HeroId::Lune)
        {
            // TODO : 루네
        }
        else if (hero.heroId == HeroId::Maelle)
        {
            _battleSystemHUD->Set_MaelleMechanismStance(hero.mechanismCom);
        }
    }

    _battleSystemHUD->Update(timeDelta);
}

void GameplayUI_Battle::Update_MonsterHpBar(float timeDelta)
{
    if (_monsterHpBar == nullptr)
        return;

    if (!_isBattleUiActive)
    {
        _monsterHpBar->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    const BattleChar* trackedTarget = nullptr;

    if (battleController != nullptr)
    {
        trackedTarget = battleController->Get_FreeAimHitTarget();

        if (trackedTarget == nullptr)
            trackedTarget = battleController->Get_TargetPreview();
    }

    if (trackedTarget != nullptr)
        _monsterHpBar->Show_TrackedTarget(*trackedTarget);
    else
        _monsterHpBar->Clear_TrackedTarget();

    _monsterHpBar->Update(timeDelta);
}

void GameplayUI_Battle::Update_PlayerHpBar(float timeDelta)
{
    if (_playerHpBar == nullptr)
        return;

    if (!_isBattleUiActive)
    {
        _playerHpBar->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    const BattleChar* trackedTarget = nullptr;
    const BattleTurnState currentState =
        battleController ? battleController->Get_CurrentTurnState() : BattleTurnState::None;

    if (currentState == BattleTurnState::ItemTargeting)
    {
        trackedTarget = battleController->Get_TargetPreview();
    }

    if (trackedTarget != nullptr &&
        trackedTarget->statCom != nullptr &&
        trackedTarget->statCom->Is_Player())
    {
        _playerHpBar->Show_TrackedTarget(*trackedTarget);
    }
    else
    {
        _playerHpBar->Clear_TrackedTarget();
    }

    _playerHpBar->Update(timeDelta);
}

void GameplayUI_Battle::Update_CrossHair(float timeDelta)
{
    if (_crossHair == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive)
    {
        _crossHair->Close(); // 플레이어 턴 UI가 아닐 때는 Free Aim 상태가 남아 있어도 크로스헤어를 숨긴다.
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _crossHair->Close();
        return;
    }

    if (battleController->Get_CurrentTurnState() != BattleTurnState::FreeAimTargeting)
    {
        _crossHair->Close();
        return;
    }

    _crossHair->Open();
    _crossHair->Set_HitState(battleController->Has_FreeAimHitTarget());

    _crossHair->Update(timeDelta);
}

void GameplayUI_Battle::Update_BattleDefense(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (_battleDefense == nullptr)
        return;

    if (!_isBattleUiActive)
    {
        _battleDefense->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _battleDefense->Close();
        return;
    }

    const BattleChar& currentActor = battleController->Get_TurnAction().Get_CurChar();
    if (currentActor.actor == nullptr || currentActor.statCom == nullptr)
    {
        _battleDefense->Close();
        return;
    }

    if (currentActor.statCom->Is_Player())
    {
        _battleDefense->Close(); // 플레이어 턴에는 Defense UI 숨기기
        return;
    }

    _battleDefense->Open(); // 몬스터 턴일 동안 유지
    _battleDefense->Update(timeDelta);

}

void GameplayUI_Battle::Update_CancelAim(float timeDelta)
{
    if (_cancelAim == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive)
    {
        _cancelAim->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _cancelAim->Close();
        return;
    }

    if (battleController->Get_CurrentTurnState() != BattleTurnState::FreeAimTargeting)
    {
        _cancelAim->Close();
        return;
    }

    _cancelAim->Open(); // Free Aim 중에는 크로스헤어, 캔슬 UI
    _cancelAim->Update(timeDelta);
}

void GameplayUI_Battle::Update_ChoiceTarget(float timeDelta)
{
    if (_choiceTarget == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive)
    {
        _choiceTarget->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _choiceTarget->Close();
        return;
    }

    const BattleTurnState turnState = battleController->Get_CurrentTurnState();

    if (turnState == BattleTurnState::AttackTargeting ||
        turnState == BattleTurnState::SkillTargeting)
    {
        _choiceTarget->Open();
        _choiceTarget->Update(timeDelta);
        return;
    }

    if (turnState != BattleTurnState::ItemTargeting)
    {
        _choiceTarget->Close();
        return;
    }

    const BattleAction& turnAction = battleController->Get_TurnAction();
    const BattleChar& actor = turnAction.Get_CurChar();
    if (actor.ModelCom != nullptr &&
        actor.ModelCom->Get_AnimationStateIndex() == ETOI(BattleAnimType::Item_Use))
    {
        _choiceTarget->Close();
        return;
    }

    _choiceTarget->Open();
    _choiceTarget->Update(timeDelta);
}

void GameplayUI_Battle::Update_TurnOrder(float timeDelta)
{
    if (_turnOrder == nullptr)
        return;

    if (!_isBattleUiActive || _battleController == nullptr)
    {
        _turnOrder->Clear_TurnOrderPreview();
        _turnOrder->Close();
        return;
    }

    _turnOrder->Open();
    _turnOrder->Set_TurnOrderPreview(Build_TurnOrderEntries());
    _turnOrder->Update(timeDelta);
}

void GameplayUI_Battle::Update_DamageFonts(float timeDelta)
{
    for (auto& damageFont : _damageFonts)
    {
        if (damageFont)
            damageFont->Update(timeDelta);
    }
}

void GameplayUI_Battle::Update_HealFonts(float timeDelta)
{
    for (auto& healFont : _healFonts)
    {
        if (healFont)
            healFont->Update(timeDelta);
    }
}

void GameplayUI_Battle::Update_EscapeSkipTurn(float timeDelta)
{
    if (_escapeSkipTurn == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive)
    {
        _escapeSkipTurn->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _escapeSkipTurn->Close();
        return;
    }

    const BattleTurnState turnState = battleController->Get_CurrentTurnState();

    if (turnState != BattleTurnState::ChooseAction &&
        turnState != BattleTurnState::ChooseItem)
    {
        _escapeSkipTurn->Close();
        return;
    }

    _escapeSkipTurn->Open();
    _escapeSkipTurn->Update(timeDelta);
}

void GameplayUI_Battle::Update_AttackTooltip(float timeDelta)
{
    if (_attackTooltip == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive)
    {
        _attackTooltip->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _attackTooltip->Close();
        return;
    }

    if (battleController->Get_CurrentTurnState() != BattleTurnState::AttackTargeting)
    {
        _attackTooltip->Close();
        return;
    }

    // F 기본 공격으로 AttackTargeting에 들어온 동안만 설명 툴팁을 유지
    _attackTooltip->Open();
    _attackTooltip->Update(timeDelta);
}

void GameplayUI_Battle::Update_ReactionResult(float timeDelta)
{
    if (_reactionResult == nullptr)
        return;

    if (!_isBattleUiActive)
    {
        _reactionResult->Close();
        return;
    }

    _reactionResult->Update(timeDelta);
}

void GameplayUI_Battle::Update_BattleVictory(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta); // 월드 timeDelta 말고, 배속 적용 전 Unscaled Time 써야함

    if (_battleVictory == nullptr)
        return;

    if (!_isVictoryUiActive)
    {
        _battleVictory->Close();
        return;
    }

    _battleVictory->Open();

    const float uiTimeDelta = GAME->Get_UnscaledTimeDelta();
    _battleVictory->Update(uiTimeDelta);

    if (GAME->KeyDown(KEY_TYPE::F))
    {
        _isVictoryUiActive = false;
        _battleVictory->Close();

        if (_battleController)
        {
            _battleController->Confirm_Victory();
        }
    }
}

void GameplayUI_Battle::Update_BattleMechanism(float timeDelta)
{
    if (_battleMechanism == nullptr)
        return;

    if (!_isBattleUiActive || !_isPlayerTurnUiActive || _activePlayerId == HeroId::END)
    {
        _battleMechanism->Close();
        return;
    }

    const auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_BattleStarted())
    {
        _battleMechanism->Close();
        return;
    }

    const BattleTurnState state = battleController->Get_CurrentTurnState();

    if (state == BattleTurnState::ChooseAction ||
        state == BattleTurnState::AttackTargeting)
    {
        _battleMechanism->Show_Hero(_activePlayerId);
        _battleMechanism->Sync_BattleChar(_currentPlayerTurnActor);
        _battleMechanism->Update(timeDelta);
        return;
    }

    if (state != BattleTurnState::AttackPlaying)
    {
        _battleMechanism->Close();
        return;
    }

    const BattleAction& action = battleController->Get_TurnAction();
    const vector<BattleActionPhase>& phases = action.GetSkillData().AnimPhases;

    if (action.Get_CurChar().actor == nullptr || phases.empty() || action.GetCurAnimIndex() >= phases.size())
    {
        _battleMechanism->Close();
        return;
    }

    const BattleActionPhase& phase = phases[action.GetCurAnimIndex()];

    // 대쉬 Out 시작하면 메커니즘 UI 닫기
    if (phase.movementPolicy == BattleMovementPolicy::ReturnToOrigin)
    {
        _isMechanismActionActive = false;
        _activePlayerId = HeroId::END;
        _battleMechanism->Close();
        return;
    }

    _battleMechanism->Show_Hero(_activePlayerId);
    _battleMechanism->Sync_BattleChar(action.Get_CurChar());
    _battleMechanism->Update(timeDelta);
}

void GameplayUI_Battle::Update_BattleItemPanel(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (_battleItemPanel == nullptr)
        return;

    auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_PlayerTurnActive())
    {
        _battleItemPanel->Close();
        return;
    }

    const Item_Data* healing = CLIENT->Get_Item(BattleItemType::HealingTint);
    const Item_Data* energy = CLIENT->Get_Item(BattleItemType::EnergyTint);
    const Item_Data* revive = CLIENT->Get_Item(BattleItemType::ReviveTint);

    if (healing && energy && revive)
        _battleItemPanel->Refresh_ItemTexts(*healing, *energy, *revive);

    auto turnState = battleController->Get_CurrentTurnState();

    switch (turnState)
    {
    case BattleTurnState::ChooseItem:
        _battleItemPanel->Open_MainPanel();
        break;

    case BattleTurnState::ItemTargeting:
        _battleItemPanel->Open_Tooltip(battleController->Get_TurnActionRequest().itemType);
        break;

    default:
        _battleItemPanel->Close();
        break;
    }
}

void GameplayUI_Battle::Update_BattleSkillPanel(float timeDelta)
{
    if (_battleSkillPanel == nullptr)
        return;

    auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr || !battleController->Is_PlayerTurnActive())
    {
        _battleSkillPanel->Close();
        return;
    }

    const BattleTurnState turnState = battleController->Get_CurrentTurnState();
    const ActionRequest& turnRequest = battleController->Get_TurnActionRequest();

    switch (turnState)
    {
    case BattleTurnState::ChooseSkill:
        if (turnRequest.actor.statCom)
        {
            // 캐릭터별 스킬 목록 패널
            _battleSkillPanel->Open_MainPanel(turnRequest.actor.statCom->Get_PlayerType());
            _battleSkillPanel->Update(timeDelta);
        }
        else
        {
            _battleSkillPanel->Close();
        }
        break;

    case BattleTurnState::SkillTargeting:
        _battleSkillPanel->Open_Tooltip(turnRequest.skillKey);
        _battleSkillPanel->Update(timeDelta);
        break;

    default:
        _battleSkillPanel->Close();
        break;
    }
}

void GameplayUI_Battle::Update_BattleAnnounce(float timeDelta)
{
    if (_battleAnnounce == nullptr)
        return;

    _battleAnnounce->Update(timeDelta);
}

void GameplayUI_Battle::Update_SuddenAttack(float timeDelta)
{
    if (_suddenAttack == nullptr)
        return;

    _suddenAttack->Update(timeDelta);
}

bool GameplayUI_Battle::Try_GetTargetHpBarWorldPos(const BattleChar& target, Vec3& outWorldPos) const
{
    if (target.actor == nullptr || target.actor->Get_Transform() == nullptr)
        return false;

    outWorldPos = target.actor->Get_Transform()->Get_WorldPosition();
    outWorldPos.y += 1.5f; // 임시 테스트 위치
    return true;
}

bool GameplayUI_Battle::Try_GetBattleCommandHudWorldPos(const BattleChar& actor, Vec3& outWorldPos) const
{
    if (actor.actor == nullptr || actor.actor->Get_Transform() == nullptr)
        return false;

    outWorldPos = actor.actor->Get_Transform()->Get_WorldPosition();
    outWorldPos.y += 0.8f;

    return true;
}

bool GameplayUI_Battle::Try_GetDamageFontWorldPos(const BattleChar& target, Vec3& outWorldPos) const
{
    if (target.actor == nullptr || target.actor->Get_Transform() == nullptr)
        return false;

    outWorldPos = target.actor->Get_Transform()->Get_WorldPosition();
    outWorldPos.y += 1.1f;

    return true;
}

bool GameplayUI_Battle::Try_GetReactionResultWorldPos(const BattleChar& target, Vec3& outWorldPos)
{
    if (target.actor == nullptr || target.actor->Get_Transform() == nullptr)
        return false;

    outWorldPos = target.actor->Get_Transform()->Get_WorldPosition();
    outWorldPos.y += 1.25f;

    return true;
}

bool GameplayUI_Battle::Try_GetHealFontWorldPos(const BattleChar& target, Vec3& outWorldPos) const
{
    if (target.actor == nullptr || target.actor->Get_Transform() == nullptr)
        return false;

    outWorldPos = target.actor->Get_Transform()->Get_WorldPosition();
    outWorldPos.y += 1.0f;

    return true;
}

vector<GameplayUI_Battle::TurnDesc> GameplayUI_Battle::Build_TurnOrderEntries() const
{
    vector<UI_BattleSystem_TurnOrder::TurnOrderEntryDesc> entries{};

    if (_battleController == nullptr)
        return entries;

    const vector<BattleChar> preview = _battleController->Get_TurnOrderPreview(8);

    for (const BattleChar& actor : preview)
    {
        UI_BattleSystem_TurnOrder::TurnOrderEntryDesc entry{};
        entry.portraitGuid = Resolve_TurnOrderPortraitGuid(actor);
        entry.isEnemy = actor.statCom != nullptr && !actor.statCom->Is_Player();

        entries.push_back(entry);
    }

    return entries;
}

string GameplayUI_Battle::Resolve_TurnOrderPortraitGuid(const BattleChar& actor)
{
    if (actor.statCom == nullptr)
        return {};

    // 프리펩에서 이미지 세팅
    return actor.statCom->Get_StatInfo().portraitGuid;
}

vector<GameplayUI_Battle::VictoryStatDesc> GameplayUI_Battle::Build_BattleVictoryPlayerStats() const
{
    vector<VictoryStatDesc> victoryStats{};

    if (_battleController == nullptr)
        return victoryStats;

    for (const BattleChar& hero : _battleController->Get_BattleHeroes())
    {
        const Shared<PlayerStatCom> playerStat =
            dynamic_pointer_cast<PlayerStatCom>(hero.statCom);

        if (playerStat == nullptr)
            continue;

        const PlayerStatCom::PRuntimeCombatStat& runtimeStat =
            playerStat->Get_PlayerRuntimeStat();

        VictoryStatDesc stat{};
        stat.playerType = playerStat->Get_PlayerType();
        stat.level = runtimeStat.CurLevel;
        stat.curExp = runtimeStat.CurExp;
        stat.maxExp = runtimeStat.CurMaxExp;
        stat.prevLevel = stat.level;
        stat.prevExp = stat.curExp;
        stat.prevMaxExp = stat.maxExp;

        for (const VictoryStatDesc& startStat : _battleStartVictoryStats)
        {
            if (startStat.playerType != stat.playerType)
                continue;

            stat.prevLevel = startStat.level;
            stat.prevExp = startStat.curExp;
            stat.prevMaxExp = startStat.maxExp;
            break;
        }

        victoryStats.push_back(stat);
    }

    return victoryStats;
}

void GameplayUI_Battle::Reset_BattleUiRuntimeState(bool closeVictoryUi)
{
    _isBattleUiActive = false;      
    _isPlayerTurnUiActive = false;
    _hasTargetPreviewSnapshot = false;
    _targetPreviewSnapshot = {};
    _hasCurrentPlayerTurnActor = false;
    _currentPlayerTurnActor = {};
    _isVictoryUiActive = false;
    _isMechanismActionActive = false;
    _activePlayerId = HeroId::END;
    _battleStartVictoryStats.clear();

    UI_RuntimePanelBase* panelsToClose[] =
    {
        _battleCommandHUD.get(),
        _battleSystemHUD.get(),
        _monsterHpBar.get(),
        _playerHpBar.get(),
        _crossHair.get(),
        _battleDefense.get(),
        _cancelAim.get(),
        _choiceTarget.get(),
        _escapeSkipTurn.get(),
        _attackTooltip.get(),
        _reactionResult.get(),
        _turnOrder.get(),
        _battleMechanism.get(),
        _battleItemPanel.get(),
        _battleSkillPanel.get(),
        _battleAnnounce.get(),
        _suddenAttack.get(),
    };

    for (auto* panel : panelsToClose)
    {
        if (panel)
            panel->Close();
    }

    for (auto& damageFont : _damageFonts)
    {
        if (damageFont)
            damageFont->Close();
    }

    for (auto& healFont : _healFonts)
    {
        if (healFont)
            healFont->Close();
    }

    Stop_BattleEnterTransition(); // 진입 중 F4에서도 페이드 잔상을 남기지 않는다.

    if (closeVictoryUi && _battleVictory)
        _battleVictory->Close();
}

Shared<GameplayUI_Battle> GameplayUI_Battle::Create(uint32 levelIndex)
{
    auto instance = make_shared<GameplayUI_Battle>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : GameplayUI_Battle");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void GameplayUI_Battle::Free()
{
    Unbind_BattleDelegates(); // BattleController가 살아있는 동안 dangling handle이 남지 않게 먼저 정리한다.

    auto Release_UI = [](auto& ui)
        {
            if (ui)
            {
                ui->Free();
                ui.reset();
            }
        };

    Release_UI(_battleTransition);
    Release_UI(_battleCommandHUD);
    Release_UI(_battleSystemHUD);
    Release_UI(_monsterHpBar);
    Release_UI(_playerHpBar);
    Release_UI(_crossHair);
    Release_UI(_battleDefense);
    Release_UI(_cancelAim);
    Release_UI(_escapeSkipTurn);
    Release_UI(_attackTooltip);
    Release_UI(_choiceTarget);
    Release_UI(_reactionResult);
    Release_UI(_battleMechanism);
    Release_UI(_battleItemPanel);
    Release_UI(_battleSkillPanel);
    Release_UI(_battleAnnounce);
    Release_UI(_suddenAttack);

    for (auto& damageFont : _damageFonts)
        Release_UI(damageFont);

    Base::Free();
}

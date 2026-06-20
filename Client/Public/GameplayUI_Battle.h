#pragma once

#include "Base.h"
#include "Battle_Enum.h"
#include "Delegate.h"
#include "GameplayUI_Types.h"
#include "UI_BattleSystem_TurnOrder.h"
#include "BattleController.h"
#include "UI_BattleVictory.h"

NS_BEGIN(Client)

class BattleWorldUI_CommandCards;
class BattleWorldUI_GradientCards;
class BattleWorldUI_ItemCards;
class BattleWorldUI_SkillCards;
class UI_BattleDefense;
class UI_CancelAim;
class BattleController;
class UI_BattleTransition;
class UI_BattleCommandHUD;
class UI_BattleSystemHUD;
class UI_MonsterHpBar;
class UI_BossHpBar;
class UI_PlayerHpBar;
class UI_CrossHair;
class UI_ChoiceTarget;
class UI_AttackTooltip;
class UI_BattleSystem_TurnOrder;
class UI_DamageFont;
class UI_DestroyShield;
class UI_HealFont;
class UI_EscapeSkipTurnBack;
class UI_ReactionResult;
class UI_BattleVictory;
class UI_BattleMechanism;
class UI_BattleItemPanel;
class UI_BattleSkillPanel;
class UI_BattleAnnounce;
class UI_SuddenAttack;
class UI_AddAP;
class UI_QTE;
class UI_MarkEffect;
class UI_BattleTutorial;

class Monster_BattleBase;

class GameplayUI_Battle final : public Base
{
public:
    GameplayUI_Battle() = default;
    ~GameplayUI_Battle() override;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);
    void    Update_Input();
    vector<RuntimeUIEntry> Get_RuntimeUIEntries() const;

    // 평소에는 Battle UI 업데이트 막기
    bool    Needs_RuntimeUpdate() const;

    void    Play_BattleEnterTransition();
    void    Stop_BattleEnterTransition();
    bool    Is_BattleEnterTransitionPlaying() const;

    void    Handle_EditorStopRuntime();

private:
    void    Bind_BattleDelegates();        // 턴 관련 델리게이트 구독
    void    Unbind_BattleDelegates();      // 턴 관련 델리게이트 구독 해제

    void    Handle_BattleStarted(uint32 fieldId, bool isPlayerAdvantage);
    void    Handle_BattleEnded(EBattleEndResult result);
    void    Handle_TurnStarted(const BattleChar& actor, BattleTurnState state);
    void    Handle_TurnEnded(const BattleChar& actor);
    void    Handle_TargetPreviewChanged(const BattleChar* target);
    void    Handle_ActionStarted(const BattleAction& action);
    void    Handle_DamageApplied(const DamageAppliedInfo& info);
    void    Handle_HealApplied(const BattleChar& target, float healAmount);
    void    Handle_ReactionSucceeded(BattleReactionType reactionType, const vector<BattleChar>& target);
    void    Handle_BattleApChanged(const BattleChar& actor, uint32 amount, EAPChangeReason reason);
    void    Handle_QTEStarted(const BattleQTERequest& request);
    void    Handle_QTEResolved(BattleQTEResult result);

    // Buff
    void    Handle_BuffApplied(const BattleChar& target, Buffs buffId);
    void    Handle_BuffConsumed(const BattleChar& target, Buffs buffId, float bonusDamage);

    // 카운터 성공 알림
    void    Handle_CounterAnnounceRequested(const vector<BattleChar>& counterActors);
    // 약점 터트린거 알림
    void    Handle_BattleWeakPointConsumed(const Shared<Monster_BattleBase>& monster);
    void    Handle_BattleTutorialRequested(const BattleTutorialRequest& request);
    void    Handle_BattleTutorialClosed();

    void    Handle_TurnStartStatusAnnounceRequested(const BattleChar& target, Buffs buffId);
    void    Handle_BattleAnnounceFinished();

    void    Update_BattleCommandHUD(float timeDelta);
    void    Update_BattleSystemHUD(float timeDelta);
    void    Update_MonsterHpBar(float timeDelta);
    bool    Ensure_BossHpBar();
    void    Update_BossHpBar(float timeDelta);
    void    Update_PlayerHpBar(float timeDelta);
    void    Update_CrossHair(float timeDelta);
    void    Update_BattleDefense(float timeDelta);
    void    Update_CancelAim(float timeDelta);
    void    Update_ChoiceTarget(float timeDelta);
    void    Update_TurnOrder(float timeDelta);
    void    Update_DamageFonts(float timeDelta);
    void    Update_DestroyShields(float timeDelta);
    void    Update_HealFonts(float timeDelta);
    void    Update_AddAPs(float timeDelta);

    void    Update_EscapeSkipTurn(float timeDelta);
    void    Update_AttackTooltip(float timeDelta);
    void    Update_ReactionResult(float timeDelta);
    void    Update_BattleVictory(float timeDelta);
    void    Update_BattleMechanism(float timeDelta);
    void    Update_BattleItemPanel(float timeDelta);
    void    Update_BattleSkillPanel(float timeDelta);
    void    Update_BattleAnnounce(float timeDelta);
    void    Update_SuddenAttack(float timeDelta);
    void    Update_QTE(float timeDelta);
    void    Update_MarkEffect(float timeDelta);
    void    Update_BattleSystemIntroState(float timeDelta);
    HRESULT Ensure_BattleWorldCommandCards();
    void    Hide_BattleWorldCommandCards();
    void    Update_BattleWorldCommandCards(
        const Vec3& commandHudWorldPos,
        BattleTurnState currentState,
        uint32 curAp,
        uint32 maxAp,
        HeroId actorHeroId);
    HRESULT Ensure_BattleWorldItemCards();
    void    Hide_BattleWorldItemCards();
    void    Update_BattleWorldItemCards(
        const BattleChar& actor,
        BattleItemType selectedItemType,
        BattleTurnState currentState);

    HRESULT Ensure_BattleWorldSkillCards();
    void    Hide_BattleWorldSkillCards();
    void    Update_BattleWorldSkillCards(
        BattleTurnState currentState,
        const BattleChar& actor,
        const string& selectedSkillKey,
        uint32 skillPage);
    HRESULT Ensure_BattleWorldGradientCards();
    void    Hide_BattleWorldGradientCards();
    void    Update_BattleWorldGradientCards(
        BattleTurnState currentState,
        const BattleChar& actor);
    void    Update_BattleTutorial(float timeDelta);

    bool    Should_ShowChoiceTargetForSkillTargeting(const ActionRequest& request) const;

private:
#pragma region UI 위치맞추기
    // 일단 기능이 비슷하긴한데 나중에 어떻게될지 몰라서 일단 나눠서 사용
    bool    Try_GetBattleCommandHudWorldPos(const BattleChar& actor, Vec3& outWorldPos) const;
    bool    Try_GetTargetHpBarWorldPos(const BattleChar& target, Vec3& outWorldPos) const;
    bool    Try_GetReactionResultWorldPos(const BattleChar& target, Vec3& outWorldPos);
#pragma endregion

    // Turn Order
    using TurnDesc = UI_BattleSystem_TurnOrder::TurnOrderEntryDesc;
    vector<TurnDesc> Build_TurnOrderEntries() const;
    // 초상화 세팅
    static string    Resolve_TurnOrderPortraitGuid(const BattleChar& actor);

    // 전투 승리 시 결과 적용
    using VictoryStatDesc = UI_BattleVictory::VictoryPlayerStatDesc;
    vector<VictoryStatDesc> Build_BattleVictoryPlayerStats() const;

    void    Reset_BattleUiRuntimeState(bool closeVictoryUi);

private:
    uint32                                  _levelIndex = 0;

    Unique<UI_BattleTransition>             _battleTransition; // 전투 진입 테스트용 페이드 UI
    Unique<UI_BattleCommandHUD>             _battleCommandHUD;
    Unique<UI_BattleSystemHUD>              _battleSystemHUD;
    Shared<BattleWorldUI_CommandCards>      _battleWorldCommandCards;
    Shared<BattleWorldUI_ItemCards>         _battleWorldItemCards;
    Shared<BattleWorldUI_SkillCards>        _battleWorldSkillCards;
    Shared<BattleWorldUI_GradientCards>     _battleWorldGradientCards;

    Unique<UI_MonsterHpBar>                 _monsterHpBar;
    Unique<UI_BossHpBar>                    _bossHpBar;
    Unique<UI_PlayerHpBar>                  _playerHpBar;

    Unique<UI_CrossHair>                    _crossHair;
    Unique<UI_BattleDefense>                _battleDefense;
    Unique<UI_CancelAim>                    _cancelAim;
    Unique<UI_ChoiceTarget>                 _choiceTarget;
    Unique<UI_EscapeSkipTurnBack>           _escapeSkipTurn;
    Unique<UI_AttackTooltip>                _attackTooltip;
    Unique<UI_ReactionResult>               _reactionResult;
    Unique<UI_BattleVictory>                _battleVictory;
    Unique<UI_BattleSystem_TurnOrder>       _turnOrder;
    Unique<UI_BattleMechanism>              _battleMechanism;
    Unique<UI_BattleItemPanel>              _battleItemPanel;
    Unique<UI_BattleSkillPanel>             _battleSkillPanel;
    Unique<UI_BattleAnnounce>               _battleAnnounce;
    Unique<UI_SuddenAttack>                 _suddenAttack;
    Unique<UI_QTE>                          _qte;
    Unique<UI_MarkEffect>                   _markEffect;
    Unique<UI_BattleTutorial>               _battleTutorial;

private:
    static constexpr uint32 kDamageFontPoolSize = 8; // 폰트 최대 8개까지만
    array<Unique<UI_DamageFont>, kDamageFontPoolSize> _damageFonts{};

    static constexpr uint32 kDestroyShieldPoolSize = 4;
    array<Unique<UI_DestroyShield>, kDestroyShieldPoolSize> _destroyShields{};

    static constexpr uint32 kHealFontPoolSize = 8;
    array<Unique<UI_HealFont>, kHealFontPoolSize> _healFonts{};

    static constexpr uint32 kAddAPPoolSize = 4;
    array<Unique<UI_AddAP>, kAddAPPoolSize> _addAPs{};

    // Delegate 구독 대상
    Shared<BattleController> _battleController;
    FDelegateHandle          _battleStartedHandle{};        // 배틀 시작
    FDelegateHandle          _battleEndedHandle{};          // 배틀 종료
    FDelegateHandle          _turnStartedHandle{};          // 턴 시작
    FDelegateHandle          _turnEndedHandle{};            // 턴 종료
    FDelegateHandle          _targetPreviewChangedHandle{}; // 타겟 바뀔 때
    FDelegateHandle          _actionStartedHandle{};
    FDelegateHandle          _damageAppliedHandle{};
    FDelegateHandle          _healAppliedHandle{};
    FDelegateHandle          _battleApChangedHandle{};
    FDelegateHandle          _reactionSucceededHandle{};

    // QTE
    FDelegateHandle          _qteStartedHandle{};
    FDelegateHandle          _qteResolvedHandle{};

    // Buff
    FDelegateHandle          _buffAppliedHandle{};
    FDelegateHandle          _buffConsumedHandle{};

    FDelegateHandle          _counterAnnounceRequestedHandle{};
    FDelegateHandle          _battleWeakPointConsumedHandle{};

    // 튜토리얼
    FDelegateHandle          _battleTutorialRequestedHandle{};
    FDelegateHandle          _battleTutorialClosedHandle{};

    FDelegateHandle          _turnStartStatusAnnounceRequestedHandle{};
    FDelegateHandle          _battleAnnounceFinishedHandle{};

private:
    BattleChar               _targetPreviewSnapshot{};
    bool                     _hasTargetPreviewSnapshot = false;
    bool                     _isBattleUiActive = false;
    bool                     _isPlayerTurnUiActive = false;
    bool                     _isVictoryUiActive = false;
    bool                     _isVictoryConfirmPending = false;

#pragma region 메커니즘
    bool                     _isMechanismActionActive = false;
    HeroId                   _activePlayerId = HeroId::END;
#pragma endregion

    vector<VictoryStatDesc>  _battleStartVictoryStats{};

    bool                     _isPlayerAdvantageStart = false; // 플레이어 선제공격?
    bool                     _hasPlayedSuddenAttack = false;  // 이미 보여줬는지?  
    bool                     _isBattleSystemIntroPlaying = false;
    float                    _battleSystemIntroElapsed = 0.f;
    bool                     _hasPendingBattleSystemIntroActivation = false;
    HeroId                   _pendingBattleSystemIntroActivationHeroId = HeroId::END;

private:
    BattleChar               _currentPlayerTurnActor{}; // 현재 턴 진행중인 플레이어
    bool                     _hasCurrentPlayerTurnActor = false;

public:
    static Shared<GameplayUI_Battle> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

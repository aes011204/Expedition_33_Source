#pragma once

#include "Base.h"
#include "Battle_Enum.h"
#include "Delegate.h"
#include "UI_BattleSystem_TurnOrder.h"
#include "BattleController.h"
#include "UI_BattleVictory.h"

NS_BEGIN(Client)

class UI_BattleDefense;
class UI_CancelAim;
class BattleController;
class UI_BattleTransition;
class UI_BattleCommandHUD;
class UI_BattleSystemHUD;
class UI_MonsterHpBar;
class UI_PlayerHpBar;
class UI_CrossHair;
class UI_ChoiceTarget;
class UI_AttackTooltip;
class UI_BattleSystem_TurnOrder;
class UI_DamageFont;
class UI_HealFont;
class UI_EscapeSkipTurnBack;
class UI_ReactionResult;
class UI_BattleVictory;
class UI_BattleMechanism;
class UI_BattleItemPanel;
class UI_BattleSkillPanel;

class GameplayUI_Battle final : public Base
{
public:
    GameplayUI_Battle() = default;
    ~GameplayUI_Battle() override;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);
    void    Update_Input();

    void    Play_BattleEnterTransition();
    void    Stop_BattleEnterTransition();
    bool    Is_BattleEnterTransitionPlaying() const;

private:
    void    Bind_BattleDelegates();        // 턴 관련 델리게이트 구독
    void    Unbind_BattleDelegates();      // 턴 관련 델리게이트 구독 해제

    void    Handle_BattleStarted(uint32 fieldId, bool isFirstHero);
    void    Handle_BattleEnded(EBattleEndResult result);
    void    Handle_TurnStarted(const BattleChar& actor, BattleTurnState state);
    void    Handle_TurnEnded(const BattleChar& actor);
    void    Handle_TargetPreviewChanged(const BattleChar* target);
    void    Handle_ActionStarted(const Action& action);
    void    Handle_DamageApplied(const BattleChar& target, float damage, AttackType attackType);
    void    Handle_HealApplied(const BattleChar& target, float healAmount);
    void    Handle_ReactionSucceeded(BattleController::ReactionType reactionType, const BattleChar& target);

    void    Update_BattleCommandHUD(float timeDelta);
    void    Update_BattleSystemHUD(float timeDelta);
    void    Update_MonsterHpBar(float timeDelta);
    void    Update_PlayerHpBar(float timeDelta);
    void    Update_CrossHair(float timeDelta);
    void    Update_BattleDefense(float timeDelta);
    void    Update_CancelAim(float timeDelta);
    void    Update_ChoiceTarget(float timeDelta);
    void    Update_TurnOrder(float timeDelta);
    void    Update_DamageFonts(float timeDelta);
    void    Update_HealFonts(float timeDelta);
    void    Update_EscapeSkipTurn(float timeDelta);
    void    Update_AttackTooltip(float timeDelta);
    void    Update_ReactionResult(float timeDelta);
    void    Update_BattleVictory(float timeDelta);
    void    Update_BattleMechanism(float timeDelta);
    void    Update_BattleItemPanel(float timeDelta);
    void    Update_BattleSkillPanel(float timeDelta);

private:
#pragma region UI 위치맞추기
    // 일단 기능이 비슷하긴한데 나중에 어떻게될지 몰라서 일단 나눠서 사용
    bool    Try_GetBattleCommandHudWorldPos(const BattleChar& actor, Vec3& outWorldPos) const;
    bool    Try_GetTargetHpBarWorldPos(const BattleChar& target, Vec3& outWorldPos) const;
    bool    Try_GetDamageFontWorldPos(const BattleChar& target, Vec3& outWorldPos) const;
    bool    Try_GetReactionResultWorldPos(const BattleChar& target, Vec3& outWorldPos);
    bool    Try_GetHealFontWorldPos(const BattleChar& target, Vec3& outWorldPos) const;
#pragma endregion

    // Turn Order
    using TurnDesc = UI_BattleSystem_TurnOrder::TurnOrderEntryDesc;
    vector<TurnDesc> Build_TurnOrderEntries() const;
    // 초상화 세팅
    static string    Resolve_TurnOrderPortraitGuid(const BattleChar& actor);

    // 전투 승리 시 결과 적용
    using VictoryStatDesc = UI_BattleVictory::VictoryPlayerStatDesc;
    vector<VictoryStatDesc> Build_BattleVictoryPlayerStats() const;

private:
    Unique<UI_BattleTransition>             _battleTransition; // 전투 진입 테스트용 페이드 UI
    Unique<UI_BattleCommandHUD>             _battleCommandHUD;
    Unique<UI_BattleSystemHUD>              _battleSystemHUD;

    Unique<UI_MonsterHpBar>                 _monsterHpBar;
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

private:
    static constexpr uint32 kDamageFontPoolSize = 8; // 폰트 최대 8개까지만
    array<Unique<UI_DamageFont>, kDamageFontPoolSize> _damageFonts{};

    static constexpr uint32 kHealFontPoolSize = 8;
    array<Unique<UI_HealFont>, kHealFontPoolSize> _healFonts{};

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
    FDelegateHandle          _reactionSucceededHandle{};

private:
    BattleChar               _targetPreviewSnapshot{};
    bool                     _hasTargetPreviewSnapshot = false;
    bool                     _isBattleUiActive = false;
    bool                     _isPlayerTurnUiActive = false;
    bool                     _isVictoryUiActive = false;

#pragma region 메커니즘
    bool                     _isMechanismActionActive = false;
    HeroId                   _activePlayerId = HeroId::END;
#pragma endregion

    vector<VictoryStatDesc>  _battleStartVictoryStats{};

private:
    BattleChar               _currentPlayerTurnActor{}; // 현재 턴 진행중인 플레이어
    bool                     _hasCurrentPlayerTurnActor = false;

public:
    static Shared<GameplayUI_Battle> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

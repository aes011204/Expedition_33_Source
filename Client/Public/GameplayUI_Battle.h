#pragma once

#include "Base.h"
#include "Battle_Enum.h"
#include "Delegate.h"

NS_BEGIN(Client)

class UI_BattleDefense;
class UI_CancelAim;
class BattleController;
class UI_BattleTransition;
class UI_BattleCommandHUD;
class UI_BattleSystemHUD;
class UI_MonsterHpBar;
class UI_CrossHair;
class UI_ChoiceTarget;

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

    void    Update_BattleCommandHUD(float timeDelta);
    void    Update_BattleSystemHUD(float timeDelta);
    void    Update_MonsterHpBar(float timeDelta);
    void    Update_CrossHair(float timeDelta);
    void    Update_BattleDefense(float timeDelta);
    void    Update_CancelAim(float timeDelta);
    void    Update_ChoiceTarget(float timeDelta);

private:
    // 타겟 체력바 위치
    bool    Try_GetTargetHpBarWorldPos(const BattleChar& target, Vec3& outWorldPos) const; // 계산한 월드 위치를 호출부 worldPos에 실제로 반영하려면 참조로 받아야 한다.

    // 임시) 턴 진행중인 플레이어한테 Battle HUD 띄우기
    bool    Try_GetBattleCommandHudWorldPos(const BattleChar& actor, Vec3& outWorldPos) const;

private:
    Unique<UI_BattleTransition> _battleTransition; // 전투 진입 테스트용 페이드 UI
    Unique<UI_BattleCommandHUD> _battleCommandHUD;
    Unique<UI_BattleSystemHUD>  _battleSystemHUD;
    Unique<UI_MonsterHpBar>     _monsterHpBar;
    Unique<UI_CrossHair>        _crossHair;
    Unique<UI_BattleDefense>    _battleDefense;
    Unique<UI_CancelAim>        _cancelAim;
    Unique<UI_ChoiceTarget>     _choiceTarget;

    // Delegate 구독 대상
    Shared<BattleController> _battleController;
    FDelegateHandle          _battleStartedHandle{};        // 배틀 시작
    FDelegateHandle          _battleEndedHandle{};          // 배틀 종료
    FDelegateHandle          _turnStartedHandle{};          // 턴 시작
    FDelegateHandle          _turnEndedHandle{};            // 턴 종료
    FDelegateHandle          _targetPreviewChangedHandle{}; // 타겟 바뀔 때

    FDelegateHandle          _actionStartedHandle{};

    BattleChar               _targetPreviewSnapshot{};
    bool                     _hasTargetPreviewSnapshot = false;
    bool                     _isBattleUiActive = false;
    bool                     _isPlayerTurnUiActive = false;

private:
    BattleChar               _currentPlayerTurnActor{}; // 현재 턴 진행중인 플레이어
    bool                     _hasCurrentPlayerTurnActor = false;

public:
    static Shared<GameplayUI_Battle> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

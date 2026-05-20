#pragma once

#include "Base.h"
#include "Battle_Enum.h"
#include "Delegate.h"

NS_BEGIN(Client)

class BattleController;
class UI_BattleTransition;
class UI_BattleCommandHUD;
class UI_MonsterHpBar;

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

    void    Update_BattleCommandHUD();
    void    Update_MonsterHpBar();
    bool    Try_GetTargetHpBarWorldPos(const BattleChar& target, Vec3& outWorldPos) const; // 계산한 월드 위치를 호출부 worldPos에 실제로 반영하려면 참조로 받아야 한다.

private:
    // 전투 진입 테스트용 페이드 UI
    Unique<UI_BattleTransition> _battleTransition;
    Unique<UI_BattleCommandHUD> _battleCommandHUD;
    Unique<UI_MonsterHpBar>     _monsterHpBar;

    // Delegate 구독 대상
    Shared<BattleController> _battleController;
    FDelegateHandle          _battleStartedHandle{};
    FDelegateHandle          _battleEndedHandle{};
    FDelegateHandle          _turnStartedHandle{};
    FDelegateHandle          _turnEndedHandle{};
    FDelegateHandle          _targetPreviewChangedHandle{};

    BattleChar _targetPreviewSnapshot{};
    bool       _hasTargetPreviewSnapshot = false;
    bool       _isBattleUiActive = false;
    bool       _isPlayerTurnUiActive = false;

public:
    static Shared<GameplayUI_Battle> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

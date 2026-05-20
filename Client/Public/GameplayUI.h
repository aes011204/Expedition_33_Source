#pragma once

#include "Base.h"
#include "SavePoint_Panel.h"
#include "Delegate.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Text;
class UI_Button;
NS_END

NS_BEGIN(Client)

class SavePoint_Panel;
class SkillTree_Panel;
class CharacterStatus_Panel;
class UI_BattleTransition;
class UI_BattleCommandHUD;
class BattleController;
class UI_MonsterHpBar;

// 실제로 렌더링되는 UI 가 아니라, 플레이 중 UI 들을 로드/토글/바인딩 등 처리할 컨트롤러라고 생각
class GameplayUI : public Base
{
public:
    enum class GameplayMenu
    {
        None,
        SavePoint,
        CharacterStatus,
        SkillTree,
        WeaponUpgrade,
    };

public:
    GameplayUI() = default;
    ~GameplayUI() override = default;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);

    void    Play_BattleEnterTransition();
    void    Stop_BattleEnterTransition();
    bool    Is_BattleEnterTransitionPlaying() const;

    bool    Is_AnyMenuOpen() const;
    GameplayMenu Get_CurrentMenu() const; // 현재 최상위 메뉴 확인용

private:
    void    Update_WorldUI(float timeDelta);
    void    Update_BattleUI(float timeDelta);

private:    /* World UI */
    void    Handle_SavePointAction(SavePoint_Panel::SavePointAction action);

    void    Handle_TabInput();
    void    Handle_BackInput();  // Esc로 현재 메뉴 닫고 이전 메뉴 복귀

    void    Push_Menu(GameplayMenu menu);
    void    Pop_Menu();                   
    void    Close_AllMenus();              // 전체 메뉴 종료

    void    Open_Menu(GameplayMenu menu);  // 메뉴 enum -> 실제 panel open
    void    Close_Menu(GameplayMenu menu); // 메뉴 enum -> 실제 panel close

private:    /* Battle UI */
    void    Bind_BattleDelegates();        // 턴 관련 델리게이트 구독
    void    Unbind_BattleDelegates();      // 턴 관련 델리게이트 구독 해제

    void    Handle_BattleStarted(uint32 fieldId, bool isFirstHero);              // 추가
    void    Handle_BattleEnded(EBattleEndResult result);                         // 추가
    void    Handle_TurnStarted(const BattleChar& actor, BattleTurnState state);  // 추가
    void    Handle_TurnEnded(const BattleChar& actor);                           // 추가
    void    Handle_TargetPreviewChanged(const BattleChar* target); 

private:
    void    Update_BattleCommandHUD();
    void    Update_MonsterHpBar();

    bool    Try_GetTargetHpBarWorldPos(const BattleChar& target, Vec3& outWorldPos) const; // 변경: 계산한 월드 위치를 호출부 worldPos에 실제로 반영하려면 참조로 받아야 한다.

private:    /* World UI */
    Unique<SavePoint_Panel>         _savePointPanel;
    Unique<CharacterStatus_Panel>   _characterStatusPanel;
    Unique<SkillTree_Panel>         _skillTreePanel;
    vector<GameplayMenu>            _menuStack{};

private:    /* Battle UI */
    // 전투 진입 테스트용 페이드 UI
    Shared<UI_BattleTransition>     _battleTransition;
    Unique<UI_BattleCommandHUD>     _battleCommandHUD;
    Unique<UI_MonsterHpBar>         _monsterHpBar;

    // Delegate 구독 대상
    Shared<BattleController>        _battleController;
    FDelegateHandle                 _battleStartedHandle{};   
    FDelegateHandle                 _battleEndedHandle{};     
    FDelegateHandle                 _turnStartedHandle{};     
    FDelegateHandle                 _turnEndedHandle{};       
    FDelegateHandle                 _targetPreviewChangedHandle{};

    BattleChar                      _targetPreviewSnapshot{};
    bool                            _hasTargetPreviewSnapshot = false;
    bool                            _isBattleUiActive = false;
    bool                            _isPlayerTurnUiActive = false;


public:
    static Shared<GameplayUI> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

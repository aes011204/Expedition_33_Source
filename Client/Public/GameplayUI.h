#pragma once

#include "Base.h"
#include "SavePoint_Panel.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Text;
class UI_Button;
NS_END

NS_BEGIN(Client)

class SavePoint_Panel;
class SkillTree_Panel;
class CharacterStatus_Panel;
class ChangeWeapon_Panel;
class UI_Fade;
class UI_BattleTransition;

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
    void    Handle_SavePointAction(SavePoint_Panel::SavePointAction action);

    void    Handle_TabInput();
    void    Handle_BackInput();  // Esc로 현재 메뉴 닫고 이전 메뉴 복귀

    void    Push_Menu(GameplayMenu menu);
    void    Pop_Menu();                   
    void    Close_AllMenus();              // 전체 메뉴 종료

    void    Open_Menu(GameplayMenu menu);  // 메뉴 enum -> 실제 panel open
    void    Close_Menu(GameplayMenu menu); // 메뉴 enum -> 실제 panel close

private:
    Unique<SavePoint_Panel>         _savePointPanel{};
    Unique<CharacterStatus_Panel>   _characterStatusPanel{};
    Unique<SkillTree_Panel>         _skillTreePanel{};

    Unique<CharacterStatus_Panel> _characterStatusPanel{};
    Unique<SkillTree_Panel> _skillTreePanel{};
    Unique<ChangeWeapon_Panel> _changeWeaponPanel{};

    // 전투 진입 테스트용 페이드 UI
    Shared<UI_BattleTransition>     _battleTransition{};

    vector<GameplayMenu>            _menuStack{};

public:
    static Unique<GameplayUI> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

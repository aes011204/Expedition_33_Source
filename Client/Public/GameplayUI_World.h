#pragma once

#include "Base.h"
#include "CharacterProgression_Panel.h"
#include "GamePlayMenuFade.h"
#include "GameplayUI_Types.h"
#include "SavePoint_Panel.h"
#include "UI_ArenaCharacterSelect.h"

NS_BEGIN(Client)

class SkillTree_Panel;
class CharacterStatus_Panel;
class ExpeditionStat_Panel;
class UI_CrossHair;
class UI_GetCharacter;
class UI_GetClothes;
class UI_GetWeapon;
class UI_GetPictos;

class ChangeSkill_Panel;
class ChangeWeapon_Panel;
class UpgradeWeapon_Panel;
class Wardrobe_Panel;
class Pictos_Panel;

class UI_GradientSkillPreviewTest;

class GameplayUI_World final : public Base
{
public:
    GameplayUI_World() = default;
    ~GameplayUI_World() override;

public:
    HRESULT Initialize(uint32 levelIndex);
    void    Update(float timeDelta);
    void    Update_Input();
    vector<RuntimeUIEntry> Get_RuntimeUIEntries() const;

    bool    Is_AnyMenuOpen() const;
    GameplayMenu Get_CurrentMenu() const; // 현재 최상위 메뉴 확인용
    void    Force_CloseAllMenus();
    void    Handle_EditorStopRuntime();

    // 아레나 UI 열기
    bool    Request_ArenaCharacterSelect();

private:
    void    Handle_SavePointAction(SavePoint_Panel::SavePointAction action);
    void    Handle_SavePointInput();
    void    Handle_TabInput();
    void    Handle_ExpeditionStat();
    void    Handle_BackInput();            // Esc로 현재 메뉴 닫고 이전 메뉴 복귀
    void    Handle_CharacterProgressionAction(CharacterProgression_Panel::Action action);
    void    Handle_ArenaCharacterSelectInput();
    void    Handle_ArenaCharacterSelectAction(UI_ArenaCharacterSelect::Action action);
    void    Handle_GradientSkillPreviewTestInput();

    void    Update_CrossHair(float timeDelta);
    void    Try_ShowPendingGetCharacter();
    void    Try_ShowPendingGetClothes();
    bool    Is_GetCharacterOpen() const;
    bool    Is_GetClothesOpen() const;
    bool    Is_GetWeaponOpen() const;
    bool    Is_GetPictosOpen() const;
    bool    Is_GradientSkillPreviewTestOpen() const;

private:
    void    Push_Menu(GameplayMenu menu);
    void    Pop_Menu();
    void    Close_AllMenus(bool immediate = false); // 전체 메뉴 종료

    void    Open_Menu(GameplayMenu menu);  // 메뉴 enum -> 실제 panel open
    void    Close_Menu(GameplayMenu menu, bool restoreWorldControl);

    void    Open_CharacterProgressionFlow();
    void    Open_CharacterProgressionFlow(CharacterProgression_Panel::ContentTab initialTab);
    void    Close_CharacterProgressionFlow();

    bool    Is_WorldFreeAimActive() const;
    bool    Is_CharacterProgressionFlowMenu(GameplayMenu menu) const;

private:
    Unique<SavePoint_Panel>       _savePointPanel;
    Unique<CharacterStatus_Panel> _characterStatusPanel;
    Unique<SkillTree_Panel>       _skillTreePanel;
    Unique<ChangeWeapon_Panel>    _changeWeaponPanel;
    Unique<UpgradeWeapon_Panel>   _upgradeWeaponPanel;
    Unique<ExpeditionStat_Panel>  _expeditionStatPanel;
    Unique<UI_CrossHair>          _crossHair;
    Unique<UI_GetCharacter>       _getCharacterPanel;
    Unique<UI_GetClothes>         _getClothesPanel;
    Unique<UI_GetWeapon>          _getWeaponPanel;
    Unique<UI_GetPictos>          _getPictosPanel;
    Unique<UI_ArenaCharacterSelect> _arenaCharacterSelectPanel;
    Unique<UI_GradientSkillPreviewTest> _gradientSkillPreviewTestPanel;

    Unique<CharacterProgression_Panel> _characterProgressionPanel;
    Unique<ChangeSkill_Panel>          _changeSkillPanel;
    Unique<Wardrobe_Panel>             _wardrobePanel;
    Unique<Pictos_Panel>               _pictosPanel;

    vector<GameplayMenu>          _menuStack{};
    GamePlayMenuFade              _menuFade{};

public:
    static Unique<GameplayUI_World> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

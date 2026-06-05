#include "GameplayUI_World.h"

#include "CharacterStatus_Panel.h"
#include "GameInstance.h"
#include "SkillTree_Panel.h"
#include "ChangeWeapon_Panel.h"
#include "ExpeditionStat_Panel.h"
#include "UI_CrossHair.h"
#include "ClientInstance.h"
#include "Player_FreeAim.h"
#include "PlayerStateMachineCom.h"
#include "Player_WorldBase.h"

GameplayUI_World::~GameplayUI_World() = default;

namespace
{
    template <typename TPanel>
    void Append_RuntimePanelEntry(
        vector<RuntimeUIEntry>& outEntries,
        const char* category,
        const char* name,
        const TPanel* panel,
        bool positionDrivenByCode = false)
    {
        if (panel == nullptr)
            return;

        Shared<UI_Base> root = panel->Get_RootUI();
        if (root == nullptr)
            return;

        outEntries.push_back({ category, name, root, nullptr, positionDrivenByCode });
    }
}

HRESULT GameplayUI_World::Initialize(uint32 levelIndex)
{
    _characterStatusPanel = CharacterStatus_Panel::Create(levelIndex);
    CHECK_NULL(_characterStatusPanel, E_FAIL);

    _skillTreePanel = SkillTree_Panel::Create(levelIndex);
    CHECK_NULL(_skillTreePanel, E_FAIL);

    _savePointPanel = SavePoint_Panel::Create(levelIndex);
    CHECK_NULL(_savePointPanel, E_FAIL);

    _changeWeaponPanel = ChangeWeapon_Panel::Create(levelIndex);
    CHECK_NULL(_changeWeaponPanel, E_FAIL);

    _expeditionStatPanel = ExpeditionStat_Panel::Create(levelIndex);
    CHECK_NULL(_expeditionStatPanel, E_FAIL);

    _crossHair = UI_CrossHair::Create(levelIndex);
    CHECK_NULL(_crossHair, E_FAIL);

    return S_OK;
}

void GameplayUI_World::Update(float timeDelta)
{
    if (_savePointPanel)
        _savePointPanel->Update(timeDelta);

    if (_characterStatusPanel)
        _characterStatusPanel->Update(timeDelta);

    if (_skillTreePanel)
        _skillTreePanel->Update(timeDelta);

    if (_changeWeaponPanel)
        _changeWeaponPanel->Update(timeDelta);

    if (_expeditionStatPanel)
        _expeditionStatPanel->Update(timeDelta);

    Update_CrossHair(timeDelta);

    if (!CLIENT->Is_FadeTransitionPlaying() && _savePointPanel && _savePointPanel->Is_Open())
    {
        const SavePoint_Panel::SavePointAction action = _savePointPanel->Consume_LastAction();
        Handle_SavePointAction(action);
    }
}

void GameplayUI_World::Update_Input()
{
    if (CLIENT->Is_FadeTransitionPlaying())
        return;

    Handle_TabInput(); // Tab은 root 메뉴인 SavePoint 진입/종료만 담당
    Handle_BackInput();
    Handle_ExpeditionStat();
}

vector<RuntimeUIEntry> GameplayUI_World::Get_RuntimeUIEntries() const
{
    vector<RuntimeUIEntry> entries{};
    entries.reserve(6);

    Append_RuntimePanelEntry(entries, "World", "SavePointPanel", _savePointPanel.get());
    Append_RuntimePanelEntry(entries, "World", "CharacterStatusPanel", _characterStatusPanel.get());
    Append_RuntimePanelEntry(entries, "World", "SkillTreePanel", _skillTreePanel.get());
    Append_RuntimePanelEntry(entries, "World", "ChangeWeaponPanel", _changeWeaponPanel.get());
    Append_RuntimePanelEntry(entries, "World", "ExpeditionStatPanel", _expeditionStatPanel.get());
    Append_RuntimePanelEntry(entries, "World", "CrossHair", _crossHair.get());

    return entries;
}

bool GameplayUI_World::Is_AnyMenuOpen() const
{
    return !_menuStack.empty();
}

GameplayMenu GameplayUI_World::Get_CurrentMenu() const
{
    if (_menuStack.empty())
        return GameplayMenu::None;

    return _menuStack.back();
}

void GameplayUI_World::Force_CloseAllMenus()
{
    Close_AllMenus(); // 배틀 진입 시 월드 메뉴가 남아 있지 않게 강제 정리
}

void GameplayUI_World::Handle_EditorStopRuntime()
{
    Force_CloseAllMenus();
}

void GameplayUI_World::Handle_SavePointAction(SavePoint_Panel::SavePointAction action)
{
    switch (action)
    {
    case SavePoint_Panel::SavePointAction::Rest:
        Close_AllMenus(); // 현재는 SavePoint 닫기만. 나중에 Rest 처리 연결
        break;

    case SavePoint_Panel::SavePointAction::UpgradeStat:
        Push_Menu(GameplayMenu::CharacterStatus);   // SavePoint 위에 CharacterStatus push
        break;

    case SavePoint_Panel::SavePointAction::UpgradeSkill:
        Push_Menu(GameplayMenu::SkillTree);         // SavePoint 위에 SkillTree push
        break;

    case SavePoint_Panel::SavePointAction::UpgradeWeapon:
        Push_Menu(GameplayMenu::WeaponUpgrade);
        break;

    case SavePoint_Panel::SavePointAction::None:
    default:
        break;
    }
}

void GameplayUI_World::Handle_TabInput()
{
    if (!GAME->KeyDown(KEY_TYPE::TAB))
        return;

    const GameplayMenu currentMenu = Get_CurrentMenu();

    if (currentMenu == GameplayMenu::None)
    {
        Push_Menu(GameplayMenu::SavePoint);
        return;
    }

    if (currentMenu == GameplayMenu::SavePoint)
    {
        // SavePoint에서 Tab은 전체 종료 -> 나중에는 깃발에서 E누르면 켜지는거로 변경 예정
        Close_AllMenus();
        return;
    }
}

void GameplayUI_World::Handle_ExpeditionStat()
{
    if (!GAME->KeyDown(KEY_TYPE::Q))
        return;

    if (_expeditionStatPanel == nullptr)
        return;

    if (!_menuStack.empty())
        return; // 다른 월드 메뉴가 열려 있으면 막기

    _expeditionStatPanel->Toggle();
}

void GameplayUI_World::Handle_BackInput()
{
    if (!GAME->KeyDown(KEY_TYPE::ESCAPE))
        return;

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
    {
        _expeditionStatPanel->Close();
        return;
    }

    if (_menuStack.empty())
        return;

    Pop_Menu();
}

void GameplayUI_World::Update_CrossHair(float timeDelta)
{
    if (_crossHair == nullptr)
        return;

    if (!Is_WorldFreeAimActive())
    {
        _crossHair->Close();
        return;
    }

    const Shared<Player_WorldBase> activeHero = CLIENT->Get_ActiveHero();
    if (activeHero == nullptr)
    {
        _crossHair->Close();
        return;
    }

    const Shared<PlayerStateMachineCom> stateCom = activeHero->Get_Component<PlayerStateMachineCom>();
    if (stateCom == nullptr)
    {
        _crossHair->Close();
        return;
    }

    const Shared<Player_FreeAim> freeAimState =
        dynamic_pointer_cast<Player_FreeAim>(stateCom->Get_State(PlayerState::FreeAim));
    if (freeAimState == nullptr)
    {
        _crossHair->Close();
        return;
    }

    _crossHair->Open();
    _crossHair->Set_HitState(freeAimState->Is_HitMonster());
    _crossHair->Update(timeDelta);
}

void GameplayUI_World::Push_Menu(GameplayMenu menu)
{
    if (menu == GameplayMenu::None)
        return;

    const GameplayMenu currentMenu = Get_CurrentMenu();

    if (currentMenu == menu)
        return; // 같은 메뉴 중복 방지

    const auto applyMenuChange = [this, currentMenu, menu]()
    {
        if (currentMenu != GameplayMenu::None)
            Close_Menu(currentMenu);

        _menuStack.push_back(menu);
        Open_Menu(menu);
    };

    _menuFade.Request_MenuTransition(currentMenu, menu, applyMenuChange);
}

void GameplayUI_World::Pop_Menu()
{
    if (_menuStack.empty())
        return;

    const GameplayMenu currentMenu = _menuStack.back();
    const GameplayMenu previousMenu = _menuStack.size() >= 2 ? _menuStack[_menuStack.size() - 2] : GameplayMenu::None;

    const auto applyMenuChange = [this, currentMenu]()
    {
        Close_Menu(currentMenu); // 현재 top 메뉴 종료
        _menuStack.pop_back();

        if (_menuStack.empty())
            return;

        Open_Menu(_menuStack.back());
    };

    _menuFade.Request_MenuTransition(currentMenu, previousMenu, applyMenuChange);
}

void GameplayUI_World::Close_AllMenus()
{
    while (!_menuStack.empty())
    {
        Close_Menu(_menuStack.back());
        _menuStack.pop_back();
    }

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
        _expeditionStatPanel->Close_Immediately(); 
}

void GameplayUI_World::Open_Menu(GameplayMenu menu)
{
    switch (menu)
    {
    case GameplayMenu::SavePoint:
        if (_savePointPanel)
            _savePointPanel->Open();
        break;

    case GameplayMenu::CharacterStatus:
        if (_characterStatusPanel)
            _characterStatusPanel->Open();
        break;

    case GameplayMenu::SkillTree:
        if (_skillTreePanel)
            _skillTreePanel->Open();
        break;

    case GameplayMenu::WeaponUpgrade:
        if (_changeWeaponPanel)
            _changeWeaponPanel->Open();
        break;

    case GameplayMenu::None:
    default:
        break;
    }
}

void GameplayUI_World::Close_Menu(GameplayMenu menu)
{
    switch (menu)
    {
    case GameplayMenu::SavePoint:
        if (_savePointPanel)
            _savePointPanel->Close();
        break;

    case GameplayMenu::CharacterStatus:
        if (_characterStatusPanel)
            _characterStatusPanel->Close();
        break;

    case GameplayMenu::SkillTree:
        if (_skillTreePanel)
            _skillTreePanel->Close();
        break;

    case GameplayMenu::WeaponUpgrade:
        if (_changeWeaponPanel)
            _changeWeaponPanel->Close();
        break;

    case GameplayMenu::None:
    default:
        break;
    }
}

bool GameplayUI_World::Is_WorldFreeAimActive() const
{
    // 배틀 중 아닐 때.
    if (CLIENT->Is_BattleActive() || CLIENT->Is_BattleEntering())
        return false;

    // 월드 입력이 막혀 있으면 조준 UI도 같이 숨긴다.
    if (GAME->Is_WorldGameplayInputBlocked())
        return false;

    // 월드 메뉴가 열려 있으면 조준 UI를 덮지 않는다.
    if (!_menuStack.empty())
        return false;

    if (_expeditionStatPanel && _expeditionStatPanel->Is_Open())
        return false;

    const Shared<Player_WorldBase> activeHero = CLIENT->Get_ActiveHero();
    if (activeHero == nullptr)
        return false;

    const Shared<PlayerStateMachineCom> stateCom = activeHero->Get_Component<PlayerStateMachineCom>();
    if (stateCom == nullptr)
        return false;

    return stateCom->Get_CurrentPlayerState() == PlayerState::FreeAim;
}

Unique<GameplayUI_World> GameplayUI_World::Create(uint32 levelIndex)
{
    auto instance = make_unique<GameplayUI_World>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : GameplayUI_World");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void GameplayUI_World::Free()
{
    auto Release_Panel = [](auto& panel)
        {
            if (panel)
            {
                panel->Free();
                panel.reset();
            }
        };

    Release_Panel(_crossHair);
    Release_Panel(_savePointPanel);
    Release_Panel(_characterStatusPanel);
    Release_Panel(_skillTreePanel);
    Release_Panel(_changeWeaponPanel);
    Release_Panel(_expeditionStatPanel);

    Base::Free();
}

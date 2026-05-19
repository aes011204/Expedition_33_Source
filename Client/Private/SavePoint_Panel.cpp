#include "SavePoint_Panel.h"

#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

HRESULT SavePoint_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void SavePoint_Panel::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!_isOpen)
        return;

    Handle_ButtonInput();
}

void SavePoint_Panel::Open()
{
    _isOpen = true;
    _lastAction = SavePointAction::None;

    if (_root)
        _root->Open();

    // 마우스 잠금
    GAME->Unlock_Mouse();
    GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly);
}

void SavePoint_Panel::Close()
{
     _isOpen = false;

    if (_root)
        _root->Close();

    GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
    GAME->Lock_Mouse();
}

void SavePoint_Panel::Toggle()
{
    if (_isOpen)
        Close();
    else
        Open();
}

SavePoint_Panel::SavePointAction SavePoint_Panel::Consume_LastAction()
{
    const SavePointAction action = _lastAction;
    _lastAction = SavePointAction::None;

    return action;
}

HRESULT SavePoint_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_SavePoint_Path; 
    desc.prototypeTagPrefix = L"SavePoint";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_SavePoint");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT SavePoint_Panel::Bind_UI()
{
    _restButton =           UI_Utils::Find_TypedUI<UI_Button>(_uiByName, L"Button_SavePoint_Rest");
    _upgradeStatButton =    UI_Utils::Find_TypedUI<UI_Button>(_uiByName, L"Button_SavePoint_Upgrade_Stat");
    _upgradeSkillButton =   UI_Utils::Find_TypedUI<UI_Button>(_uiByName, L"Button_SavePoint_Upgrade_Skill");
    _upgradeWeaponButton =  UI_Utils::Find_TypedUI<UI_Button>(_uiByName, L"Button_SavePoint_Upgrade_Weapon");

    CHECK_NULL(_restButton, E_FAIL);
    CHECK_NULL(_upgradeStatButton, E_FAIL);
    CHECK_NULL(_upgradeSkillButton, E_FAIL);
    CHECK_NULL(_upgradeWeaponButton, E_FAIL);

    return S_OK;
}

void SavePoint_Panel::Handle_ButtonInput()
{
    if (_restButton && _restButton->Was_Clicked())
    {
        _lastAction = SavePointAction::Rest;
        LOG_DEBUG("SavePoint_Panel: Rest clicked");
        return;
    }

    if (_upgradeStatButton && _upgradeStatButton->Was_Clicked())
    {
        _lastAction = SavePointAction::UpgradeStat;
        LOG_DEBUG("SavePoint_Panel: UpgradeStat clicked");
        return;
    }

    if (_upgradeSkillButton && _upgradeSkillButton->Was_Clicked())
    {
        _lastAction = SavePointAction::UpgradeSkill;
        LOG_DEBUG("SavePoint_Panel: UpgradeSkill clicked");
        return;
    }

    if (_upgradeWeaponButton && _upgradeWeaponButton->Was_Clicked())
    {
        _lastAction = SavePointAction::UpgradeWeapon;
        LOG_DEBUG("SavePoint_Panel: UpgradeWeapon clicked");
        return;
    }
}

Unique<SavePoint_Panel> SavePoint_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<SavePoint_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : SavePoint_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void SavePoint_Panel::Free()
{
    _restButton.reset();
    _upgradeStatButton.reset();
    _upgradeSkillButton.reset();
    _upgradeWeaponButton.reset();
    _root.reset();
    _uiByName.clear();

    Base::Free();
}

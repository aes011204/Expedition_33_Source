#include "GameplayUI.h"
#include "GameInstance.h"
#include "CharacterStatus_Panel.h"
#include "SkillTree_Panel.h"
#include "ChangeWeapon_Panel.h"

HRESULT GameplayUI::Initialize(uint32 levelIndex)
{
    _characterStatusPanel = CharacterStatus_Panel::Create(levelIndex);
    CHECK_NULL(_characterStatusPanel, E_FAIL);

    _skillTreePanel = SkillTree_Panel::Create(levelIndex);
    CHECK_NULL(_skillTreePanel, E_FAIL);

    _changeWeaponPanel = ChangeWeapon_Panel::Create(levelIndex);
    CHECK_NULL(_changeWeaponPanel, E_FAIL);

    return S_OK;
}

void GameplayUI::Update(float timeDelta)
{
    if (GAME->KeyDown(KEY_TYPE::TAB) && _characterStatusPanel)
        _characterStatusPanel->Toggle();

     if (_characterStatusPanel)
        _characterStatusPanel->Update(timeDelta);

    if(GAME->KeyDown(KEY_TYPE::F3) && _skillTreePanel)
        _skillTreePanel->Toggle();

    if(_skillTreePanel)
        _skillTreePanel->Update(timeDelta);

    if (GAME->KeyDown(KEY_TYPE::F4) && _changeWeaponPanel)
        _changeWeaponPanel->Toggle();

    if (_changeWeaponPanel)
        _changeWeaponPanel->Update(timeDelta);
}

Unique<GameplayUI> GameplayUI::Create(uint32 levelIndex)
{
    auto instance = make_unique<GameplayUI>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : GameplayUI");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void GameplayUI::Free()
{
    auto Release_Panel = [](auto& panel)
    {
        if (panel)
        {
            panel->Free();
            panel.reset();
        }
    };

    Release_Panel(_characterStatusPanel);
    Release_Panel(_skillTreePanel);
    Release_Panel(_changeWeaponPanel);

    Base::Free();
}

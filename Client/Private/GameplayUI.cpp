#include "GameplayUI.h"
#include "GameInstance.h"
#include "CharacterStatus_Panel.h"

HRESULT GameplayUI::Initialize(uint32 levelIndex)
{
    _characterStatusPanel = CharacterStatus_Panel::Create(levelIndex);
    CHECK_NULL(_characterStatusPanel, E_FAIL);

    return S_OK;
}

void GameplayUI::Update(float timeDelta)
{
    if (GAME->KeyDown(KEY_TYPE::TAB) && _characterStatusPanel)
        _characterStatusPanel->Toggle();

     if (_characterStatusPanel)
        _characterStatusPanel->Update(timeDelta);
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

    Base::Free();
}

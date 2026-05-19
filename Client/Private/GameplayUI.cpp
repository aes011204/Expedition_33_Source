#include "GameplayUI.h"
#include "GameInstance.h"
#include "CharacterStatus_Panel.h"
#include "SkillTree_Panel.h"
#include "UI_BattleTransition.h"

HRESULT GameplayUI::Initialize(uint32 levelIndex)
{
    _characterStatusPanel = CharacterStatus_Panel::Create(levelIndex);
    CHECK_NULL(_characterStatusPanel, E_FAIL);

    _skillTreePanel = SkillTree_Panel::Create(levelIndex);
    CHECK_NULL(_skillTreePanel, E_FAIL);

    _battleTransition = UI_BattleTransition::Create(levelIndex);
    CHECK_NULL(_battleTransition, E_FAIL);

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

     if (_battleTransition)
        _battleTransition->Update(timeDelta);
}

void GameplayUI::Play_BattleEnterTransition()
{
    if (_battleTransition)
        _battleTransition->Play_BattleEnter();
}

void GameplayUI::Stop_BattleEnterTransition()
{
    if (_battleTransition)
        _battleTransition->Stop();
}

bool GameplayUI::Is_BattleEnterTransitionPlaying() const
{
    return _battleTransition && _battleTransition->Is_Playing();
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
    Release_Panel(_battleTransition);

    Base::Free();
}

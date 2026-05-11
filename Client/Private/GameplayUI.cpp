#include "pch.h"
#include "GameplayUI.h"

#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"

HRESULT GameplayUI::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_CharacterStatus(levelIndex), E_FAIL);

    // Keep CharacterStatus hidden at gameplay start.
    Set_CharacterStatusOpen(false);

    return S_OK;
}

void GameplayUI::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (GAME->KeyDown(KEY_TYPE::TAB))
        Toggle_CharacterStatus();
}

HRESULT GameplayUI::Load_CharacterStatus(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_CharacterStatus_Path;
    desc.prototypeTagPrefix = L"CharacterStatus";
    desc.outUIByName = &_characterStatusUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _characterStatusRoot = Find_UI(_characterStatusUIByName, L"Panel_CharacterStatus");
    CHECK_NULL(_characterStatusRoot, E_FAIL);

    return S_OK;
}

void GameplayUI::Toggle_CharacterStatus()
{
    Set_CharacterStatusOpen(!_isCharacterStatusOpen);
}

void GameplayUI::Set_CharacterStatusOpen(bool isOpen)
{
    _isCharacterStatusOpen = isOpen;

    if (_characterStatusRoot != nullptr)
    {
        if (_isCharacterStatusOpen)
            _characterStatusRoot->Open();
        else
            _characterStatusRoot->Close();
    }

    if (_isCharacterStatusOpen)
    {
        // Open menu mode: show cursor and pause gameplay simulation.
        GAME->Unlock_Mouse();
        GAME->Set_GameplayPaused(true);
    }
    else
    {
        GAME->Set_GameplayPaused(false);
        GAME->Lock_Mouse();
    }
}

Shared<UI_Base> GameplayUI::Find_UI(const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name)
{
    const auto iter = uiByName.find(name);
    if (iter == uiByName.end() || iter->second == nullptr)
        return nullptr;

    return iter->second;
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
    if (_characterStatusRoot)
        _characterStatusRoot->Close();

    _characterStatusRoot.reset();
    _characterStatusUIByName.clear();
    _isCharacterStatusOpen = false;

    GAME->Set_GameplayPaused(false);

    Base::Free();
}

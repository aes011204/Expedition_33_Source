#include "UI_BattleVictory.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_BattleVictory::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);

    Close();
    return S_OK;
}

void UI_BattleVictory::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_BattleVictory::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_BattleVictory::Close()
{
    UI_RuntimePanelBase::Close();
}

HRESULT UI_BattleVictory::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Battle_Victory_Path;
    desc.prototypeTagPrefix = L"BattleVictory";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleVictory");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

Unique<UI_BattleVictory> UI_BattleVictory::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleVictory>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleVictory");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleVictory::Free()
{
    if (_root)
        _root->Close();

    _root.reset();
    _uiByName.clear();

    Base::Free();
}

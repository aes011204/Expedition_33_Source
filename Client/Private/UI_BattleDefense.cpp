#include "UI_BattleDefense.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_BattleDefense::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);

    Close();
    return S_OK;
}

void UI_BattleDefense::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_BattleDefense::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_BattleDefense::Close()
{
    UI_RuntimePanelBase::Close();
}

HRESULT UI_BattleDefense::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Battle_Defense_Path;
    desc.prototypeTagPrefix = L"BattleDefense";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem_Defense");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

Unique<UI_BattleDefense> UI_BattleDefense::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleDefense>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleDefense");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleDefense::Free()
{
    if (_root)
        _root->Close();

    _root.reset();
    _uiByName.clear();

    Base::Free();
}

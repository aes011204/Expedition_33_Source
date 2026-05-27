#include "UI_EscapeSkipTurnBack.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_EscapeSkipTurnBack::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    return S_OK;
}

void UI_EscapeSkipTurnBack::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_EscapeSkipTurnBack::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_EscapeSkipTurnBack::Close()
{
    UI_RuntimePanelBase::Close();
}

HRESULT UI_EscapeSkipTurnBack::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Escape_SkipTurnBack_Path;
    desc.prototypeTagPrefix = L"EscapeSkipTurn";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem_Escape_SkipTurn_Back");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_EscapeSkipTurnBack::Bind_UI()
{
    _keyC = Find_UI(_uiByName, L"Panel_Key_C");
    _keyTab = Find_UI(_uiByName, L"Panel_Key_Tab");

    CHECK_NULL(_keyC, E_FAIL);
    CHECK_NULL(_keyTab, E_FAIL);

    return S_OK;
}

Unique<UI_EscapeSkipTurnBack> UI_EscapeSkipTurnBack::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_EscapeSkipTurnBack>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_EscapeSkipTurnBack");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_EscapeSkipTurnBack::Free()
{
    if (_root)
        _root->Close();

    _root.reset();
    _uiByName.clear();

    Base::Free();
}

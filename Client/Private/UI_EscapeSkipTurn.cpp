#include "UI_EscapeSkipTurn.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_EscapeSkipTurn::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    return S_OK;
}

void UI_EscapeSkipTurn::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_EscapeSkipTurn::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_EscapeSkipTurn::Close()
{
    UI_RuntimePanelBase::Close();
}

HRESULT UI_EscapeSkipTurn::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Escape_SkipTurn_Path;
    desc.prototypeTagPrefix = L"EscapeSkipTurn";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem_Escape_SkipTurn");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_EscapeSkipTurn::Bind_UI()
{
    _keyC = Find_UI(_uiByName, L"Panel_Key_C");
    _keyTab = Find_UI(_uiByName, L"Panel_Key_Tab");

    CHECK_NULL(_keyC, E_FAIL);
    CHECK_NULL(_keyTab, E_FAIL);

    return S_OK;
}

Unique<UI_EscapeSkipTurn> UI_EscapeSkipTurn::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_EscapeSkipTurn>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_EscapeSkipTurn");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_EscapeSkipTurn::Free()
{
    if (_root)
        _root->Close();

    _root.reset();
    _uiByName.clear();

    Base::Free();
}

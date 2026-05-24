#include "UI_AttackTooltip.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_AttackTooltip::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);

    Close();
    return S_OK;
}

void UI_AttackTooltip::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_AttackTooltip::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_AttackTooltip::Close()
{
    UI_RuntimePanelBase::Close();
}

HRESULT UI_AttackTooltip::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Attack_Tooltip_Path;
    desc.prototypeTagPrefix = L"AttackTooltip";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem_AttackTooltip");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

Unique<UI_AttackTooltip> UI_AttackTooltip::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_AttackTooltip>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_AttackTooltip");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_AttackTooltip::Free()
{
    if (_root)
        _root->Close();

    _root.reset();
    _uiByName.clear();

    Base::Free();
}

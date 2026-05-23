#include "ExpeditionStat_Panel.h"

#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

HRESULT ExpeditionStat_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);

    Close();
    return S_OK;
}

void ExpeditionStat_Panel::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!_isOpen)
        return;
}

void ExpeditionStat_Panel::Open()
{
    UI_RuntimePanelBase::Open();

    GAME->Unlock_Mouse(); 
    GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly);
}

void ExpeditionStat_Panel::Close()
{
    UI_RuntimePanelBase::Close();

    GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
    GAME->Lock_Mouse();
}

void ExpeditionStat_Panel::Toggle()
{
    UI_RuntimePanelBase::Toggle();
}

HRESULT ExpeditionStat_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_ExpeditionStatus_Path;
    desc.prototypeTagPrefix = L"ExpeditionStatus";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_Check_Expedition");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

Unique<ExpeditionStat_Panel> ExpeditionStat_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<ExpeditionStat_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : ExpeditionStat_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void ExpeditionStat_Panel::Free()
{
    if (_root)
        _root->Close();

    _root.reset();
    _uiByName.clear();

    Base::Free();
}

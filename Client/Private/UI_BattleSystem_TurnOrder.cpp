#include "UI_BattleSystem_TurnOrder.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_BattleSystem_TurnOrder::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    return S_OK;
}

void UI_BattleSystem_TurnOrder::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_BattleSystem_TurnOrder::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_BattleSystem_TurnOrder::Close()
{
    UI_RuntimePanelBase::Close();
}

void UI_BattleSystem_TurnOrder::Set_TurnOrderPreview(const vector<TurnOrderEntryDesc>& entries)
{
    for (uint32 i = 0; i < _slots.size(); ++i)
    {
        if (_slots[i].root == nullptr || _slots[i].portrait == nullptr)
            continue;

        if (i >= entries.size())
        {
            _slots[i].root->Close();
            continue;
        }

        _slots[i].root->Open();

        const TurnOrderEntryDesc& entry = entries[i];
        if (!entry.portraitGuid.empty())
            _slots[i].portrait->Set_TextureGuid(entry.portraitGuid);

        _slots[i].portrait->Set_Color(entry.isEnemy
            ? Vec4(1.f, 1.f, 1.f, 0.75f)
            : Vec4(1.f, 1.f, 1.f, 1.f));

    }

    if (_currentTurnArrow)
    {
        if (entries.empty())
            _currentTurnArrow->Close();
        else
            _currentTurnArrow->Open();
    }

}

void UI_BattleSystem_TurnOrder::Clear_TurnOrderPreview()
{
    for (TurnOrderSlot& slot : _slots)
    {
        if (slot.root)
            slot.root->Close();
    }

    if (_currentTurnArrow)
        _currentTurnArrow->Close();
}

HRESULT UI_BattleSystem_TurnOrder::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_BattleSystem_TurnOrder_Path;
    desc.prototypeTagPrefix = L"BattleSystemTurnOrder";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem_TurnOrder");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_BattleSystem_TurnOrder::Bind_UI()
{
    _currentTurnArrow =
        Find_TypedUI<UI_Image>(_uiByName, L"Image_BattleSystem_TurnOrder_Current_Turn");
    CHECK_NULL(_currentTurnArrow, E_FAIL);

    for (uint32 i = 0; i < _slots.size(); ++i)
    {
        const wstring suffix = to_wstring(i + 1);

        _slots[i].root =
            Find_UI(_uiByName, L"Panel_BattleSystem_TurnOrder_Portrait_Frame_" + suffix);

        _slots[i].portrait =
            Find_TypedUI<UI_Image>(
                _uiByName, L"Image_BattleSystem_TurnOrder_Portrait_Frame_" + suffix + L"_TestPortrait");

        CHECK_NULL(_slots[i].root, E_FAIL);
        CHECK_NULL(_slots[i].portrait, E_FAIL);
    }

    return S_OK;
}

Unique<UI_BattleSystem_TurnOrder> UI_BattleSystem_TurnOrder::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleSystem_TurnOrder>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleSystem_TurnOrder");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleSystem_TurnOrder::Free()
{
    if (_root)
        _root->Close();

    _root.reset();
    _uiByName.clear();

    _currentTurnArrow.reset();

    for (TurnOrderSlot& slot : _slots)
    {
        slot.root.reset();
        slot.portrait.reset();
    }

    Base::Free();
}

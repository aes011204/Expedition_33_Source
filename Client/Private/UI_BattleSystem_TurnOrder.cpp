#include "UI_BattleSystem_TurnOrder.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Transform.h"
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
    if (entries.empty())
    {
        Clear_TurnOrderPreview();
        if (_root)
            _root->Close();
        return;
    }

    if (_root)
        _root->Open();

    for (uint32 i = 0; i < _slots.size(); ++i)
    {
        TurnOrderSlot& slot = _slots[i];

        if (i >= entries.size())
        {
            Close_UI(slot.currentPlayerOverlay);
            Close_UI(slot.currentMonsterOverlay);
            Close_UI(slot.playerRoot);
            Close_UI(slot.monsterRoot);
            continue;
        }

        const TurnOrderEntryDesc& entry = entries[i];
        const bool isCurrentTurn = (i == 0); // 0번째. Current 턴인지

        if (isCurrentTurn)
        {
            if (entry.isEnemy)
            {
                Close_UI(slot.currentPlayerOverlay);
                Open_UI(slot.currentMonsterOverlay);

                if (slot.currentMonsterPortrait && !entry.portraitGuid.empty())
                    slot.currentMonsterPortrait->Set_TextureGuid(entry.portraitGuid);
            }
            else
            {
                Open_UI(slot.currentPlayerOverlay);
                Close_UI(slot.currentMonsterOverlay);

                if (slot.currentPlayerPortrait && !entry.portraitGuid.empty())
                    slot.currentPlayerPortrait->Set_TextureGuid(entry.portraitGuid);
            }

        }
        else
        {
            Close_UI(slot.currentPlayerOverlay);
            Close_UI(slot.currentMonsterOverlay);
        }

        if (entry.isEnemy)
        {
            Close_UI(slot.playerRoot);
            Open_UI(slot.monsterRoot);

            if (slot.monsterPortrait && !entry.portraitGuid.empty())
                slot.monsterPortrait->Set_TextureGuid(entry.portraitGuid);
        }
        else
        {
            Open_UI(slot.playerRoot);
            Close_UI(slot.monsterRoot);

            if (slot.playerPortrait && !entry.portraitGuid.empty())
                slot.playerPortrait->Set_TextureGuid(entry.portraitGuid);
        }
    }
}

void UI_BattleSystem_TurnOrder::Clear_TurnOrderPreview()
{
    for (TurnOrderSlot& slot : _slots)
    {
        Close_UI(slot.currentPlayerOverlay);
        Close_UI(slot.currentMonsterOverlay);
        Close_UI(slot.playerRoot);
        Close_UI(slot.monsterRoot);
    }
}

HRESULT UI_BattleSystem_TurnOrder::Load_UI(uint32 levelIndex)
{
    UNREFERENCED_PARAMETER(levelIndex);

    _uiByName.clear();

    umap<wstring, Shared<UI_Base>> frameUIByName{};
    CHECK_FAILED(Load_UIFragment(
        Constants::UI::UI_BattleSystem_TurnOrder_Frame_Path,
        L"BattleSystemTurnOrderFrame",
        frameUIByName), E_FAIL);

    Merge_UIMap(frameUIByName);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem_TurnOrder_Frame");
    CHECK_NULL(_root, E_FAIL);

    for (uint32 i = 0; i < _slots.size(); ++i)
    {
        umap<wstring, Shared<UI_Base>> playerUIByName{};
        CHECK_FAILED(Load_UIFragment(
            Constants::UI::UI_BattleSystem_TurnOrder_Player_Path,
            L"BattleSystemTurnOrderPlayer_" + to_wstring(i), playerUIByName), E_FAIL);

        _slots[i].playerRoot =
            Find_UI(playerUIByName, L"Panel_TurnOrder_Portrait_Player");

        _slots[i].playerPortrait =
            Find_TypedUI<UI_Image>(playerUIByName, L"Image_BattleSystem_TurnOrder_Portrait_Player");

        CHECK_NULL(_slots[i].playerRoot, E_FAIL);

        Merge_UIMap(playerUIByName);

        umap<wstring, Shared<UI_Base>> monsterUIByName{};
        CHECK_FAILED(Load_UIFragment(
            Constants::UI::UI_BattleSystem_TurnOrder_Monster_Path,
            L"BattleSystemTurnOrderMonster_" + to_wstring(i), monsterUIByName), E_FAIL);

        _slots[i].monsterRoot =
            Find_UI(monsterUIByName, L"Panel_TurnOrder_Portrait_Monster");
        _slots[i].monsterPortrait =
            Find_TypedUI<UI_Image>(monsterUIByName, L"Image_BattleSystem_TurnOrder_Portrait_Monster");

        CHECK_NULL(_slots[i].monsterRoot, E_FAIL);

        Merge_UIMap(monsterUIByName);
    }

    umap<wstring, Shared<UI_Base>> currentPlayerUIByName{};
    CHECK_FAILED(Load_UIFragment(
        Constants::UI::UI_BattleSystem_TurnOrder_CurPlayer_Path,
        L"BattleSystemTurnOrderCurrentPlayer", currentPlayerUIByName), E_FAIL);

    _slots[0].currentPlayerOverlay =
        Find_UI(currentPlayerUIByName, L"Panel_BattleSystem_TurnOrder_CurrentTurn_Player_Frame");
    _slots[0].currentPlayerPortrait =
        Find_TypedUI<UI_Image>(currentPlayerUIByName, L"Image_BattleSystem_TurnOrder_CurrentTurn_Player_Portrait");

    CHECK_NULL(_slots[0].currentPlayerOverlay, E_FAIL);
    CHECK_NULL(_slots[0].currentPlayerPortrait, E_FAIL);
    Merge_UIMap(currentPlayerUIByName);

    umap<wstring, Shared<UI_Base>> currentMonsterUIByName{};
    CHECK_FAILED(Load_UIFragment(
        Constants::UI::UI_BattleSystem_TurnOrder_CurMonster_Path,
        L"BattleSystemTurnOrderCurrentMonster", currentMonsterUIByName), E_FAIL);

    _slots[0].currentMonsterOverlay =
        Find_UI(currentMonsterUIByName, L"Panel_BattleSystem_TurnOrder_CurrentTurn_Monster_Frame");
    _slots[0].currentMonsterPortrait =
        Find_TypedUI<UI_Image>(currentMonsterUIByName, L"Image_BattleSystem_TurnOrder_CurrentTurn_Monster_Portrait");

    CHECK_NULL(_slots[0].currentMonsterOverlay, E_FAIL);
    CHECK_NULL(_slots[0].currentMonsterPortrait, E_FAIL);
    Merge_UIMap(currentMonsterUIByName);

    return S_OK;
}

HRESULT UI_BattleSystem_TurnOrder::Bind_UI()
{
    _currentTurnAnchor =
        Find_UI(_uiByName, L"Panel_BattleSystem_TurnOrder_Portrait");
    CHECK_NULL(_currentTurnAnchor, E_FAIL);

    for (uint32 i = 0; i < _slots.size(); ++i)
    {
        const wstring suffix = to_wstring(i + 1);
        TurnOrderSlot& slot = _slots[i];

        slot.anchor =
            Find_UI(_uiByName, L"Panel_BattleSystem_TurnOrder_Portrait_Frame_" + suffix);
        CHECK_NULL(slot.anchor, E_FAIL);

        CHECK_NULL(slot.playerRoot, E_FAIL);
        CHECK_NULL(slot.monsterRoot, E_FAIL);

        Place_OnSlot(slot.playerRoot, slot.anchor, false);
        Place_OnSlot(slot.monsterRoot, slot.anchor, false);

        if (i == 0)
        {
            slot.currentPlayerOverlay->Set_Parent(_currentTurnAnchor);
            slot.currentMonsterOverlay->Set_Parent(_currentTurnAnchor);
        }

        Close_UI(slot.currentPlayerOverlay);
        Close_UI(slot.currentMonsterOverlay);
        Close_UI(slot.playerRoot);
        Close_UI(slot.monsterRoot);
    }

    return S_OK;
}

HRESULT UI_BattleSystem_TurnOrder::Load_UIFragment(
    const wchar_t* jsonPath,
    const wstring& prototypeTagPrefix,
    umap<wstring, Shared<UI_Base>>& outUIByName) const
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = _levelIndex;
    desc.jsonPath = jsonPath;
    desc.prototypeTagPrefix = prototypeTagPrefix;
    desc.outUIByName = &outUIByName;

    return UI_RuntimeLoader::Load_Document(desc);
}

void UI_BattleSystem_TurnOrder::Merge_UIMap(const umap<wstring, Shared<UI_Base>>& src)
{
    for (const auto& [name, ui] : src)
        _uiByName[name] = ui;
}

void UI_BattleSystem_TurnOrder::Place_OnSlot(
    const Shared<UI_Base>& ui,
    const Shared<UI_Base>& parent,
    bool keepOriginalSize) const
{
    if (ui == nullptr)
        return;

    ui->Set_Parent(parent);

    UI_Transform* transform = ui->Get_Transform();
    if (transform == nullptr)
        return;

    transform->Set_Anchor(Vec2(0.f, 0.f));
    transform->Set_Position(Vec2(100.f, 50.f));
    transform->Set_Pivot(Vec2(0.5f, 0.5f));
    transform->Set_Scale(Vec2(1.f, 1.f));
    transform->Set_RotationRadians(0.f);

    if (!keepOriginalSize)
        transform->Set_Size(Vec2(200.f, 100.f));

    ui->Refresh_OpenBaseTransform();
}

void UI_BattleSystem_TurnOrder::Open_UI(const Shared<UI_Base>& ui) const
{
    if (ui)
        ui->Open();
}

void UI_BattleSystem_TurnOrder::Close_UI(const Shared<UI_Base>& ui) const
{
    if (ui)
        ui->Close();
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
    _currentTurnAnchor.reset();

    for (TurnOrderSlot& slot : _slots)
    {
        slot.anchor.reset();
        slot.currentPlayerOverlay.reset();
        slot.currentMonsterOverlay.reset();
        slot.playerRoot.reset();
        slot.monsterRoot.reset();
        slot.playerPortrait.reset();
        slot.monsterPortrait.reset();
        slot.currentPlayerPortrait.reset();
        slot.currentMonsterPortrait.reset();
    }

    Base::Free();
}

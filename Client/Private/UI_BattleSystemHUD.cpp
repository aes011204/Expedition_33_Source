#include "UI_BattleSystemHUD.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_BattleSystemHUD::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    return S_OK;
}

void UI_BattleSystemHUD::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_BattleSystemHUD::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_BattleSystemHUD::Close()
{
    UI_RuntimePanelBase::Close();
}

void UI_BattleSystemHUD::Set_PartyBattleStat(PlayerCharacterType playerType, float curHp, float maxHp, uint32 curAp, uint32 maxAp)
{
    const uint32 slotIndex = Get_SlotIndex(playerType);
    if (slotIndex >= 3)
        return;

    PartyBattleStatSlot& slot = _partySlots[slotIndex];

    maxHp = max(maxHp, 1.f);
    curHp = clamp(curHp, 0.f, maxHp);

    maxAp = min(maxAp, 9u); 
    curAp = min(curAp, maxAp);

    Apply_HpGauge(slot, curHp / maxHp);
    Apply_ApGauge(slot, curAp, maxAp);

    Set_HpText(slot, curHp, maxHp);
    Set_ApText(slot, curAp);
}

HRESULT UI_BattleSystemHUD::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_BattleSystem_Path;
    desc.prototypeTagPrefix = L"BattleSystemHUD";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_BattleSystemHUD::Bind_UI()
{
    for (uint32 i = 0; i < 3; ++i)
    {
        const wstring portraitName = kPortraitNames[i];

        _partySlots[i].hpGauge = Find_TypedUI<UI_Image>(_uiByName, L"Image_BattlePortrait_" + portraitName + L"_HP_Gauge"); // fillRatio는 뒤 프레임이 아니라 실제 빨간 HP 게이지에만 적용한다.
        _partySlots[i].currentHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattlePortrait_" + portraitName + L"_Current_HP");
        _partySlots[i].currentHpTextBg = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattlePortrait_" + portraitName + L"_Current_HP_BG");
        _partySlots[i].maxHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattlePortrait_" + portraitName + L"_Max_HP");
        _partySlots[i].maxHpTextBg = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattlePortrait_" + portraitName + L"_Max_HP_BG");
        _partySlots[i].apText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattlePortrait_" + portraitName + L"_Icon_AP");
        _partySlots[i].apTextBg = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattlePortrait_" + portraitName + L"_Icon_AP_BG");

        CHECK_NULL(_partySlots[i].hpGauge, E_FAIL);
        CHECK_NULL(_partySlots[i].currentHpText, E_FAIL);
        CHECK_NULL(_partySlots[i].currentHpTextBg, E_FAIL);
        CHECK_NULL(_partySlots[i].maxHpText, E_FAIL);
        CHECK_NULL(_partySlots[i].maxHpTextBg, E_FAIL);
        CHECK_NULL(_partySlots[i].apText, E_FAIL);
        CHECK_NULL(_partySlots[i].apTextBg, E_FAIL);

        _partySlots[i].hpGauge->Set_UseFillRatio(true); 
        _partySlots[i].hpGauge->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);

        for (uint32 apIndex = 0; apIndex < 9; ++apIndex)
        {
            _partySlots[i].apStates[apIndex] =
                Find_TypedUI<UI_Image>(_uiByName, L"Image_BattlePortrait_" + portraitName + L"_AP_" + to_wstring(apIndex + 1));
            CHECK_NULL(_partySlots[i].apStates[apIndex], E_FAIL);
        }
    }

    return S_OK;
}

uint32 UI_BattleSystemHUD::Get_SlotIndex(PlayerCharacterType playerType)
{
    switch (playerType)
    {
    case PlayerCharacterType::Gustave:
        return 0;

    case PlayerCharacterType::Lune:
        return 1;

    case PlayerCharacterType::Maelle:
        return 2;

    case PlayerCharacterType::None:
    default:
        return 3;
    }
}

void UI_BattleSystemHUD::Set_TextPair(const Shared<UI_Text>& front, const Shared<UI_Text>& back, const wstring& text)
{
    if (front)
        front->Set_Text(text);

    if (back)
        back->Set_Text(text);
}

void UI_BattleSystemHUD::Apply_HpGauge(PartyBattleStatSlot& slot, float ratio)
{
    if (slot.hpGauge == nullptr)
        return;

    ratio = clamp(ratio, 0.f, 1.f);
    slot.hpGauge->Set_FillRatio(ratio);
}

void UI_BattleSystemHUD::Apply_ApGauge(PartyBattleStatSlot& slot, uint32 curAp, uint32 maxAp)
{
    const uint32 clampedMaxAp = min(maxAp, 9u);

    for (uint32 i = 0; i < 9; ++i)
    {
        if (slot.apStates[i] == nullptr)
            continue;

        if (curAp == 0 || clampedMaxAp == 0)
        {
            slot.apStates[i]->Close();
            continue;
        }

        if (i < curAp && i < clampedMaxAp)
            slot.apStates[i]->Open();
        else
            slot.apStates[i]->Close();
    }
}

void UI_BattleSystemHUD::Set_HpText(PartyBattleStatSlot& slot, float hp, float maxHp)
{
    const int32 hpValue = static_cast<int32>(hp);
    const int32 maxHpValue = static_cast<int32>(maxHp);

    Set_TextPair(slot.currentHpText, slot.currentHpTextBg, to_wstring(hpValue));
    Set_TextPair(slot.maxHpText, slot.maxHpTextBg, to_wstring(maxHpValue));
}

void UI_BattleSystemHUD::Set_ApText(PartyBattleStatSlot& slot, uint32 ap)
{
    Set_TextPair(slot.apText, slot.apTextBg, to_wstring(ap));
}

Unique<UI_BattleSystemHUD> UI_BattleSystemHUD::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleSystemHUD>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleSystemHUD");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleSystemHUD::Free()
{
    if (_root)
        _root->Close();

    for (uint32 i = 0; i < 3; ++i)
    {
        _partySlots[i].hpGauge.reset();
        _partySlots[i].currentHpText.reset();
        _partySlots[i].currentHpTextBg.reset();
        _partySlots[i].maxHpText.reset();
        _partySlots[i].maxHpTextBg.reset();
        _partySlots[i].apText.reset();
        _partySlots[i].apTextBg.reset();

        for (uint32 apIndex = 0; apIndex < 9; ++apIndex)
            _partySlots[i].apStates[apIndex].reset();
    }

    _root.reset();
    _uiByName.clear();

    Base::Free();
}

#include "UI_BattleSystemHUD.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

#include "BattleMechanismCom.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kBattleSystemIntroAnimationName[] = L"BattleSystem_Intro";

    constexpr wchar_t kBattlePortraitActivationGustaveAnimationName[] = L"BattlePortrait_Activation_Gustave";
    constexpr wchar_t kBattlePortraitActivationLuneAnimationName[] = L"BattlePortrait_Activation_Lune";
    constexpr wchar_t kBattlePortraitActivationMaelleAnimationName[] = L"BattlePortrait_Activation_Maelle";

    constexpr wchar_t kBattlePortraitDamagedGustaveAnimationName[] = L"BattlePortrait_Damaged_Gustave";
    constexpr wchar_t kBattlePortraitDamagedLuneAnimationName[] = L"BattlePortrait_Damaged_Lune";
    constexpr wchar_t kBattlePortraitDamagedMaelleAnimationName[] = L"BattlePortrait_Damaged_Maelle";

    constexpr wchar_t kBattlePortraitDeactivationGustaveAnimationName[] = L"BattlePortrait_Deactivation_Gustave";
    constexpr wchar_t kBattlePortraitDeactivationLuneAnimationName[] = L"BattlePortrait_Deactivation_Lune";
    constexpr wchar_t kBattlePortraitDeactivationMaelleAnimationName[] = L"BattlePortrait_Deactivation_Maelle";

    const wchar_t* Resolve_PortraitActivationAnimationName(HeroId heroId)
    {
        switch (heroId)
        {
        case HeroId::Gustave:
            return kBattlePortraitActivationGustaveAnimationName;

        case HeroId::Lune:
            return kBattlePortraitActivationLuneAnimationName;

        case HeroId::Maelle:
            return kBattlePortraitActivationMaelleAnimationName;

        case HeroId::END:
        default:
            return nullptr;
        }
    }

    const wchar_t* Resolve_PortraitDamagedAnimationName(HeroId heroId)
    {
        switch (heroId)
        {
        case HeroId::Gustave:
            return kBattlePortraitDamagedGustaveAnimationName;

        case HeroId::Lune:
            return kBattlePortraitDamagedLuneAnimationName;

        case HeroId::Maelle:
            return kBattlePortraitDamagedMaelleAnimationName;

        case HeroId::END:
        default:
            return nullptr;
        }
    }

    const wchar_t* Resolve_PortraitDeactivationAnimationName(HeroId heroId)
    {
        switch (heroId)
        {
        case HeroId::Gustave:
            return kBattlePortraitDeactivationGustaveAnimationName;

        case HeroId::Lune:
            return kBattlePortraitDeactivationLuneAnimationName;

        case HeroId::Maelle:
            return kBattlePortraitDeactivationMaelleAnimationName;

        case HeroId::END:
        default:
            return nullptr;
        }
    }
}

HRESULT UI_BattleSystemHUD::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Clear_GustaveMechanismCharge();   // HUD json 기본 visible/text 상태를 코드 기준값으로 덮어쓴다.
    Clear_LuneMechanismStains();      // 루네 portrait 얼룩은 시작 시 모두 닫힌 상태여야 한다.
    Clear_MaelleMechanismStance();    // 마엘도 기본 태세로 맞춘다.

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

	Stop_TargetAnimations();
    Close_AllLuneStains();            // 다시 열릴 때 데이터 기본 visible 값이 남지 않게 즉시 정리한다.
    Close_AllMaelleStanceBadges();
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

void UI_BattleSystemHUD::Set_GustaveMechanismCharge(const Shared<BattleMechanismCom>& mechanismCom)
{
    if (mechanismCom == nullptr || !mechanismCom->Is_Gustave())
    {
        Clear_GustaveMechanismCharge();
        return;
    }

    const int32 currentCharge = mechanismCom->Get_GustaveCharge();
    const int32 maxCharge = mechanismCom->Get_GustaveMaxCharge();

    Set_TextPair(_gustaveCurrentChargeText, _gustaveCurrentChargeTextBg, to_wstring(currentCharge));
    Set_TextPair(_gustaveMaxChargeText, _gustaveMaxChargeTextBg, to_wstring(maxCharge));
}

void UI_BattleSystemHUD::Set_LuneMechanismStains(const Shared<BattleMechanismCom>& mechanismCom)
{
    if (mechanismCom == nullptr || !mechanismCom->Is_Lune())
    {
        Clear_LuneMechanismStains();
        return;
    }

    Apply_LuneState(mechanismCom); // mechanism 패널과 같은 규칙으로 portrait HUD를 갱신한다.
}

void UI_BattleSystemHUD::Clear_LuneMechanismStains()
{
    Close_AllLuneStains();
}

void UI_BattleSystemHUD::Clear_GustaveMechanismCharge()
{
    Set_TextPair(_gustaveCurrentChargeText, _gustaveCurrentChargeTextBg, L"0");
    Set_TextPair(_gustaveMaxChargeText, _gustaveMaxChargeTextBg, L"10");
}

void UI_BattleSystemHUD::Set_MaelleMechanismStance(const Shared<BattleMechanismCom>& mechanismCom)
{
    if (mechanismCom == nullptr || !mechanismCom->Is_Maelle())
    {
        Clear_MaelleMechanismStance();
        return;
    }

    // 뱃지까지 적용
    Apply_MaelleStanceBadge(mechanismCom->Get_MaelleStance());
}

void UI_BattleSystemHUD::Clear_MaelleMechanismStance()
{
    // 태세없음으로 전환
    Apply_MaelleStanceBadge(BattleMechanismCom::MaelleStance::Stanceless);
}

void UI_BattleSystemHUD::Play_PortraitActivation(HeroId heroId)
{
	const wchar_t* animationName = Resolve_PortraitActivationAnimationName(heroId);
    if (animationName == nullptr)
        return;

    Play_TargetAnimation(animationName); 
}

void UI_BattleSystemHUD::Play_PortraitDamaged(HeroId heroId)
{
	const wchar_t* animationName = Resolve_PortraitDamagedAnimationName(heroId);
    if (animationName == nullptr)
        return;

    Play_TargetAnimation(animationName);
}

void UI_BattleSystemHUD::Play_PortraitDeactivation(HeroId heroId)
{
	const wchar_t* animationName = Resolve_PortraitDeactivationAnimationName(heroId);
    if (animationName == nullptr)
        return;

    Play_TargetAnimation(animationName);
}

void UI_BattleSystemHUD::Play_BattleSystemIntro()
{
    Play_TargetAnimation(kBattleSystemIntroAnimationName);
}

HRESULT UI_BattleSystemHUD::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_BattleSystem_Path;

    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattleSystemIntroAnim_Path);

    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattlePortraitActivationGustaveAnim_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattlePortraitActivationLuneAnim_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattlePortraitActivationMaelleAnim_Path);

    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattlePortraitDamagedGustaveAnim_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattlePortraitDamagedLuneAnim_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattlePortraitDamagedMaelleAnim_Path);

    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattlePortraitDeactivationGustaveAnim_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattlePortraitDeactivationLuneAnim_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattlePortraitDeactivationMaelleAnim_Path);

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

    // 구스타브 portrait 전용 메커니즘
    _gustaveMechanismRoot = Find_UI(_uiByName, L"Panel_BattlePortrait_Gustave_Mechanism");
    _gustaveCurrentChargeText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattlePortrait_Gustave_Current_Charge");
    _gustaveCurrentChargeTextBg = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattlePortrait_Gustave_Current_Charge_BG");
    _gustaveMaxChargeText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattlePortrait_Gustave_Max_Charge");
    _gustaveMaxChargeTextBg = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattlePortrait_Gustave_Max_Charge_BG");

    CHECK_NULL(_gustaveMechanismRoot, E_FAIL);
    CHECK_NULL(_gustaveCurrentChargeText, E_FAIL);
    CHECK_NULL(_gustaveCurrentChargeTextBg, E_FAIL);
    CHECK_NULL(_gustaveMaxChargeText, E_FAIL);
    CHECK_NULL(_gustaveMaxChargeTextBg, E_FAIL);

    static constexpr const wchar_t* kLuneSlotPrefixes[BattleMechanismCom::kLuneMaxStains] =
    {
        L"First",
        L"Second",
        L"Third",
        L"Fourth",
    };

    static constexpr const wchar_t* kLuneElementSuffixes[5] =
    {
        L"Fire",
        L"Ice",
        L"Lightning",
        L"Earth",
        L"Light",
    };

    for (uint32 slotIndex = 0; slotIndex < BattleMechanismCom::kLuneMaxStains; ++slotIndex)
    {
        for (uint32 elementIndex = 0; elementIndex < 5; ++elementIndex)
        {
            const wstring imageName =
                L"Image_" +
                wstring(kLuneSlotPrefixes[slotIndex]) +
                L"_" +
                kLuneElementSuffixes[elementIndex];

            _luneStainImages[slotIndex][elementIndex] = Find_UI(_uiByName, imageName);
            CHECK_NULL(_luneStainImages[slotIndex][elementIndex], E_FAIL);
        }
    }

    // 마엘
    _maelleCurrentStanceRoot = Find_UI(_uiByName, L"Panel_BattlePortrait_Maelle_Current_Stance");
    _maelleStanceless = Find_UI(_uiByName, L"Panel_BattlePortrait_Maelle_Stanceless");
    _maelleDefensive = Find_UI(_uiByName, L"Panel_BattlePortrait_Maelle_Defensive");
    _maelleOffensive = Find_UI(_uiByName, L"Panel_BattlePortrait_Maelle_Offensive");
    _maelleVirtuose = Find_UI(_uiByName, L"Panel_BattlePortrait_Maelle_Virtuose");

    CHECK_NULL(_maelleCurrentStanceRoot, E_FAIL);
    CHECK_NULL(_maelleStanceless, E_FAIL);
    CHECK_NULL(_maelleDefensive, E_FAIL);
    CHECK_NULL(_maelleOffensive, E_FAIL);
    CHECK_NULL(_maelleVirtuose, E_FAIL);

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

void UI_BattleSystemHUD::Apply_LuneState(const Shared<BattleMechanismCom>& mechanismCom)
{
    if (mechanismCom == nullptr || !mechanismCom->Is_Lune())
        return;

    Close_AllLuneStains(); // 매 프레임 현재 얼룩 배열 그대로 다시 연다.

    for (uint32 i = 0; i < mechanismCom->Get_LuneStainCount(); ++i)
    {
        if (Shared<UI_Base> image = Resolve_LuneStainImage(i, mechanismCom->Get_LuneStain(i)))
            image->Open();
    }
}

Shared<UI_Base> UI_BattleSystemHUD::Resolve_LuneStainImage(int32 slotIndex, ElementalType element) const
{
    if (slotIndex < 0 || slotIndex >= static_cast<int32>(BattleMechanismCom::kLuneMaxStains))
        return nullptr;

    uint32 elementIndex = 0;

    switch (element)
    {
    case ElementalType::Fire:
        elementIndex = 0;
        break;

    case ElementalType::Ice:
        elementIndex = 1;
        break;

    case ElementalType::Lightning:
        elementIndex = 2;
        break;

    case ElementalType::Earth:
        elementIndex = 3;
        break;

    case ElementalType::Light:
        elementIndex = 4;
        break;

    case ElementalType::None:
    case ElementalType::Dark:
    default:
        return nullptr;
    }

    return _luneStainImages[slotIndex][elementIndex];
}

void UI_BattleSystemHUD::Close_AllLuneStains()
{
    for (uint32 slotIndex = 0; slotIndex < BattleMechanismCom::kLuneMaxStains; ++slotIndex)
    {
        for (uint32 elementIndex = 0; elementIndex < 5; ++elementIndex)
        {
            if (_luneStainImages[slotIndex][elementIndex])
                _luneStainImages[slotIndex][elementIndex]->Close();
        }
    }
}

void UI_BattleSystemHUD::Apply_MaelleStanceBadge(BattleMechanismCom::MaelleStance stance)
{
    if (_maelleCurrentStanceRoot)
        _maelleCurrentStanceRoot->Open();

    Close_AllMaelleStanceBadges();

    switch (stance)
    {
    case BattleMechanismCom::MaelleStance::Offensive:
        if (_maelleOffensive)
            _maelleOffensive->Open();
        break;

    case BattleMechanismCom::MaelleStance::Defensive:
        if (_maelleDefensive)
            _maelleDefensive->Open();
        break;

    case BattleMechanismCom::MaelleStance::Virtuose:
        if (_maelleVirtuose)
            _maelleVirtuose->Open();
        break;

    case BattleMechanismCom::MaelleStance::Stanceless:
    default:
        if (_maelleStanceless)
            _maelleStanceless->Open();
        break;
    }
}

void UI_BattleSystemHUD::Close_AllMaelleStanceBadges()
{
    if (_maelleStanceless)
        _maelleStanceless->Close();

    if (_maelleDefensive)
        _maelleDefensive->Close();

    if (_maelleOffensive)
        _maelleOffensive->Close();

    if (_maelleVirtuose)
        _maelleVirtuose->Close();
}

void UI_BattleSystemHUD::Play_TargetAnimation(const wchar_t* animationName)
{
	for (auto& [uiName, ui] : _uiByName)
    {
        if (ui == nullptr)
            continue;

        ui->Play_UIAnimation(animationName, true);
    }
}

void UI_BattleSystemHUD::Stop_TargetAnimations()
{
	for (auto& [uiName, ui] : _uiByName)
    {
        if (ui == nullptr)
            continue;

        ui->Stop_UIAnimation();
    }
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

	Stop_TargetAnimations();

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

    _gustaveMechanismRoot.reset();
    _gustaveCurrentChargeText.reset();
    _gustaveCurrentChargeTextBg.reset();
    _gustaveMaxChargeText.reset();
    _gustaveMaxChargeTextBg.reset();

    for (uint32 slotIndex = 0; slotIndex < BattleMechanismCom::kLuneMaxStains; ++slotIndex)
    {
        for (uint32 elementIndex = 0; elementIndex < 5; ++elementIndex)
            _luneStainImages[slotIndex][elementIndex].reset();
    }

    _maelleCurrentStanceRoot.reset();
    _maelleStanceless.reset();
    _maelleDefensive.reset();
    _maelleOffensive.reset();
    _maelleVirtuose.reset();


    _root.reset();
    _uiByName.clear();

    Base::Free();
}

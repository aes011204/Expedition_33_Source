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

    constexpr Vec2 kBattlePictosPortraitAnchorOffset{ 70.f, -68.f };

    constexpr Vec2 kCurrentTurnApPositions[9] =
    {
        Vec2{ 95.21306268017625f, 23.29340820053867f },
        Vec2{ 95.04943987285988f, 21.70324476817105f },
        Vec2{ 95.37305604805002f, 21.546790404634205f },
        Vec2{ 95.64049251262853f, 21.147207400085513f },
        Vec2{ 95.52874902775294f, 20.965926007274472f },
        Vec2{ 95.38242665783588f, 21.186578073106332f },
        Vec2{ 95.8516612826187f, 21.33495265183805f },
        Vec2{ 95.47974026927716f, 21.422410404555134f },
        Vec2{ 95.3334178993601f, 21.635815134766453f },
    };

    constexpr Vec2 kCurrentTurnApSize{ 136.9499969482422f, 16.04599952697754f };

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
    if (Is_Open())
        return;

    UI_RuntimePanelBase::Open();
}

void UI_BattleSystemHUD::Close()
{
	Stop_TargetAnimations();
    Reset_TargetAnimationState(false);
    _activePortraitHeroId = HeroId::END;
    Invalidate_PartyStatSnapshots();

    UI_RuntimePanelBase::Close();

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

    const int32 hpValue = static_cast<int32>(curHp);
    const int32 maxHpValue = static_cast<int32>(maxHp);
    const float hpRatio = curHp / maxHp;
    const bool hpChanged =
        !slot.hasBattleStatSnapshot ||
        slot.cachedCurrentHp != hpValue ||
        slot.cachedMaxHp != maxHpValue ||
        slot.cachedHpRatio != hpRatio;
    const bool apChanged =
        !slot.hasBattleStatSnapshot ||
        slot.cachedCurrentAp != curAp ||
        slot.cachedMaxAp != maxAp;

    if (hpChanged)
    {
        Apply_HpGauge(slot, hpRatio);
        Set_HpText(slot, curHp, maxHp);
    }

    if (apChanged)
    {
        Apply_ApGauge(slot, slotIndex, curAp, maxAp);
        Set_ApText(slot, curAp);
    }

    slot.hasBattleStatSnapshot = true;
    slot.cachedCurrentHp = hpValue;
    slot.cachedMaxHp = maxHpValue;
    slot.cachedCurrentAp = curAp;
    slot.cachedMaxAp = maxAp;
    slot.cachedHpRatio = hpRatio;
}

void UI_BattleSystemHUD::Set_BattleHeroIds(const vector<HeroId>& heroIds)
{
    Set_BattleHeroIds(heroIds.empty() ? nullptr : heroIds.data(), heroIds.size());
}

void UI_BattleSystemHUD::Set_BattleHeroIds(const HeroId* heroIds, size_t heroCount)
{
    array<bool, 3> visiblePartySlots{ false, false, false };
    if (heroIds == nullptr)
        heroCount = 0;

    for (size_t i = 0; i < heroCount; ++i)
    {
        const HeroId heroId = heroIds[i];
        const uint32 slotIndex = Get_SlotIndex(heroId);
        if (slotIndex < visiblePartySlots.size())
            visiblePartySlots[slotIndex] = true;
    }

    if (!visiblePartySlots[0] && !visiblePartySlots[1] && !visiblePartySlots[2])
        visiblePartySlots = { true, true, true }; // 비정상 입력이면 기존 3인 HUD로 복구한다.

    if (_visiblePartySlots == visiblePartySlots)
        return;

    _visiblePartySlots = visiblePartySlots;
    Apply_BattlePartyLayout();
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

    if (_hasGustaveMechanismSnapshot &&
        _cachedGustaveCurrentCharge == currentCharge &&
        _cachedGustaveMaxCharge == maxCharge)
    {
        return;
    }

    Set_TextPair(_gustaveCurrentChargeText, _gustaveCurrentChargeTextBg, to_wstring(currentCharge));
    Set_TextPair(_gustaveMaxChargeText, _gustaveMaxChargeTextBg, to_wstring(maxCharge));

    _hasGustaveMechanismSnapshot = true;
    _cachedGustaveCurrentCharge = currentCharge;
    _cachedGustaveMaxCharge = maxCharge;
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
    if (_luneStainsClosed && _cachedLuneStainCount == 0)
        return;

    Close_AllLuneStains();
}

void UI_BattleSystemHUD::Clear_GustaveMechanismCharge()
{
    if (_hasGustaveMechanismSnapshot &&
        _cachedGustaveCurrentCharge == 0 &&
        _cachedGustaveMaxCharge == 10)
    {
        return;
    }

    Set_TextPair(_gustaveCurrentChargeText, _gustaveCurrentChargeTextBg, L"0");
    Set_TextPair(_gustaveMaxChargeText, _gustaveMaxChargeTextBg, L"10");
    _hasGustaveMechanismSnapshot = true;
    _cachedGustaveCurrentCharge = 0;
    _cachedGustaveMaxCharge = 10;
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

    if (_activePortraitHeroId != heroId)
        Invalidate_PartyStatSnapshots();

    _activePortraitHeroId = heroId;
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

    if (_activePortraitHeroId == heroId)
    {
        _activePortraitHeroId = HeroId::END;
        Invalidate_PartyStatSnapshots();
    }

    Play_TargetAnimation(animationName);
}

void UI_BattleSystemHUD::Play_BattleSystemIntro()
{
    _activePortraitHeroId = HeroId::END;
    Invalidate_PartyStatSnapshots();
    Reset_TargetAnimationState(true);
    Play_TargetAnimation(kBattleSystemIntroAnimationName);
}

bool UI_BattleSystemHUD::Try_GetPortraitPictosAnchor(HeroId heroId, Vec2& outPosition) const
{
    const uint32 slotIndex = Get_SlotIndex(heroId);
    if (slotIndex >= 3)
        return false;

    const PartyBattleStatSlot& slot = _partySlots[slotIndex];
    if (slot.root == nullptr || slot.root->Get_Transform() == nullptr)
        return false;

    outPosition = slot.root->Get_Transform()->Get_Position() + kBattlePictosPortraitAnchorOffset;
    return true;
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

        _partySlots[i].root = Find_UI(_uiByName, L"Panel_BattlePortrait_" + portraitName);
        CHECK_NULL(_partySlots[i].root, E_FAIL);

        if (_partySlots[i].root->Get_Transform())
            _partySlots[i].defaultPosition = _partySlots[i].root->Get_Transform()->Get_Position();

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

    Capture_TargetAnimationBaseColors();

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

uint32 UI_BattleSystemHUD::Get_SlotIndex(HeroId heroId)
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return 0;

    case HeroId::Lune:
        return 1;

    case HeroId::Maelle:
        return 2;

    case HeroId::END:
    default:
        return 3;
    }
}

void UI_BattleSystemHUD::Set_TextPair(const Shared<UI_Text>& front, const Shared<UI_Text>& back, const wstring& text)
{
    if (front && front->Get_Text() != text)
        front->Set_Text(text);

    if (back && back->Get_Text() != text)
        back->Set_Text(text);
}

void UI_BattleSystemHUD::Apply_HpGauge(PartyBattleStatSlot& slot, float ratio)
{
    if (slot.hpGauge == nullptr)
        return;

    ratio = clamp(ratio, 0.f, 1.f);
    slot.hpGauge->Set_FillRatio(ratio);
}

void UI_BattleSystemHUD::Apply_ApGauge(PartyBattleStatSlot& slot, uint32 slotIndex, uint32 curAp, uint32 maxAp)
{
    const uint32 clampedMaxAp = min(maxAp, 9u);

    for (uint32 i = 0; i < 9; ++i)
    {
        if (slot.apStates[i] == nullptr)
            continue;

        if (curAp == 0 || clampedMaxAp == 0)
        {
            if (slot.apStates[i]->Is_Open())
                slot.apStates[i]->Close();
            continue;
        }

        if (i < curAp && i < clampedMaxAp)
        {
            if (!slot.apStates[i]->Is_Open())
            {
                slot.apStates[i]->Open();
                Apply_CurrentTurnApTransform(slotIndex, i);
            }
        }
        else
        {
            if (slot.apStates[i]->Is_Open())
                slot.apStates[i]->Close();
        }
    }
}

void UI_BattleSystemHUD::Apply_BattlePartyLayout()
{
    const bool isGustaveOnly =
        _visiblePartySlots[0] &&
        !_visiblePartySlots[1] &&
        !_visiblePartySlots[2];

    for (uint32 i = 0; i < 3; ++i)
    {
        PartyBattleStatSlot& slot = _partySlots[i];
        if (slot.root == nullptr)
            continue;

        if (!_visiblePartySlots[i])
        {
            slot.root->Close(); // 부모가 닫히면 자식도 Is_EffectivelyOpen에서 렌더되지 않는다.
            continue;
        }

        if (!slot.root->Is_Open())
            slot.root->Open();

        if (slot.root->Get_Transform())
        {
            Vec2 position = slot.defaultPosition;

            if (isGustaveOnly && i == Get_SlotIndex(HeroId::Gustave))
                position = _partySlots[Get_SlotIndex(HeroId::Maelle)].defaultPosition; // 1인 튜토리얼에서는 구스타브를 가장 오른쪽 슬롯에 둔다.

            slot.root->Get_Transform()->Set_Position(position);
        }
    }
}

void UI_BattleSystemHUD::Apply_CurrentTurnApTransform(uint32 slotIndex, uint32 apIndex)
{
    if (!Is_CurrentTurnSlot(slotIndex) || apIndex >= 9)
        return;

    Shared<UI_Image> apState = _partySlots[slotIndex].apStates[apIndex];
    if (apState == nullptr || apState->Get_Transform() == nullptr)
        return;

    apState->Get_Transform()->Set_Position(kCurrentTurnApPositions[apIndex]);
    apState->Get_Transform()->Set_Size(kCurrentTurnApSize);
}

bool UI_BattleSystemHUD::Is_CurrentTurnSlot(uint32 slotIndex) const
{
    switch (_activePortraitHeroId)
    {
    case HeroId::Gustave:
        return slotIndex == 0;

    case HeroId::Lune:
        return slotIndex == 1;

    case HeroId::Maelle:
        return slotIndex == 2;

    case HeroId::END:
    default:
        return false;
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

void UI_BattleSystemHUD::Invalidate_PartyStatSnapshots()
{
    for (PartyBattleStatSlot& slot : _partySlots)
        slot.hasBattleStatSnapshot = false;
}

void UI_BattleSystemHUD::Apply_LuneState(const Shared<BattleMechanismCom>& mechanismCom)
{
    if (mechanismCom == nullptr || !mechanismCom->Is_Lune())
        return;

    array<ElementalType, BattleMechanismCom::kLuneMaxStains> nextStains{};
    nextStains.fill(ElementalType::None);

    const uint32 nextStainCount = min(mechanismCom->Get_LuneStainCount(), BattleMechanismCom::kLuneMaxStains);
    for (uint32 i = 0; i < nextStainCount; ++i)
        nextStains[i] = mechanismCom->Get_LuneStain(i);

    bool isUnchanged = _hasLuneStainSnapshot && _cachedLuneStainCount == nextStainCount;
    if (isUnchanged)
    {
        for (uint32 i = 0; i < BattleMechanismCom::kLuneMaxStains; ++i)
        {
            if (_cachedLuneStains[i] != nextStains[i])
            {
                isUnchanged = false;
                break;
            }
        }
    }

    if (isUnchanged)
        return;

    if (!_hasLuneStainSnapshot)
        Close_AllLuneStains();

    for (uint32 i = 0; i < BattleMechanismCom::kLuneMaxStains; ++i)
    {
        const ElementalType previousElement = _hasLuneStainSnapshot ? _cachedLuneStains[i] : ElementalType::None;
        const ElementalType nextElement = nextStains[i];

        if (previousElement == nextElement)
            continue;

        if (Shared<UI_Base> previousImage = Resolve_LuneStainImage(i, previousElement))
        {
            if (previousImage->Is_Open())
                previousImage->Close();
        }

        if (Shared<UI_Base> nextImage = Resolve_LuneStainImage(i, nextElement))
        {
            if (!nextImage->Is_Open())
                nextImage->Open();
        }
    }

    _cachedLuneStains = nextStains;
    _cachedLuneStainCount = nextStainCount;
    _hasLuneStainSnapshot = true;
    _luneStainsClosed = nextStainCount == 0;
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
            if (_luneStainImages[slotIndex][elementIndex] &&
                _luneStainImages[slotIndex][elementIndex]->Is_Open())
            {
                _luneStainImages[slotIndex][elementIndex]->Close();
            }
        }
    }

    _cachedLuneStains.fill(ElementalType::None);
    _cachedLuneStainCount = 0;
    _hasLuneStainSnapshot = false;
    _luneStainsClosed = true;
}

void UI_BattleSystemHUD::Apply_MaelleStanceBadge(BattleMechanismCom::MaelleStance stance)
{
    if (_hasMaelleStanceSnapshot && _cachedMaelleStance == stance)
        return;

    if (_maelleCurrentStanceRoot && !_maelleCurrentStanceRoot->Is_Open())
        _maelleCurrentStanceRoot->Open();

    if (_hasMaelleStanceSnapshot)
    {
        if (Shared<UI_Base> previousBadge = Resolve_MaelleStanceBadge(_cachedMaelleStance))
        {
            if (previousBadge->Is_Open())
                previousBadge->Close();
        }
    }
    else
    {
        Close_AllMaelleStanceBadges();
    }

    if (Shared<UI_Base> nextBadge = Resolve_MaelleStanceBadge(stance))
    {
        if (!nextBadge->Is_Open())
            nextBadge->Open();
    }

    _cachedMaelleStance = stance;
    _hasMaelleStanceSnapshot = true;
}

Shared<UI_Base> UI_BattleSystemHUD::Resolve_MaelleStanceBadge(BattleMechanismCom::MaelleStance stance) const
{
    switch (stance)
    {
    case BattleMechanismCom::MaelleStance::Offensive:
        return _maelleOffensive;

    case BattleMechanismCom::MaelleStance::Defensive:
        return _maelleDefensive;

    case BattleMechanismCom::MaelleStance::Virtuose:
        return _maelleVirtuose;

    case BattleMechanismCom::MaelleStance::Stanceless:
    default:
        return _maelleStanceless;
    }
}

void UI_BattleSystemHUD::Close_AllMaelleStanceBadges()
{
    if (_maelleStanceless)
    {
        if (_maelleStanceless->Is_Open())
            _maelleStanceless->Close();
    }

    if (_maelleDefensive)
    {
        if (_maelleDefensive->Is_Open())
            _maelleDefensive->Close();
    }

    if (_maelleOffensive)
    {
        if (_maelleOffensive->Is_Open())
            _maelleOffensive->Close();
    }

    if (_maelleVirtuose)
    {
        if (_maelleVirtuose->Is_Open())
            _maelleVirtuose->Close();
    }

    _cachedMaelleStance = BattleMechanismCom::MaelleStance::Stanceless;
    _hasMaelleStanceSnapshot = false;
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

void UI_BattleSystemHUD::Capture_TargetAnimationBaseColors()
{
    _targetAnimationBaseColors.clear();

    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        _targetAnimationBaseColors.emplace_back(ui, ui->Get_Color());
    }
}

void UI_BattleSystemHUD::Restore_TargetAnimationBaseColors()
{
    for (auto& [ui, color] : _targetAnimationBaseColors)
    {
        if (ui == nullptr)
            continue;

        ui->Set_Color(color);
    }
}

void UI_BattleSystemHUD::Reset_TargetAnimationState(bool includeRoot)
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        if (!includeRoot && ui == _root)
            continue;

        ui->Open();
        ui->Stop_UIAnimation();
    }

    Restore_TargetAnimationBaseColors();

    Close_AllLuneStains();
    Close_AllMaelleStanceBadges();
    Apply_BattlePartyLayout(); // animation reset이 전체 UI를 다시 열어도 현재 전투 파티만 보이게 복구한다.
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

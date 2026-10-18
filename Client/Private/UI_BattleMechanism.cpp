#include "UI_BattleMechanism.h"

#include "UI_Text.h"

#include "BattleMechanismCom.h"
#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kMechanismIntroAnimationName[] = L"BattleSystem_Mechanism_Intro";

    const Vec2 kGustaveChargePanelDefaultPosition{ 237.663f, 323.312f };
    const Vec2 kGustaveChargePanelFullChargePosition{ 192.663f, 296.312f };

    Vec4 Make_Color255(float r, float g, float b, float a = 255.f)
    {
        return Vec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
    }

    Vec4 Resolve_GustaveChargeColor(int32 currentCharge, int32 maxCharge)
    {
        if (maxCharge > 0 && currentCharge >= maxCharge)
            return Make_Color255(236.f, 75.f, 83.f);

        if (currentCharge >= 7)
            return Make_Color255(196.f, 115.f, 255.f);

        if (currentCharge >= 4)
            return Make_Color255(181.f, 247.f, 255.f);

        if (currentCharge >= 1)
            return Make_Color255(252.f, 251.f, 144.f);

        return Make_Color255(255.f, 255.f, 255.f);
    }

    void Set_UIOpen(const Shared<UI_Base>& ui, bool shouldOpen)
    {
        if (ui == nullptr)
            return;

        if (shouldOpen && !ui->Is_Open())
            ui->Open();
        else if (!shouldOpen && ui->Is_Open())
            ui->Close();
    }
}

HRESULT UI_BattleMechanism::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);
    CHECK_FAILED(Load_MechanismAnimations(), E_FAIL);

    Close();
    return S_OK;
}

void UI_BattleMechanism::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_BattleMechanism::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_BattleMechanism::Close()
{
    UI_RuntimePanelBase::Close();

    Close_AllRoots();
    _currentHeroId = HeroId::END;
}

void UI_BattleMechanism::Show_Hero(HeroId heroId)
{
    Shared<UI_Base> nextRoot = Resolve_HeroRoot(heroId);
    if (nextRoot == nullptr)
    {
        Close();
        return;
    }

    const bool wasClosed = !Is_Open();
    const bool heroChanged = (_currentHeroId != heroId);

    // 같은 hero가 계속 유지되는 프레임에서는 root를 다시 닫았다 열지 않는다.
    if (!wasClosed && !heroChanged)
        return;

    Open();

    _currentHeroId = heroId;
    Close_AllRoots();

    switch (heroId)
    {
    case HeroId::Gustave:
        _gustaveRoot->Open();
        break;

    case HeroId::Lune:
        _luneRoot->Open();
        break;

    case HeroId::Maelle:
        _maelleRoot->Open();
        Close_AllMaelleStances();
        if (_maelleStanceless)
            _maelleStanceless->Open();
        break;

    default:
        Close();
        return;
    }

    // 닫혀 있다가 다시 열리거나, 다른 hero mechanism으로 바뀐 경우에만 intro 1회 재생
    if (_hasMechanismIntroAnimation)
        nextRoot->Play_UIAnimation(kMechanismIntroAnimationName, true);
}

void UI_BattleMechanism::Sync_BattleChar(const BattleChar& actor)
{
    if (actor.mechanismCom == nullptr)
        return;

    switch (actor.heroId)
    {
    case HeroId::Gustave:
        Apply_GustaveState(actor.mechanismCom);
        break;

    case HeroId::Maelle:
        Apply_MaelleState(actor.mechanismCom);
        break;

    case HeroId::Lune:
        Apply_LuneState(actor.mechanismCom);
        break;

    default:
        break;
    }
}

HRESULT UI_BattleMechanism::Load_UI(uint32 levelIndex)
{
    umap<wstring, Shared<UI_Base>> gustaveUIByName{};
    CHECK_FAILED(Load_UIFragment(
        Constants::UI::UI_Gustave_Mechanism_Path,
        L"GustaveMechanism",
        gustaveUIByName), E_FAIL);

    for (const auto& [name, ui] : gustaveUIByName)
        _uiByName[name] = ui;

    umap<wstring, Shared<UI_Base>> luneUIByName{};
    CHECK_FAILED(Load_UIFragment(
        Constants::UI::UI_Lune_Mechanism_Path,
        L"LuneMechanism",
        luneUIByName), E_FAIL);

    for (const auto& [name, ui] : luneUIByName)
        _uiByName[name] = ui;

    umap<wstring, Shared<UI_Base>> maelleUIByName{};
    CHECK_FAILED(Load_UIFragment(
        Constants::UI::UI_Maelle_Mechanism_Path,
        L"MaelleMechanism",
        maelleUIByName), E_FAIL);

    for (const auto& [name, ui] : maelleUIByName)
        _uiByName[name] = ui;

    return S_OK;
}

HRESULT UI_BattleMechanism::Load_UIFragment(
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

HRESULT UI_BattleMechanism::Bind_UI()
{
    _gustaveRoot = Find_UI(_uiByName, L"Panel_Gustave_Mechanism");
    _luneRoot = Find_UI(_uiByName, L"Panel_Lune_Mechanism");
    _maelleRoot = Find_UI(_uiByName, L"Panel_Maelle_Mechanism");

    CHECK_NULL(_gustaveRoot, E_FAIL);
    CHECK_NULL(_luneRoot, E_FAIL);
    CHECK_NULL(_maelleRoot, E_FAIL);

#pragma region 구스타브
    _gustaveChargePanel = Find_UI(_uiByName, L"Panel_Gustave_Mechanism_Charge");
    _gustaveCurrentChargeText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Gustave_Mechanism_Current_Charge");
    _gustaveMaxChargeText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Gustave_Mechanism_Max_Charge");
    _gustaveSlashText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Gustave_Mechanism_Slash");
    _gustaveLightning = Find_UI(_uiByName, L"Image_Gustave_Arm_Lightning");
    _gustaveFullChargeDeco = Find_UI(_uiByName, L"Panel_Gustave_Arm_Full_Charge_Deco");

    CHECK_NULL(_gustaveChargePanel, E_FAIL);
    CHECK_NULL(_gustaveCurrentChargeText, E_FAIL);
    CHECK_NULL(_gustaveMaxChargeText, E_FAIL);
    CHECK_NULL(_gustaveSlashText, E_FAIL);
    CHECK_NULL(_gustaveLightning, E_FAIL);
    CHECK_NULL(_gustaveFullChargeDeco, E_FAIL);

    const int32 gustaveChargeNum = 10;

    for (int32 i = 0; i < gustaveChargeNum; ++i)
    {
        _gustaveChargeSlots[i] = Find_UI(_uiByName, L"Image_Gustave_Arm_Slot_" + to_wstring(i + 1));
        _gustaveChargeGlows[i] = Find_UI(_uiByName, L"Image_Gustave_Arm_Glow_" + to_wstring(i + 1));
        CHECK_NULL(_gustaveChargeSlots[i], E_FAIL);
        CHECK_NULL(_gustaveChargeGlows[i], E_FAIL);
    }
#pragma endregion

#pragma region 루네
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

#pragma endregion

#pragma region 마엘
    _maelleStanceless = Find_UI(_uiByName, L"Panel_Maelle_Mechanism_Stanceless");
    _maelleVirtuose = Find_UI(_uiByName, L"Panel_Maelle_Mechanism_Virtuose");
    _maelleDefensive = Find_UI(_uiByName, L"Panel_Maelle_Mechanism_Defensive");
    _maelleOffensive = Find_UI(_uiByName, L"Panel_Maelle_Mechanism_Offensive");

    CHECK_NULL(_maelleStanceless, E_FAIL);
    CHECK_NULL(_maelleVirtuose, E_FAIL);
    CHECK_NULL(_maelleDefensive, E_FAIL);
    CHECK_NULL(_maelleOffensive, E_FAIL);
#pragma endregion

    return S_OK;
}

void UI_BattleMechanism::Close_AllRoots()
{
    if (_gustaveRoot)
        _gustaveRoot->Close();

    if (_luneRoot)
        _luneRoot->Close();

    if (_maelleRoot)
        _maelleRoot->Close();

    Close_AllLuneStains();
    Close_AllMaelleStances();
}

HRESULT UI_BattleMechanism::Load_MechanismAnimations()
{
    _hasMechanismIntroAnimation =
        UI_RuntimeLoader::Load_PublicAnimationAsset(
            Constants::UI_Anim::UI_BattleMechanismIntroAnim_Path,
            _mechanismIntroAnimation);
    CHECK_TRUE(_hasMechanismIntroAnimation, E_FAIL);

    _gustaveRoot->Add_UIAnimationAsset(_mechanismIntroAnimation);
    _luneRoot->Add_UIAnimationAsset(_mechanismIntroAnimation);
    _maelleRoot->Add_UIAnimationAsset(_mechanismIntroAnimation);

    return S_OK;
}

Shared<UI_Base> UI_BattleMechanism::Resolve_HeroRoot(HeroId heroId) const
{
    switch (heroId)
    {
    case HeroId::Gustave:
        return _gustaveRoot;

    case HeroId::Lune:
        return _luneRoot;

    case HeroId::Maelle:
        return _maelleRoot;

    case HeroId::END:
    default:
        return nullptr;
    }
}

void UI_BattleMechanism::Apply_GustaveState(const Shared<BattleMechanismCom>& mechanismCom)
{
    if (mechanismCom == nullptr || !mechanismCom->Is_Gustave())
        return;

    const int32 currentCharge = mechanismCom->Get_GustaveCharge();
    const int32 maxCharge = mechanismCom->Get_GustaveMaxCharge();

    _gustaveCurrentChargeText->Set_Text(to_wstring(currentCharge));
    _gustaveMaxChargeText->Set_Text(to_wstring(maxCharge));

    const Vec4 chargeColor = Resolve_GustaveChargeColor(currentCharge, maxCharge);

    _gustaveCurrentChargeText->Set_Color(chargeColor);
    _gustaveMaxChargeText->Set_Color(chargeColor);
    _gustaveSlashText->Set_Color(chargeColor);

    for (int32 i = 0; i < static_cast<int32>(_gustaveChargeGlows.size()); ++i)
    {
        if (_gustaveChargeSlots[i])
        {
            _gustaveChargeSlots[i]->Set_Color(chargeColor);
            Set_UIOpen(_gustaveChargeSlots[i], i < currentCharge);
        }

        if (_gustaveChargeGlows[i])
        {
            _gustaveChargeGlows[i]->Set_Color(chargeColor);
            Set_UIOpen(_gustaveChargeGlows[i], i < currentCharge);
        }
    }

    _gustaveLightning->Set_Color(chargeColor);
    Set_UIOpen(_gustaveLightning, currentCharge > 0);

    const bool isFullCharge = (maxCharge > 0 && currentCharge >= maxCharge);
    Set_UIOpen(_gustaveFullChargeDeco, isFullCharge);

    if (_gustaveChargePanel->Get_Transform())
    {
        _gustaveChargePanel->Get_Transform()->Set_Position(
            isFullCharge ? kGustaveChargePanelFullChargePosition : kGustaveChargePanelDefaultPosition);
    }
}

void UI_BattleMechanism::Apply_MaelleState(const Shared<BattleMechanismCom>& mechanismCom)
{
    if (mechanismCom == nullptr || !mechanismCom->Is_Maelle())
        return;

    Close_AllMaelleStances();

    switch (mechanismCom->Get_MaelleStance())
    {
    case BattleMechanismCom::MaelleStance::Stanceless:
        if (_maelleStanceless)
            _maelleStanceless->Open();
        break;

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

    default:
        if (_maelleStanceless)
            _maelleStanceless->Open();
        break;
    }
}

void UI_BattleMechanism::Apply_LuneState(const Shared<BattleMechanismCom>& mechanismCom)
{
    if (mechanismCom == nullptr || !mechanismCom->Is_Lune())
        return;

    Close_AllLuneStains();

    for (uint32 i = 0; i < mechanismCom->Get_LuneStainCount(); ++i)
    {
        if (Shared<UI_Base> image = Resolve_LuneStainImage(i, mechanismCom->Get_LuneStain(i)))
            image->Open();
    }
}

Shared<UI_Base> UI_BattleMechanism::Resolve_LuneStainImage(int32 slotIndex, ElementalType element) const
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

void UI_BattleMechanism::Close_AllLuneStains()
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

void UI_BattleMechanism::Close_AllMaelleStances()
{
    if (_maelleStanceless)
        _maelleStanceless->Close();

    if (_maelleVirtuose)
        _maelleVirtuose->Close();

    if (_maelleDefensive)
        _maelleDefensive->Close();

    if (_maelleOffensive)
        _maelleOffensive->Close();
}

Unique<UI_BattleMechanism> UI_BattleMechanism::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleMechanism>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleMechanism");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleMechanism::Free()
{
    if (_root)
        _root->Close();

    Close_AllRoots();

    _gustaveRoot.reset();
    _gustaveChargePanel.reset();
    _gustaveCurrentChargeText.reset();
    _gustaveMaxChargeText.reset();
    _gustaveSlashText.reset();
    _gustaveLightning.reset();
    _gustaveFullChargeDeco.reset();

    for (Shared<UI_Base>& chargeSlot : _gustaveChargeSlots)
        chargeSlot.reset();

    for (Shared<UI_Base>& chargeGlow : _gustaveChargeGlows)
        chargeGlow.reset();

    _luneRoot.reset();
    _maelleRoot.reset();

    for (uint32 slotIndex = 0; slotIndex < BattleMechanismCom::kLuneMaxStains; ++slotIndex)
    {
        for (uint32 elementIndex = 0; elementIndex < 5; ++elementIndex)
            _luneStainImages[slotIndex][elementIndex].reset();
    }

    _maelleStanceless.reset();
    _maelleVirtuose.reset();
    _maelleDefensive.reset();
    _maelleOffensive.reset();

    _root.reset();
    _uiByName.clear();

    Base::Free();
}

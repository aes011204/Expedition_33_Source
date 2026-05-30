#include "UI_BattleMechanism.h"

#include "UI_Text.h"

#include "BattleMechanismCom.h"
#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_BattleMechanism::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

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
    Open();

    _currentHeroId = heroId;
    Close_AllRoots();

    switch (heroId)
    {
    case HeroId::Gustave:
        if (_gustaveRoot)
            _gustaveRoot->Open();
        break;

    case HeroId::Lune:
        if (_luneRoot)
            _luneRoot->Open();
        break;

    case HeroId::Maelle:
    {
        if (_maelleRoot)
            _maelleRoot->Open();

        Close_AllMaelleStances();

        if (_maelleStanceless)
        {
            _maelleStanceless->Open();
        }

    }

    break;

    default:
        Close();
        break;
    }
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
    _gustaveCurrentChargeText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Gustave_Mechanism_Current_Charge");
    _gustaveMaxChargeText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Gustave_Mechanism_Max_Charge");

    CHECK_NULL(_gustaveCurrentChargeText, E_FAIL);
    CHECK_NULL(_gustaveMaxChargeText, E_FAIL);

    const int32 gustaveChargeNum = 10;

    for (int32 i = 0; i < gustaveChargeNum; ++i)
    {
        _gustaveChargeGlows[i] = Find_UI(_uiByName, L"Image_Gustave_Arm_Glow_" + to_wstring(i + 1));
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

    static constexpr const wchar_t* kLuneElementSuffixes[4] =
    {
        L"Fire",
        L"Ice",
        L"Lightning",
        L"Light",
    };

    for (uint32 slotIndex = 0; slotIndex < BattleMechanismCom::kLuneMaxStains; ++slotIndex)
    {
        for (uint32 elementIndex = 0; elementIndex < 4; ++elementIndex)
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

void UI_BattleMechanism::Apply_GustaveState(const Shared<BattleMechanismCom>& mechanismCom)
{
    if (mechanismCom == nullptr || !mechanismCom->Is_Gustave())
        return;

    const int32 currentCharge = mechanismCom->Get_GustaveCharge();
    const int32 maxCharge = mechanismCom->Get_GustaveMaxCharge();

    _gustaveCurrentChargeText->Set_Text(to_wstring(currentCharge));
    _gustaveMaxChargeText->Set_Text(to_wstring(maxCharge));

    for (int32 i = 0; i < static_cast<int32>(_gustaveChargeGlows.size()); ++i)
    {
        if (_gustaveChargeGlows[i] == nullptr)
            continue;

        if (i < currentCharge)
            _gustaveChargeGlows[i]->Open();
        else
            _gustaveChargeGlows[i]->Close();
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

    case ElementalType::Light:
        elementIndex = 3;
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
        for (uint32 elementIndex = 0; elementIndex < 4; ++elementIndex)
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
    _luneRoot.reset();
    _maelleRoot.reset();
    for (uint32 slotIndex = 0; slotIndex < BattleMechanismCom::kLuneMaxStains; ++slotIndex)
    {
        for (uint32 elementIndex = 0; elementIndex < 4; ++elementIndex)
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

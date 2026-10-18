#include "ExpeditionStat_Panel.h"

#include "ClientInstance.h"
#include "Client_Constants.h"
#include "GameInstance.h"
#include "Player_WorldBase.h"
#include "PlayerStatCom.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT ExpeditionStat_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately();
    return S_OK;
}

void ExpeditionStat_Panel::Update(float timeDelta)
{
    if (!_isOpen)
        return;

    Refresh_PlayerHpStats();

    if (!_isClosingWithOutro)
        return;

    _closeElapsed += max(0.f, timeDelta);

    if (_closeElapsed >= _outroDuration)
        Close_Immediately();
}

void ExpeditionStat_Panel::Open()
{
    if (_root == nullptr)
        return;

    if (_isOpen && !_isClosingWithOutro)
        return;

    _isClosingWithOutro = false;
    _closeElapsed = 0.f;

    UI_RuntimePanelBase::Open();
    Refresh_PlayerHpStats();

    Play_TargetAnimation(L"Expedition_Status_Intro");

    GAME->Unlock_Mouse(); 
    GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly);
}

void ExpeditionStat_Panel::Close()
{
    if (_root == nullptr)
    {
        Close_Immediately();
        return;
    }

    if (!_isOpen && !_isClosingWithOutro)
        return;

    if (_isClosingWithOutro)
        return;

    _isClosingWithOutro = true;
    _closeElapsed = 0.f;

    Play_TargetAnimation(L"Expedition_Status_Outro");

    if (_outroDuration <= 0.f)
        Close_Immediately();
}

void ExpeditionStat_Panel::Close_Immediately()
{
    _isClosingWithOutro = false;
    _closeElapsed = 0.f;

    Stop_TargetAnimations();

    UI_RuntimePanelBase::Close();

    GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
    GAME->Lock_Mouse();
}

void ExpeditionStat_Panel::Play_TargetAnimation(const wchar_t* animationName)
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Play_UIAnimation(animationName, true);
    }
}

void ExpeditionStat_Panel::Stop_TargetAnimations()
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Stop_UIAnimation();
    }
}

void ExpeditionStat_Panel::Toggle()
{
    if (_isOpen && !_isClosingWithOutro)
    {
        Close();
        return;
    }

    Open();
}

HRESULT ExpeditionStat_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_ExpeditionStatus_Path;
    desc.prototypeTagPrefix = L"ExpeditionStatus";
    desc.outUIByName = &_uiByName;
    desc.animationPaths =
    {
        Constants::UI_Anim::UI_ExpeditionStatusIntro_Anim_Path,
        Constants::UI_Anim::UI_ExpeditionStatusOutro_Anim_Path
    };

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_Check_Expedition");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT ExpeditionStat_Panel::Bind_UI()
{
    Set_PlayerHp(HeroId::Gustave, L"Gustave", 0);
    Set_PlayerHp(HeroId::Lune, L"Lune", 1);
    Set_PlayerHp(HeroId::Maelle, L"Maelle", 2);

    for (uint32 i = 0; i < kPlayerHpSlotCount; ++i)
    {
        CHECK_NULL(_playerHpSlots[i].hpGauge, E_FAIL);
        CHECK_NULL(_playerHpSlots[i].currentHpText, E_FAIL);
        CHECK_NULL(_playerHpSlots[i].maxHpText, E_FAIL);

        _playerHpSlots[i].hpGauge->Set_UseFillRatio(true);
        _playerHpSlots[i].hpGauge->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);
    }

    return S_OK;
}

void ExpeditionStat_Panel::Refresh_PlayerHpStats()
{
    Set_PlayerHp(HeroId::Gustave, L"Gustave", 0);
    Set_PlayerHp(HeroId::Lune, L"Lune", 1);
    Set_PlayerHp(HeroId::Maelle, L"Maelle", 2);
}

void ExpeditionStat_Panel::Set_PlayerHp(HeroId heroId, const wstring& uiName, uint32 slotIndex)
{
    if (slotIndex >= kPlayerHpSlotCount)
        return;

    if (_playerHpSlots[slotIndex].hpGauge == nullptr)
    {
        _playerHpSlots[slotIndex].hpGauge =
            Find_TypedUI<UI_Image>(_uiByName, L"Text_" + uiName + L"_HP_Gauge");
    }

    if (_playerHpSlots[slotIndex].currentHpText == nullptr)
    {
        _playerHpSlots[slotIndex].currentHpText =
            Find_TypedUI<UI_Text>(_uiByName, L"Text_" + uiName + L"_Current_HP");
    }

    if (_playerHpSlots[slotIndex].maxHpText == nullptr)
    {
        _playerHpSlots[slotIndex].maxHpText =
            Find_TypedUI<UI_Text>(_uiByName, L"Text_" + uiName + L"_Max_HP");
    }

    const Shared<Player_WorldBase> player = CLIENT->Find_Hero(heroId);
    const Shared<PlayerStatCom> playerStat = player ? player->Get_Component<PlayerStatCom>() : nullptr;

    if (playerStat == nullptr)
    {
        Apply_HpSlot(slotIndex, 0.f, 1.f);
        return;
    }

    const PlayerStatCom::PRuntimeCombatStat& statInfo = playerStat->Get_PlayerRuntimeStat();
    Apply_HpSlot(slotIndex, statInfo.CurHp, statInfo.CurMaxHp);
}

void ExpeditionStat_Panel::Apply_HpSlot(uint32 slotIndex, float curHp, float maxHp)
{
    if (slotIndex >= kPlayerHpSlotCount)
        return;

    PlayerHpSlot& slot = _playerHpSlots[slotIndex];

    maxHp = max(maxHp, 1.f);
    curHp = clamp(curHp, 0.f, maxHp);

    if (slot.hpGauge)
        slot.hpGauge->Set_FillRatio(curHp / maxHp);

    Set_Text(slot.currentHpText, to_wstring(static_cast<int32>(curHp)));
    Set_Text(slot.maxHpText, to_wstring(static_cast<int32>(maxHp)));
}

void ExpeditionStat_Panel::Set_Text(const Shared<UI_Text>& text, const wstring& value)
{
    if (text)
        text->Set_Text(value);
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
        Close_Immediately();

    for (uint32 i = 0; i < kPlayerHpSlotCount; ++i)
    {
        _playerHpSlots[i].hpGauge.reset();
        _playerHpSlots[i].currentHpText.reset();
        _playerHpSlots[i].maxHpText.reset();
    }

    _root.reset();
    _uiByName.clear();

    Base::Free();
}

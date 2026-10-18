#include "SavePoint_Panel.h"

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "Player_WorldBase.h"
#include "PlayerStatCom.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

HRESULT SavePoint_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately(false);

    return S_OK;
}

void SavePoint_Panel::Update(float timeDelta)
{
    if (!_isOpen)
        return;

    Refresh_PlayerHpStats();

    if (_isClosingWithOutro)
    {
        _closeElapsed += max(0.f, timeDelta);

        if (_closeElapsed >= _outroDuration)
            Close_Immediately(_restoreWorldControlOnClose);

        return;
    }

    Handle_ButtonInput();
}

void SavePoint_Panel::Open()
{
    if (_root == nullptr)
        return;

    if (_isOpen && !_isClosingWithOutro)
        return;

    _isClosingWithOutro = false;
    _restoreWorldControlOnClose = true;
    _closeElapsed = 0.f;

    Stop_TargetAnimations();
    UI_RuntimePanelBase::Open();
    Refresh_PlayerHpStats();
    Play_TargetAnimation(L"SavePoint_Intro");
    GAME->Play_SoundCue(Constants::SoundCue::UI_SavePoint_Open);

    _lastAction = SavePointAction::None;

    // 마우스 잠금
    GAME->Unlock_Mouse();
    GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly);
}

void SavePoint_Panel::Close()
{
    Start_CloseWithOutro(true);
}

void SavePoint_Panel::Close_Immediately(bool restoreWorldControl)
{
    _isClosingWithOutro = false;
    _restoreWorldControlOnClose = true;
    _closeElapsed = 0.f;

    Stop_TargetAnimations();
    UI_RuntimePanelBase::Close();

    if (restoreWorldControl)
    {
        GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
        GAME->Lock_Mouse();
    }
}

void SavePoint_Panel::Close_ForMenuTransition()
{
    Close_Immediately(false);
}

void SavePoint_Panel::Toggle()
{
    if (_isOpen && !_isClosingWithOutro)
    {
        Close();
        return;
    }

    Open();
}

SavePoint_Panel::SavePointAction SavePoint_Panel::Consume_LastAction()
{
    const SavePointAction action = _lastAction;
    _lastAction = SavePointAction::None;

    return action;
}

HRESULT SavePoint_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_SavePoint_Path;
    desc.prototypeTagPrefix = L"SavePoint";
    desc.outUIByName = &_uiByName;
    desc.animationPaths =
    {
        Constants::UI_Anim::UI_SavePointIntro_Anim_Path,
        Constants::UI_Anim::UI_SavePointOutro_Anim_Path
    };

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_SavePoint");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT SavePoint_Panel::Bind_UI()
{
    _restButton = UI_Utils::Find_TypedUI<UI_Button>(_uiByName, L"Button_SavePoint_Rest");
    _upgradeStatButton = UI_Utils::Find_TypedUI<UI_Button>(_uiByName, L"Button_SavePoint_Upgrade_Stat");
    _upgradeSkillButton = UI_Utils::Find_TypedUI<UI_Button>(_uiByName, L"Button_SavePoint_Upgrade_Skill");

    CHECK_NULL(_restButton, E_FAIL);
    CHECK_NULL(_upgradeStatButton, E_FAIL);
    CHECK_NULL(_upgradeSkillButton, E_FAIL);

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

void SavePoint_Panel::Start_CloseWithOutro(bool restoreWorldControl)
{
    if (_root == nullptr)
    {
        Close_Immediately(restoreWorldControl);
        return;
    }

    if (!_isOpen && !_isClosingWithOutro)
        return;

    if (_isClosingWithOutro)
        return;

    _isClosingWithOutro = true;
    _restoreWorldControlOnClose = restoreWorldControl;
    _closeElapsed = 0.f;

    Play_TargetAnimation(L"SavePoint_Outro");
    GAME->Play_SoundCue(Constants::SoundCue::UI_SavePoint_Close);

    if (_outroDuration <= 0.f)
        Close_Immediately(_restoreWorldControlOnClose);
}

void SavePoint_Panel::Play_TargetAnimation(const wchar_t* animationName)
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Play_UIAnimation(animationName, true);
    }
}

void SavePoint_Panel::Stop_TargetAnimations()
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Stop_UIAnimation();
    }
}

void SavePoint_Panel::Handle_ButtonInput()
{
    if (_restButton && _restButton->Was_Clicked())
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_SavePoint_Resting);
        _lastAction = SavePointAction::Rest;
        LOG_DEBUG("SavePoint_Panel: Rest clicked");
        return;
    }

    if (_upgradeStatButton && _upgradeStatButton->Was_Clicked())
    {
        _lastAction = SavePointAction::UpgradeStat;
        LOG_DEBUG("SavePoint_Panel: UpgradeStat clicked");
        return;
    }

    if (_upgradeSkillButton && _upgradeSkillButton->Was_Clicked())
    {
        _lastAction = SavePointAction::UpgradeSkill;
        LOG_DEBUG("SavePoint_Panel: UpgradeSkill clicked");
        return;
    }
}

void SavePoint_Panel::Refresh_PlayerHpStats()
{
    Set_PlayerHp(HeroId::Gustave, L"Gustave", 0);
    Set_PlayerHp(HeroId::Lune, L"Lune", 1);
    Set_PlayerHp(HeroId::Maelle, L"Maelle", 2);
}

void SavePoint_Panel::Set_PlayerHp(HeroId heroId, const wstring& uiName, uint32 slotIndex)
{
    if (slotIndex >= kPlayerHpSlotCount)
        return;

    if (_playerHpSlots[slotIndex].hpGauge == nullptr)
    {
        _playerHpSlots[slotIndex].hpGauge =
            UI_Utils::Find_TypedUI<UI_Image>(_uiByName, L"Text_" + uiName + L"_HP_Gauge");
    }

    if (_playerHpSlots[slotIndex].currentHpText == nullptr)
    {
        _playerHpSlots[slotIndex].currentHpText =
            UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_" + uiName + L"_Current_HP");
    }

    if (_playerHpSlots[slotIndex].maxHpText == nullptr)
    {
        _playerHpSlots[slotIndex].maxHpText =
            UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_" + uiName + L"_Max_HP");
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

void SavePoint_Panel::Apply_HpSlot(uint32 slotIndex, float curHp, float maxHp)
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

void SavePoint_Panel::Set_Text(const Shared<UI_Text>& text, const wstring& value)
{
    if (text)
        text->Set_Text(value);
}

Unique<SavePoint_Panel> SavePoint_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<SavePoint_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : SavePoint_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void SavePoint_Panel::Free()
{
    if (_root)
        Close_Immediately(false);

    _restButton.reset();
    _upgradeStatButton.reset();
    _upgradeSkillButton.reset();

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

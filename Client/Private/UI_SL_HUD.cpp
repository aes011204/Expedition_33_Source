#include "UI_SL_HUD.h"

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "MonsterStatCom.h"
#include "PlayerStatCom.h"
#include "SL_Base.h"
#include "SL_Maelle.h"
#include "SL_Types.h"
#include "SoulLikeController.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr float kBossHpDelayedLerpSpeed = 4.f;

    wstring Resolve_SLBossDisplayName(wstring name)
    {
        if (!name.empty() && (name.back() == L'\uAC00' || name.back() == L'\uC774'))
            name.pop_back();

        return name;
    }
}

HRESULT UI_SL_HUD::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Clear_PlayerHUD();
    return S_OK;
}

void UI_SL_HUD::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_SL_HUD::Update_PlayerHUD(const Shared<SL_Maelle>& player)
{
    if (player == nullptr)
    {
        Clear_PlayerHUD();
        return;
    }

    const Shared<PlayerStatCom> playerStat =
        dynamic_pointer_cast<PlayerStatCom>(player->Get_Stat());

    if (playerStat == nullptr)
    {
        Clear_PlayerHUD();
        return;
    }

    Open();

    if (_estherSkillRoot)
        _estherSkillRoot->Open();

    Apply_PlayerHp(
        playerStat->Get_CurHp(),
        playerStat->Get_StatInfo().maxHp);

    Apply_SkillIcons(player);
    Apply_SkillCooldowns(player);

    if (const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController())
        Set_HealingPotionCount(controller->Get_HealingPotionCount());
}

void UI_SL_HUD::Clear_PlayerHUD()
{
    Close();

    Apply_PlayerHp(0.f, 1.f);
    Clear_BossHUD();

    for (uint32 i = 0; i < _skillIcons.size(); ++i)
        Clear_SkillSlot(i);
}

void UI_SL_HUD::Update_BossHUD(const Shared<SL_Base>& boss, float timeDelta)
{
    const Shared<MonsterStatCom> bossStat = boss
        ? dynamic_pointer_cast<MonsterStatCom>(boss->Get_Stat())
        : nullptr;

    if (boss == nullptr || boss->Is_Dead() || bossStat == nullptr || !bossStat->Is_Boss())
    {
        Clear_BossHUD();
        return;
    }

    if (_bossHpRoot)
        _bossHpRoot->Open();

    Apply_BossHp(
        bossStat->Get_CurHp(),
        bossStat->Get_StatInfo().maxHp,
        timeDelta);
    Apply_BossBreak(
        bossStat->Get_CurBreak(),
        bossStat->Get_MaxBreak());

    Apply_BossName(Resolve_SLBossDisplayName(bossStat->Get_MonsterDesc().battleAnnounceName));
}

void UI_SL_HUD::Clear_BossHUD()
{
    if (_bossHpRoot)
    {
        _bossHpRoot->Close();
    }

    _bossHpDelayedRatio = 1.f;
    Apply_BossHp(0.f, 1.f, 0.f);
    Apply_BossBreak(0.f, 0.f);
    Apply_BossName(L"");
}

void UI_SL_HUD::Set_HealingPotionCount(uint32 count)
{
    if (_healingPotionCountText)
        _healingPotionCountText->Set_Text(to_wstring(count));
}

void UI_SL_HUD::Show_BossDamage(float damage)
{
    UNREFERENCED_PARAMETER(damage);
}

HRESULT UI_SL_HUD::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_SL_HUD_Path;
    desc.prototypeTagPrefix = L"SLHUD";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_SL_HUD");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_SL_HUD::Bind_UI()
{
    _playerHpGauge = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Character_HP_Gauge");
    _playerCurrentHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Character_Current_HP");
    _playerCurrentHpBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Character_Current_HP_BG");
    _playerMaxHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Character_Max_HP");
    _playerMaxHpBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Character_Max_HP_BG");

    CHECK_NULL(_playerHpGauge, E_FAIL);
    CHECK_NULL(_playerCurrentHpText, E_FAIL);
    CHECK_NULL(_playerCurrentHpBgText, E_FAIL);
    CHECK_NULL(_playerMaxHpText, E_FAIL);
    CHECK_NULL(_playerMaxHpBgText, E_FAIL);

    _playerHpGauge->Set_UseFillRatio(true);
    _playerHpGauge->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);

    _skillIcons[0] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_First_Skill_Icon");
    _skillIcons[1] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Second_Skill_Icon");
    _skillIcons[2] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Third_Skill_Icon");
    _skillIcons[3] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Fourth_Skill_Icon");

    _skillIconBgs[0] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_First_Skill_BG");
    _skillIconBgs[1] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Second_Skill_BG");
    _skillIconBgs[2] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Third_Skill_BG");
    _skillIconBgs[3] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Fourth_Skill_BG");

    _skillCooldownOverlays[0] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_First_Skill_Cooldown");
    _skillCooldownOverlays[1] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Second_Skill_Cooldown");
    _skillCooldownOverlays[2] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Third_Skill_Cooldown");
    _skillCooldownOverlays[3] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Fourth_Skill_Cooldown");

    _skillCooldownTexts[0] = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_First_Skill_Cooldown");
    _skillCooldownTexts[1] = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Second_Skill_Cooldown");
    _skillCooldownTexts[2] = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Third_Skill_Cooldown");
    _skillCooldownTexts[3] = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Fourtht_Skill_Cooldown");

    for (uint32 i = 0; i < _skillIcons.size(); ++i)
    {
        CHECK_NULL(_skillIcons[i], E_FAIL);
        CHECK_NULL(_skillIconBgs[i], E_FAIL);
        CHECK_NULL(_skillCooldownOverlays[i], E_FAIL);
        CHECK_NULL(_skillCooldownTexts[i], E_FAIL);

        _skillCooldownOverlays[i]->Set_UseFillRatio(true);
        _skillCooldownOverlays[i]->Set_FillOrigin(UI_Image::FillOrigin::TopToBottom);
        _skillCooldownOverlays[i]->Set_Color(Vec4(0.f, 0.f, 0.f, 0.22f));
        _skillCooldownOverlays[i]->Close();
        _skillCooldownTexts[i]->Close();
    }

    _bossHpRoot = Find_UI(_uiByName, L"Panel_SL_Boss_HP_Bar");
    _bossHpGauge = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Boss_HP_Gauge");
    _bossHpGaugeBg = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Boss_HP_Gauge_BG");
    _bossBreakRoot = Find_UI(_uiByName, L"Panel_SL_Boss_Break_Bar");
    _bossBreakGauge = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Boss_Break_Gauge");
    _bossNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Boss_Name");
    _bossNameBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Boss_Name_BG");

    CHECK_NULL(_bossHpRoot, E_FAIL);
    CHECK_NULL(_bossHpGauge, E_FAIL);
    CHECK_NULL(_bossHpGaugeBg, E_FAIL);
    CHECK_NULL(_bossBreakRoot, E_FAIL);
    CHECK_NULL(_bossBreakGauge, E_FAIL);
    CHECK_NULL(_bossNameText, E_FAIL);
    CHECK_NULL(_bossNameBgText, E_FAIL);

    _bossHpGauge->Set_UseFillRatio(true);
    _bossHpGauge->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);
    _bossHpGaugeBg->Set_UseFillRatio(true);
    _bossHpGaugeBg->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);
    _bossBreakGauge->Set_UseFillRatio(true);
    _bossBreakGauge->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);
    _bossHpRoot->Close();
    _bossBreakRoot->Close();

    _staminaRoot = Find_UI(_uiByName, L"Panel_SL_Character_Stamina_Bar");
    _potionRoot = Find_UI(_uiByName, L"Panel_SL_Healing_Potion");
    _healingPotionCountText = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Healing_Potion_Current_Count");
    _estherSkillRoot = Find_UI(_uiByName, L"Panel_SL_Esther_Skill");

    CHECK_NULL(_healingPotionCountText, E_FAIL);
    CHECK_NULL(_estherSkillRoot, E_FAIL);

    if (_staminaRoot)
        _staminaRoot->Close(); // v1 has no stamina owner yet, so hide the JSON placeholder.

    _estherSkillRoot->Open();

    return S_OK;
}

void UI_SL_HUD::Apply_PlayerHp(float curHp, float maxHp)
{
    maxHp = max(maxHp, 1.f);
    curHp = clamp(curHp, 0.f, maxHp);

    if (_playerHpGauge)
        _playerHpGauge->Set_FillRatio(curHp / maxHp);

    const wstring curText = to_wstring(static_cast<uint32>(curHp));
    const wstring maxText = to_wstring(static_cast<uint32>(maxHp));

    if (_playerCurrentHpText)
        _playerCurrentHpText->Set_Text(curText);

    if (_playerCurrentHpBgText)
        _playerCurrentHpBgText->Set_Text(curText);

    if (_playerMaxHpText)
        _playerMaxHpText->Set_Text(maxText);

    if (_playerMaxHpBgText)
        _playerMaxHpBgText->Set_Text(maxText);
}

void UI_SL_HUD::Apply_BossHp(float curHp, float maxHp, float timeDelta)
{
    maxHp = max(maxHp, 1.f);
    curHp = clamp(curHp, 0.f, maxHp);

    const float ratio = curHp / maxHp;

    if (_bossHpGauge)
        _bossHpGauge->Set_FillRatio(ratio);

    if (_bossHpDelayedRatio < ratio || timeDelta <= 0.f)
        _bossHpDelayedRatio = ratio;
    else
        _bossHpDelayedRatio = std::lerp(_bossHpDelayedRatio, ratio, clamp(timeDelta * kBossHpDelayedLerpSpeed, 0.f, 1.f));

    if (_bossHpGaugeBg)
        _bossHpGaugeBg->Set_FillRatio(clamp(_bossHpDelayedRatio, 0.f, 1.f));
}

void UI_SL_HUD::Apply_BossBreak(float curBreak, float maxBreak)
{
    if (_bossBreakRoot == nullptr || _bossBreakGauge == nullptr)
        return;

    if (maxBreak <= 0.f)
    {
        _bossBreakGauge->Set_FillRatio(0.f);
        _bossBreakRoot->Close();
        return;
    }

    curBreak = clamp(curBreak, 0.f, maxBreak);

    if (!_bossBreakRoot->Is_Open())
        _bossBreakRoot->Open();

    _bossBreakGauge->Set_FillRatio(1.f - (curBreak / maxBreak));
}

void UI_SL_HUD::Apply_BossName(const wstring& name)
{
    if (_bossNameText && _bossNameText->Get_Text() != name)
        _bossNameText->Set_Text(name);

    if (_bossNameBgText && _bossNameBgText->Get_Text() != name)
        _bossNameBgText->Set_Text(name);
}

void UI_SL_HUD::Apply_SkillIcons(const Shared<SL_Maelle>& player)
{
    UNREFERENCED_PARAMETER(player);

    for (uint32 i = 0; i < _skillIcons.size(); ++i)
        Restore_DefaultSkillIcon(i);
}

void UI_SL_HUD::Restore_DefaultSkillIcon(uint32 slotIndex)
{
    if (slotIndex >= _skillIcons.size())
        return;

    if (_skillIcons[slotIndex])
    {
        _skillIcons[slotIndex]->Set_UseFillRatio(false);
        _skillIcons[slotIndex]->Set_FillRatio(1.f);
        _skillIcons[slotIndex]->Open();
    }

    if (_skillIconBgs[slotIndex])
    {
        _skillIconBgs[slotIndex]->Set_UseFillRatio(false);
        _skillIconBgs[slotIndex]->Set_FillRatio(1.f);
        _skillIconBgs[slotIndex]->Open();
    }
}

void UI_SL_HUD::Clear_SkillSlot(uint32 slotIndex)
{
    if (slotIndex >= _skillIcons.size())
        return;

    if (_skillIcons[slotIndex])
        _skillIcons[slotIndex]->Close();

    if (_skillIconBgs[slotIndex])
        _skillIconBgs[slotIndex]->Close();

    Clear_SkillCooldown(slotIndex);
}

void UI_SL_HUD::Apply_SkillCooldowns(const Shared<SL_Maelle>& player)
{
    for (uint32 i = 0; i < _skillCooldownOverlays.size(); ++i)
    {
        const SL_SkillCooldownView cooldownView = player
            ? player->Get_SkillCooldownView(i)
            : SL_SkillCooldownView{};

        if (!cooldownView.Is_OnCooldown())
        {
            Clear_SkillCooldown(i);
            continue;
        }

        if (_skillCooldownOverlays[i])
        {
            _skillCooldownOverlays[i]->Open();
            _skillCooldownOverlays[i]->Set_FillRatio(cooldownView.Get_Ratio());
        }

        if (_skillCooldownTexts[i])
            _skillCooldownTexts[i]->Close();
    }
}

void UI_SL_HUD::Clear_SkillCooldown(uint32 slotIndex)
{
    if (slotIndex >= _skillCooldownOverlays.size())
        return;

    if (_skillCooldownOverlays[slotIndex])
    {
        _skillCooldownOverlays[slotIndex]->Set_FillRatio(0.f);
        _skillCooldownOverlays[slotIndex]->Close();
    }

    if (_skillCooldownTexts[slotIndex])
        _skillCooldownTexts[slotIndex]->Close();
}

Unique<UI_SL_HUD> UI_SL_HUD::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_SL_HUD>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_SL_HUD");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_SL_HUD::Free()
{
    Close();

    _root.reset();
    _uiByName.clear();

    _playerHpGauge.reset();
    _playerCurrentHpText.reset();
    _playerCurrentHpBgText.reset();
    _playerMaxHpText.reset();
    _playerMaxHpBgText.reset();

    for (Shared<UI_Image>& icon : _skillIcons)
        icon.reset();

    for (Shared<UI_Image>& iconBg : _skillIconBgs)
        iconBg.reset();

    for (Shared<UI_Image>& cooldownOverlay : _skillCooldownOverlays)
        cooldownOverlay.reset();

    for (Shared<UI_Text>& cooldownText : _skillCooldownTexts)
        cooldownText.reset();

    _bossHpRoot.reset();
    _bossHpGauge.reset();
    _bossHpGaugeBg.reset();
    _bossBreakRoot.reset();
    _bossBreakGauge.reset();
    _bossNameText.reset();
    _bossNameBgText.reset();

    _staminaRoot.reset();
    _potionRoot.reset();
    _healingPotionCountText.reset();
    _estherSkillRoot.reset();

    Base::Free();
}


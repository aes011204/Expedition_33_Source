#include "UI_BattleVictory.h"

#include "ClientInstance.h"
#include "Client_Constants.h"
#include "UI_Animation.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kBattleVictoryIntroAnimationName[] = L"BattleVictory";
    constexpr wchar_t kBattleVictoryOutroAnimationName[] = L"BattleVictory_Outro";

    float Resolve_BattleVictoryIntroDurationSec()
    {
        UI_KeyframeAnimationAsset introAnimation{};
        introAnimation.fps = 60;
        introAnimation.startFrame = 0;
        introAnimation.endFrame = 15;
        return Get_KeyframeAnimationDurationSec(introAnimation);
    }

    float Resolve_BattleVictoryOutroDurationSec()
    {
        UI_KeyframeAnimationAsset outroAnimation{};
        outroAnimation.fps = 40;
        outroAnimation.startFrame = 0;
        outroAnimation.endFrame = 15;
        return Get_KeyframeAnimationDurationSec(outroAnimation);
    }
}

HRESULT UI_BattleVictory::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    _introDuration = Resolve_BattleVictoryIntroDurationSec();
    _outroDuration = Resolve_BattleVictoryOutroDurationSec();

    Close_Immediately();

    return S_OK;
}

void UI_BattleVictory::Update(float timeDelta)
{
    if (_expAnimDelayRemaining > 0.f)
    {
        _expAnimDelayRemaining = max(0.f, _expAnimDelayRemaining - max(0.f, timeDelta));
    }
    else
    {
        Update_ExpGauge(timeDelta);
    }

    if (!_isClosingWithOutro)
        return;

    _closeElapsed += max(0.f, timeDelta);

    if (_closeElapsed >= _outroDuration)
        Close_Immediately();
}

void UI_BattleVictory::Open()
{
    if (_root == nullptr)
        return;

    if (_isOpen && !_isClosingWithOutro)
        return;

    _isClosingWithOutro = false;
    _closeElapsed = 0.f;
    _expAnimDelayRemaining = _introDuration;

    UI_RuntimePanelBase::Open();
    Play_TargetAnimation(kBattleVictoryIntroAnimationName);
}

void UI_BattleVictory::Close()
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

    Play_TargetAnimation(kBattleVictoryOutroAnimationName);

    if (_outroDuration <= 0.f)
        Close_Immediately();
}

void UI_BattleVictory::Close_Immediately()
{
    _expAnimDelayRemaining = 0.f;
    _isClosingWithOutro = false;
    _closeElapsed = 0.f;

    Stop_TargetAnimations();

    UI_RuntimePanelBase::Close();
}

void UI_BattleVictory::Set_PlayerStatus(const vector<VictoryPlayerStatDesc>& stats)
{
    for (const auto& stat : stats)
    {
        Apply_PlayerStat(stat);
    }
}

void UI_BattleVictory::Set_RewardExp(uint32 rewardExp)
{
    Set_UIntText(_rewardExpText, rewardExp);
    Set_UIntText(_rewardExpBgText, rewardExp);
}

HRESULT UI_BattleVictory::Bind_UI()
{
    constexpr const wchar_t* names[3] =
    {
        L"Gustave",
        L"Lune",
        L"Maelle",
    };

    for (uint32 i = 0; i < _playerSlots.size(); ++i)
    {
        const wstring prefix = names[i];

        PlayerSlot& slot = _playerSlots[i];
        slot.curExpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + prefix + L"_Current_EXP");
        slot.maxExpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + prefix + L"_Max_EXP");
        slot.levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + prefix + L"_Level_Value");
        slot.expGauge = Find_TypedUI<UI_Image>(_uiByName, L"Text_" + prefix + L"_EXP_Gauge");

        CHECK_NULL(slot.curExpText, E_FAIL);
        CHECK_NULL(slot.maxExpText, E_FAIL);
        CHECK_NULL(slot.levelText, E_FAIL);
        CHECK_NULL(slot.expGauge, E_FAIL);

        slot.expGauge->Set_UseFillRatio(true);
        slot.expGauge->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);
    }

    _rewardExpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_EXP_Value");
    _rewardExpBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_EXP_Value_BG");

    CHECK_NULL(_rewardExpText, E_FAIL);
    CHECK_NULL(_rewardExpBgText, E_FAIL);

    return S_OK;
}

void UI_BattleVictory::Play_TargetAnimation(const wchar_t* animationName)
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Play_UIAnimation(animationName, true);
    }
}

void UI_BattleVictory::Stop_TargetAnimations()
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Stop_UIAnimation();
    }
}

void UI_BattleVictory::Apply_PlayerStat(const VictoryPlayerStatDesc& stat)
{
    const uint32 slotIndex = Get_SlotIndex(stat.playerType);
    if (slotIndex >= _playerSlots.size())
        return;

    PlayerSlot& slot = _playerSlots[slotIndex];

    slot.startLevel = stat.prevLevel;
    slot.targetLevel = stat.level;

    slot.startExp = static_cast<float>(stat.prevExp);
    slot.displayExp = slot.startExp;
    slot.targetExp = static_cast<float>(stat.curExp);
    slot.startMaxExp = static_cast<float>(max(stat.prevMaxExp, 1u));
    slot.targetMaxExp = static_cast<float>(max(stat.maxExp, 1u));
    slot.maxExp = slot.startMaxExp;

    slot.animElapsed = 0.f;
    slot.animDuration = 2.4f;
    slot.isAnimating =
        (slot.startLevel != slot.targetLevel) ||
        (slot.startExp != slot.targetExp) ||
        (slot.startMaxExp != slot.targetMaxExp);

    Set_UIntText(slot.curExpText, stat.prevExp);
    Set_UIntText(slot.maxExpText, stat.prevMaxExp);
    Set_UIntText(slot.levelText, stat.prevLevel);

    if (slot.expGauge)
    {
        const float startRatio = slot.startExp / max(slot.startMaxExp, 1.f);
        slot.expGauge->Set_FillRatio(clamp(startRatio, 0.f, 1.f));
    }
}

uint32 UI_BattleVictory::Get_SlotIndex(PlayerCharacterType playerType)
{
    switch (playerType)
    {
    case PlayerCharacterType::Gustave:
        return 0;
    case PlayerCharacterType::Lune:
        return 1;
    case PlayerCharacterType::Maelle:
        return 2;
    default:
        return UINT_MAX;
    }
}

void UI_BattleVictory::Set_UIntText(const Shared<UI_Text>& text, uint32 value)
{
    if (text)
        text->Set_Text(to_wstring(value));
}

void UI_BattleVictory::Update_ExpGauge(float timeDelta)
{
    for (PlayerSlot& slot : _playerSlots)
    {
        if (!slot.isAnimating)
            continue;

        slot.animElapsed += timeDelta;

        const float t = clamp(slot.animElapsed / slot.animDuration, 0.f, 1.f);
        const float smoothT = t * t * (3.f - 2.f * t);

        uint32 displayLevel = slot.startLevel;
        float displayMaxExp = max(slot.startMaxExp, 1.f);
        float displayExp = slot.startExp;

        if (slot.startLevel == slot.targetLevel)
        {
            displayExp = std::lerp(slot.startExp, slot.targetExp, smoothT);
            displayMaxExp = max(slot.targetMaxExp, 1.f);
        }
        else
        {
            float totalExpToGain = max(slot.startMaxExp - slot.startExp, 0.f);

            for (uint32 level = slot.startLevel + 1; level < slot.targetLevel; ++level)
                totalExpToGain += static_cast<float>(max(1u, CLIENT->Get_NextExp(level)));

            totalExpToGain += slot.targetExp;

            const float progressedExp = totalExpToGain * smoothT;
            displayExp = slot.startExp + progressedExp;

            while (displayLevel < slot.targetLevel && displayExp >= displayMaxExp)
            {
                displayExp -= displayMaxExp;
                ++displayLevel;

                if (displayLevel >= slot.targetLevel)
                    displayMaxExp = max(slot.targetMaxExp, 1.f);
                else
                    displayMaxExp = static_cast<float>(max(1u, CLIENT->Get_NextExp(displayLevel)));
            }
        }

        slot.displayExp = displayExp;
        slot.maxExp = displayMaxExp;

        Set_UIntText(slot.curExpText, static_cast<uint32>(slot.displayExp));
        Set_UIntText(slot.maxExpText, static_cast<uint32>(slot.maxExp));
        Set_UIntText(slot.levelText, displayLevel);

        if (slot.expGauge)
            slot.expGauge->Set_FillRatio(clamp(slot.displayExp / max(slot.maxExp, 1.f), 0.f, 1.f));

        if (t >= 1.f)
        {
            slot.isAnimating = false;
            slot.displayExp = slot.targetExp;
            slot.maxExp = slot.targetMaxExp;

            Set_UIntText(slot.curExpText, static_cast<uint32>(slot.targetExp));
            Set_UIntText(slot.levelText, slot.targetLevel);
            Set_UIntText(slot.maxExpText, static_cast<uint32>(slot.targetMaxExp));

            if (slot.expGauge)
                slot.expGauge->Set_FillRatio(clamp(slot.targetExp / max(slot.targetMaxExp, 1.f), 0.f, 1.f));
        }
    }
}

HRESULT UI_BattleVictory::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Battle_Victory_Path;
    desc.animationPaths =
    {
        Constants::UI_Anim::UI_BattleVictoryIntroAnim_Path,
        Constants::UI_Anim::UI_BattleVictoryOutroAnim_Path,
    };
    desc.prototypeTagPrefix = L"BattleVictory";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleVictory");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

Unique<UI_BattleVictory> UI_BattleVictory::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleVictory>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleVictory");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleVictory::Free()
{
    Close_Immediately();

    _root.reset();
    _uiByName.clear();

    for (PlayerSlot& slot : _playerSlots)
    {
        slot.curExpText.reset();
        slot.maxExpText.reset();
        slot.levelText.reset();
        slot.expGauge.reset();
    }

    _rewardExpText.reset();
    _rewardExpBgText.reset();

    Base::Free();
}

#include "UI_BattleVictory.h"

#include "ClientInstance.h"
#include "Client_Constants.h"
#include "GameInstance.h"
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
    constexpr wchar_t kBattleVictoryRuntimeMonsterPortraitTag[] = L"BattleVictory_Runtime_DefeatedMonsterPortrait";
    constexpr wchar_t kBattleVictoryPortraitMaskPath[] = L"../Bin/Resources/Textures/UI/Battle/HUD/Frames/T_UI_SquareIconMask.png";
    constexpr float kBattleVictoryMonsterPortraitSpacing = 86.f;

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
        Update_RewardNumberTweens(timeDelta);
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

    if (_rewardExpTween.isAnimating)
    {
        _rewardExpTween.elapsed = 0.f;
        Set_UIntText(_rewardExpTween.text, _rewardExpTween.startValue);
        Set_UIntText(_rewardExpTween.bgText, _rewardExpTween.startValue);
    }

    if (_rewardChromaTween.isAnimating)
    {
        _rewardChromaTween.elapsed = 0.f;
        Set_UIntText(_rewardChromaTween.text, _rewardChromaTween.startValue);
        Set_UIntText(_rewardChromaTween.bgText, _rewardChromaTween.startValue);
    }

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
    _rewardExpTween.isAnimating = false;
    _rewardExpTween.elapsed = 0.f;
    _rewardChromaTween.isAnimating = false;
    _rewardChromaTween.elapsed = 0.f;

    Stop_TargetAnimations();

    UI_RuntimePanelBase::Close();

    for (const Shared<UI_Image>& portrait : _defeatedMonsterPortraitSlots)
    {
        if (portrait)
            portrait->Close();
    }
}

void UI_BattleVictory::Set_PlayerStatus(const vector<VictoryPlayerStatDesc>& stats)
{
    for (PlayerSlot& slot : _playerSlots)
    {
        if (slot.root)
            slot.root->Close();

        slot.isAnimating = false;
    }

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

void UI_BattleVictory::Set_VictorySummary(const VictorySummaryDesc& summary)
{
    Apply_VictorySummary(summary);
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
        slot.root = Find_UI(_uiByName, L"Panel_BattleVictory_Frame_" + prefix);
        slot.portrait = Find_TypedUI<UI_Image>(_uiByName, L"Image_BattleVictory_Frame_" + prefix + L"_Portrait");
        slot.nameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleVictory_Frame_" + prefix + L"_Name");
        slot.curExpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + prefix + L"_Current_EXP");
        slot.maxExpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + prefix + L"_Max_EXP");
        slot.levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_" + prefix + L"_Level_Value");
        slot.expGauge = Find_TypedUI<UI_Image>(_uiByName, L"Text_" + prefix + L"_EXP_Gauge");

        CHECK_NULL(slot.root, E_FAIL);
        CHECK_NULL(slot.portrait, E_FAIL);
        CHECK_NULL(slot.nameText, E_FAIL);
        CHECK_NULL(slot.curExpText, E_FAIL);
        CHECK_NULL(slot.maxExpText, E_FAIL);
        CHECK_NULL(slot.levelText, E_FAIL);
        CHECK_NULL(slot.expGauge, E_FAIL);

        slot.expGauge->Set_UseFillRatio(true);
        slot.expGauge->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);
    }

    _rewardExpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_EXP_Value");
    _rewardExpBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_EXP_Value_BG");
    _rewardChromaText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleReward_Money");
    _rewardChromaBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleReward_Money_BG");
    _maxDamageText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Max_Damage_Value");
    _totalDamageText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Total_Damage_Value");
    _takenDamageText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Taken_Damage_Value");
    _clearTimeText = Find_TypedUI<UI_Text>(_uiByName, L"Text_ClearTime_Value");
    _parrySuccessText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Parry_Success_Value");
    _dodgeSuccessText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Dodge_Success_Value");
    _defeatedMonsterPortraitPanel = Find_UI(_uiByName, L"Panel_DefeatedMonster_Portrait");
    _defeatedMonsterPortraitTemplate = Find_TypedUI<UI_Image>(_uiByName, L"Image_DefeatedMonster_Portrait");

    CHECK_NULL(_rewardExpText, E_FAIL);
    CHECK_NULL(_rewardExpBgText, E_FAIL);
    CHECK_NULL(_rewardChromaText, E_FAIL);
    CHECK_NULL(_rewardChromaBgText, E_FAIL);
    CHECK_NULL(_maxDamageText, E_FAIL);
    CHECK_NULL(_totalDamageText, E_FAIL);
    CHECK_NULL(_takenDamageText, E_FAIL);
    CHECK_NULL(_clearTimeText, E_FAIL);
    CHECK_NULL(_parrySuccessText, E_FAIL);
    CHECK_NULL(_dodgeSuccessText, E_FAIL);
    CHECK_NULL(_defeatedMonsterPortraitPanel, E_FAIL);
    CHECK_NULL(_defeatedMonsterPortraitTemplate, E_FAIL);

    _defeatedMonsterPortraitSlots.clear();
    _defeatedMonsterPortraitSlots.push_back(_defeatedMonsterPortraitTemplate);
    if (UI_Transform* transform = _defeatedMonsterPortraitTemplate->Get_Transform())
        _defeatedMonsterPortraitBasePosition = transform->Get_Position();

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
    const uint32 slotIndex =
        stat.visualSlotIndex < _playerSlots.size()
        ? stat.visualSlotIndex
        : Get_SlotIndex(stat.playerType);

    if (slotIndex >= _playerSlots.size())
        return;

    PlayerSlot& slot = _playerSlots[slotIndex];

    if (slot.root)
        slot.root->Open();

    if (slot.portrait && !stat.portraitGuid.empty())
        slot.portrait->Set_TextureGuid(stat.portraitGuid);

    if (slot.nameText && !stat.displayName.empty())
        slot.nameText->Set_Text(stat.displayName);

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

void UI_BattleVictory::Apply_VictorySummary(const VictorySummaryDesc& summary)
{
    Start_RewardNumberTween(_rewardExpTween, _rewardExpText, _rewardExpBgText, summary.rewardExp, 0.8f);
    Start_RewardNumberTween(_rewardChromaTween, _rewardChromaText, _rewardChromaBgText, summary.rewardChroma, 0.8f);
    Set_UIntText(_maxDamageText, summary.maxDamage);
    Set_UIntText(_totalDamageText, summary.totalDamageDealt);
    Set_UIntText(_takenDamageText, summary.totalDamageTaken);
    Set_UIntText(_parrySuccessText, summary.parrySuccessCount);
    Set_UIntText(_dodgeSuccessText, summary.dodgeSuccessCount);
    Set_WStringText(_clearTimeText, Format_ClearTime(summary.elapsedSec));
    Apply_DefeatedMonsterPortraits(summary.defeatedMonsterPortraitGuids);
}

void UI_BattleVictory::Start_RewardNumberTween(RewardNumberTween& tween, const Shared<UI_Text>& text, const Shared<UI_Text>& bgText, uint32 targetValue, float duration)
{
    tween.text = text;
    tween.bgText = bgText;
    tween.startValue = 0;
    tween.targetValue = targetValue;
    tween.elapsed = 0.f;
    tween.duration = max(duration, 0.01f);
    tween.isAnimating = targetValue > 0;

    Set_UIntText(tween.text, tween.startValue);
    Set_UIntText(tween.bgText, tween.startValue);

    if (!tween.isAnimating)
    {
        Set_UIntText(tween.text, tween.targetValue);
        Set_UIntText(tween.bgText, tween.targetValue);
    }
}

void UI_BattleVictory::Update_RewardNumberTweens(float timeDelta)
{
    Update_RewardNumberTween(_rewardExpTween, timeDelta);
    Update_RewardNumberTween(_rewardChromaTween, timeDelta);
}

void UI_BattleVictory::Update_RewardNumberTween(RewardNumberTween& tween, float timeDelta)
{
    if (!tween.isAnimating)
        return;

    tween.elapsed += max(0.f, timeDelta);

    const float t = clamp(tween.elapsed / tween.duration, 0.f, 1.f);
    const float smoothT = t * t * (3.f - 2.f * t);
    const uint32 displayValue = static_cast<uint32>(roundf(std::lerp(
        static_cast<float>(tween.startValue),
        static_cast<float>(tween.targetValue),
        smoothT)));

    Set_UIntText(tween.text, displayValue);
    Set_UIntText(tween.bgText, displayValue);

    if (t >= 1.f)
    {
        tween.isAnimating = false;
        Set_UIntText(tween.text, tween.targetValue);
        Set_UIntText(tween.bgText, tween.targetValue);
    }
}

void UI_BattleVictory::Apply_DefeatedMonsterPortraits(const vector<string>& portraitGuids)
{
    for (uint32 i = 0; i < portraitGuids.size(); ++i)
    {
        if (!Ensure_DefeatedMonsterPortraitSlot(i))
            break;

        Shared<UI_Image> portrait = _defeatedMonsterPortraitSlots[i];
        if (portrait == nullptr)
            continue;

        if (UI_Transform* transform = portrait->Get_Transform())
        {
            const float xOffset = static_cast<float>(i) * kBattleVictoryMonsterPortraitSpacing;
            transform->Set_Position(Vec2(_defeatedMonsterPortraitBasePosition.x - xOffset, _defeatedMonsterPortraitBasePosition.y));
        }

        portrait->Set_TextureGuid(portraitGuids[i]);
        portrait->Open();
    }

    for (uint32 i = static_cast<uint32>(portraitGuids.size()); i < _defeatedMonsterPortraitSlots.size(); ++i)
    {
        if (_defeatedMonsterPortraitSlots[i])
            _defeatedMonsterPortraitSlots[i]->Close();
    }
}

bool UI_BattleVictory::Ensure_DefeatedMonsterPortraitSlot(uint32 slotIndex)
{
    if (_defeatedMonsterPortraitTemplate == nullptr || _defeatedMonsterPortraitPanel == nullptr)
        return false;

    while (_defeatedMonsterPortraitSlots.size() <= slotIndex)
    {
        if (!GAME->Has_Prototype(_levelIndex, kBattleVictoryRuntimeMonsterPortraitTag))
        {
            CHECK_FAILED(GAME->Add_Prototype(_levelIndex, kBattleVictoryRuntimeMonsterPortraitTag, UI_Image::Create()), false);
        }

        UI_Image::UI_IMAGE_DESC desc{};

        if (UI_Transform* templateTransform = _defeatedMonsterPortraitTemplate->Get_Transform())
        {
            desc.transformDesc.position = templateTransform->Get_Position();
            desc.transformDesc.size = templateTransform->Get_Size();
            desc.transformDesc.scale = templateTransform->Get_Scale();
            desc.transformDesc.pivot = templateTransform->Get_Pivot();
            desc.transformDesc.anchorMin = templateTransform->Get_AnchorMin();
            desc.transformDesc.anchorMax = templateTransform->Get_AnchorMax();
            desc.transformDesc.rotationRadians = templateTransform->Get_RotationRadians();
        }

        desc.isOpenOnCreate = false;
        desc.sortOrder = _defeatedMonsterPortraitTemplate->Get_SortOrder();
        desc.renderGroup = _defeatedMonsterPortraitTemplate->Get_RenderGroup();
        desc.color = _defeatedMonsterPortraitTemplate->Get_Color();
        desc.maskMode = UI_Image::MaskMode::TextureMask;
        desc.maskTexturePath = kBattleVictoryPortraitMaskPath;

        Shared<UI_Base> createdBase{};
        CHECK_FAILED(GAME->Add_UI(_levelIndex, kBattleVictoryRuntimeMonsterPortraitTag, &desc, &createdBase), false);

        Shared<UI_Image> createdImage = dynamic_pointer_cast<UI_Image>(createdBase);
        CHECK_NULL(createdImage, false);

        createdImage->Set_Parent(_defeatedMonsterPortraitPanel);
        _defeatedMonsterPortraitSlots.push_back(createdImage);
    }

    return true;
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

void UI_BattleVictory::Set_WStringText(const Shared<UI_Text>& text, const wstring& value)
{
    if (text)
        text->Set_Text(value);
}

wstring UI_BattleVictory::Format_ClearTime(float elapsedSec)
{
    const uint32 totalSec = static_cast<uint32>(max(0.f, elapsedSec));
    const uint32 minutes = totalSec / 60u;
    const uint32 seconds = totalSec % 60u;

    wchar_t buffer[16]{};
    swprintf_s(buffer, L"%02u:%02u", minutes, seconds);
    return buffer;
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

    for (size_t i = 1; i < _defeatedMonsterPortraitSlots.size(); ++i)
    {
        Shared<UI_Base> portrait = static_pointer_cast<UI_Base>(_defeatedMonsterPortraitSlots[i]);
        UI_RuntimeLoader::Release_UI(portrait);
    }

    UI_RuntimeLoader::Release_Document(_uiByName);
    _root.reset();

    for (PlayerSlot& slot : _playerSlots)
    {
        slot.root.reset();
        slot.portrait.reset();
        slot.nameText.reset();
        slot.curExpText.reset();
        slot.maxExpText.reset();
        slot.levelText.reset();
        slot.expGauge.reset();
    }

    _rewardExpText.reset();
    _rewardExpBgText.reset();
    _rewardChromaText.reset();
    _rewardChromaBgText.reset();
    _maxDamageText.reset();
    _totalDamageText.reset();
    _takenDamageText.reset();
    _clearTimeText.reset();
    _parrySuccessText.reset();
    _dodgeSuccessText.reset();
    _defeatedMonsterPortraitPanel.reset();
    _defeatedMonsterPortraitTemplate.reset();
    _defeatedMonsterPortraitSlots.clear();
    _defeatedMonsterPortraitBasePosition = Vec2::Zero;

    Base::Free();
}

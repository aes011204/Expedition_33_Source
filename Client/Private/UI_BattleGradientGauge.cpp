#include "UI_BattleGradientGauge.h"

#include "Client_Constants.h"
#include "CombatStatCom.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_BattleGradientGauge::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Set_Stack(0, 0);
    Close();

    return S_OK;
}

void UI_BattleGradientGauge::Update(float timeDelta)
{
    const float lerpFactor = min(timeDelta * 10.f, 1.f);

    for (uint32 i = 0; i < _stackImages.size(); ++i)
    {
        if (_stackImages[i] == nullptr)
            continue;

        float& visualFillRatio = _visualFillRatios[i];
        const float targetFillRatio = _targetFillRatios[i];

        visualFillRatio += (targetFillRatio - visualFillRatio) * lerpFactor;

        if (fabsf(targetFillRatio - visualFillRatio) < 0.005f)
            visualFillRatio = targetFillRatio;

        if (visualFillRatio > 0.f || targetFillRatio > 0.f)
        {
            _stackImages[i]->Open();
            _stackImages[i]->Set_AlphaMultiplier(1.f);
            _stackImages[i]->Set_FillRatio(visualFillRatio);
        }
        else
        {
            _stackImages[i]->Set_FillRatio(0.f);
            _stackImages[i]->Close();
        }
    }
}

void UI_BattleGradientGauge::Update_ForActor(const BattleChar& actor)
{
    if (actor.statCom == nullptr)
    {
        Close();
        return;
    }

    string gradientSkillKey{};
    if (!actor.statCom->Try_GetSkillKeyBySlot(GradientSkill::SlotIndex, gradientSkillKey))
    {
        Close();
        return;
    }

    Open(); // Keep the unlocked Gradient gauge visible even at 0 stacks.
    Set_Stack(
        actor.statCom->Get_GradientGaugeStack(),
        actor.statCom->Get_GradientGaugeProgressTick());
}

HRESULT UI_BattleGradientGauge::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_BattleSystem_Gradient_Gauge_Path;
    desc.prototypeTagPrefix = L"BattleGradientGauge";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem_Gradient_Gauge");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_BattleGradientGauge::Bind_UI()
{
    _stackImages[0] = Find_TypedUI<UI_Image>(_uiByName, L"Image_BattleSystem_Gradient_Gauge_1");
    _stackImages[1] = Find_TypedUI<UI_Image>(_uiByName, L"Image_BattleSystem_Gradient_Gauge_2");
    _stackImages[2] = Find_TypedUI<UI_Image>(_uiByName, L"Image_BattleSystem_Gradient_Gauge_3");
    _currentCountText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_Gradient_Gauge_Current_Count");
    _currentCountBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_Gradient_Gauge_Current_Count_BG");
    _maxCountText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_Gradient_Gauge_Max_Count");
    _maxCountBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_Gradient_Gauge_Max_Count_BG");

    for (const auto& stackImage : _stackImages)
        CHECK_NULL(stackImage, E_FAIL);

    for (const auto& stackImage : _stackImages)
    {
        stackImage->Set_UseFillRatio(true);
        stackImage->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);
        stackImage->Set_AlphaMultiplier(1.f);
        stackImage->Set_FillRatio(0.f);
    }

    _visualFillRatios.fill(0.f);
    _targetFillRatios.fill(0.f);

    CHECK_NULL(_currentCountText, E_FAIL);
    CHECK_NULL(_currentCountBgText, E_FAIL);
    CHECK_NULL(_maxCountText, E_FAIL);
    CHECK_NULL(_maxCountBgText, E_FAIL);

    Set_Text(_maxCountText, to_wstring(GradientSkill::MaxGaugeStacks));
    Set_Text(_maxCountBgText, to_wstring(GradientSkill::MaxGaugeStacks));

    return S_OK;
}

void UI_BattleGradientGauge::Set_Stack(uint32 stack, uint32 progressTick)
{
    const uint32 clampedStack = min(stack, GradientSkill::MaxGaugeStacks);
    const uint32 clampedProgressTick = min(progressTick, GradientSkill::GaugeProgressPerStack - 1);
    const uint32 totalProgress =
        min(
            clampedStack * GradientSkill::GaugeProgressPerStack + clampedProgressTick,
            GradientSkill::MaxGaugeStacks * GradientSkill::GaugeProgressPerStack);
    const float totalFillRatio =
        static_cast<float>(totalProgress) /
        static_cast<float>(GradientSkill::MaxGaugeStacks * GradientSkill::GaugeProgressPerStack);

    for (uint32 i = 0; i < _stackImages.size(); ++i)
    {
        if (_stackImages[i] == nullptr)
            continue;

        const float fillRatio = i == 0 ? totalFillRatio : 0.f;

        _targetFillRatios[i] = fillRatio;

        if (fillRatio > 0.f || _visualFillRatios[i] > 0.f)
        {
            _stackImages[i]->Open();
            _stackImages[i]->Set_AlphaMultiplier(1.f);
        }
        else
        {
            _stackImages[i]->Set_FillRatio(0.f);
            _stackImages[i]->Close();
        }
    }

    Set_Text(_currentCountText, to_wstring(clampedStack));
    Set_Text(_currentCountBgText, to_wstring(clampedStack));
}

void UI_BattleGradientGauge::Set_Text(const Shared<UI_Text>& text, const wstring& value)
{
    if (text)
        text->Set_Text(value);
}

Unique<UI_BattleGradientGauge> UI_BattleGradientGauge::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleGradientGauge>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        MSG_BOX("Failed to Created : UI_BattleGradientGauge");
        return nullptr;
    }

    return instance;
}

void UI_BattleGradientGauge::Free()
{
    Close();

    _root.reset();
    _uiByName.clear();

    for (auto& stackImage : _stackImages)
        stackImage.reset();

    _visualFillRatios.fill(0.f);
    _targetFillRatios.fill(0.f);

    _currentCountText.reset();
    _currentCountBgText.reset();
    _maxCountText.reset();
    _maxCountBgText.reset();
}

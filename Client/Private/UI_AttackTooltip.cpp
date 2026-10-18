#include "UI_AttackTooltip.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kTooltipIntroAnimationName[] = L"BattleSystem_Tooltip_Intro";
    constexpr wchar_t kTooltipOutroAnimationName[] = L"BattleSystem_Tooltip_Outro";
}

HRESULT UI_AttackTooltip::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Load_TooltipAnimations(), E_FAIL);

    Close_Immediately();

    return S_OK;
}

void UI_AttackTooltip::Update(float timeDelta)
{
    if (!_isClosingWithOutro)
        return;

    _closeElapsed += max(0.f, timeDelta);

    if (_closeElapsed >= _outroDuration)
        Finish_Close();
}

void UI_AttackTooltip::Open()
{
    if (_root == nullptr)
        return;

    if (_isOpen && !_isClosingWithOutro)
        return;

    _isClosingWithOutro = false;
    _closeElapsed = 0.f;

    UI_RuntimePanelBase::Open();

    if (_hasTooltipIntroAnimation)
        _root->Play_UIAnimation(kTooltipIntroAnimationName, true);
}

void UI_AttackTooltip::Close()
{
    if (_root == nullptr)
    {
        Finish_Close();
        return;
    }

    if (!_isOpen && !_isClosingWithOutro)
        return;

    if (_isClosingWithOutro)
        return;

    if (!_hasTooltipOutroAnimation)
    {
        Close_Immediately();
        return;
    }

    _isClosingWithOutro = true;
    _closeElapsed = 0.f;

    if (!_root->Play_UIAnimation(kTooltipOutroAnimationName, true))
        Close_Immediately();
}

void UI_AttackTooltip::Close_Immediately()
{
    _isClosingWithOutro = false;
    _closeElapsed = 0.f;

    if (_root)
        _root->Stop_UIAnimation();

    UI_RuntimePanelBase::Close();
}

HRESULT UI_AttackTooltip::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Attack_Tooltip_Path;
    desc.prototypeTagPrefix = L"AttackTooltip";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem_AttackTooltip");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_AttackTooltip::Load_TooltipAnimations()
{
    _hasTooltipIntroAnimation =
        UI_RuntimeLoader::Load_PublicAnimationAsset(
            Constants::UI_Anim::UI_BattleTooltipIntroAnim_Path,
            _tooltipIntroAnimation);
    CHECK_TRUE(_hasTooltipIntroAnimation, E_FAIL);

    _hasTooltipOutroAnimation =
        UI_RuntimeLoader::Load_PublicAnimationAsset(
            Constants::UI_Anim::UI_BattleTooltipOutroAnim_Path,
            _tooltipOutroAnimation);
    CHECK_TRUE(_hasTooltipOutroAnimation, E_FAIL);

    _outroDuration = Get_KeyframeAnimationDurationSec(_tooltipOutroAnimation);

    _root->Add_UIAnimationAsset(_tooltipIntroAnimation);
    _root->Add_UIAnimationAsset(_tooltipOutroAnimation);

    return S_OK;
}

void UI_AttackTooltip::Finish_Close()
{
    Close_Immediately();
}

Unique<UI_AttackTooltip> UI_AttackTooltip::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_AttackTooltip>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_AttackTooltip");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_AttackTooltip::Free()
{
    Close_Immediately();

    _root.reset();
    _uiByName.clear();

    Base::Free();
}

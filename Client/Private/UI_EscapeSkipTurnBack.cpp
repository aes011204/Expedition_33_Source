#include "UI_EscapeSkipTurnBack.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kControlHintIntroAnimationName[] = L"Control_Hint_Intro";
    constexpr wchar_t kControlHintOutroAnimationName[] = L"Control_Hint_Outro";
}

HRESULT UI_EscapeSkipTurnBack::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);
    CHECK_FAILED(Load_Animations(), E_FAIL);

    Close_Immediately();
    return S_OK;
}

void UI_EscapeSkipTurnBack::Update(float timeDelta)
{
    if (!_isClosingWithOutro)
        return;

    _closeElapsed += max(0.f, timeDelta);

    if (_closeElapsed >= _outroDuration)
        Close_Immediately();
}

void UI_EscapeSkipTurnBack::Open()
{
    if (_root == nullptr)
        return;

    if (_isOpen && !_isClosingWithOutro)
        return;

    _isClosingWithOutro = false;
    _closeElapsed = 0.f;

    UI_RuntimePanelBase::Open();

    if (_hasIntroAnimation)
        _root->Play_UIAnimation(kControlHintIntroAnimationName, true);
}

void UI_EscapeSkipTurnBack::Close()
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

    if (!_hasOutroAnimation)
    {
        Close_Immediately();
        return;
    }

    _isClosingWithOutro = true;
    _closeElapsed = 0.f;

    if (!_root->Play_UIAnimation(kControlHintOutroAnimationName, true))
        Close_Immediately();
}

void UI_EscapeSkipTurnBack::Close_Immediately()
{
    _isClosingWithOutro = false;
    _closeElapsed = 0.f;

    if (_root)
        _root->Stop_UIAnimation();

    UI_RuntimePanelBase::Close();
}

HRESULT UI_EscapeSkipTurnBack::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Escape_SkipTurnBack_Path;
    desc.prototypeTagPrefix = L"EscapeSkipTurn";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem_Escape_SkipTurn_Back");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_EscapeSkipTurnBack::Bind_UI()
{
    _keyC = Find_UI(_uiByName, L"Panel_Key_C");
    _keyTab = Find_UI(_uiByName, L"Panel_Key_Tab");

    CHECK_NULL(_keyC, E_FAIL);
    CHECK_NULL(_keyTab, E_FAIL);

    return S_OK;
}

HRESULT UI_EscapeSkipTurnBack::Load_Animations()
{
    _hasIntroAnimation =
        UI_RuntimeLoader::Load_PublicAnimationAsset(
            Constants::UI_Anim::UI_ControlHintIntroAnim_Path,
            _introAnimation);
    CHECK_TRUE(_hasIntroAnimation, E_FAIL);

    _hasOutroAnimation =
        UI_RuntimeLoader::Load_PublicAnimationAsset(
            Constants::UI_Anim::UI_ControlHintOutroAnim_Path,
            _outroAnimation);
    CHECK_TRUE(_hasOutroAnimation, E_FAIL);

    _outroDuration = Get_KeyframeAnimationDurationSec(_outroAnimation);

    _root->Add_UIAnimationAsset(_introAnimation);
    _root->Add_UIAnimationAsset(_outroAnimation);

    return S_OK;
}

Unique<UI_EscapeSkipTurnBack> UI_EscapeSkipTurnBack::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_EscapeSkipTurnBack>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_EscapeSkipTurnBack");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_EscapeSkipTurnBack::Free()
{
    Close_Immediately();

    _root.reset();
    _uiByName.clear();

    Base::Free();
}

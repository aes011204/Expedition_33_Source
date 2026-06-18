#include "UI_Button.h"

#include <cmath>

#include "GameInstance.h"

UI_Button::UI_Button()
{
}

UI_Button::UI_Button(const UI_Button& prototype)
    : UI_Panel{ prototype }
{
}

HRESULT UI_Button::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT UI_Button::Initialize(void* arg)
{
    UI_BUTTON_DESC desc{};
    if (nullptr != arg)
        desc = *static_cast<UI_BUTTON_DESC*>(arg);

    _stateAnimationProfile = desc.stateAnimationProfile;
    _isFocusedForState = desc.isFocused;
    _isDisabled = desc.isDisabled;
    _previousFocusedForState = _isFocusedForState;
    _previousDisabledForState = _isDisabled;

    return __super::Initialize(arg);
}

void UI_Button::Update(float timeDelta)
{
    __super::Update(timeDelta);

    _wasClicked = false;

    if (!Is_EffectivelyOpen())
    {
        _isHovered = false;
        _isPressed = false;
        _previousHoveredForState = _isHovered;
        _previousPressedForState = _isPressed;
        _previousFocusedForState = _isFocusedForState;
        _previousDisabledForState = _isDisabled;
        return;
    }

    if (_isDisabled)
    {
        _isHovered = false;
        _isPressed = false;
    }
    else
    {
        _isHovered = Hit_TestMouse();

        if (_isHovered && GAME->KeyDown(KEY_TYPE::LBUTTON))
            _isPressed = true;

        if (_isPressed && GAME->KeyUp(KEY_TYPE::LBUTTON))
        {
            _wasClicked = _isHovered;
            _isPressed = false;
        }

        if (!GAME->KeyDown(KEY_TYPE::LBUTTON) && !GAME->KeyPress(KEY_TYPE::LBUTTON))
            _isPressed = false;
    }

    Update_StateAnimationEvents();
}

Vec4 UI_Button::Resolve_RenderColor() const
{
    if (_isPressed && _isHovered)
        return Get_EffectiveRenderColor(Scale_RGB(_color, 0.78f));

    if (_isHovered)
        return Get_EffectiveRenderColor(Scale_RGB(_color, 1.18f));

    return Get_EffectiveRenderColor(_color);
}

void UI_Button::Set_Focused(bool isFocused)
{
    _isFocusedForState = isFocused;
}

void UI_Button::Set_Disabled(bool isDisabled)
{
    _isDisabled = isDisabled;
    if (_isDisabled)
    {
        _isHovered = false;
        _isPressed = false;
        _wasClicked = false;
    }
}

void UI_Button::Set_StateAnimationProfile(const UI_StateAnimationProfile& stateAnimationProfile)
{
    _stateAnimationProfile = stateAnimationProfile;
}

void UI_Button::Update_StateAnimationEvents()
{
    if (_previousDisabledForState != _isDisabled)
        Try_PlayStateAnimation(_isDisabled ? UI_StateAnimationEvent::DisabledEnter : UI_StateAnimationEvent::DisabledExit);

    if (_previousPressedForState != _isPressed)
        Try_PlayStateAnimation(_isPressed ? UI_StateAnimationEvent::PressedEnter : UI_StateAnimationEvent::PressedExit);

    if (_previousHoveredForState != _isHovered)
        Try_PlayStateAnimation(_isHovered ? UI_StateAnimationEvent::HoverEnter : UI_StateAnimationEvent::HoverExit);

    if (_previousFocusedForState != _isFocusedForState)
        Try_PlayStateAnimation(_isFocusedForState ? UI_StateAnimationEvent::FocusedEnter : UI_StateAnimationEvent::FocusedExit);

    _previousHoveredForState = _isHovered;
    _previousPressedForState = _isPressed;
    _previousFocusedForState = _isFocusedForState;
    _previousDisabledForState = _isDisabled;
}

bool UI_Button::Try_PlayStateAnimation(UI_StateAnimationEvent eventType)
{
    const size_t eventIndex = static_cast<size_t>(eventType);
    if (eventIndex >= _stateAnimationProfile.eventAnimationNames.size())
        return false;

    bool played = false;
    const wstring& animationName = _stateAnimationProfile.eventAnimationNames[eventIndex];
    if (!animationName.empty())
        played = Play_UIAnimation(animationName, true);

    const vector<UI_StateAnimationTargetAction>& targetActions = _stateAnimationProfile.eventTargetActions[eventIndex];
    for (const UI_StateAnimationTargetAction& targetAction : targetActions)
    {
        if (targetAction.animationName.empty())
            continue;

        Shared<UI_Base> target = targetAction.target.lock();
        if (nullptr == target)
            continue;

        played = target->Play_UIAnimation(targetAction.animationName, true) || played;
    }

    return played;
}

bool UI_Button::Hit_TestMouse() const
{
    const UI_Transform* transform = Get_Transform();
    if (nullptr == transform)
        return false;

    const POINT& mousePosition = GAME->Get_MousePosition();
    const Vec2 size = Get_ResolvedLocalSize();
    const Vec2& localScale = transform->Get_Scale();
    const Vec2 worldScale = Get_WorldScale();

    const float width = size.x * worldScale.x;
    const float height = size.y * worldScale.y;

    if (width <= 0.f || height <= 0.f)
        return false;

    const Vec2 center = Transform_LocalPointToWorld(Get_ScaledLocalCenter(localScale));
    const Vec2 mouseOffset(
        static_cast<float>(mousePosition.x) - center.x,
        static_cast<float>(mousePosition.y) - center.y);
    const Vec2 localMouseOffset = Rotate_UIPoint(mouseOffset, -Get_WorldRotationRadians());

    return
        fabsf(localMouseOffset.x) <= width * 0.5f &&
        fabsf(localMouseOffset.y) <= height * 0.5f;
}

Vec4 UI_Button::Scale_RGB(const Vec4& color, float scale)
{
    return Vec4(
        min(max(color.x * scale, 0.f), 1.f),
        min(max(color.y * scale, 0.f), 1.f),
        min(max(color.z * scale, 0.f), 1.f),
        color.w);
}

Shared<UI_Button> UI_Button::Create()
{
    auto instance = make_shared<UI_Button>();

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : UI_Button");
        return nullptr;
    }

    return instance;
}

Shared<UI_Base> UI_Button::Clone(void* arg)
{
    auto instance = make_shared<UI_Button>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : UI_Button");
        return nullptr;
    }

    return instance;
}

void UI_Button::Free()
{
    __super::Free();
}

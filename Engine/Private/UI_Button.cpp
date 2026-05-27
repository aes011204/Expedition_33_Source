#include "UI_Button.h"

#include <cmath>

#include "GameInstance.h"

namespace
{
    Vec2 Rotate_Point(const Vec2& point, float rotationRadians)
    {
        const float cosAngle = cosf(rotationRadians);
        const float sinAngle = sinf(rotationRadians);

        return Vec2(
            point.x * cosAngle - point.y * sinAngle,
            point.x * sinAngle + point.y * cosAngle);
    }
}

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
        return Scale_RGB(_color, 0.78f);

    if (_isHovered)
        return Scale_RGB(_color, 1.18f);

    return _color;
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
    UI_StateAnimationEvent eventType = UI_StateAnimationEvent::Count;

    if (_previousDisabledForState != _isDisabled)
        eventType = _isDisabled ? UI_StateAnimationEvent::DisabledEnter : UI_StateAnimationEvent::DisabledExit;
    else if (_previousPressedForState != _isPressed)
        eventType = _isPressed ? UI_StateAnimationEvent::PressedEnter : UI_StateAnimationEvent::PressedExit;
    else if (_previousHoveredForState != _isHovered)
        eventType = _isHovered ? UI_StateAnimationEvent::HoverEnter : UI_StateAnimationEvent::HoverExit;
    else if (_previousFocusedForState != _isFocusedForState)
        eventType = _isFocusedForState ? UI_StateAnimationEvent::FocusedEnter : UI_StateAnimationEvent::FocusedExit;

    if (eventType != UI_StateAnimationEvent::Count)
        Try_PlayStateAnimation(eventType);

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
    const Vec2 localMouseOffset = Rotate_Point(mouseOffset, -Get_WorldRotationRadians());

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

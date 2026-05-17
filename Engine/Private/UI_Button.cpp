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

   // _baseScale = _transform->Get_Scale();

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
        return;
    }

    _isHovered = Hit_TestMouse();

    if (_isHovered && GAME->KeyDown(KEY_TYPE::LBUTTON))
        _isPressed = true;

    if (_isPressed && GAME->KeyUp(KEY_TYPE::LBUTTON))
    {
        _wasClicked = _isHovered;
        _isPressed = false;
        return;
    }

    if (!GAME->KeyDown(KEY_TYPE::LBUTTON) && !GAME->KeyPress(KEY_TYPE::LBUTTON))
        _isPressed = false;

   // if(_enableHoveredAnim && _isHovered)
   // {
   //     Math::Lerp_Damp(_baseScale, _targetScale, 1, timeDelta); 
   // }
   //
   // if(_enableHoveredAnim && !_isHovered)
   // {
   //     Math::Lerp_Damp(_targetScale, _baseScale, 1, timeDelta);
   // }
}

Vec4 UI_Button::Resolve_RenderColor() const
{
    if (_isPressed && _isHovered)
        return Scale_RGB(_color, 0.78f);

    if (_isHovered)
        return Scale_RGB(_color, 1.18f);

    return _color;
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

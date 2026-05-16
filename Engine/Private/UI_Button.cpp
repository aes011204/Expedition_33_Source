#include "UI_Button.h"

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
    const Vec2 worldPosition = Get_WorldPosition();
    const Vec2& size = transform->Get_Size();
    const Vec2& scale = transform->Get_Scale();

    const float width = size.x * scale.x;
    const float height = size.y * scale.y;

    if (width <= 0.f || height <= 0.f)
        return false;

    const float mouseX = static_cast<float>(mousePosition.x);
    const float mouseY = static_cast<float>(mousePosition.y);

    return
        mouseX >= worldPosition.x &&
        mouseX <= worldPosition.x + width &&
        mouseY >= worldPosition.y &&
        mouseY <= worldPosition.y + height;
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

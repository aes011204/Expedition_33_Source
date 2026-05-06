#include "UI_Text.h"
#include "GameInstance.h"

UI_Text::UI_Text()
{
}

UI_Text::UI_Text(const UI_Text& prototype)
    : UI_Base{ prototype }
    , _text{ prototype._text }
    , _fontTag{ prototype._fontTag }
    , _color{ prototype._color }
    , _textAlign{ prototype._textAlign }
{
}

HRESULT UI_Text::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT UI_Text::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(arg)))
        return E_FAIL;

    if (nullptr != arg)
    {
        const UI_TEXT_DESC* desc = static_cast<const UI_TEXT_DESC*>(arg);

        _text = desc->text;
        _fontTag = desc->fontTag;
        _color = desc->color;
        _textAlign = desc->textAlign;
    }

    return S_OK;
}

HRESULT UI_Text::Render()
{
    if (!Is_Open())
        return S_OK;

    if (FAILED(__super::Render()))
        return E_FAIL;

    if (nullptr == _transform)
        return E_FAIL;

    if (_fontTag.empty() || _text.empty())
        return S_OK;

    const Vec2& position = _transform->Get_Position();
    const Vec2& size = _transform->Get_Size();
    const float textScale = max(0.01f, _transform->Get_Scale().x);
    const float boxWidth = max(0.f, size.x * textScale);
    const float boxHeight = max(0.f, size.y * textScale);

    const Vec2 measuredSize = GAME->Measure_Font(_fontTag, _text.c_str());
    const float textWidth = measuredSize.x * textScale;

    float offsetX = 0.f;
    if (TextAlign::Center == _textAlign)
        offsetX = (boxWidth - textWidth) * 0.5f;
    else if (TextAlign::Right == _textAlign)
        offsetX = boxWidth - textWidth;

    const Vec2 alignedTopLeft(position.x + offsetX, position.y);
    const Vec2 boxCenter(position.x + boxWidth * 0.5f, position.y + boxHeight * 0.5f);
    const Vec2 pivot(
        (boxCenter.x - alignedTopLeft.x) / textScale,
        (boxCenter.y - alignedTopLeft.y) / textScale);

    GAME->Draw_Font(
        _fontTag,
        _text.c_str(),
        boxCenter,
        XMVectorSet(_color.x, _color.y, _color.z, _color.w),
        _transform->Get_RotationRadians(),
        pivot,
        textScale);

    return S_OK;
}

Shared<UI_Text> UI_Text::Create()
{
    auto instance = make_shared<UI_Text>();

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : UI_Text");
        return nullptr;
    }

    return instance;
}

Shared<UI_Base> UI_Text::Clone(void* arg)
{
    auto instance = make_shared<UI_Text>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : UI_Text");
        return nullptr;
    }

    return instance;
}

void UI_Text::Free()
{
    __super::Free();
}

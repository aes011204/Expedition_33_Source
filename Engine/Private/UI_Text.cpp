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

    GAME->Draw_Font(_fontTag, _text.c_str(), _transform->Get_Position(), XMVectorSet(_color.x, _color.y, _color.z, _color.w));

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

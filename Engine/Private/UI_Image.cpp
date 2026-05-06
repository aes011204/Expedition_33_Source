#include "UI_Image.h"
#include "GameInstance.h"

UI_Image::UI_Image()
{
}

UI_Image::UI_Image(const UI_Image& prototype)
    : UI_Base{ prototype }
    , _texturePath{ prototype._texturePath }
    , _color{ prototype._color }
    , _textureSize{ prototype._textureSize }
{
}

HRESULT UI_Image::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT UI_Image::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(arg)))
        return E_FAIL;

    if (nullptr != arg)
    {
        const UI_IMAGE_DESC* desc = static_cast<const UI_IMAGE_DESC*>(arg);

        _texturePath = desc->texturePath;
        _color = desc->color;
    }

    if (_texturePath.empty())
        return S_OK;

    ComPtr<Device> device = GAME->Get_Device();
    ComPtr<Context> context = GAME->Get_Context();

    CHECK_NULL(device, E_FAIL);
    CHECK_NULL(context, E_FAIL);

    ComPtr<ID3D11Resource> textureResource = nullptr;
    if (FAILED(CreateWICTextureFromFile(device.Get(), _texturePath.c_str(), textureResource.GetAddressOf(), _srv.GetAddressOf())))
        return E_FAIL;

    ComPtr<Texture2D> texture = nullptr;
    if (nullptr != textureResource && SUCCEEDED(textureResource.As(&texture)))
    {
        D3D11_TEXTURE2D_DESC textureDesc{};
        texture->GetDesc(&textureDesc);
        _textureSize = Vec2(static_cast<float>(textureDesc.Width), static_cast<float>(textureDesc.Height));
    }

    _spriteBatch = make_unique<SpriteBatch>(context.Get());
    CHECK_NULL(_spriteBatch, E_FAIL);

    return S_OK;
}

HRESULT UI_Image::Render()
{
    if (!Is_Open())
        return S_OK;

    if (FAILED(__super::Render()))
        return E_FAIL;

    if (nullptr == _transform)
        return E_FAIL;

    if (nullptr == _srv || nullptr == _spriteBatch)
        return S_OK;

    const Vec2& position = _transform->Get_Position();
    const Vec2& size = _transform->Get_Size();
    const Vec2& scale = _transform->Get_Scale();

    const float width = size.x * scale.x;
    const float height = size.y * scale.y;

    if (width <= 0.f || height <= 0.f)
        return S_OK;

    _spriteBatch->Begin(
        SpriteSortMode_Deferred,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        XMMatrixIdentity());

    const Vec2 textureSize(
        max(1.f, _textureSize.x),
        max(1.f, _textureSize.y));
    const Vec2 center(position.x + width * 0.5f, position.y + height * 0.5f);
    const Vec2 pivot(textureSize.x * 0.5f, textureSize.y * 0.5f);
    const Vec2 drawScale(width / textureSize.x, height / textureSize.y);

    _spriteBatch->Draw(
        _srv.Get(),
        center,
        nullptr,
        XMVectorSet(_color.x, _color.y, _color.z, _color.w),
        _transform->Get_RotationRadians(),
        pivot,
        drawScale,
        SpriteEffects_None,
        0.f);

    _spriteBatch->End();

    return S_OK;
}

Shared<UI_Image> UI_Image::Create()
{
    auto instance = make_shared<UI_Image>();

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : UI_Image");
        return nullptr;
    }

    return instance;
}

Shared<UI_Base> UI_Image::Clone(void* arg)
{
    auto instance = make_shared<UI_Image>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : UI_Image");
        return nullptr;
    }

    return instance;
}

void UI_Image::Free()
{
    _spriteBatch.reset();
    _srv.Reset();

    __super::Free();
}

#include "CustomFont.h"

CustomFont::CustomFont(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}

CustomFont::~CustomFont()
{
    Free();
}

HRESULT CustomFont::Initialize(const wchar_t* fontFilePath)
{
    _spriteBatch = new SpriteBatch(_context.Get());
    _spriteFont = new SpriteFont(_device.Get(), fontFilePath);

    return S_OK;
}

void CustomFont::Draw(const wchar_t* text, const Vec2& position, FXMVECTOR color)
{
    _spriteBatch->Begin(
        SpriteSortMode_Deferred,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        XMMatrixIdentity());

    _spriteFont->DrawString(
        _spriteBatch,
        text,
        position,
        color,
        0.f,
        XMFLOAT2(0.f, 0.f),
        1.f,
        SpriteEffects_None,
        0.f);

    _spriteBatch->End();
}

Shared<CustomFont> CustomFont::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const wchar_t* fontFilePath)
{
    auto instance = make_shared<CustomFont>(device, context);

    if (FAILED(instance->Initialize(fontFilePath)))
    {
        MSG_BOX("Failed to Created : CustomFont");
        return nullptr;
    }
    return instance;
}

void CustomFont::Free()
{
    __super::Free();

    Safe_Delete(_spriteFont);
    Safe_Delete(_spriteBatch);
}

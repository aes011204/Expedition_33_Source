#include "CustomFont.h"

CustomFont::CustomFont(const ComPtr<Device>& device, const ComPtr<Context>& context, float scaleCorrection)
    : _device{ device }
    , _context{ context }
    , _scaleCorrection{ max(0.0001f, scaleCorrection) }
{
}

HRESULT CustomFont::Initialize(const wchar_t* fontFilePath)
{
    _spriteBatch = make_unique<SpriteBatch>(_context.Get());
    _spriteFont = make_unique<SpriteFont>(_device.Get(), fontFilePath);

    return S_OK;
}

Vec2 CustomFont::Measure(const wchar_t* text) const
{
    if (nullptr == _spriteFont || nullptr == text || L'\0' == *text)
        return Vec2(0.f, 0.f);

    XMFLOAT2 measuredSize{};
    XMStoreFloat2(&measuredSize, _spriteFont->MeasureString(text));

    return Vec2(measuredSize.x * _scaleCorrection, measuredSize.y * _scaleCorrection);
}

void CustomFont::Draw(
    const wchar_t* text,
    const Vec2& position,
    FXMVECTOR color,
    float rotationRadians,
    const Vec2& pivot,
    float scale)
{
    if (nullptr == _spriteBatch || nullptr == _spriteFont || nullptr == text || L'\0' == *text)
        return;

    _spriteBatch->Begin(
        SpriteSortMode_Deferred,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        XMMatrixIdentity());

    const float correctedScale = scale * _scaleCorrection;
    const Vec2 correctedPivot(pivot.x / _scaleCorrection, pivot.y / _scaleCorrection);

    _spriteFont->DrawString(
        _spriteBatch.get(),
        text,
        position,
        color,
        rotationRadians,
        correctedPivot,
        correctedScale,
        SpriteEffects_None,
        0.f);

    _spriteBatch->End();
}

Shared<CustomFont> CustomFont::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context,
    const wchar_t* fontFilePath,
    float scaleCorrection)
{
    auto instance = make_shared<CustomFont>(device, context, scaleCorrection);

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

}

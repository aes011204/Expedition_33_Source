#include "CustomFont.h"

CustomFont::CustomFont(ComPtr<Device> device, ComPtr<Context> context)
    : _device { device }
    , _context { context }
{
}

HRESULT CustomFont::Initialize(const wchar_t* pFontFilePath)
{
    _spriteBatch = new SpriteBatch(_context.Get());
    _spriteFont = new SpriteFont(_device.Get(), pFontFilePath);


    return S_OK;
}

void CustomFont::Draw(const wchar_t* pText, const Vec2& position, FXMVECTOR vColor)
{
    _spriteBatch->Begin();

    _spriteFont->DrawString(_spriteBatch, pText, position, vColor);

    _spriteBatch->End();
}

Shared<CustomFont> CustomFont::Create(ComPtr<Device> device, ComPtr<Context> context, const wchar_t* pFontFilePath)
{
    Shared<CustomFont> pInstance = MakeSharedFromRaw(new CustomFont(device, context));

    if (FAILED(pInstance->Initialize(pFontFilePath)))
    {
        MSG_BOX("Failed to Created : CustomFont");
        Safe_Release(pInstance);
    }
    return pInstance;
}


void CustomFont::Free()
{
    __super::Free();

    Safe_Delete(_spriteFont);
    Safe_Delete(_spriteBatch);
}




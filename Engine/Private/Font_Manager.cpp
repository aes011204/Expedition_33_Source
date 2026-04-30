#include "Font_Manager.h"
#include "CustomFont.h"

Font_Manager::Font_Manager(ComPtr<Device> device, ComPtr<Context> context)
    : _device { device }
    , _context{ context }
{
}

HRESULT Font_Manager::Add_Font(const wstring& strFontTag, const wchar_t* pFontFilePath)
{
    if (nullptr != Find_Font(strFontTag))
        return E_FAIL;

    Shared<CustomFont> pFont = CustomFont::Create(_device.Get(), _context.Get(), pFontFilePath);
    if (nullptr == pFont)
        return E_FAIL;

    _fonts.emplace(strFontTag, pFont);

    return S_OK;
}

void Font_Manager::Draw(const wstring& strFontTag, const wchar_t* pText, const Vec2& position, FXMVECTOR vColor)
{
    auto    pFont = Find_Font(strFontTag);
    
    if(nullptr == pFont)
        return;

    pFont->Draw(pText, position, vColor);
}

Shared<CustomFont> Font_Manager::Find_Font(const wstring& strFontTag)
{
    auto    iter = _fonts.find(strFontTag);
    if(iter == _fonts.end())
        return nullptr; 

    return iter->second;
}

Unique<Font_Manager> Font_Manager::Create(ComPtr<Device> device, ComPtr<Context> context)
{
    return Unique<Font_Manager>{ new Font_Manager(device, context) };
}

void Font_Manager::Free()
{
    __super::Free();

    for (auto& Pair : _fonts)
        Safe_Release(Pair.second);

    _fonts.clear();
}




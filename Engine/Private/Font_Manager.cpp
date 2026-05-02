#include "Font_Manager.h"
#include "CustomFont.h"

Font_Manager::Font_Manager(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device { device }
    , _context{ context }
{
}

Font_Manager::~Font_Manager()
{
    int a = 10;
}

HRESULT Font_Manager::Add_Font(const wstring& fontTag, const wchar_t* fontFilePath)
{
    if (nullptr != Find_Font(fontTag))
        return E_FAIL;

    Shared<CustomFont> font = CustomFont::Create(_device.Get(), _context.Get(), fontFilePath);
    if (nullptr == font)
        return E_FAIL;

    _fonts.emplace(fontTag, font);

    return S_OK;
}

void Font_Manager::Draw(const wstring& fontTag, const wchar_t* text, const Vec2& position, FXMVECTOR color)
{
    auto    font = Find_Font(fontTag);
    
    if(nullptr == font)
        return;

    font->Draw(text, position, color);
}

Shared<CustomFont> Font_Manager::Find_Font(const wstring& fontTag)
{
    auto    iter = _fonts.find(fontTag);
    if(iter == _fonts.end())
        return nullptr; 

    return iter->second;
}

Unique<Font_Manager> Font_Manager::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    return make_unique<Font_Manager>(device, context);
}

void Font_Manager::Free()
{
    __super::Free();

    _fonts.clear();
}




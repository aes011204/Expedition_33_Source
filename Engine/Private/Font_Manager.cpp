#include "Font_Manager.h"
#include "CustomFont.h"

namespace
{
    float Resolve_FontScaleCorrection(const wchar_t* fontFilePath)
    {
        if (nullptr == fontFilePath || L'\0' == *fontFilePath)
            return 1.f;

        const string fileName = String::ToLowerCopy(
            String::ToString(fs::path(fontFilePath).filename().wstring()));

        if (fileName == "33_mainen_font.spritefont" ||
            fileName == "33_subnum_font.spritefont")
        {
            return 1.f / 3.f;
        }

        if (fileName == "33_mainnum_font.spritefont")
            return 1.f / 4.f;

        if (fileName == "33_mainkr_font.spritefont" ||
            fileName == "33_subkr_font.spritefont")
        {
            return 1.f / 2.f;
        }

        return 1.f;
    }
}

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
    {
        LOG_WARN("Font already registered: {}", String::ToString(fontTag));
        return S_OK;
    }

    Shared<CustomFont> font = CustomFont::Create(
        _device.Get(),
        _context.Get(),
        fontFilePath,
        Resolve_FontScaleCorrection(fontFilePath));
    if (nullptr == font)
        return E_FAIL;

    _fonts.emplace(fontTag, font);

    return S_OK;
}

Vec2 Font_Manager::Measure(const wstring& fontTag, const wchar_t* text)
{
    auto    font = Find_Font(fontTag);

    if(nullptr == font)
        return Vec2(0.f, 0.f);

    return font->Measure(text);
}

void Font_Manager::Draw(
    const wstring& fontTag,
    const wchar_t* text,
    const Vec2& position,
    FXMVECTOR color,
    float rotationRadians,
    const Vec2& pivot,
    float scale)
{
    auto    font = Find_Font(fontTag);
    
    if(nullptr == font)
        return;

    font->Draw(text, position, color, rotationRadians, pivot, scale);
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




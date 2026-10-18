#pragma once

#include "UI_Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL UI_Text final : public UI_Base
{
public:
    enum class TextAlign
    {
        Left,
        Center,
        Right,
    };

    struct UI_TEXT_DESC : public UI_Base::UI_DESC
    {
        wstring text{};
        wstring fontTag{};
        Vec4 color{ 1.f, 1.f, 1.f, 1.f };
        TextAlign textAlign{ TextAlign::Left };
        bool textWrap{ false };
    };

public:
    UI_Text();
    UI_Text(const UI_Text& prototype);
    ~UI_Text() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    HRESULT Render() override;

public:
    void Set_Text(const wstring& text);
    const wstring& Get_Text() const { return _text; }

    void Set_FontTag(const wstring& fontTag);
    const wstring& Get_FontTag() const { return _fontTag; }

    void Set_Color(const Vec4& color) override { _color = color; }
    Vec4 Get_Color() const override { return _color; }

    void Set_TextAlign(TextAlign textAlign);
    TextAlign Get_TextAlign() const { return _textAlign; }

    void Set_TextWrap(bool textWrap);
    bool Get_TextWrap() const { return _textWrap; }

private:
    struct TextLayoutCache
    {
        bool valid = false;
        wstring text{};
        wstring fontTag{};
        bool textWrap = false;
        float localTextScale = 0.f;
        float boxWidth = 0.f;
        Vec2 measuredSize = Vec2::Zero;
        float lineHeight = 0.f;
        vector<wstring> lines{};
        vector<float> lineWidths{};
    };

    void Invalidate_TextLayoutCache();
    const TextLayoutCache& Ensure_TextLayoutCache(float localTextScale, float boxWidth) const;

private:
    wstring _text{};
    wstring _fontTag{};
    Vec4 _color{ 1.f, 1.f, 1.f, 1.f };
    TextAlign _textAlign{ TextAlign::Left };
    bool _textWrap{ false };
    mutable TextLayoutCache _layoutCache{};

public:
    static Shared<UI_Text> Create();
    Shared<UI_Base> Clone(void* arg) override;
    void Free() override;
};

NS_END

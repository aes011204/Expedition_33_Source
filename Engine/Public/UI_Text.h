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
    void Set_Text(const wstring& text) { _text = text; }
    const wstring& Get_Text() const { return _text; }

    void Set_FontTag(const wstring& fontTag) { _fontTag = fontTag; }
    const wstring& Get_FontTag() const { return _fontTag; }

    void Set_Color(const Vec4& color) { _color = color; }
    const Vec4& Get_Color() const { return _color; }

    void Set_TextAlign(TextAlign textAlign) { _textAlign = textAlign; }
    TextAlign Get_TextAlign() const { return _textAlign; }

    void Set_TextWrap(bool textWrap) { _textWrap = textWrap; }
    bool Get_TextWrap() const { return _textWrap; }

private:
    wstring _text{};
    wstring _fontTag{};
    Vec4 _color{ 1.f, 1.f, 1.f, 1.f };
    TextAlign _textAlign{ TextAlign::Left };
    bool _textWrap{ false };

public:
    static Shared<UI_Text> Create();
    Shared<UI_Base> Clone(void* arg) override;
    void Free() override;
};

NS_END

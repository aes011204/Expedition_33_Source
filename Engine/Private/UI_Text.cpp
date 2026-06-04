#include "UI_Text.h"
#include "GameInstance.h"

namespace
{
    vector<wstring> Split_TextLines(const wstring& text)
    {
        vector<wstring> lines;
        size_t lineStart = 0;

        for (size_t i = 0; i <= text.size(); ++i)
        {
            if (i == text.size() || text[i] == L'\n')
            {
                size_t lineEnd = i;
                if (lineEnd > lineStart && text[lineEnd - 1] == L'\r')
                    --lineEnd;

                lines.push_back(text.substr(lineStart, lineEnd - lineStart));
                lineStart = i + 1;
            }
        }

        if (lines.empty())
            lines.push_back(L"");

        return lines;
    }

    float Measure_TextWidth(const wstring& fontTag, const wstring& text, float scale)
    {
        if (text.empty())
            return 0.f;

        return GAME->Measure_Font(fontTag, text.c_str()).x * scale;
    }

    void Push_WrappedWord(
        const wstring& fontTag,
        const wstring& word,
        float maxWidth,
        float scale,
        wstring& currentLine,
        vector<wstring>& wrappedLines)
    {
        if (word.empty())
            return;

        const wstring candidate = currentLine.empty() ? word : currentLine + L" " + word;
        const float candidateWidth = Measure_TextWidth(fontTag, candidate, scale);
        if (candidateWidth <= maxWidth)
        {
            currentLine = candidate;
            return;
        }

        if (!currentLine.empty())
        {
            wrappedLines.push_back(currentLine);
            currentLine.clear();
        }

        if (Measure_TextWidth(fontTag, word, scale) <= maxWidth)
        {
            currentLine = word;
            return;
        }

        wstring fragment;
        for (wchar_t character : word)
        {
            const wstring candidateFragment = fragment + character;
            if (!fragment.empty() && Measure_TextWidth(fontTag, candidateFragment, scale) > maxWidth)
            {
                wrappedLines.push_back(fragment);
                fragment = wstring(1, character);
            }
            else
            {
                fragment = candidateFragment;
            }
        }

        currentLine = fragment;
    }

    vector<wstring> Wrap_TextLines(const wstring& fontTag, const wstring& text, float maxWidth, float scale)
    {
        vector<wstring> wrappedLines;
        const vector<wstring> rawLines = Split_TextLines(text);

        for (const wstring& rawLine : rawLines)
        {
            if (rawLine.empty())
            {
                wrappedLines.push_back(L"");
                continue;
            }

            wstring currentLine;
            size_t wordStart = 0;

            for (size_t i = 0; i <= rawLine.size(); ++i)
            {
                if (i == rawLine.size() || rawLine[i] == L' ' || rawLine[i] == L'\t')
                {
                    Push_WrappedWord(fontTag, rawLine.substr(wordStart, i - wordStart), maxWidth, scale, currentLine, wrappedLines);
                    wordStart = i + 1;
                }
            }

            if (!currentLine.empty())
                wrappedLines.push_back(currentLine);
        }

        if (wrappedLines.empty())
            wrappedLines.push_back(L"");

        return wrappedLines;
    }

    Vec4 Get_SpriteFontRenderColor(const UI_Text& text, const Vec4& color)
    {
        const float alphaMultiplier = text.Get_EffectiveAlphaMultiplier();
        Vec4 renderColor = color;
        renderColor.x = min(max(renderColor.x, 0.f), 1.f) * alphaMultiplier;
        renderColor.y = min(max(renderColor.y, 0.f), 1.f) * alphaMultiplier;
        renderColor.z = min(max(renderColor.z, 0.f), 1.f) * alphaMultiplier;
        renderColor.w = min(max(renderColor.w, 0.f), 1.f) * alphaMultiplier;
        return renderColor;
    }
}

UI_Text::UI_Text()
{
}

UI_Text::UI_Text(const UI_Text& prototype)
    : UI_Base{ prototype }
    , _text{ prototype._text }
    , _fontTag{ prototype._fontTag }
    , _color{ prototype._color }
    , _textAlign{ prototype._textAlign }
    , _textWrap{ prototype._textWrap }
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
        _textAlign = desc->textAlign;
        _textWrap = desc->textWrap;
    }

    return S_OK;
}

HRESULT UI_Text::Render()
{
    if (!Is_EffectivelyOpen())
        return S_OK;

    if (FAILED(__super::Render()))
        return E_FAIL;

    if (nullptr == _transform)
        return E_FAIL;

    if (_fontTag.empty() || _text.empty())
        return S_OK;

    const Vec2 localPivotPosition = Get_ResolvedLocalPosition();
    const Vec2 size = Get_ResolvedLocalSize();
    const Vec2& objectPivot = _transform->Get_Pivot();
    const float localTextScale = max(0.01f, _transform->Get_Scale().x);
    const float worldTextScale = max(0.01f, Get_WorldScale().x);
    const float boxWidth = max(0.f, size.x * localTextScale);
    const Vec2 localBoxTopLeft(
        localPivotPosition.x - size.x * objectPivot.x * localTextScale,
        localPivotPosition.y - size.y * objectPivot.y * localTextScale);
    const Vec2 pivotWorldPosition = Transform_LocalPointToWorld(localPivotPosition);
    const float worldRotationRadians = Get_WorldRotationRadians();

    if (_textWrap)
    {
        const vector<wstring> lines = Wrap_TextLines(_fontTag, _text, max(1.f, boxWidth), localTextScale);
        const float lineHeight = max(1.f, GAME->Measure_Font(_fontTag, L"Mg").y * localTextScale);
        const Vec4 renderColor = Get_SpriteFontRenderColor(*this, _color);

        for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
        {
            const wstring& line = lines[lineIndex];
            const float textWidth = Measure_TextWidth(_fontTag, line, localTextScale);

            float offsetX = 0.f;
            if (TextAlign::Center == _textAlign)
                offsetX = (boxWidth - textWidth) * 0.5f;
            else if (TextAlign::Right == _textAlign)
                offsetX = boxWidth - textWidth;

            const Vec2 alignedTopLeft(localBoxTopLeft.x + offsetX, localBoxTopLeft.y + static_cast<float>(lineIndex) * lineHeight);
            const Vec2 pivot(
                (localPivotPosition.x - alignedTopLeft.x) / localTextScale,
                (localPivotPosition.y - alignedTopLeft.y) / localTextScale);

            GAME->Draw_Font(
                _fontTag,
                line.c_str(),
                pivotWorldPosition,
                XMVectorSet(renderColor.x, renderColor.y, renderColor.z, renderColor.w),
                worldRotationRadians,
                pivot,
                worldTextScale);
        }

        return S_OK;
    }

    const Vec2 measuredSize = GAME->Measure_Font(_fontTag, _text.c_str());
    const float textWidth = measuredSize.x * localTextScale;

    float offsetX = 0.f;
    if (TextAlign::Center == _textAlign)
        offsetX = (boxWidth - textWidth) * 0.5f;
    else if (TextAlign::Right == _textAlign)
        offsetX = boxWidth - textWidth;

    const Vec2 alignedTopLeft(localBoxTopLeft.x + offsetX, localBoxTopLeft.y);
    const Vec2 pivot(
        (localPivotPosition.x - alignedTopLeft.x) / localTextScale,
        (localPivotPosition.y - alignedTopLeft.y) / localTextScale);
    const Vec4 renderColor = Get_SpriteFontRenderColor(*this, _color);

    GAME->Draw_Font(
        _fontTag,
        _text.c_str(),
        pivotWorldPosition,
        XMVectorSet(renderColor.x, renderColor.y, renderColor.z, renderColor.w),
        worldRotationRadians,
        pivot,
        worldTextScale);

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

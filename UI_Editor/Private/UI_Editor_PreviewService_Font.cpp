#include "pch.h"
#include "UI_Editor_PreviewService.h"

#include "GameInstance.h"
#include "UI_Editor_Utils.h"

namespace UIEditor
{
    namespace
    {
        float Resolve_FontPreviewScaleCorrectionFromPath(const wstring& fontFilePath)
        {
            if (fontFilePath.empty())
                return 1.f;

            const string fileName = String::ToLowerCopy(
                String::ToString(fs::path(fontFilePath).filename().wstring()));

            if (fileName == "33_mainkr_font.spritefont")
            {
                return 24.f / 56.f;
            }

            if (fileName == "33_subkr_font.spritefont")
            {
                return 24.f / 60.f;
            }

            if (fileName == "33_damagenum_font.spritefont")
            {
                return 24.f / 240.f;
            }

            if (fileName == "33_mainen_font.spritefont" ||
                fileName == "33_mainnum_font.spritefont" ||
                fileName == "33_subnum_font.spritefont")
            {
                return 24.f / 96.f;
            }

            return 1.f;
        }

        float Resolve_FontPreviewScaleCorrection(const string& fontGuid, const string& fontPath)
        {
            if (!fontGuid.empty())
            {
                const wstring guidFontPath = GAME->Resolve_AssetPath(fontGuid);
                if (!guidFontPath.empty())
                    return Resolve_FontPreviewScaleCorrectionFromPath(guidFontPath);
            }

            if (!fontPath.empty())
                return Resolve_FontPreviewScaleCorrectionFromPath(Resolve_ClientRuntimeResourcePath(fontPath));

            return 1.f;
        }

        vector<wstring> Split_FontPreviewLines(const wstring& text)
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

        float Measure_FontPreviewWidth(const SpriteFont& font, const wstring& text, float scale)
        {
            if (text.empty())
                return 0.f;

            XMFLOAT2 measuredSize{};
            XMStoreFloat2(&measuredSize, font.MeasureString(text.c_str()));
            return max(0.f, measuredSize.x * scale);
        }

        float Measure_FontPreviewLineHeight(const SpriteFont& font, float scale)
        {
            XMFLOAT2 measuredSize{};
            XMStoreFloat2(&measuredSize, font.MeasureString(L"Mg"));

            const float measuredHeight = measuredSize.y > 0.f ? measuredSize.y : 32.f;
            return max(1.f, measuredHeight * scale);
        }

        void Push_WrappedFontPreviewWord(
            const SpriteFont& font,
            const wstring& word,
            float maxWidth,
            float scale,
            wstring& currentLine,
            vector<wstring>& wrappedLines)
        {
            if (word.empty())
                return;

            const wstring candidate = currentLine.empty() ? word : currentLine + L" " + word;
            if (Measure_FontPreviewWidth(font, candidate, scale) <= maxWidth)
            {
                currentLine = candidate;
                return;
            }

            if (!currentLine.empty())
            {
                wrappedLines.push_back(currentLine);
                currentLine.clear();
            }

            if (Measure_FontPreviewWidth(font, word, scale) <= maxWidth)
            {
                currentLine = word;
                return;
            }

            wstring fragment;
            for (wchar_t character : word)
            {
                const wstring candidateFragment = fragment + character;
                if (!fragment.empty() && Measure_FontPreviewWidth(font, candidateFragment, scale) > maxWidth)
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

        vector<wstring> Wrap_FontPreviewLines(const SpriteFont& font, const wstring& text, float maxWidth, float scale)
        {
            vector<wstring> wrappedLines;
            const vector<wstring> rawLines = Split_FontPreviewLines(text);

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
                        Push_WrappedFontPreviewWord(font, rawLine.substr(wordStart, i - wordStart), maxWidth, scale, currentLine, wrappedLines);
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

        float Resolve_FontPreviewLineX(float boxLeft, float boxRight, float lineWidth, UI_TextAlign textAlign)
        {
            const float boxWidth = max(0.f, boxRight - boxLeft);

            switch (textAlign)
            {
            case UI_TextAlign::Center:
                return boxLeft + (boxWidth - lineWidth) * 0.5f;

            case UI_TextAlign::Right:
                return boxRight - lineWidth;

            case UI_TextAlign::Left:
            default:
                return boxLeft;
            }
        }

        Vec4 Make_PremultipliedFontColor(const Vec4& color)
        {
            const float alpha = min(max(color.w, 0.f), 1.f);
            return Vec4(
                min(max(color.x, 0.f), 1.f) * alpha,
                min(max(color.y, 0.f), 1.f) * alpha,
                min(max(color.z, 0.f), 1.f) * alpha,
                alpha);
        }

        bool Are_ClipRectsEqual(const D3D11_RECT& lhs, const D3D11_RECT& rhs)
        {
            return lhs.left == rhs.left &&
                lhs.top == rhs.top &&
                lhs.right == rhs.right &&
                lhs.bottom == rhs.bottom;
        }
    }

    void UI_Editor_PreviewService::Render_FontPreviews()
    {
        if (nullptr == _fontPreviewSpriteBatch || nullptr == _context || _fontPreviewCommands.empty())
            return;

        const ImGuiIO& io = ImGui::GetIO();
        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX = 0.f;
        viewport.TopLeftY = 0.f;
        viewport.Width = max(1.f, io.DisplaySize.x);
        viewport.Height = max(1.f, io.DisplaySize.y);
        viewport.MinDepth = 0.f;
        viewport.MaxDepth = 1.f;
        _fontPreviewSpriteBatch->SetViewport(viewport);

        size_t commandIndex = 0;
        while (commandIndex < _fontPreviewCommands.size())
        {
            const D3D11_RECT clipRect = _fontPreviewCommands[commandIndex].clipRect;
            size_t nextCommandIndex = commandIndex + 1;
            while (nextCommandIndex < _fontPreviewCommands.size() &&
                Are_ClipRectsEqual(clipRect, _fontPreviewCommands[nextCommandIndex].clipRect))
            {
                ++nextCommandIndex;
            }

            bool isBatchOpen = false;
            try
            {
                _context->RSSetScissorRects(1, &clipRect);

                _fontPreviewSpriteBatch->Begin(
                    SpriteSortMode_Deferred,
                    nullptr,
                    nullptr,
                    nullptr,
                    _fontPreviewRasterizerState.Get(),
                    nullptr,
                    XMMatrixIdentity());
                isBatchOpen = true;

                for (size_t drawIndex = commandIndex; drawIndex < nextCommandIndex; ++drawIndex)
                {
                    const FontPreviewCommand& command = _fontPreviewCommands[drawIndex];
                    SpriteFont* font = Get_FontPreview(command.fontGuid, command.fontPath);
                    if (nullptr == font || command.text.empty())
                        continue;

                    const wstring safeText = Build_SafeSpriteFontText(*font, command.text);
                    if (safeText.empty())
                        continue;

                    const Vec4 fontColor = Make_PremultipliedFontColor(command.color);
                    font->DrawString(
                        _fontPreviewSpriteBatch.get(),
                        safeText.c_str(),
                        command.position,
                        XMVectorSet(fontColor.x, fontColor.y, fontColor.z, fontColor.w),
                        command.rotationRadians,
                        command.pivot,
                        command.scale,
                        SpriteEffects_None,
                        0.f);
                }

                _fontPreviewSpriteBatch->End();
                isBatchOpen = false;
            }
            catch (...)
            {
                if (isBatchOpen)
                {
                    try
                    {
                        _fontPreviewSpriteBatch->End();
                    }
                    catch (...)
                    {
                    }
                }
            }

            commandIndex = nextCommandIndex;
        }
    }

    bool UI_Editor_PreviewService::Queue_FontPreview(
        const string& fontGuid,
        const string& fontPath,
        const wstring& text,
        const Vec2& boxMin,
        const Vec2& boxMax,
        const Vec4& color,
        float rotationRadians,
        const Vec2& pivotPosition,
        UI_TextAlign textAlign,
        float scale,
        bool textWrap,
        const D3D11_RECT& clipRect)
    {
        SpriteFont* font = Get_FontPreview(fontGuid, fontPath);
        if (nullptr == font || text.empty())
            return false;

        const wstring safeText = Build_SafeSpriteFontText(*font, text);
        if (safeText.empty())
            return false;

        const float safeScale = max(0.0001f, scale);
        const float drawScale = safeScale * Resolve_FontPreviewScaleCorrection(fontGuid, fontPath);
        const float boxWidth = max(1.f, boxMax.x - boxMin.x);
        const vector<wstring> lines = textWrap
            ? Wrap_FontPreviewLines(*font, safeText, boxWidth, drawScale)
            : Split_FontPreviewLines(safeText);
        const float lineHeight = Measure_FontPreviewLineHeight(*font, drawScale);
        size_t nonEmptyLineCount = 0;
        for (const wstring& line : lines)
        {
            if (!line.empty())
                ++nonEmptyLineCount;
        }

        constexpr size_t kMaxFontPreviewCommandsPerFrame = 2048;
        if (_fontPreviewCommands.size() + nonEmptyLineCount > kMaxFontPreviewCommandsPerFrame)
            return false;

        float textY = boxMin.y;
        bool queuedAny = false;

        for (const wstring& line : lines)
        {
            if (!line.empty())
            {
                const float lineWidth = Measure_FontPreviewWidth(*font, line, drawScale);
                const float textX = Resolve_FontPreviewLineX(boxMin.x, boxMax.x, lineWidth, textAlign);

                FontPreviewCommand command = {};
                command.fontGuid = fontGuid;
                command.fontPath = fontPath;
                command.text = line;
                command.position = pivotPosition;
                command.color = color;
                command.rotationRadians = rotationRadians;
                command.pivot = Vec2((pivotPosition.x - textX) / drawScale, (pivotPosition.y - textY) / drawScale);
                command.scale = drawScale;
                command.clipRect = clipRect;
                _fontPreviewCommands.push_back(command);
                queuedAny = true;
            }

            textY += lineHeight;
        }

        return queuedAny;
    }

    SpriteFont* UI_Editor_PreviewService::Get_FontPreview(const string& fontGuid, const string& fontPath)
    {
        if (nullptr == _device)
            return nullptr;

        auto loadFontPreview = [this](const string& cacheKey, const wstring& resolvedFontPath) -> SpriteFont*
        {
            if (cacheKey.empty() || resolvedFontPath.empty())
                return nullptr;

            const auto cachedIter = _fontPreviewCache.find(cacheKey);
            if (cachedIter != _fontPreviewCache.end())
                return cachedIter->second.get();

            if (_fontPreviewFailedGuids.find(cacheKey) != _fontPreviewFailedGuids.end())
                return nullptr;

            if (!fs::exists(resolvedFontPath))
            {
                _fontPreviewFailedGuids.insert(cacheKey);
                return nullptr;
            }

            try
            {
                Unique<SpriteFont> font = make_unique<SpriteFont>(_device.Get(), resolvedFontPath.c_str());
                if (font->ContainsCharacter(L'?'))
                    font->SetDefaultCharacter(L'?');

                SpriteFont* rawFont = font.get();
                _fontPreviewCache.emplace(cacheKey, move(font));
                return rawFont;
            }
            catch (...)
            {
                _fontPreviewFailedGuids.insert(cacheKey);
                return nullptr;
            }
        };

        if (!fontGuid.empty())
        {
            const wstring guidFontPath = GAME->Resolve_AssetPath(fontGuid);
            if (SpriteFont* font = loadFontPreview("guid:" + fontGuid, guidFontPath))
                return font;
        }

        if (!fontPath.empty())
        {
            const wstring resolvedFontPath = Resolve_ClientRuntimeResourcePath(fontPath);
            return loadFontPreview("path:" + String::ToString(resolvedFontPath), resolvedFontPath);
        }

        return nullptr;
    }
}

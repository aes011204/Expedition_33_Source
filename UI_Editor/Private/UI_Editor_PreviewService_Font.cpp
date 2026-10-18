#include "pch.h"
#include "UI_Editor_PreviewService.h"

#include "Debug_Profile.h"
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

        string Make_RuntimePathCacheKey(string path)
        {
            replace(path.begin(), path.end(), '\\', '/');
            return String::ToLowerCopy(path);
        }

        string Make_NormalizedWidePathKey(wstring path)
        {
            try
            {
                path = fs::path(path).lexically_normal().wstring();
            }
            catch (...)
            {
            }

            replace(path.begin(), path.end(), L'\\', L'/');
            transform(path.begin(), path.end(), path.begin(), [](wchar_t character)
                {
                    return static_cast<wchar_t>(towlower(character));
                });

            return String::ToString(path);
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

        struct FontPreviewLineLayout
        {
            wstring safeText = L"";
            float lineWidth = 0.f;
            float yOffset = 0.f;
        };
    }

    wstring UI_Editor_PreviewService::Resolve_FontPreviewPath(const string& fontPath)
    {
        if (fontPath.empty())
            return {};

        const string cacheKey = Make_RuntimePathCacheKey(fontPath);
        const auto cachedIter = _resolvedFontPathCache.find(cacheKey);
        if (cachedIter != _resolvedFontPathCache.end())
            return cachedIter->second;

        const wstring resolvedFontPath = Resolve_ClientRuntimeResourcePath(fontPath);
        _resolvedFontPathCache.emplace(cacheKey, resolvedFontPath);
        return resolvedFontPath;
    }

    string UI_Editor_PreviewService::Make_FontPreviewCacheKey(const string& fontGuid, const string& fontPath)
    {
        if (!fontGuid.empty())
            return "guid:" + fontGuid;

        if (fontPath.empty())
            return {};

        const wstring resolvedFontPath = Resolve_FontPreviewPath(fontPath);
        if (!resolvedFontPath.empty())
            return "path:" + Make_NormalizedWidePathKey(resolvedFontPath);

        return "path:" + Make_RuntimePathCacheKey(fontPath);
    }

    float UI_Editor_PreviewService::Resolve_FontPreviewScaleCorrection(const string& fontGuid, const string& fontPath)
    {
        const string fontCacheKey = Make_FontPreviewCacheKey(fontGuid, fontPath);
        const auto cachedIter = _fontScaleCorrectionCache.find(fontCacheKey);
        if (cachedIter != _fontScaleCorrectionCache.end())
            return cachedIter->second;

        float scaleCorrection = 1.f;
        if (!fontGuid.empty())
        {
            const wstring guidFontPath = GAME->Resolve_AssetPath(fontGuid);
            if (!guidFontPath.empty())
                scaleCorrection = Resolve_FontPreviewScaleCorrectionFromPath(guidFontPath);
        }
        else if (!fontPath.empty())
        {
            scaleCorrection = Resolve_FontPreviewScaleCorrectionFromPath(Resolve_FontPreviewPath(fontPath));
        }

        _fontScaleCorrectionCache[fontCacheKey] = scaleCorrection;
        return scaleCorrection;
    }

    wstring UI_Editor_PreviewService::Build_CachedSafeFontPreviewText(const string& fontCacheKey, const SpriteFont& font, const wstring& text)
    {
        if (fontCacheKey.empty() || text.empty())
            return {};

        const string cacheKey = fontCacheKey + "|text:" + String::ToString(text);
        const auto cachedIter = _safeFontTextCache.find(cacheKey);
        if (cachedIter != _safeFontTextCache.end())
            return cachedIter->second;

        constexpr size_t kMaxSafeFontTextCacheEntries = 4096;
        if (_safeFontTextCache.size() >= kMaxSafeFontTextCacheEntries)
            _safeFontTextCache.clear();

        const wstring safeText = Build_SafeSpriteFontText(font, text);
        _safeFontTextCache.emplace(cacheKey, safeText);
        return safeText;
    }

    float UI_Editor_PreviewService::Measure_CachedFontPreviewWidth(const string& fontCacheKey, const SpriteFont& font, const wstring& text, float scale)
    {
        if (text.empty())
            return 0.f;

        const string cacheKey = fontCacheKey + "|scale:" + to_string(scale) + "|width:" + String::ToString(text);
        const auto cachedIter = _fontTextWidthCache.find(cacheKey);
        if (cachedIter != _fontTextWidthCache.end())
            return cachedIter->second;

        constexpr size_t kMaxFontTextWidthCacheEntries = 8192;
        if (_fontTextWidthCache.size() >= kMaxFontTextWidthCacheEntries)
            _fontTextWidthCache.clear();

        const float width = Measure_FontPreviewWidth(font, text, scale);
        _fontTextWidthCache.emplace(cacheKey, width);
        return width;
    }

    float UI_Editor_PreviewService::Measure_CachedFontPreviewLineHeight(const string& fontCacheKey, const SpriteFont& font, float scale)
    {
        const string cacheKey = fontCacheKey + "|scale:" + to_string(scale) + "|lineHeight";
        const auto cachedIter = _fontLineHeightCache.find(cacheKey);
        if (cachedIter != _fontLineHeightCache.end())
            return cachedIter->second;

        constexpr size_t kMaxFontLineHeightCacheEntries = 1024;
        if (_fontLineHeightCache.size() >= kMaxFontLineHeightCacheEntries)
            _fontLineHeightCache.clear();

        const float lineHeight = Measure_FontPreviewLineHeight(font, scale);
        _fontLineHeightCache.emplace(cacheKey, lineHeight);
        return lineHeight;
    }

    void UI_Editor_PreviewService::Render_FontPreviews()
    {
        if (nullptr == _fontPreviewSpriteBatch || nullptr == _context || _fontPreviewCommands.empty())
            return;

        ENGINE_PROFILE_SCOPE_DETAIL("UIEditor.FontPreview.Render");

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
            ++_fontPreviewFrameStats.clipGroupCount;

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
                    if (nullptr == font || command.safeText.empty())
                        continue;

                    const Vec4 fontColor = Make_PremultipliedFontColor(command.color);
                    font->DrawString(
                        _fontPreviewSpriteBatch.get(),
                        command.safeText.c_str(),
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
        const D3D11_RECT& clipRect,
        const D3D11_RECT* sharedClipRect,
        bool allowSharedClip)
    {
        ++_fontPreviewFrameStats.requestedTextCount;

        SpriteFont* font = Get_FontPreview(fontGuid, fontPath);
        if (nullptr == font || text.empty())
            return false;

        const string fontCacheKey = Make_FontPreviewCacheKey(fontGuid, fontPath);
        const wstring safeText = Build_CachedSafeFontPreviewText(fontCacheKey, *font, text);
        if (safeText.empty())
            return false;

        const float safeScale = max(0.0001f, scale);
        const float drawScale = safeScale * Resolve_FontPreviewScaleCorrection(fontGuid, fontPath);
        const float boxWidth = max(1.f, boxMax.x - boxMin.x);
        const vector<wstring> lines = textWrap
            ? Wrap_FontPreviewLines(*font, safeText, boxWidth, drawScale)
            : Split_FontPreviewLines(safeText);
        const float lineHeight = Measure_CachedFontPreviewLineHeight(fontCacheKey, *font, drawScale);

        vector<FontPreviewLineLayout> lineLayouts;
        lineLayouts.reserve(lines.size());
        float maxLineWidth = 0.f;
        float drawnBottomOffset = 0.f;
        float layoutY = 0.f;
        for (const wstring& line : lines)
        {
            if (!line.empty())
            {
                FontPreviewLineLayout lineLayout = {};
                lineLayout.safeText = line;
                lineLayout.lineWidth = Measure_CachedFontPreviewWidth(fontCacheKey, *font, line, drawScale);
                lineLayout.yOffset = layoutY;
                maxLineWidth = max(maxLineWidth, lineLayout.lineWidth);
                drawnBottomOffset = max(drawnBottomOffset, layoutY + lineHeight);
                lineLayouts.push_back(move(lineLayout));
            }

            layoutY += lineHeight;
        }

        constexpr size_t kMaxFontPreviewCommandsPerFrame = 2048;
        if (_fontPreviewCommands.size() + lineLayouts.size() > kMaxFontPreviewCommandsPerFrame)
            return false;

        const float boxHeight = max(1.f, boxMax.y - boxMin.y);
        constexpr float kSharedClipFitPadding = 0.5f;
        const bool canUseSharedClip =
            allowSharedClip &&
            nullptr != sharedClipRect &&
            !textWrap &&
            maxLineWidth <= boxWidth + kSharedClipFitPadding &&
            drawnBottomOffset <= boxHeight + kSharedClipFitPadding;
        const D3D11_RECT finalClipRect = canUseSharedClip ? *sharedClipRect : clipRect;
        bool queuedAny = false;

        for (const FontPreviewLineLayout& lineLayout : lineLayouts)
        {
            const float textX = Resolve_FontPreviewLineX(boxMin.x, boxMax.x, lineLayout.lineWidth, textAlign);
            const float textY = boxMin.y + lineLayout.yOffset;

            FontPreviewCommand command = {};
            command.fontGuid = fontGuid;
            command.fontPath = fontPath;
            command.safeText = lineLayout.safeText;
            command.position = pivotPosition;
            command.color = color;
            command.rotationRadians = rotationRadians;
            command.pivot = Vec2((pivotPosition.x - textX) / drawScale, (pivotPosition.y - textY) / drawScale);
            command.scale = drawScale;
            command.clipRect = finalClipRect;
            _fontPreviewCommands.push_back(command);
            ++_fontPreviewFrameStats.queuedCommandCount;
            if (canUseSharedClip)
                ++_fontPreviewFrameStats.sharedClipCommandCount;
            else
                ++_fontPreviewFrameStats.objectClipCommandCount;
            queuedAny = true;
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

                _fontPreviewCache.emplace(cacheKey, move(font));
                _fontPreviewCacheOrder.push_back(cacheKey);
                Trim_FontPreviewCache();
                if (_fontPreviewCache.size() == 1 || _fontPreviewCache.size() % 16 == 0)
                {
                    LOG_INFO(
                        "[UIEditor][PreviewLeakWatch] font preview loaded. cacheSize={} path='{}'",
                        _fontPreviewCache.size(),
                        String::ToString(resolvedFontPath));
                }

                const auto loadedIter = _fontPreviewCache.find(cacheKey);
                return loadedIter != _fontPreviewCache.end() ? loadedIter->second.get() : nullptr;
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
            if (SpriteFont* font = loadFontPreview(Make_FontPreviewCacheKey(fontGuid, ""), guidFontPath))
                return font;
        }

        if (!fontPath.empty())
        {
            const wstring resolvedFontPath = Resolve_FontPreviewPath(fontPath);
            return loadFontPreview(Make_FontPreviewCacheKey("", fontPath), resolvedFontPath);
        }

        return nullptr;
    }
}

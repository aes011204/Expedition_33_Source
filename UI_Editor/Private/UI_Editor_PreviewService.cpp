#include "pch.h"
#include "UI_Editor_PreviewService.h"

#include "GameInstance.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "UI_Editor_Utils.h"
#include "VIBuffer_Rect.h"

namespace UIEditor
{
    namespace
    {
        fs::path Get_ModuleDirectory()
        {
            wchar_t modulePath[MAX_PATH] = {};
            if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
                return {};

            return fs::path(modulePath).parent_path();
        }

        bool Try_FindResourceRootFromStart(const fs::path& startPath, fs::path& outResourceRoot)
        {
            if (startPath.empty())
                return false;

            try
            {
                fs::path start = fs::absolute(startPath).lexically_normal();
                for (fs::path currentPath = start; !currentPath.empty(); currentPath = currentPath.parent_path())
                {
                    const fs::path candidate = (currentPath / L"Client" / L"Bin" / L"Resources").lexically_normal();
                    if (fs::exists(candidate) && fs::is_directory(candidate))
                    {
                        outResourceRoot = candidate;
                        return true;
                    }

                    if (currentPath == currentPath.parent_path())
                        break;
                }
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI Editor resource root search failed: {}", exception.what());
            }

            return false;
        }

        wstring Resolve_UIShaderPath()
        {
            vector<fs::path> startPaths;

            const fs::path moduleDirectory = Get_ModuleDirectory();
            if (!moduleDirectory.empty())
                startPaths.push_back(moduleDirectory);

            try
            {
                startPaths.push_back(fs::current_path());
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI Editor shader path current directory query failed: {}", exception.what());
            }

            for (const fs::path& startPath : startPaths)
            {
                fs::path resourceRoot = {};
                if (!Try_FindResourceRootFromStart(startPath, resourceRoot))
                    continue;

                const fs::path candidate = (resourceRoot.parent_path() / L"ShaderFiles" / L"Shader_UI.hlsl").lexically_normal();
                if (fs::exists(candidate))
                    return candidate.wstring();
            }

            const vector<fs::path> fallbackCandidates = {
                L"..\\Bin\\ShaderFiles\\Shader_UI.hlsl",
                L"..\\..\\Client\\Bin\\ShaderFiles\\Shader_UI.hlsl",
                L"..\\..\\..\\Client\\Bin\\ShaderFiles\\Shader_UI.hlsl",
                L"Client\\Bin\\ShaderFiles\\Shader_UI.hlsl",
            };

            for (const fs::path& candidate : fallbackCandidates)
            {
                try
                {
                    const fs::path normalizedCandidate = fs::absolute(candidate).lexically_normal();
                    if (fs::exists(normalizedCandidate))
                        return normalizedCandidate.wstring();
                }
                catch (...)
                {
                }
            }

            return {};
        }

        string Make_MaskedTexturePreviewKey(const string& textureGuid, const string& maskTextureGuid)
        {
            return textureGuid + "|" + maskTextureGuid;
        }

        float Resolve_FontPreviewScaleCorrectionFromPath(const wstring& fontFilePath)
        {
            if (fontFilePath.empty())
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

        bool Try_GetTexture2DDesc(ID3D11ShaderResourceView* srv, D3D11_TEXTURE2D_DESC& outDesc)
        {
            if (nullptr == srv)
                return false;

            ComPtr<ID3D11Resource> resource = nullptr;
            srv->GetResource(resource.GetAddressOf());
            if (nullptr == resource)
                return false;

            ComPtr<ID3D11Texture2D> texture = nullptr;
            if (FAILED(resource.As(&texture)) || nullptr == texture)
                return false;

            texture->GetDesc(&outDesc);
            return outDesc.Width > 0 && outDesc.Height > 0;
        }

        void Compute_MaskedPreviewSize(const D3D11_TEXTURE2D_DESC& sourceDesc, uint32& outWidth, uint32& outHeight)
        {
            constexpr uint32 kMaxPreviewTextureSize = 1024;

            outWidth = max(1u, sourceDesc.Width);
            outHeight = max(1u, sourceDesc.Height);

            const uint32 maxAxis = max(outWidth, outHeight);
            if (maxAxis <= kMaxPreviewTextureSize)
                return;

            const float scale = static_cast<float>(kMaxPreviewTextureSize) / static_cast<float>(maxAxis);
            outWidth = max(1u, static_cast<uint32>(roundf(static_cast<float>(outWidth) * scale)));
            outHeight = max(1u, static_cast<uint32>(roundf(static_cast<float>(outHeight) * scale)));
        }
    }

    HRESULT UI_Editor_PreviewService::Initialize(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        bool assetManagerReady)
    {
        _device = device;
        _context = context;
        _assetManagerReady = assetManagerReady;

        if (nullptr != _context)
            _fontPreviewSpriteBatch = make_unique<SpriteBatch>(_context.Get());

        if (nullptr != _device)
        {
            D3D11_RASTERIZER_DESC rasterizerDesc = {};
            rasterizerDesc.FillMode = D3D11_FILL_SOLID;
            rasterizerDesc.CullMode = D3D11_CULL_NONE;
            rasterizerDesc.ScissorEnable = TRUE;
            rasterizerDesc.DepthClipEnable = TRUE;
            _device->CreateRasterizerState(&rasterizerDesc, _fontPreviewRasterizerState.GetAddressOf());
        }

        return S_OK;
    }

    void UI_Editor_PreviewService::Begin_Frame()
    {
        _fontPreviewCommands.clear();
    }

    void UI_Editor_PreviewService::Clear_RuntimeCaches()
    {
        _textureThumbCache.clear();
        _textureNoThumbGuids.clear();
        _maskedTexturePreviewCache.clear();
        _maskedTexturePreviewFailedKeys.clear();
        _fontPreviewCache.clear();
        _fontPreviewFailedGuids.clear();
        _fontPreviewCommands.clear();
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

        for (const FontPreviewCommand& command : _fontPreviewCommands)
        {
            SpriteFont* font = Get_FontPreview(command.fontGuid, command.fontPath);
            if (nullptr == font || command.text.empty())
                continue;

            const wstring safeText = Build_SafeSpriteFontText(*font, command.text);
            if (safeText.empty())
                continue;

            bool isBatchOpen = false;
            try
            {
                _context->RSSetScissorRects(1, &command.clipRect);

                _fontPreviewSpriteBatch->Begin(
                    SpriteSortMode_Deferred,
                    nullptr,
                    nullptr,
                    nullptr,
                    _fontPreviewRasterizerState.Get(),
                    nullptr,
                    XMMatrixIdentity());
                isBatchOpen = true;

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
        }
    }

    ID3D11ShaderResourceView* UI_Editor_PreviewService::Get_TexturePreviewSRV(const string& textureGuid)
    {
        if (!_assetManagerReady || textureGuid.empty() || nullptr == _device || nullptr == _context)
            return nullptr;

        const auto cachedIter = _textureThumbCache.find(textureGuid);
        if (cachedIter != _textureThumbCache.end())
            return cachedIter->second.Get();

        if (_textureNoThumbGuids.find(textureGuid) != _textureNoThumbGuids.end())
            return nullptr;

        const wstring texturePath = GAME->Resolve_AssetPath(textureGuid);
        if (texturePath.empty())
        {
            _textureNoThumbGuids.insert(textureGuid);
            return nullptr;
        }

        Shared<Texture> texture = Texture::Create(_device, _context, texturePath.c_str(), 1);
        if (nullptr == texture || texture->Get_SRVs().empty() || nullptr == texture->Get_SRVs().front().Get())
        {
            _textureNoThumbGuids.insert(textureGuid);
            return nullptr;
        }

        _textureThumbCache.emplace(textureGuid, texture->Get_SRVs().front());
        return _textureThumbCache[textureGuid].Get();
    }

    ID3D11ShaderResourceView* UI_Editor_PreviewService::Get_MaskedTexturePreviewSRV(
        const string& textureGuid,
        const string& maskTextureGuid)
    {
        if (!_assetManagerReady || textureGuid.empty() || maskTextureGuid.empty() || nullptr == _device || nullptr == _context)
            return nullptr;

        const string cacheKey = Make_MaskedTexturePreviewKey(textureGuid, maskTextureGuid);
        const auto cachedIter = _maskedTexturePreviewCache.find(cacheKey);
        if (cachedIter != _maskedTexturePreviewCache.end())
            return cachedIter->second.Get();

        if (_maskedTexturePreviewFailedKeys.find(cacheKey) != _maskedTexturePreviewFailedKeys.end())
            return nullptr;

        ID3D11ShaderResourceView* textureSRV = Get_TexturePreviewSRV(textureGuid);
        ID3D11ShaderResourceView* maskSRV = Get_TexturePreviewSRV(maskTextureGuid);
        if (nullptr == textureSRV || nullptr == maskSRV)
        {
            _maskedTexturePreviewFailedKeys.insert(cacheKey);
            return nullptr;
        }

        return Create_MaskedTexturePreviewSRV(cacheKey, textureSRV, maskSRV);
    }

    bool UI_Editor_PreviewService::Queue_FontPreview(
        const string& fontGuid,
        const string& fontPath,
        const wstring& text,
        const Vec2& boxMin,
        const Vec2& boxMax,
        const Vec4& color,
        float rotationRadians,
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
        const Vec2 boxCenter(
            boxMin.x + max(0.f, boxMax.x - boxMin.x) * 0.5f,
            boxMin.y + max(0.f, boxMax.y - boxMin.y) * 0.5f);
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
                command.position = boxCenter;
                command.color = color;
                command.rotationRadians = rotationRadians;
                command.pivot = Vec2((boxCenter.x - textX) / drawScale, (boxCenter.y - textY) / drawScale);
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

    bool UI_Editor_PreviewService::Ensure_ImagePreviewRenderer()
    {
        if (nullptr != _imagePreviewShader && nullptr != _imagePreviewVIBuffer)
            return true;

        if (nullptr == _device || nullptr == _context)
            return false;

        const wstring shaderPath = Resolve_UIShaderPath();
        if (shaderPath.empty())
        {
            LOG_WARN("UI Editor masked preview shader path could not be resolved.");
            return false;
        }

        _imagePreviewShader = ShaderCom::Create(_device, _context, shaderPath.c_str(), VTXTEX::Elements, VTXTEX::numElements, ShaderType::UI);
        _imagePreviewVIBuffer = VIBuffer_Rect::Create(_device, _context);

        if (nullptr == _imagePreviewShader || nullptr == _imagePreviewVIBuffer)
        {
            _imagePreviewShader.reset();
            _imagePreviewVIBuffer.reset();
            LOG_WARN("UI Editor masked preview renderer create failed.");
            return false;
        }

        return true;
    }

    ID3D11ShaderResourceView* UI_Editor_PreviewService::Create_MaskedTexturePreviewSRV(
        const string& cacheKey,
        ID3D11ShaderResourceView* textureSRV,
        ID3D11ShaderResourceView* maskSRV)
    {
        if (cacheKey.empty() || nullptr == textureSRV || nullptr == maskSRV)
            return nullptr;

        if (!Ensure_ImagePreviewRenderer())
        {
            _maskedTexturePreviewFailedKeys.insert(cacheKey);
            return nullptr;
        }

        D3D11_TEXTURE2D_DESC sourceDesc = {};
        if (!Try_GetTexture2DDesc(textureSRV, sourceDesc))
        {
            _maskedTexturePreviewFailedKeys.insert(cacheKey);
            return nullptr;
        }

        uint32 previewWidth = 1;
        uint32 previewHeight = 1;
        Compute_MaskedPreviewSize(sourceDesc, previewWidth, previewHeight);

        D3D11_TEXTURE2D_DESC renderTextureDesc = {};
        renderTextureDesc.Width = previewWidth;
        renderTextureDesc.Height = previewHeight;
        renderTextureDesc.MipLevels = 1;
        renderTextureDesc.ArraySize = 1;
        renderTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        renderTextureDesc.SampleDesc.Count = 1;
        renderTextureDesc.Usage = D3D11_USAGE_DEFAULT;
        renderTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        ComPtr<ID3D11Texture2D> renderTexture = nullptr;
        if (FAILED(_device->CreateTexture2D(&renderTextureDesc, nullptr, renderTexture.GetAddressOf())))
        {
            _maskedTexturePreviewFailedKeys.insert(cacheKey);
            return nullptr;
        }

        ComPtr<ID3D11RenderTargetView> renderTargetView = nullptr;
        if (FAILED(_device->CreateRenderTargetView(renderTexture.Get(), nullptr, renderTargetView.GetAddressOf())))
        {
            _maskedTexturePreviewFailedKeys.insert(cacheKey);
            return nullptr;
        }

        ComPtr<ID3D11ShaderResourceView> previewSRV = nullptr;
        if (FAILED(_device->CreateShaderResourceView(renderTexture.Get(), nullptr, previewSRV.GetAddressOf())))
        {
            _maskedTexturePreviewFailedKeys.insert(cacheKey);
            return nullptr;
        }

        ComPtr<ID3D11RenderTargetView> previousRTV = nullptr;
        ComPtr<ID3D11DepthStencilView> previousDSV = nullptr;
        _context->OMGetRenderTargets(1, previousRTV.GetAddressOf(), previousDSV.GetAddressOf());

        UINT previousViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        D3D11_VIEWPORT previousViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
        _context->RSGetViewports(&previousViewportCount, previousViewports);

        const float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
        _context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);
        _context->ClearRenderTargetView(renderTargetView.Get(), clearColor);

        D3D11_VIEWPORT previewViewport = {};
        previewViewport.TopLeftX = 0.f;
        previewViewport.TopLeftY = 0.f;
        previewViewport.Width = static_cast<float>(previewWidth);
        previewViewport.Height = static_cast<float>(previewHeight);
        previewViewport.MinDepth = 0.f;
        previewViewport.MaxDepth = 1.f;
        _context->RSSetViewports(1, &previewViewport);

        Matrix worldMatrix = Matrix::CreateScale(static_cast<float>(previewWidth), static_cast<float>(previewHeight), 1.f);
        Matrix viewMatrix = Matrix::Identity;
        Matrix projMatrix;
        XMStoreFloat4x4(&projMatrix, XMMatrixOrthographicLH(static_cast<float>(previewWidth), static_cast<float>(previewHeight), 0.f, 1.f));
        const Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);

        bool renderSucceeded =
            SUCCEEDED(_imagePreviewShader->Bind_Matrix("g_WorldMatrix", &worldMatrix)) &&
            SUCCEEDED(_imagePreviewShader->Bind_Matrix("g_ViewMatrix", &viewMatrix)) &&
            SUCCEEDED(_imagePreviewShader->Bind_Matrix("g_ProjMatrix", &projMatrix)) &&
            SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_Color", &color, sizeof(Vec4))) &&
            SUCCEEDED(_imagePreviewShader->Bind_SRV("g_Texture", textureSRV)) &&
            SUCCEEDED(_imagePreviewShader->Bind_SRV("g_MaskTexture", maskSRV)) &&
            SUCCEEDED(_imagePreviewShader->Begin(1));

        if (renderSucceeded)
        {
            const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
            _context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);

            renderSucceeded =
                SUCCEEDED(_imagePreviewVIBuffer->Bind_Resources()) &&
                SUCCEEDED(_imagePreviewVIBuffer->Render());
        }

        ID3D11RenderTargetView* restoredRTVs[] = { previousRTV.Get() };
        _context->OMSetRenderTargets(1, restoredRTVs, previousDSV.Get());
        if (previousViewportCount > 0)
            _context->RSSetViewports(previousViewportCount, previousViewports);

        if (!renderSucceeded)
        {
            _maskedTexturePreviewFailedKeys.insert(cacheKey);
            return nullptr;
        }

        _maskedTexturePreviewCache.emplace(cacheKey, previewSRV);
        return _maskedTexturePreviewCache[cacheKey].Get();
    }
}

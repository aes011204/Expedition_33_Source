#include "pch.h"
#include "UI_Editor_PreviewService.h"

#include "ShaderCom.h"
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

        string Make_TexturePreviewGuidKey(const string& textureGuid)
        {
            if (textureGuid.empty())
                return {};

            return "guid:" + textureGuid;
        }

        string Make_TexturePreviewPathKey(const wstring& texturePath)
        {
            if (texturePath.empty())
                return {};

            wstring normalizedPath;
            try
            {
                normalizedPath = fs::path(texturePath).lexically_normal().wstring();
            }
            catch (...)
            {
                normalizedPath = texturePath;
            }

            replace(normalizedPath.begin(), normalizedPath.end(), L'\\', L'/');
            transform(normalizedPath.begin(), normalizedPath.end(), normalizedPath.begin(), [](wchar_t character)
                {
                    return static_cast<wchar_t>(towlower(character));
                });

            return "path:" + String::ToString(normalizedPath);
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

        class CompositePreviewRenderStateGuard final
        {
        public:
            explicit CompositePreviewRenderStateGuard(Context* context)
                : _context{ context }
            {
                if (nullptr == _context)
                    return;

                _context->OMGetRenderTargets(1, _renderTargetView.GetAddressOf(), _depthStencilView.GetAddressOf());
                _context->RSGetViewports(&_viewportCount, _viewports);
                _context->RSGetScissorRects(&_scissorCount, _scissorRects);
                _context->OMGetBlendState(_blendState.GetAddressOf(), _blendFactor, &_sampleMask);
                _context->OMGetDepthStencilState(_depthStencilState.GetAddressOf(), &_stencilRef);
                _context->RSGetState(_rasterizerState.GetAddressOf());

                ID3D11ShaderResourceView* shaderResourceViews[3] = {};
                _context->PSGetShaderResources(0, 3, shaderResourceViews);
                for (uint32 index = 0; index < 3; ++index)
                    _pixelShaderSRVs[index].Attach(shaderResourceViews[index]);
            }

            ~CompositePreviewRenderStateGuard()
            {
                if (nullptr == _context)
                    return;

                ID3D11RenderTargetView* renderTargetViews[1] = { _renderTargetView.Get() };
                _context->OMSetRenderTargets(1, renderTargetViews, _depthStencilView.Get());

                if (_viewportCount > 0)
                    _context->RSSetViewports(_viewportCount, _viewports);

                if (_scissorCount > 0)
                    _context->RSSetScissorRects(_scissorCount, _scissorRects);

                _context->OMSetBlendState(_blendState.Get(), _blendFactor, _sampleMask);
                _context->OMSetDepthStencilState(_depthStencilState.Get(), _stencilRef);
                _context->RSSetState(_rasterizerState.Get());

                ID3D11ShaderResourceView* shaderResourceViews[3] = {
                    _pixelShaderSRVs[0].Get(),
                    _pixelShaderSRVs[1].Get(),
                    _pixelShaderSRVs[2].Get(),
                };
                _context->PSSetShaderResources(0, 3, shaderResourceViews);
            }

        private:
            Context* _context = nullptr;
            ComPtr<ID3D11RenderTargetView> _renderTargetView = nullptr;
            ComPtr<ID3D11DepthStencilView> _depthStencilView = nullptr;
            UINT _viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            D3D11_VIEWPORT _viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
            UINT _scissorCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            D3D11_RECT _scissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
            ComPtr<ID3D11BlendState> _blendState = nullptr;
            FLOAT _blendFactor[4] = {};
            UINT _sampleMask = 0xffffffff;
            ComPtr<ID3D11DepthStencilState> _depthStencilState = nullptr;
            UINT _stencilRef = 0;
            ComPtr<ID3D11RasterizerState> _rasterizerState = nullptr;
            ComPtr<ID3D11ShaderResourceView> _pixelShaderSRVs[3] = {};
        };
    }

    ID3D11ShaderResourceView* UI_Editor_PreviewService::Get_MaskedTexturePreviewSRV(
        const string& textureGuid,
        const string& texturePath,
        const string& maskTextureGuid,
        const string& maskTexturePath,
        UI_MaskTextureMode maskTextureMode)
    {
        if (nullptr == _device || nullptr == _context)
            return nullptr;

        if ((textureGuid.empty() && texturePath.empty()) || (maskTextureGuid.empty() && maskTexturePath.empty()))
            return nullptr;

        const string cacheKey = Make_MaskedTexturePreviewKey(
            Make_TexturePreviewGuidKey(textureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_ClientRuntimeResourcePath(texturePath)),
            Make_TexturePreviewGuidKey(maskTextureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_ClientRuntimeResourcePath(maskTexturePath)) + "|" + to_string(static_cast<int32>(maskTextureMode)));

        const auto cachedIter = _maskedTexturePreviewCache.find(cacheKey);
        if (cachedIter != _maskedTexturePreviewCache.end())
            return cachedIter->second.Get();

        if (_maskedTexturePreviewFailedKeys.find(cacheKey) != _maskedTexturePreviewFailedKeys.end())
            return nullptr;

        ID3D11ShaderResourceView* textureSRV = Get_TexturePreviewSRV(textureGuid, texturePath);
        ID3D11ShaderResourceView* maskSRV = Get_TexturePreviewSRV(maskTextureGuid, maskTexturePath);
        if (nullptr == textureSRV || nullptr == maskSRV)
        {
            return nullptr;
        }

        return Create_MaskedTexturePreviewSRV(cacheKey, textureSRV, maskSRV, maskTextureMode);
    }

    ID3D11ShaderResourceView* UI_Editor_PreviewService::Get_FillRatioPreviewSRV(
        const string& textureGuid,
        const string& texturePath,
        float fillRatio,
        UI_FillOrigin fillOrigin)
    {
        if (nullptr == _device || nullptr == _context)
            return nullptr;

        if (textureGuid.empty() && texturePath.empty())
            return nullptr;

        const float clampedFillRatio = Clamp_FillRatio(fillRatio);
        const string cacheKey =
            Make_TexturePreviewGuidKey(textureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_ClientRuntimeResourcePath(texturePath)) + "|" +
            to_string(clampedFillRatio) + "|" +
            to_string(static_cast<int32>(fillOrigin));

        const auto cachedIter = _fillRatioPreviewCache.find(cacheKey);
        if (cachedIter != _fillRatioPreviewCache.end())
            return cachedIter->second.Get();

        if (_fillRatioPreviewFailedKeys.find(cacheKey) != _fillRatioPreviewFailedKeys.end())
            return nullptr;

        ID3D11ShaderResourceView* textureSRV = Get_TexturePreviewSRV(textureGuid, texturePath);
        if (nullptr == textureSRV)
            return nullptr;

        return Create_ImageCompositePreviewSRV(
            cacheKey,
            textureSRV,
            nullptr,
            nullptr,
            4u,
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            true,
            clampedFillRatio,
            fillOrigin,
            _fillRatioPreviewCache,
            _fillRatioPreviewFailedKeys);
    }

    ID3D11ShaderResourceView* UI_Editor_PreviewService::Get_PatternOverlayPreviewSRV(
        const PatternOverlayPreviewDesc& previewDesc)
    {
        if (nullptr == _device || nullptr == _context)
            return nullptr;

        if ((previewDesc.textureGuid.empty() && previewDesc.texturePath.empty()) ||
            (previewDesc.paintMaskTextureGuid.empty() && previewDesc.paintMaskTexturePath.empty()) ||
            (previewDesc.patternMaskTextureGuid.empty() && previewDesc.patternMaskTexturePath.empty()))
        {
            return nullptr;
        }

        const string cacheKey =
            Make_TexturePreviewGuidKey(previewDesc.textureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_ClientRuntimeResourcePath(previewDesc.texturePath)) + "|" +
            Make_TexturePreviewGuidKey(previewDesc.paintMaskTextureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_ClientRuntimeResourcePath(previewDesc.paintMaskTexturePath)) + "|" +
            Make_TexturePreviewGuidKey(previewDesc.patternMaskTextureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_ClientRuntimeResourcePath(previewDesc.patternMaskTexturePath)) + "|" +
            to_string(previewDesc.patternUvOffset.x) + "," + to_string(previewDesc.patternUvOffset.y) + "|" +
            to_string(previewDesc.patternUvScale.x) + "," + to_string(previewDesc.patternUvScale.y) + "|" +
            to_string(previewDesc.useFillRatio ? 1 : 0) + "|" +
            to_string(previewDesc.fillRatio) + "|" +
            to_string(static_cast<int32>(previewDesc.fillOrigin));

        const auto cachedIter = _patternOverlayPreviewCache.find(cacheKey);
        if (cachedIter != _patternOverlayPreviewCache.end())
            return cachedIter->second.Get();

        if (_patternOverlayPreviewFailedKeys.find(cacheKey) != _patternOverlayPreviewFailedKeys.end())
            return nullptr;

        ID3D11ShaderResourceView* textureSRV = Get_TexturePreviewSRV(previewDesc.textureGuid, previewDesc.texturePath);
        ID3D11ShaderResourceView* paintMaskSRV = Get_TexturePreviewSRV(previewDesc.paintMaskTextureGuid, previewDesc.paintMaskTexturePath);
        ID3D11ShaderResourceView* patternMaskSRV = Get_TexturePreviewSRV(previewDesc.patternMaskTextureGuid, previewDesc.patternMaskTexturePath);
        if (nullptr == textureSRV || nullptr == paintMaskSRV || nullptr == patternMaskSRV)
        {
            return nullptr;
        }

        return Create_ImageCompositePreviewSRV(
            cacheKey,
            textureSRV,
            paintMaskSRV,
            patternMaskSRV,
            previewDesc.useFillRatio ? 6u : 3u,
            previewDesc.patternUvOffset,
            previewDesc.patternUvScale,
            previewDesc.useFillRatio,
            previewDesc.fillRatio,
            previewDesc.fillOrigin,
            _patternOverlayPreviewCache,
            _patternOverlayPreviewFailedKeys);
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

    bool UI_Editor_PreviewService::Can_CreateCompositePreviewThisFrame() const
    {
        constexpr int32 kMaxCompositePreviewCreateCountPerFrame = 3;
        return _compositePreviewCreateCountThisFrame < kMaxCompositePreviewCreateCountPerFrame;
    }

    ID3D11ShaderResourceView* UI_Editor_PreviewService::Create_MaskedTexturePreviewSRV(
        const string& cacheKey,
        ID3D11ShaderResourceView* textureSRV,
        ID3D11ShaderResourceView* maskSRV,
        UI_MaskTextureMode maskTextureMode)
    {
        const uint32 passIndex = (maskTextureMode == UI_MaskTextureMode::BlackWhite) ? 5u : 1u;
        return Create_ImageCompositePreviewSRV(
            cacheKey,
            textureSRV,
            maskSRV,
            nullptr,
            passIndex,
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            false,
            1.f,
            UI_FillOrigin::LeftToRight,
            _maskedTexturePreviewCache,
            _maskedTexturePreviewFailedKeys);
    }

    ID3D11ShaderResourceView* UI_Editor_PreviewService::Create_ImageCompositePreviewSRV(
        const string& cacheKey,
        ID3D11ShaderResourceView* textureSRV,
        ID3D11ShaderResourceView* maskSRV,
        ID3D11ShaderResourceView* patternMaskSRV,
        uint32 passIndex,
        const ImVec2& patternUvOffset,
        const ImVec2& patternUvScale,
        bool useFillRatio,
        float fillRatio,
        UI_FillOrigin fillOrigin,
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>>& cache,
        unordered_set<string>& failedKeys)
    {
        if (cacheKey.empty() || nullptr == textureSRV)
            return nullptr;

        if (passIndex != 4u && nullptr == maskSRV)
            return nullptr;

        if (!Can_CreateCompositePreviewThisFrame())
            return nullptr;

        ++_compositePreviewCreateCountThisFrame;

        if (!Ensure_ImagePreviewRenderer())
        {
            failedKeys.insert(cacheKey);
            return nullptr;
        }

        D3D11_TEXTURE2D_DESC sourceDesc = {};
        if (!Try_GetTexture2DDesc(textureSRV, sourceDesc))
        {
            failedKeys.insert(cacheKey);
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
            failedKeys.insert(cacheKey);
            return nullptr;
        }

        ComPtr<ID3D11RenderTargetView> renderTargetView = nullptr;
        if (FAILED(_device->CreateRenderTargetView(renderTexture.Get(), nullptr, renderTargetView.GetAddressOf())))
        {
            failedKeys.insert(cacheKey);
            return nullptr;
        }

        ComPtr<ID3D11ShaderResourceView> previewSRV = nullptr;
        if (FAILED(_device->CreateShaderResourceView(renderTexture.Get(), nullptr, previewSRV.GetAddressOf())))
        {
            failedKeys.insert(cacheKey);
            return nullptr;
        }

        CompositePreviewRenderStateGuard renderStateGuard{ _context.Get() };

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
        const float clampedFillRatio = Clamp_FillRatio(fillRatio);
        const uint32 fillOriginValue = static_cast<uint32>(fillOrigin);

        bool renderSucceeded =
            SUCCEEDED(_imagePreviewShader->Bind_Matrix("g_WorldMatrix", &worldMatrix)) &&
            SUCCEEDED(_imagePreviewShader->Bind_Matrix("g_ViewMatrix", &viewMatrix)) &&
            SUCCEEDED(_imagePreviewShader->Bind_Matrix("g_ProjMatrix", &projMatrix)) &&
            SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_Color", &color, sizeof(Vec4))) &&
            SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_PatternUvOffset", &patternUvOffset, sizeof(ImVec2))) &&
            SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_PatternUvScale", &patternUvScale, sizeof(ImVec2))) &&
            SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_UseFillRatio", &useFillRatio, sizeof(bool))) &&
            SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_FillRatio", &clampedFillRatio, sizeof(float))) &&
            SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_FillOrigin", &fillOriginValue, sizeof(uint32))) &&
            SUCCEEDED(_imagePreviewShader->Bind_SRV("g_Texture", textureSRV));

        if (renderSucceeded && nullptr != maskSRV)
            renderSucceeded = SUCCEEDED(_imagePreviewShader->Bind_SRV("g_MaskTexture", maskSRV));

        if (renderSucceeded && nullptr != patternMaskSRV)
            renderSucceeded = SUCCEEDED(_imagePreviewShader->Bind_SRV("g_PatternMaskTexture", patternMaskSRV));

        if (renderSucceeded)
            renderSucceeded = SUCCEEDED(_imagePreviewShader->Begin(passIndex));

        if (renderSucceeded)
        {
            const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
            _context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);

            renderSucceeded =
                SUCCEEDED(_imagePreviewVIBuffer->Bind_Resources()) &&
                SUCCEEDED(_imagePreviewVIBuffer->Render());
        }

        if (!renderSucceeded)
        {
            failedKeys.insert(cacheKey);
            return nullptr;
        }

        cache.emplace(cacheKey, previewSRV);
        return cache[cacheKey].Get();
    }
}

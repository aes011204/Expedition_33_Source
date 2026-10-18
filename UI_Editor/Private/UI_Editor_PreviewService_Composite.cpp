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
            Make_TexturePreviewGuidKey(textureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_TexturePreviewPath(texturePath)),
            Make_TexturePreviewGuidKey(maskTextureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_TexturePreviewPath(maskTexturePath)) + "|" + to_string(static_cast<int32>(maskTextureMode)));

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
            Make_TexturePreviewGuidKey(textureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_TexturePreviewPath(texturePath)) + "|" +
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
            nullptr,
            0.f,
            false,
            false,
            nullptr,
            0.f,
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
            Make_TexturePreviewGuidKey(previewDesc.textureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_TexturePreviewPath(previewDesc.texturePath)) + "|" +
            Make_TexturePreviewGuidKey(previewDesc.paintMaskTextureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_TexturePreviewPath(previewDesc.paintMaskTexturePath)) + "|" +
            Make_TexturePreviewGuidKey(previewDesc.patternMaskTextureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_TexturePreviewPath(previewDesc.patternMaskTexturePath)) + "|" +
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
            nullptr,
            0.f,
            false,
            false,
            nullptr,
            0.f,
            _patternOverlayPreviewCache,
            _patternOverlayPreviewFailedKeys);
    }

    ID3D11ShaderResourceView* UI_Editor_PreviewService::Get_MaterialVfxPreviewSRV(
        const MaterialVfxPreviewDesc& previewDesc)
    {
        if (nullptr == _device || nullptr == _context)
            return nullptr;

        if (previewDesc.textureGuid.empty() && previewDesc.texturePath.empty())
            return nullptr;

        const int32 timeFrame = static_cast<int32>(floorf(_materialVfxPreviewTime * 30.f));
        const string cacheKey =
            Make_TexturePreviewGuidKey(previewDesc.textureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_TexturePreviewPath(previewDesc.texturePath)) + "|" +
            Make_TexturePreviewGuidKey(previewDesc.maskTextureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_TexturePreviewPath(previewDesc.maskTexturePath)) + "|" +
            Make_TexturePreviewGuidKey(previewDesc.materialVfx.noiseTextureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_TexturePreviewPath(previewDesc.materialVfx.noiseTexturePath)) + "|" +
            to_string(timeFrame) + "|" +
            to_string(static_cast<int32>(previewDesc.materialVfx.mode)) + "|" +
            to_string(previewDesc.renderColor.x) + "," + to_string(previewDesc.renderColor.y) + "," + to_string(previewDesc.renderColor.z) + "," + to_string(previewDesc.renderColor.w) + "|" +
            to_string(previewDesc.materialVfx.color.x) + "," + to_string(previewDesc.materialVfx.color.y) + "," + to_string(previewDesc.materialVfx.color.z) + "," + to_string(previewDesc.materialVfx.color.w) + "|" +
            to_string(previewDesc.materialVfx.coreColor.x) + "," + to_string(previewDesc.materialVfx.coreColor.y) + "," + to_string(previewDesc.materialVfx.coreColor.z) + "," + to_string(previewDesc.materialVfx.coreColor.w) + "|" +
            to_string(previewDesc.materialVfx.flowColor.x) + "," + to_string(previewDesc.materialVfx.flowColor.y) + "," + to_string(previewDesc.materialVfx.flowColor.z) + "," + to_string(previewDesc.materialVfx.flowColor.w) + "|" +
            to_string(previewDesc.materialVfx.glow) + "|" +
            to_string(previewDesc.materialVfx.alpha) + "|" +
            to_string(previewDesc.materialVfx.coreStrength) + "|" +
            to_string(previewDesc.materialVfx.coreWidth) + "|" +
            to_string(previewDesc.materialVfx.coreSoftness) + "|" +
            to_string(previewDesc.materialVfx.noiseStrength) + "|" +
            to_string(previewDesc.materialVfx.uvScroll.x) + "," + to_string(previewDesc.materialVfx.uvScroll.y) + "|" +
            to_string(previewDesc.materialVfx.flowSpeed) + "|" +
            to_string(previewDesc.materialVfx.preserveColor) + "|" +
            to_string(previewDesc.materialVfx.edgeWidth) + "|" +
            to_string(previewDesc.materialVfx.edgeSoftness) + "|" +
            to_string(previewDesc.materialVfx.erosion) + "|" +
            to_string(previewDesc.materialVfx.flicker) + "|" +
            to_string(previewDesc.materialVfx.distortion) + "|" +
            to_string(previewDesc.materialVfx.edgeWobbleSpeed);

        const auto cachedIter = _materialVfxPreviewCache.find(cacheKey);
        if (cachedIter != _materialVfxPreviewCache.end())
            return cachedIter->second.Get();

        if (_materialVfxPreviewFailedKeys.find(cacheKey) != _materialVfxPreviewFailedKeys.end())
            return nullptr;

        ID3D11ShaderResourceView* textureSRV = Get_TexturePreviewSRV(previewDesc.textureGuid, previewDesc.texturePath);
        if (nullptr == textureSRV)
            return nullptr;

        ID3D11ShaderResourceView* maskSRV = Get_TexturePreviewSRV(previewDesc.maskTextureGuid, previewDesc.maskTexturePath);
        const bool useBaseMask = nullptr == maskSRV;
        if (nullptr == maskSRV)
            maskSRV = textureSRV;

        ID3D11ShaderResourceView* noiseSRV = Get_TexturePreviewSRV(previewDesc.materialVfx.noiseTextureGuid, previewDesc.materialVfx.noiseTexturePath);
        const bool hasNoise = nullptr != noiseSRV;
        if (nullptr == noiseSRV)
            noiseSRV = textureSRV;

        return Create_ImageCompositePreviewSRV(
            cacheKey,
            textureSRV,
            maskSRV,
            noiseSRV,
            7u,
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            false,
            1.f,
            UI_FillOrigin::LeftToRight,
            &previewDesc.materialVfx,
            _materialVfxPreviewTime,
            useBaseMask,
            hasNoise,
            nullptr,
            0.f,
            _materialVfxPreviewCache,
            _materialVfxPreviewFailedKeys,
            Vec4(previewDesc.renderColor.x, previewDesc.renderColor.y, previewDesc.renderColor.z, previewDesc.renderColor.w));
    }

    ID3D11ShaderResourceView* UI_Editor_PreviewService::Get_ButtonSheenPreviewSRV(
        const ButtonSheenPreviewDesc& previewDesc)
    {
        if (nullptr == _device || nullptr == _context)
            return nullptr;

        if (previewDesc.textureGuid.empty() && previewDesc.texturePath.empty())
            return nullptr;

        const int32 timeFrame = static_cast<int32>(floorf(_buttonSheenPreviewTime * 30.f));
        const string cacheKey =
            Make_TexturePreviewGuidKey(previewDesc.textureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_TexturePreviewPath(previewDesc.texturePath)) + "|" +
            Make_TexturePreviewGuidKey(previewDesc.maskTextureGuid) + "|" + Make_TexturePreviewPathKey(Resolve_TexturePreviewPath(previewDesc.maskTexturePath)) + "|" +
            to_string(timeFrame) + "|" +
            to_string(previewDesc.buttonSheen.color.x) + "," + to_string(previewDesc.buttonSheen.color.y) + "," + to_string(previewDesc.buttonSheen.color.z) + "," + to_string(previewDesc.buttonSheen.color.w) + "|" +
            to_string(previewDesc.buttonSheen.speed) + "|" +
            to_string(previewDesc.buttonSheen.width) + "|" +
            to_string(previewDesc.buttonSheen.softness) + "|" +
            to_string(previewDesc.buttonSheen.angle) + "|" +
            to_string(previewDesc.buttonSheen.noiseStrength);

        const auto cachedIter = _buttonSheenPreviewCache.find(cacheKey);
        if (cachedIter != _buttonSheenPreviewCache.end())
            return cachedIter->second.Get();

        if (_buttonSheenPreviewFailedKeys.find(cacheKey) != _buttonSheenPreviewFailedKeys.end())
            return nullptr;

        ID3D11ShaderResourceView* textureSRV = Get_TexturePreviewSRV(previewDesc.textureGuid, previewDesc.texturePath);
        if (nullptr == textureSRV)
            return nullptr;

        ID3D11ShaderResourceView* maskSRV = Get_TexturePreviewSRV(previewDesc.maskTextureGuid, previewDesc.maskTexturePath);
        if (nullptr == maskSRV)
            maskSRV = textureSRV;

        return Create_ImageCompositePreviewSRV(
            cacheKey,
            textureSRV,
            maskSRV,
            nullptr,
            8u,
            ImVec2(0.f, 0.f),
            ImVec2(1.f, 1.f),
            false,
            1.f,
            UI_FillOrigin::LeftToRight,
            nullptr,
            0.f,
            false,
            false,
            &previewDesc.buttonSheen,
            _buttonSheenPreviewTime,
            _buttonSheenPreviewCache,
            _buttonSheenPreviewFailedKeys);
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
        // Button effect prefabs use three MaterialVfx layers, and multiple instances can be visible at once.
        constexpr int32 kMaxCompositePreviewCreateCountPerFrame = 16;
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
            nullptr,
            0.f,
            false,
            false,
            nullptr,
            0.f,
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
        const UI_MaterialVfxDesc* materialVfx,
        float materialVfxTime,
        bool materialVfxUseBaseMask,
        bool materialVfxHasNoise,
        const UI_ButtonSheenDesc* buttonSheen,
        float buttonSheenTime,
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>>& cache,
        unordered_set<string>& failedKeys,
        const Vec4& renderColor)
    {
        if (cacheKey.empty() || nullptr == textureSRV)
            return nullptr;

        if (passIndex != 4u && nullptr == maskSRV)
            return nullptr;

        constexpr size_t kMaxCompositePreviewCacheEntries = 256;
        if (cache.size() >= kMaxCompositePreviewCacheEntries)
        {
            LOG_INFO(
                "[UIEditor][PreviewLeakWatch] composite preview cache cap reached. pass={} size={} failedKeys={}",
                passIndex,
                cache.size(),
                failedKeys.size());
            cache.clear();
            failedKeys.clear();
        }

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

        const uint64 nextCompositeCreateCount = _totalCompositePreviewCreateCount + 1;
        if (nextCompositeCreateCount <= 8 || nextCompositeCreateCount % 120 == 0)
        {
            string logKey = cacheKey;
            constexpr size_t kMaxLoggedCompositeKeyLength = 180;
            if (logKey.size() > kMaxLoggedCompositeKeyLength)
                logKey = logKey.substr(0, kMaxLoggedCompositeKeyLength) + "...";

            LOG_INFO(
                "[UIEditor][PreviewLeakWatch] composite preview create. total={} pass={} size={}x{} cacheSize={} key='{}'",
                nextCompositeCreateCount,
                passIndex,
                previewWidth,
                previewHeight,
                cache.size(),
                logKey);
        }

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
        const float clampedFillRatio = Clamp_FillRatio(fillRatio);
        const uint32 fillOriginValue = static_cast<uint32>(fillOrigin);

        bool renderSucceeded =
            SUCCEEDED(_imagePreviewShader->Bind_Matrix("g_WorldMatrix", &worldMatrix)) &&
            SUCCEEDED(_imagePreviewShader->Bind_Matrix("g_ViewMatrix", &viewMatrix)) &&
            SUCCEEDED(_imagePreviewShader->Bind_Matrix("g_ProjMatrix", &projMatrix)) &&
            SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_Color", &renderColor, sizeof(Vec4))) &&
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

        if (renderSucceeded && nullptr != materialVfx)
        {
            const Vec4 materialColor(
                materialVfx->color.x,
                materialVfx->color.y,
                materialVfx->color.z,
                materialVfx->color.w);
            const Vec4 materialCoreColor(
                materialVfx->coreColor.x,
                materialVfx->coreColor.y,
                materialVfx->coreColor.z,
                materialVfx->coreColor.w);
            const Vec4 materialFlowColor(
                materialVfx->flowColor.x,
                materialVfx->flowColor.y,
                materialVfx->flowColor.z,
                materialVfx->flowColor.w);
            const Vec2 materialUvScroll(materialVfx->uvScroll.x, materialVfx->uvScroll.y);
            const float materialGlow = Clamp_MaterialVfxGlow(materialVfx->glow);
            const float materialAlpha = Clamp_MaterialVfxAlpha(materialVfx->alpha);
            const float materialCoreStrength = Clamp_MaterialVfxCoreStrength(materialVfx->coreStrength);
            const float materialCoreWidth = Clamp_MaterialVfxCoreWidth(materialVfx->coreWidth);
            const float materialCoreSoftness = Clamp_MaterialVfxCoreSoftness(materialVfx->coreSoftness);
            const float materialNoiseStrength = Clamp_MaterialVfxNoiseStrength(materialVfx->noiseStrength);
            const float materialFlowSpeed = Clamp_MaterialVfxFlowSpeed(materialVfx->flowSpeed);
            const float materialPreserveColor = Clamp_MaterialVfxPreserveColor(materialVfx->preserveColor);
            const float materialEdgeWidth = Clamp_MaterialVfxEdgeWidth(materialVfx->edgeWidth);
            const float materialEdgeSoftness = Clamp_MaterialVfxEdgeSoftness(materialVfx->edgeSoftness);
            const float materialErosion = Clamp_MaterialVfxErosion(materialVfx->erosion);
            const float materialFlicker = Clamp_MaterialVfxFlicker(materialVfx->flicker);
            const float materialDistortion = Clamp_MaterialVfxDistortion(materialVfx->distortion);
            const float materialEdgeWobbleSpeed = Clamp_MaterialVfxEdgeWobbleSpeed(materialVfx->edgeWobbleSpeed);
            const float materialUseBaseMask = materialVfxUseBaseMask ? 1.f : 0.f;
            const float materialHasNoise = materialVfxHasNoise ? 1.f : 0.f;
            const uint32 materialMode = static_cast<uint32>(materialVfx->mode);

            renderSucceeded =
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxMode", &materialMode, sizeof(uint32))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxColor", &materialColor, sizeof(Vec4))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxCoreColor", &materialCoreColor, sizeof(Vec4))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxFlowColor", &materialFlowColor, sizeof(Vec4))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxGlow", &materialGlow, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxAlpha", &materialAlpha, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxCoreStrength", &materialCoreStrength, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxCoreWidth", &materialCoreWidth, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxCoreSoftness", &materialCoreSoftness, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxNoiseStrength", &materialNoiseStrength, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxUvScroll", &materialUvScroll, sizeof(Vec2))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxFlowSpeed", &materialFlowSpeed, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxPreserveColor", &materialPreserveColor, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxEdgeWidth", &materialEdgeWidth, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxEdgeSoftness", &materialEdgeSoftness, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxErosion", &materialErosion, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxFlicker", &materialFlicker, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxDistortion", &materialDistortion, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxEdgeWobbleSpeed", &materialEdgeWobbleSpeed, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxTime", &materialVfxTime, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxUseBaseMask", &materialUseBaseMask, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_MaterialVfxHasNoise", &materialHasNoise, sizeof(float)));
        }

        if (renderSucceeded && nullptr != buttonSheen)
        {
            const Vec4 sheenColor(
                buttonSheen->color.x,
                buttonSheen->color.y,
                buttonSheen->color.z,
                buttonSheen->color.w);
            const float sheenSpeed = Clamp_ButtonSheenSpeed(buttonSheen->speed);
            const float sheenWidth = Clamp_ButtonSheenWidth(buttonSheen->width);
            const float sheenSoftness = Clamp_ButtonSheenSoftness(buttonSheen->softness);
            const float sheenAngle = Clamp_ButtonSheenAngle(buttonSheen->angle);
            const float sheenNoiseStrength = Clamp_ButtonSheenNoiseStrength(buttonSheen->noiseStrength);

            renderSucceeded =
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_ButtonSheenColor", &sheenColor, sizeof(Vec4))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_ButtonSheenSpeed", &sheenSpeed, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_ButtonSheenWidth", &sheenWidth, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_ButtonSheenSoftness", &sheenSoftness, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_ButtonSheenAngle", &sheenAngle, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_ButtonSheenNoiseStrength", &sheenNoiseStrength, sizeof(float))) &&
                SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_ButtonSheenTime", &buttonSheenTime, sizeof(float)));
        }

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

        auto insertResult = cache.emplace(cacheKey, previewSRV);
        if (insertResult.second)
            ++_totalCompositePreviewCreateCount;

        return insertResult.first->second.Get();
    }
}

#include "pch.h"
#include "UI_Editor_PreviewService.h"

namespace UIEditor
{
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
        const size_t clearedMaterialVfxCount = _materialVfxPreviewCache.size();
        const size_t clearedButtonSheenCount = _buttonSheenPreviewCache.size();
        const bool hadPreviewWork =
            _texturePreviewLoadCountThisFrame > 0 ||
            _compositePreviewCreateCountThisFrame > 0 ||
            clearedMaterialVfxCount > 0 ||
            clearedButtonSheenCount > 0;

        if (hadPreviewWork && _previewFrameIndex >= _nextPreviewDiagnosticsFrame)
        {
            Log_PreviewCacheStats("frame", clearedMaterialVfxCount, clearedButtonSheenCount);
            _nextPreviewDiagnosticsFrame = _previewFrameIndex + 300;
        }

        ++_previewFrameIndex;
        _fontPreviewCommands.clear();
        _fontPreviewFrameStats = {};
        _texturePreviewLoadCountThisFrame = 0;
        _compositePreviewCreateCountThisFrame = 0;
        _materialVfxPreviewTime = fmodf(_materialVfxPreviewTime + max(0.f, ImGui::GetIO().DeltaTime), 32.f);
        _materialVfxPreviewCache.clear();
        _materialVfxPreviewFailedKeys.clear();
        _buttonSheenPreviewTime = fmodf(_buttonSheenPreviewTime + max(0.f, ImGui::GetIO().DeltaTime), 32.f);
        _buttonSheenPreviewCache.clear();
        _buttonSheenPreviewFailedKeys.clear();
    }

    void UI_Editor_PreviewService::Clear_RuntimeCaches()
    {
        Log_PreviewCacheStats("clear-runtime", _materialVfxPreviewCache.size(), _buttonSheenPreviewCache.size());

        _resolvedTexturePathCache.clear();
        _resolvedFontPathCache.clear();
        _fontScaleCorrectionCache.clear();
        _safeFontTextCache.clear();
        _fontTextWidthCache.clear();
        _fontLineHeightCache.clear();
        _textureThumbCache.clear();
        _textureThumbCacheOrder.clear();
        _texturePreviewFailedKeys.clear();
        _maskedTexturePreviewCache.clear();
        _maskedTexturePreviewFailedKeys.clear();
        _fillRatioPreviewCache.clear();
        _fillRatioPreviewFailedKeys.clear();
        _patternOverlayPreviewCache.clear();
        _patternOverlayPreviewFailedKeys.clear();
        _materialVfxPreviewCache.clear();
        _materialVfxPreviewFailedKeys.clear();
        _materialVfxPreviewTime = 0.f;
        _buttonSheenPreviewCache.clear();
        _buttonSheenPreviewFailedKeys.clear();
        _buttonSheenPreviewTime = 0.f;
        _fontPreviewCache.clear();
        _fontPreviewCacheOrder.clear();
        _fontPreviewFailedGuids.clear();
        _fontPreviewCommands.clear();
    }

    void UI_Editor_PreviewService::Trim_TexturePreviewCache()
    {
        constexpr size_t kMaxTexturePreviewCacheEntries = 512;
        while (_textureThumbCache.size() > kMaxTexturePreviewCacheEntries && !_textureThumbCacheOrder.empty())
        {
            const string oldestKey = _textureThumbCacheOrder.front();
            _textureThumbCacheOrder.pop_front();
            _textureThumbCache.erase(oldestKey);
        }

        if (_texturePreviewFailedKeys.size() > kMaxTexturePreviewCacheEntries * 2)
            _texturePreviewFailedKeys.clear();
    }

    void UI_Editor_PreviewService::Trim_FontPreviewCache()
    {
        constexpr size_t kMaxFontPreviewCacheEntries = 128;
        while (_fontPreviewCache.size() > kMaxFontPreviewCacheEntries && !_fontPreviewCacheOrder.empty())
        {
            const string oldestKey = _fontPreviewCacheOrder.front();
            _fontPreviewCacheOrder.pop_front();
            _fontPreviewCache.erase(oldestKey);
        }

        if (_fontPreviewFailedGuids.size() > kMaxFontPreviewCacheEntries * 2)
            _fontPreviewFailedGuids.clear();
    }

    void UI_Editor_PreviewService::Log_PreviewCacheStats(
        const char* reason,
        size_t clearedMaterialVfxCount,
        size_t clearedButtonSheenCount) const
    {
        const bool hasAnyCache =
            !_textureThumbCache.empty() ||
            !_maskedTexturePreviewCache.empty() ||
            !_fillRatioPreviewCache.empty() ||
            !_patternOverlayPreviewCache.empty() ||
            !_materialVfxPreviewCache.empty() ||
            !_buttonSheenPreviewCache.empty() ||
            !_fontPreviewCache.empty() ||
            clearedMaterialVfxCount > 0 ||
            clearedButtonSheenCount > 0;

        if (!hasAnyCache &&
            _texturePreviewLoadCountThisFrame == 0 &&
            _compositePreviewCreateCountThisFrame == 0 &&
            _totalTexturePreviewLoadCount == 0 &&
            _totalCompositePreviewCreateCount == 0)
        {
            return;
        }

        LOG_INFO(
            "[UIEditor][PreviewLeakWatch] reason={} frame={} textureLoadsFrame={} compositeCreatesFrame={} textureLoadsTotal={} compositeCreatesTotal={} textureCache={} fontCache={} maskedCache={} fillCache={} patternCache={} materialCache={} buttonCache={} clearedMaterial={} clearedButton={} resolvedTexturePaths={} resolvedFontPaths={} safeFontTextCache={} fontWidthCache={} fontLineHeightCache={}",
            reason ? reason : "",
            _previewFrameIndex,
            _texturePreviewLoadCountThisFrame,
            _compositePreviewCreateCountThisFrame,
            _totalTexturePreviewLoadCount,
            _totalCompositePreviewCreateCount,
            _textureThumbCache.size(),
            _fontPreviewCache.size(),
            _maskedTexturePreviewCache.size(),
            _fillRatioPreviewCache.size(),
            _patternOverlayPreviewCache.size(),
            _materialVfxPreviewCache.size(),
            _buttonSheenPreviewCache.size(),
            clearedMaterialVfxCount,
            clearedButtonSheenCount,
            _resolvedTexturePathCache.size(),
            _resolvedFontPathCache.size(),
            _safeFontTextCache.size(),
            _fontTextWidthCache.size(),
            _fontLineHeightCache.size());
    }
}

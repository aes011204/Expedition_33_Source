#pragma once

#include "UI_Editor_Defines.h"
#include "UI_Editor_Struct.h"

namespace Engine
{
    class ShaderCom;
    class VIBuffer_Rect;
}

namespace UIEditor
{
    struct FontPreviewFrameStats
    {
        size_t requestedTextCount = 0;
        size_t queuedCommandCount = 0;
        size_t sharedClipCommandCount = 0;
        size_t objectClipCommandCount = 0;
        size_t clipGroupCount = 0;
    };

    struct PatternOverlayPreviewDesc
    {
        string textureGuid = "";
        string texturePath = "";
        string paintMaskTextureGuid = "";
        string paintMaskTexturePath = "";
        string patternMaskTextureGuid = "";
        string patternMaskTexturePath = "";
        ImVec2 patternUvOffset = ImVec2(0.f, 0.f);
        ImVec2 patternUvScale = ImVec2(1.f, 1.f);
        bool useFillRatio = false;
        float fillRatio = 1.f;
        UI_FillOrigin fillOrigin = UI_FillOrigin::LeftToRight;
    };

    struct MaterialVfxPreviewDesc
    {
        string textureGuid = "";
        string texturePath = "";
        string maskTextureGuid = "";
        string maskTexturePath = "";
        ImVec4 renderColor = ImVec4(1.f, 1.f, 1.f, 1.f);
        UI_MaterialVfxDesc materialVfx = Get_DefaultMaterialVfxDesc();
    };

    struct ButtonSheenPreviewDesc
    {
        string textureGuid = "";
        string texturePath = "";
        string maskTextureGuid = "";
        string maskTexturePath = "";
        UI_ButtonSheenDesc buttonSheen = Get_DefaultButtonSheenDesc();
    };

    class UI_Editor_PreviewService final
    {
    public:
        UI_Editor_PreviewService() = default;
        ~UI_Editor_PreviewService() = default;

    public:
        HRESULT Initialize(const ComPtr<Device>& device, const ComPtr<Context>& context, bool assetManagerReady);
        void Begin_Frame();
        void Clear_RuntimeCaches();
        void Render_FontPreviews();
        const FontPreviewFrameStats& Get_FontPreviewFrameStats() const { return _fontPreviewFrameStats; }

        ID3D11ShaderResourceView* Get_TexturePreviewSRV(const string& textureGuid, const string& texturePath = "");
        ID3D11ShaderResourceView* Get_MaskedTexturePreviewSRV(
            const string& textureGuid,
            const string& texturePath,
            const string& maskTextureGuid,
            const string& maskTexturePath,
            UI_MaskTextureMode maskTextureMode);
        ID3D11ShaderResourceView* Get_FillRatioPreviewSRV(
            const string& textureGuid,
            const string& texturePath,
            float fillRatio,
            UI_FillOrigin fillOrigin);
        ID3D11ShaderResourceView* Get_PatternOverlayPreviewSRV(
            const PatternOverlayPreviewDesc& previewDesc);
        ID3D11ShaderResourceView* Get_MaterialVfxPreviewSRV(
            const MaterialVfxPreviewDesc& previewDesc);
        ID3D11ShaderResourceView* Get_ButtonSheenPreviewSRV(
            const ButtonSheenPreviewDesc& previewDesc);

        bool Queue_FontPreview(
            const string& fontGuid,
            const string& fontPath,
            const wstring& text,
            const Vec2& boxMin,
            const Vec2& boxMax,
            const Vec4& color,
            float rotationRadians,
            const Vec2& pivot,
            UI_TextAlign textAlign,
            float scale,
            bool textWrap,
            const D3D11_RECT& clipRect,
            const D3D11_RECT* sharedClipRect = nullptr,
            bool allowSharedClip = false);

    private:
        struct FontPreviewCommand
        {
            string fontGuid = "";
            string fontPath = "";
            wstring safeText = L"";
            Vec2 position = Vec2(0.f, 0.f);
            Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
            float rotationRadians = 0.f;
            Vec2 pivot = Vec2(0.f, 0.f);
            float scale = 1.f;
            D3D11_RECT clipRect = {};
        };

    private:
        SpriteFont* Get_FontPreview(const string& fontGuid, const string& fontPath);
        wstring Resolve_TexturePreviewPath(const string& texturePath);
        wstring Resolve_FontPreviewPath(const string& fontPath);
        string Make_FontPreviewCacheKey(const string& fontGuid, const string& fontPath);
        float Resolve_FontPreviewScaleCorrection(const string& fontGuid, const string& fontPath);
        wstring Build_CachedSafeFontPreviewText(const string& fontCacheKey, const SpriteFont& font, const wstring& text);
        float Measure_CachedFontPreviewWidth(const string& fontCacheKey, const SpriteFont& font, const wstring& text, float scale);
        float Measure_CachedFontPreviewLineHeight(const string& fontCacheKey, const SpriteFont& font, float scale);
        ID3D11ShaderResourceView* Load_TexturePreview(const string& cacheKey, const wstring& texturePath);
        bool Can_LoadTexturePreviewThisFrame();
        bool Can_CreateCompositePreviewThisFrame() const;
        void Trim_TexturePreviewCache();
        void Trim_FontPreviewCache();
        void Log_PreviewCacheStats(const char* reason, size_t clearedMaterialVfxCount = 0, size_t clearedButtonSheenCount = 0) const;
        bool Ensure_ImagePreviewRenderer();
        ID3D11ShaderResourceView* Create_MaskedTexturePreviewSRV(
            const string& cacheKey,
            ID3D11ShaderResourceView* textureSRV,
            ID3D11ShaderResourceView* maskSRV,
            UI_MaskTextureMode maskTextureMode);
        ID3D11ShaderResourceView* Create_ImageCompositePreviewSRV(
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
            const Vec4& renderColor = Vec4(1.f, 1.f, 1.f, 1.f));

    private:
        ComPtr<Device> _device = nullptr;
        ComPtr<Context> _context = nullptr;
        bool _assetManagerReady = false;
        int32 _texturePreviewLoadCountThisFrame = 0;
        int32 _compositePreviewCreateCountThisFrame = 0;
        uint64 _previewFrameIndex = 0;
        uint64 _totalTexturePreviewLoadCount = 0;
        uint64 _totalCompositePreviewCreateCount = 0;
        uint64 _nextPreviewDiagnosticsFrame = 0;
        unordered_map<string, wstring> _resolvedTexturePathCache = {};
        unordered_map<string, wstring> _resolvedFontPathCache = {};
        unordered_map<string, float> _fontScaleCorrectionCache = {};
        unordered_map<string, wstring> _safeFontTextCache = {};
        unordered_map<string, float> _fontTextWidthCache = {};
        unordered_map<string, float> _fontLineHeightCache = {};
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _textureThumbCache = {};
        deque<string> _textureThumbCacheOrder = {};
        unordered_set<string> _texturePreviewFailedKeys = {};
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _maskedTexturePreviewCache = {};
        unordered_set<string> _maskedTexturePreviewFailedKeys = {};
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _fillRatioPreviewCache = {};
        unordered_set<string> _fillRatioPreviewFailedKeys = {};
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _patternOverlayPreviewCache = {};
        unordered_set<string> _patternOverlayPreviewFailedKeys = {};
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _materialVfxPreviewCache = {};
        unordered_set<string> _materialVfxPreviewFailedKeys = {};
        float _materialVfxPreviewTime = 0.f;
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _buttonSheenPreviewCache = {};
        unordered_set<string> _buttonSheenPreviewFailedKeys = {};
        float _buttonSheenPreviewTime = 0.f;
        Shared<ShaderCom> _imagePreviewShader = nullptr;
        Shared<VIBuffer_Rect> _imagePreviewVIBuffer = nullptr;
        Unique<SpriteBatch> _fontPreviewSpriteBatch = nullptr;
        ComPtr<ID3D11RasterizerState> _fontPreviewRasterizerState = nullptr;
        unordered_map<string, Unique<SpriteFont>> _fontPreviewCache = {};
        deque<string> _fontPreviewCacheOrder = {};
        unordered_set<string> _fontPreviewFailedGuids = {};
        vector<FontPreviewCommand> _fontPreviewCommands = {};
        FontPreviewFrameStats _fontPreviewFrameStats = {};
    };
}

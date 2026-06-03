#pragma once

#include "UI_Editor_Defines.h"

namespace Engine
{
    class ShaderCom;
    class VIBuffer_Rect;
}

namespace UIEditor
{
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

        ID3D11ShaderResourceView* Get_TexturePreviewSRV(const string& textureGuid, const string& texturePath = "");
        ID3D11ShaderResourceView* Get_MaskedTexturePreviewSRV(
            const string& textureGuid,
            const string& texturePath,
            const string& maskTextureGuid,
            const string& maskTexturePath,
            UI_MaskTextureMode maskTextureMode);
        ID3D11ShaderResourceView* Get_PatternOverlayPreviewSRV(
            const string& textureGuid,
            const string& texturePath,
            const string& paintMaskTextureGuid,
            const string& paintMaskTexturePath,
            const string& patternMaskTextureGuid,
            const string& patternMaskTexturePath);

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
            const D3D11_RECT& clipRect);

    private:
        struct FontPreviewCommand
        {
            string fontGuid = "";
            string fontPath = "";
            wstring text = L"";
            Vec2 position = Vec2(0.f, 0.f);
            Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
            float rotationRadians = 0.f;
            Vec2 pivot = Vec2(0.f, 0.f);
            float scale = 1.f;
            D3D11_RECT clipRect = {};
        };

    private:
        SpriteFont* Get_FontPreview(const string& fontGuid, const string& fontPath);
        ID3D11ShaderResourceView* Load_TexturePreview(const string& cacheKey, const wstring& texturePath);
        bool Can_LoadTexturePreviewThisFrame();
        bool Can_CreateCompositePreviewThisFrame() const;
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
            unordered_map<string, ComPtr<ID3D11ShaderResourceView>>& cache,
            unordered_set<string>& failedKeys);

    private:
        ComPtr<Device> _device = nullptr;
        ComPtr<Context> _context = nullptr;
        bool _assetManagerReady = false;
        int32 _texturePreviewLoadCountThisFrame = 0;
        int32 _compositePreviewCreateCountThisFrame = 0;
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _textureThumbCache = {};
        unordered_set<string> _texturePreviewFailedKeys = {};
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _maskedTexturePreviewCache = {};
        unordered_set<string> _maskedTexturePreviewFailedKeys = {};
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _patternOverlayPreviewCache = {};
        unordered_set<string> _patternOverlayPreviewFailedKeys = {};
        Shared<ShaderCom> _imagePreviewShader = nullptr;
        Shared<VIBuffer_Rect> _imagePreviewVIBuffer = nullptr;
        Unique<SpriteBatch> _fontPreviewSpriteBatch = nullptr;
        ComPtr<ID3D11RasterizerState> _fontPreviewRasterizerState = nullptr;
        unordered_map<string, Unique<SpriteFont>> _fontPreviewCache = {};
        unordered_set<string> _fontPreviewFailedGuids = {};
        vector<FontPreviewCommand> _fontPreviewCommands = {};
    };
}

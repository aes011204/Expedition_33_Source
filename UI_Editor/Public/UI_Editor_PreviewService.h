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

        ID3D11ShaderResourceView* Get_TexturePreviewSRV(const string& textureGuid);
        ID3D11ShaderResourceView* Get_MaskedTexturePreviewSRV(const string& textureGuid, const string& maskTextureGuid);

        bool Queue_FontPreview(
            const string& fontGuid,
            const string& fontPath,
            const wstring& text,
            const Vec2& boxMin,
            const Vec2& boxMax,
            const Vec4& color,
            float rotationRadians,
            UI_TextAlign textAlign,
            float scale,
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
        bool Ensure_ImagePreviewRenderer();
        ID3D11ShaderResourceView* Create_MaskedTexturePreviewSRV(
            const string& cacheKey,
            ID3D11ShaderResourceView* textureSRV,
            ID3D11ShaderResourceView* maskSRV);

    private:
        ComPtr<Device> _device = nullptr;
        ComPtr<Context> _context = nullptr;
        bool _assetManagerReady = false;
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _textureThumbCache = {};
        unordered_set<string> _textureNoThumbGuids = {};
        unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _maskedTexturePreviewCache = {};
        unordered_set<string> _maskedTexturePreviewFailedKeys = {};
        Shared<ShaderCom> _imagePreviewShader = nullptr;
        Shared<VIBuffer_Rect> _imagePreviewVIBuffer = nullptr;
        Unique<SpriteBatch> _fontPreviewSpriteBatch = nullptr;
        ComPtr<ID3D11RasterizerState> _fontPreviewRasterizerState = nullptr;
        unordered_map<string, Unique<SpriteFont>> _fontPreviewCache = {};
        unordered_set<string> _fontPreviewFailedGuids = {};
        vector<FontPreviewCommand> _fontPreviewCommands = {};
    };
}

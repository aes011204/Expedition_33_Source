#pragma once
#include "UI_Document.h"

NS_BEGIN(UIEditor)

class UI_Editor_Manager final
{
public:
    UI_Editor_Manager() = default;
    ~UI_Editor_Manager() = default;

public:
    HRESULT Initialize(const ComPtr<Device>& device, const ComPtr<Context>& context, bool assetManagerReady);
    void    Render();
    void    Render_FontPreviews();

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

    void Render_DockSpace();
    void Render_Hierarchy();
    void Render_Viewport();
    void Render_Inspector();
    void Render_ContentBrowser();
    void Render_TexturePicker(UI_Object& object);
    void Render_FontPicker(UI_Object& object);
    bool Draw_UITextureSlotPicker(const char* label, const char* popupId, UI_Object& object);
    bool Draw_FontSlotPicker(const char* label, const char* popupId, UI_Object& object);
    void Apply_TextureAsset(UI_Object& object, const FAssetMeta& assetMeta);
    void Apply_FontAsset(UI_Object& object, const FAssetMeta& assetMeta);
    ID3D11ShaderResourceView* Get_TexturePreviewSRV(const string& textureGuid);
    SpriteFont* Get_FontPreview(const string& fontGuid, const string& fontPath);

public:
    static Unique<UI_Editor_Manager> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, bool assetManagerReady);

private:
    ComPtr<Device> _device = nullptr;
    ComPtr<Context> _context = nullptr;
    UI_Document _document = {};
    ObjectId _selectedObjectId = 0;
    ObjectId _draggingObjectId = 0;
    ImVec2 _dragStartMouseCanvasPos = ImVec2(0.f, 0.f);
    ImVec2 _dragStartObjectPosition = ImVec2(0.f, 0.f);
    float _viewportZoomScale = 1.f;
    bool _assetManagerReady = false;
    char _textureSearchBuffer[128] = {};
    char _fontSearchBuffer[128] = {};
    char _contentBrowserSearchBuffer[128] = {};
    unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _textureThumbCache = {};
    unordered_set<string> _textureNoThumbGuids = {};
    Unique<SpriteBatch> _fontPreviewSpriteBatch = nullptr;
    ComPtr<ID3D11RasterizerState> _fontPreviewRasterizerState = nullptr;
    unordered_map<string, Unique<SpriteFont>> _fontPreviewCache = {};
    unordered_set<string> _fontPreviewFailedGuids = {};
    vector<FontPreviewCommand> _fontPreviewCommands = {};
};

NS_END

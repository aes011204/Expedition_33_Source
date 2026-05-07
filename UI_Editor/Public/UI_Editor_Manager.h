#pragma once
#include "UI_Document.h"
#include "UI_Editor_Window.h"

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
    void    Add_Window(const Shared<UI_Editor_Window>& window);
    Shared<UI_Editor_Window> Find_Window(const wstring& windowName) const;

public:
    UI_Document& Get_Document();
    const UI_Document& Get_Document() const;
    ObjectId Get_SelectedObjectId() const;
    void Set_SelectedObjectId(ObjectId objectId);
    UI_Object* Get_SelectedObject();
    const UI_Object* Get_SelectedObject() const;
    bool Is_AssetManagerReady() const;
    ID3D11ShaderResourceView* Get_TexturePreviewSRV(const string& textureGuid);
    void Create_UIObject(UI_ObjectType objectType);
    void Duplicate_UIObject(ObjectId objectId);
    void Delete_UIObject(ObjectId objectId);
    bool Queue_FontPreview(
        const string& fontGuid,
        const string& fontPath,
        const wstring& text,
        const Vec2& position,
        const Vec4& color,
        float rotationRadians,
        const Vec2& pivot,
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

    void Render_DockSpace();
    bool Save_Document();
    bool Save_DocumentAs();
    bool Load_Document();
    bool Show_SaveFileDialog(wstring& outFilePath) const;
    bool Show_LoadFileDialog(wstring& outFilePath) const;
    wstring Resolve_DefaultJsonDirectory() const;
    void Ensure_JsonExtension(wstring& filePath) const;
    void Resolve_LoadedAssetReferences();
    ObjectId Resolve_CreateParentId() const;
    SpriteFont* Get_FontPreview(const string& fontGuid, const string& fontPath);

public:
    static Unique<UI_Editor_Manager> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, bool assetManagerReady);

private:
    ComPtr<Device> _device = nullptr;
    ComPtr<Context> _context = nullptr;
    UI_Document _document = {};
    map<wstring, Shared<UI_Editor_Window>> _windows = {};
    ObjectId _selectedObjectId = 0;
    wstring _currentDocumentPath = {};
    wstring _defaultJsonDirectory = {};
    bool _assetManagerReady = false;
    unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _textureThumbCache = {};
    unordered_set<string> _textureNoThumbGuids = {};
    Unique<SpriteBatch> _fontPreviewSpriteBatch = nullptr;
    ComPtr<ID3D11RasterizerState> _fontPreviewRasterizerState = nullptr;
    unordered_map<string, Unique<SpriteFont>> _fontPreviewCache = {};
    unordered_set<string> _fontPreviewFailedGuids = {};
    vector<FontPreviewCommand> _fontPreviewCommands = {};
};

NS_END

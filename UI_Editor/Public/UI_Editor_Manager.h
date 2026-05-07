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
    bool Is_Dirty() const;
    bool Request_Close();
    ID3D11ShaderResourceView* Get_TexturePreviewSRV(const string& textureGuid);
    bool New_Document();
    void Create_UIObject(UI_ObjectType objectType);
    void Duplicate_UIObject(ObjectId objectId);
    void Delete_UIObject(ObjectId objectId);
    void Duplicate_SelectedUIObject();
    void Delete_SelectedUIObject();
    bool Rename_UIObject(ObjectId objectId, const string& newName);
    void Request_RenameUIObject(ObjectId objectId);
    bool Reparent_UIObject(ObjectId objectId, ObjectId newParentId);
    bool Can_ReparentUIObject(ObjectId objectId, ObjectId newParentId) const;
    bool Set_UIObjectWorldPosition(ObjectId objectId, const ImVec2& worldPosition);
    bool Set_UIObjectWorldRect(ObjectId objectId, const ImVec2& worldPosition, const ImVec2& worldSize);
    ImVec2 Get_UIObjectWorldPosition(ObjectId objectId) const;
    bool Undo();
    bool Redo();
    bool Can_Undo() const;
    bool Can_Redo() const;
    void Begin_EditTransaction();
    void Commit_EditTransactionIfChanged();
    void Mark_Dirty();
    void Clear_Dirty();
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

    struct HistoryState
    {
        UI_Document document = {};
        ObjectId selectedObjectId = 0;
        bool isDirty = false;
        string documentSignature = "";
    };

    void Render_DockSpace();
    bool Save_Document();
    bool Save_DocumentAs();
    bool Load_Document();
    bool Confirm_SaveIfDirty(const wchar_t* actionName);
    void Handle_Shortcuts();
    void Render_RenamePopup();
    void Clear_RuntimeCaches();
    bool Show_SaveFileDialog(wstring& outFilePath) const;
    bool Show_LoadFileDialog(wstring& outFilePath) const;
    wstring Resolve_DefaultJsonDirectory() const;
    void Ensure_JsonExtension(wstring& filePath) const;
    void Resolve_LoadedAssetReferences();
    ObjectId Resolve_CreateParentId() const;
    SpriteFont* Get_FontPreview(const string& fontGuid, const string& fontPath);
    string Make_DocumentSignature(const UI_Document& document) const;
    HistoryState Capture_HistoryState() const;
    bool Is_SameHistoryState(const HistoryState& lhs, const HistoryState& rhs) const;
    bool Is_CurrentDocumentClean() const;
    void Push_UndoSnapshot();
    void Push_HistoryState(vector<HistoryState>& historyStack, const HistoryState& historyState) const;
    void Clear_History();
    void Apply_HistoryState(const HistoryState& historyState);

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
    bool _isDirty = false;
    string _cleanDocumentSignature = "";
    vector<HistoryState> _undoStack = {};
    vector<HistoryState> _redoStack = {};
    bool _isEditTransactionOpen = false;
    HistoryState _editTransactionStartState = {};
    ObjectId _renameRequestObjectId = 0;
    bool _openRenamePopup = false;
    char _renameBuffer[128] = {};
    unordered_map<string, ComPtr<ID3D11ShaderResourceView>> _textureThumbCache = {};
    unordered_set<string> _textureNoThumbGuids = {};
    Unique<SpriteBatch> _fontPreviewSpriteBatch = nullptr;
    ComPtr<ID3D11RasterizerState> _fontPreviewRasterizerState = nullptr;
    unordered_map<string, Unique<SpriteFont>> _fontPreviewCache = {};
    unordered_set<string> _fontPreviewFailedGuids = {};
    vector<FontPreviewCommand> _fontPreviewCommands = {};
};

NS_END

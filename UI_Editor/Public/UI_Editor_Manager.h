#pragma once
#include "UI_Document.h"
#include "UI_Editor_FileService.h"
#include "UI_Editor_HistoryService.h"
#include "UI_Editor_PreviewService.h"
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
    ID3D11ShaderResourceView* Get_MaskedTexturePreviewSRV(const string& textureGuid, const string& maskTextureGuid);
    bool New_Document();
    void Create_UIObject(UI_ObjectType objectType);
    void Duplicate_UIObject(ObjectId objectId);
    void Delete_UIObject(ObjectId objectId);
    void Duplicate_SelectedUIObject();
    void Delete_SelectedUIObject();
    bool Rename_Canvas(const string& newName);
    void Request_RenameCanvas();
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
        const Vec2& boxMin,
        const Vec2& boxMax,
        const Vec4& color,
        float rotationRadians,
        UI_TextAlign textAlign,
        float scale,
        const D3D11_RECT& clipRect);

private:
    void Render_DockSpace();
    bool Save_Document();
    bool Save_DocumentAs();
    bool Load_Document();
    bool Confirm_SaveIfDirty(const wchar_t* actionName);
    void Handle_Shortcuts();
    void Render_RenamePopup();
    void Clear_RuntimeCaches();
    void Resolve_LoadedAssetReferences();
    ObjectId Resolve_CreateParentId() const;
    void Push_UndoSnapshot();
    void Clear_History();

public:
    static Unique<UI_Editor_Manager> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, bool assetManagerReady);

private:
    UI_Document _document = {};
    map<wstring, Shared<UI_Editor_Window>> _windows = {};
    ObjectId _selectedObjectId = 0;
    bool _assetManagerReady = false;
    UI_Editor_HistoryService _historyService = {};
    UI_Editor_PreviewService _previewService = {};
    UI_Editor_FileService _fileService = {};
    ObjectId _renameRequestObjectId = 0;
    bool _isRenamingCanvas = false;
    bool _openRenamePopup = false;
    char _renameBuffer[128] = {};
};

NS_END

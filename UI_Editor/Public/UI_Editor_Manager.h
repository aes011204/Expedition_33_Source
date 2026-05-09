#pragma once
#include "UI_Document.h"
#include "UI_AnimationPreviewService.h"
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
    void Set_SingleSelection(ObjectId objectId);
    void Set_SelectedObjectIds(const vector<ObjectId>& objectIds, ObjectId primaryObjectId);
    const vector<ObjectId>& Get_SelectedObjectIds() const;
    vector<ObjectId> Get_RootSelectedObjectIds() const;
    bool Is_ObjectSelected(ObjectId objectId) const;
    bool Has_SelectedObjects() const;
    size_t Get_SelectedObjectCount() const;
    void Add_ToSelection(ObjectId objectId);
    void Toggle_Selection(ObjectId objectId);
    void Clear_Selection();
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
    bool Align_SelectedObjects(UI_AlignmentCommand command);
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
    void Play_AnimationPreview(ObjectId objectId);
    void Play_AnimationAssetPreview(const UIAnimationAsset& asset, int32 startFrame);
    void Preview_AnimationAsset(const UIAnimationAsset& asset, int32 frame);
    void Pause_AnimationPreview();
    void Stop_AnimationPreview();
    void Restart_AnimationPreview(ObjectId objectId);
    bool Is_AnimationPreviewActive() const;
    bool Is_AnimationPreviewPlaying() const;
    float Get_AnimationPreviewTime() const;
    int32 Get_AnimationPreviewFrame() const;
    bool Sample_AnimationPreview(const UI_Object& object, UI_AnimationPreviewSample& outSample) const;

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
    vector<ObjectId> Normalize_Selection(const vector<ObjectId>& objectIds) const;
    vector<ObjectId> Collect_RootObjectIds(const vector<ObjectId>& objectIds) const;
    void Apply_Selection(const vector<ObjectId>& objectIds, ObjectId primaryObjectId);
    void Prune_InvalidSelection();
    void Push_UndoSnapshot();
    void Clear_History();

public:
    static Unique<UI_Editor_Manager> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, bool assetManagerReady);

private:
    UI_Document _document = {};
    map<wstring, Shared<UI_Editor_Window>> _windows = {};
    ObjectId _selectedObjectId = 0;
    vector<ObjectId> _selectedObjectIds = {};
    bool _assetManagerReady = false;
    UI_Editor_HistoryService _historyService = {};
    UI_Editor_PreviewService _previewService = {};
    UI_AnimationPreviewService _animationPreviewService = {};
    UI_Editor_FileService _fileService = {};
    ObjectId _renameRequestObjectId = 0;
    bool _isRenamingCanvas = false;
    bool _openRenamePopup = false;
    char _renameBuffer[128] = {};
};

NS_END

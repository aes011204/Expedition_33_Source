#pragma once
#include "UI_Document.h"
#include "UI_AnimationPreviewService.h"
#include "UI_ClipboardService.h"
#include "UI_Editor_FileService.h"
#include "UI_Editor_HistoryService.h"
#include "UI_Editor_PreviewService.h"
#include "UI_Editor_Window.h"
#include "UI_ObjectCommandService.h"
#include "UI_ObjectEditService.h"
#include "UI_PrefabService.h"
#include "UI_SelectionService.h"
#include "UI_TransformService.h"

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
    const UI_Object* Get_SelectedObject() const;
    bool Is_AssetManagerReady() const;
    bool Is_Dirty() const;
    bool Request_Close();
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
    bool New_Document();
    void Create_UIObject(UI_ObjectType objectType);
    bool Create_PatternOverlayFromImage(ObjectId sourceObjectId);
    bool Create_UIObjectsFromPrefab(const UI_PrefabAsset& prefab);
    void Duplicate_UIObject(ObjectId objectId);
    void Delete_UIObject(ObjectId objectId);
    void Duplicate_SelectedUIObject();
    void Delete_SelectedUIObject();
    bool Copy_SelectedUIObjects();
    bool Paste_UIObjects();
    bool Has_CopiedUIObjects() const;
    bool Rename_Canvas(const string& newName);
    void Request_RenameCanvas();
    bool Rename_UIObject(ObjectId objectId, const string& newName);
    void Request_RenameUIObject(ObjectId objectId);
    bool Reparent_UIObject(ObjectId objectId, ObjectId newParentId);
    bool Can_ReparentUIObject(ObjectId objectId, ObjectId newParentId) const;
    bool Is_ObjectEffectivelyVisible(ObjectId objectId) const;
    bool Is_ObjectEffectivelyLocked(ObjectId objectId) const;
    bool Set_ObjectVisible(ObjectId objectId, bool visible);
    bool Set_ObjectLocked(ObjectId objectId, bool locked);
    bool Set_UIObjectLocalPosition(ObjectId objectId, const ImVec2& position);
    bool Set_UIObjectSize(ObjectId objectId, const ImVec2& size);
    bool Set_UIObjectScale(ObjectId objectId, const ImVec2& scale);
    bool Set_UIObjectRotationRadians(ObjectId objectId, float rotationRadians);
    bool Set_UIObjectSortOrder(ObjectId objectId, int32 sortOrder);
    bool Set_UIObjectRenderGroup(ObjectId objectId, UI_RenderGroup renderGroup);
    bool Set_UIObjectColor(ObjectId objectId, const ImVec4& color);
    bool Set_UIObjectAlphaMultiplier(ObjectId objectId, float alphaMultiplier);
    bool Set_UIObjectImageRenderMode(ObjectId objectId, UI_ImageRenderMode imageRenderMode);
    bool Set_UIObjectTextureBinding(ObjectId objectId, const string& textureGuid, const string& texturePath);
    bool Set_UIObjectMaskTextureBinding(ObjectId objectId, const string& textureGuid, const string& texturePath);
    bool Set_UIObjectMaskTextureMode(ObjectId objectId, UI_MaskTextureMode maskTextureMode);
    bool Set_UIObjectPatternMaskTextureBinding(ObjectId objectId, const string& textureGuid, const string& texturePath);
    bool Set_UIObjectText(ObjectId objectId, const string& text);
    bool Set_UIObjectFontSize(ObjectId objectId, float fontSize);
    bool Set_UIObjectTextAlign(ObjectId objectId, UI_TextAlign textAlign);
    bool Set_UIObjectTextWrap(ObjectId objectId, bool textWrap);
    bool Set_UIObjectFontBinding(ObjectId objectId, const string& fontGuid, const string& fontPath, const string& fontTag);
    bool Set_UIObjectStateAnimationPath(ObjectId objectId, const string& stateAnimationPath);
    bool Set_UIObjectWorldPosition(ObjectId objectId, const ImVec2& worldPosition);
    bool Set_UIObjectWorldRect(ObjectId objectId, const ImVec2& worldPosition, const ImVec2& worldSize);
    bool Set_UIObjectPivotKeepWorldRect(ObjectId objectId, const ImVec2& pivot);
    bool Set_UIObjectAnchorsKeepWorldRect(ObjectId objectId, const ImVec2& anchorMin, const ImVec2& anchorMax);
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
        const Vec2& pivot,
        UI_TextAlign textAlign,
        float scale,
        bool textWrap,
        const D3D11_RECT& clipRect);
    void Play_AnimationPreview(ObjectId objectId);
    void Play_AnimationAssetPreview(const UI_AnimationAsset& asset, int32 startFrame);
    void Play_AnimationSequencePreview(const UI_AnimationSequenceAsset& sequence, int32 startFrame);
    void Preview_AnimationAsset(const UI_AnimationAsset& asset, int32 frame);
    void Preview_AnimationSequence(const UI_AnimationSequenceAsset& sequence, int32 frame);
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
    void Refresh_AnimationTargetBindings();
    void Apply_Selection(const vector<ObjectId>& objectIds, ObjectId primaryObjectId);
    void Apply_SelectionState(UI_SelectionState selectionState, bool commitTransactionOnPrimaryChange);
    void Prune_InvalidSelection();
    bool Select_ImportedObjectsAndRefresh(const vector<ObjectId>& createdRootObjectIds);
    void Restore_HistorySelectionAndRefresh(ObjectId selectedObjectId, const vector<ObjectId>& selectedObjectIds);
    bool Apply_UIObjectEdit(ObjectId objectId, const function<bool(const UI_Object&)>& canApply, const function<bool()>& applyEdit);
    void Push_UndoSnapshot();
    void Clear_History();

public:
    static Unique<UI_Editor_Manager> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, bool assetManagerReady);

private:
    UI_Document _document = {};
    map<wstring, Shared<UI_Editor_Window>> _windows = {};
    UI_SelectionService _selectionService = {};
    bool _assetManagerReady = false;
    UI_Editor_HistoryService _historyService = {};
    UI_Editor_PreviewService _previewService = {};
    UI_AnimationPreviewService _animationPreviewService = {};
    UI_Editor_FileService _fileService = {};
    UI_ClipboardService _clipboardService = {};
    UI_ObjectCommandService _objectCommandService = {};
    UI_ObjectEditService _objectEditService = {};
    UI_TransformService _transformService = {};
    ObjectId _renameRequestObjectId = 0;
    bool _isRenamingCanvas = false;
    bool _openRenamePopup = false;
    char _renameBuffer[128] = {};
};

NS_END

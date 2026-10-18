#include "pch.h"
#include "UI_Editor_Manager.h"

#include "UI_Animation_View.h"
#include "UI_Hierarchy_View.h"
#include "UI_Inspector.h"
#include "UI_Prefab_Browser.h"
#include "UI_Vfx_View.h"
#include "UI_Viewport_View.h"

namespace UIEditor
{
    HRESULT UI_Editor_Manager::Initialize(const ComPtr<Device>& device, const ComPtr<Context>& context, bool assetManagerReady)
    {
        _assetManagerReady = assetManagerReady;

        if (FAILED(_previewService.Initialize(device, context, assetManagerReady)))
            return E_FAIL;

        if (FAILED(_fileService.Initialize()))
            return E_FAIL;

        Add_Window(UI_Hierarchy_View::Create(*this));
        Add_Window(UI_Viewport_View::Create(*this));
        Add_Window(UI_Inspector::Create(*this));
        Add_Window(UI_Animation_View::Create(*this));
        Add_Window(UI_Vfx_View::Create(*this));
        Add_Window(UI_Prefab_Browser::Create(*this));

        Clear_History();
        Clear_Dirty();

        return S_OK;
    }

    void UI_Editor_Manager::Render()
    {
        _previewService.Begin_Frame();
        _animationPreviewService.Update(ImGui::GetIO().DeltaTime);
        if (_animationPreviewService.Is_Playing())
            ++_animationPreviewRevision;
        Get_ReadModel();

        Render_DockSpace();
        Handle_Shortcuts();

        for (auto& pair : _windows)
        {
            if (nullptr != pair.second)
                pair.second->Render();
        }

        Render_RenamePopup();
    }

    void UI_Editor_Manager::Render_FontPreviews()
    {
        _previewService.Render_FontPreviews();
    }

    void UI_Editor_Manager::Add_Window(const Shared<UI_Editor_Window>& window)
    {
        if (nullptr == window)
            return;

        _windows[window->Get_WindowName()] = window;
    }

    Shared<UI_Editor_Window> UI_Editor_Manager::Find_Window(const wstring& windowName) const
    {
        const auto iter = _windows.find(windowName);
        if (iter == _windows.end())
            return nullptr;

        return iter->second;
    }

    const UI_Document& UI_Editor_Manager::Get_Document() const
    {
        return _document;
    }

    const UI_EditorSession& UI_Editor_Manager::Get_Session() const
    {
        _session.Reset(_document, Get_ReadModel(), nullptr, Get_SelectedObjectId(), Get_SelectedObjectIds());
        return _session;
    }

    const UI_EditorSession& UI_Editor_Manager::Get_LayoutSession() const
    {
        _session.Reset(_document, Get_ReadModel(), &Get_LayoutCache(), Get_SelectedObjectId(), Get_SelectedObjectIds());
        return _session;
    }

    const UI_EditorReadModel& UI_Editor_Manager::Get_ReadModel() const
    {
        _readModel.Rebuild_IfNeeded(_document, _readModelRevision);
        return _readModel;
    }

    const UI_EditorLayoutCache& UI_Editor_Manager::Get_LayoutCache() const
    {
        const uint64 animationRevision = _animationPreviewService.Is_Active() ? _animationPreviewRevision : 0;
        _layoutCache.Rebuild_IfNeeded(
            _document,
            Get_ReadModel(),
            _layoutRevision,
            animationRevision,
            [this](const UI_Object& object, UI_AnimationPreviewSample& outSample)
            {
                return Sample_AnimationPreview(object, outSample);
            });

        return _layoutCache;
    }

    uint64 UI_Editor_Manager::Get_DocumentChangeSerial() const
    {
        return _documentChangeSerial;
    }

    UI_EditorEditImpact UI_Editor_Manager::Get_LastDocumentChangeImpact() const
    {
        return _lastDocumentChangeImpact;
    }

    ObjectId UI_Editor_Manager::Get_LastChangedObjectId() const
    {
        return _lastChangedObjectId;
    }

    ObjectId UI_Editor_Manager::Get_SelectedObjectId() const
    {
        return _selectionService.Get_SelectedObjectId();
    }

    void UI_Editor_Manager::Set_SelectedObjectId(ObjectId objectId)
    {
        Set_SingleSelection(objectId);
    }

    void UI_Editor_Manager::Set_SingleSelection(ObjectId objectId)
    {
        Apply_SelectionState(_selectionService.Make_SingleSelectionState(_document, objectId), false);
    }

    void UI_Editor_Manager::Set_SelectedObjectIds(const vector<ObjectId>& objectIds, ObjectId primaryObjectId)
    {
        Apply_Selection(objectIds, primaryObjectId);
    }

    const vector<ObjectId>& UI_Editor_Manager::Get_SelectedObjectIds() const
    {
        return _selectionService.Get_SelectedObjectIds();
    }

    vector<ObjectId> UI_Editor_Manager::Get_RootSelectedObjectIds() const
    {
        return _selectionService.Get_RootSelectedObjectIds(_document);
    }

    bool UI_Editor_Manager::Is_ObjectSelected(ObjectId objectId) const
    {
        return _selectionService.Is_ObjectSelected(objectId);
    }

    bool UI_Editor_Manager::Has_SelectedObjects() const
    {
        return _selectionService.Has_SelectedObjects();
    }

    size_t UI_Editor_Manager::Get_SelectedObjectCount() const
    {
        return _selectionService.Get_SelectedObjectCount();
    }

    void UI_Editor_Manager::Add_ToSelection(ObjectId objectId)
    {
        Apply_SelectionState(_selectionService.Make_AddToSelectionState(_document, objectId), true);
    }

    void UI_Editor_Manager::Toggle_Selection(ObjectId objectId)
    {
        Apply_SelectionState(_selectionService.Make_ToggleSelectionState(_document, objectId), true);
    }

    void UI_Editor_Manager::Clear_Selection()
    {
        Apply_SelectionState(_selectionService.Make_ClearSelectionState(), true);
    }

    const UI_Object* UI_Editor_Manager::Get_SelectedObject() const
    {
        return _selectionService.Get_SelectedObject(_document);
    }

    bool UI_Editor_Manager::Is_AssetManagerReady() const
    {
        return _assetManagerReady;
    }

    bool UI_Editor_Manager::Is_Dirty() const
    {
        return _historyService.Is_Dirty();
    }

    bool UI_Editor_Manager::Request_Close()
    {
        return Confirm_SaveIfDirty(L"close UI Editor");
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_TexturePreviewSRV(const string& textureGuid, const string& texturePath)
    {
        return _previewService.Get_TexturePreviewSRV(textureGuid, texturePath);
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_MaskedTexturePreviewSRV(
        const string& textureGuid,
        const string& texturePath,
        const string& maskTextureGuid,
        const string& maskTexturePath,
        UI_MaskTextureMode maskTextureMode)
    {
        return _previewService.Get_MaskedTexturePreviewSRV(textureGuid, texturePath, maskTextureGuid, maskTexturePath, maskTextureMode);
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_FillRatioPreviewSRV(
        const string& textureGuid,
        const string& texturePath,
        float fillRatio,
        UI_FillOrigin fillOrigin)
    {
        return _previewService.Get_FillRatioPreviewSRV(textureGuid, texturePath, fillRatio, fillOrigin);
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_PatternOverlayPreviewSRV(
        const PatternOverlayPreviewDesc& previewDesc)
    {
        return _previewService.Get_PatternOverlayPreviewSRV(previewDesc);
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_MaterialVfxPreviewSRV(
        const MaterialVfxPreviewDesc& previewDesc)
    {
        return _previewService.Get_MaterialVfxPreviewSRV(previewDesc);
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_ButtonSheenPreviewSRV(
        const ButtonSheenPreviewDesc& previewDesc)
    {
        return _previewService.Get_ButtonSheenPreviewSRV(previewDesc);
    }

    bool UI_Editor_Manager::Queue_FontPreview(
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
        const D3D11_RECT* sharedClipRect,
        bool allowSharedClip)
    {
        return _previewService.Queue_FontPreview(
            fontGuid,
            fontPath,
            text,
            boxMin,
            boxMax,
            color,
            rotationRadians,
            pivot,
            textAlign,
            scale,
            textWrap,
            clipRect,
            sharedClipRect,
            allowSharedClip);
    }

    void UI_Editor_Manager::Apply_Selection(const vector<ObjectId>& objectIds, ObjectId primaryObjectId)
    {
        Apply_SelectionState(_selectionService.Make_SelectionState(_document, objectIds, primaryObjectId), true);
    }

    void UI_Editor_Manager::Apply_SelectionState(UI_SelectionState selectionState, bool commitTransactionOnPrimaryChange)
    {
        const ObjectId previousSelectedObjectId = _selectionService.Get_SelectedObjectId();
        const vector<ObjectId> previousSelectedObjectIds = _selectionService.Get_SelectedObjectIds();

        if (_selectionService.Get_SelectedObjectId() != selectionState.selectedObjectId)
        {
            if (commitTransactionOnPrimaryChange)
                Commit_EditTransactionIfChanged();

            _animationPreviewService.Stop();
            ++_animationPreviewRevision;
        }

        _selectionService.Apply_SelectionState(move(selectionState));

        if (previousSelectedObjectId != _selectionService.Get_SelectedObjectId() ||
            previousSelectedObjectIds != _selectionService.Get_SelectedObjectIds())
        {
            Notify_DocumentChanged(UI_EditorEditImpact::Selection);
        }
    }

    bool UI_Editor_Manager::Undo()
    {
        ObjectId selectedObjectId = Get_SelectedObjectId();
        vector<ObjectId> selectedObjectIds = Get_SelectedObjectIds();
        if (!_historyService.Undo(_document, selectedObjectId, selectedObjectIds))
            return false;

        Notify_DocumentChanged(UI_EditorEditImpact::Structure);
        Restore_HistorySelectionAndRefresh(selectedObjectId, selectedObjectIds);
        return true;
    }

    bool UI_Editor_Manager::Redo()
    {
        ObjectId selectedObjectId = Get_SelectedObjectId();
        vector<ObjectId> selectedObjectIds = Get_SelectedObjectIds();
        if (!_historyService.Redo(_document, selectedObjectId, selectedObjectIds))
            return false;

        Notify_DocumentChanged(UI_EditorEditImpact::Structure);
        Restore_HistorySelectionAndRefresh(selectedObjectId, selectedObjectIds);
        return true;
    }

    bool UI_Editor_Manager::Can_Undo() const
    {
        return _historyService.Can_Undo();
    }

    bool UI_Editor_Manager::Can_Redo() const
    {
        return _historyService.Can_Redo();
    }

    void UI_Editor_Manager::Begin_EditTransaction()
    {
        _historyService.Begin_EditTransaction(_document, Get_SelectedObjectId(), Get_SelectedObjectIds());
    }

    void UI_Editor_Manager::Commit_EditTransactionIfChanged()
    {
        _historyService.Commit_EditTransactionIfChanged(_document, Get_SelectedObjectId(), Get_SelectedObjectIds());
    }

    void UI_Editor_Manager::Mark_Dirty()
    {
        _historyService.Mark_Dirty(_document);
    }

    void UI_Editor_Manager::Clear_Dirty()
    {
        _historyService.Clear_Dirty(_document);
    }

    void UI_Editor_Manager::Push_UndoSnapshot()
    {
        _historyService.Push_UndoSnapshot(_document, Get_SelectedObjectId(), Get_SelectedObjectIds());
    }

    void UI_Editor_Manager::Clear_History()
    {
        _historyService.Clear_History();
    }

    Unique<UI_Editor_Manager> UI_Editor_Manager::Create(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        bool assetManagerReady)
    {
        Unique<UI_Editor_Manager> instance{ new UI_Editor_Manager() };

        if (FAILED(instance->Initialize(device, context, assetManagerReady)))
            return nullptr;

        return instance;
    }
}

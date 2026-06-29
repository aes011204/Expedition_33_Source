#include "pch.h"
#include "UI_Editor_Manager.h"

namespace UIEditor
{
    void UI_Editor_Manager::Prune_InvalidSelection()
    {
        const UI_SelectionState currentSelection = _selectionService.Make_CurrentSelectionState();
        Apply_Selection(currentSelection.selectedObjectIds, currentSelection.selectedObjectId);
    }

    bool UI_Editor_Manager::Select_ImportedObjectsAndRefresh(const vector<ObjectId>& createdRootObjectIds)
    {
        if (createdRootObjectIds.empty())
            return false;

        Set_SelectedObjectIds(createdRootObjectIds, createdRootObjectIds.back());
        Resolve_LoadedAssetReferences();
        Refresh_AnimationTargetBindings();
        Clear_RuntimeCaches();
        Notify_DocumentChanged(UI_EditorEditImpact::Structure);
        Mark_Dirty();
        return true;
    }

    void UI_Editor_Manager::Restore_HistorySelectionAndRefresh(ObjectId selectedObjectId, const vector<ObjectId>& selectedObjectIds)
    {
        _selectionService.Restore_SelectionState(_document, selectedObjectId, selectedObjectIds);
        Prune_InvalidSelection();
        _animationPreviewService.Stop();
        Clear_RuntimeCaches();
        Resolve_LoadedAssetReferences();
        Refresh_AnimationTargetBindings();
    }

    void UI_Editor_Manager::Create_UIObject(UI_ObjectType objectType)
    {
        Push_UndoSnapshot();

        const ObjectId parentId = _objectCommandService.Resolve_CreateParentId(Get_ReadModel(), Get_SelectedObjectId());
        const ObjectId createdObjectId = _objectCommandService.Create_Object(_document, objectType, parentId);
        if (createdObjectId == 0)
            return;

        Set_SingleSelection(createdObjectId);
        Notify_DocumentChanged(UI_EditorEditImpact::Structure, createdObjectId);
        Mark_Dirty();
    }

    bool UI_Editor_Manager::Create_PatternOverlayFromImage(ObjectId sourceObjectId)
    {
        const UI_EditorReadModel& readModel = Get_ReadModel();
        if (!_brushPresetService.Can_CreatePatternOverlayFromImage(_document, readModel, sourceObjectId))
            return false;

        Push_UndoSnapshot();

        const ObjectId overlayObjectId = _brushPresetService.Create_PatternOverlayFromImage(_document, readModel, sourceObjectId);
        if (overlayObjectId == 0)
            return false;

        Set_SingleSelection(overlayObjectId);
        Clear_RuntimeCaches();
        Notify_DocumentChanged(UI_EditorEditImpact::Structure, overlayObjectId);
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Create_HoverBrushPreset(ObjectId targetObjectId, bool allowStateAnimationOverwrite)
    {
        const UI_EditorReadModel& readModel = Get_ReadModel();
        if (!_brushPresetService.Can_CreateHoverBrushPreset(_document, readModel, targetObjectId))
            return false;

        Push_UndoSnapshot();

        if (!_brushPresetService.Create_HoverBrushPreset(_document, readModel, targetObjectId, allowStateAnimationOverwrite))
            return false;

        Set_SingleSelection(targetObjectId);
        Resolve_LoadedAssetReferences();
        Refresh_AnimationTargetBindings();
        Clear_RuntimeCaches();
        Notify_DocumentChanged(UI_EditorEditImpact::Structure, targetObjectId);
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Create_BrushAtmospherePreset(ObjectId targetObjectId)
    {
        const UI_EditorReadModel& readModel = Get_ReadModel();
        if (!_brushPresetService.Can_CreateBrushAtmospherePreset(_document, readModel, targetObjectId))
            return false;

        Push_UndoSnapshot();

        if (!_brushPresetService.Create_BrushAtmospherePreset(_document, readModel, targetObjectId))
            return false;

        Set_SingleSelection(targetObjectId);
        Resolve_LoadedAssetReferences();
        Refresh_AnimationTargetBindings();
        Clear_RuntimeCaches();
        Notify_DocumentChanged(UI_EditorEditImpact::Structure, targetObjectId);
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Create_UIObjectsFromPrefab(const UI_PrefabAsset& prefab)
    {
        if (prefab.objects.empty() || prefab.rootObjectIds.empty())
            return false;

        Push_UndoSnapshot();

        const ObjectId parentId = _objectCommandService.Resolve_CreateParentId(Get_ReadModel(), Get_SelectedObjectId());
        const vector<ObjectId> createdRootObjectIds = _objectCommandService.Create_ObjectsFromPrefab(
            _document,
            prefab,
            parentId,
            ImVec2(100.f, 100.f));

        return Select_ImportedObjectsAndRefresh(createdRootObjectIds);
    }

    void UI_Editor_Manager::Duplicate_UIObject(ObjectId objectId)
    {
        if (!_objectCommandService.Can_DuplicateObject(Get_ReadModel(), objectId))
            return;

        Push_UndoSnapshot();

        const ObjectId duplicatedObjectId = _objectCommandService.Duplicate_Object(_document, Get_ReadModel(), objectId, ImVec2(20.f, 20.f));
        if (duplicatedObjectId == 0)
            return;

        Set_SingleSelection(duplicatedObjectId);
        Notify_DocumentChanged(UI_EditorEditImpact::Structure, duplicatedObjectId);
        Mark_Dirty();
    }

    void UI_Editor_Manager::Delete_UIObject(ObjectId objectId)
    {
        if (!_objectCommandService.Can_DeleteObject(Get_ReadModel(), objectId))
            return;

        Push_UndoSnapshot();

        if (!_objectCommandService.Delete_Object(_document, Get_ReadModel(), objectId))
            return;

        Prune_InvalidSelection();

        _animationPreviewService.Stop();
        ++_animationPreviewRevision;
        Refresh_AnimationTargetBindings();
        Notify_DocumentChanged(UI_EditorEditImpact::Structure, objectId);
        Mark_Dirty();
    }

    void UI_Editor_Manager::Duplicate_SelectedUIObject()
    {
        if (!Has_SelectedObjects())
            return;

        vector<ObjectId> rootObjectIds = _objectCommandService.Filter_UnlockedObjectIds(Get_ReadModel(), Get_RootSelectedObjectIds());

        if (rootObjectIds.empty())
            return;

        Push_UndoSnapshot();

        vector<ObjectId> duplicatedObjectIds = _objectCommandService.Duplicate_Objects(_document, Get_ReadModel(), rootObjectIds, ImVec2(20.f, 20.f));

        if (duplicatedObjectIds.empty())
            return;

        Set_SelectedObjectIds(duplicatedObjectIds, duplicatedObjectIds.back());
        Notify_DocumentChanged(UI_EditorEditImpact::Structure, duplicatedObjectIds.back());
        Mark_Dirty();
    }

    void UI_Editor_Manager::Delete_SelectedUIObject()
    {
        if (!Has_SelectedObjects())
            return;

        vector<ObjectId> rootObjectIds = _objectCommandService.Filter_UnlockedObjectIds(Get_ReadModel(), Get_RootSelectedObjectIds());

        if (rootObjectIds.empty())
            return;

        Push_UndoSnapshot();

        if (!_objectCommandService.Delete_Objects(_document, Get_ReadModel(), rootObjectIds))
            return;

        Prune_InvalidSelection();
        _animationPreviewService.Stop();
        ++_animationPreviewRevision;
        Refresh_AnimationTargetBindings();
        Notify_DocumentChanged(UI_EditorEditImpact::Structure);
        Mark_Dirty();
    }

    bool UI_Editor_Manager::Copy_SelectedUIObjects()
    {
        const vector<ObjectId> rootObjectIds = Get_RootSelectedObjectIds();
        return _clipboardService.Copy_FromDocument(_document, rootObjectIds);
    }

    bool UI_Editor_Manager::Paste_UIObjects()
    {
        if (!_clipboardService.Has_CopiedUIObjects())
            return false;

        const ObjectId parentId = _objectCommandService.Resolve_PasteParentId(_document, Get_ReadModel(), Get_SelectedObjectId());

        Push_UndoSnapshot();

        vector<ObjectId> createdRootObjectIds;
        _clipboardService.Paste_ToDocument(_document, parentId, createdRootObjectIds);
        return Select_ImportedObjectsAndRefresh(createdRootObjectIds);
    }

    bool UI_Editor_Manager::Has_CopiedUIObjects() const
    {
        return _clipboardService.Has_CopiedUIObjects();
    }

    bool UI_Editor_Manager::Rename_Canvas(const string& newName)
    {
        if (!_objectCommandService.Can_RenameCanvas(newName))
            return false;

        if (_document.Get_CanvasName() == newName)
            return true;

        Push_UndoSnapshot();
        _objectCommandService.Rename_Canvas(_document, newName);
        Notify_DocumentChanged(UI_EditorEditImpact::Visual);
        Mark_Dirty();
        return true;
    }

    void UI_Editor_Manager::Request_RenameCanvas()
    {
        _renameRequestObjectId = 0;
        _isRenamingCanvas = true;
        strncpy_s(_renameBuffer, _document.Get_CanvasName().c_str(), _TRUNCATE);
        _openRenamePopup = true;
    }

    bool UI_Editor_Manager::Rename_UIObject(ObjectId objectId, const string& newName)
    {
        if (!_objectCommandService.Can_RenameObject(Get_ReadModel(), objectId, newName))
            return false;

        const UI_Object* object = Get_ReadModel().Find_Object(objectId);
        const string uniqueName = _objectCommandService.Resolve_RenameObjectName(_document, objectId, newName);
        if (object->name == uniqueName)
            return true;

        Push_UndoSnapshot();

        _objectCommandService.Rename_Object(_document, objectId, newName);
        Notify_DocumentChanged(UI_EditorEditImpact::Visual, objectId);
        Mark_Dirty();
        return true;
    }

    void UI_Editor_Manager::Request_RenameUIObject(ObjectId objectId)
    {
        const UI_Object* object = Get_ReadModel().Find_Object(objectId);
        if (nullptr == object || Is_ObjectEffectivelyLocked(objectId))
            return;

        _renameRequestObjectId = objectId;
        _isRenamingCanvas = false;
        strncpy_s(_renameBuffer, object->name.c_str(), _TRUNCATE);
        _openRenamePopup = true;
    }

    bool UI_Editor_Manager::Reparent_UIObject(ObjectId objectId, ObjectId newParentId)
    {
        const UI_Object* object = Get_ReadModel().Find_Object(objectId);
        if (nullptr == object || object->parentId == newParentId)
            return false;

        if (!_objectCommandService.Can_ReparentObject(_document, Get_ReadModel(), objectId, newParentId))
            return false;

        Push_UndoSnapshot();

        if (!_objectCommandService.Reparent_Object(_document, Get_ReadModel(), objectId, newParentId, true))
            return false;

        Set_SingleSelection(objectId);
        Notify_DocumentChanged(UI_EditorEditImpact::Structure, objectId);
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Can_ReparentUIObject(ObjectId objectId, ObjectId newParentId) const
    {
        return _objectCommandService.Can_ReparentObject(_document, Get_ReadModel(), objectId, newParentId);
    }
}

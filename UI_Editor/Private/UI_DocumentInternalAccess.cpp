#include "pch.h"
#include "UI_DocumentInternalAccess.h"

namespace UIEditor
{
    ObjectId UI_DocumentInternalAccess::Create_Object(UI_Document& document, UI_ObjectType objectType, ObjectId parentId)
    {
        return document.Create_Object(objectType, parentId);
    }

    ObjectId UI_DocumentInternalAccess::Duplicate_Object(UI_Document& document, ObjectId objectId, const ImVec2& offset)
    {
        return document.Duplicate_Object(objectId, offset);
    }

    vector<ObjectId> UI_DocumentInternalAccess::Import_PrefabObjects(
        UI_Document& document,
        const vector<UI_Object>& sourceObjects,
        const vector<ObjectId>& rootObjectIds,
        ObjectId parentId,
        const ImVec2& rootOffset)
    {
        return document.Import_PrefabObjects(sourceObjects, rootObjectIds, parentId, rootOffset);
    }

    bool UI_DocumentInternalAccess::Delete_Object(UI_Document& document, ObjectId objectId)
    {
        return document.Delete_Object(objectId);
    }

    bool UI_DocumentInternalAccess::Change_Parent(UI_Document& document, ObjectId objectId, ObjectId newParentId, bool keepWorldPosition)
    {
        return document.Change_Parent(objectId, newParentId, keepWorldPosition);
    }

    bool UI_DocumentInternalAccess::Set_WorldPosition(UI_Document& document, ObjectId objectId, const ImVec2& worldPosition)
    {
        return document.Set_WorldPosition(objectId, worldPosition);
    }

    bool UI_DocumentInternalAccess::Set_WorldRect(UI_Document& document, ObjectId objectId, const ImVec2& worldPosition, const ImVec2& worldSize)
    {
        return document.Set_WorldRect(objectId, worldPosition, worldSize);
    }

    bool UI_DocumentInternalAccess::Set_PivotKeepWorldRect(UI_Document& document, ObjectId objectId, const ImVec2& pivot)
    {
        return document.Set_PivotKeepWorldRect(objectId, pivot);
    }

    bool UI_DocumentInternalAccess::Set_AnchorsKeepWorldRect(
        UI_Document& document,
        ObjectId objectId,
        const ImVec2& anchorMin,
        const ImVec2& anchorMax)
    {
        return document.Set_AnchorsKeepWorldRect(objectId, anchorMin, anchorMax);
    }

    bool UI_DocumentInternalAccess::Load_FromJsonFile(UI_Document& document, const wstring& filePath)
    {
        return document.Load_FromJsonFile(filePath);
    }

    void UI_DocumentInternalAccess::Replace_LoadedContent(
        UI_Document& document,
        string canvasName,
        const ImVec2& canvasSize,
        vector<UI_Object> objects)
    {
        document.Set_CanvasName(canvasName);
        document.Set_CanvasSize(canvasSize);
        document._objects = move(objects);
        document.Validate_ParentLinks();
        document.Resolve_NextObjectId();
    }

    void UI_DocumentInternalAccess::Set_CanvasName(UI_Document& document, const string& canvasName)
    {
        document.Set_CanvasName(canvasName);
    }

    void UI_DocumentInternalAccess::Reset(UI_Document& document)
    {
        document.Reset();
    }

    UI_Object* UI_DocumentInternalAccess::Find_Object(UI_Document& document, ObjectId objectId)
    {
        return document.Find_Object(objectId);
    }

    vector<UI_Object>& UI_DocumentInternalAccess::Get_MutableObjects(UI_Document& document)
    {
        return document.Get_MutableObjects();
    }
}

#pragma once

#include "UI_PrefabService.h"

namespace UIEditor
{
    class UI_ObjectCommandService final
    {
    public:
        UI_ObjectCommandService() = default;
        ~UI_ObjectCommandService() = default;

    public:
        ObjectId Resolve_CreateParentId(const UI_Document& document, ObjectId selectedObjectId) const;
        ObjectId Resolve_PasteParentId(const UI_Document& document, ObjectId selectedObjectId) const;

        ObjectId Create_Object(UI_Document& document, UI_ObjectType objectType, ObjectId parentId) const;
        vector<ObjectId> Create_ObjectsFromPrefab(
            UI_Document& document,
            const UI_PrefabAsset& prefab,
            ObjectId parentId,
            const ImVec2& rootOffset) const;

        bool Can_DuplicateObject(const UI_Document& document, ObjectId objectId) const;
        ObjectId Duplicate_Object(UI_Document& document, ObjectId objectId, const ImVec2& offset) const;
        vector<ObjectId> Duplicate_Objects(UI_Document& document, const vector<ObjectId>& objectIds, const ImVec2& offset) const;

        bool Can_DeleteObject(const UI_Document& document, ObjectId objectId) const;
        bool Delete_Object(UI_Document& document, ObjectId objectId) const;
        bool Delete_Objects(UI_Document& document, const vector<ObjectId>& objectIds) const;

        bool Can_RenameCanvas(const string& newName) const;
        bool Rename_Canvas(UI_Document& document, const string& newName) const;
        bool Can_RenameObject(const UI_Document& document, ObjectId objectId, const string& newName) const;
        string Resolve_RenameObjectName(const UI_Document& document, ObjectId objectId, const string& newName) const;
        bool Rename_Object(UI_Document& document, ObjectId objectId, const string& newName) const;

        bool Can_ReparentObject(const UI_Document& document, ObjectId objectId, ObjectId newParentId) const;
        bool Reparent_Object(UI_Document& document, ObjectId objectId, ObjectId newParentId, bool keepWorldPosition) const;

        bool Is_ObjectEffectivelyVisible(const UI_Document& document, ObjectId objectId) const;
        bool Is_ObjectEffectivelyLocked(const UI_Document& document, ObjectId objectId) const;
        vector<ObjectId> Filter_UnlockedObjectIds(const UI_Document& document, const vector<ObjectId>& objectIds) const;

        bool Can_SetObjectVisible(const UI_Document& document, ObjectId objectId, bool visible) const;
        bool Set_ObjectVisible(UI_Document& document, ObjectId objectId, bool visible) const;
        bool Can_SetObjectLocked(const UI_Document& document, ObjectId objectId, bool locked) const;
        bool Set_ObjectLocked(UI_Document& document, ObjectId objectId, bool locked) const;

        bool Can_AlignObjects(size_t objectCount, UI_AlignmentCommand command) const;
        bool Align_Objects(UI_Document& document, const vector<ObjectId>& objectIds, UI_AlignmentCommand command) const;

    private:
        void Configure_ButtonLabel(UI_Document& document, ObjectId buttonObjectId) const;
    };
}

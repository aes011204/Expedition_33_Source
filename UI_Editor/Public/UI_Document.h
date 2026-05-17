#pragma once

#include "UI_Editor_Defines.h"

namespace UIEditor
{
    class UI_DocumentSerializer;

    class UI_Document final
    {
        friend class UI_DocumentSerializer;

    public:
        UI_Document();

        const string& Get_CanvasName() const;
        const ImVec2& Get_CanvasSize() const;
        const vector<UI_Object>& Get_Objects() const;
        ObjectId Create_Object(UI_ObjectType objectType, ObjectId parentId);
        ObjectId Duplicate_Object(ObjectId objectId, const ImVec2& offset);
        vector<ObjectId> Import_PrefabObjects(const vector<UI_Object>& sourceObjects, const vector<ObjectId>& rootObjectIds, ObjectId parentId, const ImVec2& rootOffset);
        bool Delete_Object(ObjectId objectId);
        bool Change_Parent(ObjectId objectId, ObjectId newParentId, bool keepWorldPosition);
        bool Can_Parent(ObjectId objectId, ObjectId newParentId) const;
        bool Can_HaveChildren(ObjectId objectId) const;
        bool Is_Descendant(ObjectId ancestorId, ObjectId candidateDescendantId) const;
        ImVec2 Get_ParentAnchorReferenceSize(ObjectId objectId) const;
        ImVec2 Get_ResolvedLocalPosition(ObjectId objectId) const;
        ImVec2 Get_ResolvedLocalTopLeft(ObjectId objectId) const;
        ImVec2 Get_ResolvedLocalSize(ObjectId objectId) const;
        ImVec2 Get_WorldPosition(ObjectId objectId) const;
        ImVec2 Transform_LocalPointToWorld(ObjectId objectId, const ImVec2& localPoint) const;
        ImVec2 Get_WorldCenter(ObjectId objectId) const;
        ImVec2 Get_WorldSize(ObjectId objectId) const;
        ImVec2 Get_WorldScale(ObjectId objectId) const;
        float Get_WorldRotationRadians(ObjectId objectId) const;
        bool Set_WorldPosition(ObjectId objectId, const ImVec2& worldPosition);
        bool Set_WorldRect(ObjectId objectId, const ImVec2& worldPosition, const ImVec2& worldSize);
        bool Set_PivotKeepWorldRect(ObjectId objectId, const ImVec2& pivot);
        bool Set_AnchorsKeepWorldRect(ObjectId objectId, const ImVec2& anchorMin, const ImVec2& anchorMax);
        string Generate_UniqueName(const string& baseName, ObjectId ignoredObjectId = 0) const;
        string Generate_DuplicateName(const string& sourceName) const;
        UI_Object* Find_Object(ObjectId objectId);
        const UI_Object* Find_Object(ObjectId objectId) const;
        bool Save_ToJsonFile(const wstring& filePath) const;
        bool Load_FromJsonFile(const wstring& filePath);
        json To_Json() const;
        bool From_Json(const json& root);
        void Resolve_NextObjectId();
        vector<UI_Object>& Get_MutableObjects();
        void Set_CanvasName(const string& canvasName);
        void Set_CanvasSize(const ImVec2& canvasSize);

        void Reset();

    private:
        void Validate_ParentLinks();
        void Collect_ObjectAndChildren(ObjectId objectId, vector<ObjectId>& objectIds) const;
        ObjectId Duplicate_ObjectRecursive(UI_Object sourceObject, ObjectId parentId, const ImVec2& offset, bool applyOffset);

    private:
        string _canvasName = "MainCanvas";
        ImVec2 _canvasSize = ImVec2(1600.f, 900.f);
        ObjectId _nextObjectId = 1;
        vector<UI_Object> _objects = {};
        
    };
}

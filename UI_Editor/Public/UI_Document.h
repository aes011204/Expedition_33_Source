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
        ImVec2 Get_WorldPosition(ObjectId objectId) const;
        bool Set_WorldPosition(ObjectId objectId, const ImVec2& worldPosition);
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

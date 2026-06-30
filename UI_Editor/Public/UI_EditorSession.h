#pragma once

#include "UI_EditorLayoutCache.h"

namespace UIEditor
{
    class UI_EditorSession final
    {
    public:
        void Reset(
            const UI_Document& document,
            const UI_EditorReadModel& readModel,
            const UI_EditorLayoutCache* layoutCache,
            ObjectId selectedObjectId,
            const vector<ObjectId>& selectedObjectIds)
        {
            _document = &document;
            _readModel = &readModel;
            _layoutCache = layoutCache;
            _selectedObjectId = selectedObjectId;
            _selectedObjectIds = &selectedObjectIds;
        }

        const string& Get_CanvasName() const
        {
            return Get_Document().Get_CanvasName();
        }

        ImVec2 Get_CanvasSize() const
        {
            return Get_Document().Get_CanvasSize();
        }

        ObjectId Get_SelectedObjectId() const
        {
            return _selectedObjectId;
        }

        const vector<ObjectId>& Get_SelectedObjectIds() const
        {
            return nullptr != _selectedObjectIds ? *_selectedObjectIds : Empty_ObjectIds();
        }

        const UI_Object* Get_SelectedObject() const
        {
            return Find_Object(_selectedObjectId);
        }

        const UI_Object* Find_Object(ObjectId objectId) const
        {
            return nullptr != _readModel ? _readModel->Find_Object(objectId) : nullptr;
        }

        const vector<const UI_Object*>& Get_Children(ObjectId parentId) const
        {
            return nullptr != _readModel ? _readModel->Get_Children(parentId) : Empty_ObjectList();
        }

        bool Has_Children(ObjectId parentId) const
        {
            return nullptr != _readModel && _readModel->Has_Children(parentId);
        }

        bool Is_EffectivelyVisible(ObjectId objectId) const
        {
            return nullptr != _readModel && _readModel->Is_EffectivelyVisible(objectId);
        }

        bool Is_EffectivelyLocked(ObjectId objectId) const
        {
            return nullptr != _readModel && _readModel->Is_EffectivelyLocked(objectId);
        }

        const UI_EditorViewportItem* Find_ViewportItem(ObjectId objectId) const
        {
            return nullptr != _layoutCache ? _layoutCache->Find_Item(objectId) : nullptr;
        }

        bool Has_Layout() const
        {
            return nullptr != _layoutCache;
        }

    private:
        const UI_Document& Get_Document() const
        {
            return *_document;
        }

        static const vector<const UI_Object*>& Empty_ObjectList()
        {
            static const vector<const UI_Object*> emptyList;
            return emptyList;
        }

        static const vector<ObjectId>& Empty_ObjectIds()
        {
            static const vector<ObjectId> emptyList;
            return emptyList;
        }

    private:
        const UI_Document* _document = nullptr;
        const UI_EditorReadModel* _readModel = nullptr;
        const UI_EditorLayoutCache* _layoutCache = nullptr;
        const vector<ObjectId>* _selectedObjectIds = nullptr;
        ObjectId _selectedObjectId = 0;
    };
}

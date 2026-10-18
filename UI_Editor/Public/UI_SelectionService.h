#pragma once

#include "UI_Document.h"

namespace UIEditor
{
    struct UI_SelectionState
    {
        ObjectId selectedObjectId = 0;
        vector<ObjectId> selectedObjectIds = {};
    };

    class UI_SelectionService final
    {
    public:
        UI_SelectionService() = default;
        ~UI_SelectionService() = default;

    public:
        ObjectId Get_SelectedObjectId() const { return _state.selectedObjectId; }
        const vector<ObjectId>& Get_SelectedObjectIds() const { return _state.selectedObjectIds; }
        bool Is_ObjectSelected(ObjectId objectId) const;
        bool Has_SelectedObjects() const { return !_state.selectedObjectIds.empty(); }
        size_t Get_SelectedObjectCount() const { return _state.selectedObjectIds.size(); }

        const UI_Object* Get_SelectedObject(const UI_Document& document) const;

        UI_SelectionState Make_CurrentSelectionState() const;
        UI_SelectionState Make_SingleSelectionState(const UI_Document& document, ObjectId objectId) const;
        UI_SelectionState Make_SelectionState(const UI_Document& document, const vector<ObjectId>& objectIds, ObjectId primaryObjectId) const;
        UI_SelectionState Make_AddToSelectionState(const UI_Document& document, ObjectId objectId) const;
        UI_SelectionState Make_ToggleSelectionState(const UI_Document& document, ObjectId objectId) const;
        UI_SelectionState Make_ClearSelectionState() const;

        void Apply_SelectionState(UI_SelectionState selectionState);
        void Restore_SelectionState(const UI_Document& document, ObjectId selectedObjectId, const vector<ObjectId>& selectedObjectIds);

        vector<ObjectId> Normalize_Selection(const UI_Document& document, const vector<ObjectId>& objectIds) const;
        vector<ObjectId> Collect_RootObjectIds(const UI_Document& document, const vector<ObjectId>& objectIds) const;
        vector<ObjectId> Get_RootSelectedObjectIds(const UI_Document& document) const;

    private:
        void Rebuild_SelectedObjectIdSet();

    private:
        UI_SelectionState _state = {};
        unordered_set<ObjectId> _selectedObjectIdSet = {};
    };
}
